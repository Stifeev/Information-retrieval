#ifndef GUI_HEADERS_HPP
#define GUI_HEADERS_HPP

#include <shlobj_core.h>
#include <Windows.h>

#include <filesystem>
#include <algorithm>

#include <string>

#include <vector>
#include <queue>
#include <unordered_map>
#include <map>
#include <set>

#include <iostream>
#include <stdio.h>


namespace fs = std::filesystem;
using fs::path;

using std::map;
using uint = unsigned int;
using wchar = wchar_t;
using std::set;
using std::queue;
using std::pair;
#define dict std::unordered_map

using std::string;
using std::wstring;

using std::vector;
        
#define ButtonCreateIndexClicked 1
#define ButtonSearchClicked      2    
#define ButtonLoadIndexClicked   4
#define MenuExitClicked          5
#define MenuParamsClicked        6         

#define ParamsPath2CorpusButtonClicked   7
#define ParamsPath2IndexButtonClicked    8
#define ParamsPath2BlocksButtonClicked   9
#define ParamsPath2CorpusEditChanged     10
#define ParamsPath2IndexEditChanged      11
#define ParamsPath2BlocksEditChanged     12
#define ParamsEditNumThreadsChanged      13

#define IDC_list 15

#define IDC_ButtonLeft 16
#define IDC_ButtonRight 17
#define IDC_Label 18

/* Глобальные переменные */
HWND hMainWnd = NULL;        // дескриптор главного окна
HWND hParamsWnd = NULL;      // дескриптор дочернего окна с параметрами

HINSTANCE This = NULL;       // дескриптор приложения

HANDLE hConsole = NULL;      // дескриптор консоли
HANDLE hConsoleIn = NULL;

wchar_t buf[MAX_PATH] = L"";
wchar_t path2corpus[MAX_PATH] = L"";
wchar_t path2index[MAX_PATH] = L"";
wchar_t path2blocks[MAX_PATH] = L"";
int omp_num_threads = 4;
wstring last_request = L"";

dict<wstring, int> terms = {};
vector<pair<uint, int>> weights = {}; // вес -> документ
uint n_docs = 0;
vector<wstring> postfix_request = {};
set<wstring> request_terms = {};

int num_page = 1;
int n_pages = 1;

#define RADIUS_DETAILS 20

int Wndx = 3000;
int Wndy = 100;

wstr_to_wstr parse = lower_case; // простейшая функция парсинга токенов в термы

#define WARNING_HANDLE(call, action, message, ...)                            \
{                                                                             \
    if(!(call))                                                               \
    {                                                                         \
        fwprintf(stdout, L"[WARNING] " message L"\n", __VA_ARGS__);           \
        action;                                                               \
    }                                                                         \
}

#define INFO_HANDLE(message, ...)                                   \
{                                                                   \
     fwprintf(stdout, L"[INFO] " message L"\n", __VA_ARGS__);       \
}

#endif