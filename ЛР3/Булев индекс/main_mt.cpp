#include <stdio.h>
#include <iostream>
#include <string>
#include <set>
#include <map>
#include <filesystem>
#include <vector>
#include <queue>
#include <ctime>
#include <time.h>
#include <algorithm>
#include <locale>
#include <Windows.h> // для WideCharToMultiByte (работа с кодировками переменной длины)
#include <omp.h>

namespace fs = std::filesystem;

using uint = unsigned int;

using std::set;
using std::map;
using std::queue;
using std::pair;

using fs::path;

using std::string;
using std::wstring;

using std::vector;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

#define ERROR_HANDLE(call, message, ...)                                      \
do                                                                            \
{                                                                             \
    if(!(call))                                                               \
    {                                                                         \
        fwprintf(stdout, L"[WARNING] " message L"\n", __VA_ARGS__);           \
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

#define DESCRIPTION  L"usage: ./Булев\\ индекс.exe "                                 \
                     L"-i \'абс. путь к корпусу\' "                                  \
                     L"-o \'абс. путь к индексу\' "                                  \
                     L"-t \'абс. путь к директории с временными файлами"             \
                     L"\' [-create] [-merge] [-clear]\n"                             \
                     L"где флаги означают:\n"                                        \
                     L"-create - создать блочный индекс\n"                           \
                     L"-merge - выполнить слияние блочного индекса\n"                \
                     L"-clear - очистить папку с временными файлами после слияния\n"

// Начальный размер буфера для считывания одного документа
#define BUF_SIZE 50000 

#define OMP_NUM_THREADS 4

set<wstring> EXTENSIONS = { L".json", L".jsonlines", L".txt" };

bool get_doc(FILE *fp, vector<wchar_t> &buf)
{
    /*
    * Записать документ в буфер
    */

    bool end;
    int offset = 0,
        len = 0;

    buf.resize(BUF_SIZE);

    while (!(end = (fgetws(buf.data() + offset, buf.size() - offset, fp) == NULL)))
    {
        len = wcslen(buf.data()); // длина буфера в символах

        if ((len + 1) == buf.size() && buf[len - 1] != L'\n') // overflow
        {
            buf.resize(buf.size() * 2);
            offset = len;

            continue;
        }
        else
        {
            break;
        }
    }

    buf.resize(len); // убираем лишнее
    return !end;
}

struct postings_list
{
    vector<int> docs_id;
    vector<int> docs_freq;

    postings_list() // конструктор 1
    {
        docs_id = vector<int>(0);
        docs_freq = vector<int>(0);
    }

    postings_list(int id) // конструктор 2
    {
        docs_id = { id };
        docs_freq = { 1 };
    }
};

void get_terms(const vector<wchar_t> &document, int doc_id,
               map<wstring, postings_list *> &tree)
{
    /*
    * Получить термы
    */
    bool end = true;
    int i;
    wstring term = L"";

    for (i = 0; i < document.size(); i++)
    {
        if (iswalnum(document[i]) ||
            (!end && document[i] == L'-' && iswalnum(document[i + 1]))
            )
        {
            term += towlower(document[i]);
            end = false;
        }
        else if (!end) // Разделитель найден
        {
            auto ptr = tree.find(term);
            if (ptr == tree.end()) // термин не найден
            {
                tree[term] = new postings_list(doc_id);
            }
            else // найден
            {
                if (ptr->second->docs_id.back() == doc_id)
                {
                    ptr->second->docs_freq.back()++; // увеличиваем частоту
                }
                else
                {
                    ptr->second->docs_id.push_back(doc_id); // добавляем документ
                    ptr->second->docs_freq.push_back(1);
                }
            }

            term.resize(0);
            end = true;
        }
    }
}

int wmain(int argc, wchar_t *argv[])
{
    setlocale(LC_CTYPE, "rus"); // кодировка на поток вывода
    SetConsoleCP(1251);         // кодировка на поток ввода
    SetConsoleOutputCP(1251);   // кодировка на поток вывода

    bool create = false, merge = false, clear = true;
    vector<wchar_t> buf[OMP_NUM_THREADS];
    int i, j, n_files;

    uint offset, size,
         n_chars, n_docs, n_terms;

    path path2corpus = L"", path2index = L"./index", path2tmp = L"./tmp";

    wchar_t scheme_path2block_terms[256], scheme_path2block_postings_list[256], scheme_path2block_docs_id[256],
            path2block_terms[256], path2block_postings_list[256], path2block_docs_id[256],
            path2file[256];

    vector<path> pathes;

    high_resolution_clock::time_point time_start, time_end;
    double duration;

    time_start = high_resolution_clock::now();
    double total_size = 0, total_terms = 0, total_documents = 0;

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
        else if (_wcsicmp(argv[i], L"-i") == 0)
        {
            ERROR_HANDLE(i + 1 < argc, DESCRIPTION);
            path2corpus = argv[i + 1];
        }
        else if (_wcsicmp(argv[i], L"-o") == 0)
        {
            ERROR_HANDLE(i + 1 < argc, DESCRIPTION);
            path2index = argv[i + 1];
        }
        else if (_wcsicmp(argv[i], L"-t") == 0)
        {
            ERROR_HANDLE(i + 1 < argc, DESCRIPTION);
            path2tmp = argv[i + 1];
        }
    }

    ERROR_HANDLE(!path2corpus.empty(), DESCRIPTION);

    wcscpy(scheme_path2block_terms, (path2tmp / L"block_terms_%d.data").c_str());
    wcscpy(scheme_path2block_postings_list, (path2tmp / L"block_postings_list_%d.data").c_str());
    wcscpy(scheme_path2block_docs_id, (path2tmp / L"block_docs_id_%d.data").c_str());

    if (create)
    {
        /* Создание блоков */

        INFO_HANDLE(L"Создание блочного индекса");

        vector<uint> offsets[OMP_NUM_THREADS];
        map<wstring, postings_list *> tree[OMP_NUM_THREADS];

        FILE *fp_file = NULL, *fp_terms = NULL, *fp_postings_list = NULL, *fp_docs_id = NULL;

        for (i = 0; i < OMP_NUM_THREADS; i++)
        {
            buf[i] = vector<wchar_t>(BUF_SIZE);
            offsets[i] = {};
            tree[i] = {};
        }

        /* Создание директорий */
        if (!fs::exists(path2index))
        {
            fs::create_directories(path2index);
        }

        if (!fs::exists(path2tmp))
        {
            fs::create_directories(path2tmp);
        }

        /* Сбор путей */
        for (auto iter : fs::recursive_directory_iterator(path2corpus))
        {
            if (iter.is_regular_file() && EXTENSIONS.find(iter.path().extension()) != EXTENSIONS.end())
            {
                pathes.push_back(iter.path());
                total_size += iter.file_size();
            }
        }

        n_files = pathes.size(); // Количество файлов

        /* Сортировка названий файлов по убыванию их размеров */
        auto comp = [](const path &l, const path &r) -> bool
        {
            return fs::file_size(l) < fs::file_size(r);
        };

        std::sort(pathes.begin(), pathes.end(), comp);

        /* Создание блоков */
        path *ptr_pathes = pathes.data();
        #pragma omp parallel num_threads(OMP_NUM_THREADS)                                                                                   \
                                         shared(buf, offsets, tree, ptr_pathes)                                                                     \
                                         firstprivate(n_files, scheme_path2block_terms, scheme_path2block_postings_list, scheme_path2block_docs_id) \
                                         private(path2file, path2block_terms, path2block_postings_list, path2block_docs_id,                         \
                                                 i, j, fp_file, fp_terms, fp_postings_list, fp_docs_id, offset, n_docs, size, n_chars, n_terms)
        {
            int id = omp_get_thread_num(),
                id_offset = OMP_NUM_THREADS;

            for (i = id; i < n_files; i += id_offset) // Цикл по файлам
            {
                wcscpy(path2file, ptr_pathes[i].c_str());
                swprintf(path2block_terms, scheme_path2block_terms, i + 1);
                swprintf(path2block_postings_list, scheme_path2block_postings_list, i + 1);
                swprintf(path2block_docs_id, scheme_path2block_docs_id, i + 1);

                INFO_HANDLE("Thread %d processing block %d/%d : %s",
                            id, i + 1, n_files,
                            path2file);

                ERROR_HANDLE(fp_file = _wfopen(path2file, L"rt, ccs=UTF-8"), "Can't open %s", path2file);
                ERROR_HANDLE(fp_terms = _wfopen(path2block_terms, L"wb"), "Can't create %s", path2block_terms);
                ERROR_HANDLE(fp_postings_list = _wfopen(path2block_postings_list, L"wb"), "Can't create %s", path2block_postings_list);
                ERROR_HANDLE(fp_docs_id = _wfopen(path2block_docs_id, L"wb"), "Can't create %s", path2block_docs_id);

                offsets[id].resize(0);
                offset = 0;

                n_docs = 0;

                while (get_doc(fp_file, buf[id])) // Цикл по документам
                {
                    n_docs++;

                    /* Получаем размер документа в байтах (ftell не работает для текстовых файлов) */
                    size = WideCharToMultiByte(CP_UTF8, 0,
                                               buf[id].data(), buf[id].size(),
                                               NULL, 0,
                                               NULL, NULL);

                    offsets[id].push_back(offset);
                    offset += size + 1;

                    /* Создаём термы */
                    get_terms(buf[id], n_docs - 1,
                        tree[id]);
                }

                fclose(fp_file);

                offsets[id].push_back(offset);

                /* Заполнение файла с таблицей документов docs_id */

                 /* Количество документов */
                fwrite(&n_docs, sizeof(uint), 1, fp_docs_id);

                n_chars = wcslen(path2file);
                offset = 0;
                for (j = 0; j < n_docs + 1; j++) // смещений больше на единицу (храним их сумму)
                {
                    fwrite(&offset, sizeof(uint), 1, fp_docs_id);
                    offset += sizeof(uint) + n_chars * sizeof(wchar_t) + 2 * sizeof(uint);
                }
                for (j = 0; j < n_docs; j++)
                {
                    fwrite(&n_chars, sizeof(uint), 1, fp_docs_id);
                    fwrite(path2file, sizeof(wchar_t), n_chars, fp_docs_id);
                    fwrite(offsets[id].data() + j, sizeof(uint), 1, fp_docs_id);
                    size = offsets[id][j + 1] - offsets[id][j];
                    fwrite(&size, sizeof(uint), 1, fp_docs_id);
                }

                fclose(fp_docs_id);

                /* Заполнение файла с термами terms и словопозициями postings_list */
                n_terms = tree[id].size();
                fwrite(&n_terms, sizeof(uint), 1, fp_terms);
                fwrite(&n_terms, sizeof(uint), 1, fp_postings_list);

                INFO_HANDLE("Block %d has %d terms",
                            i + 1, n_terms);

                offset = 0;
                for (auto iter : tree[id])
                {
                    n_chars = iter.first.size();
                    fwrite(&n_chars, sizeof(uint), 1, fp_terms);
                    fwrite(iter.first.data(), sizeof(wchar_t), iter.first.size(), fp_terms);
                    fwrite(&offset, sizeof(uint), 1, fp_terms);

                    n_docs = iter.second->docs_id.size();
                    fwrite(&n_docs, sizeof(uint), 1, fp_postings_list);
                    fwrite(iter.second->docs_id.data(), sizeof(int), n_docs, fp_postings_list);   // список идентификаторов
                    fwrite(iter.second->docs_freq.data(), sizeof(int), n_docs, fp_postings_list); // список частот

                    offset += sizeof(uint) + 2 * n_docs * sizeof(int);
                }

                fclose(fp_terms);
                fclose(fp_postings_list);

                for (auto iter : tree[id]) // очистка дерева
                {
                    delete iter.second;
                }
                tree[id].clear();
            }
        }
    }

    if (merge)
    {
        /* Слияние блоков */

        /* Сбор путей */

        int n_blocks = 0;
        for (auto iter : fs::recursive_directory_iterator(path2tmp))
        {
            n_blocks++;
        }
        n_blocks /= 3;

        struct term_line
        {
            wstring term;
            uint offset;
        };

        vector<queue<term_line>> terms(n_blocks);
        term_line lterm = { L"", 0 };
        FILE *fp_doc_id = NULL, *fp_term = NULL, *fp_posting_list;

        FILE *fp_terms = NULL, *fp_docs_id = NULL;

        vector<FILE *> fp_postings_list(n_blocks, NULL);

        wchar_t path2docs_id[256], path2terms[256], path2postings_list[256];
        uint terms_total, terms_current;

        wcscpy(path2docs_id, (path2index / "docs_id.data").c_str());
        wcscpy(path2terms, (path2index / "terms.data").c_str());
        wcscpy(path2postings_list, (path2index / "postings_list.data").c_str());

        /* Создание очередей термов */

        terms_total = 0;
        for (i = 0; i < n_blocks; i++) // Открываем все файлы с термами и создаём очереди
        {
            wprintf(L"\r[INFO] Создание очередей термов: %4d блок из %4d", i + 1, n_blocks);
            swprintf(path2block_terms, scheme_path2block_terms, i + 1);
            fp_terms = _wfopen(path2block_terms, L"rb");

            fread(&n_terms, sizeof(uint), 1, fp_terms);
            terms_total += n_terms;

            for (j = 0; j < n_terms; j++) // чтение термов
            {
                fread(&n_chars, sizeof(uint), 1, fp_terms);
                lterm.term.resize(n_chars);
                fread(lterm.term.data(), sizeof(wchar_t), n_chars, fp_terms); // читаем терм
                fread(&lterm.offset, sizeof(uint), 1, fp_terms); // читаем сдвиг

                terms[i].push(lterm);
            }

            fclose(fp_terms);
        }
        wprintf(L"\n");

        /* Слияние docs_id */

        uint shift, last_shift;
        vector<uint> shifts(n_blocks + 1); // сдвиги по docs_id
        vector<uint> offsets = {};
        wstring doc_name = L"";

        offset = 0;
        shift = 0;
        last_shift = 0;

        /* Обработка блоков смещений перед таблицей */
        for (i = 0; i < n_blocks; i++)
        {
            swprintf(path2block_docs_id, scheme_path2block_docs_id, i + 1);
            fp_docs_id = _wfopen(path2block_docs_id, L"rb");

            fread(&n_docs, sizeof(uint), 1, fp_docs_id);
            shifts[i] = offset;
            offset += n_docs;

            offsets.resize(offsets.size() + n_docs);
            fread(offsets.data() + offsets.size() - n_docs,
                  sizeof(uint), n_docs, fp_docs_id);

            for (j = 0; j < n_docs; j++) // перерасчёт смещений в таблице
            {
                offsets[shifts[i] + j] += shift;
            }

            fread(&last_shift, sizeof(uint), 1, fp_docs_id);
            shift += last_shift;

            fclose(fp_docs_id);
        }

        fp_doc_id = _wfopen(path2docs_id, L"wb");
        total_documents = offset;
        fwrite(&offset, sizeof(uint), 1, fp_doc_id);
        fwrite(offsets.data(), sizeof(uint), offset, fp_doc_id);
        fwrite(&shift, sizeof(uint), 1, fp_doc_id);

        for (i = 0; i < n_blocks; i++)
        {
            wprintf(L"\r[INFO] Слияние docs_id: %4d блок из %4d", i + 1, n_blocks);
            swprintf(path2block_docs_id, scheme_path2block_docs_id, i + 1);
            fp_docs_id = _wfopen(path2block_docs_id, L"rb");
            fread(&n_docs, sizeof(uint), 1, fp_docs_id);
            fseek(fp_docs_id, (n_docs + 1) * sizeof(uint), SEEK_CUR);

            for (j = 0; j < n_docs; j++)
            {
                fread(&n_chars, sizeof(uint), 1, fp_docs_id);
                doc_name.resize(n_chars);
                fread(doc_name.data(), sizeof(wchar_t), n_chars, fp_docs_id);
                fread(&offset, sizeof(uint), 1, fp_docs_id);
                fread(&size, sizeof(uint), 1, fp_docs_id);

                fwrite(&n_chars, sizeof(uint), 1, fp_doc_id);
                fwrite(doc_name.data(), sizeof(wchar_t), n_chars, fp_doc_id);
                fwrite(&offset, sizeof(uint), 1, fp_doc_id);
                fwrite(&size, sizeof(uint), 1, fp_doc_id);
            }

            fclose(fp_docs_id);
        }
        wprintf(L"\n");
        fclose(fp_doc_id);

        fp_term = _wfopen(path2terms, L"wb");
        fp_posting_list = _wfopen(path2postings_list, L"wb");

        fseek(fp_term, sizeof(uint), SEEK_SET);
        fseek(fp_posting_list, sizeof(uint), SEEK_SET);

        /* Взятие минимума из термов и создание сконкатенированных списков */

        wstring min_term = L"Старт";
        vector<int> active_blocks(n_blocks);
        vector<int> docs_id = {};
        vector<int> docs_freq = {};

        /* Открытие блоков со словопозициями */
        for (i = 0; i < n_blocks; i++)
        {
            swprintf(path2block_postings_list, scheme_path2block_postings_list, i + 1);
            fp_postings_list[i] = _wfopen(path2block_postings_list, L"rb");
        }

        INFO_HANDLE(L"Слияние слопозиций термов");

        terms_current = terms_total;
        offset = 0;

        time_t current_time, current_sec = time(NULL);

        n_terms = 0;
        while (terms_current > 0)
        {
            min_term = WCHAR_MAX;
            active_blocks.resize(0);

            /* Минимум из термов */
            for (i = 0; i < n_blocks; i++)
            {
                if (terms[i].empty()) continue;

                min_term = min(terms[i].front().term, min_term);
            }
            /* Выделение индексов активных блоков */
            for (i = 0; i < n_blocks; i++)
            {
                if (!terms[i].empty() && terms[i].front().term == min_term)
                    active_blocks.push_back(i);
            }

            docs_id.resize(0);
            docs_freq.resize(0);

            for (i = 0; i < active_blocks.size(); i++) // цикл по активным блокам
            {
                fseek(fp_postings_list[active_blocks[i]],
                    terms[active_blocks[i]].front().offset + sizeof(uint),
                    SEEK_SET);
                fread(&n_docs, sizeof(uint), 1, fp_postings_list[active_blocks[i]]);

                docs_id.resize(docs_id.size() + n_docs);
                docs_freq.resize(docs_freq.size() + n_docs);

                fread(docs_id.data() + docs_id.size() - n_docs, sizeof(int), n_docs, fp_postings_list[active_blocks[i]]);
                fread(docs_freq.data() + docs_freq.size() - n_docs, sizeof(int), n_docs, fp_postings_list[active_blocks[i]]);

                for (j = 0; j < n_docs; j++) // выполняем смещение id
                {
                    docs_id[docs_id.size() - n_docs + j] += shifts[active_blocks[i]];
                }

                terms[active_blocks[i]].pop(); // убираем из очереди
                terms_current--;
            }

            n_docs = docs_id.size();
            fwrite(&n_docs, sizeof(uint), 1, fp_posting_list);
            fwrite(docs_id.data(), sizeof(int), n_docs, fp_posting_list);
            fwrite(docs_freq.data(), sizeof(int), n_docs, fp_posting_list);

            n_chars = min_term.size();
            fwrite(&n_chars, sizeof(uint), 1, fp_term);
            fwrite(min_term.data(), sizeof(wchar_t), n_chars, fp_term);
            fwrite(&offset, sizeof(uint), 1, fp_term);
            offset += sizeof(uint) + 2u * n_docs * sizeof(int);

            current_time = time(NULL);

            if (current_time > current_sec)
            {
                wprintf(L"\r[INFO] Осталось термов: %12d", terms_current);
                current_sec = current_time;
            }
            n_terms++;
        }

        wprintf(L"\r[INFO] Осталось термов: %12d", terms_current);
        wprintf(L"\n");

        fseek(fp_term, 0, SEEK_SET);
        fseek(fp_posting_list, 0, SEEK_SET);

        total_terms = n_terms;
        fwrite(&n_terms, sizeof(uint), 1, fp_term);
        fwrite(&n_terms, sizeof(uint), 1, fp_posting_list);

        fclose(fp_term);
        fclose(fp_posting_list);

        for (i = 0; i < n_blocks; i++)
        {
            swprintf(path2block_postings_list, scheme_path2block_postings_list, i + 1);
            fclose(fp_postings_list[i]);
        }

        if (clear)
        {
            INFO_HANDLE(L"Очистка временных файлов");

            fs::remove_all(path2tmp);
        }
    }

    if (create && merge) // выводим статистику
    {
        time_end = high_resolution_clock::now();
        duration = (double)std::chrono::duration_cast<milliseconds>(time_end - time_start).count();

        INFO_HANDLE(L"Общее число термов в словаре = %.0lf\n"                                          \
                    L"Время выполнения = %.1lf sec, размер корпуса = %.3lf Gb, документов = %.0lf\n"   \
                    L"Средняя скорость на документ = %.3lf ms\n"                                       \
                    L"Средняя скорость на килобайт = %.3lf ms",
                    total_terms,
                    duration / 1e3, total_size / (1024. * 1024. * 1024.), total_documents,
                    duration / total_documents,
                    (duration / total_size) * 1024);
    }


    return 0;
}