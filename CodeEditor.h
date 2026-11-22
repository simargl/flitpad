#ifndef CODE_EDITOR_H
#define CODE_EDITOR_H

#include <FL/Fl_Text_Editor.H>

// Fl_Text_Editor that exposes what FLTK 1.3 keeps protected.
class CodeEditor : public Fl_Text_Editor {
public:
    CodeEditor(int x, int y, int w, int h, const char* l = 0) : Fl_Text_Editor(x, y, w, h, l) {}

    // Buffer position nearest to the window coordinates (x, y), as the caret would
    // be placed by a click. xy_to_position() is protected in Fl_Text_Display.
    int position_at(int x, int y) const { return xy_to_position(x, y, CURSOR_POS); }
};

#endif
