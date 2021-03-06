#include "../include/defs.hpp"
#include "../include/create_index.hpp"
#include "../include/search.hpp"
#include "../include/token_parse.hpp"
#include "../include/docs_parse.hpp"
#include "../include/algebra.hpp"

#define DESCRIPTION  L"usage: ./prog.exe <commands>\n"                                \
                     L"-i \'путь к корпусу\'\n"                                       \
                     L"-o \'путь к индексу\'\n"                                       \
                     L"-t \'путь к директории с блочным индексом\'\n"                 \
                     L"-m \'путь к директории с эталонами для метрик\'\n"             \
                     L"-p кол-во_процессов_для_распараллеливания\n"                   \
                     L"-create : создать блочный индекс\n"                            \
                     L"-merge  : выполнить слияние блочного индекса\n"                \
                     L"-clear  : очистить папку с временными файлами после слияния\n" \
                     L"-search : выполнить поиск\n"                                   \
                     L"-metric : высчитать метрики\n"

#define RADIUS_DETAILS 50

path path2lemmatizator = L"python\\dist\\lemmatizator\\lemmatizator.exe";       // путь к скомпилированному скрипту
path path2request_parser = L"python\\dist\\request_parser\\request_parser.exe"; // путь к скомпилированному скрипту

int wmain(int argc, wchar *argv[])
{
    setlocale(LC_CTYPE, "rus"); // кодировка на поток вывода
    SetConsoleCP(1251);         // кодировка на поток ввода
    SetConsoleOutputCP(1251);   // кодировка на поток ввода
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    bool create = false, clear = false, merge = false, search = false, metric = false;
    int i,
        num_threads = -1;

    path path2corpus = L"", path2index = L"./index", path2blocks = L"./tmp", path2metric = L"";

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
        else if (_wcsicmp(argv[i], L"-metric") == 0)
        {
            metric = true;
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
        else if (_wcsicmp(argv[i], L"-m") == 0)
        {
            ERROR_HANDLE(i + 1 < argc, return 1, DESCRIPTION);
            path2metric = fs::absolute(argv[i + 1]);
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

        wchar path2docs_id[256], path2terms[256], path2postings_list[256];

        wcscpy(path2docs_id, (path2index / "docs_id.data").c_str());
        wcscpy(path2terms, (path2index / "terms.data").c_str());
        wcscpy(path2postings_list, (path2index / "postings_list.data").c_str());

        FILE *fp_terms = NULL, *fp_docs = NULL, *fp_postings = NULL;
        uint n_terms, n_docs;
        wstring term;

        ERROR_HANDLE(fp_terms = _wfopen(path2terms, L"rb"), return 1, L"");
        fread(&n_terms, sizeof(uint), 1, fp_terms);

        load_index(path2index, terms);

        fclose(fp_terms);

        wchar buf[512];
        wstring line = L"";
        vector<wstring> request = {};
        vector<int> posting_list = {};
        vector<wstring> postfix_request;
        vector<int> L = {}, line_ind = {}, doc_ind = {};
        wstring doc = L"",
            page_url = L"",
            title = L"";
        int len, j, k;
        bool correct = true;

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
                if (!is_term(postfix_request[i]) || terms.find(postfix_request[i]) == terms.end())  // не терм, либо его нет в корпусе
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

            vector<int> terms_id(request_wt.size());
            vector<double> req_wt(request_wt.size());

            i = 0;
            foreach(ptr, request_wt) // нормализуем и заполняем вектора с идентификаторами
            {
                ptr->second /= norm;

                terms_id[i] = terms[ptr->first].id;
                req_wt[i] = ptr->second;
                i++;
            }

            /* Вычисление компонент вектора документа */

            vector<vector<double>> doc_wt(posting_list.size(),
                                          vector<double>(request_wt.size()));
            vector<pair<uint, double>> weights(posting_list.size()); // долгожданные веса

            #pragma omp parallel num_threads(num_threads)                                   \
                                             shared(doc_wt, posting_list, weights)          \
                                             firstprivate(path2index, terms_id, req_wt)     \
                                             private(i, j)
            {
                int id = omp_get_thread_num(),
                    id_offset = omp_get_num_threads();

                vector<int> iwork(100);
                vector<double> work(100);
                double weight;
                int n = doc_wt.size();

                for (i = id; i < n; i += id_offset)
                {
                    get_terms_weights_in_doc(path2index, posting_list[i], terms_id, doc_wt[i], iwork, work);

                    weight = 0;
                    for (j = 0; j < req_wt.size(); j++) // цикл по терминам из запроса
                    {
                        weight += req_wt[j] * doc_wt[i][j];
                    }
                    weights[i] = { posting_list[i], weight }; // вес + индекс документа
                }
            }

            auto comp = [](const pair<uint, double> &l, const pair<uint, double> &r) -> bool
            {
                return l.second > r.second;
            };

            std::sort(weights.begin(), weights.end(), comp); // сортировочка

            for (k = 0; k < min(50, posting_list.size()); k++) // выводим первые 50 запросов
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
                    l_prev = 0, r_prev = 0;

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

    if (metric) // считаем метрики
    {
        const int K = 5; // глубина

        FILE *fp = _wfopen((path2metric / L"requests.txt").c_str(), L"r");
        wchar buf[256];
        wstring str;
        vector<wstring> request = {};
        vector<vector<wstring>> urls = {};
        bool start = false;

        while (fgetws(buf, 256, fp))
        {
            /* Сбор эталонных показателей из файла*/
            if (buf[0] == L'=')
            {
                if (start && urls.size() > 0 && urls.back().size() > 0) // конец ссылок
                {
                    start = false;
                }
                continue;
            }

            if (buf[0] == L'\0') continue;

            if (!start)
            {
                trim(buf, str);
                request.push_back(str);
                urls.push_back(vector<wstring>(0));
                start = true;
            }
            else
            {
                trim(buf, str);
                urls.back().push_back(str);
            }
        }

        fclose(fp);

        /* Подготовка поисковика */
        dict<wstring, item> terms = {};
        uint n_docs;

        load_index(path2index, terms, &n_docs);
        fp = _wfopen((path2index / L"postings_list.data").c_str(), L"rb");
        FILE *fp_docs = NULL;
        fp_docs = _wfopen((path2index / L"docs_id.data").c_str(), L"rb");

        vector<wstring> irequest = {}, prequest = {};
        wstring url;
        vector<int> posting_list = {};
        bool correct;
        int j, k;

        vector<vector<wstring>> find_urls(request.size(), vector<wstring>(0));
        for (i = 0; i < request.size(); i++) // цикл по запросам
        {
            string_to_infix_request(request[i], irequest, path2request_parser);

            correct = infix_request_to_postfix_request(irequest, prequest);

            if (!correct)
            {
                wprintfc(RED, L"Некорректный запрос\n");
                continue;
            }

            get_response(prequest, fp, terms, n_docs,
                         posting_list);

            wprintfc(GREEN, L"Запрос: \' %s \'\n", request[i].c_str());

            wprintfc(BLUE | RED, L"По запросу: \' ");
            for (auto iter : irequest)
            {
                wprintfc(BLUE | RED, L"%s ", iter.c_str());
            }
            wprintfc(BLUE | RED, L"\' найдено %lu документов\n", posting_list.size());

            /* Вычисление компонент вектора запроса */

            dict<wstring, double> request_wt = {};

            for (j = 0; j < prequest.size(); j++)
            {
                if (!is_term(prequest[j]) || terms.find(prequest[j]) == terms.end())  // не терм, либо его нет в корпусе
                    continue;

                if (request_wt.find(prequest[j]) == request_wt.end())
                {
                    request_wt[prequest[j]] = 1;
                }
                else
                {
                    request_wt[prequest[j]] += 1;
                }
            }

            double norm = 0;
            foreach(ptr, request_wt) // находим term_wt
            {
                ptr->second = (1 + log10(ptr->second)) * log10(n_docs / (double)terms[ptr->first].df);
                norm += ptr->second * ptr->second;
            }
            norm = sqrt(norm);

            vector<int> terms_id(request_wt.size());
            vector<double> req_wt(request_wt.size());

            j = 0;
            foreach(ptr, request_wt) // нормализуем и заполняем вектора с идентификаторами
            {
                ptr->second /= norm;

                terms_id[j] = terms[ptr->first].id;
                req_wt[j] = ptr->second;
                j++;
            }

            /* Вычисление компонент вектора документа */

            vector<vector<double>> doc_wt(posting_list.size(),
                                          vector<double>(request_wt.size()));
            vector<pair<int, double>> weights(posting_list.size()); // долгожданные веса

            #pragma omp parallel num_threads(num_threads)                                   \
                                             shared(doc_wt, posting_list, weights)          \
                                             firstprivate(path2index, terms_id, req_wt)     \
                                             private(i, j)
            {
                int id = omp_get_thread_num(),
                    id_offset = omp_get_num_threads();

                vector<int> iwork(100);
                vector<double> work(100);
                double weight;
                int n = doc_wt.size();

                for (i = id; i < n; i += id_offset)
                {
                    get_terms_weights_in_doc(path2index, posting_list[i], terms_id, doc_wt[i], iwork, work);

                    weight = 0;
                    for (j = 0; j < req_wt.size(); j++) // цикл по терминам из запроса
                    {
                        weight += req_wt[j] * doc_wt[i][j];
                    }
                    weights[i] = { posting_list[i], weight }; // вес + индекс документа
                }
            }

            auto comp = [](const pair<int, double> &l, const pair<int, double> &r) -> bool
            {
                return l.second > r.second;
            };

            std::sort(weights.begin(), weights.end(), comp); // сортировочка

            /* Вытаскиваем найденные ссылки */

            wstring doc = L"";
            find_urls[i] = vector<wstring>(min(K, weights.size()));
            for (j = 0; j < find_urls[i].size(); j++) // топ-K
            {
                get_doc_by_id(weights[j].first, n_docs, fp_docs, doc);
                fetch_page_url(doc, find_urls[i][j]); // извлекаем ссылки (все)
            }
        }

        fclose(fp);
        fclose(fp_docs);

        /* Просчёт метрик */

        vector<vector<double>> r(request.size()); // вектор оценок
        vector<int> nK(request.size());
        for (i = 0; i < request.size(); i++)
        {
            r[i] = vector<double>(find_urls[i].size(), 0);
            nK[i] = find_urls[i].size();
            for (k = 0; k < find_urls[i].size(); k++)
            {
                if (std::find(urls[i].begin(), urls[i].end(), find_urls[i][k]) != urls[i].end())
                {
                    r[i][k] = 1;
                }
            }
        }

        vector<double> accuracy(request.size(), 0); // точность на уровне K

        for (i = 0; i < request.size(); i++)
        {
            for (k = 0; k < nK[i]; k++)
            {
                accuracy[i] += r[i][k];
            }
            accuracy[i] = (nK[i] > 0) ? accuracy[i] / nK[i] : 0;
        }

        wprintf(L"Точность на уровне %d = %lf\n", K, mean(accuracy.data(), accuracy.size()));

        vector<double> DCG(request.size(), 0); // DCG на уровне K

        for (i = 0; i < request.size(); i++)
        {
            for (k = 0; k < nK[i]; k++)
            {
                DCG[i] += r[i][k] / log2(k + 2);
            }
        }

        wprintf(L"DCG на уровне %d = %lf\n", K, mean(DCG.data(), DCG.size()));

        vector<double> IDCG(request.size(), 0),
                       nDCG(request.size(), 0);

        for (i = 0; i < request.size(); i++)
        {
            for (k = 0; k < nK[i]; k++)
            {
                IDCG[i] += 1 / log2(k + 2);
            }
            nDCG[i] = DCG[i] / IDCG[i];
        }

        wprintf(L"nDCG на уровне %d = %lf\n", K, mean(nDCG.data(), nDCG.size()));

        vector<double> ERR(request.size(), 0);
        vector<vector<double>> P(request.size());
        double p;

        for (i = 0; i < request.size(); i++)
        {
            P[i] = vector<double>(nK[i], 0);
            for (k = 0; k < nK[i]; k++)
            {
                p = 1;
                for (j = 0; j < k; j++)
                {
                    p *= (1 - r[i][j]);
                }
                P[i][k] = r[i][k] * p;
            }
        }

        for (i = 0; i < request.size(); i++)
        {
            for (k = 0; k < nK[i]; k++)
            {
                ERR[i] += P[i][k] / (1 + k);
            }
        }

        wprintf(L"ERR на уровне %d = %lf\n", K, mean(ERR.data(), ERR.size()));
    }

    return 0;
}