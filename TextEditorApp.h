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

class TextEditorApp;

struct TabData {
    Fl_Group* group;
    Fl_Text_Editor* editor;
    Fl_Text_Buffer* buffer;
    Fl_Text_Buffer* style_buffer;
    std::string filepath;
    std::string filename;
    bool is_modified;
    Fl_Tabs* tabs;
};

class CustomFileBrowser : public Fl_Browser {
public:
    TextEditorApp* app_;
    Fl_Pixmap* folder_icon_;
    Fl_Pixmap* file_icon_;

    CustomFileBrowser(int x, int y, int w, int h, TextEditorApp* app, const char* l = 0);
    virtual ~CustomFileBrowser();
    int handle(int event) override;
};

class CustomTabs : public Fl_Tabs {
public:
    CustomTabs(int x, int y, int w, int h, const char* l = 0) : Fl_Tabs(x, y, w, h, l) {}
};

class FindReplaceDialog; // Forward declaration

class TextEditorApp : public Fl_Double_Window {
public:
    CustomFileBrowser* file_browser_;
    Fl_Tabs* tabs_;
    Fl_Box* status_bar_;
    std::string current_dir_;
    std::vector<TabData*> open_tabs_;

    FindReplaceDialog* find_dialog_;
    FindReplaceDialog* replace_dialog_;

    static Fl_Text_Editor::Style_Table_Entry style_table_[5];

    TextEditorApp(int width, int height, const char* title);
    virtual ~TextEditorApp();

    void set_current_dir(const std::string& dir);
    void load_directory(const std::string& dir);
    void open_file(const std::string& path, bool focus_editor = true);
    void new_file();
    void save_current_file();
    bool save_tab(TabData* tab);
    void close_current_tab();
    void try_quit();

    void rename_selected_file();
    void delete_selected_file();
    void new_file_in_sidebar();

    TabData* get_current_tab();
    void update_tab_label(TabData* tab);
    void update_status_bar();

    void show_find_dialog();
    void show_replace_dialog();
    void find_text(const std::string& query, bool match_case);
    void find_previous_text(const std::string& query, bool match_case);
    void replace_text(const std::string& query, const std::string& replacement, bool match_case);
    void replace_all_text(const std::string& query, const std::string& replacement, bool match_case);

    int handle(int event) override;

    std::string get_absolute_path(const std::string& path);
    std::string get_session_file_path();
    std::string get_command_file_path();
    std::string get_lock_file_path();

    static void poll_commands_cb(void* data);
    static void style_update_cb(int pos, int n_inserted, int n_deleted, int n_restored, const char* deleted_text, void* cb_arg);
    static void buffer_modified_cb(int pos, int n_inserted, int n_deleted, int n_restored, const char* deleted_text, void* cb_arg);
    static std::string get_language_name(const std::string& path);
    static void tabs_cb(Fl_Widget* w, void* data);
    static void browser_cb(Fl_Widget* w, void* data);
    static void window_close_cb(Fl_Widget* w, void* data);

    void save_session();
    void load_session();
};

#endif // TEXT_EDITOR_APP_H