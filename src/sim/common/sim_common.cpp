#include "sim_common.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

std::string envOr(const char* name, const char* fallback)
{
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? value : fallback;
}

std::string listenHost()
{
    return envOr("SIM_HOST", "127.0.0.1");
}

int64_t nowUtc()
{
    return static_cast<int64_t>(std::time(nullptr));
}

uint32_t nowMs()
{
    using namespace std::chrono;
    return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

std::string dataDir()
{
    return envOr("SIM_DATA", "sim-data");
}

void makeDataDir()
{
#ifdef _WIN32
    _mkdir(dataDir().c_str());
#else
    mkdir(dataDir().c_str(), 0755);
#endif
}

bool readDataFile(const char* name, std::string& out)
{
    std::ifstream in(dataDir() + "/" + name + ".json", std::ios::binary);
    if (!in)
    {
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

void writeDataFile(const char* name, const std::string& data)
{
    std::string path = dataDir() + "/" + name + ".json";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << data;
    if (!out)
    {
        std::printf("%s: write FAILED\n", path.c_str());
    }
}

HttpResponse redirect(const std::string& location)
{
    HttpResponse r;
    r.status = 303;
    r.location = location;
    return r;
}

HttpResponse notFound()
{
    return {404, "text/plain; charset=utf-8", "Nicht gefunden", "", ""};
}

namespace
{
std::string base64Decode(const std::string& in)
{
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    uint32_t buffer = 0;
    int bits = 0;
    for (char c : in)
    {
        size_t v = chars.find(c);
        if (v == std::string::npos)
        {
            break; // '=' padding or garbage ends the data
        }
        buffer = (buffer << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out += static_cast<char>((buffer >> bits) & 0xFF);
        }
    }
    return out;
}
}

bool basicAuthOk(const HttpRequest& req, const std::string& user, const std::string& password)
{
    const std::string prefix = "Basic ";
    if (req.authorization.compare(0, prefix.size(), prefix) != 0)
    {
        return false;
    }
    return base64Decode(req.authorization.substr(prefix.size())) == user + ":" + password;
}

HttpResponse basicAuthChallenge(const char* realm)
{
    HttpResponse r{401, "text/plain; charset=utf-8", "Anmeldung erforderlich", "", ""};
    r.wwwAuthenticate = std::string("Basic realm=\"") + realm + "\"";
    return r;
}

HttpResponse simPage(const std::string& html)
{
    auto link = [](uint16_t port, const char* path, const char* text) {
        return std::string("<a style=\"color:#9cf\" href=\"http://localhost:") + std::to_string(port) + path + "\">" +
               text + "</a>";
    };
    std::string banner = "<div style=\"background:#1d232b;color:#fff;padding:8px 12px;border-radius:6px;"
                         "margin-bottom:12px;font-size:.9rem\"><strong>PC-Simulator</strong> · " +
                         link(KASSE_PORT, "/kasse", "Kasse") + " · " + link(KASSE_PORT, "/automat", "Automat") + " · " + link(KASSE_PORT, "/tarif", "Tarif") +
                         " · " + link(SCHRANKEN_PORT, "/kamera", "Kamera") + " · " +
                         link(SCHRANKEN_PORT, "/registrierung", "Registrierung") + " · " +
                         link(SCHRANKEN_PORT, "/betrieb", "Betrieb") + " · " +
                         link(SCHRANKEN_PORT, "/sim", "Steuerung (Schleifen)") + "</div>";
    std::string out = html;
    size_t body = out.find("<body>");
    if (body != std::string::npos)
    {
        out.insert(body + 6, banner);
    }
    return {200, "text/html; charset=utf-8", out, "", ""};
}
