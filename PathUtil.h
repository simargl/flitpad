// PathUtil.h - file identity helpers (no FLTK).
#ifndef PATH_UTIL_H
#define PATH_UTIL_H

#include <string>

namespace PathUtil {

// Absolute path with every symlink resolved, so that two spellings of the same
// file compare equal. Works for files that don't exist yet (the directory part is
// resolved and the name appended). If nothing can be resolved the input is
// returned unchanged. On Windows it currently returns the input unchanged.
std::string canonical(const std::string& absolute_path);

// True if the final path component is itself a symbolic link.
bool is_symlink(const std::string& path);

}  // namespace PathUtil

#endif
