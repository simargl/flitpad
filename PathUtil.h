// PathUtil.h - file identity helpers (no FLTK).
#ifndef PATH_UTIL_H
#define PATH_UTIL_H

#include <functional>
#include <string>
#include <vector>

namespace PathUtil {

// Absolute path with every symlink resolved, so that two spellings of the same
// file compare equal. Works for files that don't exist yet (the directory part is
// resolved and the name appended). If nothing can be resolved the input is
// returned unchanged. On Windows it currently returns the input unchanged.
std::string canonical(const std::string& absolute_path);

// True if the final path component is itself a symbolic link.
bool is_symlink(const std::string& path);

// `name` if `taken(name)` is false, otherwise the first free "stem_N.ext" (N = 1, 2, ...).
// A numeric suffix already on the name is replaced, so "untitled_3" -> "untitled_1",
// "notes.txt" -> "notes_1.txt", ".hidden" -> ".hidden_1".
std::string unique_name(const std::string& name, const std::function<bool(const std::string&)>& taken);

// "%20" -> " " etc. Malformed escapes are left as they are.
std::string url_decode(const std::string& s);

// Local paths from a text/uri-list (what a file drop or "copy file" delivers):
// "file:///home/me/a%20b.txt" -> "/home/me/a b.txt". Blank lines and "#" comments
// are skipped. Returns an empty list unless EVERY other line is a file:// URI for
// this machine, so ordinary pasted text is never mistaken for a file list.
std::vector<std::string> parse_uri_list(const std::string& text);

}  // namespace PathUtil

#endif
