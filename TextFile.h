// TextFile.h - reading and writing text files (no FLTK).
#ifndef TEXT_FILE_H
#define TEXT_FILE_H

#include <string>

namespace TextFile {

// Reads a text file into `text` with "\n" line endings; `crlf` says whether the
// file used "\r\n". Returns false and sets `error` (a sentence for the user) if the
// file can't be read, looks binary (contains NUL bytes) or is larger than `max_bytes`.
bool read(const std::string& path, std::string& text, bool& crlf, std::string& error,
          size_t max_bytes = 100u * 1024u * 1024u);

// Writes `text` ("\n" line endings; a final newline is added if missing), as "\r\n"
// when `crlf` is set. Returns false and sets `error` on any failure.
bool write(const std::string& path, const std::string& text, bool crlf, std::string& error);

// Modification time in seconds since the epoch; 0 if the file doesn't exist.
long long modified_time(const std::string& path);

}  // namespace TextFile

#endif
