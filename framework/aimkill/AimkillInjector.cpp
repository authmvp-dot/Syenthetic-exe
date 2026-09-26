#include "AimkillInjector.hpp"
#include "resource_ids.h"
#include "skStr.h"
#include "../memory/app_config.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>
#include <ctime>
#include <cctype>
#include <cstring>

namespace AimkillInjector {

    static std::string g_stagingDir = "";
    static std::string g_extractedInj = "";
    static std::string g_extractedLib = "";
    static const char* g_lastSource = "none";

    // Compile-time encrypted secrets (plain text not stored in .rdata)
    static std::string DefaultBaseUrl()
    {
        return std::string(skCrypt("https://raw.githubusercontent.com/authmvp-dot/libshitaim/main/main").decrypt());
    }
    static std::string DefaultToken()
    {
        static const uint8_t encToken[] = {
            0x3d, 0x32, 0x2a, 0x05, 0x23, 0x36, 0x6f, 0x33, 0x6d, 0x1c, 0x2c, 0x29, 0x3b, 0x19, 0x02, 0x1d,
            0x22, 0x2c, 0x29, 0x12, 0x09, 0x6a, 0x17, 0x17, 0x2d, 0x63, 0x69, 0x36, 0x1f, 0x18, 0x33, 0x2b,
            0x33, 0x17, 0x68, 0x3c, 0x11, 0x33, 0x62, 0x2a
        };
        std::string tok;
        tok.reserve(sizeof(encToken));
        for (size_t i = 0; i < sizeof(encToken); ++i)
            tok.push_back((char)(encToken[i] ^ 0x5A));
        return tok;
    }
    static std::string RemoteInjName()
    {
        return std::string(skCrypt("AndKittyInjector").decrypt());
    }
    static std::string RemoteLibName()
    {
        return std::string(skCrypt("libjifjf.so").decrypt());
    }

    // Payload blob XOR (disk .enc) — not string related
    static constexpr uint8_t kXorKey[] = {
        0xA7, 0x3C, 0x91, 0x5E, 0xD2, 0x48, 0xB6, 0x1F,
        0x6A, 0xE3, 0x09, 0x74, 0xC8, 0x2B, 0x95, 0x4D
    };

    const char* LastPayloadSource() { return g_lastSource; }

    // Runtime decrypt helper — keeps plain text out of IDA Strings
    #define XS(str) std::string(skCrypt(str).decrypt())

    static std::string GenerateRandomSuffix(size_t length = 8)
    {
        static const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz";
        static bool seeded = false;
        if (!seeded) {
            srand(static_cast<unsigned int>(time(nullptr)) ^ GetTickCount());
            seeded = true;
        }
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i)
            result += charset[rand() % (sizeof(charset) - 1)];
        return result;
    }

    static std::string GetTempRoot()
    {
        char tempPath[MAX_PATH] = {};
        GetTempPathA(MAX_PATH, tempPath);
        return std::string(tempPath);
    }

    static std::string GetPanelExeDir()
    {
        char path[MAX_PATH] = {};
        if (GetModuleFileNameA(nullptr, path, MAX_PATH) == 0)
            return {};
        std::string full(path);
        const size_t slash = full.find_last_of("\\/");
        if (slash != std::string::npos)
            full.resize(slash);
        return full;
    }

    static std::string GetEnvA(const char* name)
    {
        char buf[2048] = {};
        const DWORD n = GetEnvironmentVariableA(name, buf, sizeof(buf));
        if (n == 0 || n >= sizeof(buf))
            return {};
        return buf;
    }

    static void HardenPathA(const std::string& path)
    {
        if (path.empty())
            return;
        const DWORD a = GetFileAttributesA(path.c_str());
        if (a == INVALID_FILE_ATTRIBUTES)
            return;
        const DWORD want = (a | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)
            & ~FILE_ATTRIBUTE_ARCHIVE;
        if (a != want)
            SetFileAttributesA(path.c_str(), want);
    }

    static void EnsureDirHardened(const std::string& dir)
    {
        CreateDirectoryA(dir.c_str(), nullptr);
        HardenPathA(dir);
    }

    static void WipeFileA(const std::string& path)
    {
        if (path.empty())
            return;
        const DWORD prev = GetFileAttributesA(path.c_str());
        if (prev != INVALID_FILE_ATTRIBUTES)
            SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        // zero-fill small attempt then delete
        HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER sz{};
            if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0 && sz.QuadPart < (32ll << 20)) {
                std::vector<char> z(static_cast<size_t>(sz.QuadPart), 0);
                DWORD written = 0;
                SetFilePointer(h, 0, nullptr, FILE_BEGIN);
                WriteFile(h, z.data(), static_cast<DWORD>(z.size()), &written, nullptr);
            }
            CloseHandle(h);
        }
        DeleteFileA(path.c_str());
    }

    static void XorTransform(std::vector<uint8_t>& data)
    {
        if (data.empty())
            return;
        const size_t keyLen = sizeof(kXorKey);
        for (size_t i = 0; i < data.size(); ++i)
            data[i] ^= kXorKey[i % keyLen];
    }

    static bool WriteBytesToFile(const std::string& path, const std::vector<uint8_t>& data)
    {
        const DWORD prev = GetFileAttributesA(path.c_str());
        if (prev != INVALID_FILE_ATTRIBUTES)
            SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
            return false;
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        out.close();
        if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
            return false;
        HardenPathA(path);
        return true;
    }

    static bool LoadResourceBytes(int resourceId, std::vector<uint8_t>& out)
    {
        out.clear();
        HMODULE hModule = GetModuleHandleA(nullptr);
        HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(resourceId), RT_RCDATA);
        if (!hRes)
            return false;
        HGLOBAL hData = LoadResource(hModule, hRes);
        if (!hData)
            return false;
        DWORD resSize = SizeofResource(hModule, hRes);
        void* pData = LockResource(hData);
        if (!pData || resSize == 0)
            return false;
        out.assign(static_cast<const uint8_t*>(pData), static_cast<const uint8_t*>(pData) + resSize);
        return true;
    }

    static bool LoadFileBytes(const std::string& path, std::vector<uint8_t>& out)
    {
        out.clear();
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return false;
        out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        return !out.empty();
    }

    static bool ParseHttpsUrl(const std::string& url, std::wstring& host, std::wstring& path)
    {
        if (url.rfind(skCrypt("https://").decrypt(), 0) != 0)
            return false;
        size_t start = 8;
        size_t slash = url.find('/', start);
        std::string hostA = slash == std::string::npos ? url.substr(start) : url.substr(start, slash - start);
        std::string pathA = slash == std::string::npos ? "/" : url.substr(slash);
        if (hostA.empty())
            return false;

        wchar_t hostW[256] = {};
        wchar_t pathW[2048] = {};
        if (MultiByteToWideChar(CP_UTF8, 0, hostA.c_str(), -1, hostW, 256) <= 0)
            return false;
        if (MultiByteToWideChar(CP_UTF8, 0, pathA.c_str(), -1, pathW, 2048) <= 0)
            return false;
        host = hostW;
        path = pathW;
        return true;
    }

    static bool HttpGetBytes(const std::string& url, const std::string& token, std::vector<uint8_t>& out)
    {
        out.clear();
        std::wstring host, path;
        if (!ParseHttpsUrl(url, host, path))
            return false;

        HINTERNET ses = WinHttpOpen(skCrypt(L"Mozilla/5.0").decrypt(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!ses)
            return false;

        HINTERNET con = WinHttpConnect(ses, host.c_str(), 443, 0);
        if (!con) {
            WinHttpCloseHandle(ses);
            return false;
        }

        HINTERNET req = WinHttpOpenRequest(con, L"GET", path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!req) {
            WinHttpCloseHandle(con);
            WinHttpCloseHandle(ses);
            return false;
        }

        if (!token.empty()) {
            std::string authHdr = std::string(skCrypt("Authorization: ").decrypt());
            if (token.rfind(skCrypt("github_pat_").decrypt(), 0) == 0)
                authHdr += std::string(skCrypt("Bearer ").decrypt()) + token;
            else
                authHdr += std::string(skCrypt("token ").decrypt()) + token;

            std::wstring authHdrW;
            const int wlen = MultiByteToWideChar(CP_UTF8, 0, authHdr.c_str(), -1, nullptr, 0);
            if (wlen > 0) {
                authHdrW.resize(static_cast<size_t>(wlen));
                MultiByteToWideChar(CP_UTF8, 0, authHdr.c_str(), -1, authHdrW.data(), wlen);
                WinHttpAddRequestHeaders(req, authHdrW.c_str(), (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            }
        }

        const BOOL sent = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (!sent || !WinHttpReceiveResponse(req, nullptr)) {
            WinHttpCloseHandle(req);
            WinHttpCloseHandle(con);
            WinHttpCloseHandle(ses);
            return false;
        }

        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
        if (status != 200) {
            WinHttpCloseHandle(req);
            WinHttpCloseHandle(con);
            WinHttpCloseHandle(ses);
            return false;
        }

        DWORD avail = 0;
        do {
            if (!WinHttpQueryDataAvailable(req, &avail))
                break;
            if (avail == 0)
                break;
            std::vector<uint8_t> chunk(avail);
            DWORD read = 0;
            if (!WinHttpReadData(req, chunk.data(), avail, &read))
                break;
            out.insert(out.end(), chunk.begin(), chunk.begin() + read);
        } while (avail > 0);

        WinHttpCloseHandle(req);
        WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
        return out.size() > 1024;
    }

    static std::string JoinUrl(const std::string& base, const std::string& file)
    {
        if (base.empty())
            return {};
        if (base.back() == '/')
            return base + file;
        return base + "/" + file;
    }

    static bool TryDownloadGitHub(std::vector<uint8_t>& inj, std::vector<uint8_t>& lib)
    {
        std::string baseUrl = GetEnvA(skCrypt("AIMKILL_PAYLOAD_BASE_URL").decrypt());
        if (baseUrl.empty())
            baseUrl = DefaultBaseUrl();

        std::string token = GetEnvA(skCrypt("AIMKILL_PAYLOAD_TOKEN").decrypt());
        if (token.empty())
            token = DefaultToken();

        std::string injUrl = GetEnvA(skCrypt("AIMKILL_PAYLOAD_INJ_URL").decrypt());
        std::string libUrl = GetEnvA(skCrypt("AIMKILL_PAYLOAD_LIB_URL").decrypt());
        if (injUrl.empty())
            injUrl = JoinUrl(baseUrl, RemoteInjName());
        if (libUrl.empty())
            libUrl = JoinUrl(baseUrl, RemoteLibName());

        if (!HttpGetBytes(injUrl, token, inj))
            return false;
        if (!HttpGetBytes(libUrl, token, lib)) {
            inj.clear();
            return false;
        }
        if (inj.size() < 1024 || lib.size() < 32768)
            return false;

        g_lastSource = "net"; // generic — not "github"
        return true;
    }

    // EMBED XOR LOADER — COMMENTED (GitHub-only test)
    // static bool TryLoadEmbeddedXor(std::vector<uint8_t>& inj, std::vector<uint8_t>& lib)
    // {
    //     if (!LoadResourceBytes(IDR_INJECTOR, inj))
    //         return false;
    //     if (!LoadResourceBytes(IDR_LIBRARY, lib))
    //         return false;
    //     if (inj.size() < 1024 || lib.size() < 32768)
    //         return false;
    //     XorTransform(inj);
    //     XorTransform(lib);
    //     g_lastSource = "embedded";
    //     return true;
    // }

    // DISK FALLBACK — COMMENTED (GitHub-only test)
    // static bool TryLoadDiskPlain(std::vector<uint8_t>& inj, std::vector<uint8_t>& lib)
    // {
    //     ...
    // }

    // Execute a command hidden (no black window, no cmd.exe) and optionally capture stdout/stderr
    static bool ExecuteCommandHidden(const std::string& cmdLine, std::string* outOutput = nullptr, DWORD timeoutMs = 25000)
    {
        HANDLE hReadPipe = NULL, hWritePipe = NULL;
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        if (outOutput) {
            if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
                return false;
            }
            SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
        }

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (outOutput) {
            si.dwFlags |= STARTF_USESTDHANDLES;
            si.hStdOutput = hWritePipe;
            si.hStdError = hWritePipe;
        }

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back('\0');

        BOOL success = CreateProcessA(
            NULL,
            cmdBuf.data(),
            NULL,
            NULL,
            outOutput ? TRUE : FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &si,
            &pi
        );

        if (hWritePipe) {
            CloseHandle(hWritePipe);
            hWritePipe = NULL;
        }

        if (!success) {
            if (hReadPipe) CloseHandle(hReadPipe);
            return false;
        }

        if (outOutput) {
            char buffer[512];
            DWORD bytesRead = 0;
            while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                *outOutput += buffer;
            }
            CloseHandle(hReadPipe);
        }

        WaitForSingleObject(pi.hProcess, timeoutMs);

        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return true;
    }

    std::string FindAdbExecutable()
    {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32W);
            if (Process32FirstW(hSnap, &pe32)) {
                do {
                    if (_wcsicmp(pe32.szExeFile, skCrypt(L"HD-Player.exe").decrypt()) == 0) {
                        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID);
                        if (hProc) {
                            wchar_t pathBuf[MAX_PATH] = { 0 };
                            DWORD sz = MAX_PATH;
                            if (QueryFullProcessImageNameW(hProc, 0, pathBuf, &sz)) {
                                std::wstring dir = pathBuf;
                                size_t slash = dir.find_last_of(L"\\/");
                                if (slash != std::wstring::npos) {
                                    dir = dir.substr(0, slash);
                                    std::wstring hdAdb = dir + skCrypt(L"\\HD-Adb.exe").decrypt();
                                    DWORD attrib = GetFileAttributesW(hdAdb.c_str());
                                    if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                                        CloseHandle(hProc);
                                        CloseHandle(hSnap);
                                        char buf[MAX_PATH] = { 0 };
                                        WideCharToMultiByte(CP_ACP, 0, hdAdb.c_str(), -1, buf, sizeof(buf), NULL, NULL);
                                        return std::string(buf);
                                    }
                                }
                            }
                            CloseHandle(hProc);
                        }
                    }
                } while (Process32NextW(hSnap, &pe32));
            }
            CloseHandle(hSnap);
        }

        const std::vector<std::string> candidatePaths = {
            XS("C:\\Program Files\\BlueStacks_msi5\\HD-Adb.exe"),
            XS("C:\\Program Files\\BlueStacks_nxt\\HD-Adb.exe"),
            XS("C:\\Program Files (x86)\\BlueStacks_nxt\\HD-Adb.exe"),
            XS("C:\\Program Files\\BlueStacks\\HD-Adb.exe"),
            XS("C:\\LDPlayer\\LDPlayer9\\adb.exe"),
            XS("C:\\leidian\\LDPlayer9\\adb.exe"),
            XS("C:\\Program Files\\Nox\\bin\\nox_adb.exe"),
            XS("C:\\platform-tools\\adb.exe")
        };

        for (const auto& path : candidatePaths) {
            DWORD attrib = GetFileAttributesA(path.c_str());
            if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                return path;
            }
        }

        char localApp[MAX_PATH] = { 0 };
        if (GetEnvironmentVariableA(skCrypt("LOCALAPPDATA").decrypt(), localApp, MAX_PATH) > 0) {
            std::string sdkAdb = std::string(localApp) + XS("\\Android\\Sdk\\platform-tools\\adb.exe");
            DWORD attrib = GetFileAttributesA(sdkAdb.c_str());
            if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                return sdkAdb;
            }
        }

        return XS("adb.exe");
    }

    bool QueryDeviceResolution(const std::string& deviceAddr, int& outWidth, int& outHeight)
    {
        std::string adbPath = FindAdbExecutable();
        std::string targetDevice = deviceAddr.empty() ? "127.0.0.1:5555" : deviceAddr;
        std::string cmd = "\"" + adbPath + "\" -s " + targetDevice + " shell wm size";
        std::string output;
        if (ExecuteCommandHidden(cmd, &output, 4000)) {
            size_t overridePos = output.find("Override size:");
            if (overridePos != std::string::npos) {
                int w = 0, h = 0;
                if (sscanf_s(output.c_str() + overridePos, "Override size: %dx%d", &w, &h) == 2 && w > 0 && h > 0) {
                    outWidth = w;
                    outHeight = h;
                    return true;
                }
            }
            size_t physPos = output.find("Physical size:");
            if (physPos != std::string::npos) {
                int w = 0, h = 0;
                if (sscanf_s(output.c_str() + physPos, "Physical size: %dx%d", &w, &h) == 2 && w > 0 && h > 0) {
                    outWidth = w;
                    outHeight = h;
                    return true;
                }
            }
        }
        return false;
    }

    bool ExtractEmbeddedFiles(std::string& outInjPath, std::string& outLibPath, std::string& outError)
    {
        CleanupStagingFiles();
        g_lastSource = "none";

        // FORCE GitHub-only test — embed/disk commented so we know source is github
        const bool forceOnline = true;
        // const std::string onlineOnly = GetEnvA("AIMKILL_PAYLOAD_ONLINE_ONLY");
        // const bool forceOnline = onlineOnly.empty()
        //     || onlineOnly == "1"
        //     || _stricmp(onlineOnly.c_str(), "true") == 0;

        std::vector<uint8_t> inj, lib;
        bool loaded = false;

        // Always try remote payload first
        loaded = TryDownloadGitHub(inj, lib);
        if (!loaded) {
            outError = XS("E01");
            return false;
        }

        // EMBED / DISK FALLBACK — COMMENTED (online-only)
        // ...

        if (!loaded) {
            outError = XS("E02");
            return false;
        }
        (void)forceOnline;

        g_stagingDir = GetTempRoot() + "." + GenerateRandomSuffix(12);
        EnsureDirHardened(g_stagingDir);

        g_extractedInj = g_stagingDir + "\\" + GenerateRandomSuffix(10);
        g_extractedLib = g_stagingDir + "\\" + GenerateRandomSuffix(10);

        if (!WriteBytesToFile(g_extractedInj, inj) || !WriteBytesToFile(g_extractedLib, lib)) {
            outError = XS("E03");
            inj.clear();
            lib.clear();
            CleanupStagingFiles();
            return false;
        }

        inj.clear();
        lib.clear();

        HardenPathA(g_stagingDir);
        outInjPath = g_extractedInj;
        outLibPath = g_extractedLib;
        return true;
    }

    static bool CheckIfGameProcessRunning(const std::string& adbQuoted, const std::string& pkg)
    {
        std::string pidOut;
        if (ExecuteCommandHidden(adbQuoted + " shell \"pidof " + pkg + "\"", &pidOut, 3000)) {
            while (!pidOut.empty() && (pidOut.back() == '\r' || pidOut.back() == '\n' || pidOut.back() == ' '))
                pidOut.pop_back();
            while (!pidOut.empty() && pidOut.front() == ' ')
                pidOut.erase(pidOut.begin());

            if (!pidOut.empty()) {
                bool hasDigit = false;
                for (char c : pidOut) {
                    if (isdigit(static_cast<unsigned char>(c))) {
                        hasDigit = true;
                        break;
                    }
                }
                if (hasDigit) return true;
            }
        }

        if (!pkg.empty()) {
            std::string safePattern = "[" + pkg.substr(0, 1) + "]" + pkg.substr(1);
            std::string psOut;
            if (ExecuteCommandHidden(adbQuoted + " shell \"ps -A 2>/dev/null | grep -E '" + safePattern + "' || ps | grep -E '" + safePattern + "'\"", &psOut, 3000)) {
                if (psOut.find(pkg) != std::string::npos) {
                    return true;
                }
            }
        }

        return false;
    }

    bool InjectIntoEmulator(
        const std::string& deviceAddr,
        const std::string& packageName,
        StatusCallback onStatus
    )
    {
        (void)deviceAddr;
        auto report = [&](const std::string& msg, bool isErr = false) {
            if (onStatus) onStatus(msg, isErr);
        };

        std::string adbPath = FindAdbExecutable();
        std::string adbQuoted = "\"" + adbPath + "\"";
        report(XS("S1 ") + adbPath);

        report(XS("S2"));
        std::string injPath, libPath, extractErr;
        if (!ExtractEmbeddedFiles(injPath, libPath, extractErr)) {
            report(XS("E ") + extractErr, true);
            return false;
        }
        {
            const char* src = LastPayloadSource();
            if (!src || std::strcmp(src, "net") != 0) {
                report(XS("E04"), true);
                CleanupStagingFiles();
                return false;
            }
            report(XS("S2 OK"));
        }

        std::string randSuffix = GenerateRandomSuffix(6);
        std::string injRemoteName = XS("sys_") + randSuffix;
        std::string libRemoteName = XS("lib") + randSuffix + XS(".so");
        const std::string suPath = XS("/system/xbin/bstk/su");
        const std::string sdCache = XS("/sdcard/.cache");
        const std::string remoteDir = XS("/data/local");
        // Manual: Use user's selected packageName directly from dropdown
        std::string pkg = packageName.empty() ? externaltest::kDefaultGuestProcessFilter : packageName;
        std::string gameName = (pkg.find("max") != std::string::npos) ? XS("Free Fire MAX") : XS("Free Fire");

        report(XS("S3"));
        ExecuteCommandHidden(adbQuoted + XS(" forward --remove tcp:21405"));

        report(XS("S4 ") + gameName);
        bool isRunning = CheckIfGameProcessRunning(adbQuoted, pkg);

        if (!isRunning) {
            report(XS("S4a"));
            ExecuteCommandHidden(adbQuoted + XS(" shell \"monkey -p ") + pkg + XS(" 1\""));
            report(XS("S4b"));

            bool verified = false;
            for (int attempt = 0; attempt < 12; ++attempt) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (CheckIfGameProcessRunning(adbQuoted, pkg)) {
                    verified = true;
                    break;
                }
                report(XS("S4w ") + std::to_string(attempt + 1));
            }

            if (!verified) {
                report(XS("E05"), true);
                CleanupStagingFiles();
                return false;
            }

            report(XS("S4c"));
            for (int i = 5; i > 0; --i) {
                report(XS("S4i ") + std::to_string(i));
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } else {
            report(XS("S4ok"));
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        report(XS("S5"));
        ExecuteCommandHidden(adbQuoted + XS(" shell \"") + suPath + XS(" -c 'pkill -9 -f ") + remoteDir + XS(" 2>/dev/null; true'\""));

        report(XS("S6"));
        ExecuteCommandHidden(adbQuoted + XS(" shell \"") + suPath + XS(" -c 'mkdir -p ") + sdCache + XS("'\""));

        report(XS("S7a"));
        std::string pushInjCmd = adbQuoted + XS(" push \"") + injPath + XS("\" ") + sdCache + "/" + injRemoteName;
        if (!ExecuteCommandHidden(pushInjCmd)) {
            report(XS("E06"), true);
            CleanupStagingFiles();
            return false;
        }

        report(XS("S7b"));
        std::string pushLibCmd = adbQuoted + XS(" push \"") + libPath + XS("\" ") + sdCache + "/" + libRemoteName;
        if (!ExecuteCommandHidden(pushLibCmd)) {
            report(XS("E07"), true);
            CleanupStagingFiles();
            return false;
        }

        CleanupStagingFiles();
        injPath.clear();
        libPath.clear();

        report(XS("S8"));
        ExecuteCommandHidden(adbQuoted + XS(" shell \"") + suPath + XS(" -c 'mv ") + sdCache + "/" + injRemoteName + " " + remoteDir + "/" + injRemoteName + XS("'\""));
        ExecuteCommandHidden(adbQuoted + XS(" shell \"") + suPath + XS(" -c 'mv ") + sdCache + "/" + libRemoteName + " " + remoteDir + "/" + libRemoteName + XS("'\""));

        report(XS("S9"));
        ExecuteCommandHidden(adbQuoted + XS(" shell \"") + suPath + XS(" -c 'chmod 700 ") + remoteDir + "/" + injRemoteName + XS("'\""));
        ExecuteCommandHidden(adbQuoted + XS(" shell \"") + suPath + XS(" -c 'chmod 644 ") + remoteDir + "/" + libRemoteName + XS("'\""));

        report(XS("S10"));
        std::string injectCmd = remoteDir + "/" + injRemoteName + XS(" -pkg ") + pkg + XS(" -lib ") + remoteDir + "/" + libRemoteName + XS(" -no-entry -hide-maps");
        std::string injectOut;
        ExecuteCommandHidden(adbQuoted + XS(" shell \"") + suPath + XS(" -c '") + injectCmd + XS("'\""), &injectOut, 15000);
        report(XS("S10o ") + injectOut);

        report(XS("S11"));
        ExecuteCommandHidden(adbQuoted + XS(" shell \"") + suPath + XS(" -c 'rm -f ") + remoteDir + "/" + injRemoteName + " " + remoteDir + "/" + libRemoteName + XS("'\""));
        ExecuteCommandHidden(adbQuoted + XS(" shell \"rm -f ") + sdCache + "/" + injRemoteName + " " + sdCache + "/" + libRemoteName + XS("\""));

        report(XS("S12"));
        ExecuteCommandHidden(adbQuoted + XS(" forward tcp:21405 tcp:21405"));

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        report(XS("S13"));
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));

        CleanupStagingFiles();

        report(XS("S14 OK"));
        return true;
    }

    void ScheduleSafeCleanup(int delaySeconds)
    {
        std::thread([delaySeconds]() {
            std::this_thread::sleep_for(std::chrono::seconds(delaySeconds));
            CleanupStagingFiles();
        }).detach();
    }

    void CleanupStagingFiles()
    {
        WipeFileA(g_extractedInj);
        WipeFileA(g_extractedLib);
        g_extractedInj.clear();
        g_extractedLib.clear();

        // Legacy wipe paths (names encrypted)
        const std::string injN = RemoteInjName();
        const std::string libN = RemoteLibName();
        WipeFileA(GetTempRoot() + std::string(skCrypt("Libs\\").decrypt()) + injN);
        WipeFileA(GetTempRoot() + std::string(skCrypt("Libs\\").decrypt()) + libN);
        WipeFileA(std::string(skCrypt("C:\\Windows\\Temp\\").decrypt()) + injN);
        WipeFileA(std::string(skCrypt("C:\\Windows\\Temp\\").decrypt()) + libN);

        const std::string exeDir = GetPanelExeDir();
        if (!exeDir.empty()) {
            WipeFileA(exeDir + "\\" + injN);
            WipeFileA(exeDir + "\\" + libN);
        }

        if (!g_stagingDir.empty()) {
            RemoveDirectoryA(g_stagingDir.c_str());
            g_stagingDir.clear();
        }
        RemoveDirectoryA((GetTempRoot() + std::string(skCrypt("Libs").decrypt())).c_str());
    }

} // namespace AimkillInjector
