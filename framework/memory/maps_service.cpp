#include "maps_service.hpp"

#include <algorithm>
#include <iostream>

namespace externaltest {
namespace {

constexpr char kWalkPath[] = "task_struct.mm -> mm_struct.mmap -> vm_area_struct{vm_start,vm_end,vm_file}";
constexpr std::size_t kMaxTasks = 4096;
constexpr std::size_t kMaxVmas = 8192;

}  // namespace

MapsService::MapsService(MemoryEngine& memory) : memory_(memory) {
}

bool MapsService::ReadExactTaskSnapshot(std::uint64_t task_va, TaskSnapshot& out_task, bool verbose) {
    out_task = {};
    out_task.task_va = task_va;

    return memory_.ReadGuestKernelObject(
               task_va + offsetof(GuestTaskStruct, tasks) + offsetof(GuestListHead, next),
               out_task.tasks_next,
               verbose) &&
           memory_.ReadGuestKernelObject(
               task_va + offsetof(GuestTaskStruct, tasks) + offsetof(GuestListHead, prev),
               out_task.tasks_prev,
               verbose) &&
           memory_.ReadGuestKernelObject(task_va + offsetof(GuestTaskStruct, mm), out_task.mm, verbose) &&
           memory_.ReadGuestKernelObject(task_va + offsetof(GuestTaskStruct, active_mm), out_task.active_mm, verbose) &&
           memory_.ReadGuestKernelObject(task_va + offsetof(GuestTaskStruct, pid), out_task.pid, verbose) &&
           memory_.ReadGuestKernelObject(task_va + offsetof(GuestTaskStruct, tgid), out_task.tgid, verbose) &&
           !(out_task.comm = ReadTaskCommExact(task_va, verbose)).empty();
}

bool MapsService::ReadMmSnapshot(std::uint64_t mm_va, MmSnapshot& out_mm, bool verbose) {
    out_mm = {};
    return memory_.ReadGuestKernelObject(mm_va + offsetof(GuestMmStruct, mmap), out_mm.mmap, verbose) &&
           memory_.ReadGuestKernelObject(mm_va + offsetof(GuestMmStruct, pgd), out_mm.pgd, verbose) &&
           memory_.ReadGuestKernelObject(mm_va + offsetof(GuestMmStruct, exe_file), out_mm.exe_file, verbose) &&
           memory_.ReadGuestKernelObject(mm_va + offsetof(GuestMmStruct, arg_start), out_mm.arg_start, verbose) &&
           memory_.ReadGuestKernelObject(mm_va + offsetof(GuestMmStruct, arg_end), out_mm.arg_end, verbose);
}

int MapsService::ScoreTaskMatch(const TaskSnapshot& task, std::string_view process_filter) const {
    if (process_filter.empty()) {
        return 1;
    }

    int score = 0;
    if (!task.cmdline.empty()) {
        if (EqualsCaseInsensitive(task.cmdline, process_filter)) {
            score = (std::max)(score, 100);
        } else if (ContainsCaseInsensitive(task.cmdline, process_filter)) {
            score = (std::max)(score, 90);
        } else if (ContainsCaseInsensitive(process_filter, task.cmdline)) {
            score = (std::max)(score, 80);
        }
    }

    if (EqualsCaseInsensitive(task.comm, process_filter)) {
        score = (std::max)(score, 60);
    } else if (MatchesTaskCommFilter(task.comm, process_filter)) {
        score = (std::max)(score, 50);
    }

    if (task.pid == task.tgid && score > 0) {
        ++score;
    }
    return score;
}

std::optional<TaskSnapshot> MapsService::FindTargetTask(std::string_view process_filter, bool verbose) {
    const std::uint64_t init_tasks_head = kInitTaskGuestVa + offsetof(GuestTaskStruct, tasks);
    std::uint64_t init_tasks_next = 0;
    std::uint64_t init_tasks_prev = 0;

    if (!memory_.ReadGuestKernelObject(init_tasks_head + offsetof(GuestListHead, next), init_tasks_next, verbose) ||
        !memory_.ReadGuestKernelObject(init_tasks_head + offsetof(GuestListHead, prev), init_tasks_prev, verbose)) {
        return std::nullopt;
    }

    std::vector<std::uint64_t> seen_heads;
    std::optional<TaskSnapshot> best_task;
    int best_score = 0;

    auto consider_task = [&](TaskSnapshot task) {
        if (task.mm == 0 || !IsLikelyGuestKernelPointer(task.mm)) {
            return;
        }

        const auto mm_cr3 = memory_.ReadMmCr3Exact(task.mm, verbose);
        if (mm_cr3.has_value()) {
            const auto cmdline = ReadTaskCmdlineExact(task.mm, *mm_cr3, verbose);
            if (cmdline.has_value()) {
                task.cmdline = *cmdline;
            }
        }

        const int score = ScoreTaskMatch(task, process_filter);
        if (score > best_score) {
            best_score = score;
            best_task = std::move(task);
        }
    };

    auto walk_direction = [&](std::uint64_t start_head, bool reverse_step) {
        std::vector<std::uint64_t> local_seen;
        std::uint64_t current_head = start_head;

        while (current_head != 0 && current_head != init_tasks_head && seen_heads.size() < kMaxTasks) {
            if (current_head < offsetof(GuestTaskStruct, tasks)) {
                break;
            }
            if (std::find(local_seen.begin(), local_seen.end(), current_head) != local_seen.end()) {
                break;
            }
            local_seen.push_back(current_head);

            const std::uint64_t task_va = current_head - offsetof(GuestTaskStruct, tasks);
            TaskSnapshot task{};
            if (!ReadExactTaskSnapshot(task_va, task, verbose)) {
                GuestListHead link{};
                if (memory_.ReadGuestKernelObject(current_head, link, false)) {
                    const std::uint64_t recovered = reverse_step ? link.prev : link.next;
                    if (recovered != 0 &&
                        recovered != current_head &&
                        std::find(local_seen.begin(), local_seen.end(), recovered) == local_seen.end()) {
                        current_head = recovered;
                        continue;
                    }
                }
                break;
            }

            if (std::find(seen_heads.begin(), seen_heads.end(), current_head) == seen_heads.end()) {
                seen_heads.push_back(current_head);
                consider_task(task);
            }

            const std::uint64_t next_head = reverse_step ? task.tasks_prev : task.tasks_next;
            if (next_head == 0 || next_head == current_head) {
                break;
            }
            current_head = next_head;
        }
    };

    walk_direction(init_tasks_next, false);
    walk_direction(init_tasks_prev, true);

    return best_task;
}

std::optional<std::uint64_t> MapsService::GetTaskCr3(const TaskSnapshot& task, bool verbose) {
    if (task.mm == 0 || !IsLikelyGuestKernelPointer(task.mm)) {
        return std::nullopt;
    }
    return memory_.ReadMmCr3Exact(task.mm, verbose);
}

std::vector<MapRecord> MapsService::GetTaskMaps(const TaskSnapshot& task,
                                                std::string_view library_filter,
                                                bool verbose) {
    if (task.task_va == 0) {
        return {};
    }
    return EnumerateTaskMaps(task.task_va, library_filter, verbose);
}

std::optional<std::uint64_t> MapsService::FindFirstLibraryBase(std::string_view process_filter,
                                                               std::string_view library_filter,
                                                               bool verbose) {
    const auto task = FindTargetTask(process_filter, verbose);
    if (!task.has_value()) {
        return std::nullopt;
    }

    const auto maps = EnumerateTaskMaps(task->task_va, library_filter, verbose);
    if (maps.empty()) {
        return std::nullopt;
    }
    return maps.front().vm_start;
}

std::string MapsService::ReadTaskCommExact(std::uint64_t task_va, bool verbose) {
    char comm[16]{};
    if (!memory_.ReadGuestKernelBytes(task_va + offsetof(GuestTaskStruct, comm), comm, sizeof(comm), verbose)) {
        return {};
    }
    return SanitizeGuestString(std::string(comm, BoundedStrLen(comm, sizeof(comm))));
}

std::optional<std::string> MapsService::ReadTaskCmdlineExact(std::uint64_t mm_va,
                                                             std::uint64_t guest_cr3,
                                                             bool verbose) {
    std::uint64_t arg_start = 0;
    std::uint64_t arg_end = 0;
    if (!memory_.ReadGuestKernelObject(mm_va + offsetof(GuestMmStruct, arg_start), arg_start, verbose) ||
        !memory_.ReadGuestKernelObject(mm_va + offsetof(GuestMmStruct, arg_end), arg_end, verbose) ||
        arg_start == 0 ||
        arg_end == 0 ||
        arg_end <= arg_start) {
        return std::nullopt;
    }

    std::uint64_t span = arg_end - arg_start;
    if (span > 0x2000ULL) {
        span = 0x2000ULL;
    }
    if (span == 0) {
        return std::nullopt;
    }

    std::vector<char> data(static_cast<std::size_t>(span), '\0');
    if (!memory_.ReadGuestUserBytesByCr3(guest_cr3, arg_start, data.data(), data.size(), verbose)) {
        return std::nullopt;
    }

    std::size_t first_end = 0;
    while (first_end < data.size() && data[first_end] != '\0') {
        ++first_end;
    }
    if (first_end == 0) {
        return std::nullopt;
    }

    return SanitizeGuestString(std::string(data.data(), first_end));
}

std::string MapsService::ReadGuestKernelDentryNameFast(std::uint64_t dentry_va, bool verbose) {
    if (dentry_va == 0) {
        return {};
    }

    std::uint64_t name_ptr = 0;
    if (memory_.ReadGuestKernelObject(dentry_va + 0x28, name_ptr, verbose) && name_ptr != 0) {
        const auto fast_name = memory_.ReadGuestKernelCStringBounded(name_ptr, 255, verbose);
        if (fast_name.has_value() && !fast_name->empty()) {
            return *fast_name;
        }
    }

    char inline_name[32]{};
    if (memory_.ReadGuestKernelBytes(dentry_va + 0x38, inline_name, sizeof(inline_name), verbose)) {
        return SanitizeGuestString(std::string(inline_name, BoundedStrLen(inline_name, sizeof(inline_name))));
    }

    return {};
}

std::string MapsService::ReadGuestKernelDentryName(std::uint64_t dentry_va, bool verbose) {
    const std::string fast_name = ReadGuestKernelDentryNameFast(dentry_va, verbose);
    if (!fast_name.empty()) {
        return fast_name;
    }

    GuestDentry dentry{};
    if (!memory_.ReadGuestKernelObject(dentry_va, dentry, verbose)) {
        return {};
    }

    if (dentry.d_name.len > 0 && dentry.d_name.len <= 256 && dentry.d_name.name != 0) {
        const auto exact_name = memory_.ReadGuestKernelString(dentry.d_name.name, dentry.d_name.len, verbose);
        if (exact_name.has_value() && !exact_name->empty()) {
            return *exact_name;
        }
    }

    return SanitizeGuestString(std::string(dentry.d_iname, BoundedStrLen(dentry.d_iname, sizeof(dentry.d_iname))));
}

std::string MapsService::ReadGuestKernelFilePath(const GuestFile& file, bool verbose) {
    if (file.f_path.dentry == 0) {
        return {};
    }

    std::vector<std::string> components;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> seen;
    std::uint64_t current_mnt = file.f_path.mnt;
    std::uint64_t current_dentry = file.f_path.dentry;

    while (current_dentry != 0 && seen.size() < 256) {
        const auto state = std::make_pair(current_mnt, current_dentry);
        if (std::find(seen.begin(), seen.end(), state) != seen.end()) {
            break;
        }
        seen.push_back(state);

        GuestDentry dentry{};
        if (!memory_.ReadGuestKernelObject(current_dentry, dentry, verbose)) {
            break;
        }

        std::string name = ReadGuestKernelDentryName(current_dentry, verbose);
        if (!name.empty() && name != "/" && name != ".") {
            components.push_back(name);
        }

        std::uint64_t mount_root = 0;
        if (current_mnt != 0) {
            GuestVfsMount mnt{};
            if (memory_.ReadGuestKernelObject(current_mnt, mnt, verbose)) {
                mount_root = mnt.mnt_root;
            }
        }

        if (mount_root != 0 && current_dentry != mount_root && dentry.d_parent != 0 && dentry.d_parent != current_dentry) {
            current_dentry = dentry.d_parent;
            continue;
        }

        if (current_mnt != 0 && current_mnt >= offsetof(GuestMount, mnt)) {
            const std::uint64_t mount_va = current_mnt - offsetof(GuestMount, mnt);
            GuestMount mount{};
            if (memory_.ReadGuestKernelObject(mount_va, mount, verbose) &&
                mount.mnt_parent != 0 &&
                mount.mnt_parent != mount_va &&
                mount.mnt_mountpoint != 0) {
                current_mnt = mount.mnt_parent + offsetof(GuestMount, mnt);
                current_dentry = mount.mnt_mountpoint;
                continue;
            }
        }

        if (dentry.d_parent == 0 || dentry.d_parent == current_dentry) {
            break;
        }
        current_dentry = dentry.d_parent;
    }

    if (components.empty()) {
        return ReadGuestKernelDentryName(file.f_path.dentry, verbose);
    }

    std::string path;
    for (auto it = components.rbegin(); it != components.rend(); ++it) {
        path.push_back('/');
        path += *it;
    }
    return path;
}

ExactFileNameInfo MapsService::ReadExactFileNameInfo(std::uint64_t vm_file, bool verbose) {
    ExactFileNameInfo info;
    if (vm_file == 0) {
        return info;
    }

    GuestFile file{};
    if (!memory_.ReadGuestKernelObject(vm_file, file, verbose)) {
        return info;
    }
    info.file_read_ok = true;
    info.file_mnt = file.f_path.mnt;
    info.file_dentry = file.f_path.dentry;
    info.file_inode = file.f_inode;

    if (info.file_dentry == 0) {
        if (info.file_inode != 0) {
            GuestInode inode{};
            if (memory_.ReadGuestKernelObject(info.file_inode, inode, verbose)) {
                info.inode = inode.i_ino;
            }
        }
        return info;
    }

    GuestDentry dentry{};
    if (!memory_.ReadGuestKernelObject(info.file_dentry, dentry, verbose)) {
        return info;
    }
    info.dentry_read_ok = true;
    if (info.file_inode == 0) {
        info.file_inode = dentry.d_inode;
    }

    if (info.file_inode != 0) {
        GuestInode inode{};
        if (memory_.ReadGuestKernelObject(info.file_inode, inode, verbose)) {
            info.inode = inode.i_ino;
        }
    }

    info.leaf_name = ReadGuestKernelDentryName(info.file_dentry, verbose);
    info.full_path = ReadGuestKernelFilePath(file, verbose);
    if (info.full_path.empty() && !info.leaf_name.empty()) {
        info.full_path = info.leaf_name;
    }
    if (info.leaf_name.empty() && !info.full_path.empty()) {
        info.leaf_name = BasenameFromPath(info.full_path);
    }

    return info;
}

std::vector<MapRecord> MapsService::EnumerateTaskMaps(std::uint64_t task_va,
                                                      std::string_view library_filter,
                                                      bool verbose) {
    std::vector<MapRecord> maps;

    std::uint64_t mm_va = 0;
    if (!memory_.ReadGuestKernelObject(task_va + offsetof(GuestTaskStruct, mm), mm_va, verbose) || mm_va == 0) {
        return maps;
    }

    std::uint64_t current_vma = 0;
    if (!memory_.ReadGuestKernelObject(mm_va + offsetof(GuestMmStruct, mmap), current_vma, verbose)) {
        return maps;
    }

    std::vector<std::uint64_t> seen_vmas;
    while (current_vma != 0 && seen_vmas.size() < kMaxVmas) {
        if (std::find(seen_vmas.begin(), seen_vmas.end(), current_vma) != seen_vmas.end()) {
            break;
        }
        seen_vmas.push_back(current_vma);

        std::uint64_t vm_start = 0;
        std::uint64_t vm_end = 0;
        std::uint64_t vm_next = 0;
        std::uint64_t vm_flags = 0;
        std::uint64_t vm_pgoff = 0;
        std::uint64_t vm_file = 0;

        const bool vma_ok =
            memory_.ReadGuestKernelObject(current_vma + 0x0, vm_start, verbose) &&
            memory_.ReadGuestKernelObject(current_vma + 0x8, vm_end, verbose) &&
            memory_.ReadGuestKernelObject(current_vma + 0x10, vm_next, verbose) &&
            memory_.ReadGuestKernelObject(current_vma + 0x50, vm_flags, verbose) &&
            memory_.ReadGuestKernelObject(current_vma + 0x98, vm_pgoff, verbose) &&
            memory_.ReadGuestKernelObject(current_vma + 0xA0, vm_file, verbose);
        if (!vma_ok) {
            break;
        }

        MapRecord record;
        record.vm_start = vm_start;
        record.vm_end = vm_end;
        record.vm_flags = vm_flags;
        record.vm_pgoff = vm_pgoff;
        record.vm_file = vm_file;
        record.file_backed = vm_file != 0;

        if (vm_file != 0) {
            const ExactFileNameInfo file_info = ReadExactFileNameInfo(vm_file, verbose);
            record.name = file_info.leaf_name;
            record.path = file_info.full_path;
            record.inode = file_info.inode;
            if (record.name.empty() && !record.path.empty()) {
                record.name = BasenameFromPath(record.path);
            }
        }

        maps.push_back(std::move(record));

        if (vm_next == 0 || vm_next == current_vma) {
            break;
        }
        current_vma = vm_next;
    }

    if (!library_filter.empty()) {
        std::vector<std::uint64_t> matched_vm_files;
        for (const auto& record : maps) {
            if (!MapRecordMatchesFilter(record, library_filter)) {
                continue;
            }
            if (record.vm_file == 0) {
                continue;
            }
            if (std::find(matched_vm_files.begin(), matched_vm_files.end(), record.vm_file) == matched_vm_files.end()) {
                matched_vm_files.push_back(record.vm_file);
            }
        }

        std::vector<MapRecord> filtered;
        filtered.reserve(maps.size());
        for (const auto& record : maps) {
            const bool name_match = MapRecordMatchesFilter(record, library_filter);
            const bool vm_file_match =
                record.vm_file != 0 &&
                std::find(matched_vm_files.begin(), matched_vm_files.end(), record.vm_file) != matched_vm_files.end();
            if (name_match || vm_file_match) {
                filtered.push_back(record);
            }
        }
        maps = std::move(filtered);
    }

    std::sort(maps.begin(), maps.end(), [](const MapRecord& left, const MapRecord& right) {
        if (left.vm_start != right.vm_start) {
            return left.vm_start < right.vm_start;
        }
        return left.vm_end < right.vm_end;
    });

    return maps;
}

bool MapsService::DumpTaskMaps(const TaskSnapshot& task, std::string_view library_filter, bool verbose) {
    MmSnapshot mm{};
    if (!ReadMmSnapshot(task.mm, mm, verbose)) {
        std::cerr << "failed to read mm_struct at " << Hex(task.mm) << "\n";
        return false;
    }

    const auto maps = EnumerateTaskMaps(task.task_va, library_filter, verbose);

    std::cout << "walk_path        = " << kWalkPath << "\n";
    std::cout << "task_va          = " << Hex(task.task_va) << "\n";
    std::cout << "task_comm        = " << task.comm << "\n";
    std::cout << "task_pid         = " << task.pid << "\n";
    std::cout << "task_tgid        = " << task.tgid << "\n";
    std::cout << "task_mm          = " << Hex(task.mm) << "\n";
    std::cout << "task_active_mm   = " << Hex(task.active_mm) << "\n";
    std::cout << "mm_mmap_head     = " << Hex(mm.mmap) << "\n";
    std::cout << "mm_pgd           = " << Hex(mm.pgd) << "\n";
    std::cout << "mm_exe_file      = " << Hex(mm.exe_file) << "\n";
    std::cout << "mm_arg_start     = " << Hex(mm.arg_start) << "\n";
    std::cout << "mm_arg_end       = " << Hex(mm.arg_end) << "\n";
    std::cout << "map_count        = " << maps.size() << "\n";

    for (const auto& record : maps) {
        std::cout << FormatMapsLine(record) << "\n";
    }

    if (maps.empty()) {
        std::cout << "result           = no VMAs matched from this task\n";
        return false;
    }

    std::cout << "result           = recovered VMA ranges via exact task walk\n";
    return true;
}

}  // namespace externaltest
