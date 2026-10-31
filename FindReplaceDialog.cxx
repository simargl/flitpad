#include "FindReplaceDialog.h"
#include "TextEditorApp.h"
#include <FL/fl_draw.H>
#include <cstring>

FindReplaceDialog::FindReplaceDialog(TextEditorApp* app, bool replace_mode) 
    : Fl_Double_Window(560, replace_mode ? 210 : 150, replace_mode ? "Find and Replace" : "Find"),
      app_(app), is_replace_mode_(replace_mode) {
    
    color(fl_rgb_color(40, 44, 52));

    int y = 20;
    find_input_ = new Fl_Input(100, y, 435, 32, "Find:");
    find_input_->color(fl_rgb_color(33, 37, 43));
    find_input_->textcolor(fl_rgb_color(220, 223, 228));
    find_input_->labelfont(FL_HELVETICA);
    find_input_->labelsize(14);
    find_input_->labelcolor(fl_rgb_color(171, 178, 191));
    find_input_->textfont(FL_HELVETICA);
    find_input_->textsize(14);
    y += 42;

    if (is_replace_mode_) {
        replace_input_ = new Fl_Input(100, y, 435, 32, "Replace:");
        replace_input_->color(fl_rgb_color(33, 37, 43));
        replace_input_->textcolor(fl_rgb_color(220, 223, 228));
        replace_input_->labelfont(FL_HELVETICA);
        replace_input_->labelsize(14);
        replace_input_->labelcolor(fl_rgb_color(171, 178, 191));
        replace_input_->textfont(FL_HELVETICA);
        replace_input_->textsize(14);
        y += 42;
    } else {
        replace_input_ = nullptr;
    }

    match_case_check_ = new Fl_Check_Button(100, y, 150, 25, "Match case");
    match_case_check_->labelfont(FL_HELVETICA);
    match_case_check_->labelsize(14);
    match_case_check_->labelcolor(fl_rgb_color(171, 178, 191));
    y += 42;

    Fl_Color btn_bg = fl_rgb_color(58, 63, 75);
    Fl_Color btn_txt = fl_rgb_color(220, 223, 228);

    if (is_replace_mode_) {
        int w1 = 90, w2 = 90, w3 = 80, w4 = 95, w5 = 65;
        int gap = 8;
        int total_w = w1 + w2 + w3 + w4 + w5 + 4 * gap;
        int bx = 560 - 15 - total_w;

        Fl_Button* btn_find_next = new Fl_Button(bx, y, w1, 32, "Find Next");
        btn_find_next->callback(find_next_cb, this);
        btn_find_next->color(btn_bg); btn_find_next->labelcolor(btn_txt);
        btn_find_next->labelfont(FL_HELVETICA); btn_find_next->labelsize(14);
        bx += w1 + gap;

        Fl_Button* btn_find_prev = new Fl_Button(bx, y, w2, 32, "Find Prev");
        btn_find_prev->callback(find_prev_cb, this);
        btn_find_prev->color(btn_bg); btn_find_prev->labelcolor(btn_txt);
        btn_find_prev->labelfont(FL_HELVETICA); btn_find_prev->labelsize(14);
        bx += w2 + gap;

        Fl_Button* btn_replace = new Fl_Button(bx, y, w3, 32, "Replace");
        btn_replace->callback(replace_cb, this);
        btn_replace->color(btn_bg); btn_replace->labelcolor(btn_txt);
        btn_replace->labelfont(FL_HELVETICA); btn_replace->labelsize(14);
        bx += w3 + gap;

        Fl_Button* btn_replace_all = new Fl_Button(bx, y, w4, 32, "Replace All");
        btn_replace_all->callback(replace_all_cb, this);
        btn_replace_all->color(btn_bg); btn_replace_all->labelcolor(btn_txt);
        btn_replace_all->labelfont(FL_HELVETICA); btn_replace_all->labelsize(14);
        bx += w4 + gap;

        Fl_Button* btn_close = new Fl_Button(bx, y, w5, 32, "Close");
        btn_close->callback(close_cb, this);
        btn_close->color(btn_bg); btn_close->labelcolor(btn_txt);
        btn_close->labelfont(FL_HELVETICA); btn_close->labelsize(14);
    } else {
        int w1 = 100, w2 = 100, w3 = 75;
        int gap = 8;
        int total_w = w1 + w2 + w3 + 2 * gap;
        int bx = 560 - 15 - total_w;

        Fl_Button* btn_find_next = new Fl_Button(bx, y, w1, 32, "Find Next");
        btn_find_next->callback(find_next_cb, this);
        btn_find_next->color(btn_bg); btn_find_next->labelcolor(btn_txt);
        btn_find_next->labelfont(FL_HELVETICA); btn_find_next->labelsize(14);
        bx += w1 + gap;

        Fl_Button* btn_find_prev = new Fl_Button(bx, y, w2, 32, "Find Prev");
        btn_find_prev->callback(find_prev_cb, this);
        btn_find_prev->color(btn_bg); btn_find_prev->labelcolor(btn_txt);
        btn_find_prev->labelfont(FL_HELVETICA); btn_find_prev->labelsize(14);
        bx += w2 + gap;

        Fl_Button* btn_close = new Fl_Button(bx, y, w3, 32, "Close");
        btn_close->callback(close_cb, this);
        btn_close->color(btn_bg); btn_close->labelcolor(btn_txt);
        btn_close->labelfont(FL_HELVETICA); btn_close->labelsize(14);
    }

    end();
    set_non_modal();
}

void FindReplaceDialog::find_next_cb(Fl_Widget*, void* data) {
    FindReplaceDialog* dlg = static_cast<FindReplaceDialog*>(data);
    dlg->do_find_next();
}

void FindReplaceDialog::find_prev_cb(Fl_Widget*, void* data) {
    FindReplaceDialog* dlg = static_cast<FindReplaceDialog*>(data);
    dlg->do_find_prev();
}

void FindReplaceDialog::replace_cb(Fl_Widget*, void* data) {
    FindReplaceDialog* dlg = static_cast<FindReplaceDialog*>(data);
    dlg->do_replace();
}

void FindReplaceDialog::replace_all_cb(Fl_Widget*, void* data) {
    FindReplaceDialog* dlg = static_cast<FindReplaceDialog*>(data);
    dlg->do_replace_all();
}

void FindReplaceDialog::close_cb(Fl_Widget*, void* data) {
    FindReplaceDialog* dlg = static_cast<FindReplaceDialog*>(data);
    dlg->hide();
}

void FindReplaceDialog::do_find_next() {
    const char* query = find_input_->value();
    if (!query || strlen(query) == 0) return;
    bool match_case = match_case_check_->value() != 0;
    app_->find_text(query, match_case);
}

void FindReplaceDialog::do_find_prev() {
    const char* query = find_input_->value();
    if (!query || strlen(query) == 0) return;
    bool match_case = match_case_check_->value() != 0;
    app_->find_previous_text(query, match_case);
}

void FindReplaceDialog::do_replace() {
    const char* query = find_input_->value();
    const char* rep = replace_input_ ? replace_input_->value() : "";
    if (!query || strlen(query) == 0) return;
    bool match_case = match_case_check_->value() != 0;
    app_->replace_text(query, rep, match_case);
}

void FindReplaceDialog::do_replace_all() {
    const char* query = find_input_->value();
    const char* rep = replace_input_ ? replace_input_->value() : "";
    if (!query || strlen(query) == 0) return;
    bool match_case = match_case_check_->value() != 0;
    app_->replace_all_text(query, rep, match_case);
}