#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cmath>
#include <cstring>
using namespace std;

// === Globals ===
atomic<bool> marqueeRunning(false);
int marqueeSpeed = 30; // ms per frame
thread marqueeThread;

// === Intro screen ===
void intro() {
    cout << R"( 
    ██████╗███████╗ ██████╗ ██████╗ ███████╗███████╗██╗   ██╗
    ██╔════╝██╔════╝██╔═══██╗██╔══██╗██╔════╝██╔════╝╚██╗ ██╔╝
    ██║     ███████╗██║   ██║██████╔╝█████╗  ███████╗ ╚████╔╝ 
    ██║     ╚════██║██║   ██║██╔═══╝ ██╔══╝  ╚════██║  ╚██╔╝  
    ╚██████╗███████║╚██████╔╝██║     ███████╗███████║   ██║   
    ╚═════╝╚══════╝ ╚═════╝ ╚═╝     ╚══════╝╚══════╝   ╚═╝   
        )" << endl;

    cout << "Group Developer:" << endl;
    cout << R"(
        1. name 1
        2. name 2
        3. name 3
        4. name 4
    )" << endl;

    cout << "Version Date: September 14, 2025" << endl;
}

// === Help menu ===
void help() {
    cout << "\nAvailable commands:\n";
    cout << " help          - Show this help menu\n";
    cout << " start_marquee - Start the donut animation\n";
    cout << " stop_marquee  - Stop the donut animation\n";
    cout << " set_speed     - Change the animation speed (ms)\n";
    cout << " exit          - Quit the program\n\n";
}

// === Donut animation loop ===
void marqueeLoop() {
    float A = 0, B = 0;
    float i, j;
    float z[1760];
    char b[1760];

    cout << "\x1b[2J"; // clear screen

    while (marqueeRunning) {
        memset(b, 32, 1760);
        memset(z, 0, 7040);

        for (j = 0; j < 6.28; j += 0.07) {
            for (i = 0; i < 6.28; i += 0.02) {
                float c = sin(i),
                      d = cos(j),
                      e = sin(A),
                      f = sin(j),
                      g = cos(A),
                      h = d + 2,
                      D = 1 / (c * h * e + f * g + 5),
                      l = cos(i),
                      m = cos(B),
                      n = sin(B),
                      t = c * h * g - f * e;

                int x = 40 + 30 * D * (l * h * m - t * n);
                int y = 12 + 15 * D * (l * h * n + t * m);
                int o = x + 80 * y;
                int N = 8 * ((f * e - c * d * g) * m - c * d * e - f * g - l * d * n);

                if (y > 0 && y < 22 && x > 0 && x < 80 && D > z[o]) {
                    z[o] = D;
                    b[o] = ".,-~:;=!*#$@"[N > 0 ? N : 0];
                }
            }
        }

        cout << "\x1b[H"; // move cursor home
        for (int k = 0; k < 1760; k++) {
            putchar(k % 80 ? b[k] : '\n');
        }

        A += 0.04;
        B += 0.02;

        this_thread::sleep_for(chrono::milliseconds(marqueeSpeed));
    }
}

// === Main driver ===
int main() {
    string command;

    intro();

    while (true) {
        cout << "\nCommand> ";
        cin >> command;

        if (command == "help") {
            help();
        }
        else if (command == "start_marquee") {
            if (!marqueeRunning) {
                marqueeRunning = true;
                marqueeThread = thread(marqueeLoop);
                cout << "Donut animation started.\n";
            } else {
                cout << "Animation is already running.\n";
            }
        }
        else if (command == "stop_marquee") {
            if (marqueeRunning) {
                marqueeRunning = false;
                if (marqueeThread.joinable()) marqueeThread.join();
                cout << "Animation stopped.\n";
            } else {
                cout << "No animation is running.\n";
            }
        }
        else if (command == "set_speed") {
            cout << "Enter speed (ms): ";
            cin >> marqueeSpeed;
        }
        else if (command == "exit") {
            marqueeRunning = false;
            if (marqueeThread.joinable()) marqueeThread.join();
            cout << "Exiting program.\n";
            break;
        }
        else {
            cout << "Unknown command. Type 'help' for a list of commands.\n";
        }
    }

    return 0;
}
