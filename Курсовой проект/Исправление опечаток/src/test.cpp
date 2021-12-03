#include <stdio.h>
#include <iostream>
#include <string>
#include <set>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <queue>
#include <ctime>
#include <time.h>
#include <clocale>
#include <limits.h>
#include <random>
#include <omp.h>

#include "../include/create_index.hpp"
#include "../include/bool_search.hpp"
#include "../include/token_parse.hpp"
#include "../include/docs_parse.hpp"

using hrc = std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::microseconds;

#define INFO_HANDLE(message, ...)                                   \
do                                                                  \
{                                                                   \
     fwprintf(stdout, L"[INFO] " message L"\n", __VA_ARGS__);       \
}                                                                   \
while(0);

#define ERROR_HANDLE(call, message, ...)                                      \
do                                                                            \
{                                                                             \
    if(!(call))                                                               \
    {                                                                         \
        fwprintf(stdout, L"[ERROR] " message L"\n", __VA_ARGS__);             \
        exit(0);                                                              \
    }                                                                         \
}                                                                             \
while(0);

#define DESCRIPTION  L"Замерить время выполнения операций\n"                          \
                     L"usage: ./Булев\\ поиск.exe <commands>\n"                       \
                     L"-i \'абс. путь к корпусу\'\n"                                  \
                     L"-o \'абс. путь к индексу\'\n"                                  \
                     L"-n1 число_запросов\n"                                          \
                     L"-n2 длина_запросы_в_термах"

wstring get_random_term(vector<wstring> &terms, 
                        std::uniform_int_distribution<int> &distrib,
                        std::mt19937 &gen)
{
    return terms[distrib(gen)];
}

void create_random_request(int len, vector<wstring> &terms,
                           wstring &req,
                           std::uniform_int_distribution<int> &distrib,
                           std::mt19937 &gen)
{
    int i;
    req.resize(0);
    wstring term = L"";

    for (i = 0; i < len; i++)
    {
        term = get_random_term(terms, distrib, gen);

        if (rand() % 2 > 0)
        {
            req += L"!";
        }
        req += term;

        if (i != len - 1)
        {
            req += (i != len - 1 && rand() % 2 > 0) ? L" || " : L" && ";
        }
    }
}

wstr_to_wstr parse = lower_case; // простейшая функция парсинга токенов в термы

int wmain(int argc, wchar *argv[])
{
    setlocale(LC_CTYPE, "rus"); // кодировка на поток вывода
    SetConsoleCP(1251);         // кодировка на поток ввода
    SetConsoleOutputCP(1251);   // кодировка на поток вывода

    int i, j,
        n1 = 40,
        n2 = 7;

    uint n_docs;

    path path2corpus = L"", path2index = L"./index", path2blocks = L"./tmp";

    /* Разбор параметров входной строки */
    for (i = 1; i < argc; i++)
    {
        if (_wcsicmp(argv[i], L"-i") == 0)
        {
            ERROR_HANDLE(i + 1 < argc, DESCRIPTION);
            path2corpus = fs::absolute(argv[i + 1]);
            i++;
        }
        else if (_wcsicmp(argv[i], L"-o") == 0)
        {
            ERROR_HANDLE(i + 1 < argc, DESCRIPTION);
            path2index = fs::absolute(argv[i + 1]);
            i++;
        }
        else if (_wcsicmp(argv[i], L"-n1") == 0)
        {
            ERROR_HANDLE(i + 1 < argc, DESCRIPTION);
            n1 = _wtoi(argv[i + 1]);
            i++;
        }
        else if (_wcsicmp(argv[i], L"-n2") == 0)
        {
            ERROR_HANDLE(i + 1 < argc, DESCRIPTION);
            n2 = _wtoi(argv[i + 1]);
            i++;
        }
    }

    wchar path2docs_id[256], path2terms[256], path2postings_list[256];

    wcscpy(path2docs_id, (path2index / "docs_id.data").c_str());
    wcscpy(path2terms, (path2index / "terms.data").c_str());
    wcscpy(path2postings_list, (path2index / "postings_list.data").c_str());

    dict<wstring, int> terms_dict = {};
    vector<wstring> terms;
    vector<int> response = {};
    wstring line_request = L"";
    vector<wstring> infix_request = {},
                    postfix_request = {};
    
    hrc::time_point time_start, time_end;
    vector<int> durations(n1);

    load_index(path2index, terms_dict);
    terms.resize(terms_dict.size());

    std::mt19937 gen(42);
    std::uniform_int_distribution<int> distrib(0, terms.size() - 1);

    i = 0;
    for (auto ptr : terms_dict)
    {
        terms[i] = ptr.first;
        i++;
    }

    FILE *fp_docs = NULL, *fp_postings = NULL;

    ERROR_HANDLE(fp_docs = _wfopen(path2docs_id, L"rb"), L"Неверный путь");
    fread(&n_docs, sizeof(uint), 1, fp_docs);
    fclose(fp_docs);

    ERROR_HANDLE(fp_postings = _wfopen(path2postings_list, L"rb"), L"Неверный путь");

    wprintf(L"%13s||%13s||%13s||%13s\n",
            L"Номер запроса", L"Документов", L"Время, ms", L"Запрос");

    for (i = 0; i < n1; i++)
    {
        create_random_request(n2, terms, line_request,
                              distrib, gen);

        time_start = hrc::now();

        string_to_infix_request(line_request, infix_request, parse);

        infix_request_to_postfix_request(infix_request, postfix_request);

        get_response(postfix_request, fp_postings, terms_dict, n_docs, response);

        time_end = hrc::now();
        durations[i] = std::chrono::duration_cast<microseconds>(time_end - time_start).count();

        wprintf(L"%13d||%13d||%13d|| %s\n", i + 1, response.size(), durations[i], line_request.c_str());
    }

    double durations_sum = 0;

    for (i = 0; i < n1; i++)
    {
        durations_sum += durations[i];
    }
    fclose(fp_postings);

    wprintf(L"Всего времени %u mc, в среднем на запрос %u mc\n", (uint)durations_sum, (uint)round(durations_sum / n1));
    //system("pause");

    return 0;
}