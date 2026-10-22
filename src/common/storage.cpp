#include <Arduino.h>
#include <LittleFS.h>

#include "storage.h"

namespace
{
String jsonPath(const char* name)
{
    return String("/") + name + ".json";
}

String tmpPath(const char* name)
{
    return String("/") + name + ".tmp";
}
}

bool storageBegin()
{
    return LittleFS.begin(); // formats automatically on first boot
}

bool storageRead(const char* name, std::string& out)
{
    String tmp = tmpPath(name);
    if (LittleFS.exists(tmp))
    {
        LittleFS.remove(tmp);
    }

    File f = LittleFS.open(jsonPath(name), "r");
    if (!f)
    {
        return false;
    }
    out.clear();
    char buf[128];
    while (f.available())
    {
        int n = f.read(reinterpret_cast<uint8_t*>(buf), sizeof(buf));
        if (n <= 0)
        {
            break;
        }
        out.append(buf, n);
    }
    f.close();
    return true;
}

bool storageWrite(const char* name, const std::string& data)
{
    String tmp = tmpPath(name);
    File f = LittleFS.open(tmp, "w");
    if (!f)
    {
        return false;
    }
    size_t written = f.write(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    f.close();
    if (written != data.size())
    {
        LittleFS.remove(tmp);
        return false;
    }
    return LittleFS.rename(tmp, jsonPath(name)); // littlefs rename replaces the target atomically
}
