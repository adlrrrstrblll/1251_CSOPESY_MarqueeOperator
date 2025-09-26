// mini_os_emulator.cpp
// g++ -std=c++11 -pthread mini_os_emulator.cpp -o mini_os_emulator.exe
// ./mini_os_emulator.exe

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <conio.h>
#include <windows.h> 

using namespace std;

// ===== GLOBAL VARIABLES =====
atomic<bool> marquee_running(false);
atomic<int> marquee_speed_ms(200);
string marquee_text = "Hello, World!";

mutex console_mutex;
thread marquee_thread;
thread input_thread;

string last_input;
mutex input_mutex;

// ===== ENABLE ANSI ESCAPES ON WINDOWS =====
void enable_ansi_escape() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

// ===== HELPERS =====
string to_lower_copy(const string &s) {
    string r = s;
    transform(r.begin(), r.end(), r.begin(),
              [](unsigned char c) { return tolower(c); });
    return r;
}
static inline void ltrim(string &s) {
    s.erase(s.begin(),
            find_if(s.begin(), s.end(), [](int ch) { return !isspace(ch); }));
}
static inline void rtrim(string &s) {
    s.erase(find_if(s.rbegin(), s.rend(),
                    [](int ch) { return !isspace(ch); }).base(),
            s.end());
}
static inline void trim(string &s) { ltrim(s); rtrim(s); }

// redraws the prompt
void redraw_prompt() {
    lock_guard<mutex> lock2(input_mutex);
    cout << "\033[12;1H"; // move to row 12
    cout << "\033[2K\r> " << last_input << flush;
}

// ===== MARQUEE WORKER =====
void marquee_worker() {
    while (true) {
        if (!marquee_running.load()) {
            this_thread::sleep_for(chrono::milliseconds(50));
            continue;
        }

        {
            lock_guard<mutex> lock(console_mutex);
            cout << "\033[10;1H";
            cout << "\033[2K";
            cout << "[MARQUEE] "
                 << (marquee_text.empty() ? "" : marquee_text) << flush;
            redraw_prompt();
        }

        if (!marquee_text.empty()) {
            char first = marquee_text.front();
            marquee_text.erase(0, 1);
            marquee_text.push_back(first);
        }

        int ms = marquee_speed_ms.load();
        if (ms < 10) ms = 10;
        this_thread::sleep_for(chrono::milliseconds(ms));
    }
}

// ===== COMMAND OUTPUT HELPERS =====
void show_help() {
    lock_guard<mutex> lock(console_mutex);
    cout << "\033[2J\033[H";
    cout << "Commands:\n"
         << " help           - show this command list\n"
         << " start_marquee  - start scrolling text\n"
         << " stop_marquee   - stop scrolling text\n"
         << " set_text       - set marquee message\n"
         << " set_speed      - set scroll speed (milliseconds)\n"
         << " show_status    - show current marquee settings\n"
         << " exit           - close emulator\n\n";
}
void show_status() {
    lock_guard<mutex> lock(console_mutex);
    cout << "\033[2J\033[H";
    cout << "Status:\n";
    cout << " marquee running : "
         << (marquee_running.load() ? "YES" : "NO") << "\n";
    cout << " marquee text    : \"" << marquee_text << "\"\n";
    cout << " marquee speed   : " << marquee_speed_ms.load() << " ms\n\n";
}

// ===== INPUT WORKER (RAW INPUT FOR WINDOWS) =====
void input_worker() {
    string current_input;

    while (true) {
        {
            lock_guard<mutex> lock(console_mutex);
            {
                lock_guard<mutex> lock2(input_mutex);
                last_input = current_input;
            }
            redraw_prompt();
        }

        if (!_kbhit()) {
            this_thread::sleep_for(chrono::milliseconds(50));
            continue;
        }

        char c = _getch();

        if (c == '\r') { // Enter
            string cmdline = current_input;
            current_input.clear();
            {
                lock_guard<mutex> lock(input_mutex);
                last_input.clear();
            }

            trim(cmdline);
            if (cmdline.empty()) continue;

            stringstream ss(cmdline);
            string cmd;
            ss >> cmd;
            string rest;
            getline(ss, rest);
            trim(rest);

            string cmd_l = to_lower_copy(cmd);

            {
                lock_guard<mutex> lock(console_mutex);
                cout << "\033[2J\033[H";
            }

            if (cmd_l == "help") {
                show_help();
            } else if (cmd_l == "exit") {
                {
                    lock_guard<mutex> lock(console_mutex);
                    cout << "Exiting... stopping marquee and closing.\n\n";
                }
                marquee_running.store(false);
                break;
            } else if (cmd_l == "start_marquee" || cmd_l == "start") {
                if (marquee_running.load()) {
                    cout << "Marquee already running.\n\n";
                } else {
                    marquee_running.store(true);
                    cout << "\033[?25l";
                    cout << "Marquee started.\n\n";
                }
            } else if (cmd_l == "stop_marquee" || cmd_l == "stop") {
                if (!marquee_running.load()) {
                    cout << "Marquee already stopped.\n\n";
                } else {
                    marquee_running.store(false);
                    cout << "\033[?25h";
                    cout << "Marquee stopped.\n\n";
                }
            } else if (cmd_l == "set_text") {
                if (!rest.empty()) {
                    marquee_text = rest;
                    cout << "Marquee text set to: \"" << marquee_text << "\"\n\n";
                }
            } else if (cmd_l == "set_speed") {
                try {
                    int v = stoi(rest);
                    if (v < 10) v = 10;
                    marquee_speed_ms.store(v);
                    cout << "Marquee speed set to " << v << " ms.\n\n";
                } catch (...) {
                    cout << "Invalid number. Please enter an integer.\n\n";
                }
            } else if (cmd_l == "show_status") {
                show_status();
            } else {
                cout << "Unknown command. Type 'help' for a list.\n\n";
            }
        }
        else if (c == 8) { // Backspace
            if (!current_input.empty()) current_input.pop_back();
        }
        else {
            current_input.push_back(c);
        }
    }
}

// ===== MAIN =====
int main() {
    enable_ansi_escape(); // make sure Windows console understands \033 codes

    marquee_thread = thread(marquee_worker);

    {
        lock_guard<mutex> lock(console_mutex);
        cout << "========================================\n";
        cout << "  Mini-OS Emulator (CLI + Marquee)\n";
        cout << "  Type 'help' to see available commands.\n";
        cout << "========================================\n\n";
    }

    input_thread = thread(input_worker);
    input_thread.join();

    {
        lock_guard<mutex> lock(console_mutex);
        cout << "\033[2J\033[H\033[?25h";
        cout << "Goodbye!\n";
    }
    if (marquee_thread.joinable()) marquee_thread.detach();

    return 0;
}
