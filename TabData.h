#ifndef TAB_DATA_H
#define TAB_DATA_H

#include <FL/Fl_Group.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Tabs.H>
#include <string>

struct TabData {
    Fl_Group* group;
    Fl_Text_Editor* editor;
    Fl_Text_Buffer* buffer;
    Fl_Text_Buffer* style_buffer;
    std::string filepath;
    std::string filename;
    std::string language;     // cached Highlighter::detect_language() result
    bool is_modified;
    bool is_untitled;         // created with Ctrl+N and never saved: Save asks for a name
    bool crlf;                // file used CRLF line endings; preserved on save
    bool restyle_pending;     // a debounced re-highlight is scheduled
    Fl_Tabs* tabs;

    TabData()
        : group(0), editor(0), buffer(0), style_buffer(0), language("Plain Text"),
          is_modified(false), is_untitled(false), crlf(false), restyle_pending(false), tabs(0) {}
};

#endif
