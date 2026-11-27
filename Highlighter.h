// Highlighter.h - toolkit-independent syntax highlighting.
//
// highlight() takes the language name and the full document text and returns a
// style string of exactly the same length (one style character per byte), ready
// to be dropped into an Fl_Text_Buffer used as a style buffer.
#ifndef HIGHLIGHTER_H
#define HIGHLIGHTER_H

#include <string>

namespace Highlighter {

// Style characters. They map 1:1 onto TextEditorApp::style_table_.
// The colour of each style comes from the active colour scheme (ColorSchemes.h);
// the roles are:
//   KEYWORD  statements (if, return)        STRING / NUMBER  literals and constants
//   TYPE     functions, object keys         VARIABLE         type names, variables
//   PREPROC  #include, decorators           SPECIAL          escape sequences
//   OPERATOR + - = ->
enum Style {
    PLAIN    = 'A',
    KEYWORD  = 'B',  // if, return, class ...
    COMMENT  = 'C',
    STRING   = 'D',
    NUMBER   = 'E',  // numbers and constants: 42, true, null, None ...
    TYPE     = 'F',  // functions, builtins, object keys, properties (identifiers)
    VARIABLE = 'G',  // type names, $variables, HTML attributes
    HEADING  = 'H',  // markdown heading level 1 (bold)
    BOLD     = 'I',  // markdown **bold** (bold)
    ITALIC   = 'J',  // markdown *italic* (italic)
    CODE     = 'K',  // markdown `code` and fenced blocks
    LINK     = 'L',  // markdown link text
    MARKUP   = 'M',  // markdown markers: bullets, >, |, fences, rules
    PREPROC  = 'N',  // #include, #define, decorators, annotations
    SPECIAL  = 'O',  // escape sequences inside strings
    HEADING2 = 'P',  // markdown heading levels 2..6 (HEADING3 .. HEADING6 follow)
    HEADING3 = 'Q',
    HEADING4 = 'R',
    HEADING5 = 'S',
    HEADING6 = 'T',
    URL      = 'U',  // markdown link destinations and bare URLs
    QUOTE    = 'V',  // markdown block quote text
    OPERATOR = 'W'   // + - * / = < > ! & | ^ ~ ? in the C-like and scripting languages
};
// Type names share the yellow style with variables.
static const char TYPENAME = VARIABLE;

static const int STYLE_COUNT = 23;

// Private-use characters (FontAwesome, Nerd Fonts, ...) get the "icon twin" of
// their style: style + ICON_STYLE_OFFSET. The editor draws those twins with an
// icon font in the same colour, because FLTK has no per-glyph font fallback.
static const int ICON_STYLE_OFFSET = 23;
static const int TOTAL_STYLE_COUNT = 46;

// Above this size highlighting is skipped to keep typing responsive.
static const size_t MAX_HIGHLIGHT_BYTES = 3u * 1024u * 1024u;

// Language from file name/extension first; shebang/XML/HTML/PHP sniffing of
// `head` (the first few hundred bytes) only when the name says nothing.
std::string detect_language(const std::string& path, const std::string& head);

// Returns styles.size() == text.size().
std::string highlight(const std::string& language, const std::string& text);

// "//", "#", "--", ";" ... or "" when the language has no line comment.
std::string line_comment_prefix(const std::string& language);

}  // namespace Highlighter

#endif
