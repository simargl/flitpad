// IconFont.h - picking an installed icon font (FontAwesome, Nerd Fonts). No FLTK.
#ifndef ICON_FONT_H
#define ICON_FONT_H

#include <string>

namespace IconFont {

// Rates one installed font for use as the icon font; 0 = unusable, higher is
// better. `attributes` uses FLTK's bits (1 = bold, 2 = italic). `wanted` is the
// user's `icon_font=` setting; an exact (case/space-insensitive) match wins.
//
// Preference: FontAwesome 4, FontAwesome 5/6 (Solid = the bold face, which has
// all the free icons), Symbols Nerd Font, any patched "... Nerd Font".
// "Brands" faces are only a last resort.
int score(const std::string& family, int attributes, const std::string& wanted);

}  // namespace IconFont

#endif
