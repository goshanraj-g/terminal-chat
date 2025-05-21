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

        say_console(std::string(pick_color(self->id)) + self->nick + ": " + kReset.data() + line, false);
    }

    // handle server disconnect
    // close socket
    closesocket(self->sock);
    {
        // locks global mutex guard, and ensures that no other thread can R/W to g_clients durnig this operation
        // lk(g_guard) also makes it so the lock is released when it goes out of the block
        std::lock_guard<std::mutex> lk(g_guard);
        g_clients.erase(
            // figures out which client to remove by ordering them to the very end, and checking id's
            std::remove_if(g_clients.begin(), g_clients.end(),
                           [id = self->id](const Client &c)
                           {
                               return c.id == id;
                           }),
            g_clients.end());

        // this lambda function checks each client in the vector, and returns true for the ones that id matches
    }
}

int main()
{
    ansi_on();

    // WinSock initialization (required before you can use any socket function)

    WSADATA wsa;                               // defining wsa as a WSADATA structure (has version + status)
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) // initializing the Winsock library, ver(2.2)
    {
        std::cerr << "WSAStartup failed\n";
        return 1;
    };

    SOCKET lsock = socket(AF_INET, SOCK_STREAM, 0); // creates TCP socket (listening socket) (IPv4, TCP, default protocol)
    if (lsock == INVALID_SOCKET)
    {
        std::cerr << "socket() failed\n";
        return 1;
    }

    sockaddr_in addr{};                // declares and zero-initializes sockadr_in struct hold (ip (IPv4) and port)
    addr.sin_family = AF_INET;         // sends address family to AF_INET (IPv4)
    addr.sin_addr.s_addr = INADDR_ANY; // binds socket to all network interfaces on machine
    // INADDR_ANY -> special constant allowing server to accept connections on localhost, local IP, and any other network interface
    addr.sin_port = htons(kPort); // sets port number the server will listen on, htons -> host to network short, this converts port number to big-endian

    if (bind(lsock, reinterpret_cast<sockaddr *>(&addr), sizeof addr) == SOCKET_ERROR) // associates lsock with local address
    // bind() expects sockaddr*, but there is only sockaddr_in, so it needs to be casted
    {
        std::cerr << "bind() failed\n";
        return 1;
    }

    // allows connections (tells listening socket to start listening to connection requests, SOMAXCONN -> maximum number of connections OS queue can hold)
    listen(lsock, SOMAXCONN);
    say_console(std::string(pick_color(5)) + "\n --- Windows Chat Room listening on port " + std::to_string(kPort) + " ---\n" + std::string(kReset));

    int seed = 0; // create a seed to assign UID to each new client
    while (true)
    {
        // infinite loop to accept new incoming client connections
        // accepts a new incoming client on the listening socket
        // once a client connects, the TCP handshake is completed, and returns a new socket (csock)
        SOCKET csock = accept(lsock, nullptr, nullptr); // the clients address info, and size of the address aren't captured
        if (csock == INVALID_SOCKET)
        {
            continue;
        }

        // create client struct, and initialize var

        Client cl;
        cl.id = ++seed;
        cl.sock = csock;
        cl.th = std::thread(serve_one, &cl); // spawns a new thread to run the function (serve_one)
        // the idea is that each client should be handled in it's own thread

        {
            // prevents other threads from reading/modifying g_guard when adding new client
            std::lock_guard lk(g_guard);
            // adds new client to end of g_clients, and uses move to move the client into the vector instead of copying, avoiding copying thread objects/socket handles
            g_clients.push_back(std::move(cl));
        }

        // detaches thread of most recently added client (gets last client which was added to g_clients, and detaches)
        // this means that the thread runs independently, and the main server thread does not wait for this to finish
        //
        g_clients.back().th.detach();
    }

    // close listening socket, and clean WSA
    closesocket(lsock);
    WSACleanup();
}