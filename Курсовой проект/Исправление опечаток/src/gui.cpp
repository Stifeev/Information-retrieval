#include "../include/create_index.hpp"
#include "../include/search.hpp"
#include "../include/docs_parse.hpp"
#include "../include/typos_correction.hpp"

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
    SetConsoleCP(1251);         // êîäèðîâêà íà ïîòîê ââîäà
    SetConsoleOutputCP(1251);   // êîäèðîâêà íà ïîòîê âûâîäà

    wprintfc(GREEN, L"Êîíñîëü ïîäêëþ÷åíà!\n");

    WNDCLASSW NWC = {0}; // íîâûé êëàññ îêíà

    This = hInst;
    NWC.hInstance = This;
    NWC.hCursor = LoadCursor(NULL, IDC_ARROW);
    NWC.hIcon = LoadIcon(This, MAKEINTRESOURCE(IDI_ICON1));
    NWC.lpszClassName = L"MainWndClass"; // èìÿ êëàññà îêíà
    NWC.hbrBackground = (HBRUSH)COLOR_WINDOW;
    NWC.lpfnWndProc = MainProcedure;

    if (!RegisterClassW(&NWC)) // ðåãèñòðàöèÿ êëàññà îêíà
    {
        return -1;
    }

    hMainWnd = CreateWindowW(L"MainWndClass", L"Ïîèñê ïî êîðïóñó äîêóìåíòîâ", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             Wndx, Wndy,
                             1200, 930,
                             HWND_DESKTOP, // äåñêðèïòîð ðîäèòåëüñêîãî îêíà
                             NULL, This, NULL);
    
    MSG MainMessage = {0};

    while (GetMessageW(&MainMessage, NULL, NULL, NULL))
    {
        TranslateMessage(&MainMessage);
        DispatchMessageW(&MainMessage);
    }
    return (int)MainMessage.wParam;
}

void MainWndAddMenu(HWND hWnd)
{
    HMENU RootMenu = CreateMenu();

    HMENU ParamsMenu = CreateMenu();

    AppendMenuW(ParamsMenu, MF_STRING, MenuParamsClicked, L"Ïàðàìåòðû");
    AppendMenuW(ParamsMenu, MF_SEPARATOR, NULL, NULL);
    AppendMenuW(ParamsMenu, MF_STRING, MenuExitClicked, L"Âûõîä");

    AppendMenuW(RootMenu, MF_POPUP, (UINT_PTR)ParamsMenu, L"Ïàðàìåòðû");

    SetMenu(hWnd, RootMenu);
}

void MainWndAddWidgets(HWND hWnd)
{
    hEditControl = CreateWindowW(L"edit", last_request.c_str(), WS_VISIBLE | WS_BORDER | WS_CHILD | ES_MULTILINE | WS_HSCROLL,
                                 5, 30,
                                 540, 45,
                                 hWnd, (HMENU)IDC_EditSearch,
                                 NULL, NULL);

    RECT rect = get_rect(hWnd, hEditControl);
    HWND wind = CreateWindowW(L"button", L"Ïîèñê", WS_VISIBLE | WS_CHILD | ES_CENTER,
                              rect.right + 10, rect.top,
                              120, 30,
                              hWnd, (HMENU)ButtonSearchClicked,
                              NULL, NULL);

    rect = get_rect(hWnd, wind);
    wind = CreateWindowW(L"button", L"Çàãðóçèòü èíäåêñ", WS_VISIBLE | WS_CHILD | ES_CENTER,
                         rect.right + 10, rect.top,
                         150, 30,
                         hWnd, (HMENU)ButtonLoadIndexClicked,
                         NULL, NULL);

    rect = get_rect(hWnd, wind);
    wind = CreateWindowW(L"button", L"Ïîñòðîèòü èíäåêñ", WS_VISIBLE | WS_CHILD | ES_CENTER,
                         rect.right + 10, rect.top,
                         150, 30,
                         hWnd, (HMENU)ButtonCreateIndexClicked,
                         NULL, NULL);

    rect = get_rect(hWnd, wind);
    wind = CreateWindowW(L"button", L"Âûïîëíèòü ñëèÿíèå", WS_VISIBLE | WS_CHILD | ES_CENTER,
                         rect.right + 10, rect.top,
                         150, 30,
                         hWnd, (HMENU)ButtonMergeIndexClicked,
                         NULL, NULL);

    rect = get_rect(hWnd, hEditControl);
    hList = CreateWindowW(WC_LISTVIEWW, L"Ñïèñîê",
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
    lvc.pszText = (LPWSTR)L"Ññûëêà";
    lvc.iSubItem = 0;
    SendMessageW(hList, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvc);

    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = 300;
    lvc.pszText = (LPWSTR)L"Çàãîëîâîê";
    lvc.iSubItem = 1;
    SendMessageW(hList, LVM_INSERTCOLUMNW, 1, (LPARAM)&lvc);

    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = rc.right - rc.left - 100 - 300;
    lvc.pszText = (LPWSTR)L"Ïîäðîáíîñòè";
    lvc.iSubItem = 2;
    SendMessageW(hList, LVM_INSERTCOLUMNW, 2, (LPARAM)&lvc);

    AddItemToList(0, L"Çäåñü", L"áóäóò", L"ðåçóëüòàòû");

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

bool ApplySearch(vector<wstring> &infix_request,
                 dict<wstring, item> &terms)
{
    vector<wstring> postfix_request = {};

    bool correct = infix_request_to_postfix_request(infix_request, postfix_request);

    if (!correct)
    {
        wprintfc(RED, L"Íåêîððåêòíûé çàïðîñ\n");
        return false;
    }

    path path2postings = path(path2index) / "postings_list.data";
    FILE *fp_postings = NULL;
    
    ERROR_HANDLE(fp_postings = _wfopen(path2postings.c_str(), L"rb"), return false, "Íåâåðíûé ïóòü ê èíäåêñó");
    vector<int> posting_list = {};

    get_response(postfix_request, fp_postings, terms, n_docs,
                 posting_list);
    fclose(fp_postings);

    wprintfc(BLUE | RED, L"Ïî çàïðîñó: \' ");
    for (auto iter : infix_request)
    {
        wprintfc(BLUE | RED, L"%s ", iter.c_str());
    }
    wprintfc(BLUE | RED, L"\' íàéäåíî %lu äîêóìåíòîâ\n", posting_list.size());

    INFO_HANDLE("Îáðàáîòêà ðåçóëüòàòîâ ïîèñêà");

    /* Âû÷èñëåíèå êîìïîíåíò âåêòîðà çàïðîñà */

    request_wt.clear();

    int i, j;
    for (i = 0; i < postfix_request.size(); i++)
    {
        if (!is_term(postfix_request[i]) || terms.find(postfix_request[i]) == terms.end())  // íå òåðì, ëèáî åãî íåò â êîðïóñå
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

    foreach(ptr, request_wt) // íàõîäèì term_wt
    {
        ptr->second = (1 + log10(ptr->second)) * log10(n_docs / (double)terms[ptr->first].df);
    }

    norm = 0;
    for (auto ptr : request_wt) // ñ÷èòàåì íîðìó
    {
        norm += ptr.second * ptr.second;
    }
    norm = sqrt(norm);

    vector<int> terms_id(request_wt.size());
    vector<double> req_wt(request_wt.size());

    i = 0;
    foreach(ptr, request_wt) // íîðìàëèçóåì è çàïîëíÿåì âåêòîðà ñ èäåíòèôèêàòîðàìè
    {
        ptr->second /= norm;

        terms_id[i] = terms[ptr->first].id;
        req_wt[i] = ptr->second;
        i++;
    }

    /* Âû÷èñëåíèå êîìïîíåíò âåêòîðà äîêóìåíòà */

    vector<vector<double>> doc_wt(posting_list.size(), 
                                  vector<double>(request_wt.size()));
    weights.resize(posting_list.size()); // äîëãîæäàííûå âåñà
    
    #pragma omp parallel num_threads(omp_num_threads)                   \
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
            for(j = 0; j < req_wt.size(); j++) // öèêë ïî òåðìèíàì èç çàïðîñà
            {
                weight += req_wt[j] * doc_wt[i][j];
            }
            weights[i] = { posting_list[i], weight }; // âåñ + èíäåêñ äîêóìåíòà
        }
    }

    auto comp = [](const pair<uint, double> &l, const pair<uint, double> &r) -> bool
    {
        return l.second > r.second;
    };

    std::sort(weights.begin(), weights.end(), comp); // ñîðòèðîâî÷êà

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
    vector<item> table(m, { L"", L"", L"", vector<int>(0), vector<int>(0) }); // òàáëèöà îòâåòîâ
    
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
                table[k].page_url = L"Íå óäàëîñü èçâëå÷ü ññûëêó\n";
            }

            if (fetch_title(doc, table[k].title) || 
                fetch_alternative_title(doc, table[k].title))
            {
            }
            else
            {
                table[k].title = L"Íå óäàëîñü èçâëå÷ü çàãîëîâîê\n";
            }

            rc.resize(0);
            lc.resize(0);

            lc.push_back(-RADIUS_DETAILS);
            rc.push_back(-RADIUS_DETAILS);

            for (auto ptr : request_wt) // ïîëó÷àåì êîîðäèíàòû òåðìîâ äëÿ êàæäîãî òåðìà èç çàïðîñà
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

                /* âûâîäèì âñ¸ â ðàäèóñå R ñëåâà */
                str.resize(r0 - l0);
                std::copy(doc.begin() + l0, doc.begin() + r0, str.begin());
                table[k].desc += str;

                /* âûâîäèì òîêåí */
                str.resize(r1 - l1);
                std::copy(doc.begin() + l1, doc.begin() + r1, str.begin());
                table[k].begin.push_back(table[k].desc.size());
                table[k].end.push_back(table[k].desc.size() + str.size());
                table[k].desc += str;

                /* âûâîäèì âñ¸ â ðàäèóñå R ñïðàâà */
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

    /* Ïîêàçûâàåì ðåçóëüòàò â êîíñîëü è ñïèñîê */
    for (k = 0; k < m; k++)
    {
        wprintfc(BLUE, L"page_url: %s\n", table[k].page_url.c_str());

        wprintf(L"title: \"%s\"\n", table[k].title.c_str());

        wprintf(L"Äåòàëè: ");

        table[k].begin.push_back(table[k].desc.size());
        table[k].end.push_back(table[k].desc.size());

        wprintf(L"%s", table[k].desc.substr(0, table[k].begin[0]).c_str()); // âûâîäèì òî, ÷òî ñëåâà

        for (j = 0; j < table[k].begin.size() - 1; j++)
        {
            wprintfc(GREEN, L"%s", table[k].desc.substr(table[k].begin[j], table[k].end[j] - table[k].begin[j]).c_str()); // õàéëàéòèì òåðì

            for (i = table[k].begin[j]; i < table[k].end[j]; i++) // Ïåðåâîä â âåðõíèé ðåãèñòð òåðìîâ (õàéëàéò)
            {
                table[k].desc[i] = towupper(table[k].desc[i]);
            }

            wprintf(L"%s", table[k].desc.substr(table[k].end[j], table[k].begin[j + 1] - table[k].end[j]).c_str()); // âûâîäèì òî, ÷òî ñïðàâà
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

void ButtonLoadIndexClickedHandler()                                                                   
{                                                                                                      
    WARNING_HANDLE(fs::exists(path2corpus), break, L"Óêàæèòå ïðàâèëüíûé ïóòü ê êîðïóñó â ïàðàìåòðàõ"); 
    WARNING_HANDLE(fs::exists(path2index), break, L"Óêàæèòå ïðàâèëüíûé ïóòü ê èíäåêñó â ïàðàìåòðàõ");  
                                                                                                       
    load_index(path(path2index), terms, &n_docs);                                                      
    terms_list = vector<pair<wstring, int>>(terms.size());                                             
                                                                                                       
    int i = 0;                                                                                         
    time_t current_sec;                                                                                
    SETUP_PROGRESS(terms.size(), &current_sec, L"Ñîçäàíèå ëèíåéíîãî ñïèñêà òåðìèíîâ");                 
    foreach(ptr, terms)                                                                                
    {                                                                                                  
        terms_list[i] = { ptr->first, ptr->second.df };                                                
        i++;                                                                                           
        UPDATE_PROGRESS(i, terms.size(), &current_sec, L"Ñîçäàíèå ëèíåéíîãî ñïèñêà òåðìèíîâ");         
    }                                                                                                  
                                                                                                       
    INFO_HANDLE(L"Çàãðóçêà çàâåðøåíà");                                                                
}

void ButtonSearchClickedHandler()                                                           
{                                                                                           
    WARNING_HANDLE(fs::exists(path2corpus), return, L"Óêàæèòå ïóòü ê êîðïóñó");       
    WARNING_HANDLE(fs::exists(path2index), return, L"Óêàæèòå ïóòü ê èíäåêñó");
                                                                                            
    wchar request[MAX_PATH];                                                                
    GetWindowTextW(hEditControl, request, MAX_PATH);                                        
    wprintfc(GREEN, L"Çàïðîñ: \' %s \'\n", request);                                        
    last_request = request;                                                                 
                                                                                            
    vector<wstring> infix_request;                                                          
                                                                                            
    string_to_infix_request(last_request, infix_request, path2request_parser);              
                                                                                            
    ApplySearch(infix_request, terms);                                                      
    UpdateLabel();                                                                          
    ShowResult(0);                                                                          
    WriteDefaultParams();                                                                   
                                                                                            
    if (weights.size() < 30) /* ïðîâåðêà îïå÷àòîê  */                                       
    {                                                                                       
        vector<wstring> correct_request, no_lemma_request;                                  
        string_to_infix_request(last_request, no_lemma_request, path2request_parser, false);
                                                                                            
        correct_typos(no_lemma_request, correct_request, infix_request,
                      n_docs, terms, terms_list,         
                      0.7, omp_num_threads);                                                          
                                                                                            
        if (no_lemma_request == correct_request)                                            
        {                                                                                   
            return;                                                                         
        }                                                                                         
                                                                                                  
        wstring mes1 = L"";                                                                       
        for (int i = 0; i < no_lemma_request.size(); i++)                                         
        {   
            if (is_binoperator(no_lemma_request[i]))
            {
                no_lemma_request[i] += no_lemma_request[i];
            }
            mes1 += no_lemma_request[i] + L" ";                                                   
        }                                                                                         
        wstring mes2 = L"";                                                                       
        for (int i = 0; i < correct_request.size(); i++)                                          
        {   
            if (is_binoperator(correct_request[i]))
            {
                correct_request[i] += correct_request[i];
            }
            mes2 += correct_request[i] + L" ";                                                    
        }                                                                                         
                                                                                                  
        int mb = MessageBoxW(NULL,                                                                
                             (wstring(L"Ïî çàïðîñó: ") + mes1 +                                                    
                              wstring(L"\níàéäåíî ") + std::to_wstring(weights.size()) +                           
                              wstring(L" äîêóìåíòîâ\n") +                                                          
                              wstring(L"Ïîâòîðèòü ñ çàïðîñîì: ") + mes2 + wstring(L"?")).c_str(),                  
                             L"Âîçìîæíûå îïå÷àòêè",                                                                
                             MB_ICONEXCLAMATION | MB_YESNO);                                                       
                                                                                                  
        if (mb == IDYES)                                                                          
        {                                                                                         
            last_request = mes2;                                                                  
            SetWindowTextW(hEditControl, last_request.c_str());                                   
            string_to_infix_request(mes2, infix_request, path2request_parser);                    
            ApplySearch(infix_request, terms);                                                   
                                                                                                 
            UpdateLabel();                                                                       
            ShowResult(0);                                                                       
            WriteDefaultParams();                                                                
        }                                                                                        
    }                                                                                            
}

LRESULT CALLBACK MainProcedure(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_KEYDOWN:

        if (wp == VK_RETURN) // ENTER
        {
            ButtonSearchClickedHandler();
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case MenuExitClicked:
            PostQuitMessage(0);
            break;
        case MenuParamsClicked: // íàæàòèå íà ïàðàìåòðû
            if(IsWindow(hParamsWnd)) break;
            RegisterParamsWindow(This);
            hParamsWnd = CreateParamsWindow(This, hMainWnd);
            break;
        case ButtonSearchClicked: // êíîïêà ïîèñêà íàæàòà
            ButtonSearchClickedHandler();

            break;
        case ButtonCreateIndexClicked:
            WARNING_HANDLE(fs::exists(path2corpus), goto DEFAULT, L"Óêàæèòå ïóòü ê êîðïóñó");

            INFO_HANDLE(L"Ñîçäàíèå áëî÷íîãî èíäåêñà êîðïóñà: %s", path2corpus.c_str());
            create_blocks(path2corpus, path2blocks, path2lemmatizator, omp_num_threads);
            INFO_HANDLE(L"Ñîçäàíèå áëî÷íîãî èíäåêñà îêîí÷åíî. Âûïîëíèòå ñëèÿíèå");

            break;
        case ButtonMergeIndexClicked:
            INFO_HANDLE(L"Ñëèÿíèå áëî÷íîãî èíäåêñà");
            merge_blocks(path2blocks, path2index);
            INFO_HANDLE(L"Ñëèÿíèå áëî÷íîãî èíäåêñà çàâåðøåíî");

            INFO_HANDLE(L"Âû÷èñëåíèå ñòàòèñòèêè");
            calculate_doc_tf(path2index);
            INFO_HANDLE(L"Âû÷èñëåíèå ñòàòèñòèêè çàâåðøåíî");

            /*INFO_HANDLE(L"Î÷èñòêà ïàïêè ñ áëî÷íûì èíäåêñîì");
            for (auto iter : fs::recursive_directory_iterator(path2blocks))
            {
                fs::remove(iter);
            }*/

            INFO_HANDLE(L"Óñïåøíî. Èíäåêñ ñîçäàí â %s", path2index.c_str());

            ButtonLoadIndexClickedHandler();

            break;

        case ButtonLoadIndexClicked:

            ButtonLoadIndexClickedHandler();

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

        DEFAULT:
            break;
        }
        break;

    case WM_CREATE:
        ReadDefaultParams(); // ÷òåíèå ïàðàìåòðîâ ïî óìîë÷àíèþ èç ini.config
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
