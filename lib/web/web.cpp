#include "web.h"

#include <cstdlib>

bool WebRequest::hasArg(const std::string& name) const
{
    return args.count(name) != 0;
}

std::string WebRequest::arg(const std::string& name) const
{
    auto it = args.find(name);
    return it == args.end() ? "" : it->second;
}

WebResponse redirect(const std::string& location)
{
    WebResponse r;
    r.status = 303;
    r.contentType = "text/plain; charset=utf-8";
    r.location = location;
    return r;
}

WebResponse htmlPage(const std::string& html)
{
    WebResponse r;
    r.body = html;
    return r;
}

WebResponse jsonReply(int status, const std::string& body)
{
    WebResponse r;
    r.status = status;
    r.contentType = "application/json";
    r.body = body;
    return r;
}

WebResponse notFound()
{
    WebResponse r;
    r.status = 404;
    r.contentType = "text/plain; charset=utf-8";
    r.body = "Nicht gefunden";
    return r;
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

void parseQuery(const std::string& text, std::map<std::string, std::string>& args)
{
    size_t pos = 0;
    while (pos < text.size())
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
