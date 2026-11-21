#include "Highlighter.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Highlighter {
namespace {

typedef std::unordered_set<std::string> WordSet;

// ---------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------

WordSet words(const char* list) {
    WordSet s;
    std::string cur;
    for (const char* p = list;; ++p) {
        if (*p == ' ' || *p == '\0') {
            if (!cur.empty()) { s.insert(cur); cur.clear(); }
            if (!*p) break;
        } else {
            cur += *p;
        }
    }
    return s;
}

inline bool is_digit(char c) { return c >= '0' && c <= '9'; }
inline bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
inline bool is_blank(char c) { return c == ' ' || c == '\t'; }
inline bool is_ident_start(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    return std::isalpha(u) || c == '_' || u >= 0x80;
}
inline bool is_ident_char(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '_' || u >= 0x80;
}

std::string lower(std::string s) {
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    return s;
}

void paint(std::string& st, size_t a, size_t b, char style) {
    if (b > st.size()) b = st.size();
    for (size_t i = a; i < b; ++i) st[i] = style;
}

bool match_at(const std::string& t, size_t i, const char* s) {
    size_t n = std::strlen(s);
    return i + n <= t.size() && t.compare(i, n, s) == 0;
}

size_t line_end(const std::string& t, size_t i) {
    size_t e = t.find('\n', i);
    return e == std::string::npos ? t.size() : e;
}

// ---------------------------------------------------------------------------
// table-driven tokenizer for "code-like" languages
// ---------------------------------------------------------------------------

struct LangDef {
    std::vector<std::string> line_comments;
    std::string block_open, block_close;
    bool sq = true;            // 'single quoted' strings
    bool dq = true;            // "double quoted" strings
    bool bt = false;           // `backtick` strings (multi-line)
    bool sq_raw = false;       // no backslash escapes inside '...'
    bool triple = false;       // python-style """ and '''
    bool rust_char = false;    // 'x' is a char literal, 'a is a lifetime
    bool preproc = false;      // C preprocessor directives
    bool nocase = false;       // case-insensitive keywords
    bool decorators = false;   // @name
    bool func_calls = true;    // ident( -> function colour
    bool camel_types = false;  // CapitalizedName -> type colour
    bool caps_const = true;    // ALL_CAPS -> constant colour
    bool bang_macros = false;  // rust name!(
    bool ident_dash = false;   // foo-bar is one word (shell, make, cmake)
    bool hash_needs_space = true;  // '#' starts a comment only after whitespace
    bool makefile = false;
    int dollar = 0;            // 1 shell/cmake, 2 make, 3 php
    WordSet kw, ty, def_intro;
};

size_t scan_shell_var(const std::string& t, size_t i) {
    // t[i] == '$'. Returns end of the variable reference.
    size_t n = t.size(), j = i + 1;
    if (j >= n) return j;
    if (t[j] == '{') {
        size_t e = t.find('}', j);
        size_t nl = line_end(t, j);
        return (e != std::string::npos && e < nl) ? e + 1 : j + 1;
    }
    if (t[j] == '(') return j + 1;  // command substitution: only mark "$("
    if (is_ident_start(t[j])) {
        while (j < n && is_ident_char(t[j])) ++j;
        return j;
    }
    if (is_digit(t[j]) || (t[j] != '\0' && std::strchr("@*#?$!-", t[j]))) return j + 1;
    return j;
}

size_t scan_make_var(const std::string& t, size_t i) {
    size_t n = t.size(), j = i + 1;
    if (j >= n) return j;
    if (t[j] == '(' || t[j] == '{') {
        char open = t[j], close = (open == '(') ? ')' : '}';
        int depth = 1;
        ++j;
        while (j < n && depth > 0 && t[j] != '\n') {
            if (t[j] == open) ++depth;
            else if (t[j] == close) --depth;
            ++j;
        }
        return j;
    }
    if (t[j] == '$') {
        ++j;
        while (j < n && is_ident_char(t[j])) ++j;
        return j;
    }
    if (t[j] != '\0' && std::strchr("@<^?*%+|", t[j])) return j + 1;
    if (is_ident_char(t[j])) return j + 1;
    return j;
}

// Makefile: colour "target:" and "VAR =" at the start of a line.
void make_line_prefix(const std::string& t, size_t b, std::string& st, size_t& resume) {
    size_t n = t.size(), j = b;
    while (j < n) {
        char c = t[j];
        if (c == '\n' || c == '#') return;
        if (c == '=') {  // VAR = value
            size_t k = j;
            while (k > b && is_blank(t[k - 1])) --k;
            if (k > b && std::strchr("+?:!", t[k - 1])) --k;
            while (k > b && is_blank(t[k - 1])) --k;
            paint(st, b, k, 'G');
            resume = j;
            return;
        }
        if (c == ':') {
            if (j + 1 < n && (t[j + 1] == '=' || (t[j + 1] == ':' && j + 2 < n && t[j + 2] == '='))) {
                size_t k = j;
                while (k > b && is_blank(t[k - 1])) --k;
                paint(st, b, k, 'G');
                resume = j;
                return;
            }
            size_t k = j;
            while (k > b && is_blank(t[k - 1])) --k;
            paint(st, b, k, 'F');
            resume = j;
            return;
        }
        if (!(is_ident_char(c) || is_blank(c) || std::strchr(".%/-$(){}@+?!", c))) return;
        ++j;
    }
}

std::string highlight_generic(const LangDef& d, const std::string& t) {
    const size_t n = t.size();
    std::string st(n, PLAIN);
    size_t i = 0, line_begin = 0;
    bool line_start = true;
    bool expect_def = false;

    while (i < n) {
        char c = t[i];
        if (c == '\n') { line_start = true; line_begin = i + 1; expect_def = false; ++i; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { ++i; continue; }
        const bool at_line_start = line_start;
        line_start = false;

        if (d.makefile && at_line_start && t[line_begin] != '\t' && c != '#') {
            size_t resume = i;
            make_line_prefix(t, i, st, resume);
            if (resume > i) { i = resume; continue; }
        }

        if (!is_ident_start(c)) expect_def = false;

        // block comment
        if (!d.block_open.empty() && match_at(t, i, d.block_open.c_str())) {
            size_t e = t.find(d.block_close, i + d.block_open.size());
            size_t end = (e == std::string::npos) ? n : e + d.block_close.size();
            paint(st, i, end, COMMENT);
            i = end;
            continue;
        }

        // line comment
        bool is_comment = false;
        for (size_t k = 0; k < d.line_comments.size(); ++k) {
            const std::string& lc = d.line_comments[k];
            if (!match_at(t, i, lc.c_str())) continue;
            if (lc == "#" && d.hash_needs_space && i > 0 && !is_space(t[i - 1]) && t[i - 1] != ';' && t[i - 1] != '(') continue;
            is_comment = true;
            break;
        }
        if (is_comment) {
            size_t e = line_end(t, i);
            paint(st, i, e, COMMENT);
            i = e;
            continue;
        }

        // C preprocessor
        if (d.preproc && c == '#' && at_line_start) {
            size_t j = i + 1;
            while (j < n && is_blank(t[j])) ++j;
            size_t k = j;
            while (k < n && is_ident_char(t[k])) ++k;
            paint(st, i, k, KEYWORD);
            std::string dir = t.substr(j, k - j);
            i = k;
            if (dir == "include" || dir == "import") {
                while (i < n && is_blank(t[i])) ++i;
                if (i < n && t[i] == '<') {
                    size_t e = t.find('>', i);
                    if (e != std::string::npos && e < line_end(t, i)) {
                        paint(st, i, e + 1, STRING);
                        i = e + 1;
                    }
                }
            }
            continue;
        }

        // python triple quotes
        if (d.triple && (c == '"' || c == '\'') && i + 2 < n && t[i + 1] == c && t[i + 2] == c) {
            size_t j = i + 3;
            bool closed = false;
            while (j < n) {
                if (t[j] == '\\') { j += 2; continue; }
                if (j + 2 < n && t[j] == c && t[j + 1] == c && t[j + 2] == c) { j += 3; closed = true; break; }
                ++j;
            }
            if (!closed) j = n;
            if (j > n) j = n;
            paint(st, i, j, STRING);
            i = j;
            continue;
        }

        // strings
        if ((c == '"' && d.dq) || (c == '\'' && d.sq) || (c == '`' && d.bt)) {
            if (c == '\'' && d.rust_char) {
                size_t j = i + 1;
                bool ok = false;
                if (j < n && t[j] == '\\') {
                    size_t e = t.find('\'', j + 2);
                    if (e != std::string::npos && e < line_end(t, i)) { j = e + 1; ok = true; }
                } else if (j < n && t[j] != '\n') {
                    size_t k = j + 1;
                    while (k < n && (static_cast<unsigned char>(t[k]) & 0xC0) == 0x80) ++k;  // utf-8 tail
                    if (k < n && t[k] == '\'') { j = k + 1; ok = true; }
                }
                if (ok) { paint(st, i, j, STRING); i = j; } else { ++i; }
                continue;
            }
            const bool multi = (c == '`');
            const bool raw = (c == '\'' && d.sq_raw);
            size_t j = i + 1;
            while (j < n) {
                if (t[j] == '\\' && !raw && j + 1 < n) { j += 2; continue; }
                if (t[j] == c) { ++j; break; }
                if (t[j] == '\n' && !multi) break;
                ++j;
            }
            if (j > n) j = n;
            paint(st, i, j, STRING);
            if (d.dollar == 1 && c == '"') {
                for (size_t k = i + 1; k < j; ++k) {
                    if (t[k] == '\\') { ++k; continue; }
                    if (t[k] == '$') {
                        size_t e = scan_shell_var(t, k);
                        if (e > j) e = j;
                        paint(st, k, e, VARIABLE);
                        if (e > k) k = e - 1;
                    }
                }
            }
            i = j;
            continue;
        }

        // $variables
        if (c == '$' && d.dollar) {
            size_t j = i + 1;
            if (d.dollar == 1) j = scan_shell_var(t, i);
            else if (d.dollar == 2) j = scan_make_var(t, i);
            else { while (j < n && is_ident_char(t[j])) ++j; }
            paint(st, i, j, VARIABLE);
            i = (j > i) ? j : i + 1;
            continue;
        }

        // decorators / attributes
        if (c == '@' && d.decorators && i + 1 < n && is_ident_start(t[i + 1])) {
            size_t j = i + 1;
            while (j < n && (is_ident_char(t[j]) || t[j] == '.')) ++j;
            paint(st, i, j, VARIABLE);
            i = j;
            continue;
        }

        // numbers
        if (is_digit(c) || (c == '.' && i + 1 < n && is_digit(t[i + 1]))) {
            size_t j = i;
            if (c == '0' && j + 1 < n && (t[j + 1] == 'x' || t[j + 1] == 'X')) {
                j += 2;
                while (j < n && (std::isxdigit(static_cast<unsigned char>(t[j])) || t[j] == '_')) ++j;
            } else if (c == '0' && j + 1 < n && (t[j + 1] == 'b' || t[j + 1] == 'B')) {
                j += 2;
                while (j < n && (t[j] == '0' || t[j] == '1' || t[j] == '_')) ++j;
            } else {
                while (j < n && (is_digit(t[j]) || t[j] == '_')) ++j;
                if (j + 1 < n && t[j] == '.' && is_digit(t[j + 1])) {
                    ++j;
                    while (j < n && (is_digit(t[j]) || t[j] == '_')) ++j;
                }
                if (j < n && (t[j] == 'e' || t[j] == 'E')) {
                    size_t k = j + 1;
                    if (k < n && (t[k] == '+' || t[k] == '-')) ++k;
                    if (k < n && is_digit(t[k])) {
                        j = k;
                        while (j < n && is_digit(t[j])) ++j;
                    }
                }
            }
            while (j < n && std::isalpha(static_cast<unsigned char>(t[j]))) ++j;  // u, l, f, ul ...
            paint(st, i, j, NUMBER);
            i = j;
            continue;
        }

        // identifiers
        if (is_ident_start(c)) {
            size_t j = i + 1;
            while (j < n && (is_ident_char(t[j]) ||
                             (d.ident_dash && t[j] == '-' && j + 1 < n && is_ident_char(t[j + 1]))))
                ++j;
            std::string w = t.substr(i, j - i);
            std::string key = d.nocase ? lower(w) : w;
            char style = PLAIN;
            if (d.kw.count(key)) {
                style = KEYWORD;
                if (d.def_intro.count(key)) expect_def = true;
            } else if (expect_def) {
                style = TYPE;
                expect_def = false;
            } else if (d.ty.count(key)) {
                style = TYPE;
            } else {
                size_t k = j;
                if (d.bang_macros && k < n && t[k] == '!' && k + 1 < n && t[k + 1] != '=') {
                    style = TYPE;
                    ++j;
                } else {
                    while (k < n && is_blank(t[k])) ++k;
                    if (d.func_calls && k < n && t[k] == '(') {
                        style = TYPE;
                    } else if (d.camel_types && std::isupper(static_cast<unsigned char>(w[0])) &&
                               std::any_of(w.begin(), w.end(), [](char ch) { return std::islower(static_cast<unsigned char>(ch)) != 0; })) {
                        style = TYPE;
                    } else if (d.caps_const && w.size() >= 2 &&
                               std::all_of(w.begin(), w.end(), [](char ch) { return std::isupper(static_cast<unsigned char>(ch)) || is_digit(ch) || ch == '_'; }) &&
                               std::isupper(static_cast<unsigned char>(w[0]))) {
                        style = NUMBER;
                    }
                }
            }
            paint(st, i, j, style);
            i = j;
            continue;
        }

        ++i;
    }
    return st;
}

// ---------------------------------------------------------------------------
// language definitions
// ---------------------------------------------------------------------------

const char* C_KW = "auto break case const continue default do else enum extern for goto if inline register "
                   "restrict return sizeof static struct switch typedef union volatile while NULL true false";
const char* C_TY = "int char short long float double void unsigned signed bool size_t ssize_t ptrdiff_t intptr_t "
                   "uintptr_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t FILE wchar_t va_list";
const char* CPP_KW = " alignas alignof and and_eq asm bitand bitor catch class co_await co_return co_yield compl "
                     "concept const_cast consteval constexpr constinit decltype delete dynamic_cast explicit export "
                     "final friend mutable namespace new noexcept not not_eq nullptr operator or or_eq override "
                     "private protected public reinterpret_cast requires static_assert static_cast template this "
                     "thread_local throw try typeid typename using virtual xor xor_eq";
const char* CPP_TY = " char8_t char16_t char32_t string wstring string_view vector unordered_map unordered_set "
                     "shared_ptr unique_ptr weak_ptr optional variant std";

LangDef c_like(bool cpp) {
    LangDef d;
    d.line_comments.push_back("//");
    d.block_open = "/*"; d.block_close = "*/";
    d.preproc = true;
    d.kw = words((std::string(C_KW) + (cpp ? CPP_KW : "")).c_str());
    d.ty = words((std::string(C_TY) + (cpp ? CPP_TY : "")).c_str());
    d.def_intro = words(cpp ? "class struct enum namespace union" : "struct enum union");
    return d;
}

LangDef js_like(bool ts) {
    LangDef d;
    d.line_comments.push_back("//");
    d.block_open = "/*"; d.block_close = "*/";
    d.bt = true;
    d.camel_types = true;
    d.kw = words("async await break case catch class const continue debugger default delete do else export extends "
                 "finally for function if import in instanceof let new of return static super switch this throw try "
                 "typeof var void while with yield null undefined true false NaN Infinity");
    d.ty = words("console window document require module exports process");
    d.def_intro = words("function class");
    if (ts) {
        for (const char* w : {"interface", "type", "enum", "namespace", "declare", "abstract", "implements", "private",
                              "protected", "public", "readonly", "as", "is", "keyof", "infer", "satisfies"})
            d.kw.insert(w);
        for (const char* w : {"string", "number", "boolean", "object", "symbol", "bigint", "unknown", "never", "any"})
            d.ty.insert(w);
        d.def_intro.insert("interface"); d.def_intro.insert("type"); d.def_intro.insert("enum");
        d.decorators = true;
    }
    return d;
}

std::unordered_map<std::string, LangDef> build_defs() {
    std::unordered_map<std::string, LangDef> m;

    m["C"] = c_like(false);
    m["C++"] = c_like(true);
    m["C/C++"] = m["C++"];
    m["JavaScript"] = js_like(false);
    m["TypeScript"] = js_like(true);

    {   LangDef d;  // Java
        d.line_comments.push_back("//"); d.block_open = "/*"; d.block_close = "*/";
        d.camel_types = true; d.decorators = true;
        d.kw = words("abstract assert break case catch class const continue default do else enum extends final finally "
                     "for goto if implements import instanceof interface native new package private protected public "
                     "return static strictfp super switch synchronized this throw throws transient try volatile while "
                     "var record sealed permits yield null true false");
        d.ty = words("int long short byte float double boolean char void");
        d.def_intro = words("class interface enum record");
        m["Java"] = d; }

    {   LangDef d;  // C#
        d.line_comments.push_back("//"); d.block_open = "/*"; d.block_close = "*/";
        d.camel_types = true; d.preproc = true;
        d.kw = words("abstract as base break case catch checked class const continue default delegate do else enum "
                     "event explicit extern finally fixed for foreach goto if implicit in interface internal is lock "
                     "namespace new operator out override params private protected public readonly ref return sealed "
                     "sizeof stackalloc static struct switch this throw try typeof unchecked unsafe using virtual "
                     "volatile while var async await yield record init null true false");
        d.ty = words("bool byte char decimal double float int long object sbyte short string uint ulong ushort void dynamic");
        d.def_intro = words("class struct interface enum namespace record");
        m["C#"] = d; }

    {   LangDef d;  // Go
        d.line_comments.push_back("//"); d.block_open = "/*"; d.block_close = "*/";
        d.bt = true; d.camel_types = false;
        d.kw = words("break case chan const continue default defer else fallthrough for func go goto if import "
                     "interface map package range return select struct switch type var nil true false iota");
        d.ty = words("bool byte complex64 complex128 error float32 float64 int int8 int16 int32 int64 rune string "
                     "uint uint8 uint16 uint32 uint64 uintptr any");
        d.def_intro = words("func type");
        m["Go"] = d; }

    {   LangDef d;  // Rust
        d.line_comments.push_back("//"); d.block_open = "/*"; d.block_close = "*/";
        d.rust_char = true; d.bang_macros = true; d.camel_types = true; d.decorators = false;
        d.kw = words("as async await break const continue crate dyn else enum extern false fn for if impl in let loop "
                     "match mod move mut pub ref return self Self static struct super trait true type unsafe use "
                     "where while");
        d.ty = words("bool char f32 f64 i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize str");
        d.def_intro = words("fn struct enum trait mod type union");
        m["Rust"] = d; }

    {   LangDef d;  // Python
        d.line_comments.push_back("#");
        d.triple = true; d.decorators = true; d.camel_types = true;
        d.kw = words("and as assert async await break class continue def del elif else except finally for from global "
                     "if import in is lambda nonlocal not or pass raise return try while with yield True False None");
        d.ty = words("print len range open isinstance enumerate zip map filter sorted sum min max abs super property "
                     "staticmethod classmethod int str float bool list dict set tuple bytes object type");
        d.def_intro = words("def class");
        m["Python"] = d; }

    {   LangDef d;  // Nim
        d.line_comments.push_back("#");
        d.block_open = "#["; d.block_close = "]#";
        d.hash_needs_space = false; d.camel_types = true;
        d.kw = words("addr and as asm bind block break case cast concept const continue converter defer discard "
                     "distinct div do elif else end enum except export finally for from func if import in include "
                     "interface is isnot iterator let macro method mixin mod nil not notin object of or out proc ptr "
                     "raise ref return shl shr static template try tuple type using var when while xor yield true false");
        d.ty = words("int int8 int16 int32 int64 uint uint8 uint16 uint32 uint64 float float32 float64 bool char "
                     "string cstring seq array set openArray pointer byte");
        d.def_intro = words("proc func method iterator template macro converter type");
        m["Nim"] = d; }

    {   LangDef d;  // PHP
        d.line_comments.push_back("//"); d.line_comments.push_back("#");
        d.block_open = "/*"; d.block_close = "*/";
        d.hash_needs_space = false; d.dollar = 3; d.camel_types = true;
        d.kw = words("abstract and array as break callable case catch class clone const continue declare default do "
                     "echo else elseif empty enddeclare endfor endforeach endif endswitch endwhile eval exit extends "
                     "final finally fn for foreach function global goto if implements include include_once "
                     "instanceof insteadof interface isset list match namespace new or print private protected "
                     "public readonly require require_once return static switch throw trait try unset use var while "
                     "xor yield null true false");
        d.ty = words("int float string bool void mixed self parent iterable object");
        d.def_intro = words("function class interface trait");
        m["PHP"] = d; }

    {   LangDef d;  // SQL
        d.line_comments.push_back("--");
        d.block_open = "/*"; d.block_close = "*/";
        d.nocase = true; d.bt = true; d.caps_const = false;
        d.kw = words("select from where insert into values update set delete create alter drop table index view "
                     "database schema join inner outer left right full cross on as and or not null is in exists "
                     "between like limit offset order by group having distinct union all case when then else end "
                     "primary key foreign references default unique check constraint begin commit rollback "
                     "transaction asc desc with returning if using cascade grant revoke");
        d.ty = words("int integer bigint smallint tinyint decimal numeric float double real varchar char text date "
                     "time timestamp datetime boolean bool blob json uuid serial");
        m["SQL"] = d; }

    {   LangDef d;  // Shell
        d.line_comments.push_back("#");
        d.dollar = 1; d.sq_raw = true; d.ident_dash = true; d.caps_const = false;
        d.kw = words("if fi then else elif for in do done while until case esac function return exit local declare "
                     "export readonly source set unset shift eval exec");
        d.ty = words("echo cd pwd mkdir rm cp mv chmod chown grep sed awk cat ls test read printf touch find xargs "
                     "sort uniq head tail curl wget tar make sudo git");
        m["Shell Script"] = d; }

    {   LangDef d;  // Makefile
        d.line_comments.push_back("#");
        d.dollar = 2; d.sq_raw = true; d.ident_dash = true; d.caps_const = false; d.makefile = true;
        d.func_calls = false;
        d.kw = words("ifeq ifneq ifdef ifndef else endif define endef include sinclude override export unexport "
                     "vpath undefine");
        m["Makefile"] = d; }

    {   LangDef d;  // CMake
        d.line_comments.push_back("#");
        d.block_open = "#[["; d.block_close = "]]";
        d.dollar = 1; d.nocase = true; d.ident_dash = false; d.caps_const = false; d.hash_needs_space = false;
        d.sq = false;
        d.kw = words("if else elseif endif foreach endforeach while endwhile function endfunction macro endmacro "
                     "return break continue");
        m["CMake"] = d; }

    return m;
}

const LangDef* find_def(const std::string& lang) {
    static const std::unordered_map<std::string, LangDef> defs = build_defs();
    std::unordered_map<std::string, LangDef>::const_iterator it = defs.find(lang);
    return it == defs.end() ? nullptr : &it->second;
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------

std::string highlight_json(const std::string& t) {
    const size_t n = t.size();
    std::string st(n, PLAIN);
    size_t i = 0;
    while (i < n) {
        char c = t[i];
        if (c == '"') {
            size_t j = i + 1;
            while (j < n && t[j] != '"' && t[j] != '\n') {
                if (t[j] == '\\' && j + 1 < n) ++j;
                ++j;
            }
            if (j < n && t[j] == '"') ++j;
            size_t k = j;
            while (k < n && is_space(t[k])) ++k;
            paint(st, i, j, (k < n && t[k] == ':') ? TYPE : STRING);
            i = j;
        } else if (c == '/' && i + 1 < n && t[i + 1] == '/') {
            size_t e = line_end(t, i);
            paint(st, i, e, COMMENT);
            i = e;
        } else if (c == '/' && i + 1 < n && t[i + 1] == '*') {
            size_t e = t.find("*/", i + 2);
            size_t end = (e == std::string::npos) ? n : e + 2;
            paint(st, i, end, COMMENT);
            i = end;
        } else if (is_digit(c) || (c == '-' && i + 1 < n && is_digit(t[i + 1]))) {
            size_t j = i + 1;
            while (j < n && (is_digit(t[j]) || t[j] == '.' || t[j] == 'e' || t[j] == 'E' || t[j] == '+' || t[j] == '-')) ++j;
            paint(st, i, j, NUMBER);
            i = j;
        } else if (std::isalpha(static_cast<unsigned char>(c))) {
            size_t j = i + 1;
            while (j < n && std::isalnum(static_cast<unsigned char>(t[j]))) ++j;
            std::string w = t.substr(i, j - i);
            if (w == "true" || w == "false" || w == "null") paint(st, i, j, KEYWORD);
            i = j;
        } else {
            ++i;
        }
    }
    return st;
}

// ---------------------------------------------------------------------------
// CSS (also good enough for SCSS/Less)
// ---------------------------------------------------------------------------

bool css_ident_char(char c) { return is_ident_char(c) || c == '-'; }

std::string highlight_css(const std::string& t) {
    const size_t n = t.size();
    std::string st(n, PLAIN);
    std::vector<char> stack;      // 'S' = selector context (top level, @media ...), 'D' = declarations
    bool pending_container = false;
    bool in_value = false;
    size_t i = 0;
    while (i < n) {
        char c = t[i];
        const bool decl = !stack.empty() && stack.back() == 'D';
        if (c == '/' && i + 1 < n && t[i + 1] == '*') {
            size_t e = t.find("*/", i + 2);
            size_t end = (e == std::string::npos) ? n : e + 2;
            paint(st, i, end, COMMENT);
            i = end;
            continue;
        }
        if (c == '/' && i + 1 < n && t[i + 1] == '/' && (i == 0 || is_space(t[i - 1]) || t[i - 1] == ';' || t[i - 1] == '{' || t[i - 1] == '}')) {
            size_t e = line_end(t, i);
            paint(st, i, e, COMMENT);
            i = e;
            continue;
        }
        if (c == '"' || c == '\'') {
            size_t j = i + 1;
            while (j < n && t[j] != c && t[j] != '\n') {
                if (t[j] == '\\' && j + 1 < n) ++j;
                ++j;
            }
            if (j < n && t[j] == c) ++j;
            paint(st, i, j, STRING);
            i = j;
            continue;
        }
        if (c == '{') { stack.push_back(pending_container ? 'S' : 'D'); pending_container = false; in_value = false; ++i; continue; }
        if (c == '}') { if (!stack.empty()) stack.pop_back(); in_value = false; ++i; continue; }
        if (c == ';') { in_value = false; ++i; continue; }
        if (c == ':' && decl && !in_value) { in_value = true; ++i; continue; }

        if (c == '@' && i + 1 < n && is_ident_start(t[i + 1])) {
            size_t j = i + 1;
            while (j < n && css_ident_char(t[j])) ++j;
            if (!decl) {
                std::string rule = t.substr(i + 1, j - i - 1);
                pending_container = (rule == "media" || rule == "supports" || rule == "layer" || rule == "container" ||
                                     rule == "document" || rule == "-moz-document" || rule == "scope" || rule == "starting-style");
            }
            paint(st, i, j, (decl && in_value) ? VARIABLE : KEYWORD);
            i = j;
            continue;
        }
        if (c == '$' && i + 1 < n && is_ident_start(t[i + 1])) {
            size_t j = i + 1;
            while (j < n && css_ident_char(t[j])) ++j;
            paint(st, i, j, VARIABLE);
            i = j;
            continue;
        }
        if (is_digit(c) || ((c == '.' || c == '-' || c == '+') && i + 1 < n &&
                            (is_digit(t[i + 1]) || (t[i + 1] == '.' && i + 2 < n && is_digit(t[i + 2]))) &&
                            (in_value || !decl))) {
            if (!(c == '.' && !decl && !in_value && i + 1 < n && !is_digit(t[i + 1]))) {
                size_t j = i + 1;
                while (j < n && (is_digit(t[j]) || t[j] == '.')) ++j;
                while (j < n && (std::isalpha(static_cast<unsigned char>(t[j])) || t[j] == '%')) ++j;
                paint(st, i, j, NUMBER);
                i = j;
                continue;
            }
        }
        if (c == '#' && i + 1 < n && std::isxdigit(static_cast<unsigned char>(t[i + 1])) && (in_value || decl)) {
            size_t j = i + 1;
            while (j < n && std::isxdigit(static_cast<unsigned char>(t[j]))) ++j;
            if (j - i - 1 == 3 || j - i - 1 == 4 || j - i - 1 == 6 || j - i - 1 == 8) {
                paint(st, i, j, NUMBER);
                i = j;
                continue;
            }
        }
        if (!decl || (!in_value && c != '(' && c != ')')) {
            // selector context (top level, or nested selector / property name)
            if ((c == '.' || c == '#') && i + 1 < n && is_ident_start(t[i + 1]) && !decl) {
                size_t j = i + 1;
                while (j < n && css_ident_char(t[j])) ++j;
                paint(st, i, j, TYPE);
                i = j;
                continue;
            }
            if (c == ':' && !decl) {
                size_t j = i + 1;
                if (j < n && t[j] == ':') ++j;
                size_t s = j;
                while (j < n && css_ident_char(t[j])) ++j;
                if (j > s) paint(st, i, j, VARIABLE);
                i = (j > i) ? j : i + 1;
                continue;
            }
        }
        if (is_ident_start(c) || (c == '-' && i + 1 < n && (is_ident_start(t[i + 1]) || t[i + 1] == '-'))) {
            size_t j = i + 1;
            while (j < n && css_ident_char(t[j])) ++j;
            size_t k = j;
            while (k < n && is_blank(t[k])) ++k;
            char style = PLAIN;
            if (!decl && !in_value) {
                style = KEYWORD;  // element selector
            } else if (!in_value) {
                style = (k < n && t[k] == ':') ? TYPE : PLAIN;  // property name
            } else if (k < n && t[k] == '(') {
                style = TYPE;  // function: url(), rgb(), var()
            } else if (t.compare(i, 2, "--") == 0) {
                style = VARIABLE;
            }
            paint(st, i, j, style);
            i = j;
            continue;
        }
        if (c == '!' && in_value) {
            size_t j = i + 1;
            while (j < n && std::isalpha(static_cast<unsigned char>(t[j]))) ++j;
            paint(st, i, j, KEYWORD);
            i = j;
            continue;
        }
        ++i;
    }
    return st;
}

// ---------------------------------------------------------------------------
// HTML / XML / SVG (with embedded <script> and <style>)
// ---------------------------------------------------------------------------

std::string highlight_text_lang(const std::string& lang, const std::string& text);

std::string highlight_html(const std::string& t) {
    const size_t n = t.size();
    std::string st(n, PLAIN);
    std::string lt;  // lazily built lower-case copy for </script> lookup
    size_t i = 0;
    while (i < n) {
        char c = t[i];
        if (c == '<' && match_at(t, i, "<!--")) {
            size_t e = t.find("-->", i + 4);
            size_t end = (e == std::string::npos) ? n : e + 3;
            paint(st, i, end, COMMENT);
            i = end;
            continue;
        }
        if (c == '<' && i + 1 < n && (t[i + 1] == '!' || t[i + 1] == '?')) {
            size_t e = t.find('>', i);
            size_t end = (e == std::string::npos) ? n : e + 1;
            paint(st, i, end, KEYWORD);
            i = end;
            continue;
        }
        if (c == '<' && i + 1 < n && (is_ident_start(t[i + 1]) || t[i + 1] == '/')) {
            size_t j = i + 1;
            bool closing = false;
            if (t[j] == '/') { closing = true; ++j; }
            size_t ns = j;
            while (j < n && (is_ident_char(t[j]) || t[j] == '-' || t[j] == ':' || t[j] == '.')) ++j;
            std::string name = lower(t.substr(ns, j - ns));
            paint(st, i, j, KEYWORD);
            while (j < n && t[j] != '>') {
                char a = t[j];
                if (a == '"' || a == '\'') {
                    size_t k = j + 1;
                    while (k < n && t[k] != a) ++k;
                    if (k < n) ++k;
                    paint(st, j, k, STRING);
                    j = k;
                } else if (a == '/') {
                    paint(st, j, j + 1, KEYWORD);
                    ++j;
                } else if (is_ident_start(a) || a == '@' || a == ':' || a == '-') {
                    size_t k = j + 1;
                    while (k < n && (is_ident_char(t[k]) || t[k] == '-' || t[k] == ':' || t[k] == '.' || t[k] == '@')) ++k;
                    paint(st, j, k, VARIABLE);
                    j = k;
                } else {
                    ++j;
                }
            }
            bool self_closing = (j > 0 && j <= n && j >= 1 && t[j - 1] == '/');
            if (j < n) { paint(st, j, j + 1, KEYWORD); ++j; }
            i = j;
            if (!closing && !self_closing && (name == "script" || name == "style")) {
                if (lt.empty()) lt = lower(t);
                size_t e = lt.find("</" + name, i);
                if (e == std::string::npos) e = n;
                std::string inner = highlight_text_lang(name == "script" ? "JavaScript" : "CSS", t.substr(i, e - i));
                st.replace(i, inner.size(), inner);
                i = e;
            }
            continue;
        }
        if (c == '&') {
            size_t j = i + 1;
            while (j < n && (std::isalnum(static_cast<unsigned char>(t[j])) || t[j] == '#')) ++j;
            if (j < n && t[j] == ';' && j - i > 1 && j - i < 12) {
                paint(st, i, j + 1, NUMBER);
                i = j + 1;
                continue;
            }
        }
        ++i;
    }
    return st;
}

// ---------------------------------------------------------------------------
// YAML / TOML / INI
// ---------------------------------------------------------------------------

enum ConfigMode { CFG_YAML, CFG_TOML, CFG_INI };

bool numeric_like(const std::string& w) {
    if (w.empty()) return false;
    size_t k = 0;
    if (w[0] == '+' || w[0] == '-') k = 1;
    if (k < w.size() && w[k] == '.') ++k;
    if (k >= w.size() || !is_digit(w[k])) return false;
    for (size_t j = k; j < w.size(); ++j) {
        char ch = w[j];
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '.' || ch == ':' || ch == '+' || ch == '-')) return false;
    }
    return true;
}

// Styles the value part [b, e) of one line. Returns the position of an opened
// multi-line string marker (TOML) via ml_open (0 = none, else the quote char).
void config_value(const std::string& t, size_t b, size_t e, std::string& st, int mode,
                  int key_indent, int& block_indent, char& ml_open) {
    size_t i = b;
    bool at_value_start = true;
    while (i < e) {
        char c = t[i];
        if (is_blank(c)) { ++i; continue; }
        if (c == '#' || (mode == CFG_INI && c == ';' && i == b)) {
            if (i == b || is_blank(t[i - 1])) { paint(st, i, e, COMMENT); return; }
        }
        if (c == '"' || c == '\'') {
            if (mode == CFG_TOML && i + 2 < e + 0 && t[i + 1] == c && t[i + 2] == c) {
                size_t close = t.find(std::string(3, c), i + 3);
                if (close != std::string::npos && close < e) {
                    paint(st, i, close + 3, STRING);
                    i = close + 3;
                } else {
                    paint(st, i, e, STRING);
                    ml_open = c;
                    return;
                }
                at_value_start = false;
                continue;
            }
            size_t j = i + 1;
            while (j < e) {
                if (t[j] == '\\' && c == '"' && j + 1 < e) { j += 2; continue; }
                if (t[j] == c) {
                    if (mode == CFG_YAML && c == '\'' && j + 1 < e && t[j + 1] == '\'') { j += 2; continue; }
                    ++j;
                    break;
                }
                ++j;
            }
            if (j > e) j = e;
            paint(st, i, j, STRING);
            i = j;
            at_value_start = false;
            continue;
        }
        if (mode == CFG_YAML && at_value_start && (c == '|' || c == '>')) {
            size_t j = i + 1;
            while (j < e && (t[j] == '+' || t[j] == '-' || is_digit(t[j]))) ++j;
            size_t k = j;
            while (k < e && is_blank(t[k])) ++k;
            if (k == e || t[k] == '#') {
                paint(st, i, j, KEYWORD);
                block_indent = key_indent;
                i = j;
                continue;
            }
        }
        if (mode == CFG_YAML && (c == '&' || c == '*') && i + 1 < e && is_ident_char(t[i + 1])) {
            size_t j = i + 1;
            while (j < e && (is_ident_char(t[j]) || t[j] == '-')) ++j;
            paint(st, i, j, VARIABLE);
            i = j;
            at_value_start = false;
            continue;
        }
        if (mode == CFG_YAML && c == '!') {
            size_t j = i + 1;
            while (j < e && !is_blank(t[j])) ++j;
            paint(st, i, j, KEYWORD);
            i = j;
            continue;
        }
        if (std::strchr(",[]{}", c)) { ++i; at_value_start = (c == '[' || c == '{' || c == ','); continue; }
        size_t j = i;
        while (j < e && !is_blank(t[j]) && !std::strchr(",]}", t[j])) ++j;
        if (j == i) j = i + 1;
        std::string w = t.substr(i, j - i);
        std::string lw = lower(w);
        if (numeric_like(w)) {
            paint(st, i, j, NUMBER);
        } else if (lw == "true" || lw == "false" || (mode != CFG_TOML && (lw == "null" || lw == "~" || lw == "yes" || lw == "no" || lw == "on" || lw == "off")) ||
                   (mode == CFG_TOML && (lw == "inf" || lw == "nan"))) {
            paint(st, i, j, KEYWORD);
        }
        i = j;
        at_value_start = false;
    }
}

std::string highlight_config(const std::string& t, int mode) {
    const size_t n = t.size();
    std::string st(n, PLAIN);
    int block_indent = -1;
    char ml_open = 0;
    size_t b = 0;
    while (b < n) {
        size_t e = line_end(t, b);
        size_t next = (e < n) ? e + 1 : n;

        if (ml_open) {
            size_t close = t.find(std::string(3, ml_open), b);
            if (close != std::string::npos && close < e) {
                paint(st, b, close + 3, STRING);
                ml_open = 0;
                config_value(t, close + 3, e, st, mode, 0, block_indent, ml_open);
            } else {
                paint(st, b, e, STRING);
            }
            b = next;
            continue;
        }

        int indent = 0;
        size_t p = b;
        while (p < e && is_blank(t[p])) { ++indent; ++p; }

        if (block_indent >= 0) {
            if (p == e || indent > block_indent) { paint(st, b, e, STRING); b = next; continue; }
            block_indent = -1;
        }
        if (p == e) { b = next; continue; }

        if (t[p] == '#' || (mode == CFG_INI && t[p] == ';')) { paint(st, p, e, COMMENT); b = next; continue; }

        if (mode == CFG_YAML && indent == 0 && (match_at(t, p, "---") || match_at(t, p, "...")) &&
            (p + 3 >= e || is_blank(t[p + 3]))) {
            paint(st, p, p + 3, MARKUP);
            config_value(t, p + 3, e, st, mode, 0, block_indent, ml_open);
            b = next;
            continue;
        }

        if (mode != CFG_YAML && t[p] == '[') {
            size_t close = t.find(']', p);
            if (close != std::string::npos && close < e) {
                if (mode == CFG_TOML && close + 1 < e && t[close + 1] == ']' && t[p + 1] == '[') ++close;
                paint(st, p, close + 1, KEYWORD);
                config_value(t, close + 1, e, st, mode, indent, block_indent, ml_open);
                b = next;
                continue;
            }
        }

        size_t cur = p;
        if (mode == CFG_YAML) {
            while (cur < e && t[cur] == '-' && (cur + 1 == e || is_blank(t[cur + 1]))) {
                paint(st, cur, cur + 1, MARKUP);
                ++cur;
                while (cur < e && is_blank(t[cur])) ++cur;
            }
        }

        size_t key_end = std::string::npos, sep = std::string::npos;
        if (cur < e && (t[cur] == '"' || t[cur] == '\'')) {
            char q = t[cur];
            size_t j = cur + 1;
            while (j < e && t[j] != q) { if (t[j] == '\\' && q == '"') ++j; ++j; }
            if (j < e) {
                size_t k = j + 1;
                while (k < e && is_blank(t[k])) ++k;
                if (k < e && ((mode == CFG_YAML && t[k] == ':') || (mode != CFG_YAML && t[k] == '='))) {
                    key_end = j + 1;
                    sep = k;
                }
            }
        } else {
            for (size_t k = cur; k < e; ++k) {
                char ch = t[k];
                if (ch == '#' && (k == cur || is_blank(t[k - 1]))) break;
                if (mode == CFG_YAML) {
                    if (ch == ':' && (k + 1 == e || is_blank(t[k + 1]))) { sep = k; break; }
                    if (ch == '[' || ch == '{') break;
                } else if (ch == '=' || (mode == CFG_INI && ch == ':')) {
                    sep = k;
                    break;
                }
            }
            if (sep != std::string::npos) {
                key_end = sep;
                while (key_end > cur && is_blank(t[key_end - 1])) --key_end;
            }
        }
        size_t val_start = cur;
        if (sep != std::string::npos && key_end != std::string::npos && key_end > cur) {
            paint(st, cur, key_end, TYPE);
            val_start = sep + 1;
        }
        config_value(t, val_start, e, st, mode, indent, block_indent, ml_open);
        b = next;
    }
    return st;
}

// ---------------------------------------------------------------------------
// diff / patch
// ---------------------------------------------------------------------------

std::string highlight_diff(const std::string& t) {
    const size_t n = t.size();
    std::string st(n, PLAIN);
    size_t b = 0;
    while (b < n) {
        size_t e = line_end(t, b);
        char c = t[b];
        char s = PLAIN;
        if (match_at(t, b, "+++") || match_at(t, b, "---") || match_at(t, b, "diff ") || match_at(t, b, "index ")) s = KEYWORD;
        else if (c == '@') s = TYPE;
        else if (c == '+') s = STRING;
        else if (c == '-') s = HEADING;
        paint(st, b, e, s);
        b = (e < n) ? e + 1 : n;
    }
    return st;
}

// ---------------------------------------------------------------------------
// Markdown
// ---------------------------------------------------------------------------

// Maps a fenced-code info string ("cpp", "py", "js", ...) to a language name.
std::string lang_from_tag(const std::string& raw) {
    std::string w = lower(raw);
    size_t e = 0;
    while (e < w.size() && !is_space(w[e]) && w[e] != '{' && w[e] != ',') ++e;
    w = w.substr(0, e);
    if (!w.empty() && w[0] == '.') w.erase(0, 1);
    static const std::unordered_map<std::string, std::string> m = {
        {"c", "C"}, {"h", "C"}, {"cpp", "C++"}, {"c++", "C++"}, {"cc", "C++"}, {"cxx", "C++"}, {"hpp", "C++"},
        {"js", "JavaScript"}, {"javascript", "JavaScript"}, {"jsx", "JavaScript"}, {"mjs", "JavaScript"}, {"node", "JavaScript"},
        {"ts", "TypeScript"}, {"typescript", "TypeScript"}, {"tsx", "TypeScript"},
        {"py", "Python"}, {"python", "Python"}, {"python3", "Python"},
        {"sh", "Shell Script"}, {"bash", "Shell Script"}, {"shell", "Shell Script"}, {"zsh", "Shell Script"}, {"fish", "Shell Script"},
        {"json", "JSON"}, {"jsonc", "JSON"}, {"yaml", "YAML"}, {"yml", "YAML"}, {"toml", "TOML"}, {"ini", "INI"},
        {"html", "HTML"}, {"xml", "XML/SVG"}, {"svg", "XML/SVG"}, {"css", "CSS"}, {"scss", "CSS"}, {"sql", "SQL"},
        {"go", "Go"}, {"golang", "Go"}, {"rust", "Rust"}, {"rs", "Rust"}, {"java", "Java"}, {"cs", "C#"}, {"csharp", "C#"},
        {"php", "PHP"}, {"make", "Makefile"}, {"makefile", "Makefile"}, {"cmake", "CMake"}, {"nim", "Nim"},
        {"diff", "Diff"}, {"patch", "Diff"}, {"md", "Markdown"}, {"markdown", "Markdown"}};
    std::unordered_map<std::string, std::string>::const_iterator it = m.find(w);
    return it == m.end() ? std::string() : it->second;
}

void md_inline(std::string& st, const std::string& t, size_t b, size_t e, char base);

// Parses a link/image starting at t[i] == '['. On success paints it and
// returns the index after it, otherwise returns 0.
size_t md_link(std::string& st, const std::string& t, size_t i, size_t e, char text_style) {
    int depth = 0;
    size_t close = std::string::npos;
    for (size_t j = i; j < e; ++j) {
        if (t[j] == '\\') { ++j; continue; }
        if (t[j] == '`') {  // skip code span
            size_t k = t.find('`', j + 1);
            if (k != std::string::npos && k < e) { j = k; continue; }
        }
        if (t[j] == '[') ++depth;
        else if (t[j] == ']') { if (--depth == 0) { close = j; break; } }
    }
    if (close == std::string::npos) return 0;

    if (close + 1 < e && t[close + 1] == '(') {
        int pd = 0;
        size_t pe = std::string::npos;
        for (size_t j = close + 1; j < e; ++j) {
            if (t[j] == '\\') { ++j; continue; }
            if (t[j] == '(') ++pd;
            else if (t[j] == ')') { if (--pd == 0) { pe = j; break; } }
        }
        if (pe == std::string::npos) return 0;
        paint(st, i, i + 1, MARKUP);
        md_inline(st, t, i + 1, close, text_style);
        paint(st, close, pe + 1, MARKUP);
        return pe + 1;
    }
    if (close + 1 < e && t[close + 1] == '[') {
        size_t r = t.find(']', close + 2);
        if (r == std::string::npos || r >= e) return 0;
        paint(st, i, i + 1, MARKUP);
        md_inline(st, t, i + 1, close, text_style);
        paint(st, close, r + 1, MARKUP);
        return r + 1;
    }
    return 0;
}

void md_inline(std::string& st, const std::string& t, size_t b, size_t e, char base) {
    paint(st, b, e, base);
    size_t i = b;
    while (i < e) {
        char c = t[i];

        if (c == '\\' && i + 1 < e) { i += 2; continue; }

        if (c == '`') {
            size_t r = i;
            while (r < e && t[r] == '`') ++r;
            size_t run = r - i, j = r, close = std::string::npos;
            while (j < e) {
                if (t[j] == '`') {
                    size_t k = j;
                    while (k < e && t[k] == '`') ++k;
                    if (k - j == run) { close = j; break; }
                    j = k;
                } else {
                    ++j;
                }
            }
            if (close != std::string::npos) { paint(st, i, close + run, CODE); i = close + run; }
            else i = r;
            continue;
        }

        if (c == '<') {
            if (match_at(t, i, "<!--")) {
                size_t k = t.find("-->", i + 4);
                size_t end = (k != std::string::npos && k + 3 <= e) ? k + 3 : e;
                paint(st, i, end, COMMENT);
                i = end;
                continue;
            }
            if (i + 1 < e && (is_ident_start(t[i + 1]) || t[i + 1] == '/')) {
                size_t k = t.find('>', i);
                if (k != std::string::npos && k < e) {
                    std::string inner = t.substr(i + 1, k - i - 1);
                    bool url = inner.find("://") != std::string::npos ||
                               (inner.find('@') != std::string::npos && inner.find(' ') == std::string::npos);
                    paint(st, i, k + 1, url ? MARKUP : KEYWORD);
                    i = k + 1;
                    continue;
                }
            }
            ++i;
            continue;
        }

        if (c == '!' && i + 1 < e && t[i + 1] == '[') {
            size_t end = md_link(st, t, i + 1, e, VARIABLE);
            if (end) { paint(st, i, i + 1, MARKUP); i = end; continue; }
            ++i;
            continue;
        }
        if (c == '[') {
            size_t end = md_link(st, t, i, e, LINK);
            if (end) { i = end; continue; }
            ++i;
            continue;
        }

        if (c == 'h' && (match_at(t, i, "http://") || match_at(t, i, "https://")) && (i == b || !is_ident_char(t[i - 1]))) {
            size_t j = i;
            while (j < e && !is_space(t[j]) && t[j] != '<' && t[j] != '>') ++j;
            while (j > i && std::strchr(".,;:!?)'\"*_", t[j - 1])) --j;
            paint(st, i, j, MARKUP);
            i = (j > i) ? j : i + 1;
            continue;
        }

        if (c == '*' || c == '_') {
            size_t r = i;
            while (r < e && t[r] == c) ++r;
            size_t run = r - i;
            bool can_open = r < e && !is_space(t[r]);
            if (c == '_' && i > b && is_ident_char(t[i - 1])) can_open = false;
            if (!can_open || run > 3) { i = r; continue; }
            size_t close = std::string::npos, j = r;
            while (j < e) {
                if (t[j] == '\\') { j += 2; continue; }
                if (t[j] == '`') {
                    size_t k = t.find('`', j + 1);
                    if (k != std::string::npos && k < e) { j = k + 1; continue; }
                }
                if (t[j] == c) {
                    size_t k = j;
                    while (k < e && t[k] == c) ++k;
                    bool can_close = j > r && !is_space(t[j - 1]);
                    if (c == '_' && k < e && is_ident_char(t[k])) can_close = false;
                    if (k - j == run && can_close) { close = j; break; }
                    j = k;
                    continue;
                }
                ++j;
            }
            if (close == std::string::npos) { i = r; continue; }
            char s = (run == 1) ? ITALIC : BOLD;
            paint(st, i, close + run, s);
            md_inline(st, t, r, close, s);
            i = close + run;
            continue;
        }

        if (c == '~' && i + 1 < e && t[i + 1] == '~') {
            size_t k = t.find("~~", i + 2);
            if (k != std::string::npos && k + 2 <= e && k > i + 2) {
                paint(st, i, k + 2, COMMENT);
                i = k + 2;
                continue;
            }
            i += 2;
            continue;
        }
        ++i;
    }
}

bool md_is_table_sep(const std::string& t, size_t b, size_t e) {
    bool dash = false;
    for (size_t i = b; i < e; ++i) {
        char c = t[i];
        if (c == '-') dash = true;
        else if (!(c == '|' || c == ':' || c == ' ' || c == '\t' || c == '\r')) return false;
    }
    return dash;
}

bool md_has_pipe(const std::string& t, size_t b, size_t e) {
    for (size_t i = b; i < e; ++i) {
        if (t[i] == '\\') { ++i; continue; }
        if (t[i] == '|') return true;
    }
    return false;
}

void md_table_row(std::string& st, const std::string& t, size_t b, size_t e, char base) {
    size_t cell = b;
    for (size_t i = b; i <= e; ++i) {
        if (i == e || (t[i] == '|' && (i == b || t[i - 1] != '\\'))) {
            md_inline(st, t, cell, i, base);
            if (i < e) paint(st, i, i + 1, MARKUP);
            cell = i + 1;
        }
    }
}

void md_fill_code(std::string& st, const std::string& t, size_t b, size_t e, const std::string& lang) {
    if (e <= b) return;
    if (!lang.empty()) {
        std::string sub = highlight_text_lang(lang, t.substr(b, e - b));
        if (sub.size() == e - b) { st.replace(b, e - b, sub); return; }
    }
    paint(st, b, e, CODE);
}

std::string highlight_markdown(const std::string& t) {
    const size_t n = t.size();
    std::string st(n, PLAIN);

    // split into lines
    std::vector<std::pair<size_t, size_t> > lines;
    for (size_t b = 0; b <= n;) {
        size_t e = line_end(t, b);
        lines.push_back(std::make_pair(b, e));
        if (e >= n) break;
        b = e + 1;
    }
    const size_t L = lines.size();
    size_t li = 0;

    // YAML front matter
    if (L > 1 && match_at(t, 0, "---") && t.find_first_not_of(" \t\r", 3) == lines[0].second) {
        for (size_t j = 1; j < L; ++j) {
            size_t b = lines[j].first, e = lines[j].second;
            if ((match_at(t, b, "---") || match_at(t, b, "...")) && t.find_first_not_of(" \t\r", b + 3) == (e < n ? e : std::string::npos)) {
                paint(st, 0, 3, MARKUP);
                paint(st, b, b + 3, MARKUP);
                size_t ys = lines[0].second + 1;
                if (b > ys) {
                    std::string sub = highlight_config(t.substr(ys, b - ys), CFG_YAML);
                    st.replace(ys, sub.size(), sub);
                }
                li = j + 1;
                break;
            }
        }
    }

    bool in_fence = false, in_comment = false, in_table = false;
    char fch = 0;
    size_t flen = 0, fstart = 0;
    std::string flang;
    long prev_para = -1;

    for (; li < L; ++li) {
        const size_t b = lines[li].first, e = lines[li].second;

        if (in_fence) {
            size_t p = b;
            int sp = 0;
            while (p < e && t[p] == ' ' && sp < 3) { ++p; ++sp; }
            size_t q = p;
            while (q < e && t[q] == fch) ++q;
            if (q - p >= flen && t.find_first_not_of(" \t\r", q) >= e) {
                md_fill_code(st, t, fstart, b, flang);
                paint(st, p, q, MARKUP);
                in_fence = false;
            }
            continue;
        }

        if (in_comment) {
            paint(st, b, e, COMMENT);
            if (t.find("-->", b) < e) in_comment = false;
            continue;
        }

        size_t p = b;
        int indent = 0;
        while (p < e && t[p] == ' ' && indent < 3) { ++p; ++indent; }
        const bool blank = t.find_first_not_of(" \t\r", b) >= e;

        if (blank) { prev_para = -1; in_table = false; continue; }

        // fenced code block start
        if (p < e && (t[p] == '`' || t[p] == '~')) {
            char ch = t[p];
            size_t q = p;
            while (q < e && t[q] == ch) ++q;
            if (q - p >= 3) {
                std::string info = t.substr(q, e - q);
                size_t f = info.find_first_not_of(" \t\r");
                bool ok = !(ch == '`' && info.find('`') != std::string::npos);
                if (ok) {
                    paint(st, p, q, MARKUP);
                    flang.clear();
                    if (f != std::string::npos) {
                        size_t w = q + f;
                        size_t we = w;
                        while (we < e && !is_space(t[we])) ++we;
                        paint(st, w, we, TYPE);
                        flang = lang_from_tag(t.substr(w, we - w));
                    }
                    in_fence = true;
                    fch = ch;
                    flen = q - p;
                    fstart = (e < n) ? e + 1 : n;
                    prev_para = -1;
                    in_table = false;
                    continue;
                }
            }
        }

        // HTML comment block
        if (match_at(t, p, "<!--")) {
            paint(st, p, e, COMMENT);
            if (t.find("-->", p + 4) >= e) in_comment = true;
            prev_para = -1;
            continue;
        }

        // ATX heading
        if (p < e && t[p] == '#') {
            size_t q = p;
            while (q < e && t[q] == '#') ++q;
            if (q - p <= 6 && (q == e || is_blank(t[q]) || t[q] == '\r')) {
                size_t end = e;
                while (end > q && (is_blank(t[end - 1]) || t[end - 1] == '\r')) --end;
                size_t k = end;
                while (k > q && t[k - 1] == '#') --k;
                if (k < end && k > q && is_blank(t[k - 1])) end = k;  // closing ###
                md_inline(st, t, p, e, HEADING);
                paint(st, p, q, HEADING);
                prev_para = -1;
                in_table = false;
                continue;
            }
        }

        // setext underline (=== or ---) directly below a paragraph line
        if (prev_para >= 0 && p < e && (t[p] == '=' || t[p] == '-')) {
            char ch = t[p];
            size_t q = p;
            while (q < e && t[q] == ch) ++q;
            if (t.find_first_not_of(" \t\r", q) >= e) {
                md_inline(st, t, lines[prev_para].first, lines[prev_para].second, HEADING);
                paint(st, p, e, HEADING);
                prev_para = -1;
                continue;
            }
        }

        // thematic break
        if (p < e && (t[p] == '-' || t[p] == '*' || t[p] == '_')) {
            char ch = t[p];
            size_t cnt = 0;
            bool only = true;
            for (size_t k = p; k < e; ++k) {
                if (t[k] == ch) ++cnt;
                else if (!(t[k] == ' ' || t[k] == '\t' || t[k] == '\r')) { only = false; break; }
            }
            if (only && cnt >= 3) {
                paint(st, p, e, MARKUP);
                prev_para = -1;
                in_table = false;
                continue;
            }
        }

        // block quote
        if (p < e && t[p] == '>') {
            size_t q = p;
            while (q < e && (t[q] == '>' || t[q] == ' ')) {
                if (t[q] == '>') paint(st, q, q + 1, MARKUP);
                ++q;
            }
            md_inline(st, t, q, e, COMMENT);
            prev_para = -1;
            in_table = false;
            continue;
        }

        // table
        {
            bool header = md_has_pipe(t, b, e) && li + 1 < L && md_is_table_sep(t, lines[li + 1].first, lines[li + 1].second) &&
                          md_has_pipe(t, lines[li + 1].first, lines[li + 1].second);
            if (header) {
                in_table = true;
                md_table_row(st, t, b, e, BOLD);
                prev_para = -1;
                continue;
            }
            if (in_table && md_is_table_sep(t, b, e)) { paint(st, b, e, MARKUP); continue; }
            if (in_table && md_has_pipe(t, b, e)) {
                md_table_row(st, t, b, e, PLAIN);
                continue;
            }
            in_table = false;
        }

        // list item
        {
            size_t q = b;
            while (q < e && is_blank(t[q])) ++q;
            size_t m = q;
            bool is_item = false;
            if (m < e && (t[m] == '-' || t[m] == '+' || t[m] == '*') && (m + 1 == e || is_blank(t[m + 1]))) {
                ++m;
                is_item = true;
            } else if (m < e && is_digit(t[m])) {
                size_t k = m;
                while (k < e && is_digit(t[k]) && k - m < 9) ++k;
                if (k < e && (t[k] == '.' || t[k] == ')') && (k + 1 == e || is_blank(t[k + 1]))) {
                    m = k + 1;
                    is_item = true;
                }
            }
            if (is_item) {
                paint(st, q, m, MARKUP);
                size_t r = m;
                while (r < e && is_blank(t[r])) ++r;
                if (r + 2 < e + 0 && t[r] == '[' && (t[r + 1] == ' ' || t[r + 1] == 'x' || t[r + 1] == 'X') && t[r + 2] == ']' &&
                    (r + 3 == e || is_blank(t[r + 3]))) {
                    paint(st, r, r + 3, MARKUP);
                    r += 3;
                }
                md_inline(st, t, r, e, PLAIN);
                prev_para = -1;
                continue;
            }
        }

        // link reference definition:  [label]: url
        if (p < e && t[p] == '[') {
            size_t close = t.find("]:", p);
            if (close != std::string::npos && close < e) {
                paint(st, p, close + 2, LINK);
                paint(st, close + 2, e, MARKUP);
                prev_para = -1;
                continue;
            }
        }

        // plain paragraph line
        md_inline(st, t, b, e, PLAIN);
        prev_para = static_cast<long>(li);
    }

    if (in_fence) md_fill_code(st, t, fstart, n, flang);
    return st;
}

// ---------------------------------------------------------------------------
// dispatcher
// ---------------------------------------------------------------------------

std::string highlight_text_lang(const std::string& lang, const std::string& text) {
    if (text.empty()) return std::string();
    if (lang == "Markdown") return highlight_markdown(text);
    if (lang == "JSON") return highlight_json(text);
    if (lang == "CSS") return highlight_css(text);
    if (lang == "HTML" || lang == "XML/SVG") return highlight_html(text);
    if (lang == "YAML") return highlight_config(text, CFG_YAML);
    if (lang == "TOML") return highlight_config(text, CFG_TOML);
    if (lang == "INI") return highlight_config(text, CFG_INI);
    if (lang == "Diff") return highlight_diff(text);
    const LangDef* d = find_def(lang);
    if (d) return highlight_generic(*d, text);
    return std::string(text.size(), PLAIN);
}

}  // namespace

// ---------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------

std::string highlight(const std::string& language, const std::string& text) {
    if (text.size() > MAX_HIGHLIGHT_BYTES) return std::string(text.size(), PLAIN);
    std::string st = highlight_text_lang(language, text);
    if (st.size() != text.size()) st.resize(text.size(), PLAIN);
    return st;
}

std::string line_comment_prefix(const std::string& lang) {
    if (lang == "C" || lang == "C++" || lang == "C/C++" || lang == "JavaScript" || lang == "TypeScript" ||
        lang == "Java" || lang == "C#" || lang == "Go" || lang == "Rust" || lang == "PHP")
        return "//";
    if (lang == "Python" || lang == "Shell Script" || lang == "Makefile" || lang == "CMake" || lang == "YAML" ||
        lang == "TOML" || lang == "Nim")
        return "#";
    if (lang == "SQL") return "--";
    if (lang == "INI") return ";";
    return "";
}

std::string detect_language(const std::string& path, const std::string& head) {
    size_t slash = path.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    std::string lname = lower(name);

    if (name == "Makefile" || name == "makefile" || name == "GNUmakefile") return "Makefile";
    if (name == "CMakeLists.txt") return "CMake";
    if (lname == ".bashrc" || lname == ".zshrc" || lname == ".profile" || lname == ".bash_profile" || lname == ".env")
        return "Shell Script";
    if (lname == ".gitconfig" || lname == ".editorconfig") return "INI";

    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < name.size()) {
        std::string ext = lname.substr(dot + 1);
        static const std::unordered_map<std::string, std::string> by_ext = {
            {"c", "C"}, {"h", "C/C++"}, {"cpp", "C++"}, {"cc", "C++"}, {"cxx", "C++"}, {"hpp", "C++"}, {"hh", "C++"}, {"hxx", "C++"},
            {"js", "JavaScript"}, {"mjs", "JavaScript"}, {"cjs", "JavaScript"}, {"jsx", "JavaScript"},
            {"ts", "TypeScript"}, {"tsx", "TypeScript"},
            {"py", "Python"}, {"pyw", "Python"}, {"nim", "Nim"}, {"nims", "Nim"}, {"go", "Go"}, {"rs", "Rust"},
            {"java", "Java"}, {"cs", "C#"}, {"php", "PHP"}, {"sql", "SQL"},
            {"yaml", "YAML"}, {"yml", "YAML"}, {"toml", "TOML"}, {"ini", "INI"}, {"cfg", "INI"}, {"conf", "INI"},
            {"cmake", "CMake"}, {"mk", "Makefile"},
            {"html", "HTML"}, {"htm", "HTML"}, {"xhtml", "HTML"},
            {"xml", "XML/SVG"}, {"svg", "XML/SVG"}, {"ui", "XML/SVG"}, {"xsl", "XML/SVG"}, {"xaml", "XML/SVG"},
            {"css", "CSS"}, {"scss", "CSS"}, {"less", "CSS"},
            {"json", "JSON"}, {"jsonc", "JSON"}, {"json5", "JSON"},
            {"md", "Markdown"}, {"markdown", "Markdown"}, {"mdown", "Markdown"},
            {"sh", "Shell Script"}, {"bash", "Shell Script"}, {"zsh", "Shell Script"}, {"fish", "Shell Script"}, {"env", "Shell Script"},
            {"diff", "Diff"}, {"patch", "Diff"},
            {"txt", "Plain Text"}, {"log", "Plain Text"}};
        std::unordered_map<std::string, std::string>::const_iterator it = by_ext.find(ext);
        if (it != by_ext.end()) return it->second;
        return "Plain Text";  // a known-unknown extension: do not guess from content
    }

    // No extension: fall back to content sniffing.
    if (!head.empty()) {
        size_t nl = head.find('\n');
        std::string first = (nl == std::string::npos) ? head : head.substr(0, nl);
        if (first.compare(0, 2, "#!") == 0) {
            if (first.find("python") != std::string::npos) return "Python";
            if (first.find("node") != std::string::npos) return "JavaScript";
            if (first.find("sh") != std::string::npos) return "Shell Script";  // sh, bash, zsh, dash
        }
        if (head.compare(0, 5, "<?xml") == 0) return "XML/SVG";
        if (head.compare(0, 5, "<?php") == 0) return "PHP";
        if (head.compare(0, 9, "<!DOCTYPE") == 0 || head.compare(0, 5, "<html") == 0) return "HTML";
    }
    return "Plain Text";
}

}  // namespace Highlighter
