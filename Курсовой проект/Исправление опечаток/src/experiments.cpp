#include "../include/defs.hpp"
#include "../include/typos_correction.hpp"

int wmain()
{
    /* Настройка кодировок */
    setlocale(LC_CTYPE, "rus");
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    wstring S = L"url";
    wstring T = L"филма";

    vector<double> work;
    dict<wchar, int> iwork;

    double dist = DamerauLevenshteinDistance(S, T, work, iwork);
    
    return 0;
}