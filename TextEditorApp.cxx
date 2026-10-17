#include "TextEditorApp.h"
#include <FL/filename.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_RGB_Image.H>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <algorithm>

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
    return Fl_Browser::handle(event);
}

Fl_Text_Editor::Style_Table_Entry TextEditorApp::style_table_[5] = {
    { fl_rgb_color(220, 223, 228), FL_COURIER, 17, 0 },
    { fl_rgb_color(198, 120, 221), FL_COURIER, 17, 0 },
    { fl_rgb_color(92, 99, 112),   FL_COURIER, 17, 0 },
    { fl_rgb_color(152, 195, 121), FL_COURIER, 17, 0 },
    { fl_rgb_color(209, 154, 102), FL_COURIER, 17, 0 }
};

std::string TextEditorApp::get_absolute_path(const std::string& path) {
    std::string cleaned = path;
    if (cleaned.rfind("file://", 0) == 0) {
        cleaned = cleaned.substr(7);
        if (cleaned.rfind("localhost", 0) == 0) {
            cleaned = cleaned.substr(9);
        }
    }

    char abs_path[1024];
    if (fl_filename_absolute(abs_path, sizeof(abs_path), cleaned.c_str())) {
        cleaned = std::string(abs_path);
    }
    std::replace(cleaned.begin(), cleaned.end(), '\\', '/');
    return cleaned;
}

std::string TextEditorApp::get_session_file_path() {
    const char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (home) return std::string(home) + "/.flitpad_session";
    return ".flitpad_session";
}

std::string TextEditorApp::get_command_file_path() {
    const char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (home) return std::string(home) + "/.flitpad_commands";
    return ".flitpad_commands";
}

std::string TextEditorApp::get_lock_file_path() {
    const char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (home) return std::string(home) + "/.flitpad_lock";
    return ".flitpad_lock";
}

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

        for (const auto& f : files_to_open) {
            self->open_file(f);
        }
        if (!files_to_open.empty()) self->redraw();
    }
    Fl::repeat_timeout(0.5, poll_commands_cb, data);
}

void TextEditorApp::style_update_cb(int pos, int n_inserted, int, int, const char*, void* cb_arg) {
    TabData* tab = static_cast<TabData*>(cb_arg);
    if (!tab || !tab->buffer || !tab->style_buffer) return;

    Fl_Text_Buffer* buf = tab->buffer;
    Fl_Text_Buffer* sbuf = tab->style_buffer;

    int length = buf->length();
    if (pos > length) pos = length;
    
    int start = buf->line_start(pos);
    int end = buf->line_end(pos + n_inserted);
    if (end > length) end = length;
    if (start > end) start = end;

    int text_len = end - start;
    if (text_len <= 0) return;

    char* raw_text = buf->text_range(start, end);
    std::string text = raw_text ? raw_text : "";
    std::free(raw_text);
    std::string styles(text.length(), 'A'); 

    bool is_markup = false;
    if (!tab->filepath.empty()) {
        size_t dot = tab->filepath.find_last_of('.');
        if (dot != std::string::npos) {
            std::string ext = tab->filepath.substr(dot + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == "xml" || ext == "svg" || ext == "html" || ext == "htm" || ext == "xhtml" || ext == "ui") {
                is_markup = true;
            }
        }
    }

    if (is_markup) {
        bool in_tag = false;
        bool in_string = false;
        char string_char = '\0';

        for (size_t i = 0; i < text.length(); ++i) {
            char c = text[i];

            if (!in_tag && !in_string && i + 3 < text.length() && text[i] == '<' && text[i+1] == '!' && text[i+2] == '-' && text[i+3] == '-') {
                while (i < text.length()) {
                    styles[i] = 'C';
                    if (i + 2 < text.length() && text[i] == '-' && text[i+1] == '-' && text[i+2] == '>') {
                        styles[i+1] = 'C';
                        styles[i+2] = 'C';
                        i += 2;
                        break;
                    }
                    i++;
                }
                continue;
            }

            if (in_tag) {
                if (!in_string && (c == '"' || c == '\'')) {
                    in_string = true;
                    string_char = c;
                    styles[i] = 'D';
                    continue;
                } else if (in_string && c == string_char) {
                    in_string = false;
                    styles[i] = 'D';
                    continue;
                } else if (in_string) {
                    styles[i] = 'D';
                    continue;
                }
            }

            if (c == '<') {
                in_tag = true;
                styles[i] = 'B';
                continue;
            }
            if (c == '>') {
                in_tag = false;
                styles[i] = 'B';
                continue;
            }

            if (in_tag) {
                styles[i] = std::isspace(static_cast<unsigned char>(c)) ? 'A' : 'B';
            } else {
                styles[i] = 'A';
            }
        }
    } else {
        bool in_single_string = false;
        bool in_double_string = false;
        bool in_triple_single = false;
        bool in_triple_double = false;

        auto is_escaped = [](const std::string& s, size_t idx) {
            if (idx == 0) return false;
            size_t count = 0, j = idx;
            while (j > 0 && s[j - 1] == '\\') {
                count++;
                j--;
            }
            return (count % 2 != 0);
        };

        for (size_t i = 0; i < text.length(); ++i) {
            char c = text[i];

            if (!in_single_string && !in_double_string) {
                if (!in_triple_single && i + 2 < text.length() && text[i] == '"' && text[i+1] == '"' && text[i+2] == '"') {
                    in_triple_double = !in_triple_double;
                    styles[i] = styles[i+1] = styles[i+2] = 'D';
                    i += 2;
                    continue;
                }
                if (!in_triple_double && i + 2 < text.length() && text[i] == '\'' && text[i+1] == '\'' && text[i+2] == '\'') {
                    in_triple_single = !in_triple_single;
                    styles[i] = styles[i+1] = styles[i+2] = 'D';
                    i += 2;
                    continue;
                }
            }

            if (in_triple_double || in_triple_single) {
                styles[i] = 'D';
                continue;
            }

            if (!in_single_string && !in_double_string) {
                if ((c == '/' && i + 1 < text.length() && text[i+1] == '/') || c == '#') {
                    while (i < text.length() && text[i] != '\n') {
                        styles[i] = 'C';
                        i++;
                    }
                    if (i < text.length()) styles[i] = 'C';
                    continue;
                }
            }

            if (!in_double_string && c == '\'' && !is_escaped(text, i)) {
                in_single_string = !in_single_string;
                styles[i] = 'D';
                continue;
            }
            if (!in_single_string && c == '"' && !is_escaped(text, i)) {
                in_double_string = !in_double_string;
                styles[i] = 'D';
                continue;
            }
            if (in_single_string || in_double_string) {
                styles[i] = 'D';
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(c))) {
                size_t start_n = i;
                while (i < text.length() && (std::isdigit(static_cast<unsigned char>(text[i])) || text[i] == '.')) i++;
                for (size_t j = start_n; j < i; ++j) styles[j] = 'E'; 
                i--;
                continue;
            }

            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                size_t start_w = i;
                while (i < text.length() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) i++;
                std::string word = text.substr(start_w, i - start_w);
                
                if (word == "class" || word == "struct" || word == "int" || word == "void" || 
                    word == "return" || word == "if" || word == "else" || word == "for" || 
                    word == "while" || word == "include" || word == "using" || word == "namespace" ||
                    word == "bool" || word == "true" || word == "false" || word == "const" || 
                    word == "public" || word == "private" || word == "auto" || word == "float" || 
                    word == "double" || word == "char" || word == "string" ||
                    word == "def" || word == "import" || word == "from" || word == "print" ||
                    word == "True" || word == "False" || word == "None" || word == "in" || 
                    word == "and" || word == "or" || word == "not" || word == "as" || 
                    word == "with" || word == "try" || word == "except" || word == "elif" ||
                    word == "is" || word == "lambda" || word == "global" || word == "pass" ||
                    word == "yield" || word == "raise" || word == "finally" || word == "assert") {
                    for (size_t j = start_w; j < i; ++j) styles[j] = 'B'; 
                }
                i--;
            }
        }
    }

    sbuf->replace(start, end, styles.c_str());
}

void TextEditorApp::buffer_modified_cb(int, int n_inserted, int n_deleted, int, const char*, void* cb_arg) {
    if (n_inserted == 0 && n_deleted == 0) return;
    TabData* tab = static_cast<TabData*>(cb_arg);
    if (!tab->is_modified) {
        tab->is_modified = true;
        update_tab_label(tab);
    }
}

void TextEditorApp::update_tab_label(TabData* tab) {
    std::string label = tab->filename;
    if (tab->is_modified) label += " *";
    tab->group->copy_label(label.c_str());
    tab->group->redraw();
    if (tab->tabs) tab->tabs->redraw();
}

std::string TextEditorApp::get_language_name(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "Plain Text";
    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == "py") return "Python";
    if (ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "hpp") return "C++";
    if (ext == "c" || ext == "h") return "C";
    if (ext == "js" || ext == "ts") return "JavaScript";
    if (ext == "html" || ext == "htm" || ext == "xhtml") return "HTML";
    if (ext == "xml" || ext == "svg" || ext == "ui") return "XML/SVG";
    if (ext == "css") return "CSS";
    if (ext == "json") return "JSON";
    if (ext == "md" || ext == "srt") return "Markdown/Subtitle";
    if (ext == "sh") return "Shell Script";
    return ext;
}

void TextEditorApp::update_status_bar() {
    Fl_Widget* current_tab = tabs_->value();
    if (!current_tab) {
        status_bar_->copy_label(" No file open | UTF-8");
        return;
    }

    for (auto tab : open_tabs_) {
        if (tab->group == current_tab) {
            int pos = tab->editor->insert_position();
            int line = tab->buffer->count_lines(0, pos) + 1;
            int start_of_line = tab->buffer->line_start(pos);
            int col = pos - start_of_line + 1;

            std::string lang = get_language_name(tab->filepath);

            char status_buf[512];
            snprintf(status_buf, sizeof(status_buf), " Ln %d, Col %d  |  UTF-8  |  %s  |  Path: %s", line, col, lang.c_str(), tab->filepath.c_str());
            status_bar_->copy_label(status_buf);
            break;
        }
    }
}

void TextEditorApp::tabs_cb(Fl_Widget* w, void* data) {
    TextEditorApp* self = static_cast<TextEditorApp*>(data);
    Fl_Tabs* tabs = static_cast<Fl_Tabs*>(w);
    Fl_Widget* current_tab = tabs->value();
    if (!current_tab) return;

    for (auto tab : self->open_tabs_) {
        if (tab->group == current_tab) {
            tab->editor->take_focus();
            size_t last_slash = tab->filepath.find_last_of("/\\");
            if (last_slash != std::string::npos) {
                std::string dir = tab->filepath.substr(0, last_slash);
                if (!dir.empty() && dir != self->current_dir_) {
                    self->set_current_dir(dir);
                }
            }
            break;
        }
    }
    self->update_status_bar();
}

void TextEditorApp::load_directory(const std::string& dir) {
    file_browser_->clear();

    dirent** files = nullptr;
    int n = fl_filename_list(dir.c_str(), &files, fl_numericsort);
    if (n > 0) {
        std::vector<std::string> folder_list;
        std::vector<std::string> file_list;

        for (int i = 0; i < n; ++i) {
            if (!files[i] || !files[i]->d_name) {
                if (files[i]) std::free(files[i]);
                continue;
            }
            std::string filename = files[i]->d_name;
            
            // Exclude ".", "..", and "./" completely from the browser
            if (filename == "." || filename == ".." || filename == "./") {
                std::free(files[i]);
                continue;
            }

            std::string full_path = dir;
            if (!full_path.empty() && full_path.back() != '/' && full_path.back() != '\\') {
                full_path += "/";
            }
            full_path += filename;

            if (fl_filename_isdir(full_path.c_str())) {
                folder_list.push_back(filename);
            } else {
                file_list.push_back(filename);
            }
            std::free(files[i]);
        }
        std::free(files);

        // Add folders first
        for (const auto& folder : folder_list) {
            file_browser_->add(folder.c_str());
            int idx = file_browser_->size();
            file_browser_->icon(idx, file_browser_->folder_icon_);
        }

        // Then add files
        for (const auto& file : file_list) {
            file_browser_->add(file.c_str());
            int idx = file_browser_->size();
            file_browser_->icon(idx, file_browser_->file_icon_);
        }
    }
}

void TextEditorApp::new_file_in_sidebar() {
    const char* name = fl_input("New file name:", "untitled");
    if (!name || std::string(name).empty()) return;

    std::string path = current_dir_;
    if (!path.empty() && path[path.length() - 1] != '/' && path[path.length() - 1] != '\\') {
        path += "/";
    }
    path += name;
    path = get_absolute_path(path);

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
    if (selected == "..") return; // Prevent renaming ".."

    std::string old_path = current_dir_;
    if (!old_path.empty() && old_path[old_path.length() - 1] != '/' && old_path[old_path.length() - 1] != '\\') {
        old_path += "/";
    }
    old_path += selected;

    const char* new_name = fl_input("Rename file to:", selected.c_str());
    if (!new_name || std::string(new_name).empty()) return;

    std::string new_path = current_dir_;
    if (!new_path.empty() && new_path[new_path.length() - 1] != '/' && new_path[new_path.length() - 1] != '\\') {
        new_path += "/";
    }
    new_path += new_name;

    old_path = get_absolute_path(old_path);
    new_path = get_absolute_path(new_path);

    if (std::rename(old_path.c_str(), new_path.c_str()) != 0) {
        fl_alert("Error: Could not rename file.");
        return;
    }

    size_t last_slash = new_path.find_last_of("/\\");
    std::string new_fname = (last_slash == std::string::npos) ? new_path : new_path.substr(last_slash + 1);

    for (auto tab : open_tabs_) {
        if (tab->filepath == old_path) {
            tab->filepath = new_path;
            tab->filename = new_fname;
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
    if (selected == "..") return; // Prevent deleting ".."

    std::string target_path = current_dir_;
    if (!target_path.empty() && target_path[target_path.length() - 1] != '/' && target_path[target_path.length() - 1] != '\\') {
        target_path += "/";
    }
    target_path += selected;
    target_path = get_absolute_path(target_path);

    if (fl_choice("Are you sure you want to delete %s?", "Cancel", "Delete", 0, selected.c_str()) == 1) {
        if (std::remove(target_path.c_str()) != 0) {
            fl_alert("Error: Could not delete file.");
            return;
        }

        for (auto it = open_tabs_.begin(); it != open_tabs_.end(); ++it) {
            if ((*it)->filepath == target_path) {
                TabData* tab = *it;
                tabs_->remove(tab->group);
                delete tab->group;
                delete tab->buffer;
                delete tab->style_buffer;
                delete tab;
                open_tabs_.erase(it);
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

    std::string selected = text;
    std::string target_path;

    if (selected == "..") {
        size_t last_slash = self->current_dir_.find_last_of("/\\");
        if (last_slash != std::string::npos && last_slash > 0) {
            target_path = self->current_dir_.substr(0, last_slash);
        } else {
            target_path = "/";
        }
    } else {
        target_path = self->current_dir_;
        if (!target_path.empty() && target_path.back() != '/' && target_path.back() != '\\') {
            target_path += "/";
        }
        target_path += selected;
    }
    
    target_path = self->get_absolute_path(target_path);

    if (fl_filename_isdir(target_path.c_str())) {
        self->current_dir_ = target_path;
        self->load_directory(target_path);
    } else {
        self->open_file(target_path);
    }
}

void TextEditorApp::window_close_cb(Fl_Widget*, void* data) {
    TextEditorApp* app = static_cast<TextEditorApp*>(data);
    app->try_quit();
}

TextEditorApp::TextEditorApp(int width, int height, const char* title) 
    : Fl_Double_Window(width, height, title), find_dialog_(nullptr), replace_dialog_(nullptr) {
    
    Fl_Color bg_color = fl_rgb_color(40, 44, 52);        
    Fl_Color panel_color = fl_rgb_color(33, 37, 43);    
    Fl_Color text_color = fl_rgb_color(171, 178, 191);  

    color(bg_color);
    callback(window_close_cb, this);

    char cwd[1024];
    if (fl_getcwd(cwd, sizeof(cwd))) {
        current_dir_ = cwd;
    } else {
        current_dir_ = ".";
    }
    std::replace(current_dir_.begin(), current_dir_.end(), '\\', '/');

    file_browser_ = new CustomFileBrowser(10, 10, 220, height - 55, this);
    file_browser_->color(panel_color);
    file_browser_->textcolor(text_color);
    file_browser_->type(FL_HOLD_BROWSER);
    load_directory(current_dir_);
    file_browser_->callback(browser_cb, this);

    CustomTabs* ctabs = new CustomTabs(240, 10, width - 250, height - 55);
    tabs_ = ctabs;
    tabs_->box(FL_THIN_UP_BOX);
    tabs_->color(bg_color);
    tabs_->callback(tabs_cb, this);
    tabs_->end();

    status_bar_ = new Fl_Box(10, height - 35, width - 20, 25, " Ready | UTF-8");
    status_bar_->box(FL_FLAT_BOX);
    status_bar_->color(fl_rgb_color(33, 37, 43));
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
    save_session();
    std::remove(get_lock_file_path().c_str());
    std::remove(get_command_file_path().c_str());
}

void TextEditorApp::set_current_dir(const std::string& dir) {
    current_dir_ = dir;
    if (file_browser_) {
        load_directory(current_dir_);
    }
}

void TextEditorApp::save_session() {
    std::string path = get_session_file_path();
    std::ofstream file(path.c_str());
    if (file.is_open()) {
        file << x() << " " << y() << " " << w() << " " << h() << "\n";
        
        TabData* current = get_current_tab();
        std::string current_path = current ? current->filepath : "";
        file << current_path << "\n";
        for (auto tab : open_tabs_) {
            if (!tab->filepath.empty()) {
                file << tab->filepath << "\n";
            }
        }
        file.close();
    }
}

void TextEditorApp::load_session() {
    std::string session_path = get_session_file_path();
    std::ifstream file(session_path.c_str());

    if (!file.is_open()) return;

    int wx, wy, ww, wh;
    if (file >> wx >> wy >> ww >> wh) {
        if (ww > 200 && wh > 200) {
            resize(wx, wy, ww, wh);
        }
    }
    
    std::string dummy;
    std::getline(file, dummy);

    std::string active_path;
    std::vector<std::string> session_paths;
    std::string line;

    if (std::getline(file, active_path)) {
        if (!active_path.empty() && active_path.back() == '\r')
            active_path.pop_back();

        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (!line.empty())
                session_paths.push_back(get_absolute_path(line));
        }
    }

    file.close();

    active_path = active_path.empty() ? "" : get_absolute_path(active_path);
    std::vector<std::string> restored_paths;

    for (const auto& path : session_paths) {
        std::ifstream check(path.c_str(), std::ios::binary);
        if (!check.is_open()) continue;
        check.close();

        open_file(path);

        for (const auto* tab : open_tabs_) {
            if (tab->filepath == path) {
                restored_paths.push_back(path);
                break;
            }
        }
    }

    std::string target_path;
    for (const auto& path : restored_paths) {
        if (path == active_path) {
            target_path = path;
            break;
        }
    }

    if (target_path.empty() && !restored_paths.empty())
        target_path = restored_paths.front();

    if (!target_path.empty()) {
        for (auto* tab : open_tabs_) {
            if (tab->filepath == target_path) {
                tabs_->value(tab->group);
                tab->editor->take_focus();
                break;
            }
        }

        size_t slash = target_path.find_last_of("/\\");
        std::string dir;
        if (slash == std::string::npos) {
            dir = ".";
        } else if (slash == 0) {
            dir = target_path.substr(0, 1);
        } else {
            dir = target_path.substr(0, slash);
        }

        set_current_dir(dir);
    }

    update_status_bar();
    redraw();
}

TabData* TextEditorApp::get_current_tab() {
    Fl_Widget* current_tab = tabs_->value();
    if (!current_tab) return nullptr;
    for (auto tab : open_tabs_) {
        if (tab->group == current_tab) return tab;
    }
    return nullptr;
}

void TextEditorApp::new_file() {
    std::string fname = "untitled";
    int counter = 0;
    std::string path = current_dir_;
    if (!path.empty() && path[path.length() - 1] != '/' && path[path.length() - 1] != '\\') {
        path += "/";
    }
    std::string candidate_name = fname;
    std::string candidate_path = get_absolute_path(path + candidate_name);

    bool exists = true;
    while (exists) {
        exists = false;
        for (auto tab : open_tabs_) {
            if (tab->filepath == candidate_path) {
                exists = true;
                counter++;
                candidate_name = "untitled_" + std::to_string(counter);
                candidate_path = get_absolute_path(path + candidate_name);
                break;
            }
        }
    }

    Fl_Color editor_bg = fl_rgb_color(30, 33, 39);
    Fl_Color editor_text = fl_rgb_color(220, 223, 228);
    Fl_Color cursor_color = fl_rgb_color(97, 175, 239); 
    Fl_Color select_color = fl_rgb_color(50, 115, 195);

    tabs_->begin();
    Fl_Group* grp = new Fl_Group(240, 35, w() - 250, h() - 80);
    grp->color(fl_rgb_color(40, 44, 52));
    
    Fl_Text_Editor* editor = new Fl_Text_Editor(245, 40, w() - 260, h() - 90);
    editor->linenumber_width(55);
    
    Fl_Text_Buffer* textbuf = new Fl_Text_Buffer();
    Fl_Text_Buffer* stylebuf = new Fl_Text_Buffer();

    editor->buffer(textbuf);
    editor->color(editor_bg);
    editor->textcolor(editor_text);
    editor->textfont(FL_COURIER);
    editor->textsize(17);                  
    editor->cursor_color(cursor_color);    
    editor->cursor_style(Fl_Text_Editor::NORMAL_CURSOR);
    editor->show_cursor();                 
    editor->selection_color(select_color);

    stylebuf->text(std::string(textbuf->length(), 'A').c_str());
    TabData* tab_data = new TabData{grp, editor, textbuf, stylebuf, candidate_path, candidate_name, true, tabs_};

    editor->highlight_data(stylebuf, style_table_, 5, 'A', 0, 0);
    style_update_cb(0, textbuf->length(), 0, 0, 0, tab_data);
    
    textbuf->add_modify_callback(style_update_cb, tab_data);
    textbuf->add_modify_callback(buffer_modified_cb, tab_data);

    grp->end();
    tabs_->end();

    open_tabs_.push_back(tab_data);
    tabs_->value(grp);
    editor->take_focus();
    update_tab_label(tab_data);
    update_status_bar();
    redraw();
}

void TextEditorApp::open_file(const std::string& path) {
    std::string abs_path = get_absolute_path(path);

    for (auto tab : open_tabs_) {
        if (tab->filepath == abs_path) {
            tabs_->value(tab->group);
            tab->editor->take_focus();
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

    std::string file_content = buffer_stream.str();
    file_content.erase(std::remove(file_content.begin(), file_content.end(), '\r'), file_content.end());

    size_t last_slash = abs_path.find_last_of("/\\");
    std::string fname = (last_slash == std::string::npos) ? abs_path : abs_path.substr(last_slash + 1);

    Fl_Color editor_bg = fl_rgb_color(30, 33, 39);
    Fl_Color editor_text = fl_rgb_color(220, 223, 228);
    Fl_Color cursor_color = fl_rgb_color(97, 175, 239); 
    Fl_Color select_color = fl_rgb_color(50, 115, 195);

    tabs_->begin();
    Fl_Group* grp = new Fl_Group(240, 35, w() - 250, h() - 80);
    grp->copy_label(fname.c_str());
    grp->color(fl_rgb_color(40, 44, 52));
    
    Fl_Text_Editor* editor = new Fl_Text_Editor(245, 40, w() - 260, h() - 90);
    editor->linenumber_width(55);

    Fl_Text_Buffer* textbuf = new Fl_Text_Buffer();
    Fl_Text_Buffer* stylebuf = new Fl_Text_Buffer();

    editor->buffer(textbuf);
    editor->color(editor_bg);
    editor->textcolor(editor_text);
    editor->textfont(FL_COURIER);
    editor->textsize(17);                  
    editor->cursor_color(cursor_color);    
    editor->cursor_style(Fl_Text_Editor::NORMAL_CURSOR);
    editor->show_cursor();                 
    editor->selection_color(select_color);

    textbuf->text(file_content.c_str());

    stylebuf->text(std::string(textbuf->length(), 'A').c_str());
    TabData* tab_data = new TabData{grp, editor, textbuf, stylebuf, abs_path, fname, false, tabs_};

    editor->highlight_data(stylebuf, style_table_, 5, 'A', 0, 0);
    style_update_cb(0, textbuf->length(), 0, 0, 0, tab_data);
    
    textbuf->add_modify_callback(style_update_cb, tab_data);
    textbuf->add_modify_callback(buffer_modified_cb, tab_data);

    grp->end();
    tabs_->end();

    open_tabs_.push_back(tab_data);
    tabs_->value(grp);
    editor->take_focus();
    update_tab_label(tab_data);
    update_status_bar();
    redraw();
}

bool TextEditorApp::save_tab(TabData* tab) {
    if (!tab) return true;
    std::ofstream file(tab->filepath.c_str());
    if (file.is_open()) {
        char* text = tab->buffer->text();
        file << (text ? text : "");
        free(text);
        file.close();
        if (!file) {
            fl_alert("Error: Could not save file %s", tab->filepath.c_str());
            return false;
        }
        tab->is_modified = false;
        update_tab_label(tab);
        
        if (file_browser_) {
            load_directory(current_dir_);
        }
        return true;
    } else {
        fl_alert("Error: Could not save file %s", tab->filepath.c_str());
        return false;
    }
}

void TextEditorApp::save_current_file() {
    TabData* tab = get_current_tab();
    if (tab) save_tab(tab);
}

void TextEditorApp::close_current_tab() {
    Fl_Widget* current_tab = tabs_->value();
    if (!current_tab) return;

    for (auto it = open_tabs_.begin(); it != open_tabs_.end(); ++it) {
        if ((*it)->group == current_tab) {
            TabData* tab = *it;
            if (tab->is_modified) {
                int choice = fl_choice("File %s has unsaved changes.", "Cancel", "Save", "Close Without Saving", tab->filename.c_str());
                if (choice == 0) return;
                if (choice == 1 && !save_tab(tab)) return;
            }

            tabs_->remove(tab->group);
            delete tab->group; 
            delete tab->buffer;
            delete tab->style_buffer;
            delete tab;
            open_tabs_.erase(it);
            update_status_bar();
            redraw();
            break;
        }
    }
}

void TextEditorApp::try_quit() {
    for (auto tab : open_tabs_) {
        if (tab->is_modified) {
            tabs_->value(tab->group);
            int choice = fl_choice("File %s has unsaved changes. Save before exiting?", "Cancel", "Save", "Don't Save", tab->filename.c_str());
            if (choice == 0) return;
            if (choice == 1 && !save_tab(tab)) return;
        }
    }
    hide();
}

void TextEditorApp::show_find_dialog() {
    if (!find_dialog_) {
        find_dialog_ = new FindReplaceDialog(this, false);
    }
    find_dialog_->show();
    find_dialog_->redraw();
}

void TextEditorApp::show_replace_dialog() {
    if (!replace_dialog_) {
        replace_dialog_ = new FindReplaceDialog(this, true);
    }
    replace_dialog_->show();
    replace_dialog_->redraw();
}

void TextEditorApp::find_text(const std::string& query, bool match_case) {
    TabData* tab = get_current_tab();
    if (!tab) return;
    Fl_Text_Buffer* buf = tab->buffer;
    Fl_Text_Editor* editor = tab->editor;

    int start_pos = editor->insert_position();
    int found_pos = 0;
    bool found = buf->search_forward(start_pos, query.c_str(), &found_pos, match_case ? 1 : 0);
    if (!found && start_pos > 0) {
        found = buf->search_forward(0, query.c_str(), &found_pos, match_case ? 1 : 0);
    }
    if (found) {
        editor->insert_position(found_pos + query.length());
        buf->select(found_pos, found_pos + query.length());
        editor->show_insert_position();
    } else {
        fl_alert("Text not found.");
    }
}

void TextEditorApp::find_previous_text(const std::string& query, bool match_case) {
    TabData* tab = get_current_tab();
    if (!tab) return;
    Fl_Text_Buffer* buf = tab->buffer;
    Fl_Text_Editor* editor = tab->editor;

    int start_pos = editor->insert_position();
    int sel_s, sel_e;
    if (buf->selection_position(&sel_s, &sel_e)) {
        start_pos = sel_s;
    }
    if (start_pos > 0) start_pos--;

    int found_pos = 0;
    bool found = buf->search_backward(start_pos, query.c_str(), &found_pos, match_case ? 1 : 0);
    if (!found) {
        found = buf->search_backward(buf->length(), query.c_str(), &found_pos, match_case ? 1 : 0);
    }
    if (found) {
        editor->insert_position(found_pos);
        buf->select(found_pos, found_pos + query.length());
        editor->show_insert_position();
    } else {
        fl_alert("Text not found.");
    }
}

void TextEditorApp::replace_text(const std::string& query, const std::string& replacement, bool match_case) {
    TabData* tab = get_current_tab();
    if (!tab) return;
    Fl_Text_Buffer* buf = tab->buffer;
    Fl_Text_Editor* editor = tab->editor;

    int sel_start = 0, sel_end = 0;
    if (buf->selection_position(&sel_start, &sel_end)) {
        char* raw_selected = buf->text_range(sel_start, sel_end);
        std::string selected_text = raw_selected ? raw_selected : "";
        std::free(raw_selected);
        bool matches = match_case ? (selected_text == query) : 
            [](std::string a, std::string b) {
                if (a.size() != b.size()) return false;
                for(size_t i=0; i<a.size(); ++i) if(std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
                return true;
            }(selected_text, query);
        if (matches) {
            buf->replace(sel_start, sel_end, replacement.c_str());
            editor->insert_position(sel_start + replacement.length());
        }
    }
    find_text(query, match_case);
}

void TextEditorApp::replace_all_text(const std::string& query, const std::string& replacement, bool match_case) {
    TabData* tab = get_current_tab();
    if (!tab) return;
    Fl_Text_Buffer* buf = tab->buffer;

    if (query.empty()) return;

    int pos = 0;
    int count = 0;
    int found_pos = 0;
    while (pos <= buf->length() &&
           buf->search_forward(pos, query.c_str(), &found_pos, match_case ? 1 : 0)) {
        buf->replace(found_pos, found_pos + static_cast<int>(query.length()), replacement.c_str());
        pos = found_pos + static_cast<int>(replacement.length());
        if (replacement.empty()) pos = found_pos;
        ++count;
        if (pos > buf->length()) break;
    }
    fl_message("Replaced %d occurrence(s).", count);
}

int TextEditorApp::handle(int event) {
    if (event == FL_KEYBOARD || event == FL_PUSH || event == FL_DRAG || event == FL_RELEASE) {
        update_status_bar();
        
        if (event == FL_KEYBOARD) {
            int key = Fl::event_key();
            if (key == FL_F + 2 && Fl::belowmouse() == file_browser_) {
                rename_selected_file();
                return 1;
            }
            if (key == FL_Delete && Fl::belowmouse() == file_browser_) {
                delete_selected_file();
                return 1;
            }

            if (Fl::event_state() & FL_CTRL) {
                if (key == 's' || key == 'S') {
                    save_current_file();
                    return 1;
                } else if (key == 'w' || key == 'W') {
                    close_current_tab();
                    return 1;
                } else if (key == 'q' || key == 'Q') {
                    try_quit();
                    return 1;
                } else if (key == 'n' || key == 'N') {
                    new_file();
                    return 1;
                } else if (key == 'f' || key == 'F') {
                    show_find_dialog();
                    return 1;
                } else if (key == 'h' || key == 'H') {
                    show_replace_dialog();
                    return 1;
                } else if (key == FL_Tab) {
                    if (!open_tabs_.empty()) {
                        Fl_Widget* current_tab = tabs_->value();
                        size_t current_idx = 0;
                        for (size_t i = 0; i < open_tabs_.size(); ++i) {
                            if (open_tabs_[i]->group == current_tab) {
                                current_idx = i;
                                break;
                            }
                        }
                        size_t next_idx;
                        if (Fl::event_state() & FL_SHIFT) {
                            next_idx = (current_idx == 0) ? open_tabs_.size() - 1 : current_idx - 1;
                        } else {
                            next_idx = (current_idx + 1) % open_tabs_.size();
                        }
                        tabs_->value(open_tabs_[next_idx]->group);
                        tabs_->set_changed();
                        tabs_->do_callback();
                        update_status_bar();
                    }
                    return 1;
                }
            }
        }
    }
    return Fl_Double_Window::handle(event);
}
