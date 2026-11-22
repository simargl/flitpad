#include "IconFont.h"

#include <cctype>

namespace IconFont {
namespace {

// lower-case, only letters and digits: "Font Awesome 6 Free" -> "fontawesome6free"
std::string key(const std::string& s) {
    std::string k;
    for (size_t i = 0; i < s.size(); ++i)
        if (std::isalnum(static_cast<unsigned char>(s[i]))) k += static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    return k;
}

bool has(const std::string& k, const char* part) { return k.find(part) != std::string::npos; }

}  // namespace

int score(const std::string& family, int attributes, const std::string& wanted) {
    const bool bold = (attributes & 1) != 0, italic = (attributes & 2) != 0;
    const std::string k = key(family);
    if (k.empty()) return 0;

    if (!wanted.empty() && k == key(wanted)) return 1000 + (attributes == 0 ? 1 : 0);

    if (italic) return 0;

    if (has(k, "fontawesome")) {
        if (has(k, "brands")) return 40;
        if (k == "fontawesome") return bold ? 0 : 100;           // FontAwesome 4: one regular face
        return bold ? 95 : 90;                                    // 5/6/...: Solid (bold) has every free icon
    }
    if (bold) return 0;
    if (has(k, "symbolsnerdfont")) return 70;
    if (has(k, "nerdfont")) return 60;
    return 0;
}

}  // namespace IconFont
