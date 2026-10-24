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

HttpResponse htmlPage(const std::string& html)
{
    return {200, "text/html; charset=utf-8", html, "", ""};
}
