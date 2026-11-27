// SessionFile.h - everything Flitpad remembers, in one file (~/.flitpad_session). No FLTK.
//
//   # Flitpad session and settings
//   [settings]
//   font_name=Courier
//   font_size=17
//   theme=dark
//   window=100,100,1200,800
//   ...
//   [recent]          <- the files that were open at exit, in tab order; reopened on start
//   /home/me/a.cpp
//   /home/me/notes.md
//
// Unknown keys inside [settings] are kept, unknown tags are ignored. The two older
// files (the previous session format and ~/.flitpad_config) are read once and folded in.
#ifndef SESSION_FILE_H
#define SESSION_FILE_H

#include <string>
#include <vector>

#include "Settings.h"

class SessionFile {
public:
    Settings settings;                       // [settings]
    std::vector<std::string> recent_files;   // [recent]: reopened on start

    // Reads the new tagged format or, if the file has no tags, the previous
    // format (geometry line, active file, one path per line). False if unreadable.
    bool load(const std::string& path);
    bool save(const std::string& path) const;

    // Adds keys from an old ~/.flitpad_config that are not set yet.
    void import_legacy_settings(const std::string& config_path);

    // "window=x,y,w,h": false if missing or implausible.
    bool get_window(int& x, int& y, int& w, int& h) const;
    void set_window(int x, int y, int w, int h);
};

#endif
