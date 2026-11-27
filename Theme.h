// Theme.h - the colours the widgets use, taken from the active colour scheme.
//
// The scheme can be switched at run time (View menu); set_scheme() + apply() and
// then re-colouring the widgets is all that takes (TextEditorApp::apply_theme()).
// The scheme definitions live in ColorSchemes.cxx.
#ifndef THEME_H
#define THEME_H

#include <FL/Fl.H>
#include "ColorMath.h"
#include "ColorSchemes.h"

namespace Theme {

inline const ColorSchemes::Scheme*& scheme_ref() {
    static const ColorSchemes::Scheme* s = ColorSchemes::fallback(true);
    return s;
}
inline const ColorSchemes::Scheme& scheme() { return *scheme_ref(); }
inline void set_scheme(const ColorSchemes::Scheme* s) { if (s) scheme_ref() = s; }

inline Fl_Color rgb(unsigned hex) {
    return fl_rgb_color(static_cast<uchar>((hex >> 16) & 0xff), static_cast<uchar>((hex >> 8) & 0xff),
                        static_cast<uchar>(hex & 0xff));
}

// Sidebar, status bar, menus and input fields sit on a darker (dark
// schemes) or a hint darker (light schemes) shade of the editor background.
inline unsigned panel_hex() { return ColorMath::blend(scheme().bg, 0x000000, scheme().dark ? 0.20 : 0.06); }
inline unsigned ui_text_hex() { return ColorMath::ensure_contrast(scheme().text, panel_hex()); }

inline Fl_Color window_bg()      { return rgb(scheme().bg); }
inline Fl_Color editor_bg()      { return rgb(scheme().bg); }
inline Fl_Color panel_bg()       { return rgb(panel_hex()); }
inline Fl_Color text()           { return rgb(ui_text_hex()); }               // UI labels
inline Fl_Color text_strong()    { return rgb(scheme().text); }               // editor and input text
inline Fl_Color line_number_fg() { return rgb(ColorSchemes::gutter_fg(scheme())); }
inline Fl_Color line_number_bg() { return rgb(scheme().gutter_bg); }
inline Fl_Color cursor()         { return rgb(scheme().cursor); }
inline Fl_Color selection()      { return rgb(scheme().selection); }
inline Fl_Color button_bg()      { return panel_bg(); }

// Global FLTK colours: widgets that are not coloured explicitly (scrollbars,
// buttons, ...) derive theirs from these. Call again after changing the scheme.
inline void apply() {
    const unsigned bg = scheme().bg, panel = panel_hex(), fg = ui_text_hex(), sel = scheme().selection;
    Fl::background(static_cast<uchar>(bg >> 16), static_cast<uchar>(bg >> 8), static_cast<uchar>(bg));
    Fl::background2(static_cast<uchar>(panel >> 16), static_cast<uchar>(panel >> 8), static_cast<uchar>(panel));
    Fl::foreground(static_cast<uchar>(fg >> 16), static_cast<uchar>(fg >> 8), static_cast<uchar>(fg));
    Fl::set_color(FL_SELECTION_COLOR, static_cast<uchar>(sel >> 16), static_cast<uchar>(sel >> 8), static_cast<uchar>(sel));
}

}  // namespace Theme

#endif
