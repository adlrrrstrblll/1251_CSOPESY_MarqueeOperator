#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <windows.h>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <conio.h>
#include <sstream>
using namespace std;

// ---------------- GLOBALS ----------------
atomic<bool> marqueeRunning(false);
int speed = 50;
mutex console_mutex;

const vector<string> boatBase = {
    "    __|__ |___| |\\",
    "    |o__| |___| | \\",
    "    |___| |___| |o \\",
    "   _|___| |___| |__o\\",
    "  /...\\_____|___|____\\_/",
    "  \\   o * o * * o o  /"
};

// ---------------- UTILITIES ----------------
void setCursorPosition(int x, int y) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(hOut, pos);
}

// Boat animation states
enum class BoatState { APPROACH, CRASH, SINK, RESET };

// ---------------- BOAT ANIMATION ----------------
void runMarquee() {
    const int boatWidth = 22;
    const int boatHeight = boatBase.size();
    const int screenWidth = 100;
    const int screenHeight = boatHeight + 8;
    const int commandLine = screenHeight + 2;

    float frame = 0.0f;
    BoatState state = BoatState::APPROACH;
    int crashFrame = 0;
    int sinkFrame = 0;

    while (marqueeRunning) {
        // full overwrite every frame
        vector<vector<char>> screen(screenHeight, vector<char>(screenWidth, ' '));

        int boat1Pos = static_cast<int>(frame * 0.5f);
        int boat2Pos = static_cast<int>(screenWidth - boatWidth - frame * 0.5f);

        int yBase = screenHeight - 4 - boatHeight;

        vector<string> boat1 = boatBase;
        vector<string> boat2 = boatBase;

        // state machine
        if (state == BoatState::APPROACH) {
            if (boat1Pos + boatWidth >= boat2Pos) {
                state = BoatState::CRASH;
                crashFrame = 0;
            }
        }
        else if (state == BoatState::CRASH) {
            for (int i = 0; i < boatHeight; i++) {
                boat1[i] = string(i / 2, ' ') + boat1[i];
                boat2[i] = string((boatHeight - i) / 2, ' ') + boat2[i];
            }
            crashFrame++;
            if (crashFrame > 20) {
                state = BoatState::SINK;
                sinkFrame = 0;
            }
        }
        else if (state == BoatState::SINK) {
            yBase += sinkFrame / 2;
            sinkFrame++;
            if (yBase > screenHeight) {
                state = BoatState::RESET;
            }
        }
        else if (state == BoatState::RESET) {
            frame = 0.0f;
            state = BoatState::APPROACH;
            continue;
        }

        // draw boats
        for (int i = 0; i < boatHeight; i++) {
            int y = yBase + i;
            if (y >= 0 && y < screenHeight) {
                for (int x = 0; x < (int)boat1[i].size(); x++) {
                    int pos1 = boat1Pos + x;
                    int pos2 = boat2Pos + x;
                    if (pos1 >= 0 && pos1 < screenWidth) screen[y][pos1] = boat1[i][x];
                    if (pos2 >= 0 && pos2 < screenWidth) screen[y][pos2] = boat2[i][x];
                }
            }
        }

        // draw waves
        for (int y = screenHeight - 3; y < screenHeight; y++) {
            for (int x = 0; x < screenWidth; x++) {
                double wave = sin((x + frame * 0.5) * 0.5);
                if (static_cast<int>(wave * 2) == y - (screenHeight - 3)) {
                    if (state == BoatState::CRASH && rand() % 6 == 0)
                        screen[y][x] = '^';
                    else
                        screen[y][x] = '~';
                }
            }
        }

        {
            lock_guard<mutex> lock(console_mutex);
            setCursorPosition(0, 0);
            for (int y = 0; y < screenHeight; y++) {
                for (int x = 0; x < screenWidth; x++) cout << screen[y][x];
                cout << "\n";
            }
            // padding
            for (int y = 0; y < 3; y++)
                cout << string(screenWidth, ' ') << "\n";

            setCursorPosition(0, commandLine);
        }

        if (state == BoatState::APPROACH) frame += 1.0f;
        Sleep(speed);
    }
}

void printOutput(const string &msg) {
    const int commandLine = boatBase.size() + 12; // below input
    lock_guard<mutex> lock(console_mutex);

    setCursorPosition(0, commandLine);
    cout << "\033[2K"; // clear line
    cout << msg << "\n" << flush;
}

// ---------------- COMMAND HELPERS ----------------
void showHelp() {
    printOutput(
        "Commands:\n"
        " help           - show this command list\n"
        " start_marquee  - start boat animation\n"
        " stop_marquee   - stop animation\n"
        " set_speed <n>  - set animation speed (ms)\n"
        " show_status    - show current status\n"
        " exit           - close emulator\n"
    );
}

void showStatus() {
    stringstream ss;
    ss << "Status:\n";
    ss << " animation running : " << (marqueeRunning ? "YES" : "NO") << "\n";
    ss << " animation speed   : " << speed << " ms/frame\n";
    printOutput(ss.str());
}

void startAnimation() {
    if (!marqueeRunning) {
        marqueeRunning = true;
        thread(runMarquee).detach();
        printOutput("Animation started.");
    } else {
        printOutput("Animation already running.");
    }
}

void stopAnimation() {
    if (marqueeRunning) {
        marqueeRunning = false;
        printOutput("Animation stopped.");
    } else {
        printOutput("Animation already stopped.");
    }
}

// ---------------- INPUT WORKER ----------------
void inputWorker() {
    string current_input;
    const int commandLine = boatBase.size() + 10;

    while (true) {
        {
            lock_guard<mutex> lock(console_mutex);
            setCursorPosition(0, commandLine);
            cout << "\033[2K"; // clear line
            cout << "> " << current_input << flush;
        }

        if (_kbhit()) {
            char c = _getch();
            if (c == '\r') { // Enter pressed
                string cmd = current_input;
                current_input.clear();

                // refresh screen before showing output
                {
                    lock_guard<mutex> lock(console_mutex);
                    cout << "\033[2J\033[H";
                }

                if (cmd == "exit") {
                    marqueeRunning = false;
                    break;
                }
                else if (cmd == "help") showHelp();
                else if (cmd == "start_marquee") startAnimation();
                else if (cmd == "stop_marquee") stopAnimation();
                else if (cmd.rfind("set_speed", 0) == 0) {
                    try {
                        int v = stoi(cmd.substr(9));
                        if (v > 0) {
                            speed = v;
                            cout << "Speed set to " << v << " ms.\n";
                        }
                    } catch (...) {
                        cout << "Invalid number.\n";
                    }
                }
                else if (cmd == "show_status") showStatus();
                else if (!cmd.empty()) {
                    cout << "Unknown command: " << cmd << "\n";
                }
            }
            else if (c == 8) { // backspace
                if (!current_input.empty()) current_input.pop_back();
            }
            else {
                current_input.push_back(c);
            }
        }

        this_thread::sleep_for(chrono::milliseconds(50));
    }
}

// ---------------- MAIN ----------------
int main() {
    cout << "=== Mini-OS Emulator (Boat) ===\n";
    cout << "Type 'help' to see available commands.\n";

    thread inputThread(inputWorker);
    inputThread.join();

    return 0;
}
