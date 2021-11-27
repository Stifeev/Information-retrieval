#include "../include/create_index.hpp"
#include "../include/bool_search.hpp"
#include "../include/docs_parse.hpp"

#include "../include/gui_defs.hpp"
#include "../include/gui_params_window.hpp"
#include "../resource.h"

HWND hEditControl = NULL;    // дескриптор текстовой панели с вводом
HWND hList = NULL;
HWND label = NULL;

LRESULT CALLBACK MainProcedure(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

void MainWndAddMenu(HWND hWnd);
void MainWndAddWidgets(HWND hWnd);
void AddItemToList(int num, wstring page_url, wstring title, wstring desc);
RECT get_rect(HWND parent, HWND child);
void UpdateLabel();

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR args, int nCmdShow)
{
    AllocConsole();
    _wfreopen(L"CONOUT$", L"w", stdout);
    _wfreopen(L"CONOUT$", L"w", stderr);
    _wfreopen(L"CONIN$", L"r", stdin);
    
    hConsole = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE hConsoleIn = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    SetStdHandle(STD_OUTPUT_HANDLE, hConsole);
    SetStdHandle(STD_ERROR_HANDLE, hConsole);
    SetStdHandle(STD_INPUT_HANDLE, hConsoleIn);
 
    setlocale(LC_CTYPE, "rus"); // кодировка на поток вывода
    SetConsoleCP(1251);         // кодировка на поток ввода
    SetConsoleOutputCP(1251);   // кодировка на поток вывода

    wprintfc(GREEN, L"Консоль подключена!\n");

    WNDCLASSW NWC = {0}; // новый класс окна

    This = hInst;
    NWC.hInstance = This;
    NWC.hCursor = LoadCursor(NULL, IDC_ARROW);
    NWC.hIcon = LoadIcon(This, MAKEINTRESOURCE(IDI_ICON1));
    NWC.lpszClassName = L"MainWndClass"; // имя класса окна
    NWC.hbrBackground = (HBRUSH)COLOR_WINDOW;
    NWC.lpfnWndProc = MainProcedure;

    if (!RegisterClassW(&NWC)) // регистрация класса окна
    {
        return -1;
    }

    hMainWnd = CreateWindowW(L"MainWndClass", L"Поиск по корпусу документов", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             Wndx, Wndy,
                             1200, 930,
                             HWND_DESKTOP, // дескриптор родительского окна
                             NULL, This, NULL);
    
    MSG MainMessage = {0};

    while (GetMessageW(&MainMessage, NULL, NULL, NULL))
    {
        TranslateMessage(&MainMessage);
        DispatchMessageW(&MainMessage);
    }
}

void MainWndAddMenu(HWND hWnd)
{
    HMENU RootMenu = CreateMenu();

    HMENU ParamsMenu = CreateMenu();

    AppendMenuW(ParamsMenu, MF_STRING, MenuParamsClicked, L"Параметры");
    AppendMenuW(ParamsMenu, MF_SEPARATOR, NULL, NULL);
    AppendMenuW(ParamsMenu, MF_STRING, MenuExitClicked, L"Выход");

    AppendMenuW(RootMenu, MF_POPUP, (UINT_PTR)ParamsMenu, L"Параметры");

    SetMenu(hWnd, RootMenu);
}

void MainWndAddWidgets(HWND hWnd)
{
    hEditControl = CreateWindowW(L"edit", last_request.c_str(), WS_VISIBLE | WS_BORDER | WS_CHILD | ES_MULTILINE | WS_HSCROLL,
                                 5, 30,
                                 540, 45,
                                 hWnd, NULL, NULL, NULL);

    RECT rect = get_rect(hWnd, hEditControl);
    HWND wind = CreateWindowW(L"button", L"Поиск", WS_VISIBLE | WS_CHILD | ES_CENTER,
                              rect.right + 10, rect.top,
                              120, 30,
                              hWnd, (HMENU)ButtonSearchClicked,
                              NULL, NULL);

    rect = get_rect(hWnd, wind);
    wind = CreateWindowW(L"button", L"Загрузить индекс", WS_VISIBLE | WS_CHILD | ES_CENTER,
                         rect.right + 10, rect.top,
                         150, 30,
                         hWnd, (HMENU)ButtonLoadIndexClicked,
                         NULL, NULL);

    rect = get_rect(hWnd, wind);
    wind = CreateWindowW(L"button", L"Построить индекс", WS_VISIBLE | WS_CHILD | ES_CENTER,
                         rect.right + 10, rect.top,
                         150, 30,
                         hWnd, (HMENU)ButtonCreateIndexClicked,
                         NULL, NULL);

    rect = get_rect(hWnd, wind);
    wind = CreateWindowW(L"button", L"Выполнить слияние", WS_VISIBLE | WS_CHILD | ES_CENTER,
                         rect.right + 10, rect.top,
                         150, 30,
                         hWnd, (HMENU)ButtonMergeIndexClicked,
                         NULL, NULL);

    rect = get_rect(hWnd, hEditControl);
    hList = CreateWindowW(WC_LISTVIEWW, L"Список",
                          WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT | LVS_EX_GRIDLINES | LVS_EDITLABELS | WS_VSCROLL,
                          rect.left, rect.bottom + 10,
                          1150, 700,
                          hWnd, (HMENU)IDC_list,
                          This, NULL);
    
    ListView_SetTextColor(hList, RGB(0, 0, 255));

    RECT rc;
    GetWindowRect(hList, &rc);
    LV_COLUMNW lvc = {0};
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = 100;
    lvc.pszText = (LPWSTR)L"Ссылка";
    lvc.iSubItem = 0;
    SendMessageW(hList, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvc);

    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = 300;
    lvc.pszText = (LPWSTR)L"Заголовок";
    lvc.iSubItem = 1;
    SendMessageW(hList, LVM_INSERTCOLUMNW, 1, (LPARAM)&lvc);

    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = rc.right - rc.left - 100 - 300;
    lvc.pszText = (LPWSTR)L"Подробности";
    lvc.iSubItem = 2;
    SendMessageW(hList, LVM_INSERTCOLUMNW, 2, (LPARAM)&lvc);

    AddItemToList(0, L"Здесь", L"будут", L"результаты");

    rect = get_rect(hWnd, hList);

    HWND hButtonLeft = CreateWindowW(L"button", L"", WS_VISIBLE | WS_CHILD | ES_CENTER | BS_BITMAP,
                                     rect.left + 5, rect.bottom + 10,
                                     40, 40,
                                     hWnd, (HMENU)IDC_ButtonLeft,
                                     NULL, NULL);
    SendMessage(hButtonLeft, BM_SETIMAGE, IMAGE_BITMAP,
                (LPARAM)LoadImage(This, MAKEINTRESOURCE(IDB_BITMAP2), IMAGE_BITMAP, 40, 40, 0));

    rect = get_rect(hWnd, hButtonLeft);

    HWND hButtonRight = CreateWindowW(L"button", L"", WS_VISIBLE | WS_CHILD | ES_CENTER | BS_BITMAP,
                                      rect.right + 10, rect.top,
                                      40, 40,
                                      hWnd, (HMENU)IDC_ButtonRight,
                                      NULL, NULL);
    SendMessage(hButtonRight, BM_SETIMAGE, IMAGE_BITMAP,
                (LPARAM)LoadImage(This, MAKEINTRESOURCE(IDB_BITMAP3), IMAGE_BITMAP, 40, 40, 0));

    rect = get_rect(hWnd, hButtonLeft);

    label = CreateWindowW(L"static", 
                          L"",
                          WS_VISIBLE | WS_CHILD | ES_CENTER | BS_BITMAP,
                          rect.left + 20, rect.bottom + 10,
                          50, 20,
                          hWnd, (HMENU)IDC_Label,
                          NULL, NULL);

    void UpdateLabel();
}

void UpdateLabel()
{
    SetWindowTextW(label, (std::to_wstring(num_page + 1) + L"/" + std::to_wstring(n_pages)).c_str());
}

void AddItemToList(int num, wstring page_url, wstring title, wstring desc)
{
    LV_ITEMW li = { 0 }; li.mask = LVIF_TEXT;

    li.iItem = num;

    li.iSubItem = 0;
    li.pszText = (LPWSTR)page_url.c_str();
    li.cchTextMax = page_url.size();
    SendMessageW(hList, LVM_INSERTITEMW, 0, (LPARAM)&li);

    li.iSubItem = 1; 
    li.pszText = (LPWSTR)title.c_str();
    li.cchTextMax = title.size();
    SendMessageW(hList, LVM_SETITEMW, 0, (LPARAM)&li);

    li.iSubItem = 2; 
    li.pszText = (LPWSTR)desc.c_str();;
    li.cchTextMax = desc.size();
    SendMessageW(hList, LVM_SETITEMW, 0, (LPARAM)&li);
}

bool ApplySearch(const wstring &line_request, dict<wstring, item> &terms)
{
    vector<wstring> infix_request = {}, postfix_request = {};
    string_to_infix_request(line_request, infix_request, path2request_parser);

    bool correct = infix_request_to_postfix_request(infix_request, postfix_request);

    if (!correct)
    {
        wprintfc(RED, L"Некорректный запрос\n");
        return false;
    }

    path path2postings = path(path2index) / "postings_list.data";
    FILE *fp_postings = NULL;
    
    ERROR_HANDLE(fp_postings = _wfopen(path2postings.c_str(), L"rb"), return false, "Неверный путь к индексу");
    vector<int> posting_list = {};

    get_response(postfix_request, fp_postings, terms, n_docs,
                 posting_list);
    fclose(fp_postings);

    wprintfc(BLUE | RED, L"По запросу: \" ");
    for (auto iter : infix_request)
    {
        wprintfc(BLUE | RED, L"%s ", iter.c_str());
    }
    wprintfc(BLUE | RED, L"\" найдено %lu документов\n", posting_list.size());

    INFO_HANDLE("Обработка результатов поиска");

    /* Вычисление компонент вектора запроса */

    request_wt.clear();

    int i;
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

    double norm;

    foreach(ptr, request_wt) // находим term_wt
    {
        ptr->second = (1 + log10(ptr->second)) * log10(n_docs / (double)terms[ptr->first].df);
    }

    norm = 0;
    for (auto ptr : request_wt) // считаем норму
    {
        norm += ptr.second * ptr.second;
    }
    norm = sqrt(norm);
    foreach(ptr, request_wt) // нормализуем
    {
        ptr->second /= norm;
    }

    /* Вычисление компонент вектора документа */

    vector<dict<wstring, double>> doc_wt(posting_list.size());
    weights.resize(posting_list.size()); // долгожданные веса
    
    #pragma omp parallel num_threads(omp_num_threads)             \
                         shared(doc_wt, posting_list, id2term)    \
                         firstprivate(path2index, request_wt)     \
                         private(i)
    {
        int id = omp_get_thread_num(),
            id_offset = omp_get_num_threads();

        vector<int> iwork(100);
        vector<double> work(100);
        double weight;

        for (i = id; i < doc_wt.size(); i += id_offset)
        {
            get_terms_weight_in_doc(path2index, posting_list[i], id2term, doc_wt[i],
                                    iwork, work);

            weight = 0;
            foreach(ptr, request_wt) // цикл по терминам из запроса
            {
                if (doc_wt[i].find(ptr->first) != doc_wt[i].end())
                {
                    weight += ptr->second * doc_wt[i][ptr->first];
                }
            }
            weights[i] = { posting_list[i], weight }; // вес + индекс документа
        }
    }

    auto comp = [](const pair<uint, double> &l, const pair<uint, double> &r) -> bool
    {
        return l.second > r.second;
    };

    std::sort(weights.begin(), weights.end(), comp); // сортировочка

    n_pages = ceil(weights.size() / 50.);
    num_page = 0;
 
    return true;
}

bool ShowResult(int n_page)
{
    int i, j, k;

    wstring path2docs_id = path(path2index) / "docs_id.data";
    wstring path2postings = path(path2index) / "postings_list.data";
    
    ListView_DeleteAllItems(hList);

    int m = min(50, weights.size() - n_page * 50);
    struct item
    {
        wstring page_url;
        wstring title;
        wstring desc;
        vector<int> begin;
        vector<int> end;
    };
    vector<item> table(m, { L"", L"", L"", vector<int>(0), vector<int>(0) }); // таблица ответов
    
    #pragma omp parallel num_threads(omp_num_threads)                     \
                         shared(weights, table, terms)                    \
                         firstprivate(m, n_page, n_docs,                  \
                                      path2docs_id, path2postings,        \
                                      request_wt)
    {
        int id = omp_get_thread_num(),
            id_offset = omp_get_num_threads();
        int k, i;

        FILE *fp_docs = NULL;
        FILE *fp_postings = NULL;
        wstring doc = L"";
        vector<int> starts(100), ends(100), work(100);

        wstring str, term;
        vector <int> lc = {}, rc = {};

        fp_docs = _wfopen(path2docs_id.c_str(), L"rb");
        fp_postings = _wfopen(path2postings.c_str(), L"rb");

        for (k = id; k < m; k += id_offset)
        {
            i = n_page * 50 + k;
            get_doc_by_id(weights[i].first, n_docs, fp_docs, doc);

            if (fetch_page_url(doc, table[k].page_url))
            {

            }
            else
            {
                table[k].page_url = L"Не удалось извлечь ссылку\n";
            }

            if (fetch_title(doc, table[k].title) || 
                fetch_alternative_title(doc, table[k].title))
            {
            }
            else
            {
                table[k].title = L"Не удалось извлечь заголовок\n";
            }

            rc.resize(0);
            lc.resize(0);

            lc.push_back(-RADIUS_DETAILS);
            rc.push_back(-RADIUS_DETAILS);

            for (auto ptr : request_wt) // получаем координаты термов для каждого терма из запроса
            {
                get_term_coords(fp_postings, ptr.first, weights[i].first,
                                terms, starts, ends, work);

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
                    table[k].desc += L"..";
                }

                /* выводим всё в радиусе R слева */
                str.resize(r0 - l0);
                std::copy(doc.begin() + l0, doc.begin() + r0, str.begin());
                table[k].desc += str;

                /* выводим токен */
                str.resize(r1 - l1);
                std::copy(doc.begin() + l1, doc.begin() + r1, str.begin());
                table[k].begin.push_back(table[k].desc.size());
                table[k].end.push_back(table[k].desc.size() + str.size());
                table[k].desc += str;

                /* выводим всё в радиусе R справа */
                str.resize(r2 - l2);
                std::copy(doc.begin() + l2, doc.begin() + r2, str.begin());
                table[k].desc += str;

                if (lc[i + 1] - RADIUS_DETAILS > r2)
                {
                    table[k].desc += L"..";
                }
            }
        }

        fclose(fp_postings);
        fclose(fp_docs);
    }

    /* Показываем результат в консоль и список */
    for (k = 0; k < m; k++)
    {
        wprintfc(BLUE, L"page_url: %s\n", table[k].page_url.c_str());

        wprintf(L"title: \"%s\"\n", table[k].title.c_str());

        wprintf(L"Детали: ");

        table[k].begin.push_back(table[k].desc.size());
        table[k].end.push_back(table[k].desc.size());

        wprintf(L"%s", table[k].desc.substr(0, table[k].begin[0]).c_str()); // выводим то, что слева

        for (j = 0; j < table[k].begin.size() - 1; j++)
        {
            wprintfc(GREEN, L"%s", table[k].desc.substr(table[k].begin[j], table[k].end[j] - table[k].begin[j]).c_str()); // хайлайтим терм

            for (i = table[k].begin[j]; i < table[k].end[j]; i++) // Перевод в верхний регистр термов (хайлайт)
            {
                table[k].desc[i] = towupper(table[k].desc[i]);
            }

            wprintf(L"%s", table[k].desc.substr(table[k].end[j], table[k].begin[j + 1] - table[k].end[j]).c_str()); // выводим то, что справа
        }
        wprintf(L"\n");
        AddItemToList(k, table[k].page_url, table[k].title, table[k].desc);
    }
    
    return true;
}

RECT get_rect(HWND parent, HWND child)
{
    RECT prect, crect, res;
   
    GetWindowRect(parent, &prect);
    GetWindowRect(child, &crect);

    res.left = crect.left - prect.left - 8;
    res.top = crect.top - prect.top - 51;
    res.right = res.left + crect.right - crect.left;
    res.bottom = res.top + crect.bottom - crect.top;

    return res;
}

LRESULT CALLBACK MainProcedure(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        if (wp == 13) // ENTER
        {
            wchar request[MAX_PATH];
            GetWindowTextW(hEditControl, request, MAX_PATH);
            last_request = request;
            ApplySearch(last_request, terms);
            UpdateLabel();
            ShowResult(0);
            WriteDefaultParams();
        }
        break;

    case WM_COMMAND:
        switch (wp)
        {
        case MenuExitClicked:
            PostQuitMessage(0);
            break;
        case MenuParamsClicked: // нажатие на параметры
            if(IsWindow(hParamsWnd)) break;
            RegisterParamsWindow(This);
            hParamsWnd = CreateParamsWindow(This, hMainWnd);
            break;
        case ButtonSearchClicked: // кнопка поиска нажата
            WARNING_HANDLE(fs::exists(path2corpus), break, L"Укажите путь к корпусу");
            WARNING_HANDLE(fs::exists(path2index), break, L"Укажите путь к индексу");

            wchar request[MAX_PATH];
            GetWindowTextW(hEditControl, request, MAX_PATH);
            last_request = request;
            ApplySearch(last_request, terms);
            UpdateLabel();
            ShowResult(0);
            WriteDefaultParams();

            break;
        case ButtonCreateIndexClicked:

            WARNING_HANDLE(fs::exists(path2corpus), break, L"Укажите путь к корпусу");

            INFO_HANDLE(L"Создание блочного индекса корпуса: %s", path2corpus.c_str());
            create_blocks(path2corpus, path2blocks, path2lemmatizator, omp_num_threads);
            INFO_HANDLE(L"Создание блочного индекса окончено. Выполните слияние");

            break;
        case ButtonMergeIndexClicked:
            INFO_HANDLE(L"Слияние блочного индекса");
            merge_blocks(path2blocks, path2index);
            INFO_HANDLE(L"Слияние блочного индекса завершено");

            INFO_HANDLE(L"Вычисление статистики");
            calculate_doc_tf(path2index);
            INFO_HANDLE(L"Вычисление статистики завершено");

            INFO_HANDLE(L"Очистка папки с блочным индексом");
            for (auto iter : fs::recursive_directory_iterator(path2blocks))
            {
                fs::remove(iter);
            }
            INFO_HANDLE(L"Успешно. Индекс создан в %s", path2index.c_str());

            load_index(path(path2index), terms, id2term, &n_docs); /* не так уж это и долго */

            break;

        case ButtonLoadIndexClicked:

            WARNING_HANDLE(fs::exists(path2corpus), break, L"Укажите правильный путь к корпусу в параметрах");
            WARNING_HANDLE(fs::exists(path2index), break, L"Укажите правильный путь к индексу в параметрах");

            load_index(path(path2index), terms, id2term, &n_docs);

            break;

        case IDC_ButtonLeft:

            if(num_page == 0) break;

            num_page--;
            UpdateLabel();
            ShowResult(num_page);

            break;

        case IDC_ButtonRight:

            if (num_page >= n_pages - 1) break;

            num_page++;
            UpdateLabel();
            ShowResult(num_page);

            break;

        default: 
            break;
        }
        break;

    case WM_CREATE:
        ReadDefaultParams(); // чтение параметров по умолчанию из ini.config
        MainWndAddMenu(hWnd);
        MainWndAddWidgets(hWnd);
        break;
    
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default: 
        return DefWindowProcW(hWnd, msg, wp, lp);
    }

    return 0;
}