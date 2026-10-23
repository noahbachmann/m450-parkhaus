#include "html.h"

namespace
{
    const char* STYLE = "body{font-family:system-ui,sans-serif;max-width:860px;margin:0 auto;padding:16px;"
                        "background:#f6f7f9;color:#1d232b}"
                        "nav a{margin-right:16px;font-weight:600}"
                        "h1{font-size:1.5rem}h2{font-size:1.15rem;margin-top:28px}"
                        "form{margin:12px 0}input,button,select{font-size:1rem;padding:6px 10px}"
                        "button{cursor:pointer}"
                        "table{border-collapse:collapse;width:100%;background:#fff}"
                        "th,td{text-align:left;padding:6px 8px;border-bottom:1px solid #dde1e6}"
                        ".card{background:#fff;border:1px solid #dde1e6;border-radius:6px;padding:12px 16px;margin:12px 0}"
                        ".card th{width:180px;color:#555}"
                        ".notice{padding:10px 14px;border-radius:6px;margin:12px 0;font-weight:600}"
                        ".ok{background:#dff3e4;color:#14532d}.info{background:#fff4d6;color:#713f12}"
                        ".err{background:#fde2e1;color:#7f1d1d}"
                        ".methods label{display:block;margin:6px 0}.muted{color:#666;font-size:.9rem}"
                        ".scroll{overflow-x:auto}"
                        ".lanes{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:12px}"
                        ".terminal{font-family:monospace;background:#1d232b;color:#e6f0ff;padding:10px;"
                        "border-radius:4px;min-height:1.4em}"
                        "form.inline{display:inline;margin:0}";
}

std::string htmlEscape(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (char c : text)
    {
        switch (c)
        {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&#39;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

void appendPageStart(std::string& html, const char* title, const char* nav, const char* heading)
{
    html += "<!doctype html><html lang=\"de\"><head><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>";
    html += title;
    html += " – Parkhaus</title><style>";
    html += STYLE;
    html += "</style></head><body><nav>";
    html += nav;
    html += "</nav><h1>";
    html += heading;
    html += "</h1>";
}

void appendPageEnd(std::string& html)
{
    html += "</body></html>";
}

void appendNotice(std::string& html, const char* cls, const std::string& text)
{
    html += "<p class=\"notice ";
    html += cls;
    html += "\">" + htmlEscape(text) + "</p>";
}

void appendRow(std::string& html, const char* label, const std::string& value)
{
    if (value.empty())
    {
        return;
    }
    html += "<tr><th>";
    html += label;
    html += "</th><td>" + htmlEscape(value) + "</td></tr>";
}
