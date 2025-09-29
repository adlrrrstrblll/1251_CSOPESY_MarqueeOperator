// g++ -std=c++11 text_marquee.cpp -o text_marquee.exe

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

atomic<bool> marquee_running(false);
atomic<bool> marquee_paused(false);
atomic<int> marquee_speed_ms(200);
string marquee_text = "Hello, World!";

mutex console_mutex;
thread marquee_thread;
thread input_thread;

string last_input;
mutex input_mutex;

void enable_ansi_escape() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

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

void redraw_prompt() {
    lock_guard<mutex> lock2(input_mutex);
    cout << "\033[12;1H";
    cout << "\033[2K\rCommand> " << last_input << flush;
}

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

        if (!marquee_paused.load() && !marquee_text.empty()) {
            char first = marquee_text.front();
            marquee_text.erase(0, 1);
            marquee_text.push_back(first);
        }

        int ms = marquee_speed_ms.load();
        if (ms < 10) ms = 10;
        this_thread::sleep_for(chrono::milliseconds(ms));
    }
}

void show_help() {
    lock_guard<mutex> lock(console_mutex);
    cout << "\033[2J\033[H";
    cout << "Commands:\n"
         << " help           - show this command list\n"
         << " start_marquee  - start scrolling text\n"
         << " stop_marquee   - stop scrolling text (freeze)\n"
         << " set_text       - set marquee message\n"
         << " set_speed      - set scroll speed (milliseconds)\n"
         << " exit           - close emulator\n\n";
}

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

        if (c == '\r') {
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
                if (marquee_running.load() && !marquee_paused.load()) {
                    cout << "Marquee already running.\n\n";
                } else {
                    marquee_running.store(true);
                    marquee_paused.store(false);
                    cout << "\033[?25l";
                    cout << "Marquee started.\n\n";
                }
            } else if (cmd_l == "stop_marquee" || cmd_l == "stop") {
                if (!marquee_running.load() || marquee_paused.load()) {
                    cout << "Marquee already stopped.\n\n";
                } else {
                    marquee_paused.store(true);
                    cout << "\033[?25h";
                    cout << "Marquee paused (frozen in place).\n\n";
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
            } else {
                cout << "Unknown command. Type 'help' for a list.\n\n";
            }
        }
        else if (c == 8) {
            if (!current_input.empty()) current_input.pop_back();
        }
        else {
            current_input.push_back(c);
        }
    }
}

void lock_console_view() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return;

    SHORT windowWidth  = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    SHORT windowHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    SMALL_RECT tempWindow = {0, 0, (SHORT)(windowWidth - 1), 0};
    SetConsoleWindowInfo(hOut, TRUE, &tempWindow);

    COORD newSize = {windowWidth, windowHeight};
    SetConsoleScreenBufferSize(hOut, newSize);

    SMALL_RECT finalWindow = {0, 0, (SHORT)(windowWidth - 1), (SHORT)(windowHeight - 1)};
    SetConsoleWindowInfo(hOut, TRUE, &finalWindow);

    DWORD mode;
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (GetConsoleMode(hIn, &mode)) {
        mode &= ~ENABLE_QUICK_EDIT_MODE;
        mode |= ENABLE_EXTENDED_FLAGS;
        SetConsoleMode(hIn, mode);
    }

    HWND hwnd = GetConsoleWindow();
    if (hwnd) {
        ShowScrollBar(hwnd, SB_BOTH, FALSE);
    }

    cout << "\033[2J\033[H";
}

int main() {
    enable_ansi_escape();
    lock_console_view();

    marquee_thread = thread(marquee_worker);

    {
        lock_guard<mutex> lock(console_mutex);
        cout << "========================================\n";
        cout << "  Welcome to CSOPESY!\n";
        cout << "  Group Developers:\n";
        cout << "    Sofia Ashley M. Aguete\n";
        cout << "    Chrisane Ianna B. Gaspar\n";
        cout << "    Evan Andrew J. Pinca\n";
        cout << "    Adler Clarence E. Strebel\n";
        cout << "  Version date: 01/01/2025\n";
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
