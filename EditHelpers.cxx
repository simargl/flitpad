#include "EditHelpers.h"
#include "Highlighter.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <vector>

namespace EditHelpers {
namespace {

bool is_blank(char c) { return c == ' ' || c == '\t'; }

std::string leading_blanks(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && is_blank(s[i])) ++i;
    return s.substr(0, i);
}

std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> out;
    size_t b = 0;
    for (;;) {
        size_t e = s.find('\n', b);
        if (e == std::string::npos) { out.push_back(s.substr(b)); break; }
        out.push_back(s.substr(b, e - b));
        b = e + 1;
    }
    return out;
}

std::string join_lines(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += '\n';
        out += v[i];
    }
    return out;
}

bool only_blanks(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i)
        if (!is_blank(s[i]) && s[i] != '\r') return false;
    return true;
}

int line_start_of(const std::string& d, int pos) {
    while (pos > 0 && d[pos - 1] != '\n') --pos;
    return pos;
}

int line_end_of(const std::string& d, int pos) {
    const int n = static_cast<int>(d.size());
    while (pos < n && d[pos] != '\n') ++pos;
    return pos;
}

struct Span {
    int a, b;   // first byte of the first line, end (before '\n') of the last line
    int s, e;   // selection (or caret), clamped
    int cursor;
};

Span line_span(const std::string& d, int s, int e, int cursor, bool has_sel) {
    const int n = static_cast<int>(d.size());
    Span sp;
    sp.cursor = std::min(std::max(cursor, 0), n);
    if (!has_sel) s = e = sp.cursor;
    s = std::min(std::max(s, 0), n);
    e = std::min(std::max(e, 0), n);
    if (e < s) std::swap(s, e);
    int last = e;
    if (has_sel && last > s && d[last - 1] == '\n') --last;   // selection ends at a line start
    sp.s = s;
    sp.e = e;
    sp.a = line_start_of(d, s);
    sp.b = line_end_of(d, last);
    return sp;
}

void shift_selection(RegionEdit& r, const Span& sp, bool has_sel, int shift) {
    r.cursor = sp.cursor + shift;
    if (has_sel) { r.sel_start = sp.s + shift; r.sel_end = sp.e + shift; }
}

}  // namespace

RegionEdit duplicate_lines(const std::string& doc, int s, int e, int cursor, bool has_sel) {
    RegionEdit r;
    Span sp = line_span(doc, s, e, cursor, has_sel);
    std::string block = doc.substr(sp.a, sp.b - sp.a);
    r.valid = true;
    r.start = r.end = sp.b;
    r.text = "\n" + block;
    shift_selection(r, sp, has_sel, static_cast<int>(block.size()) + 1);   // caret / selection move onto the copy
    return r;
}

RegionEdit delete_lines(const std::string& doc, int s, int e, int cursor, bool has_sel) {
    RegionEdit r;
    Span sp = line_span(doc, s, e, cursor, has_sel);
    r.valid = true;
    r.start = sp.a;
    r.end = sp.b;
    if (sp.b < static_cast<int>(doc.size())) r.end = sp.b + 1;       // take the line break with the line
    else if (sp.a > 0) r.start = sp.a - 1;                           // last line: take the break before it
    r.cursor = r.start;
    return r;
}

RegionEdit move_lines(const std::string& doc, int s, int e, int cursor, bool has_sel, bool up) {
    RegionEdit r;
    Span sp = line_span(doc, s, e, cursor, has_sel);
    const std::string block = doc.substr(sp.a, sp.b - sp.a);
    if (up) {
        if (sp.a == 0) return r;
        const int ps = line_start_of(doc, sp.a - 1);
        const std::string prev = doc.substr(ps, sp.a - 1 - ps);
        r.valid = true;
        r.start = ps;
        r.end = sp.b;
        r.text = block + "\n" + prev;
        shift_selection(r, sp, has_sel, -(static_cast<int>(prev.size()) + 1));
    } else {
        if (sp.b >= static_cast<int>(doc.size())) return r;
        const int ns = sp.b + 1, ne = line_end_of(doc, ns);
        const std::string next = doc.substr(ns, ne - ns);
        r.valid = true;
        r.start = sp.a;
        r.end = ne;
        r.text = next + "\n" + block;
        shift_selection(r, sp, has_sel, static_cast<int>(next.size()) + 1);
    }
    return r;
}

NewlineEdit smart_newline(const std::string& lang, const std::string& before,
                          const std::string& after, int indent_width) {
    NewlineEdit r;
    const std::string indent = leading_blanks(before);

    if (lang == "Markdown") {
        std::string rest = before.substr(indent.size());
        std::string marker;      // text to repeat on the next line
        size_t content_at = 0;   // where the item's content starts in `rest`
        if (!rest.empty() && (rest[0] == '-' || rest[0] == '*' || rest[0] == '+') &&
            rest.size() > 1 && rest[1] == ' ') {
            marker = rest.substr(0, 2);
            content_at = 2;
            if (rest.size() >= 6 && rest[2] == '[' && (rest[3] == ' ' || rest[3] == 'x' || rest[3] == 'X') &&
                rest[4] == ']' && rest[5] == ' ') {
                marker += "[ ] ";
                content_at = 6;
            }
        } else if (!rest.empty() && std::isdigit(static_cast<unsigned char>(rest[0]))) {
            size_t k = 0;
            while (k < rest.size() && std::isdigit(static_cast<unsigned char>(rest[k])) && k < 9) ++k;
            if (k + 1 < rest.size() && (rest[k] == '.' || rest[k] == ')') && rest[k + 1] == ' ') {
                long num = std::atol(rest.substr(0, k).c_str()) + 1;
                marker = std::to_string(num) + rest.substr(k, 2);
                content_at = k + 2;
            }
        } else if (!rest.empty() && rest[0] == '>') {
            size_t k = 0;
            while (k < rest.size() && (rest[k] == '>' || rest[k] == ' ')) ++k;
            marker = rest.substr(0, k);
            if (marker.empty() || marker[marker.size() - 1] != ' ') marker += ' ';
            content_at = k;
        }
        if (!marker.empty()) {
            if (only_blanks(rest.substr(content_at))) {  // empty item: end the list
                r.delete_before = static_cast<int>(before.size());
                return r;
            }
            r.text = "\n" + indent + marker;
            return r;
        }
    }

    // trailing non-blank character before the cursor
    char last = 0;
    for (size_t i = before.size(); i > 0; --i) {
        if (!is_blank(before[i - 1]) && before[i - 1] != '\r') { last = before[i - 1]; break; }
    }
    bool extra = (last == '{' || last == '(' || last == '[');
    if (last == ':' && (lang == "Python" || lang == "Nim" || lang == "YAML")) extra = true;

    std::string unit = (indent.find('\t') != std::string::npos) ? std::string("\t")
                                                                 : std::string(indent_width > 0 ? indent_width : 4, ' ');
    r.text = "\n" + indent;
    if (extra) {
        r.text += unit;
        size_t a = 0;
        while (a < after.size() && is_blank(after[a])) ++a;
        char next = a < after.size() ? after[a] : 0;
        if ((last == '{' && next == '}') || (last == '(' && next == ')') || (last == '[' && next == ']')) {
            r.cursor_back = static_cast<int>(1 + indent.size());
            r.text += "\n" + indent;
        }
    }
    return r;
}

std::string indent_lines(const std::string& block, int width) {
    std::vector<std::string> v = split_lines(block);
    std::string pad(width > 0 ? width : 4, ' ');
    for (size_t i = 0; i < v.size(); ++i)
        if (!v[i].empty()) v[i] = pad + v[i];
    return join_lines(v);
}

std::string outdent_lines(const std::string& block, int width) {
    std::vector<std::string> v = split_lines(block);
    if (width <= 0) width = 4;
    for (size_t i = 0; i < v.size(); ++i) {
        std::string& l = v[i];
        if (!l.empty() && l[0] == '\t') { l.erase(0, 1); continue; }
        int k = 0;
        while (k < width && k < static_cast<int>(l.size()) && l[k] == ' ') ++k;
        l.erase(0, k);
    }
    return join_lines(v);
}

bool toggle_line_comments(const std::string& lang, const std::string& block, std::string& out, bool* all_commented) {
    const std::string p = Highlighter::line_comment_prefix(lang);
    if (p.empty()) return false;
    std::vector<std::string> v = split_lines(block);

    bool any = false, all = true;
    size_t min_indent = std::string::npos;
    for (size_t i = 0; i < v.size(); ++i) {
        if (only_blanks(v[i])) continue;
        any = true;
        size_t ind = leading_blanks(v[i]).size();
        if (ind < min_indent) min_indent = ind;
        if (v[i].compare(ind, p.size(), p) != 0) all = false;
    }
    if (!any) { out = block; if (all_commented) *all_commented = false; return true; }

    for (size_t i = 0; i < v.size(); ++i) {
        std::string& l = v[i];
        if (only_blanks(l)) continue;
        if (all) {
            size_t ind = leading_blanks(l).size();
            size_t len = p.size();
            if (ind + len < l.size() && l[ind + len] == ' ') ++len;
            l.erase(ind, len);
        } else {
            l.insert(min_indent, p + " ");
        }
    }
    out = join_lines(v);
    if (all_commented) *all_commented = all;
    return true;
}

}  // namespace EditHelpers
