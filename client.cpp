#include "common.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <atomic>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

constexpr char kServerIp[] = "127.0.0.1";
constexpr uint16_t kPort = 10000;
constexpr size_t kMaxLineLen = 200;

std::atomic<bool> g_quit = false;

/* ───────────────────────── network helpers ─────────────────────────────── */
SOCKET dial_server()
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kPort);
    inet_pton(AF_INET, kServerIp, &addr.sin_addr);
    if (connect(s, reinterpret_cast<sockaddr *>(&addr), sizeof addr) == SOCKET_ERROR)
        return INVALID_SOCKET;
    return s;
}

void listener(SOCKET s)
{
    char name[kMaxLineLen]{}, line[kMaxLineLen]{};
    int color_id = 0;

    while (!g_quit.load())
    {
        if (recv(s, name, sizeof name, 0) <= 0)
            break;
        if (recv(s,
                 reinterpret_cast<char *>(&color_id), 
                 sizeof color_id,
                 0) <= 0)
            break;
            
        if (recv(s, line, sizeof line, 0) <= 0)
            break;

        if (std::strcmp(name, "#NULL") == 0)
            std::cout << pick_color(color_id) << line << kReset << '\n';
        else
            std::cout << pick_color(color_id) << name << " : "
                      << kReset << line << '\n';

        std::cout << pick_color(1) << "You : " << kReset << std::flush;
    }
    g_quit = true;
}

/* ---------------------------- main ---------------------------- */
int main()
{
    enable_ansi();
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa))
        return std::cerr << "WSAStartup failed\n", 1;

    SOCKET s = dial_server();
    if (s == INVALID_SOCKET)
        return std::cerr << "Connection failed\n", 1;

    char nick[kMaxLineLen]{};
    std::cout << "Enter your name : ";
    std::cin.getline(nick, sizeof nick);
    send(s, nick, static_cast<int>(std::strlen(nick)) + 1, 0);

    std::cout << pick_color(5)
              << "\n ==== Connected.  type #exit to quit ====\n"
              << kReset;

    std::thread t(listener, s);

    char line[kMaxLineLen]{};
    while (!g_quit.load())
    {
        std::cout << pick_color(1) << "You : " << kReset << std::flush;
        if (!std::cin.getline(line, sizeof line))
            break;

        send(s, line, static_cast<int>(std::strlen(line)) + 1, 0);
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
