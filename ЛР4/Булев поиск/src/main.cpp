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
#include <omp.h>

#include "../include/create_index.hpp"
#include "../include/bool_search.hpp"
#include "../include/token_parse.hpp"
#include "../include/docs_parse.hpp"

namespace fs = std::filesystem;

using uint = unsigned int;
using wchar = wchar_t;

using std::set;
using std::queue;
using std::pair;
#define dict std::unordered_map

using fs::path;

using std::string;
using std::wstring;

using std::vector;

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

#define INFO_HANDLE(message, ...)                                   \
do                                                                  \
{                                                                   \
     fwprintf(stdout, L"[INFO] " message L"\n", __VA_ARGS__);       \
}                                                                   \
while(0);

#define RED FOREGROUND_RED
#define GREEN FOREGROUND_GREEN
#define BLUE FOREGROUND_BLUE

#define wprintfc(color, format, ...)                               \
{                                                                  \
    SetConsoleTextAttribute(hConsole, color);                      \
    wprintf(format, __VA_ARGS__);                                  \
    SetConsoleTextAttribute(hConsole, RED | GREEN | BLUE);         \
}

#define DESCRIPTION  "usage: ./Булев\\ поиск.exe <commands>\n"                       \
                     "-i \'абс. путь к корпусу\'\n"                                  \
                     "-o \'абс. путь к индексу\'\n"                                  \
                     "-t \'абс. путь к директории с блочным индексом\n"              \
                     "-p кол-во_процессов_для_распараллеливания\n"                   \
                     "-create : создать блочный индекс\n"                            \
                     "-merge  : выполнить слияние блочного индекса\n"                \
                     "-clear  : очистить папку с временными файлами после слияния\n" \
                     "-search : выполнить поиск\n"

#define RADIUS_DETAILS 20

wstr_to_wstr parse = lower_case; // простейшая функция парсинга токенов в термы

int wmain(int argc, wchar *argv[])
{
    setlocale(LC_CTYPE, "rus"); // кодировка на поток вывода
    SetConsoleCP(1251);         // кодировка на поток ввода
    SetConsoleOutputCP(1251);   // кодировка на поток ввода

    bool create = false, clear = false, merge = false, search = false;
    int i,
        num_threads=-1;
   
    path path2corpus=L"", path2index=L"./index", path2blocks=L"./tmp";
 
    /* Разбор параметров входной строки */
    for (i = 1; i < argc; i++)
    {
        if (_wcsicmp(argv[i], L"-create") == 0)
        {
            create = true;
        }
        else if (_wcsicmp(argv[i], L"-merge") == 0)
        {
            merge = true;
        }
        else if (_wcsicmp(argv[i], L"-clear") == 0)
        {
            clear = true;
        }
        else if (_wcsicmp(argv[i], L"-search") == 0)
        {
            search = true;
        }
        else if (_wcsicmp(argv[i], L"-i") == 0)
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
        else if (_wcsicmp(argv[i], L"-t") == 0)
        {
            ERROR_HANDLE(i + 1 < argc, DESCRIPTION);
            path2blocks = fs::absolute(argv[i + 1]);
            i++;
        }
        else if (_wcsicmp(argv[i], L"-p") == 0)
        {
            ERROR_HANDLE(i + 1 < argc, DESCRIPTION);
            num_threads = _wtoi(argv[i + 1]);
            i++;
        }
    }

    if (create)
    {
        ERROR_HANDLE(!path2corpus.empty(), L"Укажите путь к корпусу");

        INFO_HANDLE(L"Создание блочного индекса");

        if(num_threads > 0) 
            create_blocks(path2corpus, path2blocks, parse, num_threads);
        else 
            create_blocks(path2corpus, path2blocks, parse);
    }

    if (merge)
    {
        INFO_HANDLE(L"Слияние блочного индекса");

        merge_blocks(path2blocks, path2index);
    }

    if (clear)
    {
        INFO_HANDLE(L"Очистка блочного индекса");

        fs::remove_all(path2blocks);
    }

    if (search)
    {
        dict<wstring, int> terms = {};

        wchar path2docs_id[256], path2terms[256], path2postings_list[256];

        wcscpy(path2docs_id, (path2index / "docs_id.data").c_str());
        wcscpy(path2terms, (path2index / "terms.data").c_str());
        wcscpy(path2postings_list, (path2index / "postings_list.data").c_str());

        FILE *fp_terms=NULL, *fp_docs=NULL, *fp_postings=NULL;
        int offset;
        uint n_terms, n_chars, n_docs;
        wstring term;

        ERROR_HANDLE(fp_terms = _wfopen(path2terms, L"rb"), L"");
        fread(&n_terms, sizeof(uint), 1, fp_terms);

        INFO_HANDLE(L"Чтение термов");

        for (i = 0; i < n_terms; i++)
        {
            fread(&n_chars, sizeof(uint), 1, fp_terms);
            term.resize(n_chars);

            fread(term.data(), sizeof(wchar), n_chars, fp_terms);
            fread(&offset, sizeof(int), 1, fp_terms);

            terms[term] = offset;
        }

        INFO_HANDLE(L"Загружено %d термов", terms.size());

        fclose(fp_terms);

        wchar buf[512];
        wstring line = L"";
        vector<wstring> request = {};
        vector<int> posting_list = {};
        vector<wstring> postfix_request;
        vector<int> L = {}, line_ind = {}, doc_ind = {};
        wstring doc=L"",
                page_url=L"",
                title=L"";
        int len, j, k;
        bool correct=true;
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        dict<wstring, pair<vector<int> *, vector<int> *>> coord = {};
        wstring str;
        vector <int> lc = {}, rc = {};

        fp_postings = _wfopen(path2postings_list, L"rb");
        fp_docs = _wfopen(path2docs_id, L"rb");
        fread(&n_docs, sizeof(uint), 1, fp_docs);

        wprintf(L"Запрос: ");
        while (fgetws(buf, 512, stdin))
        {
            len = wcslen(buf);
            if (buf[len - 1] == L'\n')
            {
                buf[len - 1] = L'\0';
            }

            line = wstring(buf);

            if(line == L"quit") break;
            
            string_to_infix_request(line, request, parse);

            correct = infix_request_to_postfix_request(request, postfix_request);

            if (!correct)
            {
                wprintfc(RED, L"Некорректный запрос\n");
                continue;
            }
            
            get_response(postfix_request, fp_postings, terms, n_docs,
                         posting_list);

            wprintfc(BLUE | RED, L"По запросу: \" ");
            for (auto iter : request)
            {
                wprintfc(BLUE | RED, L"%s ", iter.c_str());
            }
            wprintfc(BLUE | RED, L"\" найдено %d документов\n", posting_list.size());

            /* Извлечём термы */
            set<wstring> request_terms = {};
            
            for (i = 0; i < postfix_request.size(); i++)
            {
                if(is_term(postfix_request[i])) request_terms.insert(postfix_request[i]);
            }

            /* Извлечение частот-весов */

            vector<pair<uint, int>> weights(posting_list.size());
            for (i = 0; i < weights.size(); i++)
            {
                weights[i] = { 0, posting_list[i] }; // вес + индекс документа
            }

            vector<int> work(100); // рабочее пространство
            int freq;

            for (auto term : request_terms)
            {
                for (k = 0; k < posting_list.size(); k++)
                {
                    freq = get_term_freq_in_doc(term, posting_list[k], terms, fp_postings, work);
                    weights[k].first += freq; // увеличиваем вес
                }
            }

            auto comp = [](const pair<uint, int> &l, const pair<uint, int> &r) -> bool
            {
                return l.first > r.first;
            };

            std::sort(weights.begin(), weights.end(), comp);

            for (k = 0; k < min(50, posting_list.size()); k++)
            {
                get_doc_by_id(weights[k].second, n_docs, fp_docs, doc);

                if (fetch_page_url(doc, page_url))
                {
                    wprintfc(BLUE, L"page_url: %s\n", page_url.c_str());
                }
                else
                {
                    wprintfc(BLUE, L"Не удалось извлечь ссылку\n");
                }

                if (fetch_title(doc, title) || fetch_alternative_title(doc, title))
                {
                    wprintf(L"title: \"%s\"\n", title.c_str());
                }
                else
                {
                    wprintf(L"Не удалось извлечь заголовок\n");
                }

                get_terms_coord(doc, parse, coord); // получаем координаты термов

                int l0, r0, l1, r1, l2, r2,
                    l_prev=0, r_prev=0;
                auto iter = coord.begin();
               
                rc.resize(0);
                lc.resize(0);

                lc.push_back(-RADIUS_DETAILS);
                rc.push_back(-RADIUS_DETAILS);

                for (auto term : request_terms)
                {
                    if ((iter = coord.find(term)) != coord.end())
                    {
                        lc.push_back(iter->second.first->front());
                        rc.push_back(iter->second.second->front());
                    }
                }

                lc.push_back(doc.size());
                rc.push_back(doc.size());

                std::sort(lc.begin(), lc.end());
                std::sort(rc.begin(), rc.end());

                wprintf(L"Подробно: \n");
                
                for (i = 1; i < lc.size() - 1; i++)
                {
                    l1 = lc[i];
                    r1 = rc[i];

                    l0 = max(l1 - RADIUS_DETAILS, min(rc[i - 1] + RADIUS_DETAILS, l1));
                    r0 = l1;

                    l2 = r1;
                    r2 = min(r1 + RADIUS_DETAILS, lc[i + 1]);

                    if (l0 > rc[i - 1] + RADIUS_DETAILS)
                    {
                        wprintf(L"..");
                    }
                    
                    /* выводим всё в радиусе R слева */
                    str.resize(r0 - l0); 
                    std::copy(doc.begin() + l0, doc.begin() + r0, str.begin());
                    wprintf(L"%s", str.c_str());

                    /* выводим токен */
                    str.resize(r1 - l1);
                    std::copy(doc.begin() + l1, doc.begin() + r1, str.begin());
                    wprintfc(GREEN, L"%s", str.c_str());

                    /* выводим всё в радиусе R справа */
                    str.resize(r2 - l2);
                    std::copy(doc.begin() + l2, doc.begin() + r2, str.begin());
                    wprintf(L"%s", str.c_str());

                    if (lc[i + 1] - RADIUS_DETAILS > r2)
                    {
                        wprintf(L"..");
                    }
                }

                wprintf(L"\n");

                /* Очистка словаря */
                for (auto iter : coord)
                {
                    delete iter.second.first;
                    delete iter.second.second;
                }

                coord.clear();
            }

            wprintf(L"Запрос: ");
        }

        fclose(fp_postings);
        fclose(fp_docs);
    }

    system("pause");

    return 0;
}