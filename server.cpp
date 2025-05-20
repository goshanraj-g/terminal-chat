#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <algorithm>
#include <string>
#include <cstring>

#include "common.hpp";
using namespace std;

constexpr uint16_t kPort = 10000;
constexpr size_t kMaxLineLen = 200;

struct Client
{
    int id{};
    SOCKET sock{};
    string nick = "Anon";
    thread th;
};

vector<Client> g_clients; // vector holding all connected clients
mutex g_guard;            // used to control access to shared data across multiple threads -- ensures only one threat at a time can modify/cread g_guard

void ansi_on() // enables vrt, enabling ansi colors in terminal
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;                                                   // variable to hold console mode flags
    GetConsoleMode(h, &mode);                                     // stores current console mode flags in h
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING); // add vrt
};

void say_console(string_view msg, bool nl = true)
{
    std::lock_guard lk(g_guard); // creates a lock on global mutex, ensuring exclusive access to console when message is printed
    // prevents multiple threads from alternating between their ouput (messy)
    std::cout << msg;
    if (nl)
        std::cout << "\n";
}

void broadcast_raw(const void *data, int bytes, int except_id = -1)
{
    std::lock_guard lk(g_guard);
    for (auto &c : g_clients)
    {
        if (c.id != except_id)
        {
            send(c.sock, static_cast<const char *>(data), bytes, 0);
        }
    }
}

void broadcast_text(string_view text, int except_id = -1)
{
    broadcast_raw(text.data(), static_cast<int>(text.size()) + 1, except_id);
}