#include "kernel_structs.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace externaltest {

static_assert(sizeof(void*) == 8, "Build externaltest as x64.");
static_assert(offsetof(GuestDentry, d_name) == 0x20, "GuestDentry.d_name offset mismatch");
static_assert(offsetof(GuestDentry, d_inode) == 0x30, "GuestDentry.d_inode offset mismatch");
static_assert(offsetof(GuestDentry, d_op) == 0x60, "GuestDentry.d_op offset mismatch");
static_assert(offsetof(GuestMount, mnt_parent) == 0x10, "GuestMount.mnt_parent offset mismatch");
static_assert(offsetof(GuestMount, mnt_mountpoint) == 0x18, "GuestMount.mnt_mountpoint offset mismatch");
static_assert(offsetof(GuestMount, mnt) == 0x20, "GuestMount.mnt offset mismatch");
static_assert(offsetof(GuestFile, f_path) == 0x10, "GuestFile.f_path offset mismatch");
static_assert(offsetof(GuestFile, f_count) == 0x38, "GuestFile.f_count offset mismatch");
static_assert(offsetof(GuestInode, i_ino) == 0x40, "GuestInode.i_ino offset mismatch");
static_assert(offsetof(GuestVmAreaStruct, vm_flags) == 0x50, "GuestVmAreaStruct.vm_flags offset mismatch");
static_assert(offsetof(GuestVmAreaStruct, vm_pgoff) == 0x98, "GuestVmAreaStruct.vm_pgoff offset mismatch");
static_assert(offsetof(GuestVmAreaStruct, vm_file) == 0xA0, "GuestVmAreaStruct.vm_file offset mismatch");
static_assert(offsetof(GuestMmStruct, mmap) == 0x0, "GuestMmStruct.mmap offset mismatch");
static_assert(offsetof(GuestMmStruct, pgd) == 0x50, "GuestMmStruct.pgd offset mismatch");
static_assert(offsetof(GuestMmStruct, arg_start) == 0x138, "GuestMmStruct.arg_start offset mismatch");
static_assert(offsetof(GuestMmStruct, env_end) == 0x150, "GuestMmStruct.env_end offset mismatch");
static_assert(offsetof(GuestMmStruct, exe_file) == 0x3A8, "GuestMmStruct.exe_file offset mismatch");
static_assert(offsetof(GuestTaskStruct, tasks) == 0x470, "GuestTaskStruct.tasks offset mismatch");
static_assert(offsetof(GuestTaskStruct, mm) == 0x4C0, "GuestTaskStruct.mm offset mismatch");
static_assert(offsetof(GuestTaskStruct, pid) == 0x570, "GuestTaskStruct.pid offset mismatch");
static_assert(offsetof(GuestTaskStruct, real_cred) == 0x710, "GuestTaskStruct.real_cred offset mismatch");
static_assert(offsetof(GuestTaskStruct, cred) == 0x718, "GuestTaskStruct.cred offset mismatch");
static_assert(offsetof(GuestTaskStruct, comm) == 0x720, "GuestTaskStruct.comm offset mismatch");

std::string Hex(std::uint64_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << value;
    return out.str();
}

std::string HexLower(std::uint64_t value, std::size_t min_width) {
    std::ostringstream out;
    out << std::hex << std::nouppercase;
    if (min_width != 0) {
        out << std::setw(static_cast<int>(min_width)) << std::setfill('0');
    }
    out << value;
    return out.str();
}

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::size_t BoundedStrLen(const char* text, std::size_t limit) {
    std::size_t len = 0;
    while (len < limit && text[len] != '\0') {
        ++len;
    }
    return len;
}

std::string SanitizeGuestString(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
        return ch == '\0' || ch == '\r' || ch == '\n';
    }), value.end());
    return value;
}

std::string BasenameFromPath(std::string_view path) {
    if (path.empty()) {
        return {};
    }
    const auto pos = path.find_last_of("/\\");
    if (pos == std::string_view::npos) {
        return std::string(path);
    }
    if (pos + 1 >= path.size()) {
        return {};
    }
    return std::string(path.substr(pos + 1));
}

bool EqualsCaseInsensitive(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        const auto a = static_cast<unsigned char>(left[i]);
        const auto b = static_cast<unsigned char>(right[i]);
        if (std::tolower(a) != std::tolower(b)) {
            return false;
        }
    }
    return true;
}

bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }
    if (needle.size() > haystack.size()) {
        return false;
    }
    for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        if (EqualsCaseInsensitive(haystack.substr(i, needle.size()), needle)) {
            return true;
        }
    }
    return false;
}

bool MatchesTaskCommFilter(std::string_view task_comm, std::string_view wanted_process_name) {
    if (wanted_process_name.empty()) {
        return true;
    }
    if (EqualsCaseInsensitive(task_comm, wanted_process_name)) {
        return true;
    }
    if (ContainsCaseInsensitive(task_comm, wanted_process_name)) {
        return true;
    }
    if (task_comm.size() >= 6 && ContainsCaseInsensitive(wanted_process_name, task_comm)) {
        return true;
    }
    if (task_comm.size() == 15 && wanted_process_name.size() >= task_comm.size()) {
        return EqualsCaseInsensitive(task_comm, wanted_process_name.substr(0, task_comm.size()));
    }
    const auto last_dot = wanted_process_name.rfind('.');
    if (last_dot != std::string_view::npos && last_dot + 1 < wanted_process_name.size()) {
        const auto leaf = wanted_process_name.substr(last_dot + 1);
        if (EqualsCaseInsensitive(task_comm, leaf) ||
            ContainsCaseInsensitive(task_comm, leaf) ||
            ContainsCaseInsensitive(leaf, task_comm)) {
            return true;
        }
    }
    if (wanted_process_name.size() < task_comm.size()) {
        return EqualsCaseInsensitive(task_comm.substr(0, wanted_process_name.size()), wanted_process_name);
    }
    return false;
}

bool MatchesLibraryFilter(std::string_view library_name, std::string_view wanted_library_name) {
    if (wanted_library_name.empty()) {
        return true;
    }
    if (library_name.empty()) {
        return false;
    }
    if (EqualsCaseInsensitive(library_name, wanted_library_name)) {
        return true;
    }
    return ContainsCaseInsensitive(library_name, wanted_library_name);
}

bool MapRecordMatchesFilter(const MapRecord& record, std::string_view wanted_library_name) {
    if (wanted_library_name.empty()) {
        return true;
    }
    return MatchesLibraryFilter(record.name, wanted_library_name) ||
           MatchesLibraryFilter(record.path, wanted_library_name);
}

std::string FormatVmPerms(std::uint64_t vm_flags) {
    std::string perms = "----";
    if ((vm_flags & 0x1ULL) != 0) {
        perms[0] = 'r';
    }
    if ((vm_flags & 0x2ULL) != 0) {
        perms[1] = 'w';
    }
    if ((vm_flags & 0x4ULL) != 0) {
        perms[2] = 'x';
    }
    perms[3] = (vm_flags & 0x8ULL) != 0 ? 's' : 'p';
    return perms;
}

std::uint64_t VmPgoffBytes(std::uint64_t vm_pgoff) {
    return vm_pgoff << 12;
}

std::string DisplayMapName(const MapRecord& record) {
    if (!record.path.empty()) {
        return record.path;
    }
    if (!record.name.empty()) {
        return record.name;
    }
    if (record.file_backed) {
        return "[unnamed_file_map]";
    }
    if ((record.vm_flags & 0x100ULL) != 0) {
        return "[stack]";
    }
    return "[anonymous]";
}

std::string FormatMapsLine(const MapRecord& record) {
    std::ostringstream out;
    out << HexLower(record.vm_start)
        << "-" << HexLower(record.vm_end)
        << " " << FormatVmPerms(record.vm_flags)
        << " " << HexLower(VmPgoffBytes(record.vm_pgoff), 8)
        << " 00:00 "
        << std::dec << record.inode
        << " " << DisplayMapName(record);
    return out.str();
}

bool TryConvertKernelVirtualToPhysical(std::uint64_t kernel_va, std::uint64_t& out_phys) {
    if (kernel_va >= kLinuxDirectMapBase && kernel_va <= kLinuxDirectMapEnd) {
        out_phys = kernel_va - kLinuxDirectMapBase;
        return true;
    }
    if (kernel_va >= kLinuxLegacyDirectMapBase && kernel_va <= kLinuxLegacyDirectMapEnd) {
        out_phys = kernel_va - kLinuxLegacyDirectMapBase;
        return true;
    }
    if (kernel_va >= kLinuxKernelImageBase && kernel_va < kLinuxKernelImageEnd) {
        out_phys = kernel_va - kLinuxKernelImageBase;
        return true;
    }
    return false;
}

bool IsLikelyGuestKernelPointer(std::uint64_t value) {
    return value >= 0xFFFF800000000000ULL;
}

}  // namespace externaltest
