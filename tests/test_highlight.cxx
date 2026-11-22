// Unit tests for Highlighter and EditHelpers (no FLTK needed).  Run: make test
#include "../Highlighter.h"
#include "../EditHelpers.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

static int failures = 0, checks = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        ++checks;                                                                     \
        if (!(cond)) { ++failures; std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } \
    } while (0)

// style of the first occurrence of `needle` (checks the whole needle has one style)
static char style_of(const std::string& lang, const std::string& text, const std::string& needle, size_t nth = 0) {
    std::string st = Highlighter::highlight(lang, text);
    size_t pos = std::string::npos, from = 0;
    for (size_t k = 0; k <= nth; ++k) {
        pos = text.find(needle, from);
        if (pos == std::string::npos) { std::cerr << "needle not found: " << needle << "\n"; return '?'; }
        from = pos + 1;
    }
    char c = st[pos];
    for (size_t i = 0; i < needle.size(); ++i)
        if (st[pos + i] != c) return '?';
    return c;
}

using namespace Highlighter;

static void test_detection() {
    CHECK(detect_language("README.md", "FROM x <html") == "Markdown");     // extension beats content
    CHECK(detect_language("notes.txt", "") == "Plain Text");               // .txt used to be CMake
    CHECK(detect_language("CMakeLists.txt", "") == "CMake");
    CHECK(detect_language("/a/b/Makefile", "") == "Makefile");
    CHECK(detect_language("script", "#!/usr/bin/env python3\n") == "Python");
    CHECK(detect_language("script", "#!/bin/bash\n") == "Shell Script");
    CHECK(detect_language("noext", "SELECT 1 FROM t") == "Plain Text");    // no more SQL guessing
    CHECK(detect_language("x.CPP", "") == "C++");
    CHECK(detect_language("a.weird", "<html>") == "Plain Text");
}

static void test_markdown() {
    const std::string L = "Markdown";
    // apostrophes must not open strings
    CHECK(style_of(L, "Don't panic, it's fine\n", "panic, it") == 'A');
    CHECK(style_of(L, "# Title\n", "Title") == HEADING);
    CHECK(style_of(L, "###### Six\n", "######") == HEADING6);
    CHECK(style_of(L, "#hashtag\n", "#hashtag") == 'A');                   // not a heading
    CHECK(style_of(L, "####### seven\n", "seven") == 'A');
    CHECK(style_of(L, "a **bold** b\n", "bold") == BOLD);
    CHECK(style_of(L, "a *it* b\n", "it") == ITALIC);
    CHECK(style_of(L, "a _it_ b\n", "it") == ITALIC);
    CHECK(style_of(L, "snake_case_word\n", "case") == 'A');                // intraword underscores
    CHECK(style_of(L, "2 * 3 * 4\n", "3") == 'A');
    CHECK(style_of(L, "a `code *not em*` b\n", "code *not em*") == CODE);
    CHECK(style_of(L, "a ~~gone~~ b\n", "gone") == COMMENT);
    CHECK(style_of(L, "[text](http://x.y/a_(b))\n", "text") == LINK);
    CHECK(style_of(L, "[text](http://x.y/a_(b))\n", "http://x.y/a_(b)") == URL);
    CHECK(style_of(L, "[text](http://x.y/a_(b))\n", "[") == LINK);
    CHECK(style_of(L, "![alt](i.png)\n", "alt") == VARIABLE);
    CHECK(style_of(L, "see https://example.com/x, ok\n", "https://example.com/x") == URL);
    CHECK(style_of(L, "<https://a.b/c>\n", "https://a.b/c") == URL);
    CHECK(style_of(L, "> quote\n", "quote") == QUOTE);
    CHECK(style_of(L, "- item\n", "-") == MARKUP);
    CHECK(style_of(L, "1. item\n", "1.") == MARKUP);
    CHECK(style_of(L, "- [x] done\n", "[x]") == SPECIAL);
    CHECK(style_of(L, "- [ ] todo\n", "[ ]") == MARKUP);
    CHECK(style_of(L, "---\n", "---") == MARKUP);                          // hr (no front matter: single line)
    CHECK(style_of(L, "Title\n=====\n", "Title") == HEADING);              // setext
    CHECK(style_of(L, "| a | b |\n|---|---|\n| c | d |\n", "|---|---|") == MARKUP);
    CHECK(style_of(L, "<!-- c -->\n", "c") == COMMENT);
    CHECK(style_of(L, "x\n<!-- a\nb -->\ny\n", "b") == COMMENT);           // multi-line comment
    CHECK(style_of(L, "[id]: http://x\n", "id") == LINK);
    CHECK(style_of(L, "[id]: http://x\n", "http://x") == URL);
    // one colour per heading level
    CHECK(style_of(L, "# a\n", "a") == HEADING);
    CHECK(style_of(L, "## a\n", "a") == HEADING2);
    CHECK(style_of(L, "### a\n", "a") == HEADING3);
    CHECK(style_of(L, "#### a\n", "a") == HEADING4);
    CHECK(style_of(L, "##### a\n", "a") == HEADING5);
    CHECK(style_of(L, "###### a\n", "a") == HEADING6);
    CHECK(style_of(L, "Sub\n---\n", "Sub") == HEADING2);       // setext level 2
    CHECK(style_of(L, "## a *em* b\n", "em") == ITALIC);        // inline styles still win inside headings

    // fenced blocks: language-aware, unterminated, and not leaking out
    CHECK(style_of(L, "```cpp\nint x = 1;\n```\n", "int") == TYPENAME);
    CHECK(style_of(L, "```cpp\nint x = 1;\n```\n", "1") == NUMBER);
    CHECK(style_of(L, "```cpp\nreturn x;\n```\n", "return") == KEYWORD);
    CHECK(style_of(L, "```\nDon't *x*\n```\n", "Don't *x*") == CODE);
    CHECK(style_of(L, "```py\nx = 1\n```\n*em*\n", "em") == ITALIC);
    CHECK(style_of(L, "```\nunterminated *x*\n", "unterminated") == CODE);
    CHECK(style_of(L, "~~~js\nlet a;\n~~~\n", "let") == KEYWORD);
    CHECK(style_of(L, "````\n```\nstill code\n````\n", "still code") == CODE);

    // front matter
    CHECK(style_of(L, "---\ntitle: x\n---\n# H\n", "title") == TYPE);
    CHECK(style_of(L, "---\ntitle: x\n---\n# H\n", "H") == HEADING);

    // CRLF input shouldn't confuse line handling
    CHECK(style_of(L, "# Head\r\ntext *em*\r\n", "em") == ITALIC);
}

static void test_code_languages() {
    // '--' is a comment only where it should be (this was a bug for every language)
    CHECK(style_of("JavaScript", "for (;;) { i--; x = 1; }", "x") == 'A');
    CHECK(style_of("JavaScript", "i--; // c", "// c") == COMMENT);
    CHECK(style_of("SQL", "select 1 -- c", "-- c") == COMMENT);
    CHECK(style_of("C++", "int a; /* c\nd */ int b;", "d") == COMMENT);
    CHECK(style_of("C++", "int a; /* c\nd */ int b;", "b") == 'A');
    CHECK(style_of("C++", "#include <vector>\n", "#include") == PREPROC);
    CHECK(style_of("C++", "#include <vector>\n", "<vector>") == STRING);
    CHECK(style_of("C++", "foo(1);", "foo") == TYPE);
    CHECK(style_of("C++", "if (x) {}", "if") == KEYWORD);
    // an unterminated string must stop at the end of the line
    CHECK(style_of("C++", "char c = 'x;\nint y = 2;\n", "y") == 'A');
    CHECK(style_of("C++", "char c = 'x;\nint y = 2;\n", "2") == NUMBER);
    CHECK(style_of("Python", "s = \"\"\"a\nb\"\"\"\nx = 1", "b") == STRING);
    CHECK(style_of("Python", "x = 1 # it's", "# it's") == COMMENT);
    CHECK(style_of("Python", "def foo(): pass", "foo") == TYPE);
    CHECK(style_of("Python", "@dec\ndef f(): pass", "@dec") == PREPROC);
    CHECK(style_of("Rust", "fn f<'a>(x: &'a str) {}", "'a", 0) == 'A');   // lifetime, not a char
    CHECK(style_of("Rust", "let c = 'x';", "'x'") == STRING);
    CHECK(style_of("Rust", "println!(\"hi\");", "println!") == TYPE);
    CHECK(style_of("Shell Script", "echo \"$HOME\"", "$HOME") == VARIABLE);
    CHECK(style_of("Shell Script", "echo $#", "$#") == VARIABLE);
    CHECK(style_of("Shell Script", "echo a#b", "#b") == 'A');              // not a comment mid-word
    CHECK(style_of("Shell Script", "x # c", "# c") == COMMENT);
    CHECK(style_of("Makefile", "CXX ?= g++\n", "CXX") == VARIABLE);
    CHECK(style_of("Makefile", "all: $(OBJS)\n", "all") == TYPE);
    CHECK(style_of("Makefile", "all: $(OBJS)\n", "$(OBJS)") == VARIABLE);
    CHECK(style_of("Makefile", "\t$(CXX) -o $@ $<\n", "$@") == VARIABLE);
    CHECK(style_of("CMake", "IF(X)\nENDIF()\n", "IF") == KEYWORD);          // case-insensitive
    CHECK(style_of("JSON", "{\"k\": \"v\", \"n\": 1}", "\"k\"") == TYPE);
    CHECK(style_of("JSON", "{\"k\": \"v\", \"n\": 1}", "\"v\"") == STRING);
    CHECK(style_of("JSON", "{\"k\": true}", "true") == NUMBER);
    CHECK(style_of("CSS", "@media (a: 1px) { .x:hover { color: #fff; } }", ".x") == TYPE);
    CHECK(style_of("CSS", "@media (a: 1px) { .x:hover { color: #fff; } }", "color") == TYPE);
    CHECK(style_of("CSS", "@media (a: 1px) { .x:hover { color: #fff; } }", "#fff") == NUMBER);
    CHECK(style_of("HTML", "<div class=\"a\">x</div>", "div") == KEYWORD);
    CHECK(style_of("HTML", "<div class=\"a\">x</div>", "class") == VARIABLE);
    CHECK(style_of("HTML", "<div class=\"a\">x</div>", "\"a\"") == STRING);
    CHECK(style_of("HTML", "<script>var a = 1;</script>", "var") == KEYWORD);
    CHECK(style_of("HTML", "<style>p{color:red}</style>", "color") == TYPE);
    CHECK(style_of("HTML", "<!-- c -->", "c") == COMMENT);
    CHECK(style_of("YAML", "key: 'v' # c\n", "key") == TYPE);
    CHECK(style_of("YAML", "key: 'v' # c\n", "# c") == COMMENT);
    CHECK(style_of("YAML", "a: |\n  # not comment\nb: 1\n", "# not comment") == STRING);
    CHECK(style_of("YAML", "a: |\n  # not comment\nb: 1\n", "b") == TYPE);
    CHECK(style_of("TOML", "[tool.x]\nk = 1\n", "[tool.x]") == KEYWORD);
    CHECK(style_of("TOML", "[tool.x]\nk = 1\n", "1") == NUMBER);
    CHECK(style_of("Plain Text", "if (x) // y", "if") == 'A');

    // operators are green in the languages where that reads well, plain elsewhere
    CHECK(style_of("C++", "a = b + c;", "=") == SPECIAL);
    CHECK(style_of("C++", "a = b + c;", "+") == SPECIAL);
    CHECK(style_of("C++", "p->x", "->") == SPECIAL);
    CHECK(style_of("Python", "x = 1", "=") == SPECIAL);
    CHECK(style_of("JavaScript", "a => b", "=>") == SPECIAL);
    CHECK(style_of("C++", "a = b + c;", "b") == 'A');
    CHECK(style_of("Shell Script", "a=b", "=") == 'A');
    CHECK(style_of("Makefile", "A = b\n", "=") == 'A');
    CHECK(style_of("C++", "a /* c */ + b", "/* c */") == COMMENT);     // comments still win
    CHECK(style_of("C++", "x = 1.5e3;", "1.5e3") == NUMBER);

    // solarized-dark roles: statement orange, constant cyan, type yellow, identifier blue
    CHECK(style_of("C++", "int x;", "int") == TYPENAME);
    CHECK(style_of("C++", "return x;", "return") == KEYWORD);
    CHECK(style_of("C++", "x = NULL;", "NULL") == NUMBER);
    CHECK(style_of("C++", "x = true;", "true") == NUMBER);
    CHECK(style_of("C++", "class Foo : public Bar {};", "Foo") == TYPENAME);
    CHECK(style_of("C++", "class Foo : public Bar {};", "public") == KEYWORD);
    CHECK(style_of("C++", "struct stat st;", "stat") == TYPENAME);
    CHECK(style_of("C++", "x = FOO_BAR;", "FOO_BAR") == 'A');                // no ALL_CAPS guessing
    CHECK(style_of("C++", "x = \"a\\nb\";", "\\n") == SPECIAL);          // escapes are green
    CHECK(style_of("C++", "x = \"a\\nb\";", "a") == STRING);
    CHECK(style_of("Python", "x = None", "None") == NUMBER);
    CHECK(style_of("Python", "class Foo: pass", "Foo") == TYPENAME);
    CHECK(style_of("Python", "print(x)", "print") == TYPE);                   // builtin function: blue
    CHECK(style_of("Python", "x: int = 1", "int") == TYPENAME);
    CHECK(style_of("JavaScript", "class A extends B {}", "A") == TYPENAME);
    CHECK(style_of("JavaScript", "function go() {}", "go") == TYPE);
    CHECK(style_of("JavaScript", "x = undefined;", "undefined") == NUMBER);
    CHECK(style_of("Rust", "struct Point;", "Point") == TYPENAME);
    CHECK(style_of("Rust", "fn main() {}", "main") == TYPE);
    CHECK(style_of("Go", "func main() {}", "main") == TYPE);
    CHECK(style_of("Go", "type Foo struct{}", "Foo") == TYPENAME);
    CHECK(style_of("SQL", "SELECT NULL", "NULL") == NUMBER);
    CHECK(style_of("SQL", "SELECT NULL", "SELECT") == KEYWORD);
    CHECK(style_of("Shell Script", "echo hi", "echo") == KEYWORD);
    CHECK(style_of("JSON", "{\"a\": \"x\\ty\"}", "\\t") == SPECIAL);
}

static char twin(char c) { return static_cast<char>(c + ICON_STYLE_OFFSET); }

static void test_icons() {
    const std::string fa = "\xef\x80\x95";             // U+F015 (FontAwesome "home")
    const std::string nf = "\xf3\xb0\x80\x81";        // U+F0001 (Nerd Fonts, supplementary private use)
    const char P = twin(PLAIN);
    std::string st = highlight("Plain Text", "a" + fa + "b");
    CHECK(st == std::string("A") + std::string(3, P) + "A");         // plain -> its icon twin
    st = highlight("Plain Text", nf + "x");
    CHECK(st == std::string(4, P) + "A");
    st = highlight("C++", "// " + fa + "\nint x;");
    CHECK(st.substr(3, 3) == std::string(3, twin(COMMENT)));         // comment colour is kept
    CHECK(st[0] == COMMENT && st[2] == COMMENT);
    st = highlight("Markdown", "# T " + fa + "\n");
    CHECK(st.substr(4, 3) == std::string(3, twin(HEADING)));
    st = highlight("Python", "s = \"" + fa + "\"");
    CHECK(st.substr(5, 3) == std::string(3, twin(STRING)) && st[4] == STRING);
    // not private use: U+E000-F8FF only; EF A4 80 is U+F900 (CJK compatibility)
    CHECK(highlight("Plain Text", "\xef\xa4\x80") == "AAA");
    CHECK(highlight("Plain Text", "\xef\xa3\xbf") == std::string(3, P));   // U+F8FF is the last private-use char
    CHECK(highlight("Plain Text", "\xee\x80\x80") == std::string(3, P));   // U+E000 is the first
    CHECK(highlight("Plain Text", "\xe4\xb8\xad") == "AAA");        // ordinary CJK untouched
    CHECK(highlight("Plain Text", "caf\xc3\xa9") == "AAAAA");        // so is accented text
    CHECK(highlight("Plain Text", "\xef\x80") == "AA");              // truncated sequence: no overrun
    CHECK(highlight("Plain Text", "\xf3\xb0\x80") == "AAA");
    CHECK(highlight("Plain Text", "\xf3\xa0\x80\x80") == "AAAA");  // F3 A0 = U+E0000 tags, not private use
    CHECK(ICON_STYLE_OFFSET == STYLE_COUNT && TOTAL_STYLE_COUNT == 2 * STYLE_COUNT);
}

static void test_robustness() {
    const char* langs[] = {"C", "C++", "C/C++", "JavaScript", "TypeScript", "Java", "C#", "Go", "Rust", "Python", "Nim",
                           "PHP", "SQL", "Shell Script", "Makefile", "CMake", "JSON", "CSS", "HTML", "XML/SVG", "YAML",
                           "TOML", "INI", "Markdown", "Diff", "Plain Text", "Unknown"};
    // fixed nasty inputs
    const char* nasty[] = {"", "\n", "`", "```", "```\n", "[", "[](", "![", "**", "*", "_", "<", "<!--", "<a", "<a href=\"",
                           "\"", "'", "\"\"\"", "/*", "$(", "${", "$", "#[[", "|", "|-|\n|", "---\n", "---\na: b", "> >", "1)",
                           "- [ ", "\xef\x80", "\xef", "\xf3\xb0\x80", "\xee\x80\x80\xef", "{", "}", "@", "@media", "\xff\xfe", "a\\", "\\", "<script>", "<style>", "a: |\n"};
    for (size_t l = 0; l < sizeof(langs) / sizeof(*langs); ++l)
        for (size_t k = 0; k < sizeof(nasty) / sizeof(*nasty); ++k) {
            std::string t = nasty[k];
            CHECK(highlight(langs[l], t).size() == t.size());
        }
    // random garbage from a markup-heavy alphabet
    const char alphabet[] = "ab *_`~[]()<>!#-+=|:;\"'\\/{}$@\n\t 019.%&";
    unsigned seed = 12345;
    for (int round = 0; round < 400; ++round) {
        std::string t;
        int len = 1 + static_cast<int>(std::rand() % 200);
        for (int i = 0; i < len; ++i) {
            seed = seed * 1103515245u + 12345u;
            t += alphabet[(seed >> 16) % (sizeof(alphabet) - 1)];
        }
        for (size_t l = 0; l < sizeof(langs) / sizeof(*langs); ++l) {
            std::string st = highlight(langs[l], t);
            CHECK(st.size() == t.size());
            bool ok = true;
            for (size_t i = 0; i < st.size(); ++i)
                if (st[i] < 'A' || st[i] >= 'A' + TOTAL_STYLE_COUNT) ok = false;
            CHECK(ok);
        }
    }
    // big input stays fast and correct
    std::string big;
    for (int i = 0; i < 20000; ++i) big += "# h *x* `c` [l](u) text\n- item **b**\n";
    CHECK(highlight("Markdown", big).size() == big.size());
    std::string huge(MAX_HIGHLIGHT_BYTES + 1, 'x');
    CHECK(highlight("C++", huge) == std::string(huge.size(), 'A'));
}

static void test_edit_helpers() {
    using namespace EditHelpers;
    NewlineEdit e;

    e = smart_newline("C++", "    if (x) {", "", 4);
    CHECK(e.text == "\n        " && e.cursor_back == 0 && e.delete_before == 0);
    e = smart_newline("C++", "    foo();", "", 4);
    CHECK(e.text == "\n    ");
    e = smart_newline("C++", "    f({", ")", 4);
    CHECK(e.text == "\n        " && e.cursor_back == 0);                  // no split unless the closer is adjacent
    e = smart_newline("C++", "  if (x) {", "}", 4);
    CHECK(e.text == "\n      \n  " && e.cursor_back == 3);
    e = smart_newline("Python", "def f():", "", 4);
    CHECK(e.text == "\n    ");
    e = smart_newline("C++", "x:", "", 4);
    CHECK(e.text == "\n");                                               // ':' only matters in python-like langs
    e = smart_newline("C++", "\tfoo {", "", 4);
    CHECK(e.text == "\n\t\t");                                           // tabs stay tabs

    e = smart_newline("Markdown", "- item", "", 4);
    CHECK(e.text == "\n- ");
    e = smart_newline("Markdown", "  * item", "", 4);
    CHECK(e.text == "\n  * ");
    e = smart_newline("Markdown", "9. item", "", 4);
    CHECK(e.text == "\n10. ");
    e = smart_newline("Markdown", "- [x] done", "", 4);
    CHECK(e.text == "\n- [ ] ");
    e = smart_newline("Markdown", "> quote", "", 4);
    CHECK(e.text == "\n> ");
    e = smart_newline("Markdown", "- ", "", 4);
    CHECK(e.text.empty() && e.delete_before == 2);                       // empty item ends the list
    e = smart_newline("Markdown", "  1. ", "", 4);
    CHECK(e.text.empty() && e.delete_before == 5);
    e = smart_newline("Markdown", "plain paragraph", "", 4);
    CHECK(e.text == "\n");
    e = smart_newline("Markdown", "-not a list", "", 4);
    CHECK(e.text == "\n");

    CHECK(indent_lines("a\n\nb", 4) == "    a\n\n    b");
    CHECK(outdent_lines("    a\n  b\n\tc\nd", 4) == "a\nb\nc\nd");

    std::string out;
    bool all = false;
    CHECK(toggle_line_comments("C++", "a\n  b", out, &all));
    CHECK(out == "// a\n//   b" && !all);                                // comments at the minimum indent
    CHECK(toggle_line_comments("C++", out, out, &all) && out == "a\n  b" && all);
    CHECK(toggle_line_comments("Python", "x\n\ny", out) && out == "# x\n\n# y");
    CHECK(toggle_line_comments("Python", "# x\ny", out, &all) && out == "# # x\n# y" && !all);
    CHECK(toggle_line_comments("SQL", "select 1", out) && out == "-- select 1");
    CHECK(!toggle_line_comments("Markdown", "x", out));
    CHECK(!toggle_line_comments("JSON", "x", out));
    CHECK(Highlighter::line_comment_prefix("Makefile") == "#");
}

int main() {
    test_detection();
    test_markdown();
    test_code_languages();
    test_icons();
    test_robustness();
    test_edit_helpers();
    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures ? 1 : 0;
}
