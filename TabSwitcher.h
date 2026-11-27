#ifndef TAB_SWITCHER_H
#define TAB_SWITCHER_H

#include <FL/Fl_Widget.H>
#include <FL/Fl_Group.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "Theme.h"

// List of the open files (in tab order, with full paths) that
// appears in the middle of the window on Ctrl+Tab. It is a plain child widget, not a
// window, so the main window keeps keyboard focus and sees the Ctrl release.
// The owner drives it: open(), move(), selected(), close().
class TabSwitcher : public Fl_Widget {
public:
    struct Entry {
        std::string title;
        std::string path;
    };

    TabSwitcher() : Fl_Widget(0, 0, 10, 10), selected_(0), top_(0) { hide(); }

    void open(const std::vector<Entry>& entries, int selected, int window_w, int window_h) {
        entries_ = entries;
        selected_ = std::min(std::max(selected, 0), static_cast<int>(entries_.size()) - 1);
        const int rows = visible_rows();
        const int w = std::min(900, std::max(420, window_w * 6 / 10));
        const int h = rows * ROW_H + 2 * PAD;
        resize((window_w - w) / 2, (window_h - h) / 2, w, h);
        keep_selection_visible();
        show();
        redraw();
    }

    void close() {
        if (!visible()) return;
        hide();
        entries_.clear();
        if (parent()) parent()->redraw();
    }

    int selected() const { return selected_; }
    int count() const { return static_cast<int>(entries_.size()); }

    void move(int delta) {
        if (entries_.empty()) return;
        const int n = static_cast<int>(entries_.size());
        selected_ = ((selected_ + delta) % n + n) % n;     // wraps around
        keep_selection_visible();
        redraw();
    }

    void select(int row) {
        if (row < 0 || row >= count()) return;
        selected_ = row;
        redraw();
    }

    // Row under window coordinates (ex, ey), or -1.
    int row_at(int ex, int ey) const {
        if (ex < x() || ex >= x() + w() || ey < y() + PAD || ey >= y() + h() - PAD) return -1;
        const int row = top_ + (ey - y() - PAD) / ROW_H;
        return row < count() ? row : -1;
    }

    void draw() override {
        fl_color(Theme::panel_bg());
        fl_rectf(x(), y(), w(), h());
        fl_color(Theme::rgb(Theme::scheme().comment));
        fl_rect(x(), y(), w(), h());

        const int rows = visible_rows();
        for (int i = 0; i < rows; ++i) {
            const int index = top_ + i;
            if (index >= count()) break;
            const int ry = y() + PAD + i * ROW_H;
            const bool sel = (index == selected_);
            if (sel) {
                fl_color(Theme::selection());
                fl_rectf(x() + 3, ry, w() - 6, ROW_H);
            }
            const Fl_Color bg = sel ? Theme::selection() : Theme::panel_bg();
            fl_font(FL_HELVETICA_BOLD, 14);
            fl_color(fl_contrast(Theme::text_strong(), bg));
            fl_draw(entries_[index].title.c_str(), x() + 14, ry + 5, w() - 28, 18, FL_ALIGN_LEFT | FL_ALIGN_CLIP);
            fl_font(FL_HELVETICA, 12);
            fl_color(fl_contrast(Theme::text(), bg));
            const std::string path = elide_left(entries_[index].path, w() - 28);
            fl_draw(path.c_str(), x() + 14, ry + 24, w() - 28, 16, FL_ALIGN_LEFT | FL_ALIGN_CLIP);
        }
        if (count() > rows) {   // a hint that the list scrolls
            char more[48];
            std::snprintf(more, sizeof(more), "%d / %d", selected_ + 1, count());
            fl_font(FL_HELVETICA, 11);
            fl_color(fl_contrast(Theme::text(), Theme::panel_bg()));
            fl_draw(more, x(), y() + h() - PAD, w() - 8, PAD, FL_ALIGN_RIGHT | FL_ALIGN_CLIP);
        }
    }

private:
    enum { ROW_H = 44, PAD = 8, MAX_ROWS = 10 };

    std::vector<Entry> entries_;
    int selected_, top_;

    int visible_rows() const { return std::min(static_cast<int>(entries_.size()), static_cast<int>(MAX_ROWS)); }

    void keep_selection_visible() {
        const int rows = visible_rows();
        if (selected_ < top_) top_ = selected_;
        if (selected_ >= top_ + rows) top_ = selected_ - rows + 1;
        top_ = std::max(0, std::min(top_, std::max(0, count() - rows)));
    }

    // "/very/long/path/file.txt" -> ".../path/file.txt" so the file name stays visible
    static std::string elide_left(const std::string& s, int max_w) {
        if (fl_width(s.c_str()) <= max_w) return s;
        std::string t = s;
        while (t.size() > 4 && fl_width(("..." + t).c_str()) > max_w) {
            t.erase(0, 1);
            while (!t.empty() && (static_cast<unsigned char>(t[0]) & 0xC0) == 0x80) t.erase(0, 1);   // keep UTF-8 intact
        }
        return "..." + t;
    }
};

#endif
