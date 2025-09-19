#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <windows.h>
#include <algorithm>

using namespace std;

atomic<bool> marqueeRunning(false);
string marqueeText;
string scrolledText; 
int speed = 100;

HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
short marqueeLine; 

string scrollText(const string& text) {
    if (text.empty()) return text;
    return text.substr(1) + text[0];
}

void printMarquee(const string& text) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    short inputLine = csbi.dwCursorPosition.Y; 

    COORD pos = {0, marqueeLine}; 
    SetConsoleCursorPosition(hConsole, pos);
    cout << text << "                    ";

    COORD restore = {0, inputLine};
    SetConsoleCursorPosition(hConsole, restore);
    cout.flush();
}

void runMarquee() {
    while (marqueeRunning) {
        printMarquee(scrolledText);
        scrolledText = scrollText(scrolledText);
        Sleep(speed);
    }
}

void printHelp() {
    cout << "Commands:\n";
    cout << " start_marquee      - Start scrolling text\n";
    cout << " stop_marquee       - Stop scrolling text\n";
    cout << " set_text           - Change marquee text (keeps scroll position)\n";
    cout << " set_speed <number> - Change speed in milliseconds\n";
    cout << " help               - Show this list\n";
    cout << " exit               - Quit\n";
}

int main() {
    cout << "Enter initial marquee text: ";
    getline(cin, marqueeText);
    scrolledText = marqueeText; 

    cout << "====================================\n";
    cout << "   Mini-OS Emulator\n";
    cout << "   Type 'help' for commands\n";
    cout << "====================================\n";

    // reserve a line for the marquee
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    marqueeLine = csbi.dwCursorPosition.Y; 

    thread marqueeThread;
    string command;

    while (true) {
        cout << "\n> ";
        getline(cin, command);
        transform(command.begin(), command.end(), command.begin(), ::tolower);

        if (command == "help") {
            printHelp();
        }
        else if (command == "start_marquee") {
            if (!marqueeRunning) {
                if (scrolledText.empty()) scrolledText = marqueeText;
                marqueeRunning = true;
                marqueeThread = thread(runMarquee);
            } else {
                cout << "Marquee already running.\n";
            }
        }
        else if (command == "stop_marquee") {
            if (marqueeRunning) {
                marqueeRunning = false;
                if (marqueeThread.joinable()) marqueeThread.join();
            } else {
                cout << "Marquee not running.\n";
            }
        }
        else if (command == "set_text") {
            cout << "New text: ";
            string newText;
            getline(cin, newText);
            if (!newText.empty()) {
                int offset = 0;
                if (!scrolledText.empty() && scrolledText.size() == marqueeText.size()) {
                    auto pos = (marqueeText + marqueeText).find(scrolledText);
                    if (pos != string::npos) offset = pos;
                }

                marqueeText = newText;
                scrolledText = marqueeText;

                for (int i = 0; i < offset % (int)marqueeText.size(); i++)
                    scrolledText = scrollText(scrolledText);

                cout << "Text updated (position preserved).\n";
            }
        }
        else if (command.rfind("set_speed", 0) == 0) { 
            string arg = command.substr(9);
            arg.erase(0, arg.find_first_not_of(" \t"));

            if (!arg.empty() && all_of(arg.begin(), arg.end(), ::isdigit)) {
                int newSpeed = stoi(arg);
                if (newSpeed > 0) {
                    speed = newSpeed;
                    cout << "Marquee speed updated to " << speed << " ms.\n";
                } else {
                    cout << "Speed must be a positive number.\n";
                }
            } else {
                cout << "Invalid speed. Usage: set_speed <positive number>\n";
            }
        }
        else if (command == "exit") {
            if (marqueeRunning) {
                marqueeRunning = false;
                if (marqueeThread.joinable()) marqueeThread.join();
            }
            cout << "Goodbye!\n";
            break;
        }
        else {
            cout << "Unknown command.\n";
        }
    }

    if (marqueeThread.joinable()) marqueeThread.join();
    return 0;
}
