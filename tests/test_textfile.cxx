// Tests TextFile and the URI helpers in PathUtil.  Run: make test
#include "../TextFile.h"
#include "../PathUtil.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

static std::string slurp(const std::string& p) {
    std::ifstream in(p.c_str(), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
static void spit(const std::string& p, const std::string& s) { std::ofstream(p.c_str(), std::ios::binary) << s; }

int main() {
    char tmpl[] = "/tmp/flitpad_textfile_XXXXXX";
    if (!mkdtemp(tmpl)) return 2;
    const std::string dir = tmpl, f = dir + "/a.txt";
    std::string text, err;
    bool crlf = true;

    // plain LF file
    spit(f, "one\ntwo\n");
    CHECK(TextFile::read(f, text, crlf, err) && text == "one\ntwo\n" && !crlf);
    // CRLF is detected and normalised, and written back as CRLF
    spit(f, "one\r\ntwo\r\n");
    CHECK(TextFile::read(f, text, crlf, err) && text == "one\ntwo\n" && crlf);
    CHECK(TextFile::write(f, text, crlf, err) && slurp(f) == "one\r\ntwo\r\n");
    // lone CR characters are dropped (as before), not mistaken for CRLF
    spit(f, "a\rb\n");
    CHECK(TextFile::read(f, text, crlf, err) && text == "ab\n" && !crlf);
    // final newline is added on write, never doubled, and empty stays empty
    CHECK(TextFile::write(f, "no newline", false, err) && slurp(f) == "no newline\n");
    CHECK(TextFile::write(f, "has newline\n", false, err) && slurp(f) == "has newline\n");
    CHECK(TextFile::write(f, "", false, err) && slurp(f).empty());
    CHECK(TextFile::read(f, text, crlf, err) && text.empty());
    CHECK(TextFile::write(f, "a\nb", true, err) && slurp(f) == "a\r\nb\r\n");
    // UTF-8 passes through untouched
    CHECK(TextFile::write(f, "caf\xc3\xa9 \xef\x80\x95\n", false, err) && TextFile::read(f, text, crlf, err) && text == "caf\xc3\xa9 \xef\x80\x95\n");

    // errors
    text = "untouched";
    CHECK(!TextFile::read(dir + "/missing", text, crlf, err) && !err.empty() && text == "untouched");
    spit(f, std::string("bin\0ary", 7));
    CHECK(!TextFile::read(f, text, crlf, err) && err.find("binary") != std::string::npos && text == "untouched");
    spit(f, std::string(2000, 'x'));
    CHECK(!TextFile::read(f, text, crlf, err, 1000) && err.find("too large") != std::string::npos);
    CHECK(TextFile::read(f, text, crlf, err, 2000) && text.size() == 2000);              // exactly at the limit is fine
    CHECK(!TextFile::write(dir + "/no/such/dir/x.txt", "x", false, err) && !err.empty());

    // modification time
    CHECK(TextFile::modified_time(dir + "/missing") == 0);
    spit(f, "x");
    const long long now = TextFile::modified_time(f);
    CHECK(now > 1000000000LL);
    struct utimbuf old_time = {static_cast<time_t>(now - 100), static_cast<time_t>(now - 100)};
    CHECK(utime(f.c_str(), &old_time) == 0);
    CHECK(TextFile::modified_time(f) == now - 100);                                      // a change is visible
    CHECK(TextFile::write(f, "y", false, err) && TextFile::modified_time(f) >= now);

    // URI lists (drag and drop)
    using PathUtil::parse_uri_list;
    using PathUtil::url_decode;
    CHECK(url_decode("a%20b%2Fc") == "a b/c" && url_decode("100%") == "100%" && url_decode("%zz%4") == "%zz%4" && url_decode("%41") == "A");
    std::vector<std::string> p = parse_uri_list("file:///home/me/a%20b.txt\r\nfile:///tmp/c.md\r\n");
    CHECK(p.size() == 2 && p[0] == "/home/me/a b.txt" && p[1] == "/tmp/c.md");
    p = parse_uri_list("# comment\nfile://localhost/etc/hosts\n\n");
    CHECK(p.size() == 1 && p[0] == "/etc/hosts");
    p = parse_uri_list("file:///C:/Users/me/x.txt");
    CHECK(p.size() == 1 && p[0] == "C:/Users/me/x.txt");
    CHECK(parse_uri_list("file:///a\nhello world\n").empty());           // ordinary text mixed in: not a file list
    CHECK(parse_uri_list("hello").empty() && parse_uri_list("").empty() && parse_uri_list("\n\n").empty());
    CHECK(parse_uri_list("http://example.com/x").empty());
    CHECK(parse_uri_list("file://otherhost/share/x").empty());           // another machine
    CHECK(parse_uri_list("file://").empty());
    CHECK(parse_uri_list("file:///caf%C3%A9.txt")[0] == "/caf\xc3\xa9.txt");

    std::string cmd = "rm -rf '" + dir + "'";
    if (std::system(cmd.c_str()) != 0) std::cerr << "cleanup failed\n";
    std::cout << (checks - failures) << "/" << checks << " textfile checks passed\n";
    return failures ? 1 : 0;
}
