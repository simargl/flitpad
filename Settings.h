// Settings.h - tiny key=value settings file (no FLTK).
#ifndef SETTINGS_H
#define SETTINGS_H

#include <map>
#include <string>

// File format: one "key=value" per line, '#' starts a comment line. Keys the
// program doesn't know about are kept when saving, so hand-edited entries such
// as icon_font survive.
class Settings {
public:
    bool load(const std::string& path);          // false if the file can't be read
    bool save(const std::string& path) const;

    std::string get(const std::string& key, const std::string& fallback = std::string()) const;
    // Integer value clamped to [lo, hi]; `fallback` if missing or not a number.
    int get_int(const std::string& key, int fallback, int lo, int hi) const;

    bool has(const std::string& key) const;
    void set(const std::string& key, const std::string& value);
    const std::map<std::string, std::string>& all() const { return values_; }
    void set_int(const std::string& key, int value);

private:
    std::map<std::string, std::string> values_;
};

#endif
