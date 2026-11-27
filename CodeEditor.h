#ifndef CODE_EDITOR_H
#define CODE_EDITOR_H

#include <FL/Fl.H>
#include <FL/Fl_Text_Editor.H>
#include <functional>
#include <string>

// Fl_Text_Editor that exposes what FLTK 1.3 keeps protected.
class CodeEditor : public Fl_Text_Editor {
public:
    CodeEditor(int x, int y, int w, int h, const char* l = 0) : Fl_Text_Editor(x, y, w, h, l) {}

    // Buffer position nearest to the window coordinates (x, y), as the caret would
    // be placed by a click. xy_to_position() is protected in Fl_Text_Display.
    int position_at(int x, int y) const { return xy_to_position(x, y, CURSOR_POS); }

    // Called with the text of a drop / paste; returns true if it was a list of files
    // and has been handled (the text is then not inserted).
    std::function<bool(const std::string&)> on_drop;

    int handle(int event) override {
        if (event == FL_PASTE && on_drop && Fl::event_text() &&
            on_drop(std::string(Fl::event_text(), static_cast<size_t>(Fl::event_length()))))
            return 1;
        return Fl_Text_Editor::handle(event);
    }
};

#endif
