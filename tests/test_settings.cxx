// Tests Settings and IconFont.  Run: make test
#include "../Settings.h"
#include "../IconFont.h"
#include "../ColorMath.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unistd.h>

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

int main() {
    char tmpl[] = "/tmp/flitpad_settings_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return 2;
    close(fd);
    const std::string path = tmpl;

    // missing / empty file
    Settings s;
    CHECK(!s.load("/nonexistent/dir/file"));
    CHECK(s.get_int("font_size", 17, 9, 40) == 17);
    CHECK(s.get("icon_font", "x") == "x");

    // round trip, unknown keys survive
    s.set_int("font_size", 21);
    s.set("icon_font", "Font Awesome 6 Free");
    s.set("future_key", "a=b c");
    CHECK(s.save(path));
    Settings t;
    CHECK(t.load(path));
    CHECK(t.get_int("font_size", 17, 9, 40) == 21);
    CHECK(t.get("icon_font") == "Font Awesome 6 Free");
    CHECK(t.get("future_key") == "a=b c");                  // value may contain '='
    t.set_int("font_size", 12);
    CHECK(t.save(path));                                    // overwrite an existing file
    Settings u;
    u.load(path);
    CHECK(u.get_int("font_size", 17, 9, 40) == 12 && u.get("icon_font") == "Font Awesome 6 Free");

    // hand-edited / damaged file
    { std::ofstream(path) << "# c\n\n  font_size =  99 \nbad line\n=novalue\nicon_font=  X Y \r\nother=12abc\nneg=-5\n"; }
    Settings v;
    CHECK(v.load(path));
    CHECK(v.get_int("font_size", 17, 9, 40) == 40);          // clamped
    CHECK(v.get_int("neg", 17, 9, 40) == 9);
    CHECK(v.get_int("other", 17, 9, 40) == 17);              // not a number -> fallback
    CHECK(v.get("icon_font") == "X Y");
    CHECK(v.get("bad line", "none") == "none");
    std::remove(path.c_str());

    // icon font choice
    using IconFont::score;
    CHECK(score("DejaVu Sans Mono", 0, "") == 0);
    CHECK(score("Courier", 0, "") == 0);
    CHECK(score("FontAwesome", 0, "") > score("Symbols Nerd Font", 0, ""));
    CHECK(score("Font Awesome 6 Free", 1, "") > score("Font Awesome 6 Free", 0, ""));   // Solid beats Regular
    CHECK(score("Font Awesome 5 Free", 1, "") > score("Symbols Nerd Font Mono", 0, ""));
    CHECK(score("Font Awesome 6 Free", 2, "") == 0);                                     // never italic
    CHECK(score("Font Awesome 6 Brands", 0, "") > 0 && score("Font Awesome 6 Brands", 0, "") < score("Symbols Nerd Font", 0, ""));
    CHECK(score("JetBrainsMono Nerd Font", 0, "") > 0);
    CHECK(score("JetBrainsMono Nerd Font", 1, "") == 0);
    CHECK(score("Symbols Nerd Font", 0, "") > score("JetBrainsMono Nerd Font", 0, ""));
    CHECK(score("My Icons", 0, "my  icons") >= 1000);                                    // user override wins
    CHECK(score("My Icons", 0, "my  icons") > score("FontAwesome", 0, "my icons"));
    CHECK(score("", 0, "") == 0);

    // colour lifting (FLTK replaces colours too close to the background by white/black)
    {
        const unsigned bg = 0x002b36;
        const unsigned accents[] = {0xcb4b16, 0xdc322f, 0xd33682, 0x6c71c4, 0x268bd2, 0x2aa198, 0x859900, 0xb58900, 0x586e75, 0x93a1a1, 0xeee8d5};
        for (size_t i = 0; i < sizeof(accents) / sizeof(*accents); ++i) {
            unsigned c = ColorMath::lifted(accents[i], bg);
            CHECK(ColorMath::luminance(c) - ColorMath::luminance(bg) > 105);     // clears FLTK's ~99 threshold
            CHECK(ColorMath::lifted(c, bg) == c);                                // idempotent
            // hue is kept: no channel gets darker, and the ordering of the channels is not inverted
            for (int sh = 0; sh <= 16; sh += 8) CHECK(((c >> sh) & 255) >= ((accents[i] >> sh) & 255));
        }
        CHECK(ColorMath::lifted(0x93a1a1, bg) == 0x93a1a1);                       // already bright: untouched
        CHECK(ColorMath::lifted(0xeee8d5, bg) == 0xeee8d5);
        CHECK(ColorMath::lifted(0x000000, 0x000000, 300) == 0x000000);            // impossible target: unchanged
        unsigned o = ColorMath::lifted(0xcb4b16, bg);                              // orange stays orange
        CHECK(((o >> 16) & 255) > ((o >> 8) & 255) && ((o >> 8) & 255) > (o & 255));
        unsigned b = ColorMath::lifted(0x268bd2, bg);                              // blue stays blue
        CHECK((b & 255) > ((b >> 8) & 255) && ((b >> 8) & 255) > ((b >> 16) & 255));
    }

    std::cout << (checks - failures) << "/" << checks << " settings checks passed\n";
    return failures ? 1 : 0;
}
