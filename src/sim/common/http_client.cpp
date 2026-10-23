#include "http_client.h"

#include <chrono>
#include <cstdlib>

#include "socket_compat.h"

namespace
{
    bool splitUrl(const std::string& url, std::string& host, std::string& port)
    {
        const std::string scheme = "http://";
        if (url.compare(0, scheme.size(), scheme) != 0)
        {
            return false;
        }
        std::string rest = url.substr(scheme.size());
        rest = rest.substr(0, rest.find('/'));
        size_t colon = rest.find(':');
        host = rest.substr(0, colon);
        port = colon == std::string::npos ? "80" : rest.substr(colon + 1);
        return !host.empty() && !port.empty();
    }

    void setNonBlocking(socket_t s, bool on)
    {
#ifdef _WIN32
        u_long mode = on ? 1 : 0;
        ioctlsocket(s, FIONBIO, &mode);
#else
        int flags = fcntl(s, F_GETFL, 0);
        fcntl(s, F_SETFL, on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
#endif
    }

    bool connectWithTimeout(socket_t s, const sockaddr* addr, int addrLen, int timeoutMs)
    {
        setNonBlocking(s, true);
        if (connect(s, addr, addrLen) != 0)
        {
#ifdef _WIN32
            if (WSAGetLastError() != WSAEWOULDBLOCK)
            {
                return false;
            }
#else
            if (errno != EINPROGRESS)
            {
                return false;
            }
#endif
            fd_set writable;
            fd_set failed;
            FD_ZERO(&writable);
            FD_ZERO(&failed);
            FD_SET(s, &writable);
            FD_SET(s, &failed);
            timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
            if (select(static_cast<int>(s + 1), nullptr, &writable, &failed, &tv) <= 0 || FD_ISSET(s, &failed))
            {
                return false;
            }
            int error = 0;
            socklen_t len = sizeof(error);
            getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len);
            if (error != 0)
            {
                return false;
            }
        }
        setNonBlocking(s, false);
        return true;
    }
}

namespace
{
// body == nullptr: GET, otherwise POST with JSON.
HttpReply httpRequest(const std::string& baseUrl, const std::string& path, const std::string* body, int timeoutMs)
{
    using Clock = std::chrono::steady_clock;
    Clock::time_point deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    auto remainingMs = [&]() {
        return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count());
    };

    std::string host;
    std::string port;
    if (!socketsInit() || !splitUrl(baseUrl, host, port))
    {
        return {};
    }
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* found = nullptr;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &found) != 0 || found == nullptr)
    {
        return {};
    }
    socket_t s = socket(found->ai_family, found->ai_socktype, found->ai_protocol);
    bool connected = s != INVALID_SOCKET && remainingMs() > 0 &&
                     connectWithTimeout(s, found->ai_addr, static_cast<int>(found->ai_addrlen), remainingMs());
    freeaddrinfo(found);
    if (!connected)
    {
        if (s != INVALID_SOCKET)
        {
            closeSocket(s);
        }
        return {};
    }

    std::string request = (body != nullptr ? "POST " : "GET ") + path + " HTTP/1.0\r\nHost: " + host +
                          "\r\nConnection: close\r\n";
    if (body != nullptr)
    {
        request += "Content-Type: application/json\r\nContent-Length: " + std::to_string(body->size()) + "\r\n\r\n" +
                   *body;
    }
    else
    {
        request += "\r\n";
    }
    std::string response;
    bool ok = true;
    for (size_t sent = 0; ok && sent < request.size();)
    {
        int left = remainingMs();
        setSocketTimeouts(s, left > 0 ? left : 1);
        int n = left > 0 ? send(s, request.data() + sent, static_cast<int>(request.size() - sent), 0) : -1;
        ok = n > 0;
        sent += ok ? static_cast<size_t>(n) : 0;
    }
    char buf[4096];
    while (ok)
    {
        int left = remainingMs();
        if (left <= 0)
        {
            ok = false;
            break;
        }
        setSocketTimeouts(s, left);
        int n = recv(s, buf, sizeof(buf), 0);
        if (n == 0)
        {
            break; // server closed: response complete
        }
        ok = n > 0;
        if (ok)
        {
            response.append(buf, static_cast<size_t>(n));
        }
    }
    closeSocket(s);

    size_t headerEnd = response.find("\r\n\r\n");
    if (!ok || response.compare(0, 5, "HTTP/") != 0 || headerEnd == std::string::npos)
    {
        return {};
    }
    size_t sp = response.find(' ');
    int status = sp == std::string::npos ? 0 : std::atoi(response.c_str() + sp + 1);
    if (status <= 0)
    {
        return {};
    }
    return {status, response.substr(headerEnd + 4)};
}
}

HttpReply httpPostJson(const std::string& baseUrl, const std::string& path, const std::string& body, int timeoutMs)
{
    return httpRequest(baseUrl, path, &body, timeoutMs);
}

HttpReply httpGetJson(const std::string& baseUrl, const std::string& path, int timeoutMs)
{
    return httpRequest(baseUrl, path, nullptr, timeoutMs);
}
