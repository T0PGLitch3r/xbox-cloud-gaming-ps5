// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 RafaelNGP
// crash_guard.h on Windows: an unhandled structured exception (access
// violation, illegal instruction...) or an uncaught C++ exception writes the
// marker the next start reads, in the same three lines as crash_guard.cpp:
// the kind, the stage, "crash: <name> pc=<module>+0x.. addr=0x..".
#include "app/crash_guard.h"

#include "platform/platform.h"
#include "util/log.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <thread>

namespace xc::app::crash {

namespace {

char g_marker[1024] = {};
char g_stage[96] = "starting";
std::atomic<uint64_t> g_beat{0};
std::atomic<bool> g_written{false};

void appendStr(char* buf, size_t cap, size_t& len, const char* s) {
    if (!s) return;
    while (*s && len + 1 < cap) buf[len++] = *s++;
    buf[len] = 0;
}

void appendHex(char* buf, size_t cap, size_t& len, uintptr_t val) {
    appendStr(buf, cap, len, "0x");
    char tmp[20];
    int n = 0;
    do {
        unsigned int d = static_cast<unsigned int>(val & 0xf);
        tmp[n++] = static_cast<char>(d < 10 ? ('0' + d) : ('a' + d - 10));
        val >>= 4;
    } while (val && n < 16);
    while (n > 0 && len + 1 < cap) buf[len++] = tmp[--n];
    buf[len] = 0;
}

const char* baseName(const char* path) {
    const char* last = path;
    for (const char* p = path; *p; ++p)
        if (*p == '/' || *p == '\\') last = p + 1;
    return last;
}

// "psbox.exe+0x1a2b3c" for an address inside a loaded module, else the address.
void appendCodeAddr(char* buf, size_t cap, size_t& len, uintptr_t addr) {
    HMODULE mod = nullptr;
    if (addr != 0 && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                        reinterpret_cast<LPCSTR>(addr), &mod) &&
        mod) {
        char name[MAX_PATH];
        DWORD n = GetModuleFileNameA(mod, name, sizeof name);
        if (n > 0 && n < sizeof name) {
            appendStr(buf, cap, len, baseName(name));
            appendStr(buf, cap, len, "+");
            appendHex(buf, cap, len, addr - reinterpret_cast<uintptr_t>(mod));
            return;
        }
    }
    appendHex(buf, cap, len, addr);
}

const char* exceptionName(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
    case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
    case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
    case 0xC0000409: return "STACK_BUFFER_OVERRUN";  // also abort() / fast fail
    case 0xC0000374: return "HEAP_CORRUPTION";
    default: return "EXCEPTION";
    }
}

// No allocation: CreateFile / WriteFile only.
void writeMarker(const char* kind, const char* crashLine = nullptr) {
    if (!g_marker[0]) return;
    HANDLE h = CreateFileA(g_marker, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    auto put = [h](const char* s) {
        DWORD done = 0;
        WriteFile(h, s, static_cast<DWORD>(std::strlen(s)), &done, nullptr);
    };
    put(kind);
    put("\n");
    put(g_stage);
    put("\n");
    if (crashLine && crashLine[0]) {
        put(crashLine);
        put("\n");
    }
    CloseHandle(h);
}

LONG WINAPI onException(EXCEPTION_POINTERS* ep) {
    if (g_written.exchange(true)) return EXCEPTION_CONTINUE_SEARCH;
    DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    uintptr_t pc = ep && ep->ExceptionRecord ? reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress) : 0;
    uintptr_t fault = 0;
    if (ep && ep->ExceptionRecord &&
        (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) &&
        ep->ExceptionRecord->NumberParameters >= 2) {
        fault = static_cast<uintptr_t>(ep->ExceptionRecord->ExceptionInformation[1]);
    }

    char kind[64];
    size_t klen = 0;
    appendStr(kind, sizeof kind, klen, "exception ");
    appendHex(kind, sizeof kind, klen, code);
    appendStr(kind, sizeof kind, klen, " (");
    appendStr(kind, sizeof kind, klen, exceptionName(code));
    appendStr(kind, sizeof kind, klen, ")");

    char line[512];
    size_t len = 0;
    appendStr(line, sizeof line, len, "crash: ");
    appendStr(line, sizeof line, len, exceptionName(code));
    appendStr(line, sizeof line, len, " pc=");
    appendCodeAddr(line, sizeof line, len, pc);
    appendStr(line, sizeof line, len, " addr=");
    appendHex(line, sizeof line, len, fault);

    // The callers, from the stack the exception left (not after a stack
    // overflow: there is no room to walk it).
    if (code != EXCEPTION_STACK_OVERFLOW) {
        void* frames[8];
        USHORT n = CaptureStackBackTrace(0, 8, frames, nullptr);
        bool any = false;
        for (USHORT i = 0; i < n; ++i) {
            uintptr_t ret = reinterpret_cast<uintptr_t>(frames[i]);
            if (!ret) break;
            appendStr(line, sizeof line, len, any ? "," : " bt=");
            appendCodeAddr(line, sizeof line, len, ret - 1);
            any = true;
        }
    }
    writeMarker(kind, line);
    return EXCEPTION_CONTINUE_SEARCH;  // Windows ends the process as it would have
}

}  // namespace

void install(const std::string& markerPath) {
    std::strncpy(g_marker, markerPath.c_str(), sizeof g_marker - 1);
    g_marker[sizeof g_marker - 1] = 0;
    SetUnhandledExceptionFilter(onException);
    // No "the program stopped working" box hiding a crash from an unattended run.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    std::set_terminate([] {
        char line[256];
        size_t len = 0;
        appendStr(line, sizeof line, len, "crash: terminate pc=");
        appendCodeAddr(line, sizeof line, len, reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
        appendStr(line, sizeof line, len, " addr=0x0");
        if (auto ep = std::current_exception()) {
            try {
                std::rethrow_exception(ep);
            } catch (const std::exception& e) {
                appendStr(line, sizeof line, len, " what=");
                appendStr(line, sizeof line, len, e.what());
            } catch (...) {
                appendStr(line, sizeof line, len, " what=<non-std>");
            }
        }
        if (!g_written.exchange(true)) writeMarker("uncaught exception", line);
        TerminateProcess(GetCurrentProcess(), 3);
    });
}

void setStage(const char* stage) {
    // Written before a crash can read it half-copied: the end first.
    char tmp[sizeof g_stage];
    std::strncpy(tmp, stage, sizeof tmp - 1);
    tmp[sizeof tmp - 1] = 0;
    std::memcpy(g_stage, tmp, sizeof tmp);
}

void heartbeat() { g_beat = platform::nowMs(); }

void startWatchdog(uint32_t stallMs) {
    heartbeat();
    std::thread([stallMs] {
        bool stalled = false;
        uint64_t last = platform::nowMs();
        for (;;) {
            platform::sleepMs(1000);
            uint64_t now = platform::nowMs();
            // This second took far longer: the PC slept, not the main loop stuck.
            if (now - last > 5000) {
                XC_LOGI("crash guard: the app was suspended for %llu ms",
                        static_cast<unsigned long long>(now - last));
                heartbeat();
                last = now;
                continue;
            }
            last = now;
            uint64_t beat = g_beat.load();
            uint64_t idle = now > beat ? now - beat : 0;
            if (!stalled && idle > stallMs) {
                stalled = true;
                XC_LOGW("crash guard: the main loop has not run for %llu ms (%s)",
                        static_cast<unsigned long long>(idle), g_stage);
                writeMarker("stalled");
            } else if (stalled && idle < 1000) {
                stalled = false;
                XC_LOGI("crash guard: the main loop is back");
                DeleteFileA(g_marker);
            }
        }
    }).detach();
}

Previous takePrevious(const std::string& markerPath) {
    Previous p;
    std::string text;
    if (!platform::readFile(markerPath, text)) return p;
    DeleteFileA(markerPath.c_str());
    auto trimCr = [](std::string& s) {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    };
    size_t nl1 = text.find('\n');
    p.found = true;
    p.kind = text.substr(0, nl1);
    trimCr(p.kind);
    if (nl1 != std::string::npos) {
        size_t nl2 = text.find('\n', nl1 + 1);
        p.stage = text.substr(nl1 + 1, nl2 == std::string::npos ? std::string::npos : nl2 - (nl1 + 1));
        trimCr(p.stage);
        if (nl2 != std::string::npos) {
            p.crash = text.substr(nl2 + 1);
            trimCr(p.crash);
        }
    }
    return p;
}

}  // namespace xc::app::crash
