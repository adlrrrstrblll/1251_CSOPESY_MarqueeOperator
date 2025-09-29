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
#include <fstream>  // <-- needed for file reading
using namespace std;

// ---------------- GLOBALS ----------------
atomic<bool> marqueeRunning(false);
atomic<bool> animationActive(false);
int speed = 50;
mutex console_mutex;

// --- Load boat art from file (no fallback) ---
vector<string> loadBoatFromFile(const string &filename) {
    vector<string> boat;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open " << filename << ". Make sure the file exists.\n";
        return boat;
    }

    string line;
    while (getline(file, line)) {
        boat.push_back(line);
    }
    file.close();

    if (boat.empty()) {
        cerr << "Error: " << filename << " is empty. Please add boat ASCII art.\n";
    }
    return boat;
}

// --- Load boat art at startup ---
const vector<string> boatBase = loadBoatFromFile("boat.txt");

// ---------------- UTILITIES ----------------
void setCursorPosition(int x, int y) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(hOut, pos);
}

// Boat animation states
enum class BoatState { APPROACH, CRASH, TILT, SINK, RESET };

// ---------------- BOAT ANIMATION ----------------
void runMarquee() {
    const int boatWidth = boatBase.empty() ? 0 : boatBase[0].size();
    const int boatHeight = boatBase.size();
    const int screenWidth = 100;
    const int screenHeight = boatHeight + 8;
    const int commandLine = screenHeight + 2;

    float frame = 0.0f;
    BoatState state = BoatState::APPROACH;
    int shockFrame = 0;
    int sinkFrame = 0;

    while (marqueeRunning) {
        vector<vector<char>> screen(screenHeight, vector<char>(screenWidth, ' '));

        int boat1Pos = static_cast<int>(frame * 0.5f);
        int boat2Pos = static_cast<int>(screenWidth - boatWidth - frame * 0.5f);
        int yBase = screenHeight - 4 - boatHeight;

        vector<string> boat1 = boatBase;
        vector<string> boat2 = boatBase;

        // --- state machine ---
        if (state == BoatState::APPROACH) {
            if (boat1Pos + boatWidth >= boat2Pos) {
                state = BoatState::CRASH;
                shockFrame = 0;
            }
        }
        else if (state == BoatState::CRASH) {
            for (int i = 0; i < boatHeight; i++) {
                boat1[i] = string(i / 2, ' ') + boat1[i];
                boat2[i] = string((boatHeight - i) / 2, ' ') + boat2[i];
            }

            int midX = (boat1Pos + boat2Pos + boatWidth) / 2;
            int splashY = yBase - 2;
            if (splashY >= 0 && splashY < screenHeight) {
                string splash = "\\  ^  ^  ^  /";
                int startX = midX - splash.size() / 2;
                for (int i = 0; i < (int)splash.size(); i++) {
                    int col = startX + i;
                    if (col >= 0 && col < screenWidth) screen[splashY][col] = splash[i];
                }
            }

            shockFrame++;
            if (shockFrame > 15) {
                state = BoatState::SINK;
                sinkFrame = 0;
            }
        }
        else if (state == BoatState::SINK) {
            yBase += sinkFrame / 2;
            sinkFrame++;

            for (int i = 0; i < boatHeight; i++) {
                boat1[i] = string(i / 3, ' ') + boat1[i];
                boat2[i] = string((boatHeight - i) / 3, ' ') + boat2[i];
            }

            if (sinkFrame < 40) {
                vector<char> fireChars = {'^', '*', '!', '#', '\''};
                for (int i = 0; i < boatHeight; i++) {
                    for (int j = 0; j < (int)boat1[i].size(); j++) {
                        if (boat1[i][j] != ' ' && rand() % 15 == 0)
                            boat1[i][j] = fireChars[rand() % fireChars.size()];
                        if (boat2[i][j] != ' ' && rand() % 15 == 0)
                            boat2[i][j] = fireChars[rand() % fireChars.size()];
                    }
                }
            }

            if (yBase > screenHeight) {
                state = BoatState::RESET;
            }
        }
        else if (state == BoatState::RESET) {
            frame = 0.0f;
            state = BoatState::APPROACH;
            continue;
        }

        if (state == BoatState::APPROACH || state == BoatState::CRASH || state == BoatState::SINK) {
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
        }

        for (int y = screenHeight - 3; y < screenHeight; y++) {
            for (int x = 0; x < screenWidth; x++) {
                double wave = sin((x + frame * 0.5) * 0.5);
                if (static_cast<int>(wave * 2) == y - (screenHeight - 3)) {
                    if (state == BoatState::CRASH) {
                        char shockChars[] = { '~', '^', '#', '*' };
                        screen[y][x] = shockChars[rand() % 4];
                    }
                    else if (state == BoatState::SINK && rand() % 6 == 0) {
                        screen[y][x] = '^';
                    }
                    else {
                        screen[y][x] = '~';
                    }
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
            for (int y = 0; y < 3; y++)
                cout << string(screenWidth, ' ') << "\n";

            setCursorPosition(0, commandLine);
        }

        if (animationActive && state == BoatState::APPROACH) {
            frame += 1.0f;
        }
        Sleep(speed);
    }
}

void printOutput(const string &msg) {
    const int commandLine = boatBase.size() + 12; 
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
        " exit           - close emulator\n"
    );
}

void startAnimation() {
    if (!marqueeRunning) {
        marqueeRunning = true;
        animationActive = true;
        thread(runMarquee).detach();
        printOutput("Animation started.");
    } else {
        animationActive = true;
        printOutput("Animation resumed.");
    }
}

void stopAnimation() {
    if (marqueeRunning && animationActive) {
        animationActive = false; 
        printOutput("Animation paused (boats idle).");
    } else if (!marqueeRunning) {
        printOutput("Animation not started yet.");
    } else {
        printOutput("Animation already paused.");
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
            cout << "\033[2K"; 
            cout << "> " << current_input << flush;
        }

        if (_kbhit()) {
            char c = _getch();
            if (c == '\r') { 
                string cmd = current_input;
                current_input.clear();

                {
                    lock_guard<mutex> lock(console_mutex);
                    cout << "\033[2J\033[H";
                }

                if (cmd == "exit") {
                    marqueeRunning = false; 
                    animationActive = false;
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
    if (boatBase.empty()) {
        cerr << "No boat art loaded. Exiting.\n";
        return 1;
    }

    cout << "=== Mini-OS Emulator (Boat) ===\n";
    cout << "Type 'help' to see available commands.\n";

    thread inputThread(inputWorker);
    inputThread.join();

    return 0;
}

