#pragma once

#include <stdint.h>
#include <functional>
#include <string>
#include <vector>

#include "web.h"

// Minimal HTTP/1.0 server for the PC simulator: one request per connection, single thread.
// Winsock on Windows, POSIX sockets elsewhere.

using HttpHandler = std::function<WebResponse(const WebRequest&)>;

struct HttpListener
{
    uint16_t port;
    HttpHandler handler;
};

// Binds every listener on host (e.g. "127.0.0.1", "0.0.0.0" in Docker) and serves forever.
// tick() runs between requests, at least every 20 ms. Returns false if a port can't be opened.
bool runHttpServers(const std::string& host, const std::vector<HttpListener>& listeners,
                    const std::function<void()>& tick);
