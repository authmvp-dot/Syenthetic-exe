#pragma once

#include "memoryengine.hpp"

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace externaltest {

class GvaMemoryBridge {
public:
    GvaMemoryBridge() = default;

    void Attach(MemoryEngine* memory, std::uint64_t guest_cr3);
    bool IsReady() const;
    std::uint64_t cr3() const;

    template <typename T>
    T ReadS(std::uint64_t guest_va, bool verbose = false) const {
        static_assert(std::is_trivially_copyable_v<T>, "ReadS requires a POD-like type");
        T result{};
        if (!Read(guest_va, result, verbose)) {
            return T{};
        }
        return result;
    }

    template <typename T>
    bool Read(std::uint64_t guest_va, T& out_value, bool verbose = false) const {
        static_assert(std::is_trivially_copyable_v<T>, "Read requires a POD-like type");
        if (!IsReady()) {
            return false;
        }
        return memory_->ReadGVA(guest_cr3_, guest_va, out_value, verbose);
    }

    template <typename T>
    bool Write(std::uint64_t guest_va, const T& value, bool verbose = false) const {
        static_assert(std::is_trivially_copyable_v<T>, "Write requires a POD-like type");
        if (!IsReady()) {
            return false;
        }
        return memory_->WriteGVA(guest_cr3_, guest_va, value, verbose);
    }

    bool ReadArray(std::uint64_t guest_va, void* buffer, std::size_t byte_count, bool verbose = false) const;
    bool WriteArray(std::uint64_t guest_va, const void* buffer, std::size_t byte_count, bool verbose = false) const;
    std::string String(std::uint64_t guest_va, int size, bool unicode, bool verbose = false) const;
    void ClearTranslationCaches() const;

private:
    MemoryEngine* memory_ = nullptr;
    std::uint64_t guest_cr3_ = 0;
};

}  // namespace externaltest
