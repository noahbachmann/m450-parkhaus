#pragma once

#include <map>
#include <string>

// Platform-neutral HTTP request and response. The board glue fills them from WebServer, the PC simulator from
// its own socket server; the routes in lib/ only see these.

struct WebRequest
{
    std::string method; // "GET", "POST", ...
    std::string path;   // without the query string
    std::map<std::string, std::string> args; // query string + urlencoded form fields, decoded
    std::string body;   // raw body (JSON API)
    std::string cookie; // Cookie header, e.g. "parkhaus_session=ab12"

    bool hasArg(const std::string& name) const;
    std::string arg(const std::string& name) const;
};

struct WebResponse
{
    int status = 200;
    std::string contentType = "text/html; charset=utf-8";
    std::string body;
    std::string location;  // Location header, for redirects
    std::string setCookie; // Set-Cookie header value
};

// 303 See Other.
WebResponse redirect(const std::string& location);
// 200 text/html.
WebResponse htmlPage(const std::string& html);
WebResponse jsonReply(int status, const std::string& body);
// 404 «Nicht gefunden».
WebResponse notFound();

// "a+b%C3%A4" -> "a bä"
std::string urlDecode(const std::string& text);
// "kz=LU+1&m=2" -> args["kz"] = "LU 1", args["m"] = "2" (added to what is already there).
void parseQuery(const std::string& text, std::map<std::string, std::string>& args);
