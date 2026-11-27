#include "TextFile.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace TextFile {

bool read(const std::string& path, std::string& text, bool& crlf, std::string& error, size_t max_bytes) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        error = "Could not open file:\n" + path;
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size > 0 && static_cast<unsigned long long>(size) > max_bytes) {
        error = "This file is too large to edit here (" + std::to_string(static_cast<long long>(size) / (1024 * 1024)) +
                " MB):\n" + path;
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::stringstream stream;
    stream << file.rdbuf();
    std::string content = stream.str();

    if (content.find('\0') != std::string::npos) {
        error = "This looks like a binary file, not text:\n" + path;
        return false;
    }
    crlf = content.find("\r\n") != std::string::npos;
    content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
    text.swap(content);
    return true;
}

bool write(const std::string& path, const std::string& text, bool crlf, std::string& error) {
    std::string content = text;
    if (!content.empty() && content[content.size() - 1] != '\n') content += '\n';

    if (crlf) {
        std::string converted;
        converted.reserve(content.size() + content.size() / 16);
        for (size_t i = 0; i < content.size(); ++i) {
            if (content[i] == '\n') converted += '\r';
            converted += content[i];
        }
        content.swap(converted);
    }

    std::ofstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        error = "Could not save file:\n" + path;
        return false;
    }
    file << content;
    file.close();
    if (!file) {
        error = "Could not save file (disk full?):\n" + path;
        return false;
    }
    return true;
}

long long modified_time(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    return static_cast<long long>(st.st_mtime);
}

}  // namespace TextFile
