#include "TextEditorApp.h"
#include "FindReplaceDialog.h"
#include "TabData.h"
#include "Highlighter.h"
#include "EditHelpers.h"
#include "PathUtil.h"
#include "Theme.h"
#include "CodeEditor.h"
#include "IconFont.h"
#include <FL/filename.H>
#include <FL/fl_ask.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Menu_Item.H>
#include <ctime>
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
static const int MIN_FONT_SIZE = 9;
static const int MAX_FONT_SIZE = 40;
static const int TAB_WIDTH = 4;

// context-menu / shortcut commands
enum MenuCommand {
    CMD_UNDO = 1, CMD_CUT, CMD_COPY, CMD_PASTE, CMD_DELETE, CMD_SELECT_ALL,
    CMD_INSERT_DATE, CMD_INSERT_TIME, CMD_INSERT_DATETIME,
    CMD_COMMENT, CMD_INDENT, CMD_OUTDENT, CMD_UPPER, CMD_LOWER,
    CMD_DUPLICATE_LINE, CMD_DELETE_LINE, CMD_LINE_UP, CMD_LINE_DOWN,
    CMD_FIND_WORD, CMD_GOTO_LINE
};

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

// GtkSourceView "solarized-dark" palette. Letters match Highlighter::Style.
// Theme::syntax() lifts colours that FLTK would otherwise draw white (see
// ColorMath.h). Entries 22..43 are the "icon twins" of 0..21 (same colour, drawn
// with the icon font); the constructor fills them in and resolve_icon_font()
// gives them the real font.
#define SYN(c) Theme::syntax(Theme::c)
Fl_Text_Editor::Style_Table_Entry TextEditorApp::style_table_[TextEditorApp::STYLE_COUNT] = {
    { SYN(BASE1),   FL_COURIER,        DEFAULT_FONT_SIZE, 0 }, // A plain text
    { SYN(ORANGE),  FL_COURIER,        DEFAULT_FONT_SIZE, 0 }, // B keywords (def:statement)
    { Theme::rgb(ColorMath::lifted(Theme::BASE01, Theme::BASE03, 108)),
                    FL_COURIER_ITALIC, DEFAULT_FONT_SIZE, 0 }, // C comments, ~~strike~~ (base01, italic)
    { SYN(CYAN),    FL_COURIER,        DEFAULT_FONT_SIZE, 0 }, // D strings
    { SYN(MAGENTA), FL_COURIER,        DEFAULT_FONT_SIZE, 0 }, // E numbers, true/null/None
    { SYN(BLUE),    FL_COURIER,        DEFAULT_FONT_SIZE, 0 }, // F functions, keys (def:identifier)
    { SYN(YELLOW),  FL_COURIER,        DEFAULT_FONT_SIZE, 0 }, // G types, variables (def:type)
    { SYN(ORANGE),  FL_COURIER_BOLD,   DEFAULT_FONT_SIZE, 0 }, // H markdown heading 1
    { SYN(MAGENTA), FL_COURIER_BOLD,   DEFAULT_FONT_SIZE, 0 }, // I markdown bold
    { SYN(GREEN),   FL_COURIER_ITALIC, DEFAULT_FONT_SIZE, 0 }, // J markdown italic
    { SYN(YELLOW),  FL_COURIER,        DEFAULT_FONT_SIZE, 0 }, // K markdown code
    { SYN(BLUE),    FL_COURIER,        DEFAULT_FONT_SIZE, 0 }, // L markdown link text
    { SYN(RED),     FL_COURIER,        DEFAULT_FONT_SIZE, 0 }, // M markdown markers (bullets, >, |, ```)
    { SYN(VIOLET),  FL_COURIER,        DEFAULT_FONT_SIZE, 0 }, // N preprocessor, decorators (def:preprocessor)
    { SYN(GREEN),   FL_COURIER,        DEFAULT_FONT_SIZE, 0 }, // O escapes, operators (def:special-char, def:operator)
    { SYN(YELLOW),  FL_COURIER_BOLD,   DEFAULT_FONT_SIZE, 0 }, // P markdown heading 2
    { SYN(GREEN),   FL_COURIER_BOLD,   DEFAULT_FONT_SIZE, 0 }, // Q markdown heading 3
    { SYN(CYAN),    FL_COURIER_BOLD,   DEFAULT_FONT_SIZE, 0 }, // R markdown heading 4
    { SYN(BLUE),    FL_COURIER_BOLD,   DEFAULT_FONT_SIZE, 0 }, // S markdown heading 5
    { SYN(VIOLET),  FL_COURIER_BOLD,   DEFAULT_FONT_SIZE, 0 }, // T markdown heading 6
    { SYN(CYAN),    FL_COURIER,        DEFAULT_FONT_SIZE, 0 }, // U markdown link destinations, URLs
    { SYN(VIOLET),  FL_COURIER,        DEFAULT_FONT_SIZE, 0 }  // V markdown block quotes
};
#undef SYN

static std::string url_decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
            out += static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), 0, 16));
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

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
        cleaned = url_decode(cleaned.substr(7));
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

std::string TextEditorApp::get_language_name(const std::string& path, const std::string& content) {
    return Highlighter::detect_language(path, content);
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
        std::string label = tab->filename + " *";
        tab->group->copy_label(label.c_str());
        tab->group->redraw();
        if (tab->tabs) tab->tabs->redraw();
    }
}

void TextEditorApp::update_tab_label(TabData* tab) {
    std::string label = tab->filename;
    if (tab->is_modified) label += " *";
    tab->group->copy_label(label.c_str());
    tab->group->redraw();
    if (tab->tabs) tab->tabs->redraw();
}

void TextEditorApp::set_status_message(const std::string& message) {
    status_bar_->copy_label((" " + message).c_str());
    status_bar_->redraw();
}

void TextEditorApp::update_status_bar() {
    TabData* tab = get_current_tab();
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
      find_dialog_(nullptr), replace_dialog_(nullptr), icon_font_resolved_(false) {

    // saved font size and optional icon font override
    settings_.load(home_file(".flitpad_config"));
    font_size_ = settings_.get_int("font_size", DEFAULT_FONT_SIZE, MIN_FONT_SIZE, MAX_FONT_SIZE);
    icon_font_name_ = settings_.get("icon_font");
    for (int i = 0; i < STYLE_COUNT; ++i) {
        if (i >= Highlighter::ICON_STYLE_OFFSET) {
            style_table_[i] = style_table_[i - Highlighter::ICON_STYLE_OFFSET];
            style_table_[i].font = FL_COURIER;
        }
        style_table_[i].size = font_size_;
    }

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

    end();
    resizable(tabs_);

    Fl::add_timeout(0.5, poll_commands_cb, this);

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
    for (size_t i = 0; i < open_tabs_.size(); ++i) Fl::remove_timeout(restyle_timeout_cb, open_tabs_[i]);
    settings_.set_int("font_size", font_size_);
    settings_.save(home_file(".flitpad_config"));
    save_session();
    std::remove(get_lock_file_path().c_str());
    std::remove(get_command_file_path().c_str());
}

// ---------------------------------------------------------------------------
// session
// ---------------------------------------------------------------------------

void TextEditorApp::save_session() {
    std::ofstream file(get_session_file_path().c_str());
    if (!file.is_open()) return;
    file << x() << " " << y() << " " << w() << " " << h() << "\n";

    TabData* current = get_current_tab();
    file << ((current && !current->is_untitled) ? current->filepath : std::string()) << "\n";
    for (size_t i = 0; i < open_tabs_.size(); ++i) {
        if (!open_tabs_[i]->is_untitled && !open_tabs_[i]->filepath.empty()) file << open_tabs_[i]->filepath << "\n";
    }
}

void TextEditorApp::load_session() {
    std::ifstream file(get_session_file_path().c_str());
    if (!file.is_open()) return;

    int wx, wy, ww, wh;
    if (file >> wx >> wy >> ww >> wh) {
        if (ww > 200 && wh > 200) resize(wx, wy, ww, wh);
    }

    std::string dummy;
    std::getline(file, dummy);

    std::string active_path, line;
    std::vector<std::string> session_paths;
    if (std::getline(file, active_path)) {
        if (!active_path.empty() && active_path.back() == '\r') active_path.pop_back();
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) session_paths.push_back(canonical_path(line));
        }
    }
    file.close();

    active_path = active_path.empty() ? "" : canonical_path(active_path);
    std::vector<std::string> restored;

    for (size_t i = 0; i < session_paths.size(); ++i) {
        std::ifstream check(session_paths[i].c_str(), std::ios::binary);
        if (!check.is_open()) continue;
        check.close();

        open_file(session_paths[i], false);
        for (size_t k = 0; k < open_tabs_.size(); ++k) {
            if (open_tabs_[k]->filepath == session_paths[i]) { restored.push_back(session_paths[i]); break; }
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

void TextEditorApp::select_tab(size_t index) {
    if (index >= open_tabs_.size()) return;
    tabs_->value(open_tabs_[index]->group);
    tabs_->set_changed();
    tabs_->do_callback();
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
    editor->textfont(FL_COURIER);
    editor->textsize(font_size_);
    editor->cursor_color(cursor_color);
    editor->cursor_style(Fl_Text_Editor::NORMAL_CURSOR);
    editor->show_cursor();
    editor->selection_color(select_color);

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

void TextEditorApp::new_file() {
    std::string name = "untitled";
    int counter = 0;
    for (;;) {
        std::string candidate = canonical_path(join_path(current_dir_, name));
        bool taken = false;
        for (size_t i = 0; i < open_tabs_.size(); ++i)
            if (open_tabs_[i]->filepath == candidate) { taken = true; break; }
        if (!taken) break;
        name = "untitled_" + std::to_string(++counter);
    }
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

    std::ifstream file(abs_path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        fl_alert("Could not open file:\n%s", abs_path.c_str());
        return;
    }
    std::stringstream buffer_stream;
    buffer_stream << file.rdbuf();
    file.close();
    std::string content = buffer_stream.str();

    if (content.find('\0') != std::string::npos) {
        fl_alert("This looks like a binary file, not text:\n%s", abs_path.c_str());
        return;
    }

    bool crlf = content.find("\r\n") != std::string::npos;
    content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());

    size_t last_slash = shown_path.find_last_of("/\\");
    std::string fname = (last_slash == std::string::npos) ? shown_path : shown_path.substr(last_slash + 1);

    TabData* tab = create_tab(abs_path, fname, content, false, crlf);
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
    std::string content = buffer_range(tab->buffer, 0, tab->buffer->length());
    if (!content.empty() && content[content.size() - 1] != '\n') content += '\n';

    if (tab->crlf) {
        std::string converted;
        converted.reserve(content.size() + content.size() / 16);
        for (size_t i = 0; i < content.size(); ++i) {
            if (content[i] == '\n') converted += '\r';
            converted += content[i];
        }
        content.swap(converted);
    }

    std::ofstream file(tab->filepath.c_str(), std::ios::binary);
    if (!file.is_open()) {
        fl_alert("Error: Could not save file %s", tab->filepath.c_str());
        return false;
    }
    file << content;
    file.close();
    if (!file) {
        fl_alert("Error: Could not save file %s", tab->filepath.c_str());
        return false;
    }
    tab->is_modified = false;
    update_tab_label(tab);
    if (file_browser_) load_directory(current_dir_);
    return true;
}

bool TextEditorApp::save_tab(TabData* tab) {
    if (!tab) return true;
    if (tab->is_untitled) return save_tab_as(tab);
    return write_tab_to_disk(tab);
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
    find_dialog_->focus_query();
}

void TextEditorApp::show_replace_dialog() {
    if (!replace_dialog_) replace_dialog_ = new FindReplaceDialog(this, true);
    std::string sel = selected_single_line(get_current_tab());
    if (!sel.empty()) replace_dialog_->set_query(sel);
    replace_dialog_->show();
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

// Applies `f` to the whole lines touched by the selection (or the caret's line).
static void transform_lines(TabData* tab, bool (*f)(const std::string&, std::string&)) {
    int a, b;
    bool has_sel;
    selection_lines(tab, a, b, has_sel);
    std::string block = buffer_range(tab->buffer, a, b), out;
    if (!f(block, out) || out == block) return;
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

static bool indent_block(const std::string& in, std::string& out) { out = EditHelpers::indent_lines(in, TAB_WIDTH); return true; }
static bool outdent_block(const std::string& in, std::string& out) { out = EditHelpers::outdent_lines(in, TAB_WIDTH); return true; }

static void change_selection_case(TabData* tab, bool upper) {
    int s = 0, e = 0;
    if (!tab->buffer->selection_position(&s, &e) || e <= s) return;
    std::string out = EditHelpers::change_case(buffer_range(tab->buffer, s, e), upper);
    tab->buffer->replace(s, e, out.c_str());     // same length: ASCII only
    tab->buffer->select(s, s + static_cast<int>(out.size()));
    tab->editor->insert_position(s + static_cast<int>(out.size()));
}

static void insert_at_cursor(TabData* tab, const std::string& text) {
    Fl_Text_Buffer* buf = tab->buffer;
    int s = 0, e = 0, pos = tab->editor->insert_position();
    if (buf->selection_position(&s, &e) && e > s) {
        buf->unselect();
        buf->replace(s, e, text.c_str());
        pos = s;
    } else {
        buf->insert(pos, text.c_str());
    }
    tab->editor->insert_position(pos + static_cast<int>(text.size()));
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

// Selected text, or the word under the caret; the caret ends up behind it so that
// the search finds the *next* occurrence.
void TextEditorApp::find_word_under_cursor() {
    TabData* tab = get_current_tab();
    if (!tab) return;
    Fl_Text_Buffer* buf = tab->buffer;
    int s = 0, e = 0;
    std::string query;
    if (buf->selection_position(&s, &e) && e > s && e - s <= 200) {
        query = buffer_range(buf, s, e);
    } else {
        int pos = tab->editor->insert_position();
        s = buf->word_start(pos);
        e = buf->word_end(pos);
        if (e > s) query = buffer_range(buf, s, e);
    }
    if (query.empty() || query.find('\n') != std::string::npos) {
        set_status_message("Nothing to search for: select text or put the caret on a word");
        return;
    }
    if (!find_dialog_) find_dialog_ = new FindReplaceDialog(this, false);   // so F3 repeats this search
    find_dialog_->set_query(query);
    buf->unselect();
    tab->editor->insert_position(e);
    find_text(query, true);
}

void TextEditorApp::run_menu_command(int command) {
    TabData* tab = get_current_tab();
    if (!tab) return;
    char text[64];
    time_t now = std::time(nullptr);
    switch (command) {
        case CMD_UNDO:       Fl_Text_Editor::kf_undo(0, tab->editor); break;
        case CMD_CUT:        Fl_Text_Editor::kf_cut(0, tab->editor); break;
        case CMD_COPY:       Fl_Text_Editor::kf_copy(0, tab->editor); break;
        case CMD_PASTE:      Fl_Text_Editor::kf_paste(0, tab->editor); break;
        case CMD_SELECT_ALL: Fl_Text_Editor::kf_select_all(0, tab->editor); break;
        case CMD_DELETE:
            if (tab->buffer->selected()) tab->buffer->remove_selection();
            break;
        case CMD_INSERT_DATE:
        case CMD_INSERT_TIME:
        case CMD_INSERT_DATETIME: {
            const char* fmt = command == CMD_INSERT_DATE ? "%Y-%m-%d" : command == CMD_INSERT_TIME ? "%H:%M:%S" : "%Y-%m-%d %H:%M:%S";
            std::strftime(text, sizeof(text), fmt, std::localtime(&now));
            insert_at_cursor(tab, text);
            break;
        }
        case CMD_COMMENT:        toggle_comment(); break;
        case CMD_INDENT:         transform_lines(tab, indent_block); break;
        case CMD_OUTDENT:        transform_lines(tab, outdent_block); break;
        case CMD_UPPER:          change_selection_case(tab, true); break;
        case CMD_LOWER:          change_selection_case(tab, false); break;
        case CMD_DUPLICATE_LINE: run_line_command(tab, EditHelpers::duplicate_lines); break;
        case CMD_DELETE_LINE:    run_line_command(tab, EditHelpers::delete_lines); break;
        case CMD_LINE_UP:        run_line_command(tab, move_up_cmd); break;
        case CMD_LINE_DOWN:      run_line_command(tab, move_down_cmd); break;
        case CMD_FIND_WORD:      find_word_under_cursor(); break;
        case CMD_GOTO_LINE:      goto_line(); break;
        default: break;
    }
    tab->editor->take_focus();
    update_status_bar();
}

// Right-click menu, modelled on Geany's editor popup.
void TextEditorApp::show_context_menu() {
    TabData* tab = get_current_tab();
    if (!tab) return;

    const int need_sel = tab->buffer->selected() ? 0 : FL_MENU_INACTIVE;
    const int need_comment = Highlighter::line_comment_prefix(tab->language).empty() ? FL_MENU_INACTIVE : 0;

    time_t now = std::time(nullptr);
    char d[32], t[32], dt[48];
    std::strftime(d, sizeof(d), "%Y-%m-%d", std::localtime(&now));
    std::strftime(t, sizeof(t), "%H:%M:%S", std::localtime(&now));
    std::strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    const std::string date_label = std::string("Date:  ") + d;
    const std::string time_label = std::string("Time:  ") + t;
    const std::string datetime_label = std::string("Date and Time:  ") + dt;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // FLTK menu tables list only the first few fields
#endif
#define CMD(c) reinterpret_cast<void*>(static_cast<intptr_t>(c))
    Fl_Menu_Item items[] = {
        { "Undo",       FL_CTRL + 'z', 0, CMD(CMD_UNDO),       FL_MENU_DIVIDER },
        { "Cut",        FL_CTRL + 'x', 0, CMD(CMD_CUT),        need_sel },
        { "Copy",       FL_CTRL + 'c', 0, CMD(CMD_COPY),       need_sel },
        { "Paste",      FL_CTRL + 'v', 0, CMD(CMD_PASTE),      0 },
        { "Delete",     0,             0, CMD(CMD_DELETE),     need_sel | FL_MENU_DIVIDER },
        { "Select All", FL_CTRL + 'a', 0, CMD(CMD_SELECT_ALL), FL_MENU_DIVIDER },
        { "Insert",     0, 0, 0, FL_SUBMENU },
            { date_label.c_str(),     0, 0, CMD(CMD_INSERT_DATE),     0 },
            { time_label.c_str(),     0, 0, CMD(CMD_INSERT_TIME),     0 },
            { datetime_label.c_str(), 0, 0, CMD(CMD_INSERT_DATETIME), 0 },
            { 0 },
        { "Format",     0, 0, 0, FL_SUBMENU },
            { "Toggle Comment",        FL_CTRL + '/', 0, CMD(CMD_COMMENT), need_comment | FL_MENU_DIVIDER },
            { "Increase Indent",       0, 0, CMD(CMD_INDENT),  0 },
            { "Decrease Indent",       0, 0, CMD(CMD_OUTDENT), FL_MENU_DIVIDER },
            { "Convert to Upper Case", 0, 0, CMD(CMD_UPPER),   need_sel },
            { "Convert to Lower Case", 0, 0, CMD(CMD_LOWER),   need_sel },
            { 0 },
        { "Line",       0, 0, 0, FL_SUBMENU | FL_MENU_DIVIDER },
            { "Duplicate Line",  FL_CTRL + 'd',  0, CMD(CMD_DUPLICATE_LINE), 0 },
            { "Delete Line",     FL_CTRL + 'k',  0, CMD(CMD_DELETE_LINE),    FL_MENU_DIVIDER },
            { "Move Line Up",    FL_ALT + FL_Up,   0, CMD(CMD_LINE_UP),      0 },
            { "Move Line Down",  FL_ALT + FL_Down, 0, CMD(CMD_LINE_DOWN),    0 },
            { 0 },
        { "Find Word / Selection", 0,          0, CMD(CMD_FIND_WORD), 0 },
        { "Go to Line...",         FL_CTRL + 'g', 0, CMD(CMD_GOTO_LINE), 0 },
        { 0 }
    };
#undef CMD
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y(), 0, 0, 0);
    if (picked && picked->user_data()) run_menu_command(static_cast<int>(reinterpret_cast<intptr_t>(picked->user_data())));
}

// FLTK 1.3 draws each style run with one font and has no per-glyph fallback, so
// private-use characters (FontAwesome & co.) would show as empty boxes in a
// monospace font. Their style twins use an icon font instead; look for one among
// the installed fonts. Done lazily, because listing every font is slow-ish.
void TextEditorApp::resolve_icon_font() {
    icon_font_resolved_ = true;
    const int count = Fl::set_fonts("*");
    int best = -1, best_score = 0, best_attr = 0;
    std::string best_name;
    for (int i = 0; i < count; ++i) {
        int attr = 0;
        const char* name = Fl::get_font_name(static_cast<Fl_Font>(i), &attr);
        if (!name) continue;
        int sc = IconFont::score(name, attr, icon_font_name_);
        if (sc > best_score) { best_score = sc; best = i; best_attr = attr; best_name = name; }
    }
    if (best < 0) {
        set_status_message("No icon font found: install FontAwesome or a Nerd Font, or set icon_font=NAME in ~/.flitpad_config");
        return;
    }
    for (int i = Highlighter::ICON_STYLE_OFFSET; i < STYLE_COUNT; ++i) style_table_[i].font = static_cast<Fl_Font>(best);
    for (size_t i = 0; i < open_tabs_.size(); ++i) open_tabs_[i]->editor->redraw();
    set_status_message("Icon font: " + best_name + ((best_attr & 1) ? " (bold)" : ""));
}

void TextEditorApp::set_font_size(int size) {
    size = std::max(MIN_FONT_SIZE, std::min(MAX_FONT_SIZE, size));
    font_size_ = size;
    for (int i = 0; i < STYLE_COUNT; ++i) style_table_[i].size = size;
    for (size_t i = 0; i < open_tabs_.size(); ++i) {
        TabData* tab = open_tabs_[i];
        tab->editor->textsize(size);
        tab->editor->linenumber_width(static_cast<int>(size * 3.3));
        tab->editor->linenumber_size(std::max(8, size - 3));
        tab->editor->highlight_data(tab->style_buffer, style_table_, STYLE_COUNT, 'A', 0, 0);
        tab->editor->redraw();
    }
    set_status_message("Font size " + std::to_string(size));
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
                case FL_Tab:
                    if (!open_tabs_.empty()) {
                        size_t cur = 0;
                        Fl_Widget* current = tabs_->value();
                        for (size_t i = 0; i < open_tabs_.size(); ++i)
                            if (open_tabs_[i]->group == current) { cur = i; break; }
                        size_t next = (state & FL_SHIFT) ? (cur == 0 ? open_tabs_.size() - 1 : cur - 1)
                                                         : (cur + 1) % open_tabs_.size();
                        select_tab(next);
                    }
                    return 1;
                default: break;
            }
        }
    }

    // Right-click in the editor: like Geany, the caret moves to the click unless it
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
