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
enum Style {
    PLAIN    = 'A',
    KEYWORD  = 'B',
    COMMENT  = 'C',
    STRING   = 'D',
    NUMBER   = 'E',
    TYPE     = 'F',  // functions, types, builtins, keys, properties
    VARIABLE = 'G',  // $vars, attributes, decorators, image alt text
    HEADING  = 'H',  // markdown headings (bold)
    BOLD     = 'I',  // markdown **bold** (bold)
    ITALIC   = 'J',  // markdown *italic* (italic)
    CODE     = 'K',  // markdown `code` and fenced blocks
    LINK     = 'L',  // markdown link text
    MARKUP   = 'M'   // markdown punctuation: bullets, >, |, urls, fences
};

static const int STYLE_COUNT = 13;

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
