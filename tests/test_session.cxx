// Tests SessionFile.  Run: make test
#include "../SessionFile.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unistd.h>

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

static std::string tmpname() {
    char t[] = "/tmp/flitpad_session_XXXXXX";
    int fd = mkstemp(t);
    if (fd >= 0) close(fd);
    return t;
}

static std::string slurp(const std::string& p) {
    std::ifstream in(p.c_str());
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

int main() {
    const std::string path = tmpname();

    // round trip with all three tags
    {
        SessionFile s;
        s.settings.set("font_name", "DejaVu Sans Mono");
        s.settings.set_int("font_size", 19);
        s.settings.set("theme", "light");
        s.set_window(10, 20, 1300, 850);
        s.recent_files.push_back("/a/one.cpp");
        s.recent_files.push_back("/a/two words.md");
        CHECK(s.save(path));
        const std::string text = slurp(path);
        CHECK(text.find("[settings]") != std::string::npos && text.find("[recent]") != std::string::npos && text.find("[open]") == std::string::npos);
        CHECK(text.find("font_name=DejaVu Sans Mono") != std::string::npos);

        SessionFile t;
        CHECK(t.load(path));
        CHECK(t.settings.get("font_name") == "DejaVu Sans Mono");
        CHECK(t.settings.get_int("font_size", 0, 1, 99) == 19);
        CHECK(t.settings.get("theme") == "light");
        int x, y, w, h;
        CHECK(t.get_window(x, y, w, h) && x == 10 && y == 20 && w == 1300 && h == 850);
        CHECK(t.recent_files.size() == 2 && t.recent_files[0] == "/a/one.cpp" && t.recent_files[1] == "/a/two words.md");
        // saving again keeps it all
        CHECK(t.save(path));
        SessionFile u; u.load(path);
        CHECK(u.recent_files == t.recent_files && u.settings.all() == t.settings.all());
    }

    // previous session format is understood
    {
        { std::ofstream(path) << "100 200 1200 800\n/home/me/active.txt\n/home/me/a.txt\n/home/me/b.txt\n"; }
        SessionFile s;
        CHECK(s.load(path));
        int x, y, w, h;
        CHECK(s.get_window(x, y, w, h) && x == 100 && y == 200 && w == 1200 && h == 800);
        CHECK(s.settings.get("active_file") == "/home/me/active.txt");
        CHECK(s.recent_files.size() == 2 && s.recent_files[1] == "/home/me/b.txt");
        { std::ofstream(path) << "5 6 900 700\n\n/p/q\n"; }          // no active file
        SessionFile e; e.load(path);
        CHECK(!e.settings.has("active_file") && e.recent_files.size() == 1 && e.recent_files[0] == "/p/q");
    }

    // old ~/.flitpad_config is folded in without overriding anything
    {
        { std::ofstream(path) << "font_size=23\nicon_font=Font Awesome 6 Free\n"; }
        SessionFile s;
        s.settings.set("font_size", "17");
        s.import_legacy_settings(path);
        CHECK(s.settings.get("font_size") == "17");                    // already set: kept
        CHECK(s.settings.get("icon_font") == "Font Awesome 6 Free");   // missing: imported
        SessionFile none;
        none.import_legacy_settings("/nonexistent/file");             // harmless
        CHECK(none.settings.all().empty());
    }

    // hand-edited / damaged file
    {
        { std::ofstream(path) << "junk before tags\n# comment\n[Settings]\n font_size = 12 \nno equals sign\n=novalue\n"
                                 "[OPEN]\n/ignored\n[unknown]\nwhatever\n[RECENT]\n/a\n\n  /b  \n/a\n"; }
        SessionFile s;
        CHECK(s.load(path));
        CHECK(s.settings.get("font_size") == "12");                    // tags are case-insensitive, values trimmed
        CHECK(s.recent_files.size() == 2 && s.recent_files[0] == "/a" && s.recent_files[1] == "/b");   // duplicates dropped
        CHECK(s.settings.all().size() == 1);                           // unknown tags are ignored
    }
    { std::ofstream(path) << ""; }
    { SessionFile s; CHECK(s.load(path) && s.recent_files.empty() && s.settings.all().empty()); }
    { SessionFile s; CHECK(!s.load("/nonexistent/dir/session")); }

    // window sanity
    {
        SessionFile s;
        int x, y, w, h;
        CHECK(!s.get_window(x, y, w, h));
        s.settings.set("window", "1,2,3,4");     CHECK(!s.get_window(x, y, w, h));   // too small
        s.settings.set("window", "1,2,900");     CHECK(!s.get_window(x, y, w, h));
        s.settings.set("window", "1,2,900,700,5"); CHECK(!s.get_window(x, y, w, h));
        s.settings.set("window", "a,b,c,d");     CHECK(!s.get_window(x, y, w, h));
        s.settings.set("window", "-50,-20,900,700"); CHECK(s.get_window(x, y, w, h) && x == -50 && y == -20);
        s.settings.set("window", "1,2,90000,700"); CHECK(!s.get_window(x, y, w, h));
    }

    std::remove(path.c_str());
    std::cout << (checks - failures) << "/" << checks << " session checks passed\n";
    return failures ? 1 : 0;
}
