// Tests ColorSchemes.  Run: make test
#include "../ColorSchemes.h"
#include "../ColorMath.h"
#include "../Highlighter.h"

#include <iostream>
#include <set>
#include <string>

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

int main() {
    using namespace ColorSchemes;
    const std::vector<const Scheme*>& v = all();
    CHECK(v.size() >= 6);

    std::set<std::string> ids, names;
    int dark = 0, light = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        const Scheme& s = *v[i];
        CHECK(ids.insert(s.id).second);                     // unique ids
        CHECK(names.insert(s.name).second);
        CHECK(find(s.id) == &s);
        (s.dark ? dark : light)++;
        // dark schemes have dark backgrounds and vice versa
        CHECK(s.dark == (ColorMath::luminance(s.bg) < 128));
        CHECK(s.dark == (ColorMath::luminance(s.text) > ColorMath::luminance(s.bg)));

        const std::vector<StyleSpec> specs = style_specs(s);
        CHECK(static_cast<int>(specs.size()) == Highlighter::STYLE_COUNT);
        for (size_t k = 0; k < specs.size(); ++k) {
            int diff = ColorMath::luminance(specs[k].rgb) - ColorMath::luminance(s.bg);
            if (diff < 0) diff = -diff;
            CHECK(diff > 105);                              // FLTK would otherwise draw it white/black
            CHECK(specs[k].rgb <= 0xffffff);
        }
        // the roles really differ from each other in every scheme
        CHECK(specs[Highlighter::KEYWORD - 'A'].rgb != specs[Highlighter::STRING - 'A'].rgb);
        CHECK(specs[Highlighter::STRING - 'A'].rgb != specs[Highlighter::TYPE - 'A'].rgb);
        CHECK(specs[Highlighter::TYPE - 'A'].rgb != specs[Highlighter::VARIABLE - 'A'].rgb);
        CHECK(specs[Highlighter::COMMENT - 'A'].rgb != specs[Highlighter::PLAIN - 'A'].rgb);
        CHECK(specs[Highlighter::HEADING - 'A'].bold && specs[Highlighter::BOLD - 'A'].bold);
        CHECK(specs[Highlighter::ITALIC - 'A'].italic);

        const unsigned g = gutter_fg(s);
        int gd = ColorMath::luminance(g) - ColorMath::luminance(s.gutter_bg);
        if (gd < 0) gd = -gd;
        CHECK(gd > 99);
    }
    CHECK(dark >= 3 && light >= 2);
    CHECK(find("nope") == nullptr && find("") == nullptr);
    CHECK(fallback(true) != nullptr && fallback(true)->dark && std::string(fallback(true)->id) == "monokai");
    CHECK(fallback(false) != nullptr && !fallback(false)->dark);

    // Oblivion is taken from the supplied scheme file
    const Scheme* o = find("oblivion");
    CHECK(o && o->bg == 0x2e3436 && o->text == 0xd3d7cf && o->selection == 0x888a85 && o->gutter_bg == 0x000000);
    CHECK(o && o->string == 0xedd400 && o->identifier == 0x729fcf && o->type == 0x8ae234 && o->preproc == 0xad7fa8);
    CHECK(o && o->keyword == 0xffffff && o->keyword_bold && o->type_bold && o->special == 0xce5c00);
    // Monokai, the default dark scheme
    const Scheme* m = find("monokai");
    CHECK(m && m->bg == 0x272822 && m->text == 0xf8f8f2 && m->keyword == 0xf92672 && m->string == 0xe6db74 && m->number == 0xae81ff);
    CHECK(m && m->type == 0x66d9ef && m->type_italic && m->identifier == 0xa6e22e);
    // bright colours are not touched, dull ones are moved in the right direction
    const std::vector<StyleSpec> ms = style_specs(*m);
    CHECK(ms[Highlighter::STRING - 'A'].rgb == 0xe6db74);
    CHECK(ms[Highlighter::KEYWORD - 'A'].rgb == 0xf92672 || ms[Highlighter::KEYWORD - 'A'].rgb > 0xf92672);
    const std::vector<StyleSpec> ls = style_specs(*find("solarized-light"));
    CHECK(ColorMath::luminance(ls[Highlighter::COMMENT - 'A'].rgb) < ColorMath::luminance(0x93a1a1));   // darkened on light

    std::cout << (checks - failures) << "/" << checks << " scheme checks passed\n";
    return failures ? 1 : 0;
}
