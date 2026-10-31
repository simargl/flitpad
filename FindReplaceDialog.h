#ifndef FIND_REPLACE_DIALOG_H
#define FIND_REPLACE_DIALOG_H

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Button.H>

class TextEditorApp;

class FindReplaceDialog : public Fl_Double_Window {
private:
    TextEditorApp* app_;
    Fl_Input* find_input_;
    Fl_Input* replace_input_;
    Fl_Check_Button* match_case_check_;
    bool is_replace_mode_;

    static void find_next_cb(Fl_Widget*, void* data);
    static void find_prev_cb(Fl_Widget*, void* data);
    static void replace_cb(Fl_Widget*, void* data);
    static void replace_all_cb(Fl_Widget*, void* data);
    static void close_cb(Fl_Widget*, void* data);

public:
    FindReplaceDialog(TextEditorApp* app, bool replace_mode);

    void do_find_next();
    void do_find_prev();
    void do_replace();
    void do_replace_all();
};

#endif
