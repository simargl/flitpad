// EditHelpers.h - pure text-editing logic (no FLTK), so it can be unit tested.
#ifndef EDIT_HELPERS_H
#define EDIT_HELPERS_H

#include <string>

namespace EditHelpers {

struct NewlineEdit {
    int delete_before;       // characters to delete before the cursor first
    std::string text;        // text to insert afterwards
    int cursor_back;         // then move the cursor this many characters back
    NewlineEdit() : delete_before(0), cursor_back(0) {}
};

// What pressing Enter should do. `before` / `after` are the parts of the
// current line left and right of the cursor.
//  * keeps the current indentation
//  * indents one level after { ( [ (and ':' in Python/Nim/YAML)
//  * splits "{|}" into an indented block
//  * Markdown: continues "- ", "1. ", "- [ ] " and "> "; Enter on an empty
//    item removes the marker and ends the list
NewlineEdit smart_newline(const std::string& language, const std::string& before,
                          const std::string& after, int indent_width);

// Prefix every non-empty line / remove up to `width` leading blanks.
std::string indent_lines(const std::string& block, int width);
std::string outdent_lines(const std::string& block, int width);

// Comment or uncomment a block of whole lines using the language's line-comment
// token. Returns false if the language has none. `all_commented` tells which
// way it went.
bool toggle_line_comments(const std::string& language, const std::string& block,
                          std::string& out, bool* all_commented = 0);

}  // namespace EditHelpers

#endif
