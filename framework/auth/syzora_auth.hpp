#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>
#include <windows.h>
#include <wininet.h>
#include <fstream>

#pragma comment(lib, "wininet.lib")

namespace SyzoraAuth {

    struct ResponseData {
        bool success = false;
        std::string message = "Ready";
    };

    struct UserData {
        std::string username = "";
        std::string expiry = "";
        std::string subscription = "";
    };

    struct AppData {
        std::string motd = "";
        std::string version = "1.0";
    };

    class api {
    public:
        std::string name    = "MvpAimExe";
        std::string ownerid = "dcc307c596";
        std::string secret  = "fed3efa5ae769e8a8fe3f4dae72238346116dd3cef8123c22f616ddda183867b";
        std::string version = "1.0";
        std::string url     = "http://syzoraauth.mvpcheats.online/api/1.3/";
        std::string server_host = "syzoraauth.mvpcheats.online";
        std::string server_path = "/api/1.3/";

        ResponseData response;
        UserData     user_data;
        AppData      app_data;

        std::atomic<bool> is_logged_in{ false };
        std::atomic<bool> is_busy{ false };
        std::atomic<bool> is_initialized{ false };

        std::string saved_license_key = "";
        std::string saved_username = "";
        bool remember_credentials = true;

        api() {
            load_saved_credentials();
        }

        std::string get_hwid() {
            HW_PROFILE_INFO hw;
            if (GetCurrentHwProfileA(&hw)) {
                return std::string(hw.szHwProfileGuid);
            }
            return "UNKNOWN_HWID";
        }

        static std::string extract_json_field(const std::string& json, const std::string& field) {
            std::string key = "\"" + field + "\"";
            size_t pos = json.find(key);
            if (pos == std::string::npos) return "";

            pos += key.length();
            while (pos < json.length() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t')) {
                pos++;
            }

            if (pos >= json.length()) return "";

            if (json[pos] == '\"') {
                pos++;
                size_t end_pos = json.find('\"', pos);
                if (end_pos != std::string::npos) {
                    return json.substr(pos, end_pos - pos);
                }
            } else {
                size_t end_pos = json.find_first_of(",}\n\r ", pos);
                if (end_pos != std::string::npos) {
                    return json.substr(pos, end_pos - pos);
                }
            }
            return "";
        }

        static bool extract_json_bool(const std::string& json, const std::string& field) {
            std::string key = "\"" + field + "\"";
            size_t pos = json.find(key);
            if (pos == std::string::npos) return false;

            pos += key.length();
            while (pos < json.length() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t')) {
                pos++;
            }
            return json.substr(pos, 4) == "true";
        }

        std::string req(const std::string& post_data) {
            std::string result = "";

            HINTERNET hInternet = InternetOpenA("SyzoraAuth/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
            if (!hInternet) return "";

            DWORD timeoutMs = 8000;
            InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
            InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
            InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

            // Try direct WinINet HTTP POST first
            HINTERNET hConnect = InternetConnectA(hInternet, server_host.c_str(), INTERNET_DEFAULT_HTTP_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
            if (hConnect) {
                const char* rgpAcceptedTypes[] = { "*/*", NULL };
                HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", server_path.c_str(), NULL, NULL, rgpAcceptedTypes, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
                if (hRequest) {
                    std::string headers = "Content-Type: application/x-www-form-urlencoded\r\n";
                    BOOL sent = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(), (LPVOID)post_data.c_str(), (DWORD)post_data.length());
                    if (sent) {
                        char buffer[4096];
                        DWORD bytesRead = 0;
                        while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                            buffer[bytesRead] = '\0';
                            result += buffer;
                        }
                    }
                    InternetCloseHandle(hRequest);
                }
                InternetCloseHandle(hConnect);
            }

            // Fallback: If POST returned empty or failed, try via InternetOpenUrl with query params
            if (result.empty()) {
                std::string full_url = url;
                if (full_url.back() != '?' && full_url.find('?') == std::string::npos)
                    full_url += "?" + post_data;
                else
                    full_url += "&" + post_data;

                HINTERNET hUrl = InternetOpenUrlA(hInternet, full_url.c_str(), "Content-Type: application/x-www-form-urlencoded\r\n", -1, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
                if (hUrl) {
                    char buffer[4096];
                    DWORD bytesRead = 0;
                    while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                        buffer[bytesRead] = '\0';
                        result += buffer;
                    }
                    InternetCloseHandle(hUrl);
                }
            }

            InternetCloseHandle(hInternet);
            return result;
        }

        bool init() {
            std::string p = "type=init&ver=" + version + "&name=" + name + "&ownerid=" + ownerid + "&secret=" + secret;
            std::string res = req(p);

            if (res.empty()) {
                response.success = false;
                response.message = "Failed to connect to authentication server!";
                return false;
            }

            response.success = extract_json_bool(res, "success");
            response.message = extract_json_field(res, "message");
            app_data.motd    = extract_json_field(res, "motd");

            if (response.success) {
                is_initialized = true;
                if (response.message.empty()) response.message = "Initialized";
                return true;
            }

            if (response.message.empty()) response.message = "Initialization failed!";
            return false;
        }

        bool license(const std::string& key) {
            if (key.empty()) {
                response.success = false;
                response.message = "Please enter a license key!";
                return false;
            }

            std::string p = "type=license&key=" + key + "&hwid=" + get_hwid() + "&ownerid=" + ownerid + "&name=" + name + "&secret=" + secret + "&ver=" + version;
            std::string res = req(p);

            if (res.empty()) {
                response.success = false;
                response.message = "Connection error. Please try again.";
                return false;
            }

            response.success = extract_json_bool(res, "success");
            response.message = extract_json_field(res, "message");

            if (response.success) {
                user_data.username = extract_json_field(res, "username");
                if (user_data.username.empty()) user_data.username = "LicenseUser";
                user_data.expiry   = extract_json_field(res, "expiry");
                user_data.subscription = extract_json_field(res, "subscription");
                is_logged_in = true;
                if (remember_credentials) {
                    save_credentials(key, user_data.username);
                }
                return true;
            }

            if (response.message.empty()) response.message = "Invalid or expired license key!";
            return false;
        }

        bool login(const std::string& u, const std::string& pass) {
            if (u.empty() || pass.empty()) {
                response.success = false;
                response.message = "Please enter both username and password!";
                return false;
            }

            std::string p = "type=login&username=" + u + "&pass=" + pass + "&hwid=" + get_hwid() + "&ownerid=" + ownerid + "&name=" + name + "&secret=" + secret + "&ver=" + version;
            std::string res = req(p);

            if (res.empty()) {
                response.success = false;
                response.message = "Connection error. Please try again.";
                return false;
            }

            response.success = extract_json_bool(res, "success");
            response.message = extract_json_field(res, "message");

            if (response.success) {
                user_data.username = u;
                user_data.expiry   = extract_json_field(res, "expiry");
                user_data.subscription = extract_json_field(res, "subscription");
                is_logged_in = true;
                if (remember_credentials) {
                    save_credentials("", u);
                }
                return true;
            }

            if (response.message.empty()) response.message = "Invalid username or password!";
            return false;
        }

        bool regstr(const std::string& u, const std::string& pass, const std::string& k) {
            if (u.empty() || pass.empty() || k.empty()) {
                response.success = false;
                response.message = "All fields are required to register!";
                return false;
            }

            std::string p = "type=register&username=" + u + "&pass=" + pass + "&key=" + k + "&hwid=" + get_hwid() + "&ownerid=" + ownerid + "&name=" + name + "&secret=" + secret + "&ver=" + version;
            std::string res = req(p);

            if (res.empty()) {
                response.success = false;
                response.message = "Connection error. Please try again.";
                return false;
            }

            response.success = extract_json_bool(res, "success");
            response.message = extract_json_field(res, "message");

            if (response.success) {
                user_data.username = u;
                user_data.expiry   = extract_json_field(res, "expiry");
                user_data.subscription = extract_json_field(res, "subscription");
                is_logged_in = true;
                if (remember_credentials) {
                    save_credentials(k, u);
                }
                return true;
            }

            if (response.message.empty()) response.message = "Registration failed!";
            return false;
        }

        // Credentials file save & load
        void save_credentials(const std::string& key, const std::string& user) {
            std::ofstream f("auth_config.ini");
            if (f.is_open()) {
                if (!key.empty()) f << "license=" << key << "\n";
                if (!user.empty()) f << "username=" << user << "\n";
                f << "remember=" << (remember_credentials ? "1" : "0") << "\n";
                f.close();
            }
        }

        void load_saved_credentials() {
            std::ifstream f("auth_config.ini");
            if (f.is_open()) {
                std::string line;
                while (std::getline(f, line)) {
                    size_t eq = line.find('=');
                    if (eq != std::string::npos) {
                        std::string k = line.substr(0, eq);
                        std::string v = line.substr(eq + 1);
                        if (k == "license") saved_license_key = v;
                        else if (k == "username") saved_username = v;
                        else if (k == "remember") remember_credentials = (v == "1");
                    }
                }
                f.close();
            }
        }
    };

    inline api g_Auth;
}
