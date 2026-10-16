#ifndef CUSTOM_TABS_H
#define CUSTOM_TABS_H

#include <FL/Fl_Tabs.H>

class CustomTabs : public Fl_Tabs {
public:
    CustomTabs(int x, int y, int w, int h, const char* l = 0) : Fl_Tabs(x, y, w, h, l) {}
    
    int handle(int event) override {
        int ret = Fl_Tabs::handle(event);
        if (event == FL_PUSH && Fl::event_inside(this)) {
            if (Fl::event_y() >= y() && Fl::event_y() < y() + 35) {
                set_changed();
                do_callback();
            }
        }
        return ret;
    }
};

#endif
