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