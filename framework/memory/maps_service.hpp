#pragma once

#include "kernel_structs.hpp"
#include "memoryengine.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace externaltest {

class MapsService {
public:
    explicit MapsService(MemoryEngine& memory);

    std::optional<TaskSnapshot> FindTargetTask(std::string_view process_filter, bool verbose = false);
    std::optional<std::uint64_t> GetTaskCr3(const TaskSnapshot& task, bool verbose = false);
    std::vector<MapRecord> GetTaskMaps(const TaskSnapshot& task, std::string_view library_filter = {}, bool verbose = false);
    std::optional<std::uint64_t> FindFirstLibraryBase(std::string_view process_filter,
                                                      std::string_view library_filter,
                                                      bool verbose = false);
    bool DumpTaskMaps(const TaskSnapshot& task, std::string_view library_filter, bool verbose = false);

private:
    struct MmSnapshot {
        std::uint64_t mmap = 0;
        std::uint64_t pgd = 0;
        std::uint64_t exe_file = 0;
        std::uint64_t arg_start = 0;
        std::uint64_t arg_end = 0;
    };

    bool ReadExactTaskSnapshot(std::uint64_t task_va, TaskSnapshot& out_task, bool verbose);
    bool ReadMmSnapshot(std::uint64_t mm_va, MmSnapshot& out_mm, bool verbose);
    int ScoreTaskMatch(const TaskSnapshot& task, std::string_view process_filter) const;

    std::string ReadTaskCommExact(std::uint64_t task_va, bool verbose);
    std::optional<std::string> ReadTaskCmdlineExact(std::uint64_t mm_va, std::uint64_t guest_cr3, bool verbose);
    std::string ReadGuestKernelDentryNameFast(std::uint64_t dentry_va, bool verbose);
    std::string ReadGuestKernelDentryName(std::uint64_t dentry_va, bool verbose);
    std::string ReadGuestKernelFilePath(const GuestFile& file, bool verbose);
    ExactFileNameInfo ReadExactFileNameInfo(std::uint64_t vm_file, bool verbose);
    std::vector<MapRecord> EnumerateTaskMaps(std::uint64_t task_va,
                                             std::string_view library_filter,
                                             bool verbose);

    MemoryEngine& memory_;
};

}  // namespace externaltest
