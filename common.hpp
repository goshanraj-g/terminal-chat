#pragma once           // file is only included once
#include <string_view> // read-only string
#include <array>       // fixed size array
#include <windows.h>

void enable_ansi()
{
    // get handle to console output
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    // get current console mode
    DWORD mode;
    GetConsoleMode(h, &mode);
    // enable VTP
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING); // virtual terminal processing allows for terminal to interpret ANSI escape sequences
}

// ansi escape codes to change color in terminal
inline constexpr std::array<std::string_view, 6> kAnsi = {
    "\x1b[31m", "\x1b[32m", "\x1b[33m",
    "\x1b[34m", "\x1b[35m", "\x1b[36m"};

// resets text formatting back to default
inline constexpr std::string_view kReset = "\x1b[0m";

[[notidcard]] inline std::string_view pick_color(int id)
{
    return kAnsi[id % kAnsi.size()]; // returns a color from it's id
}

// [[notidcard]] tells compiler to warn if this function is called, but ignore return value