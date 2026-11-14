// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 RafaelNGP
// The Windows platform layer (psbox.exe): Winsock, %LOCALAPPDATA%\psbox for
// the user's data, QueryPerformanceCounter clocks, Win32 threads.
//
// Paths are UTF-8 strings handed to the "A" functions and to fopen(): the
// executable's manifest (windows/psbox.manifest) makes UTF-8 the process
// code page on Windows 10 1903 and newer, so a user folder with any name
// works. On an older Windows the same calls use the system code page,
// consistently, which covers the names that code page can spell.
#include "platform/platform.h"
#include "platform/toasts.h"
#include "util/log.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <mmsystem.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdint>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace xc::platform {

namespace {

std::string g_dataDir;
std::string g_dataDirNote;
bool g_dataDirWritable = true;
int g_dataDirErrno = 0;
bool g_timerPeriod = false;

// An environment variable, whether the C runtime set it (the user's shell,
// _putenv) or SetEnvironmentVariable did (SDL_setenv: psbox --data-dir).
std::string envVar(const char* name) {
    char buf[2048];
    DWORD n = GetEnvironmentVariableA(name, buf, sizeof buf);
    if (n > 0 && n < sizeof buf) return std::string(buf, n);
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

bool isDir(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

// mkdir -p ("C:" itself is skipped: not a folder to make).
void makeDirs(const std::string& path) {
    for (size_t i = 1; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/' || path[i] == '\\') {
            if (path[i - 1] == ':' || path[i - 1] == '/' || path[i - 1] == '\\') continue;
            _mkdir(path.substr(0, i).c_str());
        }
    }
}

const std::string& exeDir() {
    static const std::string dir = [] {
        char buf[2048];
        DWORD len = GetModuleFileNameA(nullptr, buf, sizeof buf);
        if (len == 0 || len >= sizeof buf) return std::string();
        std::string path(buf, len);
        size_t slash = path.find_last_of("/\\");
        if (slash == std::string::npos) return std::string();
        return path.substr(0, slash);
    }();
    return dir;
}

uint64_t qpc(uint64_t perSecond) {
    static const LONGLONG freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f.QuadPart ? f.QuadPart : 1;
    }();
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    // Whole seconds and the rest apart: no overflow after long uptimes.
    uint64_t whole = static_cast<uint64_t>(count.QuadPart / freq);
    uint64_t part = static_cast<uint64_t>(count.QuadPart % freq);
    return whole * perSecond + part * perSecond / static_cast<uint64_t>(freq);
}

}  // namespace

bool init() {
    // fopen() without "b" must not turn "\n" into "\r\n" (or stop at a Ctrl+Z): every
    // file here is written and read back byte for byte, as on the other platforms.
    _set_fmode(_O_BINARY);
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
    // Sleep() and the waits in 1 ms steps, not the default 15.6 ms: the
    // main loop and the stream's pacing count on it.
    g_timerPeriod = timeBeginPeriod(1) == TIMERR_NOERROR;

    std::string env = envVar("XCLOUD_DATA_DIR");
    if (!env.empty()) {
        g_dataDir = env;
    } else {
        char base[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, nullptr, 0, base))) {
            g_dataDir = std::string(base) + "\\psbox";
        } else {
            g_dataDir = (exeDir().empty() ? std::string(".") : exeDir()) + "\\psbox-data";
            g_dataDirNote = "the local application data folder is unknown: kept beside the app";
        }
    }
    while (g_dataDir.size() > 3 && (g_dataDir.back() == '/' || g_dataDir.back() == '\\')) g_dataDir.pop_back();
    makeDirs(g_dataDir);

    // Can it be written to? (a read-only folder given with --data-dir...)
    const std::string probe = g_dataDir + "\\.write-test";
    if (FILE* f = std::fopen(probe.c_str(), "wb")) {
        std::fclose(f);
        std::remove(probe.c_str());
    } else {
        g_dataDirWritable = false;
        g_dataDirErrno = errno;
    }
    return true;
}

void shutdown() {
    if (g_timerPeriod) timeEndPeriod(1);
    g_timerPeriod = false;
    WSACleanup();
}

std::string dataDir() { return g_dataDir; }
std::string dataDirNote() { return g_dataDirNote; }
bool dataDirWritable() { return g_dataDirWritable; }
int dataDirErrno() { return g_dataDirErrno; }

std::string appDir() { return exeDir().empty() ? std::string(".") : exeDir(); }

std::string assetDir() {
    if (std::string env = envVar("XCLOUD_ASSETS"); !env.empty()) return env;
    if (!exeDir().empty() && isDir(exeDir() + "\\assets")) return exeDir() + "\\assets";
    return "assets";
}

std::string caBundlePath() {
    if (std::string env = envVar("XCLOUD_CA_BUNDLE"); !env.empty()) return env;
    return assetDir() + "\\cacert.pem";
}

bool randomBytes(void* out, size_t len) {
    auto* p = static_cast<PUCHAR>(out);
    while (len) {
        ULONG chunk = static_cast<ULONG>(std::min<size_t>(len, 1u << 20));
        if (BCryptGenRandom(nullptr, p, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return false;
        p += chunk;
        len -= chunk;
    }
    return true;
}

bool resolveIPv4(const std::string& host, std::vector<uint32_t>& addrs, std::string& err) {
    in_addr literal{};
    if (inet_pton(AF_INET, host.c_str(), &literal) == 1) {
        addrs.push_back(literal.s_addr);
        return true;
    }
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    int rc = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (rc != 0 || !res) {
        err = "DNS lookup failed for " + host + " (error " + std::to_string(rc) + ")";
        return false;
    }
    for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET && p->ai_addr) {
            uint32_t a = reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_addr.s_addr;
            if (std::find(addrs.begin(), addrs.end(), a) == addrs.end()) addrs.push_back(a);
        }
    }
    freeaddrinfo(res);
    if (addrs.empty()) {
        err = "DNS lookup failed for " + host + ": no IPv4 address found";
        return false;
    }
    return true;
}

bool resolveIPv4(const std::string& host, uint32_t& addr, std::string& err) {
    std::vector<uint32_t> addrs;
    if (!resolveIPv4(host, addrs, err)) return false;
    addr = addrs[0];
    return true;
}

// The address the route to the Internet leaves from: a UDP socket
// "connected" there sends nothing.
std::string localIPv4() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) return "127.0.0.1";
    std::string result = "127.0.0.1";
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &dst.sin_addr);
    if (connect(s, reinterpret_cast<sockaddr*>(&dst), sizeof(dst)) == 0) {
        sockaddr_in local{};
        int len = sizeof(local);
        char ip[INET_ADDRSTRLEN];
        if (getsockname(s, reinterpret_cast<sockaddr*>(&local), &len) == 0 &&
            inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip))) {
            result = ip;
        }
    }
    closesocket(s);
    return result;
}

uint64_t nowMs() { return qpc(1000); }
uint64_t nowUs() { return qpc(1000000); }
void sleepMs(uint32_t ms) { Sleep(ms); }

void probeNetworking() {}

namespace {
struct ThreadArgs {
    std::function<void()> fn;
};

DWORD WINAPI winThreadEntry(LPVOID param) {
    auto* args = static_cast<ThreadArgs*>(param);
    if (args->fn) args->fn();
    delete args;
    return 0;
}

std::atomic<bool> g_noThreadPinning{true};
int g_handheldForced = -1;  // forceHandheld(): -1 not forced
}  // namespace

bool startThread(Thread& t, std::function<void()> fn, size_t stackBytes) {
    auto* args = new ThreadArgs{std::move(fn)};
    // The size is a reservation of address space, committed as it is used.
    HANDLE h = CreateThread(nullptr, stackBytes, winThreadEntry, args, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (!h) {
        delete args;
        return false;
    }
    t.handle = h;
    return true;
}

void Thread::join() {
    if (!handle) return;
    HANDLE h = static_cast<HANDLE>(handle);
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
    handle = nullptr;
}

// Threads keep the scheduler's choice unless setNoThreadPinning(false), as on
// Linux and the PS5: pinned, FFmpeg's decode threads all share one core.
void setNoThreadPinning(bool on) { g_noThreadPinning = on; }

void setThreadAffinity(ThreadRole role, const char* name) {
    if (name && *name) {
        // SetThreadDescription (Windows 10 1607+), looked up so an older
        // kernel32 still loads the app: the name shows in debuggers.
        using SetDesc = HRESULT(WINAPI*)(HANDLE, PCWSTR);
        static const SetDesc setDesc = reinterpret_cast<SetDesc>(
            reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetThreadDescription")));
        if (setDesc) {
            wchar_t wide[64];
            int n = MultiByteToWideChar(CP_UTF8, 0, name, -1, wide, 64);
            if (n > 0) setDesc(GetCurrentThread(), wide);
        }
    }
    if (g_noThreadPinning) return;
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD numCores = std::min<DWORD>(sysInfo.dwNumberOfProcessors, 64);
    if (numCores < 4) return;  // Do not restrict on systems with few cores

    DWORD_PTR mask = 0;
    switch (role) {
    case ThreadRole::Main:
        mask = DWORD_PTR{1} << 0;
        break;
    case ThreadRole::Network:
        mask = DWORD_PTR{1} << 1;
        break;
    case ThreadRole::VideoDecode:
        mask = DWORD_PTR{1} << 2;
        break;
    case ThreadRole::Audio:
        mask = DWORD_PTR{1} << 3;
        break;
    case ThreadRole::Worker:
        for (DWORD c = 4; c < numCores; ++c) mask |= DWORD_PTR{1} << c;
        if (numCores <= 4) mask = DWORD_PTR{1} << (numCores - 1);
        break;
    }
    if (mask) SetThreadAffinityMask(GetCurrentThread(), mask);
}

void setThreadRealtimePriority() { SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST); }

// A handheld PC (ROG Ally, Legion Go, a Steam Deck running Windows...):
// PSBOX_HANDHELD=1 or 0 decides; none is detected by itself yet.
bool isHandheld() {
    if (g_handheldForced >= 0) return g_handheldForced == 1;
    if (std::string env = envVar("PSBOX_HANDHELD"); !env.empty()) return env != "0";
    return false;
}
void forceHandheld(bool on) { g_handheldForced = on ? 1 : 0; }

void notify(const std::string& text) { notify(text, text); }

void notify(const std::string& text, const std::string& forLog) {
    if (toastsEnabled()) {
        XC_LOGI("NOTIFY: %s", forLog.c_str());
        toasts().push(text, nowMs());
        return;
    }
    std::printf("\n>>> %s\n\n", text.c_str());
}

bool systemKeyboardAvailable() { return false; }
bool openSystemKeyboard(const std::string&, const std::string&, size_t, KeyboardKind) { return false; }
KeyboardStatus pollSystemKeyboard(std::string&) { return KeyboardStatus::Closed; }

bool restartApp() { return false; }
std::string firmwareVersion() { return {}; }
std::string diagnostics() { return {}; }
std::string platformName() { return "Windows"; }
bool isPS4() { return false; }
bool isPS5() { return false; }
size_t imageCacheBudget() { return 160u << 20; }

bool readFile(const std::string& path, std::string& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

// Written beside the file, flushed to the disk, then moved over it in one
// step (rename() does not replace a file on Windows; MoveFileEx does).
bool writeFileAtomic(const std::string& path, const std::string& data) {
    std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = (std::fflush(f) == 0) && ok;
    if (ok) _commit(_fileno(f));
    std::fclose(f);
    if (!ok) {
        std::remove(tmp.c_str());
        return false;
    }
    // A virus scanner or the indexer may hold the old file for a moment.
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
        Sleep(20);
    }
    std::remove(tmp.c_str());
    return false;
}

// %LOCALAPPDATA% is already the user's alone (its ACL comes from the profile).
void makePrivate(const std::string&) {}

}  // namespace xc::platform
