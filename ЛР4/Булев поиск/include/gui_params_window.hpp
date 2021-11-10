#ifndef GUI_PARAMS_WINDOW_HPP
#define GUI_PARAMS_WINDOW_HPP

#include "../include/gui_header.hpp"
#include "../resource.h"

const wchar_t ParamsWndClassName[] = L"ParamsWndClass";

HWND buttonOpenPath2Corpus = NULL;
HWND editPath2Corpus = NULL;
HWND buttonOpenPath2Index = NULL;
HWND editPath2Index = NULL;
HWND buttonOpenPath2Blocks = NULL;
HWND editPath2Blocks = NULL;
HWND editNumThreads = NULL;

static bool IsCreate = false;

LRESULT CALLBACK ParamsProcedure(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
bool BrowseForFolder(HWND hwnd, const wchar_t *title, const wchar_t *folder, wchar_t *select_folder);
INT CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData);

ATOM RegisterParamsWindow(HINSTANCE hInst) // регистрация окна с параметрами
{
    WNDCLASSW NWC = { 0 }; // новый класс окна

    NWC.hInstance = hInst;
    NWC.hCursor = LoadCursor(NULL, IDC_ARROW);
    NWC.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
    NWC.lpszClassName = ParamsWndClassName; // имя класса окна
    NWC.hbrBackground = (HBRUSH)COLOR_WINDOW;
    NWC.lpfnWndProc = ParamsProcedure;

    return RegisterClassW(&NWC);
}

HWND CreateParamsWindow(HINSTANCE hInst, HWND parent) // создание окна с параметрами
{
    return CreateWindowW(ParamsWndClassName, L"Параметры",
                         WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_BORDER | WS_VISIBLE,
                         Wndx + 300, Wndy + 200,
                         800, 300,
                         parent, NULL,
                         hInst, NULL);
}

void ParamsWndAddWidgets(HWND hWnd)
{
    /* ************************************************************************* */

    int xLabel = 5, yLabel = 10,
        wLabel = 150, hLabel = 40,
        offset = 10;

    int xEdit = xLabel + wLabel + 10, yEdit = 10,
        wEdit = 550, hEdit = 40;

    int xButton = xEdit + wEdit + 10, yButton = 10,
        wButton = 30, hButton = 30;

    CreateWindowW(L"static", L"Путь к директории с корпусом", WS_VISIBLE | WS_CHILD,
                  xLabel, yLabel,
                  wLabel, hLabel,
                  hWnd, NULL,
                  NULL, NULL);

    editPath2Corpus = CreateWindowW(WC_EDITW, path2corpus, WS_VISIBLE | WS_BORDER | WS_CHILD | ES_MULTILINE | WS_HSCROLL,
                                    xEdit, yEdit,
                                    wEdit, hEdit,
                                    hWnd, (HMENU)ParamsPath2CorpusEditChanged,
                                    This, NULL);

    buttonOpenPath2Corpus = CreateWindowW(L"button", L"", WS_VISIBLE | WS_CHILD | ES_CENTER | BS_BITMAP,
                                          xButton, yButton,
                                          wButton, hButton,
                                          hWnd, (HMENU)ParamsPath2CorpusButtonClicked,
                                          NULL, NULL);
  
    SendMessage(buttonOpenPath2Corpus, BM_SETIMAGE, IMAGE_BITMAP,
                (LPARAM)LoadImage(This, MAKEINTRESOURCE(IDB_BITMAP1), IMAGE_BITMAP, 30, 30, 0));

    /* ************************************************************************* */

    CreateWindowW(L"static", L"Путь к директории с индексом", WS_VISIBLE | WS_CHILD,
                  xLabel, yLabel + hLabel + offset,
                  wLabel, hLabel,
                  hWnd, NULL,
                  NULL, NULL);

    editPath2Index = CreateWindowW(L"edit", path2index, WS_VISIBLE | WS_BORDER | WS_CHILD | ES_MULTILINE | WS_HSCROLL,
                                    xEdit, yEdit + hEdit + offset,
                                    wEdit, hEdit,
                                    hWnd, (HMENU)ParamsPath2IndexEditChanged,
                                    NULL, NULL);

    buttonOpenPath2Index = CreateWindowW(L"button", L"", WS_VISIBLE | WS_CHILD | ES_CENTER | BS_BITMAP,
                                         xButton, yButton + hButton + (offset + hLabel - hButton),
                                         wButton, hButton,
                                         hWnd, (HMENU)ParamsPath2IndexButtonClicked,
                                         NULL, NULL);

    SendMessage(buttonOpenPath2Index, BM_SETIMAGE, IMAGE_BITMAP,
                (LPARAM)LoadImage(This, MAKEINTRESOURCE(IDB_BITMAP1), IMAGE_BITMAP, 30, 30, 0));

    /* ************************************************************************* */

    CreateWindowW(L"static", L"Путь к директории с временными файлами", WS_VISIBLE | WS_CHILD,
                  xLabel, yLabel + 2 * hLabel + 2 * offset,
                  wLabel, hLabel,
                  hWnd, NULL,
                  NULL, NULL);

    editPath2Blocks = CreateWindowW(L"edit", path2blocks, WS_VISIBLE | WS_BORDER | WS_CHILD | ES_MULTILINE | WS_HSCROLL,
                                    xEdit, yEdit + 2 * hEdit + 2 * offset,
                                    wEdit, hEdit,
                                    hWnd, (HMENU)ParamsPath2BlocksEditChanged,
                                    NULL, NULL);

    buttonOpenPath2Blocks = CreateWindowW(L"button", L"", WS_VISIBLE | WS_CHILD | ES_CENTER | BS_BITMAP,
                                          xButton, yButton + 2 * hButton + 2 * (offset + hLabel - hButton),
                                          wButton, hButton,
                                          hWnd, (HMENU)ParamsPath2BlocksButtonClicked,
                                          NULL, NULL);

    SendMessage(buttonOpenPath2Blocks, BM_SETIMAGE, IMAGE_BITMAP,
                (LPARAM)LoadImage(This, MAKEINTRESOURCE(IDB_BITMAP1), IMAGE_BITMAP, 30, 30, 0));

    /* ************************************************************************* */

    CreateWindowW(L"static", L"Число потоков", WS_VISIBLE | WS_CHILD,
                  xLabel, yLabel + 3 * hLabel + 3 * offset,
                  wLabel, hLabel,
                  hWnd, NULL,
                  NULL, NULL);

    editNumThreads = CreateWindowW(L"edit", L"4", WS_VISIBLE | WS_BORDER | WS_CHILD,
                                   xEdit, yEdit + 3 * hEdit + 3 * offset,
                                   60, 30,
                                   hWnd, (HMENU)ParamsEditNumThreadsChanged,
                                   NULL, NULL);

    HWND UpDownNumThreads = CreateWindowW(UPDOWN_CLASSW,
                                          L"4",
                                          WS_CHILDWINDOW | WS_VISIBLE |
                                          UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_HOTTRACK, 
                                          0, 0,
                                          0, 0,
                                          hWnd, NULL,
                                          This, NULL);

    SendMessageW(UpDownNumThreads, UDM_SETRANGE, 0, MAKELPARAM(8, 1));
    SendMessageW(UpDownNumThreads, UDM_SETPOS, 0, (LPARAM)omp_num_threads);

    IsCreate = true;
}

void trim(const wstring &str, wstring &res)
{
    int i;
    for (i = 0; i < str.size(); i++)
    {
        if (str[i] != L' ' && str[i] != L'\n')
        {
            break;
        }
    }
    int j;
    for (j = str.size() - 1; j >= 0; j--)
    {
        if (str[j] != L' ' && str[j] != L'\n')
        {
            break;
        }
    }
    j++;

    res.resize(j - i);

    std::copy(str.begin() + i, str.begin() + j, res.begin());
}

void WriteDefaultParams()
{
    FILE *fp;
    fp = _wfopen(L"ini.config", L"w");
    fwprintf(fp, L"Путь к корпусу : %s\n", path2corpus);
    fwprintf(fp, L"Путь к блочному индексу : %s\n", path2blocks);
    fwprintf(fp, L"Путь к индексу : %s\n", path2index);
    fwprintf(fp, L"Число потоков : %d\n", omp_num_threads);
    fwprintf(fp, L"Последний запрос : %s\n", last_request.c_str());
    fclose(fp);
}

void ReadDefaultParams()
{
    FILE *fp = NULL;
    if (!fs::exists(L"ini.config"))
    {
        wcscpy(path2corpus, L"укажите путь к корпусу");
        wcscpy(path2blocks, (fs::current_path() / L"tmp\\").c_str());
        wcscpy(path2index, (fs::current_path() / L"index\\").c_str());
        last_request = L"Самый лучший фильм";
        omp_num_threads = 4;
    }
    else
    {
        fp = _wfopen(L"ini.config", L"r");

        map<wstring, wstring> pathes = {};
        wstring _key, _value, key, value;

        while (fgetws(buf, MAX_PATH, fp))
        {
            int len = wcslen(buf);

            int point = wstring(buf).find(L":");
            
            _key.resize(point);
            _value.resize(len - point - 1);
            std::copy(buf, buf + point, _key.begin());
            std::copy(buf + point + 1, buf + len, _value.begin());

            trim(_key, key);
            trim(_value, value);

            pathes[key] = value;
        }

        auto ptr = pathes.begin();

        if ((ptr = pathes.find(wstring(L"Путь к корпусу"))) != pathes.end())
        {
            wcscpy(path2corpus, ptr->second.data());
        }
        else
        {
            wcscpy(path2corpus, L"укажите путь к корпусу");
        }
        if ((ptr = pathes.find(wstring(L"Путь к временным файлам"))) != pathes.end())
        {
            wcscpy(path2blocks, ptr->second.data());
        }
        else
        {
            wcscpy(path2blocks, (fs::current_path() / L"tmp\\").c_str());
        }
        if ((ptr = pathes.find(wstring(L"Путь к индексу"))) != pathes.end())
        {
            wcscpy(path2index, ptr->second.data());
        }
        else
        {
            wcscpy(path2index, (fs::current_path() / L"index\\").c_str());
        }
        if ((ptr = pathes.find(wstring(L"Число потоков"))) != pathes.end())
        {
            omp_num_threads = stoi(ptr->second);
        }
        else
        {
            omp_num_threads = 4;
        }
        if ((ptr = pathes.find(wstring(L"Последний запрос"))) != pathes.end())
        {
            last_request = ptr->second;
        }
        else
        {
            last_request = L"Самый лучший фильм";
        }
    }
}

LRESULT CALLBACK ParamsProcedure(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case ParamsPath2CorpusButtonClicked:
            if (BrowseForFolder(hWnd, L"Выберите директорию с корпусом", L"C:\\", path2corpus));
            {
                SetWindowTextW(editPath2Corpus, path2corpus);
                WriteDefaultParams();
            }

            break;
        case ParamsPath2IndexButtonClicked:
            if (BrowseForFolder(hWnd, L"Выберите директорию для хранения индекса", L"C:\\", path2index));
            {
                SetWindowTextW(editPath2Index, path2index);
                WriteDefaultParams();
            }

            break;
        case ParamsPath2BlocksButtonClicked:
            if (BrowseForFolder(hWnd, L"Выберите директорию для хранения временных файлов", L"C:\\", path2blocks));
            {
                SetWindowTextW(editPath2Blocks, path2blocks);
                WriteDefaultParams();
            }

            break;

        case ParamsPath2CorpusEditChanged:
            if(!IsCreate) break;

            GetWindowTextW(editPath2Corpus, path2corpus, MAX_PATH);
            WriteDefaultParams();
            break;

        case ParamsPath2IndexEditChanged:
            if (!IsCreate) break;

            GetWindowTextW(editPath2Index, path2index, MAX_PATH);
            WriteDefaultParams();
            break;

        case ParamsPath2BlocksEditChanged:
            if (!IsCreate) break;

            GetWindowTextW(editPath2Blocks, path2blocks, MAX_PATH);
            WriteDefaultParams();
            break;
        
        case ParamsEditNumThreadsChanged:
            if (!IsCreate) break;

            GetWindowTextW(editNumThreads, buf, MAX_PATH);
            omp_num_threads = _wtoi(buf);
            WriteDefaultParams();
            break;
        default:
            break;
        }
        break;

    case WM_CREATE:
        ParamsWndAddWidgets(hWnd);
        break;

    case WM_DESTROY:
        IsCreate = false;
        break;
    default:
        return DefWindowProcW(hWnd, msg, wp, lp);
    }
}

bool BrowseForFolder(HWND hwnd, const wchar_t *title, const wchar_t *folder, wchar_t *select_folder)
{
    BROWSEINFOW br = {0};
    wchar_t szDisplayName[MAX_PATH] = L"";
    br.lpfn = BrowseCallbackProc;
    br.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    br.hwndOwner = hwnd;
    br.lpszTitle = title;
    br.lParam = (LPARAM)folder;
    br.pszDisplayName = szDisplayName;

    LPITEMIDLIST pidl = NULL;
    if ((pidl = SHBrowseForFolderW(&br)) != NULL)
    {
        return SHGetPathFromIDListW(pidl, select_folder);
    }
    else
    {
        return false;
    }
}

INT CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
    if (uMsg == BFFM_INITIALIZED) SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
    return 0;
}

#endif