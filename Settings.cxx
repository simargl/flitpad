#include "Settings.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace {

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string();
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

}  // namespace

bool Settings::load(const std::string& path) {
    std::ifstream in(path.c_str());
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        if (!key.empty()) values_[key] = trim(line.substr(eq + 1));
    }
    return true;
}

bool Settings::save(const std::string& path) const {
    // write next to the target and rename, so a crash can't leave a half-written file
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp.c_str(), std::ios::trunc);
        if (!out.is_open()) return false;
        out << "# Flitpad settings\n";
        for (std::map<std::string, std::string>::const_iterator it = values_.begin(); it != values_.end(); ++it)
            out << it->first << "=" << it->second << "\n";
        out.close();
        if (!out) { std::remove(tmp.c_str()); return false; }
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(path.c_str());  // Windows can't rename over an existing file
        if (std::rename(tmp.c_str(), path.c_str()) != 0) {
            std::remove(tmp.c_str());
            return false;
        }
    }
    return true;
}

std::string Settings::get(const std::string& key, const std::string& fallback) const {
    std::map<std::string, std::string>::const_iterator it = values_.find(key);
    return it == values_.end() ? fallback : it->second;
}

int Settings::get_int(const std::string& key, int fallback, int lo, int hi) const {
    std::map<std::string, std::string>::const_iterator it = values_.find(key);
    if (it == values_.end() || it->second.empty()) return fallback;
    char* end = 0;
    long v = std::strtol(it->second.c_str(), &end, 10);
    if (*end != '\0') return fallback;
    if (v < lo) return lo;
    if (v > hi) return hi;
    return static_cast<int>(v);
}

bool Settings::has(const std::string& key) const { return values_.find(key) != values_.end(); }

void Settings::set(const std::string& key, const std::string& value) { values_[key] = value; }

void Settings::set_int(const std::string& key, int value) { values_[key] = std::to_string(value); }
