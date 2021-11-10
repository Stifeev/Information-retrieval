#ifndef CREATE_INDEX_HPP
#define CREATE_INDEX_HPP

#include <stdio.h>
#include <iostream>
#include <string>
#include <set>
#include <map>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <queue>
#include <ctime>
#include <time.h>
#include <algorithm>
#include <locale>
#include <Windows.h>
#include <omp.h>

#include "token_parse.hpp"

namespace fs = std::filesystem;

using uint = unsigned int;
using wchar = wchar_t;

using std::set;
using std::map;
#define dict std::unordered_map
using std::queue;
using std::pair;

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

#define WARNING_HANDLE(call, message, ...)                                    \
do                                                                            \
{                                                                             \
    if(!(call))                                                               \
    {                                                                         \
        fwprintf(stdout, L"[WARNING] " message L"\n", __VA_ARGS__);           \
        return false;                                                         \
    }                                                                         \
}                                                                             \
while(0);

#define INFO_HANDLE(message, ...)                                   \
do                                                                  \
{                                                                   \
     fwprintf(stdout, L"[INFO] " message L"\n", __VA_ARGS__);       \
}                                                                   \
while(0);

// Начальный размер буфера для считывания одного документа
#define BUF_SIZE 50000 

// Количество потоков по умолчанию
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

void get_terms(const vector<wchar_t> &document, int doc_id, wstr_to_wstr parse,
               map<wstring, postings_list *> &tree)
{
    /*
    * Получить термы из документа
    * 
    * Параметры:
    * document - документ в виде последовательности символов
    * doc_id   - идентификатор документа
    * parse    - преобразователь токена в терм
    * tree     - ссылка на текущее дерево с термами
    */
    bool end = true;
    int i;
    wstring token = L"",
            term;

    for (i = 0; i < document.size(); i++)
    {
        if (iswalnum(document[i]) ||
            (!end && document[i] == L'-' && iswalnum(document[i + 1]))
            )
        {
            token += document[i];
            end = false;
        }
        else if (!end) // Разделитель найден
        {
            term = parse(token);
            
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

            token.resize(0);
            term.resize(0);
            end = true;
        }
    }
}

bool create_blocks(path path2corpus, path path2blocks, wstr_to_wstr parse, int omp_num_threads=OMP_NUM_THREADS)
{
    /*
    * Создать блочный индекс
    */

    bool create = false, merge = false, clear = true;
    int i, j, n_files;

    uint offset, size,
         n_chars, n_docs, n_terms;

    wchar_t scheme_path2block_terms[256], scheme_path2block_postings_list[256], scheme_path2block_docs_id[256],
            path2block_terms[256], path2block_postings_list[256], path2block_docs_id[256],
            path2file[256];

    vector<path> pathes;
    
    wcscpy(scheme_path2block_terms, (path2blocks / L"block_terms_%d.data").c_str());
    wcscpy(scheme_path2block_postings_list, (path2blocks / L"block_postings_list_%d.data").c_str());
    wcscpy(scheme_path2block_docs_id, (path2blocks / L"block_docs_id_%d.data").c_str());

    vector<vector<wchar_t>> buf(omp_num_threads); // используется всеми потоками
    vector<vector<uint>> offsets(omp_num_threads);
    vector<map<wstring, postings_list *>> tree(omp_num_threads);

    /* Создание блоков */

    INFO_HANDLE(L"Число запрошенных потоков = %d", omp_num_threads);

    FILE *fp_file = NULL, *fp_terms = NULL, *fp_postings_list = NULL, *fp_docs_id = NULL;

    for (i = 0; i < omp_num_threads; i++)
    {
        buf[i] = vector<wchar_t>(BUF_SIZE);
        offsets[i] = {};
        tree[i] = {};
    }

    if (!fs::exists(path2blocks)) // создание директории
    {
        fs::create_directories(path2blocks);
    }

    for (auto iter : fs::recursive_directory_iterator(path2blocks)) // очистка директории
    {
        fs::remove(iter);
    }
   
    /* Сбор путей */
    for (auto iter : fs::recursive_directory_iterator(path2corpus))
    {
        if (iter.is_regular_file() && EXTENSIONS.find(iter.path().extension()) != EXTENSIONS.end())
        {
            pathes.push_back(iter.path());
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
 
    #pragma omp parallel num_threads(omp_num_threads)                                                                                   \
                         shared(buf, offsets, tree, pathes)                                                                             \
                         firstprivate(n_files, scheme_path2block_terms, scheme_path2block_postings_list, scheme_path2block_docs_id)     \
                         private(path2file, path2block_terms, path2block_postings_list, path2block_docs_id,                             \
                                 i, j, fp_file, fp_terms, fp_postings_list, fp_docs_id, offset, n_docs, size, n_chars, n_terms)
    {
        int id = omp_get_thread_num(),
            id_offset = omp_get_num_threads();

        for (i = id; i < n_files; i += id_offset) // Цикл по файлам
        {
            wcscpy(path2file, pathes[i].c_str());
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
                get_terms(buf[id], n_docs - 1, parse,
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

    return true;
}

bool merge_blocks(path path2blocks, path path2index)
{
    /* 
    * Слияние блоков 
    */

    setlocale(LC_ALL, "Russian");

    int i, j;

    uint offset, size,
         n_chars, n_docs, n_terms;

    double total_documents=0,
           total_terms=0;

    wchar_t scheme_path2block_terms[256], scheme_path2block_postings_list[256], scheme_path2block_docs_id[256],
            path2block_terms[256], path2block_postings_list[256], path2block_docs_id[256];

    if (!fs::exists(path2index)) // создание директории
    {
        fs::create_directories(path2index);
    }

    for (auto iter : fs::recursive_directory_iterator(path2index)) // очистка директории
    {
        fs::remove(iter);
    }

    /* Сбор путей */

    int n_blocks = 0;
    for (auto iter : fs::recursive_directory_iterator(path2blocks))
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

    wcscpy(scheme_path2block_terms, (path2blocks / L"block_terms_%d.data").c_str());
    wcscpy(scheme_path2block_postings_list, (path2blocks / L"block_postings_list_%d.data").c_str());
    wcscpy(scheme_path2block_docs_id, (path2blocks / L"block_docs_id_%d.data").c_str());

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

    INFO_HANDLE(L"Общее число термов в словаре = %.0lf\n"
                L"Документов = %.0lf",
                total_terms,
                total_documents);

    return true;
}

bool load_index(const path &path2index, dict<wstring, int> &terms)
{
    terms.clear();

    wchar path2terms[256];
    wcscpy(path2terms, (path2index / "terms.data").c_str());

    FILE *fp_terms = NULL;
    int i, offset;
    uint n_terms, n_chars;
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

    fclose(fp_terms);

    INFO_HANDLE("Загружено %d термов", n_terms);

    return true;
}

#undef INFO_HANDLE
#undef WARNING_HANDLE
#undef ERROR_HANDLE

#endif