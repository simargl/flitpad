#include "ColorSchemes.h"

#include "ColorMath.h"
#include "Highlighter.h"

namespace ColorSchemes {
namespace {

// Field order: id, name, dark | bg, text, selection, cursor, gutter_bg, gutter_fg |
// comment, string, number, keyword, identifier, type, preproc, special, op |
// keyword_bold, type_bold, type_italic, comment_italic |
// red, orange, yellow, green, cyan, blue, violet, magenta
const Scheme kSchemes[] = {
    // the default dark scheme
    {"monokai", "Monokai", true,
     0x272822, 0xf8f8f2, 0x49483e, 0xf8f8f0, 0x272822, 0x8f908a,
     0x75715e, 0xe6db74, 0xae81ff, 0xf92672, 0xa6e22e, 0x66d9ef, 0xf92672, 0xae81ff, 0xf92672,
     false, false, true, false,
     0xf92672, 0xfd971f, 0xe6db74, 0xa6e22e, 0x66d9ef, 0x66d9ef, 0xae81ff, 0xf92672},

    // Oblivion (Tango palette)
    {"oblivion", "Oblivion", true,
     0x2e3436, 0xd3d7cf, 0x888a85, 0xd3d7cf, 0x000000, 0x555753,
     0x888a85, 0xedd400, 0xedd400, 0xffffff, 0x729fcf, 0x8ae234, 0xad7fa8, 0xce5c00, 0xd3d7cf,
     true, true, false, false,
     0xef2929, 0xfcaf3e, 0xedd400, 0x8ae234, 0xe9b96e, 0x729fcf, 0xad7fa8, 0xfce94f},

    // Solarized Dark
    {"solarized-dark", "Solarized Dark", true,
     0x002b36, 0x93a1a1, 0x657b83, 0x93a1a1, 0x073642, 0x586e75,
     0x586e75, 0x2aa198, 0xd33682, 0xcb4b16, 0x268bd2, 0xb58900, 0x6c71c4, 0x859900, 0x859900,
     false, false, false, true,
     0xdc322f, 0xcb4b16, 0xb58900, 0x859900, 0x2aa198, 0x268bd2, 0x6c71c4, 0xd33682},

    {"tomorrow-night", "Tomorrow Night", true,
     0x1d1f21, 0xc5c8c6, 0x373b41, 0xaeafad, 0x1d1f21, 0x969896,
     0x969896, 0xb5bd68, 0xde935f, 0xb294bb, 0x81a2be, 0xf0c674, 0x8abeb7, 0x8abeb7, 0x8abeb7,
     false, false, false, false,
     0xcc6666, 0xde935f, 0xf0c674, 0xb5bd68, 0x8abeb7, 0x81a2be, 0xb294bb, 0xcc6666},

    {"solarized-light", "Solarized Light", false,
     0xfdf6e3, 0x657b83, 0xeee8d5, 0x586e75, 0xeee8d5, 0x93a1a1,
     0x93a1a1, 0x2aa198, 0xd33682, 0xcb4b16, 0x268bd2, 0xb58900, 0x6c71c4, 0x859900, 0x859900,
     false, false, false, true,
     0xdc322f, 0xcb4b16, 0xb58900, 0x859900, 0x2aa198, 0x268bd2, 0x6c71c4, 0xd33682},

    {"tomorrow", "Tomorrow", false,
     0xffffff, 0x4d4d4c, 0xd6d6d6, 0xaeafad, 0xefefef, 0x8e908c,
     0x8e908c, 0x718c00, 0xf5871f, 0x8959a8, 0x4271ae, 0xeab700, 0x3e999f, 0x3e999f, 0x3e999f,
     false, false, false, false,
     0xc82829, 0xf5871f, 0xeab700, 0x718c00, 0x3e999f, 0x4271ae, 0x8959a8, 0xc82829},
};

const int kMinDiff = 112;          // FLTK keeps colours whose luminance differs by > ~99
const int kCommentMinDiff = 108;   // comments are meant to stay quiet: use the smallest margin

}  // namespace

const std::vector<const Scheme*>& all() {
    static const std::vector<const Scheme*> v = [] {
        std::vector<const Scheme*> out;
        for (size_t i = 0; i < sizeof(kSchemes) / sizeof(*kSchemes); ++i) out.push_back(&kSchemes[i]);
        return out;
    }();
    return v;
}

const Scheme* find(const std::string& id) {
    const std::vector<const Scheme*>& v = all();
    for (size_t i = 0; i < v.size(); ++i)
        if (id == v[i]->id) return v[i];
    return nullptr;
}

const Scheme* fallback(bool dark) { return find(dark ? "monokai" : "solarized-light"); }

unsigned gutter_fg(const Scheme& s) { return ColorMath::ensure_contrast(s.gutter_fg, s.gutter_bg, 106); }

std::vector<StyleSpec> style_specs(const Scheme& s) {
    std::vector<StyleSpec> v(Highlighter::STYLE_COUNT);
    for (size_t i = 0; i < v.size(); ++i) {
        v[i].rgb = ColorMath::ensure_contrast(s.text, s.bg, kMinDiff);
        v[i].bold = v[i].italic = false;
    }
    using namespace Highlighter;
    // role colour on this scheme's background
    struct Setter {
        std::vector<StyleSpec>& v;
        const Scheme& s;
        void set(char style, unsigned rgb, bool bold = false, bool italic = false, int min_diff = kMinDiff) {
            StyleSpec& e = v[static_cast<size_t>(style - 'A')];
            e.rgb = ColorMath::ensure_contrast(rgb, s.bg, min_diff);
            e.bold = bold;
            e.italic = italic;
        }
    } z = {v, s};

    z.set(KEYWORD, s.keyword, s.keyword_bold);
    z.set(COMMENT, s.comment, false, s.comment_italic, kCommentMinDiff);
    z.set(STRING, s.string);
    z.set(NUMBER, s.number);
    z.set(TYPE, s.identifier);
    z.set(VARIABLE, s.type, s.type_bold, s.type_italic);
    z.set(PREPROC, s.preproc);
    z.set(SPECIAL, s.special);
    z.set(OPERATOR, s.op);

    // Markdown: one colour per heading level, the rest from the scheme's accents
    z.set(HEADING, s.orange, true);
    z.set(HEADING2, s.yellow, true);
    z.set(HEADING3, s.green, true);
    z.set(HEADING4, s.cyan, true);
    z.set(HEADING5, s.blue, true);
    z.set(HEADING6, s.violet, true);
    z.set(BOLD, s.magenta, true);
    z.set(ITALIC, s.green, false, true);
    z.set(CODE, s.yellow);
    z.set(LINK, s.blue);
    z.set(URL, s.cyan);
    z.set(MARKUP, s.red);
    z.set(QUOTE, s.violet);
    return v;
}

}  // namespace ColorSchemes
