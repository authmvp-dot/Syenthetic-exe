#pragma once

#include "gva_memory_bridge.hpp"

#include <Windows.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class MemoryUtils {
public:
    inline static constexpr std::uintptr_t MIN_VALID_ADDRESS = 0x1000;

    std::unordered_map<std::uintptr_t, std::uintptr_t> Cache;

    static void BindBridge(externaltest::GvaMemoryBridge* bridge) {
        bridge_ = bridge;
    }

    static bool IsReady() {
        return bridge_ != nullptr && bridge_->IsReady();
    }

    static void ClearEngineCaches() {
        if (bridge_ != nullptr) {
            bridge_->ClearTranslationCaches();
        }
    }

    bool Convert(std::uintptr_t address, std::uintptr_t& phys) {
        phys = 0;
        if (!IsReady() || address <= MIN_VALID_ADDRESS) {
            return false;
        }

        auto cached = Cache.find(address);
        if (cached != Cache.end() && cached->second != 0) {
            phys = cached->second;
            return true;
        }

        // External backend reads guest-virtual directly (CR3 aware), so keep identity mapping.
        phys = address;
        Cache[address] = address;
        return true;
    }

    template <typename T>
    T ReadS(std::uintptr_t address) {
        T result{};
        if (!IsReady()) {
            return result;
        }
        if (!bridge_->Read(address, result)) {
            return T{};
        }
        return result;
    }

    template <typename T>
    bool Read(std::uintptr_t address, T& data) {
        if (!IsReady()) {
            return false;
        }
        return bridge_->Read(address, data);
    }

    template <typename T>
    void Write(std::uintptr_t address, const T& value) {
        if (!IsReady()) {
            return;
        }
        bridge_->Write(address, value);
    }

    template <typename T>
    bool ReadArray(std::uintptr_t address, std::vector<T>& array) {
        if (!IsReady() || array.empty()) {
            return false;
        }
        return bridge_->ReadArray(address, array.data(), sizeof(T) * array.size());
    }

    std::string utf16_to_utf8(const std::wstring& wstr) {
        std::string str;
        if (wstr.empty()) {
            return str;
        }

        const int len = WideCharToMultiByte(
            CP_UTF8,
            0,
            wstr.data(),
            static_cast<int>(wstr.size()),
            nullptr,
            0,
            nullptr,
            nullptr);
        if (len > 0) {
            str.resize(static_cast<std::size_t>(len));
            WideCharToMultiByte(
                CP_UTF8,
                0,
                wstr.data(),
                static_cast<int>(wstr.size()),
                str.data(),
                len,
                nullptr,
                nullptr);
        }
        return str;
    }

    std::string String(std::uintptr_t address, int size, bool unicode) {
        if (size <= 0) {
            return {};
        }

            // unicode=true: `size` is BYTE count (caller should pass nameLen * 2 for Il2Cpp)
        // Hard cap to avoid huge guest reads / OOM
        if (size > 512) {
            size = 512;
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size), 0);
        if (!ReadArray(address, bytes)) {
            return {};
        }

        std::string out;
        if (unicode) {
            const auto* wide = reinterpret_cast<const wchar_t*>(bytes.data());
            int wide_len = size / static_cast<int>(sizeof(wchar_t));
            if (wide_len <= 0) {
                return {};
            }
            // Trim at first null wchar (exact length like Og Esp)
            int real_len = 0;
            while (real_len < wide_len && wide[real_len] != 0) {
                ++real_len;
            }
            if (real_len <= 0) {
                return {};
            }
            // Reject obviously broken UTF-16 (surrogate alone is ok for WC2MB)
            std::wstring temp(wide, wide + real_len);
            out = utf16_to_utf8(temp);
        } else {
            out.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }

        const auto nul = out.find('\0');
        if (nul != std::string::npos) {
            out.resize(nul);
        }
        return out;
    }

private:
    inline static externaltest::GvaMemoryBridge* bridge_ = nullptr;
};

inline MemoryUtils Mem;
