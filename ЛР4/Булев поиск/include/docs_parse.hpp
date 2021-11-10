#ifndef DOCS_PARSE_HPP
#define DOCS_PARSE_HPP

#include <stdio.h>
#include <string>
#include <vector>
#include <regex>
#include <algorithm>
#include <Windows.h>

using uint = unsigned int;
using std::wstring;
using std::string;
using std::vector;

int string_to_wide_string(const string &str, wstring &wstr)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0,
        str.data(), str.size(),
        NULL, 0);

    wstr.resize(size_needed);

    MultiByteToWideChar(CP_UTF8, 0,
                        str.data(), str.size(),
                        wstr.data(), size_needed);
    return size_needed;
}

int wide_string_to_string(const wstring &wstr, string &str)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0,
                                          wstr.data(), wstr.size(),
                                          NULL, 0,
                                          NULL, NULL);

    str.resize(size_needed);

    WideCharToMultiByte(CP_UTF8, 0,
                        wstr.data(), wstr.size(),
                        str.data(), str.size(),
                        NULL, NULL);
    return size_needed;
}

bool get_doc_by_id(int id, uint n_docs, FILE *fp_docs, wstring &doc)
{
    if (id >= n_docs) return false;

    uint offset, size, n_chars;
    wstring doc_name = L"";
    string doc_data;
    FILE *fp_doc;

    fseek(fp_docs, sizeof(uint) + id * sizeof(uint), SEEK_SET);
  
    fread(&offset, sizeof(uint), 1, fp_docs);
    fseek(fp_docs, sizeof(uint) + (n_docs + 1) * sizeof(uint) + offset, SEEK_SET);
    fread(&n_chars, sizeof(uint), 1, fp_docs);
    doc_name.resize(n_chars);
    fread(doc_name.data(), sizeof(wchar_t), n_chars, fp_docs);
    fread(&offset, sizeof(uint), 1, fp_docs);
    fread(&size, sizeof(uint), 1, fp_docs);

    fp_doc = _wfopen(doc_name.c_str(), L"rb");

    if(!fp_doc) return false;

    fseek(fp_doc, offset, SEEK_SET);
    doc_data.resize(size);
    fread(doc_data.data(), sizeof(char), size, fp_doc);
    fclose(fp_doc);

    string_to_wide_string(doc_data, doc); // декодирование из юникода

    return true;
}

#define FETCH_VALUE(DATA)                                                                    \
bool fetch_##DATA(wstring &doc, wstring &DATA)                                               \
{                                                                                            \
    wstring re = L"\""#DATA"\"";                                                             \
    auto res = std::search(doc.begin(), doc.end(), re.begin(), re.end());                    \
                                                                                             \
    if (res == doc.end())                                                                    \
    {                                                                                        \
        return false;                                                                        \
    }                                                                                        \
    else                                                                                     \
    {                                                                                        \
        int i = res + re.size() - doc.begin();                                               \
                                                                                             \
        while (i < doc.size() && doc[i] != L'"')                                             \
        {                                                                                    \
            i++;                                                                             \
        }                                                                                    \
                                                                                             \
        if(i >= doc.size()) return false;                                                    \
                                                                                             \
        i++;                                                                                 \
                                                                                             \
        DATA.resize(0);                                                                      \
        while(i < doc.size() && doc[i-1] != L'\\' && doc[i] != L'"')                         \
        {                                                                                    \
            DATA += doc[i];                                                                  \
            i++;                                                                             \
        }                                                                                    \
    }                                                                                        \
    return true;                                                                             \
}

FETCH_VALUE(page_url)
FETCH_VALUE(title)
FETCH_VALUE(alternative_title)

void get_terms_coord(const wstring &document, wstr_to_wstr parse,
                     dict<wstring, pair<vector<int> *, vector<int> *>> &tree)
{
    /*
    * Получить термы из документа в координатном виде
    *
    * Параметры:
    * document - документ в виде последовательности символов
    * parse    - преобразователь токена в терм
    * tree     - ссылка на текущее дерево с термами и координатами
    */

    bool end = true;
    int i;
    wstring token = L"",
            term = L"";

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
                tree[term] = { new vector<int>{i - (int)token.size()}, new vector<int>{i} }; // добавляем координаты
            }
            else // найден
            {
                ptr->second.first->push_back(i - token.size());
                ptr->second.second->push_back(i);
            }

            token.resize(0);
            end = true;
        }
    }
}

int get_term_freq_in_doc(wstring &term, int id,
                         const dict<wstring, int> &terms, FILE *fp_posting,
                         vector<int> &work)
{
    /*
    * По сути немного долго
    */
    auto ptr = terms.begin();
    int offset, n_docs, freq;

    if ((ptr = terms.find(term)) == terms.end())
    {
        return 0;
    }
    else
    {
        offset = ptr->second;
        fseek(fp_posting, sizeof(uint) + offset, SEEK_SET);
        fread(&n_docs, sizeof(uint), 1, fp_posting);

        work.resize(n_docs);
        fread(work.data(), sizeof(int), n_docs, fp_posting);

        int i = std::lower_bound(work.begin(), work.end(), id) - work.begin();
        fseek(fp_posting, i * sizeof(int), SEEK_CUR);
        fread(&freq, sizeof(int), 1, fp_posting);
    }

    return freq;
}
#endif