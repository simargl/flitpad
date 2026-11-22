#include "FindReplaceDialog.h"
#include "TextEditorApp.h"
#include "Theme.h"
#include <FL/fl_draw.H>
#include <cstring>

FindReplaceDialog::FindReplaceDialog(TextEditorApp* app, bool replace_mode) 
    : Fl_Double_Window(540, replace_mode ? 190 : 140, replace_mode ? "Find and Replace" : "Find"),
      app_(app), is_replace_mode_(replace_mode) {
    
    color(Theme::window_bg());

    int y = 20;
    find_input_ = new Fl_Input(90, y, 435, 28, "Find:");
    find_input_->color(Theme::panel_bg());
    find_input_->textcolor(Theme::text_strong());
    find_input_->labelfont(FL_HELVETICA);
    find_input_->labelsize(13);
    find_input_->labelcolor(Theme::text());
    find_input_->when(FL_WHEN_ENTER_KEY_ALWAYS);   // Enter = Find Next
    find_input_->callback(find_enter_cb, this);
    y += 38;

    if (is_replace_mode_) {
        replace_input_ = new Fl_Input(90, y, 435, 28, "Replace:");
        replace_input_->color(Theme::panel_bg());
        replace_input_->textcolor(Theme::text_strong());
        replace_input_->labelfont(FL_HELVETICA);
        replace_input_->labelsize(13);
        replace_input_->labelcolor(Theme::text());
        replace_input_->when(FL_WHEN_ENTER_KEY_ALWAYS);   // Enter = Replace
        replace_input_->callback(replace_enter_cb, this);
        y += 38;
    } else {
        replace_input_ = nullptr;
    }

    match_case_check_ = new Fl_Check_Button(90, y, 150, 25, "Match case");
    match_case_check_->labelsize(13);
    match_case_check_->labelcolor(Theme::text());
    y += 42;

    Fl_Color btn_bg = Theme::button_bg();
    Fl_Color btn_txt = Theme::text_strong();

    if (is_replace_mode_) {
        int w1 = 85, w2 = 85, w3 = 75, w4 = 85, w5 = 65;
        int gap = 8;
        int total_w = w1 + w2 + w3 + w4 + w5 + 4 * gap;
        int bx = 540 - 15 - total_w;

        Fl_Button* btn_find_next = new Fl_Button(bx, y, w1, 30, "Find Next");
        btn_find_next->callback(find_next_cb, this);
        btn_find_next->color(btn_bg); btn_find_next->labelcolor(btn_txt);
        bx += w1 + gap;

        Fl_Button* btn_find_prev = new Fl_Button(bx, y, w2, 30, "Find Prev");
        btn_find_prev->callback(find_prev_cb, this);
        btn_find_prev->color(btn_bg); btn_find_prev->labelcolor(btn_txt);
        bx += w2 + gap;

        Fl_Button* btn_replace = new Fl_Button(bx, y, w3, 30, "Replace");
        btn_replace->callback(replace_cb, this);
        btn_replace->color(btn_bg); btn_replace->labelcolor(btn_txt);
        bx += w3 + gap;

        Fl_Button* btn_replace_all = new Fl_Button(bx, y, w4, 30, "Replace All");
        btn_replace_all->callback(replace_all_cb, this);
        btn_replace_all->color(btn_bg); btn_replace_all->labelcolor(btn_txt);
        bx += w4 + gap;

        Fl_Button* btn_close = new Fl_Button(bx, y, w5, 30, "Close");
        btn_close->callback(close_cb, this);
        btn_close->color(btn_bg); btn_close->labelcolor(btn_txt);
    } else {
        int w1 = 95, w2 = 95, w3 = 75;
        int gap = 8;
        int total_w = w1 + w2 + w3 + 2 * gap;
        int bx = 540 - 15 - total_w;

        Fl_Button* btn_find_next = new Fl_Button(bx, y, w1, 30, "Find Next");
        btn_find_next->callback(find_next_cb, this);
        btn_find_next->color(btn_bg); btn_find_next->labelcolor(btn_txt);
        bx += w1 + gap;

        Fl_Button* btn_find_prev = new Fl_Button(bx, y, w2, 30, "Find Prev");
        btn_find_prev->callback(find_prev_cb, this);
        btn_find_prev->color(btn_bg); btn_find_prev->labelcolor(btn_txt);
        bx += w2 + gap;

        Fl_Button* btn_close = new Fl_Button(bx, y, w3, 30, "Close");
        btn_close->callback(close_cb, this);
        btn_close->color(btn_bg); btn_close->labelcolor(btn_txt);
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

void FindReplaceDialog::find_enter_cb(Fl_Widget*, void* data) {
    static_cast<FindReplaceDialog*>(data)->do_find_next();
}

void FindReplaceDialog::replace_enter_cb(Fl_Widget*, void* data) {
    static_cast<FindReplaceDialog*>(data)->do_replace();
}

void FindReplaceDialog::set_query(const std::string& query) {
    find_input_->value(query.c_str());
    find_input_->position(static_cast<int>(query.size()), 0);  // cursor at end, mark at start = all selected
}

void FindReplaceDialog::focus_query() {
    find_input_->take_focus();
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
