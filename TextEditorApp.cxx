#include "TextEditorApp.h"
#include "FindReplaceDialog.h"
#include "TabData.h"
#include "Highlighter.h"
#include "EditHelpers.h"
#include "PathUtil.h"
#include "Theme.h"
#include "CodeEditor.h"
#include "TabSwitcher.h"
#include "IconFont.h"
#include "TextFile.h"
#include "SchemeDialog.h"
#include "NativeTheme.h"
#include <FL/filename.H>
#include <FL/fl_ask.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Menu_Item.H>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <algorithm>
#include <vector>

static_assert(TextEditorApp::STYLE_COUNT == Highlighter::TOTAL_STYLE_COUNT, "style table size mismatch");

static const int DEFAULT_FONT_SIZE = 17;
static const char* const DEFAULT_FONT_NAME = "Courier";
static const int MIN_FONT_SIZE = 9;
static const int MAX_FONT_SIZE = 40;
static const int TAB_WIDTH = 4;

// context-menu / shortcut commands
enum MenuCommand {
    CMD_UNDO = 1, CMD_CUT, CMD_COPY, CMD_PASTE, CMD_SELECT_ALL, CMD_COMMENT,
    CMD_DUPLICATE_LINE, CMD_DELETE_LINE, CMD_LINE_UP, CMD_LINE_DOWN, CMD_THEME
};

enum SidebarCommand { SB_NEW_FILE = 1, SB_RENAME, SB_DELETE, SB_COPY_PATH, SB_REFRESH };
enum TabCommand { TAB_CLOSE = 1, TAB_CLOSE_OTHERS, TAB_CLOSE_ALL, TAB_COPY_PATH };

// 16x16 XPM Icon for Folders (Gold/Yellow)
static const char* folder_xpm[] = {
  "16 16 2 1",
  "  c None",
  "# c #E5C07B",
  "                ",
  "  ####          ",
  " ######         ",
  " ################",
  " ################",
  " ################",
  " ################",
  " ################",
  " ################",
  " ################",
  " ################",
  "                 ",
  "                 ",
  "                 ",
  "                 ",
  "                 "
};

// 16x16 XPM Icon for Files (Blue)
static const char* file_xpm[] = {
  "16 16 2 1",
  "  c None",
  "# c #61AFEF",
  "                ",
  "   ##########   ",
  "   #        #   ",
  "   #  ##### #   ",
  "   #      # #   ",
  "   #  ##### #   ",
  "   #      # #   ",
  "   #  ##### #   ",
  "   #      # #   ",
  "   #  ##### #   ",
  "   #      # #   ",
  "   #        #   ",
  "   ##########   ",
  "                ",
  "                ",
  "                "
};

CustomFileBrowser::CustomFileBrowser(int x, int y, int w, int h, TextEditorApp* app, const char* l)
    : Fl_Browser(x, y, w, h, l), app_(app) {
    folder_icon_ = new Fl_Pixmap(folder_xpm);
    file_icon_ = new Fl_Pixmap(file_xpm);
}

CustomFileBrowser::~CustomFileBrowser() {
    delete folder_icon_;
    delete file_icon_;
}

int CustomFileBrowser::handle(int event) {
    if (event == FL_DND_ENTER || event == FL_DND_DRAG || event == FL_DND_RELEASE || event == FL_DND_LEAVE) return 1;
    if (event == FL_PASTE && Fl::event_text() &&
        app_->open_dropped(std::string(Fl::event_text(), static_cast<size_t>(Fl::event_length()))))
        return 1;
    if (event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE) {
        // Let the browser select the item under the mouse, but without running our
        // callback (which would open the file); then show the context menu.
        const uchar saved = when();
        when(FL_WHEN_NEVER);
        Fl_Browser::handle(event);
        when(saved);
        take_focus();
        app_->show_sidebar_menu();
        return 1;
    }
    if (event == FL_KEYBOARD) {
        int key = Fl::event_key();
        if (value() > 0) {
            if (key == (FL_F + 2)) {
                app_->rename_selected_file();
                return 1;
            }
            if (key == FL_Delete) {
                app_->delete_selected_file();
                return 1;
            }
        }
    }
    return Fl_Browser::handle(event);
}

// Letters match Highlighter::Style; entries 23..45 are the "icon twins" of 0..22.
// The contents come from the active colour scheme (rebuild_style_table()).
Fl_Text_Editor::Style_Table_Entry TextEditorApp::style_table_[TextEditorApp::STYLE_COUNT];

static std::string home_file(const char* name) {
    const char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (home) return std::string(home) + "/" + name;
    return name;
}

static bool ends_with_slash(const std::string& s) {
    return !s.empty() && (s[s.size() - 1] == '/' || s[s.size() - 1] == '\\');
}

static std::string join_path(const std::string& dir, const std::string& name) {
    std::string p = dir;
    if (!p.empty() && !ends_with_slash(p)) p += "/";
    return p + name;
}

static std::string buffer_range(Fl_Text_Buffer* buf, int a, int b) {
    char* raw = buf->text_range(a, b);
    std::string s = raw ? raw : "";
    std::free(raw);
    return s;
}

std::string TextEditorApp::get_absolute_path(const std::string& path) {
    std::string cleaned = path;
    if (cleaned.rfind("file://", 0) == 0) {
        cleaned = PathUtil::url_decode(cleaned.substr(7));
        if (cleaned.rfind("localhost", 0) == 0) cleaned = cleaned.substr(9);
    }

    char abs_path[1024];
    if (fl_filename_absolute(abs_path, sizeof(abs_path), cleaned.c_str())) {
        cleaned = std::string(abs_path);
    }
    std::replace(cleaned.begin(), cleaned.end(), '\\', '/');
    return cleaned;
}

// Identity of a file: absolute, symlinks resolved. Two tabs are the same document
// exactly when their canonical paths are equal. get_absolute_path() is only a
// lexical clean-up and would treat link/a.txt and real/a.txt as different files.
std::string TextEditorApp::canonical_path(const std::string& path) {
    return PathUtil::canonical(get_absolute_path(path));
}

std::string TextEditorApp::get_session_file_path() { return home_file(".flitpad_session"); }
std::string TextEditorApp::get_command_file_path() { return home_file(".flitpad_commands"); }
std::string TextEditorApp::get_lock_file_path() { return home_file(".flitpad_lock"); }

void TextEditorApp::poll_commands_cb(void* data) {
    TextEditorApp* self = static_cast<TextEditorApp*>(data);
    std::string cmd_path = self->get_command_file_path();
    std::ifstream file(cmd_path.c_str());
    if (file.is_open()) {
        std::string line;
        std::vector<std::string> files_to_open;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) files_to_open.push_back(line);
        }
        file.close();
        std::remove(cmd_path.c_str());

        for (size_t i = 0; i < files_to_open.size(); ++i) self->open_file(files_to_open[i]);
        if (!files_to_open.empty()) {
            self->show();  // raise the window when another instance handed us files
            self->redraw();
        }
    }
    Fl::repeat_timeout(0.5, poll_commands_cb, data);
}

// ---------------------------------------------------------------------------
// syntax highlighting plumbing
// ---------------------------------------------------------------------------

// Re-highlights the whole buffer, but only touches the part of the style buffer
// that actually changed, so FLTK redraws as little as possible.
void TextEditorApp::restyle_tab(TabData* tab) {
    if (!tab || !tab->buffer || !tab->style_buffer) return;
    tab->restyle_pending = false;

    Fl_Text_Buffer* sbuf = tab->style_buffer;
    const int length = tab->buffer->length();
    if (length == 0) {
        sbuf->text("");
        return;
    }

    std::string text = buffer_range(tab->buffer, 0, length);
    std::string styles = Highlighter::highlight(tab->language, text);
    styles.resize(static_cast<size_t>(length), 'A');

    // first document with private-use glyphs: look for an icon font (once)
    TextEditorApp* app = tab->tabs ? static_cast<TextEditorApp*>(tab->tabs->top_window()) : nullptr;
    if (app && !app->icon_font_resolved_ &&
        std::any_of(styles.begin(), styles.end(), [](char c) { return c >= 'A' + Highlighter::ICON_STYLE_OFFSET; }))
        app->resolve_icon_font();

    std::string old_styles = buffer_range(sbuf, 0, sbuf->length());
    if (old_styles.size() != styles.size()) {
        sbuf->text(styles.c_str());
        return;
    }
    size_t first = 0;
    while (first < styles.size() && styles[first] == old_styles[first]) ++first;
    if (first == styles.size()) return;  // nothing changed
    size_t last = styles.size();
    while (last > first && styles[last - 1] == old_styles[last - 1]) --last;
    sbuf->replace(static_cast<int>(first), static_cast<int>(last), styles.substr(first, last - first).c_str());
}

void TextEditorApp::restyle_timeout_cb(void* data) {
    restyle_tab(static_cast<TabData*>(data));
}

// Called synchronously on every buffer change. It only keeps the style buffer the
// same length as the text (new text inherits the style before it, so typing inside
// a comment does not flicker) and schedules the real work a few ms later.
void TextEditorApp::style_update_cb(int pos, int n_inserted, int n_deleted, int, const char*, void* cb_arg) {
    if (n_inserted == 0 && n_deleted == 0) return;  // selection change only
    TabData* tab = static_cast<TabData*>(cb_arg);
    if (!tab || !tab->buffer || !tab->style_buffer) return;

    Fl_Text_Buffer* sbuf = tab->style_buffer;
    if (n_inserted > 0) {
        char inherit = 'A';
        if (pos > 0 && pos <= sbuf->length()) inherit = sbuf->byte_at(pos - 1);
        sbuf->replace(pos, pos + n_deleted, std::string(static_cast<size_t>(n_inserted), inherit).c_str());
    } else {
        sbuf->remove(pos, pos + n_deleted);
    }

    if (!tab->restyle_pending) {
        tab->restyle_pending = true;
        double delay = 0.02 + tab->buffer->length() / 4000000.0;  // bigger files re-highlight less eagerly
        Fl::add_timeout(delay, restyle_timeout_cb, tab);
    }
}

void TextEditorApp::buffer_modified_cb(int, int n_inserted, int n_deleted, int, const char*, void* cb_arg) {
    if (n_inserted == 0 && n_deleted == 0) return;
    TabData* tab = static_cast<TabData*>(cb_arg);
    if (!tab->is_modified) {
        tab->is_modified = true;
        TextEditorApp* app = tab->tabs ? static_cast<TextEditorApp*>(tab->tabs->top_window()) : nullptr;
        if (app) app->update_tab_label(tab);
    }
}

void TextEditorApp::update_tab_label(TabData* tab) {
    std::string label = tab->filename;
    if (tab->is_modified) label += " *";
    tab->group->copy_label(label.c_str());
    tab->group->redraw();
    if (tab->tabs) tab->tabs->redraw();
    update_window_title();   // the title shows the same "*"; saving by keyboard never reaches update_status_bar()
}

// "name * - FlitPad" for the current tab. Only touches the window when the text changes.
void TextEditorApp::update_window_title() {
    TabData* tab = get_current_tab();
    const std::string title = tab ? tab->filename + (tab->is_modified ? " *" : "") + " - FlitPad" : std::string("FlitPad");
    if (title != window_title_) {
        window_title_ = title;
        copy_label(title.c_str());
    }
}

void TextEditorApp::set_status_message(const std::string& message) {
    status_bar_->copy_label((" " + message).c_str());
    status_bar_->redraw();
}

void TextEditorApp::update_status_bar() {
    TabData* tab = get_current_tab();
    update_window_title();   // also follows tab switches
    if (!tab) {
        status_bar_->copy_label(" No file open  |  UTF-8");
        return;
    }
    Fl_Text_Buffer* buf = tab->buffer;
    int pos = tab->editor->insert_position();
    int line = buf->count_lines(0, pos) + 1;
    int col = buf->count_displayed_characters(buf->line_start(pos), pos) + 1;

    std::string sel;
    int s = 0, e = 0;
    if (buf->selection_position(&s, &e) && e > s) sel = "  |  " + std::to_string(e - s) + " selected";

    std::string text = " Ln " + std::to_string(line) + ", Col " + std::to_string(col) + sel + "  |  UTF-8  |  " +
                       (tab->crlf ? "CRLF" : "LF") + "  |  " + tab->language + "  |  " +
                       (tab->is_untitled ? std::string("(unsaved)") : tab->filepath);
    status_bar_->copy_label(text.c_str());
}

void TextEditorApp::tabs_cb(Fl_Widget* w, void* data) {
    TextEditorApp* self = static_cast<TextEditorApp*>(data);
    Fl_Tabs* tabs = static_cast<Fl_Tabs*>(w);
    Fl_Widget* current_tab = tabs->value();
    if (!current_tab) return;

    for (size_t i = 0; i < self->open_tabs_.size(); ++i) {
        TabData* tab = self->open_tabs_[i];
        if (tab->group == current_tab) {
            tab->editor->take_focus();
            size_t last_slash = tab->filepath.find_last_of("/\\");
            if (last_slash != std::string::npos && !tab->is_untitled) {
                std::string dir = tab->filepath.substr(0, last_slash);
                if (!dir.empty() && self->canonical_path(dir) != self->canonical_path(self->current_dir_)) self->set_current_dir(dir);
            }
            break;
        }
    }
    self->update_status_bar();
    self->check_external_change(self->get_current_tab());
}

// ---------------------------------------------------------------------------
// sidebar
// ---------------------------------------------------------------------------

void TextEditorApp::load_directory(const std::string& dir) {
    file_browser_->clear();

    dirent** files = nullptr;
    int n = fl_filename_list(dir.c_str(), &files, fl_casenumericsort);
    if (n > 0) {
        std::vector<std::string> folder_list;
        std::vector<std::string> file_list;

        for (int i = 0; i < n; ++i) {
            if (!files[i]) continue;
            std::string filename = files[i]->d_name;

            if (filename == "." || filename == "./") {
                std::free(files[i]);
                continue;
            }

            if (filename == ".." || filename == "../") {
                folder_list.insert(folder_list.begin(), filename);  // keep ".." on top
                std::free(files[i]);
                continue;
            }

            if (fl_filename_isdir(join_path(dir, filename).c_str())) folder_list.push_back(filename);
            else file_list.push_back(filename);
            std::free(files[i]);
        }
        std::free(files);

        for (size_t i = 0; i < folder_list.size(); ++i) {
            file_browser_->add(folder_list[i].c_str());
            file_browser_->icon(file_browser_->size(), file_browser_->folder_icon_);
        }
        for (size_t i = 0; i < file_list.size(); ++i) {
            file_browser_->add(file_list[i].c_str());
            file_browser_->icon(file_browser_->size(), file_browser_->file_icon_);
        }
    }
}

void TextEditorApp::set_current_dir(const std::string& dir) {
    current_dir_ = dir;
    if (file_browser_) load_directory(current_dir_);
}

static bool is_parent_entry(const std::string& name) { return name == ".." || name == "../"; }

void TextEditorApp::new_file_in_sidebar() {
    const char* name = fl_input("New file name:", "untitled");
    if (!name || std::string(name).empty()) return;

    std::string path = get_absolute_path(join_path(current_dir_, name));
    std::ofstream outfile(path.c_str());
    outfile.close();

    load_directory(current_dir_);
    open_file(path);
}

void TextEditorApp::rename_selected_file() {
    int line = file_browser_->value();
    if (line <= 0) return;
    const char* text = file_browser_->text(line);
    if (!text) return;

    std::string selected = text;
    if (is_parent_entry(selected)) return;

    const char* new_name = fl_input("Rename to:", selected.c_str());
    if (!new_name || std::string(new_name).empty()) return;

    std::string old_path = get_absolute_path(join_path(current_dir_, selected));
    std::string new_path = get_absolute_path(join_path(current_dir_, new_name));
    const bool is_link = PathUtil::is_symlink(old_path);          // renaming a link leaves its target in place
    const std::string old_id = canonical_path(old_path);          // must be taken before the rename

    if (std::rename(old_path.c_str(), new_path.c_str()) != 0) {
        fl_alert("Error: Could not rename file.");
        return;
    }

    size_t last_slash = new_path.find_last_of("/\\");
    std::string new_fname = (last_slash == std::string::npos) ? new_path : new_path.substr(last_slash + 1);

    for (size_t i = 0; i < open_tabs_.size() && !is_link; ++i) {
        TabData* tab = open_tabs_[i];
        if (tab->filepath == old_id) {
            tab->filepath = canonical_path(new_path);
            tab->filename = new_fname;
            apply_language(tab);
            update_tab_label(tab);
        }
    }

    load_directory(current_dir_);
    update_status_bar();
    redraw();
}

void TextEditorApp::delete_selected_file() {
    int line = file_browser_->value();
    if (line <= 0) return;
    const char* text = file_browser_->text(line);
    if (!text) return;

    std::string selected = text;
    if (is_parent_entry(selected)) return;
    std::string target_path = get_absolute_path(join_path(current_dir_, selected));
    const bool is_link = PathUtil::is_symlink(target_path);
    const std::string target_id = canonical_path(target_path);

    if (fl_choice("Are you sure you want to delete %s?", "Cancel", "Delete", 0, selected.c_str()) == 1) {
        if (std::remove(target_path.c_str()) != 0) {
            fl_alert("Error: Could not delete %s.", selected.c_str());
            return;
        }
        for (size_t i = 0; i < open_tabs_.size() && !is_link; ++i) {
            if (open_tabs_[i]->filepath == target_id) {
                destroy_tab(open_tabs_[i]);
                break;
            }
        }
        load_directory(current_dir_);
        update_status_bar();
        redraw();
    }
}

void TextEditorApp::browser_cb(Fl_Widget* w, void* data) {
    TextEditorApp* self = static_cast<TextEditorApp*>(data);
    CustomFileBrowser* fb = static_cast<CustomFileBrowser*>(w);

    int line = fb->value();
    if (line <= 0) return;
    const char* text = fb->text(line);
    if (!text) return;

    std::string target_path = self->get_absolute_path(join_path(self->current_dir_, text));

    if (fl_filename_isdir(target_path.c_str())) {
        self->current_dir_ = target_path;
        self->load_directory(target_path);
        fb->take_focus();
    } else {
        const std::string id = self->canonical_path(target_path);
        for (size_t i = 0; i < self->open_tabs_.size(); ++i) {
            if (self->open_tabs_[i]->filepath == id) {
                self->tabs_->value(self->open_tabs_[i]->group);
                self->update_status_bar();
                fb->take_focus();
                return;
            }
        }
        self->open_file(target_path, true);
    }
}

void TextEditorApp::window_close_cb(Fl_Widget*, void* data) {
    static_cast<TextEditorApp*>(data)->try_quit();
}

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------

TextEditorApp::TextEditorApp(int width, int height, const char* title)
    : Fl_Double_Window(width, height, title), font_size_(DEFAULT_FONT_SIZE),
      find_dialog_(nullptr), replace_dialog_(nullptr), icon_font_resolved_(false),
      font_regular_(FL_COURIER), font_bold_(FL_COURIER_BOLD), font_italic_(FL_COURIER_ITALIC),
      font_bold_italic_(FL_COURIER_BOLD_ITALIC), icon_font_(-1), dark_theme_(true), switcher_(nullptr),
      fonts_listed_(false), checking_changes_(false) {

    // Everything Flitpad remembers is in one file: [settings] and [recent].
    session_.load(get_session_file_path());
    session_.import_legacy_settings(home_file(".flitpad_config"));
    const Settings& st = session_.settings;
    font_size_ = st.get_int("font_size", DEFAULT_FONT_SIZE, MIN_FONT_SIZE, MAX_FONT_SIZE);
    font_name_ = st.get("font_name", DEFAULT_FONT_NAME);
    icon_font_name_ = st.get("icon_font");
    dark_theme_ = st.get("theme", "dark") != "light";
    Theme::set_scheme(active_scheme());
    Theme::apply();
    const bool font_found = resolve_font(font_name_);
    rebuild_style_table();

    Fl_Color bg_color = Theme::window_bg();
    Fl_Color panel_color = Theme::panel_bg();
    Fl_Color text_color = Theme::text();

    color(bg_color);
    callback(window_close_cb, this);

    char cwd[1024];
    if (fl_getcwd(cwd, sizeof(cwd))) current_dir_ = cwd;
    else current_dir_ = ".";
    std::replace(current_dir_.begin(), current_dir_.end(), '\\', '/');

    file_browser_ = new CustomFileBrowser(10, 10, 220, height - 55, this, nullptr);
    file_browser_->color(panel_color);
    file_browser_->textcolor(text_color);
    file_browser_->type(FL_HOLD_BROWSER);
    load_directory(current_dir_);
    file_browser_->callback(browser_cb, this);

    CustomTabs* ctabs = new CustomTabs(240, 10, width - 250, height - 55);
    ctabs->on_middle_click = [this](Fl_Widget* group) { schedule_close(group); };
    ctabs->on_right_click = [this](Fl_Widget* group) { show_tab_menu(group); };
    tabs_ = ctabs;
    tabs_->box(FL_THIN_UP_BOX);
    tabs_->color(bg_color);
    tabs_->callback(tabs_cb, this);
    tabs_->end();

    status_bar_ = new Fl_Box(10, height - 35, width - 20, 25, " Ready | UTF-8");
    status_bar_->box(FL_FLAT_BOX);
    status_bar_->color(panel_color);
    status_bar_->labelfont(FL_HELVETICA);
    status_bar_->labelsize(12);
    status_bar_->labelcolor(text_color);
    status_bar_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    switcher_ = new TabSwitcher();   // last child, so it is drawn on top

    end();
    resizable(tabs_);

    Fl::add_timeout(0.5, poll_commands_cb, this);
    Fl::add_timeout(1.0, external_change_cb, this);
    if (!font_found) set_status_message("Font \"" + font_name_ + "\" not found, using " + DEFAULT_FONT_NAME);

    int icon_w = 32, icon_h = 32;
    uchar* icon_pixels = new uchar[icon_w * icon_h * 4];
    for (int y = 0; y < icon_h; ++y) {
        for (int x = 0; x < icon_w; ++x) {
            int idx = (y * icon_w + x) * 4;
            bool inside_card = (x >= 4 && x <= 27 && y >= 2 && y <= 29);
            if (inside_card) {
                icon_pixels[idx] = 40;
                icon_pixels[idx+1] = 44;
                icon_pixels[idx+2] = 52;
                icon_pixels[idx+3] = 255;

                if (y >= 5 && y <= 7 && x >= 8 && x <= 23) {
                    icon_pixels[idx] = 97;   icon_pixels[idx+1] = 175; icon_pixels[idx+2] = 239;
                }
                else if (y >= 11 && y <= 12 && x >= 8 && x <= 20) {
                    icon_pixels[idx] = 198;  icon_pixels[idx+1] = 120; icon_pixels[idx+2] = 221;
                }
                else if (y >= 15 && y <= 16 && x >= 8 && x <= 24) {
                    icon_pixels[idx] = 152;  icon_pixels[idx+1] = 195; icon_pixels[idx+2] = 121;
                }
                else if (y >= 19 && y <= 20 && x >= 8 && x <= 16) {
                    icon_pixels[idx] = 209;  icon_pixels[idx+1] = 154; icon_pixels[idx+2] = 102;
                }
            } else {
                icon_pixels[idx] = 0;
                icon_pixels[idx+1] = 0;
                icon_pixels[idx+2] = 0;
                icon_pixels[idx+3] = 0;
            }
        }
    }
    Fl_RGB_Image* app_icon = new Fl_RGB_Image(icon_pixels, icon_w, icon_h, 4);
    icon(app_icon);
}

TextEditorApp::~TextEditorApp() {
    Fl::remove_timeout(poll_commands_cb, this);
    Fl::remove_timeout(external_change_cb, this);
    for (size_t i = 0; i < open_tabs_.size(); ++i) Fl::remove_timeout(restyle_timeout_cb, open_tabs_[i]);
    save_session();
    std::remove(get_lock_file_path().c_str());
    std::remove(get_command_file_path().c_str());
}

// ---------------------------------------------------------------------------
// session
// ---------------------------------------------------------------------------

void TextEditorApp::save_session() {
    Settings& st = session_.settings;
    st.set_int("font_size", font_size_);
    st.set("font_name", font_name_);
    st.set("theme", dark_theme_ ? "dark" : "light");
    if (!st.has("dark_scheme")) st.set("dark_scheme", ColorSchemes::fallback(true)->id);     // visible in the file, easy to edit
    if (!st.has("light_scheme")) st.set("light_scheme", ColorSchemes::fallback(false)->id);
    session_.set_window(x(), y(), w(), h());

    TabData* current = get_current_tab();
    st.set("active_file", (current && !current->is_untitled) ? current->filepath : std::string());
    session_.recent_files.clear();
    for (size_t i = 0; i < open_tabs_.size(); ++i)
        if (!open_tabs_[i]->is_untitled && !open_tabs_[i]->filepath.empty()) session_.recent_files.push_back(open_tabs_[i]->filepath);

    if (session_.save(get_session_file_path())) std::remove(home_file(".flitpad_config").c_str());   // folded into [settings]
}

void TextEditorApp::load_session() {
    int wx, wy, ww, wh;
    if (session_.get_window(wx, wy, ww, wh)) resize(wx, wy, ww, wh);

    std::string active_path = session_.settings.get("active_file");
    if (!active_path.empty()) active_path = canonical_path(active_path);
    std::vector<std::string> restored;

    for (size_t i = 0; i < session_.recent_files.size(); ++i) {
        const std::string id = canonical_path(session_.recent_files[i]);
        std::ifstream check(id.c_str(), std::ios::binary);
        if (!check.is_open()) continue;     // deleted or moved since last time
        check.close();

        open_file(id, false);
        for (size_t k = 0; k < open_tabs_.size(); ++k) {
            if (open_tabs_[k]->filepath == id) { restored.push_back(id); break; }
        }
    }

    std::string target;
    for (size_t i = 0; i < restored.size(); ++i) {
        if (restored[i] == active_path) { target = restored[i]; break; }
    }
    if (target.empty() && !restored.empty()) target = restored.front();

    if (!target.empty()) {
        for (size_t i = 0; i < open_tabs_.size(); ++i) {
            if (open_tabs_[i]->filepath == target) {
                tabs_->value(open_tabs_[i]->group);
                open_tabs_[i]->editor->take_focus();
                break;
            }
        }
        size_t slash = target.find_last_of("/\\");
        std::string dir;
        if (slash == std::string::npos) dir = ".";
        else if (slash == 0) dir = target.substr(0, 1);
        else dir = target.substr(0, slash);
        set_current_dir(dir);
    }

    update_status_bar();
    redraw();
}

// ---------------------------------------------------------------------------
// tabs
// ---------------------------------------------------------------------------

TabData* TextEditorApp::get_current_tab() {
    Fl_Widget* current = tabs_->value();
    if (!current) return nullptr;
    for (size_t i = 0; i < open_tabs_.size(); ++i)
        if (open_tabs_[i]->group == current) return open_tabs_[i];
    return nullptr;
}

TabData* TextEditorApp::tab_for_editor(Fl_Text_Editor* editor) {
    for (size_t i = 0; i < open_tabs_.size(); ++i)
        if (open_tabs_[i]->editor == editor) return open_tabs_[i];
    return nullptr;
}

TabData* TextEditorApp::tab_for_group(Fl_Widget* group) {
    for (size_t i = 0; i < open_tabs_.size(); ++i)
        if (open_tabs_[i]->group == group) return open_tabs_[i];
    return nullptr;
}

void TextEditorApp::configure_editor(TabData* tab) {
    bool prose = (tab->language == "Markdown" || tab->language == "Plain Text");
    tab->editor->wrap_mode(prose ? Fl_Text_Display::WRAP_AT_BOUNDS : Fl_Text_Display::WRAP_NONE, 0);
}

// Re-detect the language after the file name changed (rename / save as).
void TextEditorApp::apply_language(TabData* tab) {
    int len = std::min(tab->buffer->length(), 512);
    tab->language = Highlighter::detect_language(tab->filename, buffer_range(tab->buffer, 0, len));
    configure_editor(tab);
    restyle_tab(tab);
}

TabData* TextEditorApp::create_tab(const std::string& path, const std::string& name, const std::string& content,
                                   bool untitled, bool crlf) {
    Fl_Color editor_bg = Theme::editor_bg();
    Fl_Color editor_text = Theme::text_strong();
    Fl_Color cursor_color = Theme::cursor();
    Fl_Color select_color = Theme::selection();

    tabs_->begin();
    Fl_Group* grp = new Fl_Group(240, 35, w() - 250, h() - 80);
    grp->copy_label(name.c_str());
    grp->color(Theme::window_bg());

    Fl_Text_Editor* editor = new CodeEditor(245, 40, w() - 260, h() - 90);
    editor->linenumber_width(static_cast<int>(font_size_ * 3.3));
    editor->linenumber_size(std::max(8, font_size_ - 3));
    editor->linenumber_fgcolor(Theme::line_number_fg());
    editor->linenumber_bgcolor(Theme::line_number_bg());

    Fl_Text_Buffer* textbuf = new Fl_Text_Buffer();
    Fl_Text_Buffer* stylebuf = new Fl_Text_Buffer();

    editor->buffer(textbuf);
    textbuf->tab_distance(TAB_WIDTH);

    editor->color(editor_bg);
    editor->textcolor(editor_text);
    editor->textfont(font_regular_);
    editor->textsize(font_size_);
    editor->cursor_color(cursor_color);
    editor->cursor_style(Fl_Text_Editor::NORMAL_CURSOR);
    editor->show_cursor();
    editor->selection_color(select_color);

    static_cast<CodeEditor*>(editor)->on_drop = [this](const std::string& dropped) { return open_dropped(dropped); };
    editor->add_key_binding(FL_Enter, FL_TEXT_EDITOR_ANY_STATE, kf_smart_enter);
    editor->add_key_binding(FL_KP_Enter, FL_TEXT_EDITOR_ANY_STATE, kf_smart_enter);
    editor->add_key_binding(FL_Tab, 0, kf_tab);
    editor->add_key_binding(FL_Tab, FL_SHIFT, kf_shift_tab);

    textbuf->text(content.c_str());
    stylebuf->text(std::string(static_cast<size_t>(textbuf->length()), 'A').c_str());

    TabData* tab = new TabData();
    tab->group = grp;
    tab->editor = editor;
    tab->buffer = textbuf;
    tab->style_buffer = stylebuf;
    tab->filepath = path;
    tab->filename = name;
    tab->is_modified = untitled;
    tab->is_untitled = untitled;
    tab->crlf = crlf;
    tab->tabs = tabs_;
    tab->language = Highlighter::detect_language(name, content.substr(0, 512));

    editor->highlight_data(stylebuf, style_table_, STYLE_COUNT, 'A', 0, 0);
    configure_editor(tab);
    restyle_tab(tab);

    textbuf->add_modify_callback(style_update_cb, tab);
    textbuf->add_modify_callback(buffer_modified_cb, tab);

    grp->end();
    tabs_->end();

    open_tabs_.push_back(tab);
    tabs_->value(grp);
    update_tab_label(tab);
    return tab;
}

void TextEditorApp::destroy_tab(TabData* tab) {
    size_t idx = 0;
    for (; idx < open_tabs_.size(); ++idx)
        if (open_tabs_[idx] == tab) break;
    if (idx == open_tabs_.size()) return;

    Fl::remove_timeout(restyle_timeout_cb, tab);
    tabs_->remove(tab->group);
    delete tab->group;  // also destroys the editor; the buffers must outlive it
    delete tab->buffer;
    delete tab->style_buffer;
    delete tab;
    open_tabs_.erase(open_tabs_.begin() + idx);

    if (!open_tabs_.empty()) {
        size_t next = std::min(idx, open_tabs_.size() - 1);
        tabs_->value(open_tabs_[next]->group);
        open_tabs_[next]->editor->take_focus();
    }
    update_status_bar();
    redraw();
}

bool TextEditorApp::close_tab(TabData* tab) {
    if (!tab) return true;
    if (tab->is_modified) {
        tabs_->value(tab->group);
        int choice = fl_choice("File %s has unsaved changes.", "Cancel", "Save", "Close Without Saving", tab->filename.c_str());
        if (choice == 0) return false;
        if (choice == 1 && !save_tab(tab)) return false;
    }
    destroy_tab(tab);
    return true;
}

void TextEditorApp::close_current_tab() { close_tab(get_current_tab()); }

struct CloseRequest {
    TextEditorApp* app;
    Fl_Widget* group;
};

// Middle-click arrives from inside Fl_Tabs::handle(); closing the tab there would
// delete the widget being handled, so do it from the event loop instead.
void TextEditorApp::schedule_close(Fl_Widget* group) {
    CloseRequest* req = new CloseRequest();
    req->app = this;
    req->group = group;
    Fl::add_timeout(0.0, deferred_close_cb, req);
}

void TextEditorApp::deferred_close_cb(void* data) {
    CloseRequest* req = static_cast<CloseRequest*>(data);
    for (size_t i = 0; i < req->app->open_tabs_.size(); ++i) {
        if (req->app->open_tabs_[i]->group == req->group) {
            req->app->close_tab(req->app->open_tabs_[i]);
            break;
        }
    }
    delete req;
}

void TextEditorApp::try_quit() {
    for (size_t i = 0; i < open_tabs_.size(); ++i) {
        TabData* tab = open_tabs_[i];
        if (tab->is_modified) {
            tabs_->value(tab->group);
            int choice = fl_choice("File %s has unsaved changes. Save before exiting?", "Cancel", "Save", "Don't Save", tab->filename.c_str());
            if (choice == 0) return;
            if (choice == 1 && !save_tab(tab)) return;
        }
    }
    hide();
}

// ---------------------------------------------------------------------------
// open / save
// ---------------------------------------------------------------------------

// A path is "in use" if another open tab has it or a file of that name exists.
bool TextEditorApp::path_in_use(const std::string& path, const TabData* ignore) {
    const std::string id = canonical_path(path);
    for (size_t i = 0; i < open_tabs_.size(); ++i)
        if (open_tabs_[i] != ignore && open_tabs_[i]->filepath == id) return true;
    std::FILE* f = std::fopen(id.c_str(), "rb");
    if (f) { std::fclose(f); return true; }
    return false;
}

void TextEditorApp::new_file() {
    const std::string name = PathUtil::unique_name("untitled", [this](const std::string& n) {
        return path_in_use(join_path(current_dir_, n), nullptr);
    });
    TabData* tab = create_tab(canonical_path(join_path(current_dir_, name)), name, "", true, false);
    tab->editor->take_focus();
    update_status_bar();
    redraw();
}

void TextEditorApp::open_file(const std::string& path) { open_file(path, true); }

void TextEditorApp::open_file(const std::string& path, bool focus_editor) {
    const std::string shown_path = get_absolute_path(path);   // as spelled; used for the tab title
    std::string abs_path = canonical_path(path);              // identity; used for dedupe and saving

    for (size_t i = 0; i < open_tabs_.size(); ++i) {
        if (open_tabs_[i]->filepath == abs_path) {
            tabs_->value(open_tabs_[i]->group);
            if (focus_editor) open_tabs_[i]->editor->take_focus();
            update_status_bar();
            return;
        }
    }

    std::string content, error;
    bool crlf = false;
    if (!TextFile::read(abs_path, content, crlf, error)) {
        fl_alert("%s", error.c_str());
        return;
    }

    size_t last_slash = shown_path.find_last_of("/\\");
    std::string fname = (last_slash == std::string::npos) ? shown_path : shown_path.substr(last_slash + 1);

    TabData* tab = create_tab(abs_path, fname, content, false, crlf);
    tab->mtime = TextFile::modified_time(abs_path);
    if (focus_editor) tab->editor->take_focus();
    update_status_bar();
    redraw();
}

void TextEditorApp::open_file_dialog() {
    Fl_Native_File_Chooser fc;
    fc.title("Open File");
    fc.type(Fl_Native_File_Chooser::BROWSE_MULTI_FILE);
    fc.directory(current_dir_.c_str());
    if (fc.show() != 0) return;
    for (int i = 0; i < fc.count(); ++i) open_file(fc.filename(i));
}

bool TextEditorApp::write_tab_to_disk(TabData* tab) {
    std::string error;
    if (!TextFile::write(tab->filepath, buffer_range(tab->buffer, 0, tab->buffer->length()), tab->crlf, error)) {
        fl_alert("%s", error.c_str());
        return false;
    }
    tab->mtime = TextFile::modified_time(tab->filepath);   // our own write must not look like an outside change
    tab->is_modified = false;
    update_tab_label(tab);
    if (file_browser_) load_directory(current_dir_);
    return true;
}

// A new file is saved under the name it already has, in the directory it was
// created in; no dialog (Save As is still there for choosing). Only if that name
// was taken in the meantime does it get the next free one, so nothing is overwritten.
bool TextEditorApp::save_tab(TabData* tab) {
    if (!tab) return true;
    if (!tab->is_untitled) return write_tab_to_disk(tab);

    const size_t slash = tab->filepath.find_last_of("/\\");
    const std::string dir = (slash == std::string::npos) ? current_dir_ : tab->filepath.substr(0, slash == 0 ? 1 : slash);
    const std::string wanted = tab->filename;
    const std::string name = PathUtil::unique_name(wanted, [&](const std::string& n) {
        return path_in_use(join_path(dir, n), tab);
    });

    const std::string old_path = tab->filepath, old_name = tab->filename;
    tab->filename = name;
    tab->filepath = canonical_path(join_path(dir, name));
    tab->is_untitled = false;
    if (!write_tab_to_disk(tab)) {
        tab->filename = old_name;
        tab->filepath = old_path;
        tab->is_untitled = true;
        update_tab_label(tab);
        return false;
    }
    apply_language(tab);
    update_status_bar();
    if (name != wanted) set_status_message("\"" + wanted + "\" already exists, saved as \"" + name + "\"");
    return true;
}

bool TextEditorApp::save_tab_as(TabData* tab) {
    if (!tab) return true;
    Fl_Native_File_Chooser fc;
    fc.title("Save As");
    fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM | Fl_Native_File_Chooser::NEW_FOLDER);
    fc.directory(current_dir_.c_str());
    fc.preset_file(tab->filename.c_str());
    if (fc.show() != 0) return false;

    std::string path = canonical_path(fc.filename());
    for (size_t i = 0; i < open_tabs_.size(); ++i) {
        if (open_tabs_[i] != tab && open_tabs_[i]->filepath == path) {
            fl_alert("%s is already open in another tab.", path.c_str());
            return false;
        }
    }

    std::string old_path = tab->filepath;
    bool old_untitled = tab->is_untitled;
    tab->filepath = path;
    size_t slash = path.find_last_of("/\\");
    tab->filename = (slash == std::string::npos) ? path : path.substr(slash + 1);
    tab->is_untitled = false;
    if (!write_tab_to_disk(tab)) {
        tab->filepath = old_path;
        tab->is_untitled = old_untitled;
        return false;
    }
    apply_language(tab);
    if (slash != std::string::npos && slash > 0 && path.substr(0, slash) != canonical_path(current_dir_))
        set_current_dir(path.substr(0, slash));
    update_status_bar();
    return true;
}

void TextEditorApp::save_current_file() {
    TabData* tab = get_current_tab();
    if (tab) save_tab(tab);
}

void TextEditorApp::save_current_file_as() {
    TabData* tab = get_current_tab();
    if (tab) save_tab_as(tab);
}

// ---------------------------------------------------------------------------
// find / replace
// ---------------------------------------------------------------------------

static std::string selected_single_line(TabData* tab) {
    if (!tab) return std::string();
    int s = 0, e = 0;
    if (!tab->buffer->selection_position(&s, &e) || e <= s || e - s > 200) return std::string();
    std::string text = buffer_range(tab->buffer, s, e);
    return text.find('\n') == std::string::npos ? text : std::string();
}

void TextEditorApp::show_find_dialog() {
    if (!find_dialog_) find_dialog_ = new FindReplaceDialog(this, false);
    std::string sel = selected_single_line(get_current_tab());
    if (!sel.empty()) find_dialog_->set_query(sel);
    find_dialog_->show();
    NativeTheme::apply(find_dialog_);
    find_dialog_->focus_query();
}

void TextEditorApp::show_replace_dialog() {
    if (!replace_dialog_) replace_dialog_ = new FindReplaceDialog(this, true);
    std::string sel = selected_single_line(get_current_tab());
    if (!sel.empty()) replace_dialog_->set_query(sel);
    replace_dialog_->show();
    NativeTheme::apply(replace_dialog_);
    replace_dialog_->focus_query();
}

// F3 / Shift+F3: repeat the last search from the dialog's query
void TextEditorApp::find_again(bool backwards) {
    FindReplaceDialog* dlg = replace_dialog_ && replace_dialog_->visible() ? replace_dialog_ : find_dialog_;
    if (!dlg) dlg = replace_dialog_;
    if (!dlg) { show_find_dialog(); return; }
    if (backwards) dlg->do_find_prev();
    else dlg->do_find_next();
}

void TextEditorApp::find_text(const std::string& query, bool match_case) {
    TabData* tab = get_current_tab();
    if (!tab) return;
    Fl_Text_Buffer* buf = tab->buffer;
    Fl_Text_Editor* editor = tab->editor;

    int start_pos = editor->insert_position();
    int found_pos = 0;
    bool found = buf->search_forward(start_pos, query.c_str(), &found_pos, match_case ? 1 : 0);
    bool wrapped = false;
    if (!found && start_pos > 0) {
        found = buf->search_forward(0, query.c_str(), &found_pos, match_case ? 1 : 0);
        wrapped = found;
    }
    if (found) {
        editor->insert_position(found_pos + static_cast<int>(query.length()));
        buf->select(found_pos, found_pos + static_cast<int>(query.length()));
        editor->show_insert_position();
        if (wrapped) set_status_message("Search wrapped to the top");
    } else {
        set_status_message("Not found: " + query);
    }
}

void TextEditorApp::find_previous_text(const std::string& query, bool match_case) {
    TabData* tab = get_current_tab();
    if (!tab) return;
    Fl_Text_Buffer* buf = tab->buffer;
    Fl_Text_Editor* editor = tab->editor;

    int start_pos = editor->insert_position();
    int sel_s, sel_e;
    if (buf->selection_position(&sel_s, &sel_e)) start_pos = sel_s;
    if (start_pos > 0) --start_pos;

    int found_pos = 0;
    bool found = buf->search_backward(start_pos, query.c_str(), &found_pos, match_case ? 1 : 0);
    bool wrapped = false;
    if (!found) {
        found = buf->search_backward(buf->length(), query.c_str(), &found_pos, match_case ? 1 : 0);
        wrapped = found;
    }
    if (found) {
        editor->insert_position(found_pos);
        buf->select(found_pos, found_pos + static_cast<int>(query.length()));
        editor->show_insert_position();
        if (wrapped) set_status_message("Search wrapped to the bottom");
    } else {
        set_status_message("Not found: " + query);
    }
}

static bool equals_nocase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
}

void TextEditorApp::replace_text(const std::string& query, const std::string& replacement, bool match_case) {
    TabData* tab = get_current_tab();
    if (!tab) return;
    Fl_Text_Buffer* buf = tab->buffer;

    int sel_start = 0, sel_end = 0;
    if (buf->selection_position(&sel_start, &sel_end)) {
        std::string selected = buffer_range(buf, sel_start, sel_end);
        bool matches = match_case ? (selected == query) : equals_nocase(selected, query);
        if (matches) {
            buf->replace(sel_start, sel_end, replacement.c_str());
            tab->editor->insert_position(sel_start + static_cast<int>(replacement.length()));
        }
    }
    find_text(query, match_case);
}

// Done on a std::string and applied with a single buffer change. Replacing match
// by match made every replacement a separate edit of a potentially huge buffer.
void TextEditorApp::replace_all_text(const std::string& query, const std::string& replacement, bool match_case) {
    TabData* tab = get_current_tab();
    if (!tab || query.empty()) return;

    std::string text = buffer_range(tab->buffer, 0, tab->buffer->length());
    std::string hay = text, needle = query;
    if (!match_case) {
        for (size_t i = 0; i < hay.size(); ++i) hay[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(hay[i])));
        for (size_t i = 0; i < needle.size(); ++i) needle[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(needle[i])));
    }

    std::string result;
    size_t pos = 0, hit;
    int count = 0;
    while ((hit = hay.find(needle, pos)) != std::string::npos) {
        result.append(text, pos, hit - pos);
        result += replacement;
        pos = hit + needle.size();
        ++count;
    }
    if (count == 0) {
        set_status_message("Not found: " + query);
        return;
    }
    result.append(text, pos, std::string::npos);

    int cursor = tab->editor->insert_position();
    tab->buffer->text(result.c_str());
    tab->editor->insert_position(std::min(cursor, tab->buffer->length()));
    tab->editor->show_insert_position();
    set_status_message("Replaced " + std::to_string(count) + " occurrence(s)");
}

// ---------------------------------------------------------------------------
// editing commands
// ---------------------------------------------------------------------------

void TextEditorApp::goto_line() {
    TabData* tab = get_current_tab();
    if (!tab) return;
    const char* answer = fl_input("Go to line:", "");
    if (!answer) return;
    int line = std::atoi(answer);
    if (line < 1) return;
    int pos = tab->buffer->skip_lines(0, line - 1);
    tab->editor->insert_position(pos);
    tab->editor->show_insert_position();
    tab->editor->take_focus();
    update_status_bar();
}

// Whole-line range covered by the selection (or by the caret's line).
static void selection_lines(TabData* tab, int& line_a, int& line_b, bool& has_sel) {
    Fl_Text_Buffer* buf = tab->buffer;
    int s = 0, e = 0;
    has_sel = buf->selection_position(&s, &e) != 0 && e > s;
    if (!has_sel) s = e = tab->editor->insert_position();
    else if (buf->byte_at(e - 1) == '\n') --e;  // a selection that ends at a line start doesn't include that line
    line_a = buf->line_start(s);
    line_b = buf->line_end(e);
}

void TextEditorApp::toggle_comment() {
    TabData* tab = get_current_tab();
    if (!tab) return;
    int a, b;
    bool has_sel;
    selection_lines(tab, a, b, has_sel);

    std::string block = buffer_range(tab->buffer, a, b), out;
    if (!EditHelpers::toggle_line_comments(tab->language, block, out)) {
        set_status_message("No line-comment syntax for " + tab->language);
        return;
    }
    int cursor = tab->editor->insert_position();
    tab->buffer->replace(a, b, out.c_str());
    if (has_sel) {
        tab->buffer->select(a, a + static_cast<int>(out.size()));
        tab->editor->insert_position(a + static_cast<int>(out.size()));
    } else {
        int delta = static_cast<int>(out.size()) - static_cast<int>(block.size());
        tab->editor->insert_position(std::max(a, cursor + delta));
    }
    tab->editor->show_insert_position();
}

typedef EditHelpers::RegionEdit (*LineCommand)(const std::string&, int, int, int, bool);

static void apply_region_edit(TabData* tab, const EditHelpers::RegionEdit& r) {
    if (!r.valid) return;
    tab->buffer->unselect();
    tab->buffer->replace(r.start, r.end, r.text.c_str());
    if (r.sel_start >= 0) tab->buffer->select(r.sel_start, r.sel_end);
    tab->editor->insert_position(r.cursor);
    tab->editor->show_insert_position();
}

static void run_line_command(TabData* tab, LineCommand cmd) {
    Fl_Text_Buffer* buf = tab->buffer;
    int s = 0, e = 0;
    bool has_sel = buf->selection_position(&s, &e) != 0 && e > s;
    if (!has_sel) s = e = tab->editor->insert_position();
    apply_region_edit(tab, cmd(buffer_range(buf, 0, buf->length()), s, e, tab->editor->insert_position(), has_sel));
}

static EditHelpers::RegionEdit move_up_cmd(const std::string& d, int s, int e, int c, bool h) { return EditHelpers::move_lines(d, s, e, c, h, true); }
static EditHelpers::RegionEdit move_down_cmd(const std::string& d, int s, int e, int c, bool h) { return EditHelpers::move_lines(d, s, e, c, h, false); }

void TextEditorApp::run_menu_command(int command) {
    TabData* tab = get_current_tab();
    if (!tab) return;
    switch (command) {
        case CMD_UNDO:           Fl_Text_Editor::kf_undo(0, tab->editor); break;
        case CMD_CUT:            Fl_Text_Editor::kf_cut(0, tab->editor); break;
        case CMD_COPY:           Fl_Text_Editor::kf_copy(0, tab->editor); break;
        case CMD_PASTE:          Fl_Text_Editor::kf_paste(0, tab->editor); break;
        case CMD_SELECT_ALL:     Fl_Text_Editor::kf_select_all(0, tab->editor); break;
        case CMD_COMMENT:        toggle_comment(); break;
        case CMD_DUPLICATE_LINE: run_line_command(tab, EditHelpers::duplicate_lines); break;
        case CMD_DELETE_LINE:    run_line_command(tab, EditHelpers::delete_lines); break;
        case CMD_LINE_UP:        run_line_command(tab, move_up_cmd); break;
        case CMD_LINE_DOWN:      run_line_command(tab, move_down_cmd); break;
        case CMD_THEME:          choose_theme(); break;
        default: break;
    }
    tab->editor->take_focus();
    update_status_bar();
}

// Right-click menu: only what an editor really needs. Everything else has a shortcut.
void TextEditorApp::show_context_menu() {
    TabData* tab = get_current_tab();
    if (!tab) return;

    const int need_sel = tab->buffer->selected() ? 0 : FL_MENU_INACTIVE;
    const int need_comment = Highlighter::line_comment_prefix(tab->language).empty() ? FL_MENU_INACTIVE : 0;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // FLTK menu tables list only the first few fields
#endif
#define CMD(c) reinterpret_cast<void*>(static_cast<intptr_t>(c))
    Fl_Menu_Item items[] = {
        { "Undo",           FL_CTRL + 'z', 0, CMD(CMD_UNDO),       FL_MENU_DIVIDER },
        { "Cut",            FL_CTRL + 'x', 0, CMD(CMD_CUT),        need_sel },
        { "Copy",           FL_CTRL + 'c', 0, CMD(CMD_COPY),       need_sel },
        { "Paste",          FL_CTRL + 'v', 0, CMD(CMD_PASTE),      0 },
        { "Select All",     FL_CTRL + 'a', 0, CMD(CMD_SELECT_ALL), FL_MENU_DIVIDER },
        { "Toggle Comment", FL_CTRL + '/', 0, CMD(CMD_COMMENT),    need_comment | FL_MENU_DIVIDER },
        { "Theme...",       0,             0, CMD(CMD_THEME),      0 },
        { 0 }
    };
#undef CMD
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y(), 0, 0, 0);
    if (picked && picked->user_data()) run_menu_command(static_cast<int>(reinterpret_cast<intptr_t>(picked->user_data())));
}

static void copy_to_clipboard(const std::string& text) {
    Fl::copy(text.c_str(), static_cast<int>(text.size()), 1);
}

// Sidebar right-click menu. CustomFileBrowser has already selected the item under
// the mouse (without opening it).
void TextEditorApp::show_sidebar_menu() {
    const int line = file_browser_->value();
    std::string selected;
    if (line > 0 && file_browser_->text(line)) selected = file_browser_->text(line);
    const bool has_item = !selected.empty();
    const bool is_parent = (selected == ".." || selected == "../");
    const int need_real = (has_item && !is_parent) ? 0 : FL_MENU_INACTIVE;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#define CMD(c) reinterpret_cast<void*>(static_cast<intptr_t>(c))
    Fl_Menu_Item items[] = {
        { "New File...", 0,         0, CMD(SB_NEW_FILE),  FL_MENU_DIVIDER },
        { "Rename",      FL_F + 2,  0, CMD(SB_RENAME),    need_real },
        { "Delete",      FL_Delete, 0, CMD(SB_DELETE),    need_real | FL_MENU_DIVIDER },
        { "Copy Path",   0,         0, CMD(SB_COPY_PATH), need_real },
        { "Refresh",     0,         0, CMD(SB_REFRESH),   0 },
        { 0 }
    };
#undef CMD
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y(), 0, 0, 0);
    if (!picked || !picked->user_data()) return;
    switch (static_cast<int>(reinterpret_cast<intptr_t>(picked->user_data()))) {
        case SB_NEW_FILE:  new_file_in_sidebar(); break;
        case SB_RENAME:    rename_selected_file(); break;
        case SB_DELETE:    delete_selected_file(); break;
        case SB_COPY_PATH: copy_to_clipboard(get_absolute_path(join_path(current_dir_, selected))); break;
        case SB_REFRESH:   load_directory(current_dir_); break;
        default: break;
    }
}

struct TabAction {
    TextEditorApp* app;
    Fl_Widget* group;
    int command;
};

// Tab right-click menu. The chosen action runs from the event loop, not from inside
// Fl_Tabs::handle(), because most of them delete tab widgets.
void TextEditorApp::show_tab_menu(Fl_Widget* group) {
    TabData* tab = tab_for_group(group);
    if (!tab) return;
    const int need_others = open_tabs_.size() > 1 ? 0 : FL_MENU_INACTIVE;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#define CMD(c) reinterpret_cast<void*>(static_cast<intptr_t>(c))
    Fl_Menu_Item items[] = {
        { "Close",           FL_CTRL + 'w', 0, CMD(TAB_CLOSE),        0 },
        { "Close Others",    0,             0, CMD(TAB_CLOSE_OTHERS), need_others },
        { "Close All",       0,             0, CMD(TAB_CLOSE_ALL),    FL_MENU_DIVIDER },
        { "Copy Full Path",  0,             0, CMD(TAB_COPY_PATH),    0 },
        { 0 }
    };
#undef CMD
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y(), 0, 0, 0);
    if (!picked || !picked->user_data()) return;
    TabAction* action = new TabAction();
    action->app = this;
    action->group = group;
    action->command = static_cast<int>(reinterpret_cast<intptr_t>(picked->user_data()));
    Fl::add_timeout(0.0, deferred_tab_action_cb, action);
}

void TextEditorApp::deferred_tab_action_cb(void* data) {
    TabAction* action = static_cast<TabAction*>(data);
    TabData* tab = action->app->tab_for_group(action->group);   // may be gone by now
    if (tab) action->app->run_tab_command(tab, action->command);
    delete action;
}

void TextEditorApp::run_tab_command(TabData* tab, int command) {
    switch (command) {
        case TAB_CLOSE:
            close_tab(tab);
            break;
        case TAB_CLOSE_OTHERS:
        case TAB_CLOSE_ALL: {
            const std::vector<TabData*> snapshot = open_tabs_;   // closing edits open_tabs_
            for (size_t i = 0; i < snapshot.size(); ++i) {
                if (command == TAB_CLOSE_OTHERS && snapshot[i] == tab) continue;
                if (!close_tab(snapshot[i])) break;              // Cancel on a prompt stops the rest
            }
            break;
        }
        case TAB_COPY_PATH: copy_to_clipboard(tab->filepath); break;
        default: break;
    }
    update_status_bar();
}

// ---------------------------------------------------------------------------
// theme, fonts
// ---------------------------------------------------------------------------

// The scheme for the current mode. dark_scheme= / light_scheme= in the session file
// choose which one; an unknown (or wrong-mode) name falls back to the default.
const ColorSchemes::Scheme* TextEditorApp::active_scheme() const {
    const ColorSchemes::Scheme* scheme =
        ColorSchemes::find(session_.settings.get(dark_theme_ ? "dark_scheme" : "light_scheme"));
    if (!scheme || scheme->dark != dark_theme_) scheme = ColorSchemes::fallback(dark_theme_);
    return scheme;
}

// Fills the style table from the active scheme and the current fonts. The table is
// shared by every editor, so changing it in place re-colours all of them.
void TextEditorApp::rebuild_style_table() {
    const std::vector<ColorSchemes::StyleSpec> specs = ColorSchemes::style_specs(Theme::scheme());
    for (int i = 0; i < Highlighter::STYLE_COUNT; ++i) {
        const ColorSchemes::StyleSpec& spec = specs[static_cast<size_t>(i)];
        Fl_Font font = font_regular_;
        if (spec.bold && spec.italic) font = font_bold_italic_;
        else if (spec.bold) font = font_bold_;
        else if (spec.italic) font = font_italic_;

        Fl_Text_Editor::Style_Table_Entry& entry = style_table_[i];
        entry.color = Theme::rgb(spec.rgb);
        entry.font = font;
        entry.size = font_size_;
        entry.attr = 0;

        Fl_Text_Editor::Style_Table_Entry& twin = style_table_[i + Highlighter::ICON_STYLE_OFFSET];
        twin = entry;
        twin.font = (icon_font_ >= 0) ? icon_font_ : font_regular_;   // icons: see resolve_icon_font()
    }
}

void TextEditorApp::apply_theme() {
    Theme::set_scheme(active_scheme());
    Theme::apply();
    rebuild_style_table();

    color(Theme::window_bg());
    file_browser_->color(Theme::panel_bg());
    file_browser_->textcolor(Theme::text());
    tabs_->color(Theme::window_bg());
    status_bar_->color(Theme::panel_bg());
    status_bar_->labelcolor(Theme::text());
    for (size_t i = 0; i < open_tabs_.size(); ++i) {
        TabData* tab = open_tabs_[i];
        tab->group->color(Theme::window_bg());
        Fl_Text_Editor* e = tab->editor;
        e->color(Theme::editor_bg());
        e->textcolor(Theme::text_strong());
        e->cursor_color(Theme::cursor());
        e->selection_color(Theme::selection());
        e->linenumber_fgcolor(Theme::line_number_fg());
        e->linenumber_bgcolor(Theme::line_number_bg());
        e->redraw();
    }
    if (find_dialog_) find_dialog_->apply_theme();
    if (replace_dialog_) replace_dialog_->apply_theme();
    redraw();
}

void TextEditorApp::set_scheme(const ColorSchemes::Scheme* scheme) {
    if (!scheme) return;
    dark_theme_ = scheme->dark;
    session_.settings.set("theme", dark_theme_ ? "dark" : "light");
    session_.settings.set(dark_theme_ ? "dark_scheme" : "light_scheme", scheme->id);
    apply_theme();
}

// "Theme..." dialog: previews each scheme as it is selected; Cancel puts back exactly
// what was set before (including the other mode's remembered scheme).
void TextEditorApp::choose_theme() {
    Settings& st = session_.settings;
    const bool was_dark = dark_theme_;
    const std::string was_theme = st.get("theme", "dark");
    const std::string was_dark_scheme = st.get("dark_scheme", ColorSchemes::fallback(true)->id);
    const std::string was_light_scheme = st.get("light_scheme", ColorSchemes::fallback(false)->id);

    const ColorSchemes::Scheme* picked = SchemeDialog::run(
        &Theme::scheme(), [this](const ColorSchemes::Scheme* s) { set_scheme(s); }, x() + w() / 2, y() + h() / 2);
    if (picked) {
        set_scheme(picked);
        set_status_message(std::string("Theme: ") + picked->name);
        return;
    }
    dark_theme_ = was_dark;
    st.set("theme", was_theme);
    st.set("dark_scheme", was_dark_scheme);
    st.set("light_scheme", was_light_scheme);
    apply_theme();
}

// Lists every installed font face once (FLTK's list is slow-ish, so only on demand:
// for a font_name= other than the default, and for the icon font).
void TextEditorApp::ensure_font_list() {
    if (fonts_listed_) return;
    fonts_listed_ = true;
    const int count = Fl::set_fonts("*");
    for (int i = 0; i < count; ++i) {
        int attr = 0;
        const char* name = Fl::get_font_name(static_cast<Fl_Font>(i), &attr);
        if (!name) continue;
        FontFace face;
        face.name = name;
        face.attr = attr;
        face.index = static_cast<Fl_Font>(i);
        font_faces_.push_back(face);
    }
}

// Picks the regular/bold/italic/bold-italic faces of the font family `name`
// (case-insensitive). Unknown names keep Courier; the name itself stays in the
// session file. Returns false if the family was not found.
bool TextEditorApp::resolve_font(const std::string& name) {
    font_regular_ = FL_COURIER;
    font_bold_ = FL_COURIER_BOLD;
    font_italic_ = FL_COURIER_ITALIC;
    font_bold_italic_ = FL_COURIER_BOLD_ITALIC;
    if (name.empty() || equals_nocase(name, DEFAULT_FONT_NAME)) return true;

    ensure_font_list();
    int face[4] = {-1, -1, -1, -1};   // index by attribute bits: bold = 1, italic = 2
    for (size_t i = 0; i < font_faces_.size(); ++i) {
        const FontFace& f = font_faces_[i];
        if (f.attr >= 0 && f.attr < 4 && face[f.attr] < 0 && equals_nocase(f.name, name)) face[f.attr] = f.index;
    }
    if (face[0] < 0) return false;
    font_regular_ = static_cast<Fl_Font>(face[0]);
    font_bold_ = static_cast<Fl_Font>(face[1] >= 0 ? face[1] : face[0]);
    font_italic_ = static_cast<Fl_Font>(face[2] >= 0 ? face[2] : face[0]);
    font_bold_italic_ = static_cast<Fl_Font>(face[3] >= 0 ? face[3] : (face[1] >= 0 ? face[1] : face[0]));
    return true;
}

// FLTK 1.3 draws each style run with one font and has no per-glyph fallback, so
// private-use characters (FontAwesome & co.) would show as empty boxes in a
// monospace font. Their style twins use an icon font instead; look for one among
// the installed fonts. Done lazily, the first time a document contains such glyphs.
void TextEditorApp::resolve_icon_font() {
    icon_font_resolved_ = true;
    ensure_font_list();
    int best = -1, best_score = 0, best_attr = 0;
    std::string best_name;
    for (size_t i = 0; i < font_faces_.size(); ++i) {
        const int sc = IconFont::score(font_faces_[i].name, font_faces_[i].attr, icon_font_name_);
        if (sc > best_score) { best_score = sc; best = font_faces_[i].index; best_attr = font_faces_[i].attr; best_name = font_faces_[i].name; }
    }
    if (best < 0) {
        set_status_message("No icon font found: install FontAwesome or a Nerd Font, or set icon_font=NAME in ~/.flitpad_session");
        return;
    }
    icon_font_ = static_cast<Fl_Font>(best);
    rebuild_style_table();
    for (size_t i = 0; i < open_tabs_.size(); ++i) open_tabs_[i]->editor->redraw();
    set_status_message("Icon font: " + best_name + ((best_attr & 1) ? " (bold)" : ""));
}

void TextEditorApp::apply_font_changes() {
    rebuild_style_table();
    for (size_t i = 0; i < open_tabs_.size(); ++i) {
        TabData* tab = open_tabs_[i];
        tab->editor->textfont(font_regular_);
        tab->editor->textsize(font_size_);
        tab->editor->linenumber_width(static_cast<int>(font_size_ * 3.3));
        tab->editor->linenumber_size(std::max(8, font_size_ - 3));
        tab->editor->highlight_data(tab->style_buffer, style_table_, STYLE_COUNT, 'A', 0, 0);
        tab->editor->redraw();
    }
}

void TextEditorApp::set_font_size(int size) {
    font_size_ = std::max(MIN_FONT_SIZE, std::min(MAX_FONT_SIZE, size));
    apply_font_changes();
    set_status_message("Font size " + std::to_string(font_size_));
}

// ---------------------------------------------------------------------------
// outside changes and dropped files
// ---------------------------------------------------------------------------

void TextEditorApp::external_change_cb(void* data) {
    TextEditorApp* self = static_cast<TextEditorApp*>(data);
    self->check_external_change(self->get_current_tab());
    if (NativeTheme::preference_changed()) NativeTheme::apply(self);   // Windows: dark / light title bar
    Fl::repeat_timeout(1.0, external_change_cb, data);
}

// If another program changed the file under this tab: reload it silently when there
// is nothing unsaved here, otherwise ask. Asked once per change.
void TextEditorApp::check_external_change(TabData* tab) {
    if (!tab || tab->is_untitled || checking_changes_) return;
    const long long modified = TextFile::modified_time(tab->filepath);
    if (modified == 0 || modified == tab->mtime) return;   // unchanged, or deleted (the text stays in the editor)

    checking_changes_ = true;
    tab->mtime = modified;
    bool reload = true;
    if (tab->is_modified) {
        reload = fl_choice("%s was changed on disk, and you have unsaved changes here.", "Keep Mine", "Reload", 0,
                           tab->filename.c_str()) == 1;
        if (std::find(open_tabs_.begin(), open_tabs_.end(), tab) == open_tabs_.end()) reload = false;   // closed meanwhile
    }
    if (reload) reload_tab(tab);
    checking_changes_ = false;
}

void TextEditorApp::reload_tab(TabData* tab) {
    std::string content, error;
    bool crlf = false;
    if (!TextFile::read(tab->filepath, content, crlf, error)) {
        set_status_message("Could not reload " + tab->filename);
        return;
    }
    const int pos = tab->editor->insert_position();
    tab->crlf = crlf;
    tab->buffer->text(content.c_str());
    tab->editor->insert_position(std::min(pos, tab->buffer->length()));
    tab->editor->show_insert_position();
    tab->is_modified = false;   // the text change above marked it modified
    update_tab_label(tab);
    set_status_message(tab->filename + " changed on disk and was reloaded");
}

// Files dropped on the editor or the sidebar (or "copy file" + paste): open them;
// a dropped folder becomes the sidebar folder. False if the text is not a file list.
bool TextEditorApp::open_dropped(const std::string& text) {
    const std::vector<std::string> paths = PathUtil::parse_uri_list(text);
    if (paths.empty()) return false;
    for (size_t i = 0; i < paths.size(); ++i) {
        if (fl_filename_isdir(paths[i].c_str())) set_current_dir(get_absolute_path(paths[i]));
        else open_file(paths[i]);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Ctrl+Tab switcher
// ---------------------------------------------------------------------------

void TextEditorApp::activate_tab(TabData* tab) {
    tabs_->value(tab->group);
    tabs_->set_changed();
    tabs_->do_callback();   // tabs_cb focuses the editor and refreshes the status bar
}

// Ctrl+Tab opens a list of the open files in tab order, with full paths, and moves down
// to the next one; every further Tab moves down, Shift+Tab (or Ctrl+Shift+Tab to begin
// with) moves up, and releasing Ctrl switches to the selected file.
void TextEditorApp::show_tab_switcher(bool backwards) {
    if (!switcher_ || open_tabs_.size() < 2) return;

    switcher_tabs_ = open_tabs_;
    int current = 0;
    std::vector<TabSwitcher::Entry> entries;
    for (size_t i = 0; i < switcher_tabs_.size(); ++i) {
        const TabData* t = switcher_tabs_[i];
        if (t == get_current_tab()) current = static_cast<int>(i);
        TabSwitcher::Entry e;
        e.title = t->filename + (t->is_modified ? " *" : "");
        e.path = t->is_untitled ? t->filepath + "   (not saved yet)" : t->filepath;
        entries.push_back(e);
    }
    switcher_->open(entries, current, w(), h());   // starts on the current file ...
    switcher_->move(backwards ? -1 : 1);           // ... and the key press moves from there (wraps around)
}

void TextEditorApp::commit_tab_switcher() {
    if (!switcher_ || !switcher_->visible()) return;
    const int row = switcher_->selected();
    switcher_->close();
    if (row >= 0 && row < static_cast<int>(switcher_tabs_.size()) &&
        std::find(open_tabs_.begin(), open_tabs_.end(), switcher_tabs_[static_cast<size_t>(row)]) != open_tabs_.end())
        activate_tab(switcher_tabs_[static_cast<size_t>(row)]);
    switcher_tabs_.clear();
}

// ---------------------------------------------------------------------------
// editor key bindings
// ---------------------------------------------------------------------------

int TextEditorApp::kf_smart_enter(int, Fl_Text_Editor* e) {
    Fl_Text_Buffer* buf = e->buffer();
    int s = 0, en = 0;
    if (buf->selection_position(&s, &en)) {
        buf->remove_selection();
        e->insert_position(s);
    }
    int pos = e->insert_position();
    std::string before = buffer_range(buf, buf->line_start(pos), pos);
    std::string after = buffer_range(buf, pos, buf->line_end(pos));

    TextEditorApp* app = static_cast<TextEditorApp*>(e->top_window());
    TabData* tab = app ? app->tab_for_editor(e) : nullptr;
    EditHelpers::NewlineEdit ed = EditHelpers::smart_newline(tab ? tab->language : "Plain Text", before, after, TAB_WIDTH);

    if (ed.delete_before > 0) {
        buf->remove(pos - ed.delete_before, pos);
        pos -= ed.delete_before;
    }
    if (!ed.text.empty()) {
        buf->insert(pos, ed.text.c_str());
        pos += static_cast<int>(ed.text.size());
    }
    e->insert_position(pos - ed.cursor_back);
    e->show_insert_position();
    return 1;
}

int TextEditorApp::kf_tab(int, Fl_Text_Editor* e) {
    Fl_Text_Buffer* buf = e->buffer();
    TextEditorApp* app = static_cast<TextEditorApp*>(e->top_window());
    TabData* tab = app ? app->tab_for_editor(e) : nullptr;

    int s = 0, en = 0;
    if (tab && buf->selection_position(&s, &en) && buf->line_start(s) != buf->line_start(en > s ? en - 1 : en)) {
        int a, b;
        bool has_sel;
        selection_lines(tab, a, b, has_sel);
        std::string out = EditHelpers::indent_lines(buffer_range(buf, a, b), TAB_WIDTH);
        buf->replace(a, b, out.c_str());
        buf->select(a, a + static_cast<int>(out.size()));
        e->insert_position(a + static_cast<int>(out.size()));
        return 1;
    }
    if (buf->selection_position(&s, &en)) {
        buf->remove_selection();
        e->insert_position(s);
    }
    int pos = e->insert_position();
    int col = buf->count_displayed_characters(buf->line_start(pos), pos);
    int n = TAB_WIDTH - (col % TAB_WIDTH);
    buf->insert(pos, std::string(static_cast<size_t>(n), ' ').c_str());
    e->insert_position(pos + n);
    e->show_insert_position();
    return 1;
}

int TextEditorApp::kf_shift_tab(int, Fl_Text_Editor* e) {
    Fl_Text_Buffer* buf = e->buffer();
    TextEditorApp* app = static_cast<TextEditorApp*>(e->top_window());
    TabData* tab = app ? app->tab_for_editor(e) : nullptr;
    if (!tab) return 1;

    int a, b;
    bool has_sel;
    selection_lines(tab, a, b, has_sel);
    std::string block = buffer_range(buf, a, b);
    std::string out = EditHelpers::outdent_lines(block, TAB_WIDTH);
    if (out == block) return 1;
    int cursor = e->insert_position();
    buf->replace(a, b, out.c_str());
    if (has_sel) {
        buf->select(a, a + static_cast<int>(out.size()));
        e->insert_position(a + static_cast<int>(out.size()));
    } else {
        int delta = static_cast<int>(out.size()) - static_cast<int>(block.size());
        e->insert_position(std::max(a, cursor + delta));
    }
    e->show_insert_position();
    return 1;
}

// ---------------------------------------------------------------------------
// window-level shortcuts
// ---------------------------------------------------------------------------

int TextEditorApp::handle(int event) {
    if (switcher_ && switcher_->visible()) {
        if (event == FL_KEYBOARD) {
            const int key = Fl::event_key();
            if (key == FL_Tab) switcher_->move((Fl::event_state() & FL_SHIFT) ? -1 : 1);
            else if (key == FL_Down) switcher_->move(1);
            else if (key == FL_Up) switcher_->move(-1);
            else if (key == FL_Escape) { switcher_->close(); switcher_tabs_.clear(); }
            else if (key == FL_Enter || key == FL_KP_Enter) commit_tab_switcher();
            return 1;   // nothing reaches the editor while the list is open
        }
        if (event == FL_KEYUP) {
            const int key = Fl::event_key();
            if (key == FL_Control_L || key == FL_Control_R) commit_tab_switcher();   // releasing Ctrl = switch
            return 1;
        }
        if (event == FL_PUSH) {
            const int row = switcher_->row_at(Fl::event_x(), Fl::event_y());
            if (row >= 0) { switcher_->select(row); commit_tab_switcher(); }
            else { switcher_->close(); switcher_tabs_.clear(); }
            return 1;
        }
        if (event == FL_UNFOCUS) { switcher_->close(); switcher_tabs_.clear(); }
    }

    if (open_tabs_.empty()) {   // no editor to drop onto: the window itself takes the files
        if (event == FL_DND_ENTER || event == FL_DND_DRAG || event == FL_DND_RELEASE || event == FL_DND_LEAVE) return 1;
        if (event == FL_PASTE && Fl::event_text() &&
            open_dropped(std::string(Fl::event_text(), static_cast<size_t>(Fl::event_length()))))
            return 1;
    }

    if (event == FL_KEYBOARD) {
        const int key = Fl::event_key();
        const int state = Fl::event_state();

        if (file_browser_ && file_browser_->value() > 0 && Fl::focus() == file_browser_) {
            if (key == (FL_F + 2)) { rename_selected_file(); return 1; }
            if (key == FL_Delete) { delete_selected_file(); return 1; }
        }

        if (key == (FL_F + 3)) {
            find_again((state & FL_SHIFT) != 0);
            return 1;
        }

        if ((state & FL_ALT) && !(state & (FL_CTRL | FL_SHIFT)) && (key == FL_Up || key == FL_Down)) {
            run_menu_command(key == FL_Up ? CMD_LINE_UP : CMD_LINE_DOWN);
            return 1;
        }

        if (state & FL_CTRL) {
            switch (key) {
                case 's': case 'S':
                    if (state & FL_SHIFT) save_current_file_as(); else save_current_file();
                    return 1;
                case 'o': case 'O': open_file_dialog(); return 1;
                case 'w': case 'W': close_current_tab(); return 1;
                case 'q': case 'Q': try_quit(); return 1;
                case 'n': case 'N': new_file(); return 1;
                case 'f': case 'F': show_find_dialog(); return 1;
                case 'h': case 'H': show_replace_dialog(); return 1;
                case 'g': case 'G': goto_line(); return 1;
                case '/': toggle_comment(); return 1;
                case 'd': case 'D': run_menu_command(CMD_DUPLICATE_LINE); return 1;
                case 'k': case 'K': run_menu_command(CMD_DELETE_LINE); return 1;
                case '=': case '+': case FL_KP + '+': set_font_size(font_size_ + 1); return 1;
                case '-': case FL_KP + '-': set_font_size(font_size_ - 1); return 1;
                case '0': case FL_KP + '0': set_font_size(DEFAULT_FONT_SIZE); return 1;
                case FL_Tab: show_tab_switcher((state & FL_SHIFT) != 0); return 1;
                default: break;
            }
        }
    }

    // Right-click in the editor: the caret moves to the click unless it
    // landed inside the selection, then the context menu opens.
    if (event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE) {
        TabData* tab = get_current_tab();
        if (tab && Fl::event_inside(tab->editor)) {
            tab->editor->take_focus();
            int pos = static_cast<CodeEditor*>(tab->editor)->position_at(Fl::event_x(), Fl::event_y());
            int s = 0, e = 0;
            if (!(tab->buffer->selection_position(&s, &e) && pos >= s && pos <= e)) {
                tab->buffer->unselect();
                tab->editor->insert_position(pos);
            }
            show_context_menu();
            update_status_bar();
            return 1;
        }
    }

    int ret = Fl_Double_Window::handle(event);
    // Update after the child widgets processed the event; before it the status bar
    // would always lag one keystroke behind and never follow mouse clicks.
    if (event == FL_KEYBOARD || event == FL_RELEASE || event == FL_DRAG) update_status_bar();
    return ret;
}
