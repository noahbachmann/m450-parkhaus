#pragma once

// Socket API differences between Winsock and POSIX, for the simulator only.

#ifdef _WIN32
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 // Windows 7+: inet_pton, getaddrinfo
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#define closeSocket closesocket
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <cerrno>
using socket_t = int;
constexpr socket_t INVALID_SOCKET = -1;
#define closeSocket close
#endif

// WSAStartup on Windows (once), nothing elsewhere. False if sockets are unusable.
bool socketsInit();

void setSocketTimeouts(socket_t s, int timeoutMs);
