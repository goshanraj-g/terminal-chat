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


