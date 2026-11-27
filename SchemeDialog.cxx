#include "SchemeDialog.h"
#include "Theme.h"
#include "NativeTheme.h"

#include <FL/Fl.H>
#include <string>

SchemeDialog::SchemeDialog(const ColorSchemes::Scheme* current, const Preview& preview)
    : Fl_Double_Window(300, 290, "Theme"), schemes_(ColorSchemes::all()), preview_(preview), list_(nullptr),
      accepted_(false) {
    list_ = new Fl_Hold_Browser(10, 10, 280, 232);
    list_->textsize(15);
    list_->when(FL_WHEN_CHANGED);   // arrow keys preview too
    list_->callback(list_cb, this);
    int select = 1;
    for (size_t i = 0; i < schemes_.size(); ++i) {
        list_->add((std::string(schemes_[i]->name) + (schemes_[i]->dark ? "   (dark)" : "   (light)")).c_str());
        if (schemes_[i] == current) select = static_cast<int>(i) + 1;
    }
    list_->value(select);

    Fl_Button* ok = new Fl_Button(110, 252, 85, 28, "OK");
    ok->callback(ok_cb, this);
    ok->shortcut(FL_Enter);
    Fl_Button* cancel = new Fl_Button(205, 252, 85, 28, "Cancel");
    cancel->callback(cancel_cb, this);

    end();
    callback(cancel_cb, this);   // Esc and the window's close button
    set_modal();
    apply_theme();
}

void SchemeDialog::apply_theme() {
    color(Theme::window_bg());
    list_->color(Theme::panel_bg());
    list_->textcolor(Theme::text_strong());
    for (int i = 0; i < children(); ++i) {
        Fl_Widget* w = child(i);
        if (dynamic_cast<Fl_Button*>(w)) {
            w->color(Theme::button_bg());
            w->labelcolor(Theme::text_strong());
        }
    }
    redraw();
}

const ColorSchemes::Scheme* SchemeDialog::selected() const {
    const int line = list_->value();
    if (line < 1 || line > static_cast<int>(schemes_.size())) return nullptr;
    return schemes_[static_cast<size_t>(line) - 1];
}

void SchemeDialog::list_cb(Fl_Widget*, void* data) {
    SchemeDialog* d = static_cast<SchemeDialog*>(data);
    const ColorSchemes::Scheme* s = d->selected();
    if (!s) return;
    if (d->preview_) d->preview_(s);
    d->apply_theme();
    if (Fl::event_clicks() > 0) ok_cb(nullptr, d);   // double-click = OK
}

void SchemeDialog::ok_cb(Fl_Widget*, void* data) {
    SchemeDialog* d = static_cast<SchemeDialog*>(data);
    d->accepted_ = d->selected() != nullptr;
    d->hide();
}

void SchemeDialog::cancel_cb(Fl_Widget*, void* data) { static_cast<SchemeDialog*>(data)->hide(); }

const ColorSchemes::Scheme* SchemeDialog::run(const ColorSchemes::Scheme* current, const Preview& preview,
                                              int center_x, int center_y) {
    SchemeDialog dlg(current, preview);
    dlg.position(center_x - dlg.w() / 2, center_y - dlg.h() / 2);
    dlg.show();
    NativeTheme::apply(&dlg);
    while (dlg.shown()) Fl::wait();
    return dlg.accepted_ ? dlg.selected() : nullptr;
}
