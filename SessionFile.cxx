#include "SessionFile.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string();
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string lower(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    return s;
}

bool is_header(const std::string& line) {
    if (line.size() < 3 || line[0] != '[' || line[line.size() - 1] != ']') return false;
    const std::string name = lower(trim(line.substr(1, line.size() - 2)));
    return name == "settings" || name == "recent";
}

bool parse_four_ints(std::string s, int out[4]) {
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == ',') s[i] = ' ';          // "x,y,w,h" and "x y w h"
    std::istringstream in(s);
    if (!(in >> out[0] >> out[1] >> out[2] >> out[3])) return false;
    std::string rest;
    return !(in >> rest);                      // exactly four numbers
}

}  // namespace

bool SessionFile::load(const std::string& path) {
    std::ifstream in(path.c_str());
    if (!in.is_open()) return false;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        lines.push_back(line);
    }

    bool tagged = false;
    for (size_t i = 0; i < lines.size() && !tagged; ++i) tagged = is_header(trim(lines[i]));

    if (!tagged) {   // previous format: "x y w h", active file, then one path per line
        int g[4];
        if (!lines.empty() && parse_four_ints(lines[0], g)) {
            std::ostringstream w;
            w << g[0] << "," << g[1] << "," << g[2] << "," << g[3];
            settings.set("window", w.str());
            if (lines.size() > 1 && !trim(lines[1]).empty()) settings.set("active_file", trim(lines[1]));
            for (size_t i = 2; i < lines.size(); ++i)
                if (!trim(lines[i]).empty()) recent_files.push_back(trim(lines[i]));
        }
        return true;
    }

    std::string section;
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string t = trim(lines[i]);
        if (t.empty() || t[0] == '#') continue;
        // any [tag] starts a section; sections this version doesn't know are skipped
        if (t.size() >= 3 && t[0] == '[' && t[t.size() - 1] == ']') { section = lower(trim(t.substr(1, t.size() - 2))); continue; }
        if (section == "settings") {
            const size_t eq = t.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = trim(t.substr(0, eq));
            if (!key.empty()) settings.set(key, trim(t.substr(eq + 1)));
        } else if (section == "recent") {
            if (std::find(recent_files.begin(), recent_files.end(), t) == recent_files.end()) recent_files.push_back(t);
        }
    }
    return true;
}

bool SessionFile::save(const std::string& path) const {
    const std::string tmp = path + ".tmp";   // write beside it and rename: no half-written file after a crash
    {
        std::ofstream out(tmp.c_str(), std::ios::trunc);
        if (!out.is_open()) return false;
        out << "# Flitpad session and settings (rewritten when Flitpad exits)\n[settings]\n";
        const std::map<std::string, std::string>& all = settings.all();
        for (std::map<std::string, std::string>::const_iterator it = all.begin(); it != all.end(); ++it)
            out << it->first << "=" << it->second << "\n";
        out << "\n[recent]\n";
        for (size_t i = 0; i < recent_files.size(); ++i) out << recent_files[i] << "\n";
        out.close();
        if (!out) { std::remove(tmp.c_str()); return false; }
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(path.c_str());   // Windows cannot rename over an existing file
        if (std::rename(tmp.c_str(), path.c_str()) != 0) { std::remove(tmp.c_str()); return false; }
    }
    return true;
}

void SessionFile::import_legacy_settings(const std::string& config_path) {
    Settings old;
    if (!old.load(config_path)) return;
    const std::map<std::string, std::string>& all = old.all();
    for (std::map<std::string, std::string>::const_iterator it = all.begin(); it != all.end(); ++it)
        if (!settings.has(it->first)) settings.set(it->first, it->second);
}

bool SessionFile::get_window(int& x, int& y, int& w, int& h) const {
    int g[4];
    if (!parse_four_ints(settings.get("window"), g)) return false;
    if (g[2] < 300 || g[3] < 200 || g[2] > 20000 || g[3] > 20000) return false;     // implausible size
    if (g[0] < -20000 || g[0] > 20000 || g[1] < -20000 || g[1] > 20000) return false;
    x = g[0]; y = g[1]; w = g[2]; h = g[3];
    return true;
}

void SessionFile::set_window(int x, int y, int w, int h) {
    std::ostringstream o;
    o << x << "," << y << "," << w << "," << h;
    settings.set("window", o.str());
}
