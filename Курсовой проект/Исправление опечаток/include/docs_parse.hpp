#ifndef DOCS_PARSE_HPP
#define DOCS_PARSE_HPP

#include "defs.hpp"

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

bool find_posting_list(const wstring &term, vector<int> &posting_list, FILE *fp,
                       const dict<wstring, item> &terms)
{
    auto iter = terms.find(term);

    if (iter == terms.end())
    {
        posting_list.resize(0);
        return false;
    }

    fseek(fp, sizeof(uint) + iter->second.offset, SEEK_SET);

    uint n_docs;
    fread(&n_docs, sizeof(uint), 1, fp);
    posting_list.resize(n_docs);
    fread(posting_list.data(), sizeof(int), n_docs, fp);

    return true;
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
        wstring tmp;                                                                         \
        trim(DATA, tmp);                                                                     \
        DATA = tmp;                                                                          \
    }                                                                                        \
    return true;                                                                             \
}

FETCH_VALUE(page_url)
FETCH_VALUE(title)
FETCH_VALUE(alternative_title)

void get_term_coords(FILE *fp_posting, const wstring &term, int doc_id, const dict<wstring, item> &terms,
                     vector<int> &starts, vector<int> &ends, vector<int> &work)
{
    /*
    * ѕолучить координаты терма в документе
    */
    
    auto ptr = terms.find(term);
    if (ptr == terms.end())
    {
        starts.resize(0);
        ends.resize(0);
        return;
    }

    fseek(fp_posting, sizeof(uint) + ptr->second.offset, SEEK_SET);
    uint n_docs;
    fread(&n_docs, sizeof(uint), 1, fp_posting);
    work.resize(n_docs);
    fread(work.data(), sizeof(int), n_docs, fp_posting);

    int i = std::lower_bound(work.begin(), work.end(), doc_id) - work.begin();
    if (i >= work.size() || work[i] != doc_id) // документ не найден
    {
        starts.resize(0);
        ends.resize(0);
        return;
    }

    int freq;
    fseek(fp_posting, i * sizeof(int), SEEK_CUR);
    fread(&freq, sizeof(int), 1, fp_posting);
    uint offset;
    fseek(fp_posting, ((int)n_docs - i - 1) * sizeof(int) + i * sizeof(uint), SEEK_CUR);
    fread(&offset, sizeof(uint), 1, fp_posting);
    fseek(fp_posting, ((int)n_docs - i - 1) * sizeof(uint) + offset * sizeof(int), SEEK_CUR);
    starts.resize(freq);
    ends.resize(freq);
    fread(starts.data(), sizeof(int), freq, fp_posting);
    fread(ends.data(), sizeof(int), freq, fp_posting);
}

void get_terms_weights_in_doc(const path &path2index, int doc_id, 
                              const vector<int> &terms_id, vector<double> &weights, 
                              vector<int> &iwork, vector<double> &work)
{
    weights = vector<double>(terms_id.size(), 0);
    wstring path2tf = path2index / L"tf.data";
    FILE *fp = _wfopen(path2tf.c_str(), L"rb");
    uint n_docs;
    fread(&n_docs, sizeof(uint), 1, fp);
    if (doc_id >= n_docs)
    {
        return;
    }
    fseek(fp, doc_id * sizeof(uint), SEEK_CUR);
    uint offset;
    fread(&offset, sizeof(uint), 1, fp);
    fseek(fp, (n_docs - doc_id - 1) * sizeof(uint) + offset, SEEK_CUR);
    uint n_terms;
    fread(&n_terms, sizeof(uint), 1, fp);
    iwork.resize(n_terms);
    work.resize(n_terms);

    int i, j;
    auto &all_terms = iwork;
    auto &all_weights = work;
    fread(all_terms.data(), sizeof(int), n_terms, fp);
    fread(all_weights.data(), sizeof(double), n_terms, fp);
    fclose(fp);

    for (i = 0; i < terms_id.size(); i++)
    {
        j = std::lower_bound(all_terms.begin(), all_terms.end(), terms_id[i]) - all_terms.begin();
        if(j >= all_terms.size() || all_terms[j] != terms_id[i]) continue;

        weights[i] = all_weights[j];
    }
}

#endif