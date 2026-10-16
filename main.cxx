#include <FL/Fl.H>
#include <FL/filename.H>
#include <fstream>
#include <string>
#include <algorithm>
#include <cstdlib>

#ifndef _WIN32
#include <unistd.h>
#include <signal.h>
#else
#include <windows.h>
#endif

#include "TextEditorApp.h"

bool is_process_running(int pid) {
    if (pid <= 0) return false;
#ifndef _WIN32
    return (kill(pid, 0) == 0);
#else
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process) {
        DWORD exit_code;
        GetExitCodeProcess(process, &exit_code);
        CloseHandle(process);
        return (exit_code == STILL_ACTIVE);
    }
    return false;
#endif
}

std::string get_lock_file_path() {
    const char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (home) return std::string(home) + "/.flitpad_lock";
    return ".flitpad_lock";
}

std::string get_command_file_path() {
    const char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (home) return std::string(home) + "/.flitpad_commands";
    return ".flitpad_commands";
}

int main(int argc, char** argv) {
    std::string lock_path = get_lock_file_path();
    std::ifstream lock_in(lock_path.c_str());
    int existing_pid = 0;
    if (lock_in >> existing_pid) {
        lock_in.close();
        if (is_process_running(existing_pid)) {
            std::string cmd_path = get_command_file_path();
            std::ofstream cmd_out(cmd_path.c_str(), std::ios::app);
            for (int i = 1; i < argc; ++i) {
                if (argv[i][0] != '-') {
                    char abs_p[1024];
                    if (fl_filename_absolute(abs_p, sizeof(abs_p), argv[i])) {
                        cmd_out << abs_p << "\n";
                    } else {
                        cmd_out << argv[i] << "\n";
                    }
                }
            }
            cmd_out.close();
            return 0; 
        }
    }

    std::ofstream lock_out(lock_path.c_str());
    if (lock_out.is_open()) {
#ifndef _WIN32
        lock_out << (int)getpid() << "\n";
#else
        lock_out << (int)GetCurrentProcessId() << "\n";
#endif
        lock_out.close();
    }

    Fl::scheme("gtk+");
    Fl::background(40, 44, 52);
    Fl::background2(33, 37, 43);
    Fl::foreground(171, 178, 191);

    TextEditorApp window(1200, 800, "FlitPad");
    window.show(argc, argv);

    bool opened_any = false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            std::string path = argv[i];
            window.open_file(path);
            opened_any = true;

            if (i == 1) {
                std::string abs_path = path;
                if (path.rfind("file://", 0) == 0) {
                    abs_path = path.substr(7);
                    if (abs_path.rfind("localhost", 0) == 0) abs_path = abs_path.substr(9);
                }
                char full_p[1024];
                if (fl_filename_absolute(full_p, sizeof(full_p), abs_path.c_str())) {
                    abs_path = full_p;
                }
                std::replace(abs_path.begin(), abs_path.end(), '\\', '/');
                size_t last_slash = abs_path.find_last_of('/');
                if (last_slash != std::string::npos) {
                    std::string dir = abs_path.substr(0, last_slash);
                    window.set_current_dir(dir);
                }
            }
        }
    }

    if (!opened_any) {
        window.load_session();
    }

    return Fl::run();
}
