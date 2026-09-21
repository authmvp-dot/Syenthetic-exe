#include "memoryengine.hpp"

#include "app_config.hpp"
#include "kernel_structs.hpp"

#include <tlhelp32.h>
#include <basetsd.h>

#include <algorithm>
#include <cwctype>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>

namespace externaltest {
namespace {

struct Offsets {
    static constexpr std::uint64_t VmCpuCount = 0x3C;
    static constexpr std::uint64_t VmReadCounter = 0x10AFF0;
    static constexpr std::uint64_t VmDirectRamWindow = 0x104000;
    static constexpr std::uint64_t VmDirectRamTag = 0x106000;
    static constexpr std::uint64_t VmRamRangeMode = 0x106019;
    static constexpr std::uint64_t VmRangeCache = 0x106050;
    static constexpr std::uint64_t VmRangeTreeRoot = 0x106098;
    static constexpr std::uint64_t VmMmio2Table = 0x1060B0;
    static constexpr std::uint64_t VmChunkCache = 0x106860;
    static constexpr std::uint64_t VmChunkTreeRoot = 0x106C60;
    static constexpr std::uint64_t VmPageMapCache = 0x106C80;
    static constexpr std::uint64_t VmCpuArray = 0x11FE00;
};

constexpr std::uint64_t kLongModePageFrameMask = 0x000FFFFFFFFFF000ULL;
constexpr std::uint64_t kBstkMapChunkRva = 0xDA070ULL;

bool IsLikelyUserPointer(std::uint64_t value) {
    return value >= 0x10000ULL && value < 0x00007FFFFFFFFFFFULL;
}

void VerboseLog(bool verbose, const std::string& message) {
    if (verbose) {
        std::cout << message << "\n";
    }
}

std::wstring ToLowerWide(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(::towlower(ch));
    });
    return value;
}

// Low-level NT API definitions
typedef LONG NTSTATUS;
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

// Standard Win32/NT types that might be missing in some headers
typedef unsigned __int64 ULONG_PTR;
typedef __int64 LONG_PTR;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG           Length;
    HANDLE          RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG           Attributes;
    PVOID           SecurityDescriptor;
    PVOID           SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

typedef struct _CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

typedef enum _SYSTEM_INFORMATION_CLASS {
    SystemProcessInformation = 5
} SYSTEM_INFORMATION_CLASS;

typedef struct _SYSTEM_PROCESS_INFORMATION {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    BYTE Reserved1[48];
    UNICODE_STRING ImageName;
    LONG BasePriority;
    HANDLE UniqueProcessId;
    PVOID Reserved2;
    ULONG HandleCount;
    ULONG SessionId;
    PVOID Reserved3;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG Reserved4;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    PVOID Reserved5;
    SIZE_T QuotaPagedPoolUsage;
    PVOID Reserved6;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

typedef enum _PROCESSINFOCLASS {
    ProcessBasicInformation = 0
} PROCESSINFOCLASS;

typedef struct _PROCESS_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PVOID PebBaseAddress;
    ULONG_PTR AffinityMask;
    LONG BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION;

typedef struct _PEB_LDR_DATA {
    ULONG Length;
    BOOLEAN Initialized;
    HANDLE SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

typedef NTSTATUS(NTAPI* pfnNtQuerySystemInformation)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

typedef NTSTATUS(NTAPI* pfnNtOpenProcess)(
    PHANDLE ProcessHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PCLIENT_ID ClientId
);

typedef NTSTATUS(NTAPI* pfnNtQueryInformationProcess)(
    HANDLE ProcessHandle,
    PROCESSINFOCLASS ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
);

template <typename T>
T GetNtFunction(const char* name) {
    static HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return nullptr;
    return reinterpret_cast<T>(GetProcAddress(ntdll, name));
}

HANDLE NtOpenProcessWrapper(DWORD pid, ACCESS_MASK access) {
    static auto NtOpenProcess = GetNtFunction<pfnNtOpenProcess>("NtOpenProcess");
    if (!NtOpenProcess) return nullptr;

    HANDLE handle = nullptr;
    OBJECT_ATTRIBUTES oa{};
    oa.Length = sizeof(oa);
    CLIENT_ID cid{};
    cid.UniqueProcess = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(pid));

    if (!NT_SUCCESS(NtOpenProcess(&handle, access, &oa, &cid))) {
        return nullptr;
    }
    return handle;
}

}  // namespace

MemoryEngine::~MemoryEngine() {
    Reset();
}

bool MemoryEngine::Initialize(bool verbose) {
    Reset();
    last_error_.clear();

    const auto pid = FindProcessIdByName(kDefaultProcessName);
    if (!pid.has_value()) {
        last_error_ = "HD-Player Not Found";
        return false;
    }

    process_ = NtOpenProcessWrapper(*pid, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION);
    if (process_ == nullptr) {
        last_error_ = "OpenProcess Failed";
        Reset();
        return false;
    }
    process_id_ = *pid;

    const auto module = FindModuleInProcess(process_id_, kDefaultModuleName);
    if (!module.has_value()) {
        last_error_ = "BstkVMM Failed";
        Reset();
        return false;
    }
    module_ = *module;
    bstk_module_base_ = module_.base;

    const auto auto_vm = AutoLocateVmInfo();
    if (!auto_vm.has_value()) {
        last_error_ = "VM Locate Failed";
        Reset();
        return false;
    }
    auto_vm_ = *auto_vm;
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        gva_to_phys_page_cache_.reserve(65536);
        gva_to_host_page_cache_.reserve(65536);
        phys_to_host_page_cache_.reserve(65536);
    }

    VerboseLog(verbose, "memory engine initialized");
    return true;
}

bool MemoryEngine::IsInitialized() const {
    return process_ != nullptr && process_id_ != 0 && auto_vm_.pvm != 0 && auto_vm_.pvcpu0 != 0;
}

HANDLE MemoryEngine::process() const {
    return process_;
}

DWORD MemoryEngine::process_id() const {
    return process_id_;
}

const ModuleInfo& MemoryEngine::module() const {
    return module_;
}

const AutoVmInfo& MemoryEngine::auto_vm() const {
    return auto_vm_;
}

std::uint64_t MemoryEngine::pvm() const {//
    return auto_vm_.pvm;
}

std::uint64_t MemoryEngine::pvcpu() const {
    return auto_vm_.pvcpu0;
}

void MemoryEngine::ClearRuntimeCaches() {
    ClearTranslationCaches();
}

void MemoryEngine::ClearTranslationCaches() {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    gva_to_phys_page_cache_.clear();
    gva_to_host_page_cache_.clear();
    phys_to_host_page_cache_.clear();
}

std::optional<std::uint64_t> MemoryEngine::LookupCachedPhysPage(std::uint64_t guest_cr3, std::uint64_t guest_va) const {
    const GvaPageKey key{guest_cr3 & kLongModePageFrameMask, guest_va & ~0xFFFULL};
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    const auto it = gva_to_phys_page_cache_.find(key);
    if (it == gva_to_phys_page_cache_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void MemoryEngine::StoreCachedPhysPage(std::uint64_t guest_cr3, std::uint64_t guest_va, std::uint64_t guest_phys_page) {
    const GvaPageKey key{guest_cr3 & kLongModePageFrameMask, guest_va & ~0xFFFULL};
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    if (gva_to_phys_page_cache_.size() >= kMaxGvaCacheEntries) {
        gva_to_phys_page_cache_.clear();
    }
    gva_to_phys_page_cache_[key] = guest_phys_page & ~0xFFFULL;
}

void MemoryEngine::InvalidateCachedPhysPage(std::uint64_t guest_cr3, std::uint64_t guest_va) {
    const GvaPageKey key{guest_cr3 & kLongModePageFrameMask, guest_va & ~0xFFFULL};
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    gva_to_phys_page_cache_.erase(key);
    gva_to_host_page_cache_.erase(key);
}

std::optional<std::uint64_t> MemoryEngine::LookupCachedHostPageByGva(std::uint64_t guest_cr3,
                                                                      std::uint64_t guest_va) const {
    const GvaPageKey key{guest_cr3 & kLongModePageFrameMask, guest_va & ~0xFFFULL};
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    const auto it = gva_to_host_page_cache_.find(key);
    if (it == gva_to_host_page_cache_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void MemoryEngine::StoreCachedHostPageByGva(std::uint64_t guest_cr3,
                                            std::uint64_t guest_va,
                                            std::uint64_t host_page_base) {
    const GvaPageKey key{guest_cr3 & kLongModePageFrameMask, guest_va & ~0xFFFULL};
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    if (gva_to_host_page_cache_.size() >= kMaxGvaHostCacheEntries) {
        gva_to_host_page_cache_.clear();
    }
    gva_to_host_page_cache_[key] = host_page_base & ~0xFFFULL;
}

void MemoryEngine::InvalidateCachedHostPageByGva(std::uint64_t guest_cr3, std::uint64_t guest_va) {
    const GvaPageKey key{guest_cr3 & kLongModePageFrameMask, guest_va & ~0xFFFULL};
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    gva_to_host_page_cache_.erase(key);
}

std::optional<std::uint64_t> MemoryEngine::LookupCachedHostPage(std::uint64_t guest_phys) const {
    const std::uint64_t guest_phys_page = guest_phys & ~0xFFFULL;
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    const auto it = phys_to_host_page_cache_.find(guest_phys_page);
    if (it == phys_to_host_page_cache_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void MemoryEngine::StoreCachedHostPage(std::uint64_t guest_phys, std::uint64_t host_page_base) {
    const std::uint64_t guest_phys_page = guest_phys & ~0xFFFULL;
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    if (phys_to_host_page_cache_.size() >= kMaxPhysCacheEntries) {
        phys_to_host_page_cache_.clear();
    }
    phys_to_host_page_cache_[guest_phys_page] = host_page_base & ~0xFFFULL;
}

void MemoryEngine::InvalidateCachedHostPage(std::uint64_t guest_phys) {
    const std::uint64_t guest_phys_page = guest_phys & ~0xFFFULL;
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    phys_to_host_page_cache_.erase(guest_phys_page);
}

void MemoryEngine::Reset() {
    ClearTranslationCaches();
    if (process_ != nullptr) {
        CloseHandle(process_);
        process_ = nullptr;
    }
    process_id_ = 0;
    module_ = {};
    auto_vm_ = {};
    bstk_module_base_ = 0;
}

bool MemoryEngine::ReadHostBytes(std::uint64_t address, void* buffer, std::size_t size) const {
    if (process_ == nullptr) {
        return false;
    }
    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(process_, reinterpret_cast<LPCVOID>(address), buffer, size, &bytes_read)) {
        return false;
    }
    return bytes_read == size;
}

bool MemoryEngine::WriteHostBytes(std::uint64_t address, const void* buffer, std::size_t size) const {
    if (process_ == nullptr) {
        return false;
    }
    SIZE_T bytes_written = 0;
    if (!WriteProcessMemory(process_, reinterpret_cast<LPVOID>(address), buffer, size, &bytes_written)) {
        return false;
    }
    return bytes_written == size;
}

std::optional<AutoVmInfo> MemoryEngine::AutoLocateVmInfo() const {
    MEMORY_BASIC_INFORMATION mbi{};
    std::uint8_t* addr = nullptr;

    while (VirtualQueryEx(process_, addr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        const DWORD prot = mbi.Protect & 0xFF;
        const bool readable_writable = prot == PAGE_READWRITE || prot == PAGE_EXECUTE_READWRITE;

        if (mbi.State == MEM_COMMIT && readable_writable && mbi.RegionSize >= (128 * 1024)) {
            const std::uint64_t candidate = reinterpret_cast<std::uint64_t>(mbi.BaseAddress);

            std::uint32_t cpu_count = 0;
            if (!ReadHostValue(candidate + Offsets::VmCpuCount, cpu_count) || cpu_count < 1 || cpu_count > 8) {
                goto next_region;
            }

            if (Offsets::VmCpuArray + sizeof(std::uint64_t) > static_cast<std::uint64_t>(mbi.RegionSize)) {
                goto next_region;
            }

            std::uint64_t cpu0 = 0;
            if (!ReadHostValue(candidate + Offsets::VmCpuArray, cpu0)) {
                goto next_region;
            }
            if (!IsLikelyUserPointer(cpu0)) {
                goto next_region;
            }

            std::uint64_t read_count = 0;
            if (!ReadHostValue(candidate + Offsets::VmReadCounter, read_count)) {
                goto next_region;
            }
            if (read_count == 0 || read_count > 1000000000000ULL) {
                goto next_region;
            }

            // Older BlueStacks embeds VCPU at pvm+0x120000. Newer builds allocate
            // VCPUs separately — accept either, but require readable VCPU slots + tree.
            const bool embedded_vcpu = (cpu0 == candidate + 0x120000ULL);
            if (!embedded_vcpu) {
                std::uint64_t vcpu_probe = 0;
                if (!ReadHostValue(cpu0, vcpu_probe)) {
                    goto next_region;
                }
                for (std::uint32_t i = 1; i < cpu_count; ++i) {
                    std::uint64_t slot = 0;
                    if (!ReadHostValue(candidate + Offsets::VmCpuArray + static_cast<std::uint64_t>(i) * sizeof(std::uint64_t), slot) ||
                        !IsLikelyUserPointer(slot) ||
                        !ReadHostValue(slot, vcpu_probe)) {
                        goto next_region;
                    }
                }
                std::uint64_t range_root = 0;
                std::uint64_t chunk_root = 0;
                const bool range_ok =
                    ReadHostValue(candidate + Offsets::VmRangeTreeRoot, range_root) && IsLikelyUserPointer(range_root);
                const bool chunk_ok =
                    ReadHostValue(candidate + Offsets::VmChunkTreeRoot, chunk_root) && IsLikelyUserPointer(chunk_root);
                if (!range_ok && !chunk_ok) {
                    goto next_region;
                }
            }

            AutoVmInfo info;
            info.pvm = candidate;
            info.pvcpu0 = cpu0;
            info.cpu_count = cpu_count;
            info.read_counter = read_count;
            info.region_size = static_cast<std::uint64_t>(mbi.RegionSize);
            return info;
        }

    next_region:
        addr += mbi.RegionSize;
        if (!addr) {
            break;
        }
    }

    return std::nullopt;
}

std::optional<DWORD> MemoryEngine::FindProcessIdByName(const std::wstring& process_name) {
    const std::wstring wanted = ToLowerWide(process_name);

    static auto NtQuerySystemInformation = GetNtFunction<pfnNtQuerySystemInformation>("NtQuerySystemInformation");
    if (NtQuerySystemInformation) {
        ULONG size = 0;
        NtQuerySystemInformation(SystemProcessInformation, nullptr, 0, &size);
        if (size != 0) {
            std::vector<std::uint8_t> buffer(size + 0x1000);
            if (NT_SUCCESS(NtQuerySystemInformation(SystemProcessInformation, buffer.data(), static_cast<ULONG>(buffer.size()), &size))) {
                auto current = reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>(buffer.data());
                while (true) {
                    if (current->ImageName.Buffer != nullptr) {
                        std::wstring name(current->ImageName.Buffer, current->ImageName.Length / sizeof(wchar_t));
                        if (ToLowerWide(name) == wanted) {
                            return static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(current->UniqueProcessId));
                        }
                    }
                    if (current->NextEntryOffset == 0) break;
                    current = reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>(
                        reinterpret_cast<std::uint8_t*>(current) + current->NextEntryOffset);
                }
            }
        }
    }

    // Toolhelp fallback (same path Overlay uses) — NtQuery can miss under some AV/hooks
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (ToLowerWide(pe.szExeFile) == wanted) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return std::nullopt;
}

std::optional<ModuleInfo> MemoryEngine::FindModuleInProcess(DWORD pid, const std::wstring& module_name) {
    static auto NtQueryInformationProcess = GetNtFunction<pfnNtQueryInformationProcess>("NtQueryInformationProcess");
    if (!NtQueryInformationProcess) return std::nullopt;

    HANDLE process = NtOpenProcessWrapper(pid, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ);
    if (!process) return std::nullopt;

    PROCESS_BASIC_INFORMATION pbi{};
    if (!NT_SUCCESS(NtQueryInformationProcess(process, ProcessBasicInformation, &pbi, sizeof(pbi), nullptr))) {
        CloseHandle(process);
        return std::nullopt;
    }

    if (!pbi.PebBaseAddress) {
        CloseHandle(process);
        return std::nullopt;
    }

    PEB_LDR_DATA ldr_data{};
    // Offset to Ldr in PEB is 0x18 for x64
    PVOID ldr_ptr = nullptr;
    if (!ReadProcessMemory(process, reinterpret_cast<PVOID>(reinterpret_cast<ULONG_PTR>(pbi.PebBaseAddress) + 0x18), &ldr_ptr, sizeof(ldr_ptr), nullptr) || !ldr_ptr) {
        CloseHandle(process);
        return std::nullopt;
    }

    if (!ReadProcessMemory(process, ldr_ptr, &ldr_data, sizeof(ldr_data), nullptr)) {
        CloseHandle(process);
        return std::nullopt;
    }

    const std::wstring wanted = ToLowerWide(module_name);
    LIST_ENTRY* start = &ldr_data.InLoadOrderModuleList;
    LIST_ENTRY* current_node = ldr_data.InLoadOrderModuleList.Flink;

    // Use a safety counter to avoid infinite loops if memory is corrupted
    for (int i = 0; i < 512 && current_node != start && current_node != nullptr; ++i) {
        LDR_DATA_TABLE_ENTRY entry{};
        if (!ReadProcessMemory(process, current_node, &entry, sizeof(entry), nullptr)) break;

        if (entry.BaseDllName.Buffer != nullptr) {
            std::wstring name;
            name.resize(entry.BaseDllName.Length / sizeof(wchar_t));
            if (ReadProcessMemory(process, entry.BaseDllName.Buffer, &name[0], entry.BaseDllName.Length, nullptr)) {
                if (ToLowerWide(name) == wanted) {
                    ModuleInfo info;
                    info.name = name;
                    info.base = reinterpret_cast<std::uint64_t>(entry.DllBase);
                    info.size = entry.SizeOfImage;
                    
                    // Also try to get path if needed, though name is usually enough
                    std::wstring path;
                    path.resize(entry.FullDllName.Length / sizeof(wchar_t));
                    if (ReadProcessMemory(process, entry.FullDllName.Buffer, &path[0], entry.FullDllName.Length, nullptr)) {
                        info.path = path;
                    }

                    CloseHandle(process);
                    return info;
                }
            }
        }

        current_node = entry.InLoadOrderLinks.Flink;
    }

    CloseHandle(process);
    return std::nullopt;
}

bool MemoryEngine::TryMapChunkViaBstkVmm(std::uint32_t chunk_id,
                                         std::uint64_t& out_chunk_ptr,
                                         bool verbose) {
    out_chunk_ptr = 0;
    if (bstk_module_base_ == 0 || process_id_ == 0) {
        return false;
    }

    HANDLE remote_process = NtOpenProcessWrapper(
        process_id_,
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE);
    if (remote_process == nullptr) {
        return false;
    }

    struct RemoteChunkMapResult {
        std::uint64_t chunk_ptr = 0;
        std::int64_t status = 0;
    };

    constexpr std::size_t kCodeSize = 64;
    const std::size_t total_size = sizeof(RemoteChunkMapResult) + kCodeSize;
    auto cleanup = [&](LPVOID remote_mem) {
        if (remote_mem != nullptr) {
            VirtualFreeEx(remote_process, remote_mem, 0, MEM_RELEASE);
        }
        CloseHandle(remote_process);
    };

    std::uint8_t* remote_mem = static_cast<std::uint8_t*>(
        VirtualAllocEx(remote_process, nullptr, total_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (remote_mem == nullptr) {
        cleanup(nullptr);
        return false;
    }

    const std::uint64_t remote_result = reinterpret_cast<std::uint64_t>(remote_mem);
    const std::uint64_t remote_code = remote_result + sizeof(RemoteChunkMapResult);
    const std::uint64_t remote_func = bstk_module_base_ + kBstkMapChunkRva;

    std::vector<std::uint8_t> stub = {
        0x48, 0x83, 0xEC, 0x28,
        0x48, 0xB9,
        0, 0, 0, 0, 0, 0, 0, 0,
        0xBA,
        0, 0, 0, 0,
        0x49, 0xB8,
        0, 0, 0, 0, 0, 0, 0, 0,
        0x48, 0xB8,
        0, 0, 0, 0, 0, 0, 0, 0,
        0xFF, 0xD0,
        0x48, 0xB9,
        0, 0, 0, 0, 0, 0, 0, 0,
        0x48, 0x63, 0xC0,
        0x48, 0x89, 0x41, 0x08,
        0x48, 0x83, 0xC4, 0x28,
        0xC3
    };

    auto patch_u64 = [&](std::size_t offset, std::uint64_t value) {
        std::memcpy(stub.data() + offset, &value, sizeof(value));
    };
    auto patch_u32 = [&](std::size_t offset, std::uint32_t value) {
        std::memcpy(stub.data() + offset, &value, sizeof(value));
    };

    patch_u64(6, auto_vm_.pvm);
    patch_u32(15, chunk_id);
    patch_u64(21, remote_result);
    patch_u64(31, remote_func);
    patch_u64(43, remote_result);

    RemoteChunkMapResult initial{};
    SIZE_T written = 0;
    if (!WriteProcessMemory(remote_process, remote_mem, &initial, sizeof(initial), &written) || written != sizeof(initial) ||
        !WriteProcessMemory(remote_process, reinterpret_cast<LPVOID>(remote_code), stub.data(), stub.size(), &written) || written != stub.size()) {
        cleanup(remote_mem);
        return false;
    }

    HANDLE thread = CreateRemoteThread(
        remote_process,
        nullptr,
        0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(remote_code),
        nullptr,
        0,
        nullptr);
    if (thread == nullptr) {
        cleanup(remote_mem);
        return false;
    }

    const DWORD wait = WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    if (wait != WAIT_OBJECT_0) {
        cleanup(remote_mem);
        return false;
    }

    RemoteChunkMapResult result{};
    SIZE_T read = 0;
    const bool ok =
        ReadProcessMemory(remote_process, remote_mem, &result, sizeof(result), &read) &&
        read == sizeof(result);
    cleanup(remote_mem);
    if (!ok || result.status < 0 || result.chunk_ptr == 0) {
        if (verbose) {
            std::cout << "[map] remote chunk map failed for chunk " << chunk_id
                      << " status=" << Hex(static_cast<std::uint64_t>(result.status)) << "\n";
        }
        return false;
    }

    if (verbose) {
        std::cout << "[map] remote chunk map succeeded for chunk " << chunk_id
                  << " chunk_ptr=" << Hex(result.chunk_ptr) << "\n";
    }
    out_chunk_ptr = result.chunk_ptr;
    return true;
}

std::optional<std::uint64_t> MemoryEngine::FindChunkFromCache(std::uint32_t chunk_id) const {
    const auto slot = chunk_id & 0x3F;
    const auto slot_addr = auto_vm_.pvm + Offsets::VmChunkCache + static_cast<std::uint64_t>(slot) * 0x10;

    std::uint32_t cached_id = 0;
    std::uint64_t chunk_ptr = 0;
    if (!ReadHostValue(slot_addr, cached_id) || !ReadHostValue(slot_addr + 0x8, chunk_ptr)) {
        return std::nullopt;
    }
    if (cached_id != chunk_id || !IsLikelyUserPointer(chunk_ptr)) {
        return std::nullopt;
    }
    return chunk_ptr;
}

std::optional<std::uint64_t> MemoryEngine::FindChunkFromTree(std::uint32_t chunk_id, bool verbose) const {
    constexpr std::uint64_t kAvlLeft = 0x0;
    constexpr std::uint64_t kAvlRight = 0x8;
    constexpr std::uint64_t kAvlKey = 0x10;

    std::uint64_t node = 0;
    if (!ReadHostValue(auto_vm_.pvm + Offsets::VmChunkTreeRoot, node)) {
        return std::nullopt;
    }

    while (node != 0) {
        std::uint32_t key = 0;
        if (!ReadHostValue(node + kAvlKey, key)) {
            return std::nullopt;
        }

        if (verbose) {
            std::cout << "[map] chunk-tree node=" << Hex(node)
                      << " key=" << key << "\n";
        }

        if (key == chunk_id) {
            return node;
        }

        const std::uint64_t next_offset = chunk_id < key ? kAvlLeft : kAvlRight;
        if (!ReadHostValue(node + next_offset, node)) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<std::uint64_t> MemoryEngine::FindPhysRange(std::uint64_t guest_phys) const {
    const auto slot = (guest_phys >> 20) & 0x7ULL;
    const auto cache_addr = auto_vm_.pvm + Offsets::VmRangeCache + slot * sizeof(std::uint64_t);

    std::uint64_t node = 0;
    if (!ReadHostValue(cache_addr, node)) {
        return std::nullopt;
    }

    auto range_contains = [&](std::uint64_t candidate) -> std::optional<bool> {
        if (candidate == 0) {
            return false;
        }

        std::uint64_t start = 0;
        std::uint64_t size = 0;
        if (!ReadHostValue(candidate + 0x0, start) || !ReadHostValue(candidate + 0x8, size)) {
            return std::nullopt;
        }

        return guest_phys >= start && guest_phys - start < size;
    };

    if (node != 0) {
        const auto contains = range_contains(node);
        if (!contains.has_value()) {
            return std::nullopt;
        }
        if (*contains) {
            return node;
        }
    }

    if (!ReadHostValue(auto_vm_.pvm + Offsets::VmRangeTreeRoot, node)) {
        return std::nullopt;
    }

    while (node != 0) {
        std::uint64_t start = 0;
        std::uint64_t size = 0;
        if (!ReadHostValue(node + 0x0, start) || !ReadHostValue(node + 0x8, size)) {
            return std::nullopt;
        }

        if (guest_phys >= start && guest_phys - start < size) {
            return node;
        }

        if (guest_phys < start) {
            if (!ReadHostValue(node + 0x50, node)) {
                return std::nullopt;
            }
        } else {
            if (!ReadHostValue(node + 0x58, node)) {
                return std::nullopt;
            }
        }
    }

    return std::nullopt;
}

std::optional<std::uint64_t> MemoryEngine::TryPageMapCache(std::uint64_t guest_phys, bool verbose) const {
    const auto slot = (guest_phys >> 12) & 0xFFULL;
    const auto entry = auto_vm_.pvm + Offsets::VmPageMapCache + slot * 0x20;
    const auto page_tag = guest_phys & ~0xFFFULL;

    std::uint64_t cached_tag = 0;
    std::uint64_t host_base = 0;
    std::uint64_t page_desc = 0;
    if (!ReadHostValue(entry + 0x0, cached_tag) ||
        !ReadHostValue(entry + 0x8, page_desc) ||
        !ReadHostValue(entry + 0x18, host_base)) {
        VerboseLog(verbose, "[map] failed to read page-map cache entry");
        return std::nullopt;
    }

    if (verbose) {
        std::cout << "[map] page-cache slot=" << slot
                  << " tag=" << Hex(cached_tag)
                  << " desc=" << Hex(page_desc)
                  << " host=" << Hex(host_base) << "\n";
    }

    if (cached_tag == page_tag && IsLikelyUserPointer(host_base)) {
        VerboseLog(verbose, "[map] page-cache hit");
        return host_base | (guest_phys & 0xFFFULL);
    }

    VerboseLog(verbose, "[map] page-cache miss");
    return std::nullopt;
}

std::optional<std::uint64_t> MemoryEngine::MapPhysPageToHostBase(std::uint64_t guest_phys, bool verbose) {
    const auto range = FindPhysRange(guest_phys);
    if (!range.has_value()) {
        VerboseLog(verbose, "[map] no physical range found for GPA " + Hex(guest_phys));
        return std::nullopt;
    }

    std::uint64_t range_start = 0;
    if (!ReadHostValue(*range + 0x0, range_start)) {
        return std::nullopt;
    }

    const auto page_index = (guest_phys - range_start) >> 12;
    const auto desc = *range + 0x70 + page_index * 0x10;

    std::uint64_t state = 0;
    std::uint32_t info = 0;
    if (!ReadHostValue(desc + 0x0, state) || !ReadHostValue(desc + 0x8, info)) {
        return std::nullopt;
    }

    const auto page_class = static_cast<std::uint32_t>((state >> 51) & 0x7);
    const auto read_map_kind = static_cast<std::uint32_t>((state >> 48) & 0x3);
    std::uint8_t ram_range_mode = 0;
    if (!ReadHostValue(auto_vm_.pvm + Offsets::VmRamRangeMode, ram_range_mode)) {
        ram_range_mode = 0;
    }

    if (verbose) {
        std::cout << "[map] desc=" << Hex(desc)
                  << " state=" << Hex(state)
                  << " info=" << Hex(info)
                  << " page_class=" << page_class
                  << " read_map_kind=" << read_map_kind
                  << " ram_range_mode=" << static_cast<unsigned>(ram_range_mode)
                  << "\n";
    }

    if (page_class != 2 && page_class != 3 && ram_range_mode != 0) {
        std::uint64_t range_host_base = 0;
        if (!ReadHostValue(*range + 0x30, range_host_base) || !IsLikelyUserPointer(range_host_base)) {
            return std::nullopt;
        }
        return range_host_base + ((guest_phys - range_start) & ~0xFFFULL);
    }

    if (read_map_kind == 0) {
        return auto_vm_.pvm + Offsets::VmDirectRamWindow;
    }

    if (page_class == 2 || page_class == 3) {
        const std::uint32_t mmio_id = info >> 24;
        const std::uint32_t mmio_page = info & 0x00FFFFFF;

        std::uint64_t mmio_range = 0;
        if (!ReadHostValue(auto_vm_.pvm + Offsets::VmMmio2Table + static_cast<std::uint64_t>(mmio_id) * sizeof(std::uint64_t), mmio_range) ||
            !IsLikelyUserPointer(mmio_range)) {
            return std::nullopt;
        }

        std::uint64_t host_base = 0;
        if (!ReadHostValue(mmio_range + 0x70, host_base)) {
            return std::nullopt;
        }
        return host_base + (static_cast<std::uint64_t>(mmio_page) << 12);
    }

    const std::uint32_t chunk_id = info >> 9;
    const std::uint32_t page_in_chunk = info & 0x1FF;
    if (chunk_id != 0) {
        auto chunk = FindChunkFromCache(chunk_id);
        if (!chunk.has_value()) {
            chunk = FindChunkFromTree(chunk_id, verbose);
            if (!chunk.has_value()) {
                std::uint64_t remote_chunk = 0;
                if (TryMapChunkViaBstkVmm(chunk_id, remote_chunk, verbose)) {
                    chunk = remote_chunk;
                }
            }
            if (!chunk.has_value()) {
                return std::nullopt;
            }
        }

        std::uint64_t chunk_host_base = 0;
        if (!ReadHostValue(*chunk + 0x28, chunk_host_base)) {
            return std::nullopt;
        }
        return chunk_host_base + (static_cast<std::uint64_t>(page_in_chunk) << 12);
    }

    if (info == 0) {
        std::uint64_t direct_tag = 0;
        if (!ReadHostValue(auto_vm_.pvm + Offsets::VmDirectRamTag, direct_tag)) {
            return std::nullopt;
        }

        const bool direct_class = page_class == 4;
        const bool direct_state_match =
            (state & 0x7000000000000ULL) == 0 &&
            (state & 0x0FFFFFFFFFF000ULL) == direct_tag;

        if (direct_class || direct_state_match) {
            return auto_vm_.pvm + Offsets::VmDirectRamWindow;
        }
    }

    return std::nullopt;
}

std::optional<std::uint64_t> MemoryEngine::MapPhysToHost(std::uint64_t guest_phys, bool verbose) {
    if (const auto cached_host_page = LookupCachedHostPage(guest_phys); cached_host_page.has_value()) {
        return *cached_host_page | (guest_phys & 0xFFFULL);
    }

    const auto cached = TryPageMapCache(guest_phys, verbose);
    if (cached.has_value()) {
        StoreCachedHostPage(guest_phys, *cached & ~0xFFFULL);
        return cached;
    }

    const auto page_base = MapPhysPageToHostBase(guest_phys, verbose);
    if (!page_base.has_value()) {
        return std::nullopt;
    }
    StoreCachedHostPage(guest_phys, *page_base);
    return *page_base | (guest_phys & 0xFFFULL);
}

std::optional<TranslationResult> MemoryEngine::ResolveGuestPhysical(std::uint64_t guest_phys, bool verbose) {
    const auto host = MapPhysToHost(guest_phys, verbose);
    if (!host.has_value()) {
        return std::nullopt;
    }

    TranslationResult result;
    result.translated_phys = guest_phys;
    result.host_ptr = *host;
    return result;
}

bool MemoryEngine::ReadGuestPhysicalBytes(std::uint64_t guest_phys,
                                          void* buffer,
                                          std::size_t size,
                                          bool verbose) {
    auto* out = static_cast<std::uint8_t*>(buffer);
    std::size_t copied = 0;

    while (copied < size) {
        const std::uint64_t current_phys = guest_phys + copied;
        const std::size_t chunk = std::min<std::size_t>(size - copied, 0x1000ULL - (current_phys & 0xFFFULL));
        const auto translated = ResolveGuestPhysical(current_phys, verbose);
        if (!translated.has_value()) {
            return false;
        }
        if (!ReadHostBytes(translated->host_ptr, out + copied, chunk)) {
            InvalidateCachedHostPage(current_phys);
            const auto retried = ResolveGuestPhysical(current_phys, verbose);
            if (!retried.has_value() || !ReadHostBytes(retried->host_ptr, out + copied, chunk)) {
                return false;
            }
        }
        copied += chunk;
    }

    return true;
}

bool MemoryEngine::WriteGuestPhysicalBytes(std::uint64_t guest_phys,
                                           const void* buffer,
                                           std::size_t size,
                                           bool verbose) {
    const auto* in = static_cast<const std::uint8_t*>(buffer);
    std::size_t written = 0;

    while (written < size) {
        const std::uint64_t current_phys = guest_phys + written;
        const std::size_t chunk = std::min<std::size_t>(size - written, 0x1000ULL - (current_phys & 0xFFFULL));
        const auto translated = ResolveGuestPhysical(current_phys, verbose);
        if (!translated.has_value()) {
            return false;
        }
        if (!WriteHostBytes(translated->host_ptr, in + written, chunk)) {
            InvalidateCachedHostPage(current_phys);
            const auto retried = ResolveGuestPhysical(current_phys, verbose);
            if (!retried.has_value() || !WriteHostBytes(retried->host_ptr, in + written, chunk)) {
                return false;
            }
        }
        written += chunk;
    }

    return true;
}

bool MemoryEngine::ReadGuestKernelBytes(std::uint64_t kernel_va,
                                        void* buffer,
                                        std::size_t size,
                                        bool verbose) {
    auto* out = static_cast<std::uint8_t*>(buffer);
    std::size_t copied = 0;

    while (copied < size) {
        const std::uint64_t current_va = kernel_va + copied;
        const std::size_t chunk = std::min<std::size_t>(size - copied, 0x1000ULL - (current_va & 0xFFFULL));
        std::uint64_t current_phys = 0;
        if (!TryConvertKernelVirtualToPhysical(current_va, current_phys)) {
            return false;
        }
        if (!ReadGuestPhysicalBytes(current_phys, out + copied, chunk, verbose)) {
            return false;
        }
        copied += chunk;
    }

    return true;
}

bool MemoryEngine::WriteGuestKernelBytes(std::uint64_t kernel_va,
                                         const void* buffer,
                                         std::size_t size,
                                         bool verbose) {
    const auto* in = static_cast<const std::uint8_t*>(buffer);
    std::size_t written = 0;

    while (written < size) {
        const std::uint64_t current_va = kernel_va + written;
        const std::size_t chunk = std::min<std::size_t>(size - written, 0x1000ULL - (current_va & 0xFFFULL));
        std::uint64_t current_phys = 0;
        if (!TryConvertKernelVirtualToPhysical(current_va, current_phys)) {
            return false;
        }
        if (!WriteGuestPhysicalBytes(current_phys, in + written, chunk, verbose)) {
            return false;
        }
        written += chunk;
    }

    return true;
}

std::optional<std::string> MemoryEngine::ReadGuestKernelCStringBounded(std::uint64_t kernel_va,
                                                                       std::size_t max_chars,
                                                                       bool verbose) {
    if (kernel_va == 0 || max_chars == 0) {
        return std::nullopt;
    }

    std::string out;
    out.reserve(max_chars);

    for (std::size_t i = 0; i < max_chars; ++i) {
        char ch = '\0';
        if (!ReadGuestKernelObject(kernel_va + i, ch, verbose)) {
            break;
        }
        if (ch == '\0') {
            break;
        }
        out.push_back(ch);
    }

    if (out.empty()) {
        return std::nullopt;
    }
    return SanitizeGuestString(std::move(out));
}

std::optional<std::string> MemoryEngine::ReadGuestKernelString(std::uint64_t kernel_va,
                                                               std::size_t size,
                                                               bool verbose) {
    if (kernel_va == 0 || size == 0) {
        return std::nullopt;
    }

    std::vector<char> data(size + 1, '\0');
    if (!ReadGuestKernelBytes(kernel_va, data.data(), size, verbose)) {
        return std::nullopt;
    }
    return SanitizeGuestString(std::string(data.data(), size));
}

std::optional<std::uint64_t> MemoryEngine::ReadMmCr3Exact(std::uint64_t mm_va, bool verbose) {
    std::uint64_t pgd_kernel_va = 0;
    if (!ReadGuestKernelObject(mm_va + offsetof(GuestMmStruct, pgd), pgd_kernel_va, verbose) || pgd_kernel_va == 0) {
        return std::nullopt;
    }

    std::uint64_t pgd_phys = 0;
    if (!TryConvertKernelVirtualToPhysical(pgd_kernel_va, pgd_phys)) {
        return std::nullopt;
    }
    return pgd_phys;
}

std::optional<std::uint64_t> MemoryEngine::TranslateGuestVa64ByCr3(std::uint64_t guest_cr3,
                                                                   std::uint64_t guest_va,
                                                                   bool verbose) {
    constexpr std::uint64_t kPresentBit = 1ULL << 0;
    constexpr std::uint64_t kPageSizeBit = 1ULL << 7;

    const std::uint64_t canonical_check = guest_va >> 47;
    if (canonical_check != 0 && canonical_check != 0x1FFFFULL) {
        return std::nullopt;
    }

    if (const auto cached_phys_page = LookupCachedPhysPage(guest_cr3, guest_va); cached_phys_page.has_value()) {
        return *cached_phys_page | (guest_va & 0xFFFULL);
    }

    const std::uint64_t pml4_base = guest_cr3 & kLongModePageFrameMask;
    std::uint64_t pml4e = 0;
    if (!ReadGuestPhysicalObject(
            pml4_base + ((guest_va >> 39) & 0x1FFULL) * sizeof(std::uint64_t),
            pml4e,
            verbose) ||
        (pml4e & kPresentBit) == 0) {
        return std::nullopt;
    }

    const std::uint64_t pdpt_base = pml4e & kLongModePageFrameMask;
    std::uint64_t pdpte = 0;
    if (!ReadGuestPhysicalObject(
            pdpt_base + ((guest_va >> 30) & 0x1FFULL) * sizeof(std::uint64_t),
            pdpte,
            verbose) ||
        (pdpte & kPresentBit) == 0) {
        return std::nullopt;
    }
    if ((pdpte & kPageSizeBit) != 0) {
        const std::uint64_t resolved = (pdpte & 0x000FFFFFFFE00000ULL) | (guest_va & 0x3FFFFFFFULL);
        StoreCachedPhysPage(guest_cr3, guest_va, resolved & ~0xFFFULL);
        return resolved;
    }

    const std::uint64_t pd_base = pdpte & kLongModePageFrameMask;
    std::uint64_t pde = 0;
    if (!ReadGuestPhysicalObject(
            pd_base + ((guest_va >> 21) & 0x1FFULL) * sizeof(std::uint64_t),
            pde,
            verbose) ||
        (pde & kPresentBit) == 0) {
        return std::nullopt;
    }
    if ((pde & kPageSizeBit) != 0) {
        const std::uint64_t resolved = (pde & 0x000FFFFFFFE00000ULL) | (guest_va & 0x1FFFFFULL);
        StoreCachedPhysPage(guest_cr3, guest_va, resolved & ~0xFFFULL);
        return resolved;
    }

    const std::uint64_t pt_base = pde & kLongModePageFrameMask;
    std::uint64_t pte = 0;
    if (!ReadGuestPhysicalObject(
            pt_base + ((guest_va >> 12) & 0x1FFULL) * sizeof(std::uint64_t),
            pte,
            verbose) ||
        (pte & kPresentBit) == 0) {
        return std::nullopt;
    }

    const std::uint64_t resolved = (pte & kLongModePageFrameMask) | (guest_va & 0xFFFULL);
    StoreCachedPhysPage(guest_cr3, guest_va, resolved & ~0xFFFULL);
    return resolved;
}

bool MemoryEngine::ReadGuestUserBytesByCr3(std::uint64_t guest_cr3,
                                           std::uint64_t guest_va,
                                           void* buffer,
                                           std::size_t size,
                                           bool verbose) {
    auto* out = static_cast<std::uint8_t*>(buffer);
    std::size_t copied = 0;

    while (copied < size) {
        const std::uint64_t current_va = guest_va + copied;
        const std::size_t chunk = std::min<std::size_t>(size - copied, 0x1000ULL - (current_va & 0xFFFULL));
        const std::uint64_t page_offset = current_va & 0xFFFULL;

        if (const auto host_page = LookupCachedHostPageByGva(guest_cr3, current_va); host_page.has_value()) {
            if (ReadHostBytes(*host_page + page_offset, out + copied, chunk)) {
                copied += chunk;
                continue;
            }
            InvalidateCachedHostPageByGva(guest_cr3, current_va);
        }

        bool read_ok = false;
        for (int attempt = 0; attempt < 2 && !read_ok; ++attempt) {
            const auto phys = TranslateGuestVa64ByCr3(guest_cr3, current_va, verbose);
            if (!phys.has_value()) {
                InvalidateCachedPhysPage(guest_cr3, current_va);
                continue;
            }

            const auto host = MapPhysToHost(*phys, verbose);
            if (!host.has_value()) {
                InvalidateCachedPhysPage(guest_cr3, current_va);
                InvalidateCachedHostPage(*phys);
                continue;
            }

            if (ReadHostBytes(*host, out + copied, chunk)) {
                StoreCachedHostPageByGva(guest_cr3, current_va, *host & ~0xFFFULL);
                read_ok = true;
                break;
            }

            InvalidateCachedHostPageByGva(guest_cr3, current_va);
            InvalidateCachedPhysPage(guest_cr3, current_va);
            InvalidateCachedHostPage(*phys);
        }

        if (!read_ok) {
            return false;
        }
        copied += chunk;
    }

    return true;
}

bool MemoryEngine::WriteGuestUserBytesByCr3(std::uint64_t guest_cr3,
                                            std::uint64_t guest_va,
                                            const void* buffer,
                                            std::size_t size,
                                            bool verbose) {
    const auto* in = static_cast<const std::uint8_t*>(buffer);
    std::size_t written = 0;

    while (written < size) {
        const std::uint64_t current_va = guest_va + written;
        const std::size_t chunk = std::min<std::size_t>(size - written, 0x1000ULL - (current_va & 0xFFFULL));
        const std::uint64_t page_offset = current_va & 0xFFFULL;

        if (const auto host_page = LookupCachedHostPageByGva(guest_cr3, current_va); host_page.has_value()) {
            if (WriteHostBytes(*host_page + page_offset, in + written, chunk)) {
                written += chunk;
                continue;
            }
            InvalidateCachedHostPageByGva(guest_cr3, current_va);
        }

        bool write_ok = false;
        for (int attempt = 0; attempt < 2 && !write_ok; ++attempt) {
            const auto phys = TranslateGuestVa64ByCr3(guest_cr3, current_va, verbose);
            if (!phys.has_value()) {
                InvalidateCachedPhysPage(guest_cr3, current_va);
                continue;
            }

            const auto host = MapPhysToHost(*phys, verbose);
            if (!host.has_value()) {
                InvalidateCachedPhysPage(guest_cr3, current_va);
                InvalidateCachedHostPage(*phys);
                continue;
            }

            if (WriteHostBytes(*host, in + written, chunk)) {
                StoreCachedHostPageByGva(guest_cr3, current_va, *host & ~0xFFFULL);
                write_ok = true;
                break;
            }

            InvalidateCachedHostPageByGva(guest_cr3, current_va);
            InvalidateCachedPhysPage(guest_cr3, current_va);
            InvalidateCachedHostPage(*phys);
        }

        if (!write_ok) {
            return false;
        }
        written += chunk;
    }

    return true;
}

bool MemoryEngine::ReadGVA(std::uint64_t guest_cr3,
                           std::uint64_t guest_va,
                           void* buffer,
                           std::size_t size,
                           bool verbose) {
    return ReadGuestUserBytesByCr3(guest_cr3, guest_va, buffer, size, verbose);
}

bool MemoryEngine::WriteGVA(std::uint64_t guest_cr3,
                            std::uint64_t guest_va,
                            const void* buffer,
                            std::size_t size,
                            bool verbose) {
    return WriteGuestUserBytesByCr3(guest_cr3, guest_va, buffer, size, verbose);
}

std::optional<std::string> MemoryEngine::ReadGuestUserCStringByCr3(std::uint64_t guest_cr3,
                                                                   std::uint64_t guest_va,
                                                                   std::size_t max_chars,
                                                                   bool verbose) {
    if (guest_va == 0 || max_chars == 0) {
        return std::nullopt;
    }

    std::string out;
    out.reserve(max_chars);

    for (std::size_t i = 0; i < max_chars; ++i) {
        char ch = '\0';
        if (!ReadGuestUserObjectByCr3(guest_cr3, guest_va + i, ch, verbose)) {
            break;
        }
        if (ch == '\0') {
            break;
        }
        out.push_back(ch);
    }

    if (out.empty()) {
        return std::nullopt;
    }
    return SanitizeGuestString(std::move(out));
}

}  // namespace externaltest
