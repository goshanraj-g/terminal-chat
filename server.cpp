#include "common.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

constexpr uint16_t kPort = 10000;
constexpr size_t kMaxLineLen = 200;

struct Client
{
    int id{};
    SOCKET sock{};
    std::string nick = "Anon";
    std::thread th;
};

std::vector<Client> g_clients;
std::mutex g_guard;

// --------------------------- helpers --------------------
void say_console(std::string_view msg, bool nl = true)
{
    std::lock_guard lk(g_guard);
    std::cout << msg;
    if (nl)
        std::cout << '\n';
}

void broadcast_raw(const void *data, int bytes, int except_id = -1)
{
    std::lock_guard lk(g_guard);
    for (auto &c : g_clients)
        if (c.id != except_id)
            send(c.sock, static_cast<const char *>(data), bytes, 0);
}

void broadcast_text(std::string_view txt, int except_id = -1)
{
    broadcast_raw(txt.data(), static_cast<int>(txt.size()) + 1, except_id);
}

// ------------------------ worker -------------------------------------------
void serve_one(Client *self)
{
    char line[kMaxLineLen]{};

    /* --- 1st message = nickname ------------------------------------------ */
    if (recv(self->sock, line, sizeof line, 0) <= 0)
        return;
    self->nick = line;

    std::string joined = self->nick + " has joined the chatroom!";
    broadcast_text("#NULL");
    broadcast_raw(&self->id, sizeof self->id);
    broadcast_text(joined);
    say_console(std::string(pick_color(self->id)) + joined + std::string(kReset));

    /* --- main loop -------------------------------------------------------- */
    int n = 0;
    while ((n = recv(self->sock, line, sizeof line, 0)) > 0)
    {
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

        say_console(std::string(pick_color(self->id)) + self->nick +
                    ": " + kReset.data() + line);
    }

    /* --- cleanup ---------------------------------------------------------- */
    closesocket(self->sock);
    std::lock_guard lk(g_guard);
    g_clients.erase(std::remove_if(g_clients.begin(), g_clients.end(),
                                   [id = self->id](const Client &c)
                                   { return c.id == id; }),
                    g_clients.end());
}

int main()
{
    enable_ansi();

    /* WinSock init --------------------------------------------------------- */
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa))
        return say_console("WSAStartup failed"), 1;

    SOCKET lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock == INVALID_SOCKET)
        return say_console("socket() failed"), 1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(kPort);

    if (bind(lsock, reinterpret_cast<sockaddr *>(&addr), sizeof addr) == SOCKET_ERROR)
        return say_console("bind() failed"), 1;

    listen(lsock, SOMAXCONN);
    say_console(std::string(pick_color(5)) +
                " --- Windows Chat Room listening on port " + std::to_string(kPort) + " ---" +
                std::string(kReset));

    /* accept loop ---------------------------------------------------------- */
    int seed = 0;
    while (true)
    {
        SOCKET csock = accept(lsock, nullptr, nullptr);
        if (csock == INVALID_SOCKET)
            continue;

        /* allocate inside vector first, then start thread -- avoids
           the *dangling-pointer* bug of the original code             */
        std::lock_guard lk(g_guard);
        auto &cl = g_clients.emplace_back();
        cl.id = ++seed;
        cl.sock = csock;
        cl.th = std::thread(serve_one, &cl);
        cl.th.detach(); // runs independently
    }

    closesocket(lsock);
    WSACleanup();
}
