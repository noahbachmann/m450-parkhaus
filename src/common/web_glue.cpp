#include "web_glue.h"

#include <Arduino.h>

WebRequest readRequest(WebServer& server)
{
    WebRequest req;
    req.method = server.method() == HTTP_GET ? "GET" : server.method() == HTTP_POST ? "POST" : "OTHER";
    req.path = server.uri().c_str();
    for (int i = 0; i < server.args(); ++i)
    {
        req.args[server.argName(i).c_str()] = server.arg(i).c_str();
    }
    req.body = server.arg("plain").c_str();
    req.cookie = server.header("Cookie").c_str();
    return req;
}

void sendResponse(WebServer& server, const WebRequest& req, const WebResponse& res, unsigned long startMs)
{
    if (!res.location.empty())
    {
        server.sendHeader("Location", res.location.c_str());
    }
    if (!res.setCookie.empty())
    {
        server.sendHeader("Set-Cookie", res.setCookie.c_str());
    }
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(res.body.size());
    server.send(res.status, res.contentType.c_str(), "");
    if (!res.body.empty())
    {
        server.sendContent(res.body.c_str(), res.body.size());
    }
    Serial.printf("[http] %s %s", req.method.c_str(), req.path.c_str());
    if (req.hasArg("kz"))
    {
        Serial.printf(" kz=%s", req.arg("kz").c_str());
    }
    Serial.printf(" -> %d (%lu ms)\n", res.status, millis() - startMs);
}
