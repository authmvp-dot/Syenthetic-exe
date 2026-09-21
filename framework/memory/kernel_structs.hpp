#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace externaltest {

struct GuestListHead {
    std::uint64_t next = 0;
    std::uint64_t prev = 0;
};

struct GuestQstr {
    std::uint32_t hash = 0;
    std::uint32_t len = 0;
    std::uint64_t name = 0;
};

struct GuestPath {
    std::uint64_t mnt = 0;
    std::uint64_t dentry = 0;
};

struct GuestVfsMount {
    std::uint64_t mnt_root = 0;
    std::uint64_t mnt_sb = 0;
    std::int32_t mnt_flags = 0;
};

struct GuestMount {
    std::uint8_t pad0[0x10]{};
    std::uint64_t mnt_parent = 0;
    std::uint64_t mnt_mountpoint = 0;
    GuestVfsMount mnt{};
};

struct GuestDentry {
    std::uint8_t pad0[0x18]{};
    std::uint64_t d_parent = 0;
    GuestQstr d_name{};
    std::uint64_t d_inode = 0;
    char d_iname[32]{};
    std::uint8_t pad1[0x08]{};
    std::uint64_t d_op = 0;
};

struct GuestFile {
    std::uint8_t pad0[0x10]{};
    GuestPath f_path{};
    std::uint64_t f_inode = 0;
    std::uint64_t f_op = 0;
    std::uint8_t pad1[0x08]{};
    std::uint64_t f_count = 0;
    std::uint32_t f_flags = 0;
    std::uint32_t f_mode = 0;
    std::uint8_t pad2[0x10]{};
    std::uint64_t f_pos = 0;
};

struct GuestInode {
    std::uint16_t i_mode = 0;
    std::uint8_t pad0[0x1E]{};
    std::uint64_t i_op = 0;
    std::uint64_t i_sb = 0;
    std::uint8_t pad1[0x10]{};
    std::uint64_t i_ino = 0;
};

struct GuestVmAreaStruct {
    std::uint64_t vm_start = 0;
    std::uint64_t vm_end = 0;
    std::uint64_t vm_next = 0;
    std::uint8_t pad0[0x38]{};
    std::uint64_t vm_flags = 0;
    std::uint8_t pad1[0x40]{};
    std::uint64_t vm_pgoff = 0;
    std::uint64_t vm_file = 0;
};

struct GuestMmStruct {
    std::uint64_t mmap = 0;
    std::uint64_t mm_rb_root = 0;
    std::uint8_t pad0[0x40]{};
    std::uint64_t pgd = 0;
    std::uint8_t pad1[0xE0]{};
    std::uint64_t arg_start = 0;
    std::uint64_t arg_end = 0;
    std::uint64_t env_start = 0;
    std::uint64_t env_end = 0;
    std::uint8_t pad2[0x250]{};
    std::uint64_t exe_file = 0;
};

struct GuestTaskStruct {
    std::uint8_t pad0[0x470]{};
    GuestListHead tasks{};
    std::uint8_t pad1[0x40]{};
    std::uint64_t mm = 0;
    std::uint64_t active_mm = 0;
    std::uint8_t pad2[0xA0]{};
    std::uint32_t pid = 0;
    std::uint32_t tgid = 0;
    std::uint64_t real_parent = 0;
    std::uint64_t parent = 0;
    std::uint8_t pad3[0x188]{};
    std::uint64_t real_cred = 0;
    std::uint64_t cred = 0;
    char comm[16]{};
};

struct MapRecord {
    std::string name;
    std::string path;
    std::uint64_t vm_start = 0;
    std::uint64_t vm_end = 0;
    std::uint64_t vm_flags = 0;
    std::uint64_t vm_pgoff = 0;
    std::uint64_t vm_file = 0;
    std::uint64_t inode = 0;
    bool file_backed = false;
};

struct ExactFileNameInfo {
    bool file_read_ok = false;
    bool dentry_read_ok = false;
    std::string leaf_name;
    std::string full_path;
    std::uint64_t file_mnt = 0;
    std::uint64_t file_dentry = 0;
    std::uint64_t file_inode = 0;
    std::uint64_t inode = 0;
};

struct TaskSnapshot {
    std::uint64_t task_va = 0;
    std::uint64_t tasks_next = 0;
    std::uint64_t tasks_prev = 0;
    std::uint64_t mm = 0;
    std::uint64_t active_mm = 0;
    std::uint32_t pid = 0;
    std::uint32_t tgid = 0;
    std::string comm;
    std::string cmdline;
};

inline constexpr std::uint64_t kInitTaskGuestVa = 0xFFFFFFFF822147C0ULL;
inline constexpr std::uint64_t kLinuxDirectMapBase = 0xFFFF888000000000ULL;
inline constexpr std::uint64_t kLinuxDirectMapEnd = 0xFFFFC87FFFFFFFFFULL;
inline constexpr std::uint64_t kLinuxLegacyDirectMapBase = 0xFFFF880000000000ULL;
inline constexpr std::uint64_t kLinuxLegacyDirectMapEnd = 0xFFFFC7FFFFFFFFFFULL;
inline constexpr std::uint64_t kLinuxKernelImageBase = 0xFFFFFFFF80000000ULL;
inline constexpr std::uint64_t kLinuxKernelImageEnd = 0xFFFFFFFFFF000000ULL;

std::string Hex(std::uint64_t value);
std::string HexLower(std::uint64_t value, std::size_t min_width = 0);
std::string ToLowerAscii(std::string value);
std::size_t BoundedStrLen(const char* text, std::size_t limit);
std::string SanitizeGuestString(std::string value);
std::string BasenameFromPath(std::string_view path);
bool EqualsCaseInsensitive(std::string_view left, std::string_view right);
bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle);
bool MatchesTaskCommFilter(std::string_view task_comm, std::string_view wanted_process_name);
bool MatchesLibraryFilter(std::string_view library_name, std::string_view wanted_library_name);
bool MapRecordMatchesFilter(const MapRecord& record, std::string_view wanted_library_name);
std::string FormatVmPerms(std::uint64_t vm_flags);
std::uint64_t VmPgoffBytes(std::uint64_t vm_pgoff);
std::string DisplayMapName(const MapRecord& record);
std::string FormatMapsLine(const MapRecord& record);
bool TryConvertKernelVirtualToPhysical(std::uint64_t kernel_va, std::uint64_t& out_phys);
bool IsLikelyGuestKernelPointer(std::uint64_t value);

}  // namespace externaltest
