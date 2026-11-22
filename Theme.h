// Theme.h - every colour Flitpad uses lives here.
//
// GtkSourceView's "solarized-dark" scheme (Craig Russell, Solarized palette by
// Ethan Schoonover). The values below are taken from that scheme file:
//   text        base1 on base03        selection   base00 (text on it: base03 there)
//   cursor      base1                  line-numbers base01 on base02
// To try another theme, change the palette and the role functions below;
// nothing else hard-codes a colour.
#ifndef THEME_H
#define THEME_H

#include <FL/Fl.H>
#include "ColorMath.h"

namespace Theme {

// Solarized palette
static const unsigned BASE03 = 0x002b36;  // background
static const unsigned BASE02 = 0x073642;  // background highlights
static const unsigned BASE01 = 0x586e75;  // comments, secondary content
static const unsigned BASE00 = 0x657b83;
static const unsigned BASE0  = 0x839496;  // body text
static const unsigned BASE1  = 0x93a1a1;  // emphasized text
static const unsigned BASE2  = 0xeee8d5;
static const unsigned BASE3  = 0xfdf6e3;
static const unsigned YELLOW  = 0xb58900;
static const unsigned ORANGE  = 0xcb4b16;
static const unsigned RED     = 0xdc322f;
static const unsigned MAGENTA = 0xd33682;
static const unsigned VIOLET  = 0x6c71c4;
static const unsigned BLUE    = 0x268bd2;
static const unsigned CYAN    = 0x2aa198;
static const unsigned GREEN   = 0x859900;

inline Fl_Color rgb(unsigned hex) {
    return fl_rgb_color(static_cast<uchar>((hex >> 16) & 0xff), static_cast<uchar>((hex >> 8) & 0xff),
                        static_cast<uchar>(hex & 0xff));
}

// Colour of a syntax style. FLTK replaces colours that are too close to the
// background's luminance with plain white (see ColorMath.h), which on base03
// would turn most of the palette white. syntax() keeps the hue and brightens
// the colour just enough to survive; colours that are bright enough (base1,
// yellow, green) come out unchanged.
inline Fl_Color syntax(unsigned hex, int min_diff = 112) {
    return rgb(ColorMath::lifted(hex, BASE03, min_diff));
}

// roles
inline Fl_Color window_bg()      { return rgb(BASE03); }
inline Fl_Color panel_bg()       { return rgb(BASE02); }   // sidebar, status bar, input fields
inline Fl_Color editor_bg()      { return rgb(BASE03); }
inline Fl_Color text()           { return rgb(BASE0); }   // UI labels (sidebar, status bar)
inline Fl_Color text_strong()    { return rgb(BASE1); }   // editor and input text: the scheme's "text"
inline Fl_Color line_number_fg() { return rgb(ColorMath::lifted(BASE01, BASE02, 106)); }
inline Fl_Color line_number_bg() { return rgb(BASE02); }
inline Fl_Color cursor()         { return rgb(BASE1); }
inline Fl_Color selection()      { return rgb(BASE00); }
inline Fl_Color button_bg()      { return rgb(BASE02); }

// Global FLTK colours: widgets I don't colour explicitly (scrollbars, buttons, ...)
// derive theirs from these.
inline void apply() {
    Fl::scheme("gtk+");
    Fl::background(0x00, 0x2b, 0x36);
    Fl::background2(0x07, 0x36, 0x42);
    Fl::foreground(0x83, 0x94, 0x96);
    Fl::set_color(FL_SELECTION_COLOR, 0x65, 0x7b, 0x83);   // base00
}

}  // namespace Theme

#endif
