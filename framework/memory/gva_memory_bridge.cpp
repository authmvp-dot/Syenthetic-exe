#include "gva_memory_bridge.hpp"

#include "kernel_structs.hpp"

#include <Windows.h>

namespace externaltest {

void GvaMemoryBridge::Attach(MemoryEngine* memory, std::uint64_t guest_cr3) {
    memory_ = memory;
    guest_cr3_ = guest_cr3;
}

bool GvaMemoryBridge::IsReady() const {
    return memory_ != nullptr && guest_cr3_ != 0;
}

std::uint64_t GvaMemoryBridge::cr3() const {
    return guest_cr3_;
}

bool GvaMemoryBridge::ReadArray(std::uint64_t guest_va, void* buffer, std::size_t byte_count, bool verbose) const {
    if (!IsReady() || buffer == nullptr || byte_count == 0) {
        return false;
    }
    return memory_->ReadGVA(guest_cr3_, guest_va, buffer, byte_count, verbose);
}

bool GvaMemoryBridge::WriteArray(std::uint64_t guest_va, const void* buffer, std::size_t byte_count, bool verbose) const {
    if (!IsReady() || buffer == nullptr || byte_count == 0) {
        return false;
    }
    return memory_->WriteGVA(guest_cr3_, guest_va, buffer, byte_count, verbose);
}

std::string GvaMemoryBridge::String(std::uint64_t guest_va, int size, bool unicode, bool verbose) const {
    if (!IsReady() || guest_va == 0 || size <= 0) {
        return {};
    }

    std::vector<std::uint8_t> string_bytes(static_cast<std::size_t>(size), 0);
    if (!ReadArray(guest_va, string_bytes.data(), string_bytes.size(), verbose)) {
        return {};
    }

    if (!unicode) {
        std::string result(reinterpret_cast<const char*>(string_bytes.data()), string_bytes.size());
        const auto nul = result.find('\0');
        if (nul != std::string::npos) {
            result.resize(nul);
        }
        return SanitizeGuestString(std::move(result));
    }

    const auto* wide_ptr = reinterpret_cast<const wchar_t*>(string_bytes.data());
    const int wide_len = size / static_cast<int>(sizeof(wchar_t));
    if (wide_len <= 0) {
        return {};
    }

    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide_ptr, wide_len, nullptr, 0, nullptr, nullptr);
    if (utf8_len <= 0) {
        return {};
    }

    std::string out(static_cast<std::size_t>(utf8_len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide_ptr, wide_len, out.data(), utf8_len, nullptr, nullptr);
    const auto nul = out.find('\0');
    if (nul != std::string::npos) {
        out.resize(nul);
    }
    return SanitizeGuestString(std::move(out));
}

void GvaMemoryBridge::ClearTranslationCaches() const {
    if (memory_ != nullptr) {
        memory_->ClearRuntimeCaches();
    }
}

}  // namespace externaltest
