#ifndef QUOTE_SEARCH_HPP
#define QUOTE_SEARCH_HPP

#include "defs.hpp"

bool close(int begin1, int end1,
           int begin2, int end2,
           int dist)
{
    if (end1 - 1 < begin2) // сначала идЄт терм1, затем терм2
    {
        return begin2 - (end1 - 1) <= dist;
    }
    else // наоборот
    {
        return begin1 - (end2 - 1) <= dist;
    }
}

bool quote_search(const vector<wstring> &quote_terms, const vector<vector<int>> &quote_postings, FILE *fp_postings,
    const dict<wstring, item> &terms, vector<int> &res, int dist = -1)
{
    const vector<int> *p1, *p2;
    int i, j, k,
        i1, i2,
        p;
    vector<int> work = {};
    bool find = false, find_doc = false;

    if (quote_terms.size() == 0) // быстрый выход 1
    {
        res.resize(0);
        return true;
    }

    if (quote_terms.size() == 1) // быстрый выход 2
    {
        res.resize(quote_postings[0].size());
        std::copy(quote_postings[0].begin(), quote_postings[0].end(), res.begin());
        return res.size() > 0;
    }

    if (dist < 0) dist = quote_terms.size() - 1;
    dist = max(dist, quote_terms.size() - 1);

    /* ѕеревод dist из рассто€ни€ в словах в рассто€ние в символах */
    int avg_space = 2,
        avg_word = 7;
    int dist_char = 0;
    for (i = 1; i < quote_terms.size(); i++)
    {
        dist_char += avg_space + quote_terms[i].size();
    }
    dist_char += (dist - quote_terms.size() + 1) * (avg_word + avg_space);
    dist_char -= avg_word;

    dist = dist_char;

    dict<int, vector<int>> begin1 = {}, begin2 = {},
                           end1 = {}, end2 = {};
    vector<int> *begins1, *begins2, *ends1, *ends2;
    vector<int> tmp = {}, begins_tmp = {}, ends_tmp = {};

    p1 = &quote_postings[0]; // указатель на вектор постингов

    i1 = 0;
    i2 = 0;

    for (i = 1; i < quote_terms.size(); i++) // цикл по термам
    {
        p2 = &quote_postings[i];
        i1 = 0;
        i2 = 0;
        tmp.resize(0); // результирующий список
        while (i1 < p1->size() && i2 < p2->size())
        {
            if (p1->at(i1) == p2->at(i2)) // в обоих документов есть термы
            {
                p = p1->at(i1); // идентификатор документа

                if (i == 1) // перва€ итераци€
                {
                    begin1[p] = {};
                    end1[p] = {};
                    /* достаЄм координаты дл€ первого терма */
                    get_term_coords(fp_postings, quote_terms[0], p, terms,
                                    begin1[p], end1[p],
                                    work);
                }

                begin2[p].clear();
                end2[p].clear();
                /* достаЄм координаты дл€ второго терма */
                get_term_coords(fp_postings, quote_terms[i], p, terms,
                                begin2[p], end2[p],
                                work);

                begins1 = &begin1[p];
                ends1 = &end1[p];
                begins2 = &begin2[p];
                ends2 = &end2[p];

                find = false;
                find_doc = false;
                begins_tmp.resize(0);
                ends_tmp.resize(0);

                for (j = 0; j < begins1->size(); j++) // O(n * log(n))
                {
                    k = std::lower_bound(begins2->begin(), begins2->end(), begins1->at(j)) - begins2->begin();

                    if (k == 0) // слева
                    {
                        find = close(begins1->at(j), ends1->at(j),
                                     begins2->at(0), ends2->at(0),
                                     dist);
                    }
                    else if (k == begins2->size()) // справа
                    {
                        find = close(begins1->at(j), ends1->at(j),
                                     begins2->at(k - 1), ends2->at(k - 1),
                                     dist);
                    }
                    else // usual case
                    {
                        find = close(begins1->at(j), ends1->at(j),
                                     begins2->at(k - 1), ends2->at(k - 1),
                                     dist);
                        if (!find)
                        {
                            find = close(begins1->at(j), ends1->at(j),
                                         begins2->at(k), ends2->at(k),
                                         dist);
                        }
                    }

                    if (find)
                    {
                        find_doc = true;
                        begins_tmp.push_back(begins1->at(j)); // сохран€ем координаты цитат
                        ends_tmp.push_back(ends1->at(j));
                        find = false;
                    }
                }

                if (find_doc) // найдено хоть одно соотвествие
                {
                    /* перезаписываем */
                    begin1[p] = begins_tmp;
                    end1[p] = ends_tmp;
                    tmp.push_back(p);
                }
                else
                {
                    /* «атираем */
                    begin1.erase(p);
                    end1.erase(p);
                }

                i1++;
                i2++;
            }
            else if (p1->at(i1) < p2->at(i2))
            {
                i1++;
            }
            else
            {
                i2++;
            }
        }

        res.resize(tmp.size());
        std::copy(tmp.begin(), tmp.end(), res.begin());

        p1 = &res; // p1 указываем на результирующий список
    }

    return res.size() > 0;
}

#endif