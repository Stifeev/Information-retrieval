#ifndef SEARCH_HPP
#define SEARCH_HPP

#include "defs.hpp"
#include "token_parse.hpp"
#include "docs_parse.hpp"

bool is_operator(const wstring &str)
{
    return str == L"&" || str == L"|" || str == L"!";
}

bool is_binoperator(const wstring &str)
{
    return is_operator(str) && str != L"!";
}

bool is_bracket(const wstring &str)
{
    return str == L"(" || str == L")";
}

bool is_quote(const wstring &str)
{
    return str[0] == L'\"' || str[0] == L'/';
}

bool is_term(const wstring &str)
{
    return !is_operator(str) && !is_bracket(str) && !is_quote(str) && str != L"!!";
}

bool string_to_infix_request(const wstring &str, vector<wstring> &request,
                             path path2request_parser, bool lemma=true)
{
    int len;

    request.resize(0);

    path path2request_parser_dir = path2request_parser.parent_path().parent_path().parent_path();
    wstring parse_request = path2request_parser_dir / (wstring(L"io\\i.txt"));
    wstring parse_response = path2request_parser_dir / (wstring(L"io\\o.txt"));

    FILE *fp = _wfopen(parse_request.c_str(),
                       L"w");
    fwprintf(fp, L"%s", str.c_str());
    fclose(fp);

    wstring command = path2request_parser.wstring() + wstring(L" ") + 
                      parse_request + wstring(L" ") + 
                      parse_response + wstring(L" ") +
                      (lemma ? wstring(L"-lemma") : wstring(L"-no_lemma"));

    wsystem(command.c_str());

    ERROR_HANDLE(fp = _wfopen(parse_response.c_str(), L"r"), return false, L"");

    wstring buf(400, L' ');
    while (fwscanf(fp, L"%s", buf.data()) != -1)
    {
        len = wcslen(buf.data());
        buf.resize(len);

        request.push_back(buf);
        buf.resize(400);
    }

    fclose(fp);
}

int priority(wstring &op)
{
    if (op == L"|")
    {
        return 0;
    }
    else if(op == L"&")
    {
         return 1;
    }
    else if (op == L"!")
    {
        return 2;
    }
    else
    {
        return -1;
    }
}

bool infix_request_to_postfix_request(const vector<wstring> &irequest, vector<wstring> &prequest)
{
    bool open = false;
    int i, j;
    wstring token;

    prequest = {};
    stack<wstring> st = {};

    if(irequest.size() == 0)
        return true;
    else
    {
        if (irequest[0] == L"!!") // случай нечёткого поиска
        {
            prequest = irequest;
            return true;
        }
    }

    for (i = 0; i < irequest.size(); i++)
    {
        token = irequest[i];

        if (is_term(token))
        {
            prequest.push_back(token);
            if(open) j++;
        }
        else if (token == L"!") // префиксная функция
        {
            st.push(token);
        }
        else if (token == L"(")
        {
            st.push(token);
        }
        else if (token == L"\"" && !open) // цитата
        {
            st.push(token);
            open = true; // признак того, что открывающая кавычка в стеке
            j = 0;
        }
        else if (token == L"\"" && open) // нашли закрывающую кавычку
        {
            open = false;
            
            if (st.empty())
            {
                return false; // неверная расстановка кавычек
            }
            else
            {
                st.pop();
                prequest.push_back(L"\"" + std::to_wstring(j)); // символ цитатности + размерность
            }
        }
        else if (token == L"/")
        {
            if(open) return false;
            i++;
            if(i >= irequest.size()) return false;
            prequest.back() += L" " + irequest[i]; // добавляем дистанцию
        }
        else if (token == L")")
        {
            while (!st.empty() && st.top() != L"(")
            {
                token = st.top();
                prequest.push_back(token);
                st.pop();
            }

            if (st.empty())
            {
                return false; // неверная расстановка скобок
            }
            else
            {
                st.pop();
            }
        }
        else if (is_binoperator(token))
        {
            while (!st.empty() && (st.top() == L"!" || priority(st.top()) > priority(token)))
            {
                prequest.push_back(st.top());
                st.pop();
            }

            st.push(token);
        }
    }

    while (!st.empty())
    {
        if (!is_operator(st.top())) 
        { 
            return false; 
        }
        else
        {
            prequest.push_back(st.top());
            st.pop();
        }
    }

    return true;
}

void intersect_postings(const vector<int> &p1, const vector<int> &p2,
                        vector<int> &answer)
{
    int i=0, j=0;

    answer.resize(0);

    while (i < p1.size() && j < p2.size())
    {
        if (p1[i] == p2[j])
        {
            answer.push_back(p1[i]);
            i++;
            j++;
        }
        else if (p1[i] < p2[j])
        {
            i++;
        }
        else
        {
            j++;
        }
    }
}

void union_postings(const vector<int> &p1, const vector<int> &p2,
                    vector<int> &answer)
{
    int i = 0, j = 0;

    answer.resize(0);

    while (i < p1.size() && j < p2.size())
    {
        if (p1[i] < p2[j])
        {
            answer.push_back(p1[i]);
            i++;
        }
        else if (p1[i] > p2[j])
        {
            answer.push_back(p2[j]);
            j++;
        }
        else
        {
            answer.push_back(p1[i]);
            i++;
            j++;
        }
    }

    if (i < p1.size())
    {
        int m = answer.size();
        answer.resize(m + p1.size() - i);
        std::copy(p1.begin() + i, p1.end(), answer.begin() + m);
    }

    if (j < p2.size())
    {
        int m = answer.size();
        answer.resize(m + p2.size() - j);
        std::copy(p2.begin() + j, p2.end(), answer.begin() + m);
    }
}

void complement_posting(const vector<int> &p,  
                        vector<int> &answer, uint n_docs)
{
    answer.resize(0);
    int i, j;

    for (i = 0; i < p.size(); i++)
    {
        if (i < p.size() - 1)
        {
            for (j = p[i] + 1; j < p[i + 1]; j++)
            {
                answer.push_back(j);
            }
        }
        else // i == p.size() - 1
        {
            for (j = p[i] + 1; j < n_docs; j++)
            {
                answer.push_back(j);
            }
        }
    }
}

void subsets(vector<int> &A, vector<vector<int>> &res) // находит все подмножества
{
    res = { vector<int>(0) };
    vector<vector<int>> tmp = {};
    int i, j, m;
    for (i = 0; i < A.size(); i++)
    {
        tmp.resize(res.size());
        for (j = 0; j < res.size(); j++) // перебор всех текущих элементов в res
        {
            tmp[j] = res[j];
            tmp[j].push_back(A[i]);
        }
        m = res.size();
        res.resize(m + m);
        std::copy(tmp.begin(), tmp.end(), res.begin() + m);
    }

    auto comp = [](const vector<int> &l, const vector<int> &r) -> bool
    {
        return l.size() > r.size();
    };

    std::sort(res.begin(), res.end(), comp);
}

int factorial(int a)
{
    int res = 1;
    int i;

    for (i = 2; i <= a; i++)
    {
        res *= i;
    }
    return res;
}

bool fuzzy_search(const vector<wstring> &fuzzy_terms, const vector<vector<int>> &fuzzy_postings,
                  vector<int> &res, int threshold = 50)
{
    int i, j, k;
    vector<int> work = {};
    bool find = false, find_doc = false;

    if (fuzzy_terms.size() == 0) // быстрый выход 1
    {
        res = {};
        return true;
    }

    if (fuzzy_terms.size() == 1) // быстрый выход 2
    {
        res = fuzzy_postings[0];
        return res.size() > 0;
    }

    vector<int> tmp = {}, tmp_inter = {}, tmp_union = {};
    res = {};
    int n_terms = fuzzy_terms.size();
    vector<int> A(n_terms);
    for (i = 0; i < n_terms; i++)
    {
        A[i] = i;
    }
    vector<vector<int>> subs = {};
    subsets(A, subs);
    int offset = 0,
        n_indicies;
    for (k = n_terms; k > 0; k--) // цикл по длинам подмножеств
    {
        n_indicies = factorial(n_terms) / (factorial(k) * factorial(n_terms - k));
        for (j = 0; j < n_indicies; j++) // цикл по подмножествам длины k
        {
            tmp_inter = fuzzy_postings[subs[offset + j][0]];
            for (i = 1; i < k; i++) // цикл по множеству длины k
            {
                intersect_postings(tmp_inter, fuzzy_postings[subs[offset + j][i]], tmp); // пересекаем постинги
                tmp_inter = tmp;
            }
            union_postings(tmp_union, tmp_inter, tmp); // добавляем к результату
            tmp_union = tmp;
        }

        if (tmp_union.size() >= threshold)
        {
            res = tmp_union;
            break;
        }

        offset += n_indicies;
    }

    return res.size() > 0;
}

bool close(int begin1, int end1,
           int begin2, int end2,
           int dist)
{
    if (end1 - 1 < begin2) // сначала идёт терм1, затем терм2
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
        res = {};
        return true;
    }

    if (quote_terms.size() == 1) // быстрый выход 2
    {
        res = quote_postings[0];
        return res.size() > 0;
    }

    if (dist < 0) dist = quote_terms.size() - 1;
    dist = max(dist, quote_terms.size() - 1);

    /* Перевод dist из расстояния в словах в расстояние в символах */
    int avg_space = 2,
        avg_word = 7;
    int dist_char = 0;
    for (i = 1; i < quote_terms.size(); i++)
    {
        dist_char += avg_space + quote_terms[i].size();
    }
    dist_char += (dist - quote_terms.size() + 1) * (avg_word + avg_space);
    dist_char -= (dist == quote_terms.size() - 1) ? quote_terms.back().size() : avg_word;

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
        tmp = {}; // результирующий список
        while (i1 < p1->size() && i2 < p2->size())
        {
            if (p1->at(i1) == p2->at(i2)) // в обоих документов есть термы
            {
                p = p1->at(i1); // идентификатор документа

                if (i == 1) // первая итерация
                {
                    begin1[p] = {};
                    end1[p] = {};
                    /* достаём координаты для первого терма */
                    get_term_coords(fp_postings, quote_terms[0], p, terms,
                                    begin1[p], end1[p],
                                    work);
                }

                begin2[p].clear();
                end2[p].clear();
                /* достаём координаты для второго терма */
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
                        begins_tmp.push_back(begins1->at(j)); // сохраняем координаты цитат
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
                    /* Затираем */
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

        res = tmp;
        p1 = &res; // p1 указываем на результирующий список
    }

    return res.size() > 0;
}

void get_response(const vector<wstring> &request, FILE *fp, const dict<wstring, item> &terms, uint n_docs,
                  vector<int> &response)
{
    stack<vector<int>> st = {};
    wstring token, str;
    vector<wstring> quote_terms = {};
    vector<vector<int>> quote_postings = {};
    vector<int> p1 = {},
                p2 = {}, 
                ans = {};
    int i, j,
        operands, dist;

    response = {};

    if (request.size() == 0)
    {
        return;
    }

    /* Проверка на нечёткий поиск */

    if (request[0] == L"!!")
    {
        vector<vector<int>> fuzzy_postings(request.size() - 1, vector<int>(0));
        vector<wstring> fuzzy_terms(request.size() - 1, L"");

        for (i = 1; i < request.size(); i++)
        {
            fuzzy_terms[i - 1] = request[i];
            find_posting_list(fuzzy_terms[i - 1], fuzzy_postings[i - 1], fp, terms);
        }

        fuzzy_search(fuzzy_terms, fuzzy_postings, response, 50);
        return;
    }

    for (i = 0; i < request.size(); i++)
    {
        token = request[i];
        if (is_term(token))
        {
            find_posting_list(token, p1, fp, terms);
            st.push(p1); // по задумке это копия
        }
        else if (token == L"!")
        {
            p1 = st.top();
            st.pop();

            complement_posting(p1, ans, n_docs);
            st.push(ans);
        }
        else if (token[0] == L'\"') // цитатный поиск
        {
            j = 1;
            while (token[j] != L' ' && j < token.size())
            {
                j++;
            }

            str = token.substr(1, j - 1);
            operands = _wtoi(str.c_str());

            dist = -1;
            if(j == token.size())
                dist = -1;
            else
            {
                str = token.substr(j + 1, token.size());
                dist = _wtoi(str.c_str());
            }

            quote_terms.resize(operands);
            quote_postings.resize(0);
            quote_postings.resize(operands, {});

            for (j = 0; j < operands; j++)
            {
                quote_terms[j] = request[i - operands + j];
                quote_postings[operands - j - 1] = st.top();
                st.pop();
            }

            quote_search(quote_terms, quote_postings, fp, terms, ans, dist);
            st.push(ans);
        }
        else // is_binoperator
        {
            p1 = st.top();
            st.pop();
            p2 = st.top();
            st.pop();

            if (token == L"&")
            {
                intersect_postings(p1, p2, ans);
            }
            else // |
            {
                union_postings(p1, p2, ans);
            }

            st.push(ans);
        }
    }

    if (!st.empty())
    {
        response.resize(st.top().size());
        std::copy(st.top().begin(), st.top().end(), response.begin());
    }
}

#endif