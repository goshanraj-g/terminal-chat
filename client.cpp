#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <cstring>

#include "common.hpp"

const char kServerIp[] = "127.0.0.1";
constexpr uint16_t kPort = 10000;
constexpr size_t kMaxLineLen = 200;
// eval at compile time

// ensures thread-safe access to g_quit
std::atomic<bool> g_quit = false;

// dials server
SOCKET dial_server()
{
    // create TCP socket with IPv4
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kPort);
    // convert server IP to binary, and stores in socket address
    inet_pton(AF_INET, kServerIp, &addr.sin_addr);
    // connect to the remote server
    connect(s, reinterpret_cast<sockaddr *>(&addr), sizeof addr);
    return s;
}

// console colors
void ansi_on()
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m;
    GetConsoleMode(h, &m);
    SetConsoleMode(h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

void listener(SOCKET s)
{
    char name[kMaxLineLen]{}, line[kMaxLineLen]{};
    int color_id;
    char color_buffer[sizeof(int)];

    while (!g_quit.load())
    {
        if (recv(s, name, sizeof name, 0) <= 0)
            break;

        // receive into char buffer first, then copy to int
        if (recv(s, color_buffer, sizeof(color_buffer), 0) <= 0)
            break;
        std::memcpy(&color_id, color_buffer, sizeof(color_id));

        if (recv(s, line, sizeof line, 0) <= 0)
            break;

        if (std::strcmp(name, "#NULL") == 0)
            std::cout << pick_color(color_id) << line << kReset << '\n';
        else
            std::cout << pick_color(color_id) << name << " : " << kReset << line << '\n';
        std::cout << pick_color(1) << "You : " << kReset << std::flush;
    }
    g_quit = true;
}

int main()
{
    ansi_on();
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET s = dial_server();
    if (s == INVALID_SOCKET)
    {
        std::cerr << "Connection failed\n";
        return 1;
    }

    char nick[kMaxLineLen];
    std::cout << "Enter your name : ";
    std::cin.getline(nick, sizeof nick);
    send(s, nick, sizeof nick, 0);
    std::cout << pick_color(5)
              << "\n ==== Connected.  type #exit to quit ====\n"
              << kReset;

    // create new thread to execute listener function while passing s as arguement
    std::thread t(listener, s);
    char line[kMaxLineLen];

    while (!g_quit.load())
    {
        std::cout << pick_color(1) << "You : " << kReset << std::flush;
        if (!std::cin.getline(line, sizeof line))
            break;
        send(s, line, sizeof line, 0);
        if (std::strcmp(line, "#exit") == 0)
        {
            g_quit = true;
            break;
        }
    }
    t.join();
    closesocket(s);
    WSACleanup();
}