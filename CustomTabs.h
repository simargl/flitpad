#ifndef CUSTOM_TABS_H
#define CUSTOM_TABS_H

#include <FL/Fl.H>
#include <FL/Fl_Tabs.H>
#include <functional>

// Fl_Tabs with two extras:
//  * clicking anywhere on the tab bar (even on the already active tab) fires the
//    callback, so the editor gets keyboard focus back
//  * middle-click on a tab asks the owner to close it
//  * right-click on a tab asks the owner to show its context menu
class CustomTabs : public Fl_Tabs {
public:
    std::function<void(Fl_Widget*)> on_middle_click;
    std::function<void(Fl_Widget*)> on_right_click;

    CustomTabs(int x, int y, int w, int h, const char* l = 0) : Fl_Tabs(x, y, w, h, l) {}

    int handle(int event) override {
        if (event == FL_PUSH && Fl::event_button() == FL_MIDDLE_MOUSE && in_tab_bar()) {
            Fl_Widget* tab = which(Fl::event_x(), Fl::event_y());
            if (tab && on_middle_click) {
                on_middle_click(tab);
                return 1;
            }
        }
        if (event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE && in_tab_bar()) {
            Fl_Widget* tab = which(Fl::event_x(), Fl::event_y());
            if (tab && on_right_click) {
                on_right_click(tab);   // the owner defers anything that deletes tabs
                return 1;
            }
        }
        int ret = Fl_Tabs::handle(event);
        if (event == FL_RELEASE && in_tab_bar()) {
            set_changed();
            do_callback();
        }
        return ret;
    }

private:
    bool in_tab_bar() const { return Fl::event_inside(x(), y(), w(), 35); }
};

#endif
