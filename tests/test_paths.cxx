// Tests PathUtil against real symlinks in a temp directory.  Run: make test
#include "../PathUtil.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

int main() {
    char tmpl[] = "/tmp/flitpad_paths_XXXXXX";
    if (!mkdtemp(tmpl)) { std::cerr << "mkdtemp failed\n"; return 2; }
    const std::string root = PathUtil::canonical(tmpl);   // /tmp itself may be a symlink (macOS)
    const std::string real = root + "/real", other = root + "/other";
    mkdir(real.c_str(), 0755);
    mkdir(other.c_str(), 0755);
    { std::ofstream(real + "/a.txt") << "x"; }
    CHECK(symlink(real.c_str(), (root + "/link").c_str()) == 0);               // dir symlink
    CHECK(symlink("real", (root + "/rellink").c_str()) == 0);                  // relative dir symlink
    CHECK(symlink((root + "/link").c_str(), (root + "/link2").c_str()) == 0);  // symlink to a symlink
    CHECK(symlink((real + "/a.txt").c_str(), (other + "/file_link.txt").c_str()) == 0);  // file symlink
    CHECK(symlink((root + "/loop_b").c_str(), (root + "/loop_a").c_str()) == 0);
    CHECK(symlink((root + "/loop_a").c_str(), (root + "/loop_b").c_str()) == 0);   // loop

    const std::string want = real + "/a.txt";
    CHECK(PathUtil::canonical(root + "/real/a.txt") == want);
    CHECK(PathUtil::canonical(root + "/link/a.txt") == want);          // the reported bug
    CHECK(PathUtil::canonical(root + "/rellink/a.txt") == want);
    CHECK(PathUtil::canonical(root + "/link2/a.txt") == want);
    CHECK(PathUtil::canonical(root + "/link/../real/a.txt") == want);  // ".." after a symlink
    CHECK(PathUtil::canonical(other + "/file_link.txt") == want);      // symlink to the file itself
    CHECK(PathUtil::canonical(root + "/link/./a.txt") == want);

    // files that don't exist yet (untitled / Save As) resolve through the directory
    CHECK(PathUtil::canonical(root + "/link/new.txt") == real + "/new.txt");
    CHECK(PathUtil::canonical(root + "/link2/new.txt") == real + "/new.txt");
    CHECK(PathUtil::canonical("/nonexistent_dir_xyz/new.txt") == "/nonexistent_dir_xyz/new.txt");
    CHECK(PathUtil::canonical("/new_in_root_xyz") == "/new_in_root_xyz");
    CHECK(PathUtil::canonical("") == "");

    // loops must not hang or crash
    std::string l = PathUtil::canonical(root + "/loop_a/x.txt");
    CHECK(!l.empty());

    CHECK(PathUtil::is_symlink(root + "/link"));
    CHECK(PathUtil::is_symlink(other + "/file_link.txt"));
    CHECK(!PathUtil::is_symlink(real));
    CHECK(!PathUtil::is_symlink(want));
    CHECK(!PathUtil::is_symlink(root + "/missing"));
    CHECK(!PathUtil::is_symlink(root + "/link/a.txt"));   // only the last component counts

    std::string cmd = "rm -rf '" + root + "'";
    if (std::system(cmd.c_str()) != 0) std::cerr << "cleanup failed\n";
    std::cout << (checks - failures) << "/" << checks << " path checks passed\n";
    return failures ? 1 : 0;
}
