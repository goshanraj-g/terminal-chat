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
            // call to client's socket with Winsock send() function
            // changed the data type inside of static_cast to cast to a char* since send() needs const char* buffer
        }
    }
}

void broadcast_text(string_view text, int except_id = -1)
{
    broadcast_raw(text.data(), static_cast<int>(text.size()) + 1, except_id);
}

void serve_one(Client *self)
{
    char line[kMaxLineLen]{};

    // winsock function to read data from socket, where self->sock is where it's reading from (the socket connected to the client)
    // line is the buffer where the data is stored
    if (recv(self->sock, line, sizeof line, 0) <= 0)
    {
        return;
    }
    // check if client discounnected or if an error occured before they set their name

    self->nick = line;

    string joined = self->nick + " has joined the chatroom!";

    // tells client server that this is a flag to notify the chatroom
    broadcast_text("#NULL");
    broadcast_raw(&self->id, sizeof self->id);
    broadcast_text(joined);
    say_console(std::string(pick_color(self->id)) + joined + std::string(kReset));

    while (int n = recv(self->sock, line, sizeof line, 0))
    {
        if (n <= 0)
            break;
        if (std::strcmp(line, "#exit") == 0)
        {
            std::string left = self->nick + " has left the chatroom!";
            broadcast_text("#NULL");
            broadcast_raw(&self->id, sizeof self->id);
            broadcast_text(left);
            say_console(std::string(pick_color(self->id)) + left + std::string(kReset));
            break;
        }

        broadcast_text(self->nick);
        broadcast_raw(&self->id, sizeof self->id);
        broadcast_text(line);

        say_console(std::string(pick_color(self->id)) + self->nick + " : " + kReset.data() + line, false);
    }
    closesocket(self->sock);
    {
        std::lock_guard lk(g_guard);
        std::erase_if(g_clients, [id = self->id](const Client &c)
                      { return c.id == id; });
    }
}