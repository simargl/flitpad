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

// ASCII-only case conversion (UTF-8 multi-byte characters are left untouched).
std::string change_case(const std::string& text, bool upper);

// A single replacement of doc[start, end) by `text`, plus where the caret and
// the selection (sel_start < 0: none) end up afterwards. valid == false: nothing to do.
struct RegionEdit {
    bool valid;
    int start, end;
    std::string text;
    int cursor;
    int sel_start, sel_end;
    RegionEdit() : valid(false), start(0), end(0), cursor(0), sel_start(-1), sel_end(-1) {}
};

// Whole-line commands on `doc`, acting on the lines touched by the selection
// [sel_start, sel_end) or, without a selection, on the caret's line.
// A selection that ends right after a newline does not include the next line.
RegionEdit duplicate_lines(const std::string& doc, int sel_start, int sel_end, int cursor, bool has_selection);
RegionEdit delete_lines(const std::string& doc, int sel_start, int sel_end, int cursor, bool has_selection);
RegionEdit move_lines(const std::string& doc, int sel_start, int sel_end, int cursor, bool has_selection, bool up);

}  // namespace EditHelpers

#endif
