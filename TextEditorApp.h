#ifndef TEXT_EDITOR_APP_H
#define TEXT_EDITOR_APP_H

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Pixmap.H>
#include <vector>
#include <string>
#include "TabData.h"
#include "CustomTabs.h"
#include "FindReplaceDialog.h"

class TextEditorApp;

class CustomFileBrowser : public Fl_Browser {
private:
    TextEditorApp* app_;

public:
    Fl_Image* folder_icon_;
    Fl_Image* file_icon_;

    CustomFileBrowser(int x, int y, int w, int h, TextEditorApp* app, const char* l = 0);
    ~CustomFileBrowser();
    int handle(int event) override;
};

class TextEditorApp : public Fl_Double_Window {
private:
    CustomFileBrowser* file_browser_;
    Fl_Tabs* tabs_;
    Fl_Box* status_bar_;
    std::vector<TabData*> open_tabs_;
    std::string current_dir_;

    FindReplaceDialog* find_dialog_;
    FindReplaceDialog* replace_dialog_;

    static Fl_Text_Editor::Style_Table_Entry style_table_[5];

    std::string get_absolute_path(const std::string& path);
    std::string get_session_file_path();
    std::string get_command_file_path();
    std::string get_lock_file_path();

    static void poll_commands_cb(void* data);
    static void style_update_cb(int pos, int n_inserted, int n_deleted, int, const char*, void* cb_arg);
    static void buffer_modified_cb(int, int n_inserted, int n_deleted, int, const char*, void* cb_arg);
    static void update_tab_label(TabData* tab);
    std::string get_language_name(const std::string& path);

    static void tabs_cb(Fl_Widget* w, void* data);
    static void browser_cb(Fl_Widget* w, void* data);
    static void window_close_cb(Fl_Widget*, void* data);

public:
    TextEditorApp(int width, int height, const char* title);
    ~TextEditorApp();

    void set_current_dir(const std::string& dir);
    void load_directory(const std::string& dir);
    void save_session();
    void load_session();
    TabData* get_current_tab();
    void new_file();
    void open_file(const std::string& path);
    bool save_tab(TabData* tab);
    void save_current_file();
    void close_current_tab();
    void try_quit();
    void show_find_dialog();
    void show_replace_dialog();
    
    void find_text(const std::string& query, bool match_case);
    void find_previous_text(const std::string& query, bool match_case);
    void replace_text(const std::string& query, const std::string& replacement, bool match_case);
    void replace_all_text(const std::string& query, const std::string& replacement, bool match_case);

    void rename_selected_file();
    void delete_selected_file();
    void new_file_in_sidebar();

    void update_status_bar();
    int handle(int event) override;
};

#endif