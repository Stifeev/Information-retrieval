#include "../include/defs.hpp"
#include "../include/create_index.hpp"
#include "../include/bool_search.hpp"
#include "../include/token_parse.hpp"
#include "../include/docs_parse.hpp"

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

path path2lemmatizator = L"python\\dist\\lemmatizator\\lemmatizator.exe"; // путь к скомпилированному скрипту
path path2request_parser = L"python\\dist\\request_parser\\request_parser.exe"; // путь к скомпилированному скрипту

int wmain(int argc, wchar *argv[])
{
    setlocale(LC_CTYPE, "rus"); // кодировка на поток вывода
    SetConsoleCP(1251);         // кодировка на поток ввода
    SetConsoleOutputCP(1251);   // кодировка на поток ввода
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

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
            ERROR_HANDLE(i + 1 < argc, return 1, DESCRIPTION);
            path2corpus = fs::absolute(argv[i + 1]);
            i++;
        }
        else if (_wcsicmp(argv[i], L"-o") == 0)
        {
            ERROR_HANDLE(i + 1 < argc, return 1, DESCRIPTION);
            path2index = fs::absolute(argv[i + 1]);
            i++;
        }
        else if (_wcsicmp(argv[i], L"-t") == 0)
        {
            ERROR_HANDLE(i + 1 < argc, return 1, DESCRIPTION);
            path2blocks = fs::absolute(argv[i + 1]);
            i++;
        }
        else if (_wcsicmp(argv[i], L"-p") == 0)
        {
            ERROR_HANDLE(i + 1 < argc, return 1, DESCRIPTION);
            num_threads = _wtoi(argv[i + 1]);
            i++;
        }
    }

    if (create)
    {
        ERROR_HANDLE(!path2corpus.empty(), return 1, L"Укажите путь к корпусу");

        INFO_HANDLE(L"Создание блочного индекса");

        time_t start = time(NULL);
        create_blocks(path2corpus, path2blocks, path2lemmatizator, num_threads > 0 ? num_threads : 1);
        time_t end = time(NULL);

        INFO_HANDLE(L"Время на создание блочного индекса: %lf min", (end - start) / 60.);
    }

    if (merge)
    {
        INFO_HANDLE(L"Слияние блочного индекса");

        time_t start = time(NULL);
        merge_blocks(path2blocks, path2index);
        time_t end = time(NULL);

        INFO_HANDLE(L"Время на слияние блочного индекса: %lld sec", end - start);

        INFO_HANDLE(L"Вычисление статистики");
        calculate_doc_tf(path2index);
        INFO_HANDLE(L"Вычисление статистики закончено");
    }

    if (clear)
    {
        INFO_HANDLE(L"Очистка блочного индекса");

        for (auto iter : path2blocks)
        {
            fs::remove(iter);
        }
    }

    if (search)
    {
        dict<wstring, item> terms = {};
        vector<wstring> id2term = {};

        wchar path2docs_id[256], path2terms[256], path2postings_list[256];

        wcscpy(path2docs_id, (path2index / "docs_id.data").c_str());
        wcscpy(path2terms, (path2index / "terms.data").c_str());
        wcscpy(path2postings_list, (path2index / "postings_list.data").c_str());

        FILE *fp_terms=NULL, *fp_docs=NULL, *fp_postings=NULL;
        uint n_terms, n_docs;
        wstring term;

        ERROR_HANDLE(fp_terms = _wfopen(path2terms, L"rb"), return 1, L"");
        fread(&n_terms, sizeof(uint), 1, fp_terms);

        load_index(path2index, terms, id2term);

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
        
        wstring str;
        vector<int> starts(300), ends(300), iwork(300);
        vector<double> work(300);
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

            if (line == L"quit") break;

            string_to_infix_request(line, request, path2request_parser);

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
            wprintfc(BLUE | RED, L"\" найдено %lu документов\n", posting_list.size());

            /* Вычисление компонент вектора запроса */

            dict<wstring, double> request_wt = {};
            
            for (i = 0; i < postfix_request.size(); i++)
            {
                if(!is_term(postfix_request[i]) || terms.find(postfix_request[i]) == terms.end())  // не терм, либо его нет в корпусе
                    continue;

                if (request_wt.find(postfix_request[i]) == request_wt.end())
                {
                    request_wt[postfix_request[i]] = 1;
                }
                else
                {
                    request_wt[postfix_request[i]] += 1;
                }
            }

            double norm = 0;
            foreach(ptr, request_wt) // находим term_wt
            {
                ptr->second = (1 + log10(ptr->second)) * log10(n_docs / (double)terms[ptr->first].df);
                norm += ptr->second * ptr->second;
            }

            norm = sqrt(norm);
            foreach(ptr, request_wt) // нормализуем
            {
                ptr->second /= norm;
            }

            /* Вычисление компонент вектора документа */

            vector<dict<wstring, double>> doc_wt(posting_list.size());

            for (i = 0; i < doc_wt.size(); i++)
            {
                get_terms_weight_in_doc(path2index, posting_list[i], id2term, doc_wt[i], 
                                        iwork, work);
            }

            vector<pair<uint, double>> weights(posting_list.size()); // долгожданные веса
            double weight;
            for (i = 0; i < weights.size(); i++)
            {
                weight = 0;
                for (auto ptr : request_wt) // цикл по терминам из запроса
                {
                    if (doc_wt[i].find(ptr.first) != doc_wt[i].end())
                    {
                        weight += ptr.second * doc_wt[i][ptr.first];
                    }
                }
                
                weights[i] = { posting_list[i], weight }; // вес + индекс документа
            }

            auto comp = [](const pair<uint, double> &l, const pair<uint, double> &r) -> bool
            {
                return l.second > r.second;
            };

            std::sort(weights.begin(), weights.end(), comp);

            for (k = 0; k < min(50, posting_list.size()); k++)
            {
                get_doc_by_id(weights[k].first, n_docs, fp_docs, doc);

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

                rc.resize(0);
                lc.resize(0);

                lc.push_back(-RADIUS_DETAILS);
                rc.push_back(-RADIUS_DETAILS);

                for (auto ptr : request_wt) // получаем координаты термов для каждого терма из запроса
                {
                    get_term_coords(fp_postings, ptr.first, weights[k].first,
                                    terms, starts, ends, iwork); 

                    if (starts.size() > 0)
                    {
                        lc.push_back(starts[0]);
                        rc.push_back(ends[0]);
                    }
                }
                
                int l0, r0, l1, r1, l2, r2,
                    l_prev=0, r_prev=0;
                   
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
            }

            wprintf(L"Запрос: ");
        }

        fclose(fp_postings);
        fclose(fp_docs);
    }

    return 0;
}