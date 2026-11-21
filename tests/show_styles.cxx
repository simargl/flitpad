// Debug tool: prints every line of a file followed by its style letters.
//   ./show_styles <Language> <file>
#include "../Highlighter.h"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc < 3) { std::cerr << "usage: show_styles <language> <file>\n"; return 1; }
    std::ifstream f(argv[2], std::ios::binary);
    std::stringstream ss; ss << f.rdbuf();
    std::string text = ss.str();
    std::string st = Highlighter::highlight(argv[1], text);
    if (argc > 3 && std::string(argv[3]) == "--runs") {   // print "text<STYLE>" runs
        size_t i = 0;
        while (i < text.size()) {
            size_t j = i;
            while (j < text.size() && st[j] == st[i]) ++j;
            if (st[i] != 'A') std::cout << "[" << st[i] << ":" << text.substr(i, j - i) << "]";
            else std::cout << text.substr(i, j - i);
            i = j;
        }
        std::cout << "\n";
        return 0;
    }
    size_t b = 0;
    while (b <= text.size()) {
        size_t e = text.find('\n', b);
        if (e == std::string::npos) e = text.size();
        std::cout << text.substr(b, e - b) << "\n" << st.substr(b, e - b) << "\n";
        if (e >= text.size()) break;
        b = e + 1;
    }
}
