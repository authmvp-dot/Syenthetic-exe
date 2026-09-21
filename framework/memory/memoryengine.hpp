#pragma once

#include <windows.h>

#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace externaltest {

struct ModuleInfo {
    std::wstring name;
    std::wstring path;
    std::uint64_t base = 0;
    std::uint32_t size = 0;
};

struct AutoVmInfo {
    std::uint64_t pvm = 0;
    std::uint64_t pvcpu0 = 0;
    std::uint32_t cpu_count = 0;
    std::uint64_t read_counter = 0;
    std::uint64_t region_size = 0;
};

struct TranslationResult {
    std::uint64_t translated_phys = 0;
    std::uint64_t host_ptr = 0;
};

class MemoryEngine {
public:
    MemoryEngine() = default;
    ~MemoryEngine();

    MemoryEngine(const MemoryEngine&) = delete;
    MemoryEngine& operator=(const MemoryEngine&) = delete;

    bool Initialize(bool verbose = false);
    bool IsInitialized() const;
    const std::string& LastError() const { return last_error_; }

    HANDLE process() const;
    DWORD process_id() const;
    const ModuleInfo& module() const;
    const AutoVmInfo& auto_vm() const;
    std::uint64_t pvm() const;
    std::uint64_t pvcpu() const;
    void ClearRuntimeCaches();

    std::optional<TranslationResult> ResolveGuestPhysical(std::uint64_t guest_phys, bool verbose = false);
    bool ReadGuestPhysicalBytes(std::uint64_t guest_phys, void* buffer, std::size_t size, bool verbose = false);
    bool WriteGuestPhysicalBytes(std::uint64_t guest_phys, const void* buffer, std::size_t size, bool verbose = false);
    bool ReadGuestKernelBytes(std::uint64_t kernel_va, void* buffer, std::size_t size, bool verbose = false);
    bool WriteGuestKernelBytes(std::uint64_t kernel_va, const void* buffer, std::size_t size, bool verbose = false);
    std::optional<std::string> ReadGuestKernelCStringBounded(std::uint64_t kernel_va,
                                                             std::size_t max_chars,
                                                             bool verbose = false);
    std::optional<std::string> ReadGuestKernelString(std::uint64_t kernel_va,
                                                     std::size_t size,
                                                     bool verbose = false);
    std::optional<std::uint64_t> ReadMmCr3Exact(std::uint64_t mm_va, bool verbose = false);
    std::optional<std::uint64_t> TranslateGuestVa64ByCr3(std::uint64_t guest_cr3,
                                                         std::uint64_t guest_va,
                                                         bool verbose = false);
    bool ReadGuestUserBytesByCr3(std::uint64_t guest_cr3,
                                 std::uint64_t guest_va,
                                 void* buffer,
                                 std::size_t size,
                                 bool verbose = false);
    bool WriteGuestUserBytesByCr3(std::uint64_t guest_cr3,
                                  std::uint64_t guest_va,
                                  const void* buffer,
                                  std::size_t size,
                                  bool verbose = false);
    std::optional<std::string> ReadGuestUserCStringByCr3(std::uint64_t guest_cr3,
                                                         std::uint64_t guest_va,
                                                         std::size_t max_chars,
                                                         bool verbose = false);

    // Generic guest-virtual wrappers (user-space via CR3), for external integrations.
    bool ReadGVA(std::uint64_t guest_cr3,
                 std::uint64_t guest_va,
                 void* buffer,
                 std::size_t size,
                 bool verbose = false);
    bool WriteGVA(std::uint64_t guest_cr3,
                  std::uint64_t guest_va,
                  const void* buffer,
                  std::size_t size,
                  bool verbose = false);

    template <typename T>
    bool ReadGuestPhysicalObject(std::uint64_t guest_phys, T& out_value, bool verbose = false) {
        static_assert(std::is_trivially_copyable_v<T>, "ReadGuestPhysicalObject requires a POD-like type");
        return ReadGuestPhysicalBytes(guest_phys, &out_value, sizeof(T), verbose);
    }

    template <typename T>
    bool ReadGuestKernelObject(std::uint64_t kernel_va, T& out_value, bool verbose = false) {
        static_assert(std::is_trivially_copyable_v<T>, "ReadGuestKernelObject requires a POD-like type");
        return ReadGuestKernelBytes(kernel_va, &out_value, sizeof(T), verbose);
    }

    template <typename T>
    bool ReadGuestUserObjectByCr3(std::uint64_t guest_cr3, std::uint64_t guest_va, T& out_value, bool verbose = false) {
        static_assert(std::is_trivially_copyable_v<T>, "ReadGuestUserObjectByCr3 requires a POD-like type");
        return ReadGuestUserBytesByCr3(guest_cr3, guest_va, &out_value, sizeof(T), verbose);
    }

    template <typename T>
    bool WriteGuestUserObjectByCr3(std::uint64_t guest_cr3, std::uint64_t guest_va, const T& value, bool verbose = false) {
        static_assert(std::is_trivially_copyable_v<T>, "WriteGuestUserObjectByCr3 requires a POD-like type");
        return WriteGuestUserBytesByCr3(guest_cr3, guest_va, &value, sizeof(T), verbose);
    }

    template <typename T>
    bool ReadGVA(std::uint64_t guest_cr3, std::uint64_t guest_va, T& out_value, bool verbose = false) {
        static_assert(std::is_trivially_copyable_v<T>, "ReadGVA requires a POD-like type");
        return ReadGVA(guest_cr3, guest_va, &out_value, sizeof(T), verbose);
    }

    template <typename T>
    bool WriteGVA(std::uint64_t guest_cr3, std::uint64_t guest_va, const T& value, bool verbose = false) {
        static_assert(std::is_trivially_copyable_v<T>, "WriteGVA requires a POD-like type");
        return WriteGVA(guest_cr3, guest_va, &value, sizeof(T), verbose);
    }

private:
    struct GvaPageKey {
        std::uint64_t cr3_page = 0;
        std::uint64_t va_page = 0;

        bool operator==(const GvaPageKey& other) const noexcept {
            return cr3_page == other.cr3_page && va_page == other.va_page;
        }
    };

    struct GvaPageKeyHash {
        std::size_t operator()(const GvaPageKey& key) const noexcept {
            const std::uint64_t mixed =
                key.cr3_page ^ (key.va_page + 0x9E3779B97F4A7C15ULL + (key.cr3_page << 6) + (key.cr3_page >> 2));
#if SIZE_MAX == UINT64_MAX
            return static_cast<std::size_t>(mixed);
#else
            return static_cast<std::size_t>(mixed ^ (mixed >> 32));
#endif
        }
    };

    void Reset();
    bool ReadHostBytes(std::uint64_t address, void* buffer, std::size_t size) const;
    bool WriteHostBytes(std::uint64_t address, const void* buffer, std::size_t size) const;

    template <typename T>
    bool ReadHostValue(std::uint64_t address, T& out_value) const {
        static_assert(std::is_trivially_copyable_v<T>, "ReadHostValue requires a POD-like type");
        return ReadHostBytes(address, &out_value, sizeof(T));
    }

    std::optional<AutoVmInfo> AutoLocateVmInfo() const;
    std::optional<std::uint64_t> FindChunkFromCache(std::uint32_t chunk_id) const;
    std::optional<std::uint64_t> FindChunkFromTree(std::uint32_t chunk_id, bool verbose) const;
    std::optional<std::uint64_t> FindPhysRange(std::uint64_t guest_phys) const;
    std::optional<std::uint64_t> TryPageMapCache(std::uint64_t guest_phys, bool verbose) const;
    std::optional<std::uint64_t> MapPhysPageToHostBase(std::uint64_t guest_phys, bool verbose);
    std::optional<std::uint64_t> MapPhysToHost(std::uint64_t guest_phys, bool verbose);
    bool TryMapChunkViaBstkVmm(std::uint32_t chunk_id, std::uint64_t& out_chunk_ptr, bool verbose);
    std::optional<std::uint64_t> LookupCachedPhysPage(std::uint64_t guest_cr3, std::uint64_t guest_va) const;
    void StoreCachedPhysPage(std::uint64_t guest_cr3, std::uint64_t guest_va, std::uint64_t guest_phys_page);
    void InvalidateCachedPhysPage(std::uint64_t guest_cr3, std::uint64_t guest_va);
    std::optional<std::uint64_t> LookupCachedHostPageByGva(std::uint64_t guest_cr3, std::uint64_t guest_va) const;
    void StoreCachedHostPageByGva(std::uint64_t guest_cr3, std::uint64_t guest_va, std::uint64_t host_page_base);
    void InvalidateCachedHostPageByGva(std::uint64_t guest_cr3, std::uint64_t guest_va);
    std::optional<std::uint64_t> LookupCachedHostPage(std::uint64_t guest_phys) const;
    void StoreCachedHostPage(std::uint64_t guest_phys, std::uint64_t host_page_base);
    void InvalidateCachedHostPage(std::uint64_t guest_phys);
    void ClearTranslationCaches();

    static std::optional<DWORD> FindProcessIdByName(const std::wstring& process_name);
    static std::optional<ModuleInfo> FindModuleInProcess(DWORD pid, const std::wstring& module_name);

    HANDLE process_ = nullptr;
    DWORD process_id_ = 0;
    ModuleInfo module_{};
    AutoVmInfo auto_vm_{};
    std::uint64_t bstk_module_base_ = 0;
    std::string last_error_;
    mutable std::shared_mutex cache_mutex_;
    std::unordered_map<GvaPageKey, std::uint64_t, GvaPageKeyHash> gva_to_phys_page_cache_;
    std::unordered_map<GvaPageKey, std::uint64_t, GvaPageKeyHash> gva_to_host_page_cache_;
    std::unordered_map<std::uint64_t, std::uint64_t> phys_to_host_page_cache_;
    static constexpr std::size_t kMaxGvaCacheEntries = 262144;
    static constexpr std::size_t kMaxGvaHostCacheEntries = 262144;
    static constexpr std::size_t kMaxPhysCacheEntries = 262144;
};

}  // namespace externaltest
