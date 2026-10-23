#include "http_server.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "socket_compat.h"

namespace
{
    constexpr size_t MAX_REQUEST_BYTES = 64 * 1024;

    const char* statusText(int status)
    {
        switch (status)
        {
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 303:
            return "See Other";
        case 401:
            return "Unauthorized";
        case 400:
            return "Bad Request";
        case 402:
            return "Payment Required";
        case 404:
            return "Not Found";
        case 409:
            return "Conflict";
        case 503:
            return "Service Unavailable";
        default:
            return "Status";
        }
    }

    void parseArgs(const std::string& text, std::map<std::string, std::string>& args)
    {
        size_t pos = 0;
        while (!text.empty())
        {
            size_t amp = text.find('&', pos);
            std::string pair = text.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
            if (!pair.empty())
            {
                size_t eq = pair.find('=');
                std::string key = urlDecode(pair.substr(0, eq));
                args[key] = eq == std::string::npos ? "" : urlDecode(pair.substr(eq + 1));
            }
            if (amp == std::string::npos)
            {
                break;
            }
            pos = amp + 1;
        }
    }

    std::string lower(std::string s)
    {
        for (char& c : s)
        {
            if (c >= 'A' && c <= 'Z')
            {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        return s;
    }

    // Reads one request; false if the connection is broken or the request is malformed.
    bool readRequest(socket_t client, HttpRequest& req)
    {
        std::string data;
        char buf[4096];
        size_t headerEnd = std::string::npos;
        while ((headerEnd = data.find("\r\n\r\n")) == std::string::npos)
        {
            int n = recv(client, buf, sizeof(buf), 0);
            if (n <= 0 || data.size() > MAX_REQUEST_BYTES)
            {
                return false;
            }
            data.append(buf, static_cast<size_t>(n));
        }

        std::string head = data.substr(0, headerEnd);
        size_t lineEnd = head.find("\r\n");
        std::string requestLine = head.substr(0, lineEnd);
        size_t sp1 = requestLine.find(' ');
        size_t sp2 = requestLine.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos)
        {
            return false;
        }
        req.method = requestLine.substr(0, sp1);
        std::string target = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);
        size_t q = target.find('?');
        req.path = target.substr(0, q);
        if (q != std::string::npos)
        {
            parseArgs(target.substr(q + 1), req.args);
        }

        size_t contentLength = 0;
        std::string contentType;
        size_t pos = lineEnd == std::string::npos ? head.size() : lineEnd + 2;
        while (pos < head.size())
        {
            size_t end = head.find("\r\n", pos);
            std::string line = head.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
            size_t colon = line.find(':');
            if (colon != std::string::npos)
            {
                std::string name = lower(line.substr(0, colon));
                std::string value = line.substr(line.find_first_not_of(' ', colon + 1) == std::string::npos
                                                    ? line.size()
                                                    : line.find_first_not_of(' ', colon + 1));
                if (name == "content-length")
                {
                    contentLength = std::strtoul(value.c_str(), nullptr, 10);
                }
                else if (name == "content-type")
                {
                    contentType = lower(value);
                }
                else if (name == "authorization")
                {
                    req.authorization = value;
                }
            }
            pos = end == std::string::npos ? head.size() : end + 2;
        }
        if (contentLength > MAX_REQUEST_BYTES)
        {
            return false;
        }

        req.body = data.substr(headerEnd + 4);
        while (req.body.size() < contentLength)
        {
            int n = recv(client, buf, sizeof(buf), 0);
            if (n <= 0)
            {
                return false;
            }
            req.body.append(buf, static_cast<size_t>(n));
        }
        req.body.resize(contentLength);
        if (contentType.find("application/x-www-form-urlencoded") != std::string::npos)
        {
            parseArgs(req.body, req.args);
        }
        return true;
    }

    void sendAll(socket_t client, const std::string& data)
    {
        size_t sent = 0;
        while (sent < data.size())
        {
            int n = send(client, data.data() + sent, static_cast<int>(data.size() - sent), 0);
            if (n <= 0)
            {
                return;
            }
            sent += static_cast<size_t>(n);
        }
    }

    void handleClient(socket_t client, const HttpHandler& handler)
    {
        setSocketTimeouts(client, 2000);
        HttpRequest req;
        HttpResponse res;
        if (!readRequest(client, req))
        {
            res = {400, "text/plain; charset=utf-8", "Bad Request", "", ""};
        }
        else
        {
            res = handler(req);
            std::printf("[http] %s %s -> %d\n", req.method.c_str(), req.path.c_str(), res.status);
            std::fflush(stdout);
        }
        std::string out = "HTTP/1.0 " + std::to_string(res.status) + " " + statusText(res.status) + "\r\n";
        out += "Content-Type: " + res.contentType + "\r\n";
        out += "Content-Length: " + std::to_string(res.body.size()) + "\r\n";
        if (!res.location.empty())
        {
            out += "Location: " + res.location + "\r\n";
        }
        if (!res.wwwAuthenticate.empty())
        {
            out += "WWW-Authenticate: " + res.wwwAuthenticate + "\r\n";
        }
        out += "Cache-Control: no-store\r\nConnection: close\r\n\r\n" + res.body;
        sendAll(client, out);
        closeSocket(client);
    }

    socket_t openListener(const std::string& host, uint16_t port)
    {
        socket_t s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == INVALID_SOCKET)
        {
            return INVALID_SOCKET;
        }
        int yes = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
        {
            closeSocket(s);
            return INVALID_SOCKET;
        }
        if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(s, 16) != 0)
        {
            closeSocket(s);
            return INVALID_SOCKET;
        }
        return s;
    }
}

bool HttpRequest::hasArg(const std::string& name) const
{
    return args.count(name) != 0;
}

std::string HttpRequest::arg(const std::string& name) const
{
    auto it = args.find(name);
    return it == args.end() ? "" : it->second;
}

std::string urlDecode(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i)
    {
        char c = text[i];
        if (c == '+')
        {
            out += ' ';
        }
        else if (c == '%' && i + 2 < text.size())
        {
            char hex[3] = {text[i + 1], text[i + 2], 0};
            char* end = nullptr;
            long v = std::strtol(hex, &end, 16);
            if (end == hex + 2)
            {
                out += static_cast<char>(v);
                i += 2;
            }
            else
            {
                out += c;
            }
        }
        else
        {
            out += c;
        }
    }
    return out;
}

bool socketsInit()
{
#ifdef _WIN32
    static bool done = false;
    if (!done)
    {
        SetConsoleOutputCP(CP_UTF8);
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            return false;
        }
        done = true;
    }
#endif
    return true;
}

void setSocketTimeouts(socket_t s, int timeoutMs)
{
#ifdef _WIN32
    DWORD timeout = static_cast<DWORD>(timeoutMs);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    timeval timeout{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

bool runHttpServers(const std::string& host, const std::vector<HttpListener>& listeners,
                    const std::function<void()>& tick)
{
    if (!socketsInit())
    {
        return false;
    }
    std::vector<socket_t> sockets;
    for (const HttpListener& l : listeners)
    {
        socket_t s = openListener(host, l.port);
        if (s == INVALID_SOCKET)
        {
            std::fprintf(stderr, "%s:%u kann nicht geöffnet werden (Port belegt oder Adresse ungültig?)\n",
                         host.c_str(), l.port);
            return false;
        }
        sockets.push_back(s);
    }

    for (;;)
    {
        fd_set readable;
        FD_ZERO(&readable);
        socket_t maxFd = 0;
        for (socket_t s : sockets)
        {
            FD_SET(s, &readable);
            maxFd = s > maxFd ? s : maxFd;
        }
        timeval timeout{0, 20000};
        int ready = select(static_cast<int>(maxFd + 1), &readable, nullptr, nullptr, &timeout);
        tick();
        if (ready <= 0)
        {
            continue;
        }
        for (size_t i = 0; i < sockets.size(); ++i)
        {
            if (FD_ISSET(sockets[i], &readable))
            {
                socket_t client = accept(sockets[i], nullptr, nullptr);
                if (client != INVALID_SOCKET)
                {
                    handleClient(client, listeners[i].handler);
                    tick();
                }
            }
        }
    }
}
