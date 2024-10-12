#pragma once

#if defined(_WIN32) 
//#define WIN32_LEAN_AND_MEAN 
//#include <Windows.h>
#include <locale.h>
static int __ForSupportWindowsUTF8 = ([] {
    setlocale(LC_ALL, ".utf-8");
    SetConsoleOutputCP(650001);
    SetConsoleCP(650001);
    }(), 0);
#endif
