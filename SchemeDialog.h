#ifndef SCHEME_DIALOG_H
#define SCHEME_DIALOG_H

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Button.H>
#include <functional>
#include <vector>

#include "ColorSchemes.h"

// Small modal "pick a colour scheme" dialog. Selecting a scheme previews it right
// away (through the callback); OK keeps it, Cancel / Esc / closing the window does not.
class SchemeDialog : public Fl_Double_Window {
public:
    typedef std::function<void(const ColorSchemes::Scheme*)> Preview;

    // Returns the chosen scheme, or nullptr if cancelled (the caller then restores
    // whatever it had before previewing). The dialog is centred on (center_x, center_y).
    static const ColorSchemes::Scheme* run(const ColorSchemes::Scheme* current, const Preview& preview,
                                           int center_x, int center_y);

private:
    SchemeDialog(const ColorSchemes::Scheme* current, const Preview& preview);

    std::vector<const ColorSchemes::Scheme*> schemes_;
    Preview preview_;
    Fl_Hold_Browser* list_;
    bool accepted_;

    const ColorSchemes::Scheme* selected() const;
    void apply_theme();   // the dialog re-colours itself while a scheme is previewed

    static void list_cb(Fl_Widget*, void* data);
    static void ok_cb(Fl_Widget*, void* data);
    static void cancel_cb(Fl_Widget*, void* data);
};

#endif
