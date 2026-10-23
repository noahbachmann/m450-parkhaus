#pragma once

#include <stdint.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

// Minimal HTTP/1.0 server for the PC simulator: one request per connection, single thread.
// Winsock on Windows, POSIX sockets elsewhere.

struct HttpRequest
{
    std::string method; // "GET", "POST", ...
    std::string path;   // without the query string
    std::map<std::string, std::string> args; // query string + urlencoded form fields, decoded
    std::string body;   // raw body (JSON API)
    std::string authorization; // Authorization header, e.g. "Basic YmV0cmllYjpwYXJraGF1cw=="

    bool hasArg(const std::string& name) const;
    std::string arg(const std::string& name) const;
};

struct HttpResponse
{
    int status = 200;
    std::string contentType = "text/html; charset=utf-8";
    std::string body;
    std::string location; // Location header, for redirects
    std::string wwwAuthenticate; // WWW-Authenticate header, for 401
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

struct HttpListener
{
    uint16_t port;
    HttpHandler handler;
};

// Binds every listener on host (e.g. "127.0.0.1", "0.0.0.0" in Docker) and serves forever.
// tick() runs between requests, at least every 20 ms. Returns false if a port can't be opened.
bool runHttpServers(const std::string& host, const std::vector<HttpListener>& listeners,
                    const std::function<void()>& tick);

// "a+b%C3%A4" -> "a bä"
std::string urlDecode(const std::string& text);
