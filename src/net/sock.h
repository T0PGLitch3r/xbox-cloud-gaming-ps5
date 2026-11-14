// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 RafaelNGP
// The few socket calls that differ between BSD sockets (the consoles, Linux)
// and Winsock, behind one name each. Everything else (socket, connect, bind,
// listen, accept, getaddrinfo, htons...) is spelled the same on both and is
// called directly; on Windows the POSIX header names resolve to
// src/platform/windows/compat, which brings Winsock in.
//
// A socket is an `int` here as in the rest of the code; Windows' SOCKET
// handles fit (they are small kernel handle values), INVALID_SOCKET is -1.
#pragma once

#include <cerrno>
#include <cstddef>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/types.h>  // ssize_t
#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif
#else
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace xc::net {

#if defined(_WIN32)

struct PollFd {
    int fd;
    short events;
    short revents;
};

// The last socket error as the errno value the callers compare with
// (EAGAIN: a timed-out blocking call; EINPROGRESS: a non-blocking connect
// under way); any other Winsock error as it is (10000 and up).
inline int sockErrno() {
    int e = ::WSAGetLastError();
    switch (e) {
    case WSAEWOULDBLOCK: return EINPROGRESS;
    case WSAEINPROGRESS: return EINPROGRESS;
    case WSAETIMEDOUT: return EAGAIN;
    case WSAEINTR: return EINTR;
    default: return e;
    }
}
inline int closeSocket(int fd) { return ::closesocket(static_cast<SOCKET>(fd)); }
inline void setNonBlocking(int fd, bool on) {
    u_long v = on ? 1 : 0;
    ::ioctlsocket(static_cast<SOCKET>(fd), FIONBIO, &v);
}
// Winsock takes the two time limits in milliseconds, not as a timeval.
inline void setSocketTimeouts(int fd, int timeoutMs) {
    DWORD v = timeoutMs > 0 ? static_cast<DWORD>(timeoutMs) : 0;
    ::setsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&v), sizeof v);
    ::setsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&v), sizeof v);
}
inline int setSockOpt(int fd, int level, int name, const void* value, size_t len) {
    return ::setsockopt(static_cast<SOCKET>(fd), level, name, static_cast<const char*>(value), static_cast<int>(len));
}
// SO_ERROR: 0 when the socket is fine; false when it cannot be asked.
inline bool socketError(int fd, int& error) {
    int len = sizeof error;
    error = 0;
    return ::getsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) == 0;
}
inline ssize_t sockSend(int fd, const void* buf, size_t len, int flags) {
    return ::send(static_cast<SOCKET>(fd), static_cast<const char*>(buf), static_cast<int>(len), flags);
}
inline ssize_t sockRecv(int fd, void* buf, size_t len, int flags) {
    return ::recv(static_cast<SOCKET>(fd), static_cast<char*>(buf), static_cast<int>(len), flags);
}
inline ssize_t sockSendTo(int fd, const void* buf, size_t len, int flags, const sockaddr* to, size_t toLen) {
    return ::sendto(static_cast<SOCKET>(fd), static_cast<const char*>(buf), static_cast<int>(len), flags, to,
                    static_cast<int>(toLen));
}
inline int sockPoll(PollFd* fds, size_t count, int timeoutMs) {
    WSAPOLLFD w[8];
    if (count > 8) count = 8;
    for (size_t i = 0; i < count; ++i) {
        w[i].fd = static_cast<SOCKET>(fds[i].fd);
        w[i].events = fds[i].events;
        w[i].revents = 0;
    }
    int rc = ::WSAPoll(w, static_cast<ULONG>(count), timeoutMs);
    for (size_t i = 0; i < count; ++i) fds[i].revents = w[i].revents;
    return rc;
}

#else

using PollFd = ::pollfd;

inline int sockErrno() { return errno; }
inline int closeSocket(int fd) { return ::close(fd); }
inline void setNonBlocking(int fd, bool on) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return;
    ::fcntl(fd, F_SETFL, on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
}
inline void setSocketTimeouts(int fd, int timeoutMs) {
    timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
}
inline int setSockOpt(int fd, int level, int name, const void* value, size_t len) {
    return ::setsockopt(fd, level, name, value, static_cast<socklen_t>(len));
}
inline bool socketError(int fd, int& error) {
    socklen_t len = sizeof error;
    error = 0;
    return ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0;
}
inline ssize_t sockSend(int fd, const void* buf, size_t len, int flags) { return ::send(fd, buf, len, flags); }
inline ssize_t sockRecv(int fd, void* buf, size_t len, int flags) { return ::recv(fd, buf, len, flags); }
inline ssize_t sockSendTo(int fd, const void* buf, size_t len, int flags, const sockaddr* to, size_t toLen) {
    return ::sendto(fd, buf, len, flags, to, static_cast<socklen_t>(toLen));
}
inline int sockPoll(PollFd* fds, size_t count, int timeoutMs) {
    return ::poll(fds, static_cast<nfds_t>(count), timeoutMs);
}

#endif

}  // namespace xc::net
