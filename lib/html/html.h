#pragma once

#include <string>

// Building blocks shared by the server-rendered pages of both Picos.

struct Notice
{
    const char* cls = ""; // "ok", "info" or "err"; empty = no notice
    std::string text;
};

// Result of handling a form POST: what to show after the redirect, and whether data must be saved.
struct FormResult
{
    Notice notice;
    bool changed = false;
};

std::string htmlEscape(const std::string& text);

// <!doctype html> ... <body><nav>…</nav><h1>heading</h1>. nav is raw HTML (links).
void appendPageStart(std::string& html, const char* title, const char* nav, const char* heading);
void appendPageEnd(std::string& html);
void appendNotice(std::string& html, const char* cls, const std::string& text);
// Table row label/value; skipped if value is empty.
void appendRow(std::string& html, const char* label, const std::string& value);
