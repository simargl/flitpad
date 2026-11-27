#include "PathUtil.h"

#include <cctype>
#include <cstdlib>

#ifndef _WIN32
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#endif

namespace PathUtil {

std::string canonical(const std::string& p) {
#ifndef _WIN32
    char buf[PATH_MAX];
    if (realpath(p.c_str(), buf)) return buf;

    // Not there (yet): resolve the directory, keep the file name.
    size_t slash = p.find_last_of('/');
    if (slash != std::string::npos && slash + 1 < p.size()) {
        std::string dir = p.substr(0, slash);
        if (dir.empty()) dir = "/";
        if (realpath(dir.c_str(), buf)) {
            std::string resolved = buf;
            if (resolved == "/") return "/" + p.substr(slash + 1);
            return resolved + p.substr(slash);
        }
    }
#endif
    return p;
}

bool is_symlink(const std::string& path) {
#ifndef _WIN32
    struct stat st;
    return lstat(path.c_str(), &st) == 0 && S_ISLNK(st.st_mode);
#else
    (void)path;
    return false;
#endif
}

std::string unique_name(const std::string& name, const std::function<bool(const std::string&)>& taken) {
    if (!taken(name)) return name;

    std::string stem = name, ext;
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) {
        stem = name.substr(0, dot);
        ext = name.substr(dot);
    }
    // drop an existing "_N" suffix so the numbers don't pile up
    size_t k = stem.size();
    while (k > 0 && std::isdigit(static_cast<unsigned char>(stem[k - 1]))) --k;
    if (k > 1 && k < stem.size() && stem[k - 1] == '_') stem.erase(k - 1);

    for (int i = 1; i < 100000; ++i) {
        std::string candidate = stem + "_" + std::to_string(i) + ext;
        if (!taken(candidate)) return candidate;
    }
    return name;
}

std::string url_decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        // "%XX" needs two more characters after the '%'
        const bool escape = s[i] == '%' && i + 2 < s.size() &&
                            std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
                            std::isxdigit(static_cast<unsigned char>(s[i + 2]));
        if (escape) {
            out += static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), 0, 16));
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

std::vector<std::string> parse_uri_list(const std::string& text) {
    std::vector<std::string> paths;
    size_t b = 0;
    while (b <= text.size()) {
        size_t e = text.find('\n', b);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(b, e - b);
        b = e + 1;

        const size_t first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) continue;                                // blank
        line = line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
        if (line[0] == '#') continue;                                            // comment

        if (line.compare(0, 7, "file://") != 0) return std::vector<std::string>();
        std::string rest = line.substr(7);
        if (!rest.empty() && rest[0] != '/') {                                   // file://host/path
            const size_t slash = rest.find('/');
            if (slash == std::string::npos || rest.substr(0, slash) != "localhost") return std::vector<std::string>();
            rest = rest.substr(slash);
        }
        std::string path = url_decode(rest);
        if (path.size() > 2 && path[0] == '/' && std::isalpha(static_cast<unsigned char>(path[1])) && path[2] == ':')
            path.erase(0, 1);                                                    // Windows: /C:/x -> C:/x
        if (path.empty()) return std::vector<std::string>();
        paths.push_back(path);
    }
    return paths;
}

}  // namespace PathUtil
