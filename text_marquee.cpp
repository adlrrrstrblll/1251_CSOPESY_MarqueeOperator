// mini_os_emulator.cpp
// g++ -std=c++11 -pthread mini_os_emulator.cpp -o mini_os_emulator
// ./mini_os_emulator
//
// This is a mini "OS emulator" that has a scrolling text (marquee) and a simple command system.
// You can start/stop the marquee, change the text, change its speed, and see its status.
// Everything happens live while you type commands.

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <sstream>

using namespace std;

// ===== GLOBAL VARIABLES =====

// whether the marquee is running or not
atomic<bool> marquee_running(false);

// speed of scrolling (in ms) – default is 200ms per shift
atomic<int> marquee_speed_ms(200);

// text that we scroll across the screen
string marquee_text = "Hello, World!";

// so our threads don't print on top of each other
mutex console_mutex;

// the two threads we run in the background
thread marquee_thread;
thread input_thread;

// turn a string into all lowercase (used for commands like START_MARQUEE)
string to_lower_copy(const string &s) {
    string r = s;
    transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return tolower(c); });
    return r;
}

// ===== THE MARQUEE WORKER =====
// this is the function that runs in the background to make the text scroll
void marquee_worker() {
    while (true) {
        // if marquee is off, just sleep a bit then loop
        if (!marquee_running.load()) {
            this_thread::sleep_for(chrono::milliseconds(50));
            continue;
        }

        {
            lock_guard<mutex> lock(console_mutex);

            // save cursor position so we don't mess up where the user is typing
            cout << "\033[s";

            // jump to a fixed row (so marquee always appears in the same place)
            cout << "\033[10;1H";

            // clear the whole line first (so old text disappears)
            cout << "\033[2K";

            // print the current marquee text
            cout << "[MARQUEE] " << (marquee_text.empty() ? "" : marquee_text) << flush;

            // go back to where user was typing
            cout << "\033[u";
            cout << flush;
        }

        // shift the text (rotate left)
        if (!marquee_text.empty()) {
            char first = marquee_text.front();
            marquee_text.erase(0, 1);
            marquee_text.push_back(first);
        }

        // wait before showing next frame
        int ms = marquee_speed_ms.load();
        if (ms < 10) ms = 10; // don't go too fast
        this_thread::sleep_for(chrono::milliseconds(ms));
    }
}

// shows the list of commands you can type
void show_help() {
    lock_guard<mutex> lock(console_mutex);
    cout << "\nCommands:\n"
         << " help           - show this command list\n"
         << " start_marquee  - start scrolling text\n"
         << " stop_marquee   - stop scrolling text\n"
         << " set_text       - set marquee message\n"
         << " set_speed      - set scroll speed (milliseconds)\n"
         << " show_status    - show current marquee settings\n"
         << " exit           - close emulator\n\n";
}

// shows what's going on with the marquee (running? what text? what speed?)
void show_status() {
    lock_guard<mutex> lock(console_mutex);
    cout << "\nStatus:\n";
    cout << " marquee running : " << (marquee_running.load() ? "YES" : "NO") << "\n";
    cout << " marquee text    : \"" << marquee_text << "\"\n";
    cout << " marquee speed   : " << marquee_speed_ms.load() << " ms\n\n";
}

// little string helpers to remove extra spaces
static inline void ltrim(string &s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](int ch) { return !isspace(ch); }));
}
static inline void rtrim(string &s) {
    s.erase(find_if(s.rbegin(), s.rend(), [](int ch) { return !isspace(ch); }).base(), s.end());
}
static inline void trim(string &s) { ltrim(s); rtrim(s); }

// redraws the "> " prompt so it always looks clean
void redraw_prompt(const string &current_input = "") {
    lock_guard<mutex> lock(console_mutex);
    cout << "\033[2K\r> " << current_input << flush;
}

// ===== INPUT WORKER =====
// runs in another thread, just waits for user commands and runs them
void input_worker() {
    string raw;
    while (true) {
        redraw_prompt(); // show "> " waiting for input

        if (!getline(cin, raw)) {
            break; // if input is closed, exit
        }
        string cmdline = raw;
        trim(cmdline);
        if (cmdline.empty()) continue;

        stringstream ss(cmdline);
        string cmd;
        ss >> cmd;
        string rest;
        getline(ss, rest);
        trim(rest);

        string cmd_l = to_lower_copy(cmd); // lowercase command

        if (cmd_l == "help") {
            show_help();
        } else if (cmd_l == "exit") {
            {
                lock_guard<mutex> lock(console_mutex);
                cout << "\033[2J\033[H\033[?25h"; // clear screen, reset cursor, show cursor
                cout << "Exiting... stopping marquee and closing.\n\n";
            }
            marquee_running.store(false);
            break;
        } else if (cmd_l == "start_marquee" || cmd_l == "start") {
            if (marquee_running.load()) {
                cout << "Marquee already running.\n\n";
            } else {
                marquee_running.store(true);
                cout << "\033[?25l"; // hide cursor while marquee is running
                cout << "Marquee started.\n\n";
            }
        } else if (cmd_l == "stop_marquee" || cmd_l == "stop") {
            if (!marquee_running.load()) {
                cout << "Marquee already stopped.\n\n";
            } else {
                marquee_running.store(false);
                cout << "\033[s\033[2A\033[2K\033[u"; // clear marquee line
                cout << "\033[?25h"; // show cursor again
                cout << "Marquee stopped. (Screen cleared)\n\n";
            }
        } else if (cmd_l == "set_text") {
            if (!rest.empty()) {
                marquee_text = rest;
                cout << "Marquee text set to: \"" << marquee_text << "\"\n\n";
            }
        } else if (cmd_l == "set_speed") {
            string value = rest;
            if (value.empty()) {
                cout << "Enter speed in milliseconds (e.g., 200): ";
                string s;
                if (!getline(cin, s)) { break; }
                trim(s);
                value = s;
            }
            try {
                int v = stoi(value);
                if (v < 10) {
                    cout << "Speed too small; set to minimum 10 ms.\n\n";
                    v = 10;
                }
                marquee_speed_ms.store(v);
                cout << "Marquee speed set to " << v << " ms.\n\n";
            } catch (...) {
                cout << "Invalid number. Please enter milliseconds as an integer.\n\n";
            }
        } else if (cmd_l == "show_status") {
            show_status();
        } else {
            cout << "Unknown command. Type 'help' for a list.\n\n";
        }
    }
}

// ===== MAIN FUNCTION =====
int main() {
    // start the marquee thread right away (it will just wait until start is called)
    marquee_thread = thread(marquee_worker);

    {
        lock_guard<mutex> lock(console_mutex);
        cout << "========================================\n";
        cout << "  Mini-OS Emulator (CLI + Marquee)\n";
        cout << "  Type 'help' to see available commands.\n";
        cout << "========================================\n\n";
    }

    // start listening for commands
    input_thread = thread(input_worker);
    input_thread.join();

    // cleanup before exit
    {
        lock_guard<mutex> lock(console_mutex);
        cout << "\033[2J\033[H\033[?25h"; // clear screen, reset cursor, show cursor
        cout << "Goodbye!\n";
    }
    if (marquee_thread.joinable()) marquee_thread.detach();

    return 0;
}
