#ifndef CREATE_INDEX_HPP
#define CREATE_INDEX_HPP

#include "defs.hpp"
#include "token_parse.hpp"

namespace fs = std::filesystem;

// Начальный размер буфера для считывания одного документа
#define BUF_SIZE 50000 

// Количество потоков по умолчанию
#define OMP_NUM_THREADS 4

set<wstring> EXTENSIONS = { L".json", L".jsonlines", L".txt" };

bool get_doc(FILE *fp, wstring &buf)
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
    vector<vector<int>> starts;
    vector<vector<int>> ends;
};

void get_terms(const wstring &document, const path &path2lemmatizator, int req_num,
               vector<wstring> &terms, vector<vector<int>> &starts, vector<vector<int>> &ends)
{
    path lemmatizator_dir = path2lemmatizator.parent_path().parent_path().parent_path();
    wstring lemm_request = lemmatizator_dir / (wstring(L"io\\i") + std::to_wstring(req_num) + wstring(L".txt"));
    wstring lemm_response = lemmatizator_dir / (wstring(L"io\\o") + std::to_wstring(req_num) + wstring(L".txt"));

    FILE *fp = _wfopen(lemm_request.c_str(),
                       L"w");
    fwprintf(fp, L"%s", document.c_str());
    fclose(fp);

    wstring command = path2lemmatizator.wstring() + wstring(L" ") + lemm_request + wstring(L" ") + lemm_response;

    /* Запрос */
    _wsystem(command.c_str());

    fp = _wfopen(lemm_response.c_str(),
                 L"r");

    wstring buf = L"",
            term = L"";
    buf.resize(256);
    int freq, len, i, res;

    terms.resize(0);
    starts.resize(0);
    ends.resize(0);

    while (fwscanf(fp, L"%s %d", buf.data(), &freq) != -1)
    {
        len = wcslen(buf.data());
        term.resize(len);
        
        wcscpy(term.data(), buf.data());
        terms.push_back(term);
        starts.push_back(vector<int>(freq));
        ends.push_back(vector<int>(freq));

        i;
        for (i = 0; i < freq; i++)
        {
            fwscanf(fp, L"%d %d", 
                    starts.back().data() + i,
                    ends.back().data() + i);
        }
    }

    fclose(fp);
}

void store_terms(int doc_id,
                 const vector<wstring> &terms, const vector<vector<int>> &starts, const vector<vector<int>> &ends,
                 map<wstring, postings_list *> &tree)
{
    /*
    * Записать термы, постинги и координаты в дерево
    */
    bool end = true;
    int i, freq;
    wstring token = L"",
            term;
    postings_list *posting=NULL;

    for (i = 0; i < terms.size(); i++)
    {
        freq = starts[i].size();
        auto ptr = tree.find(terms[i]);

        if (ptr == tree.end()) // термин не найден
        {
            posting = new postings_list;
            posting->docs_id = { doc_id };
            posting->starts = vector<vector<int>>(1, starts[i]);
            posting->ends = vector<vector<int>>(1, ends[i]);

            tree[terms[i]] = posting;
        }
        else // термин уже есть в коллекции
        {
            ptr->second->docs_id.push_back(doc_id);
            ptr->second->starts.push_back(vector<int>(starts[i]));
            ptr->second->ends.push_back(vector<int>(ends[i]));
        }
    }
}

bool create_blocks(const path &path2corpus, const path path2blocks, 
                   const path path2lemmatizator, 
                   int omp_num_threads=OMP_NUM_THREADS)
{
    /*
    * Создать блочный индекс
    */

    ERROR_HANDLE(fs::exists(path2corpus), return false, L"Путь к корпусу не существует");
    ERROR_HANDLE(fs::exists(path2lemmatizator), return false, L"Путь к лемматизатору не существует");

    int n_files;

    vector<path> pathes;
    
    /* Создание блоков */

    INFO_HANDLE(L"Число запрошенных потоков = %d", omp_num_threads);

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
 
    #pragma omp parallel num_threads(omp_num_threads)       \
                         shared(pathes)                     \
                         firstprivate(n_files,              \
                                      path2blocks,          \
                                      path2lemmatizator)
    {
        int id = omp_get_thread_num(),
            id_offset = omp_get_num_threads(),
            i;

        wchar path2file[256];

        for (i = id; i < n_files; i += id_offset) // Цикл по файлам
        {
            wcscpy(path2file, pathes[i].c_str());
            
            INFO_HANDLE("Thread %d processing block %d/%d : %s",
                        id, i + 1, n_files,
                        path2file);

            wstring command = path2lemmatizator.wstring() + 
                              L" -i " + L"\"" + path2file + L"\"" +
                              L" -o " + L"\"" + path2blocks.wstring() + L"\"" +
                              L" -id " + std::to_wstring(i);

            _wsystem(command.c_str());
        }
    }

    return true;
}

bool merge_blocks(path path2blocks, path path2index)
{
    /* 
    * Слияние блоков 
    */

    int i, j;

    uint offset, size,
         n_chars, n_docs, n_terms;

    uint total_documents=0,
         total_terms=0;

    wchar scheme_path2block_terms[256], scheme_path2block_postings_list[256], scheme_path2block_docs_id[256],
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
    wstring name;
    wstring prefix = L"block_terms";
    for (auto iter : fs::directory_iterator(path2blocks))
    {
        name = iter.path().filename().wstring();
        auto ptr = std::search(name.begin(), name.end(), prefix.begin(), prefix.end());
        if(ptr == name.begin())
            n_blocks++;
    }

    ERROR_HANDLE(n_blocks > 0, return false, L"tmp dir has no block index");

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
            fread(lterm.term.data(), sizeof(wchar_t), n_chars, fp_terms);     // читаем терм
            fread(&lterm.offset, sizeof(uint), 1, fp_terms);                  // читаем сдвиг

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
    vector<uint> docs_offsets = {};
    vector<vector<int>> starts = {};
    vector<vector<int>> ends = {};

    /* Открытие блоков со словопозициями и координатами */
    for (i = 0; i < n_blocks; i++)
    {
        swprintf(path2block_postings_list, scheme_path2block_postings_list, i + 1);
        fp_postings_list[i] = _wfopen(path2block_postings_list, L"rb");
    }

    INFO_HANDLE(L"Слияние слопозиций термов");

    terms_current = terms_total;
    uint global_offset = 0;

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
        docs_offsets.resize(0);
        starts.resize(0);
        ends.resize(0);

        offset = 0;
        uint m = 0;
        for (i = 0; i < active_blocks.size(); i++) // цикл по активным блокам
        {
            /* Сдвиг для чтения числа документов, в которых встречается терм */
            fseek(fp_postings_list[active_blocks[i]],
                  terms[active_blocks[i]].front().offset + sizeof(uint),
                  SEEK_SET);
            fread(&n_docs, sizeof(uint), 1, fp_postings_list[active_blocks[i]]);

            m = docs_id.size();
            docs_id.resize(m + n_docs);
            docs_freq.resize(m + n_docs);
            docs_offsets.resize(m + n_docs);
            starts.resize(m + n_docs);
            ends.resize(m + n_docs);

            fread(docs_id.data() + m, sizeof(int), n_docs, fp_postings_list[active_blocks[i]]);
            fread(docs_freq.data() + m, sizeof(int), n_docs, fp_postings_list[active_blocks[i]]);
            fread(docs_offsets.data() + m, sizeof(uint), n_docs, fp_postings_list[active_blocks[i]]);
            
            for (j = 0; j < n_docs; j++) // выполняем смещение id и по offset
            {
                docs_id[m + j] += shifts[active_blocks[i]];
                docs_offsets[m + j] += offset;
            }

            /* Продолжаем чтение */
            for (j = 0; j < n_docs; j++)
            {
                starts[m + j] = vector<int>(docs_freq[m + j]);
                ends[m + j] = vector<int>(docs_freq[m + j]);

                fread(starts[m + j].data(), sizeof(int), starts[m + j].size(), fp_postings_list[active_blocks[i]]);
                fread(ends[m + j].data(), sizeof(int), ends[m + j].size(), fp_postings_list[active_blocks[i]]);

                offset += 2 * starts[m + j].size();
            }
          
            terms[active_blocks[i]].pop(); // убираем из очереди
            terms_current--;
        }

        n_docs = docs_id.size();
        fwrite(&n_docs, sizeof(uint), 1, fp_posting_list);
        fwrite(docs_id.data(), sizeof(int), n_docs, fp_posting_list);
        fwrite(docs_freq.data(), sizeof(int), n_docs, fp_posting_list);
        fwrite(docs_offsets.data(), sizeof(uint), n_docs, fp_posting_list);

        for (i = 0; i < n_docs; i++)
        {
            fwrite(starts[i].data(), sizeof(int), starts[i].size(), fp_posting_list);
            fwrite(ends[i].data(), sizeof(int), ends[i].size(), fp_posting_list);
        }

        n_chars = min_term.size();
        fwrite(&n_chars, sizeof(uint), 1, fp_term);
        fwrite(min_term.data(), sizeof(wchar_t), n_chars, fp_term);

        fwrite(&global_offset, sizeof(uint), 1, fp_term);
        global_offset += sizeof(uint) + 2u * n_docs * sizeof(int) + n_docs * sizeof(uint) + offset * sizeof(int);

        n_terms++;
        NOTIFY(current_sec, 1, L"\r[INFO] Осталось термов: %12d", terms_current);
    }

    wprintf(L"\r[INFO] Осталось термов: %12d\n", terms_current);

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

    INFO_HANDLE(L"Общее число термов в словаре = %d\n"
                L"Документов = %d",
                total_terms,
                total_documents);

    return true;
}

bool load_index(const path &path2index,
                dict<wstring, item> &terms, uint *num_docs = NULL)
{
    terms.clear();

    wstring path2terms    = path2index / "terms.data",
            path2postings = path2index / "postings_list.data",
            path2docs     = path2index / "docs_id.data";

    FILE *fp_terms = NULL, *fp_postings = NULL;
    int i;
    uint n_terms, n_chars, offset, n_docs;
    wstring term;

    ERROR_HANDLE(fp_terms    = _wfopen(path2terms.c_str(), L"rb"), return false, L"");
    ERROR_HANDLE(fp_postings = _wfopen(path2postings.c_str(), L"rb"), return false, L"");

    fread(&n_terms, sizeof(uint), 1, fp_terms);

    time_t current_sec;
    SETUP_PROGRESS(n_terms, &current_sec, L"Чтение термов");
    for (i = 0; i < n_terms; i++)
    {
        fread(&n_chars, sizeof(uint), 1, fp_terms);
        term.resize(n_chars);

        fread(term.data(), sizeof(wchar), n_chars, fp_terms);
        fread(&offset, sizeof(uint), 1, fp_terms);

        fseek(fp_postings, sizeof(uint) + offset, SEEK_SET);
        fread(&n_docs, sizeof(uint), 1, fp_postings);

        terms[term] = { offset, (int)n_docs, i };

        UPDATE_PROGRESS(i, n_terms, &current_sec, L"Чтение термов");
    }

    fclose(fp_terms);
    fclose(fp_postings);

    if (num_docs)
    {
        FILE *fp = _wfopen(path2docs.c_str(), L"rb");
        fread(num_docs, sizeof(uint), 1, fp);
        fclose(fp);
    }

    INFO_HANDLE("Загружено %d термов", n_terms);

    return true;
}

bool calculate_doc_tf(const path &path2index)
{
    wstring path2postings = path2index / L"postings_list.data",
            path2docs     = path2index / L"docs_id.data",
            path2terms    = path2index / L"terms_id.data",
            path2tf       = path2index / L"tf.data";

    FILE *fp_docs = NULL;
    ERROR_HANDLE(fp_docs = _wfopen(path2docs.c_str(), L"rb"), return false, L"path doesn't exists");
    uint n_docs;

    fread(&n_docs, sizeof(uint), 1, fp_docs);
    fclose(fp_docs);

    vector<pair<vector<int>, vector<int>>> tf(n_docs, { vector<int>(0), vector<int>(0) }); // документ -> кол-во терминов

    FILE *fp_posting_list = NULL;

    ERROR_HANDLE(fp_posting_list = _wfopen(path2postings.c_str(), L"rb"), return false, L"path doesn't exists");
    uint n_terms;
    fread(&n_terms, sizeof(uint), 1, fp_posting_list);
    int i, j;
    vector<int> docs(0), freq(0);
    uint offset;

    time_t current_sec = time(NULL), current_time;
    vector<int> df(n_terms);

    for (i = 0; i < n_terms; i++)
    {
        fread(&n_docs, sizeof(uint), 1, fp_posting_list);
        df[i] = n_docs; // сохраням число документов для терма
        docs.resize(n_docs);
        fread(docs.data(), sizeof(int), n_docs, fp_posting_list);
        freq.resize(n_docs);
        fread(freq.data(), sizeof(int), n_docs, fp_posting_list);

        for (j = 0; j < n_docs; j++)
        {
            tf[docs[j]].first.push_back(i);
            tf[docs[j]].second.push_back(freq[j]);
        }

        fseek(fp_posting_list, (n_docs - 1) * sizeof(uint), SEEK_CUR);
        fread(&offset, sizeof(uint), 1, fp_posting_list);
        fseek(fp_posting_list, offset * sizeof(int) + 2u * (uint)(freq.back()) * sizeof(int), SEEK_CUR);

        NOTIFY(current_sec, 1, L"\rПервый проход. Термов осталось: %12d", (int)(n_terms - i - 1));
    }

    wprintf(L"\rПервый проход. Термов осталось: %12d\n", 0);
    fclose(fp_posting_list);

    FILE *fp_tf = NULL;
    
    ERROR_HANDLE(fp_tf = _wfopen(path2tf.c_str(), L"wb"), return false, L"can't create tf");
    n_docs = tf.size();
    vector<uint> offsets(n_docs + 1, 0);
    fwrite(&n_docs, sizeof(uint), 1, fp_tf);
    fseek(fp_tf, n_docs * sizeof(uint), SEEK_CUR);
    
    vector<double> doc_wt = {};
    double norm;

    current_sec = time(NULL);
    for (i = 0; i < tf.size(); i++) // цикл по документам
    {
        n_terms = tf[i].first.size();
        fwrite(&n_terms, sizeof(uint), 1, fp_tf);
        fwrite(tf[i].first.data(), sizeof(int), n_terms, fp_tf); // пишем индексы термов

        doc_wt.resize(n_terms);
        norm = 0;
        for (j = 0; j < n_terms; j++)
        {
            doc_wt[j] = (1 + log10(tf[i].second[j])) * log10(n_docs / (double)df[tf[i].first[j]]);
            norm += doc_wt[j] * doc_wt[j];
        }
        norm = sqrt(norm);
        for (j = 0; j < n_terms; j++)
        {
            doc_wt[j] /= norm;
        }

        fwrite(doc_wt.data(), sizeof(double), n_terms, fp_tf);
        offsets[i + 1] = offsets[i] + 1 * sizeof(uint) + n_terms * sizeof(int) + n_terms * sizeof(double);

        NOTIFY(current_sec, 1, L"\rВторой проход. Документов осталось: %12d", (int)(tf.size() - i - 1));
    }
    fseek(fp_tf, sizeof(uint), SEEK_SET);
    fwrite(offsets.data(), sizeof(uint), n_docs, fp_tf);

    wprintf(L"\rВторой проход. Документов осталось: %12d\n", 0);
    fclose(fp_tf);
    return true;
}

#endif