#ifndef GUI_HEADERS_HPP
#define GUI_HEADERS_HPP

#include <shlobj_core.h>
#include <Windows.h>

#include "defs.hpp"

#define ButtonCreateIndexClicked 1
#define ButtonSearchClicked      2    
#define ButtonLoadIndexClicked   4
#define ButtonMergeIndexClicked  5
#define MenuExitClicked          6
#define MenuParamsClicked        7         

#define ParamsPath2CorpusButtonClicked   8
#define ParamsPath2IndexButtonClicked    9
#define ParamsPath2BlocksButtonClicked   10
#define ParamsPath2CorpusEditChanged     11
#define ParamsPath2IndexEditChanged      12
#define ParamsPath2BlocksEditChanged     13
#define ParamsEditNumThreadsChanged      14

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

wstring buf(MAX_PATH, L' ');
wstring path2corpus = L"укажите путь к корпусу";
wstring path2index = fs::current_path() / L"index";
wstring path2blocks = fs::current_path() / L"tmp";;
int omp_num_threads = 4;
wstring last_request = L"самый лучший фильм";

dict<wstring, item> terms = {};
vector<wstring> id2term = {};

dict<wstring, double> request_wt = {};
vector<pair<uint, double>> weights = {}; // вес -> документ
uint n_docs = 0;

int num_page = 1;
int n_pages = 1;

#define RADIUS_DETAILS 20

int Wndx = 3000;
int Wndy = 100;

path path2lemmatizator = L"python\\dist\\lemmatizator\\lemmatizator.exe";       // путь к скомпилированному скрипту
path path2request_parser = L"python\\dist\\request_parser\\request_parser.exe"; // путь к скомпилированному скрипту

void buf2str(const wstring &buf, wstring &str)
{
    int len = wcslen(buf.data());
    str.resize(len);

    std::copy(buf.begin(), buf.begin() + len, str.begin());
}

#endif