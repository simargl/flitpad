#ifndef TEXT_EDITOR_APP_H
#define TEXT_EDITOR_APP_H

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Pixmap.H>
#include <string>
#include <vector>

#include "CustomTabs.h"

class TextEditorApp;
class FindReplaceDialog;
struct TabData;

class CustomFileBrowser : public Fl_Browser {
public:
    TextEditorApp* app_;
    Fl_Pixmap* folder_icon_;
    Fl_Pixmap* file_icon_;

    CustomFileBrowser(int x, int y, int w, int h, TextEditorApp* app, const char* l = 0);
    virtual ~CustomFileBrowser();
    int handle(int event) override;
};

class TextEditorApp : public Fl_Double_Window {
public:
    enum { STYLE_COUNT = 13 };  // must equal Highlighter::STYLE_COUNT

    CustomFileBrowser* file_browser_;
    Fl_Tabs* tabs_;
    Fl_Box* status_bar_;
    std::string current_dir_;
    std::vector<TabData*> open_tabs_;
    int font_size_;

    FindReplaceDialog* find_dialog_;
    FindReplaceDialog* replace_dialog_;

    static Fl_Text_Editor::Style_Table_Entry style_table_[STYLE_COUNT];

    TextEditorApp(int width, int height, const char* title);
    virtual ~TextEditorApp();

    // files and tabs
    void set_current_dir(const std::string& dir);
    void load_directory(const std::string& dir);
    void open_file(const std::string& path);
    void open_file(const std::string& path, bool focus_editor);
    void open_file_dialog();
    void new_file();
    void save_current_file();
    void save_current_file_as();
    bool save_tab(TabData* tab);
    bool save_tab_as(TabData* tab);
    void close_current_tab();
    bool close_tab(TabData* tab);
    void try_quit();

    void rename_selected_file();
    void delete_selected_file();
    void new_file_in_sidebar();

    TabData* get_current_tab();
    TabData* tab_for_editor(Fl_Text_Editor* editor);
    void select_tab(size_t index);
    void update_tab_label(TabData* tab);
    void update_status_bar();
    void set_status_message(const std::string& message);

    // editing
    void goto_line();
    void toggle_comment();
    void set_font_size(int size);

    // find / replace
    void show_find_dialog();
    void show_replace_dialog();
    void find_again(bool backwards);
    void find_text(const std::string& query, bool match_case);
    void find_previous_text(const std::string& query, bool match_case);
    void replace_text(const std::string& query, const std::string& replacement, bool match_case);
    void replace_all_text(const std::string& query, const std::string& replacement, bool match_case);

    int handle(int event) override;

    std::string get_absolute_path(const std::string& path);   // lexical: makes absolute, removes ./ and ../
    std::string canonical_path(const std::string& path);      // file identity: symlinks resolved
    std::string get_session_file_path();
    std::string get_command_file_path();
    std::string get_lock_file_path();

    void save_session();
    void load_session();

    // callbacks
    static void poll_commands_cb(void* data);
    static void style_update_cb(int pos, int n_inserted, int n_deleted, int n_restored, const char* deleted_text, void* cb_arg);
    static void buffer_modified_cb(int pos, int n_inserted, int n_deleted, int n_restored, const char* deleted_text, void* cb_arg);
    static void restyle_timeout_cb(void* data);
    static void restyle_tab(TabData* tab);
    static void deferred_close_cb(void* data);
    static void tabs_cb(Fl_Widget* w, void* data);
    static void browser_cb(Fl_Widget* w, void* data);
    static void window_close_cb(Fl_Widget* w, void* data);

    // editor key bindings
    static int kf_smart_enter(int key, Fl_Text_Editor* e);
    static int kf_tab(int key, Fl_Text_Editor* e);
    static int kf_shift_tab(int key, Fl_Text_Editor* e);

    static std::string get_language_name(const std::string& path, const std::string& content = "");

private:
    TabData* create_tab(const std::string& path, const std::string& name, const std::string& content,
                        bool untitled, bool crlf);
    void configure_editor(TabData* tab);
    void apply_language(TabData* tab);
    void destroy_tab(TabData* tab);
    bool write_tab_to_disk(TabData* tab);
    void schedule_close(Fl_Widget* group);
};

#endif // TEXT_EDITOR_APP_H
