#pragma once

namespace externaltest {

inline constexpr wchar_t kDefaultProcessName[] = L"HD-Player.exe";
inline constexpr wchar_t kDefaultModuleName[] = L"BstkVMM.dll";
inline constexpr char kDefaultGuestProcessFilter[] = "com.dts.freefireth";
inline constexpr char kDefaultGuestProcessFilterMax[] = "com.dts.freefiremax";
inline constexpr char kDefaultGuestLibraryFilter[] = "libil2cpp.so";

enum class DumpMode {
    FocusedLibrary = 1,
    FullMaps = 2,
    EspOverlay = 3,
};

}  // namespace externaltest
