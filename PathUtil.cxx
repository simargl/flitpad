#include "PathUtil.h"

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

}  // namespace PathUtil
