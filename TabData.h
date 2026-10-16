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
    bool is_modified;
    Fl_Tabs* tabs;
};

#endif
