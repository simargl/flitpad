// ColorSchemes.h - editor colour schemes (no FLTK).
//
// Each scheme lists the colours the editor needs; style_specs() turns that into one
// colour (+ bold/italic) per Highlighter style, already adjusted so FLTK keeps it
// (see ColorMath.h).
#ifndef COLOR_SCHEMES_H
#define COLOR_SCHEMES_H

#include <string>
#include <vector>

namespace ColorSchemes {

struct Scheme {
    const char* id;      // stored in the session file, e.g. "monokai"
    const char* name;    // shown in the menu
    bool dark;

    // editor
    unsigned bg, text, selection, cursor, gutter_bg, gutter_fg;
    // syntax roles
    unsigned comment;
    unsigned string;
    unsigned number;      // numbers and constants such as true / null / None
    unsigned keyword;     // statements: if, return, class ...
    unsigned identifier;  // functions, object keys
    unsigned type;        // type names, variables
    unsigned preproc;     // #include, decorators
    unsigned special;     // escape sequences
    unsigned op;          // operators
    bool keyword_bold, type_bold, type_italic, comment_italic;
    // accents for Markdown
    unsigned red, orange, yellow, green, cyan, blue, violet, magenta;
};

struct StyleSpec {
    unsigned rgb;
    bool bold;
    bool italic;
};

// All schemes, dark ones first.
const std::vector<const Scheme*>& all();

// nullptr if `id` is unknown.
const Scheme* find(const std::string& id);

// Default scheme of a mode: Monokai (dark) / Solarized Light (light).
const Scheme* fallback(bool dark);

// One entry per Highlighter style (Highlighter::STYLE_COUNT), contrast-adjusted.
std::vector<StyleSpec> style_specs(const Scheme& scheme);

// Line-number colour, adjusted for the gutter background.
unsigned gutter_fg(const Scheme& scheme);

}  // namespace ColorSchemes

#endif
