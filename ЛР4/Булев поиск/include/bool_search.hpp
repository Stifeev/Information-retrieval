#ifndef BOOL_SEARCH_HPP
#define BOOL_SEARCH_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <stack>

#include "token_parse.hpp"

using std::wstring;
using std::queue;
using std::stack;
using std::vector;
using uint = unsigned int;
#define dict std::unordered_map

bool is_operator(wstring &str)
{
    return str == L"&" || str == L"|" || str == L"!";
}

bool is_binoperator(wstring &str)
{
    return is_operator(str) && str != L"!";
}

bool is_bracket(wstring &str)
{
    return str == L"(" || str == L")";
}

bool is_term(wstring &str)
{
    return !is_operator(str) && !is_bracket(str);
}

void string_to_infix_request(const wstring &str, vector<wstring> &request,
                             wstr_to_wstr parse)
{
    bool end = true;
    int i, j;
    wstring tmp;

    request.resize(0);

    j = 0; // номер термина
    for (i = 0; i < str.size(); i++)
    {
        if (iswalnum(str[i]) || (!end && str[i] == L'-' && iswalnum(str[i + 1]))) // если не пробел или дефис в середине токена
        {
            if(j >= request.size()) request.push_back(L""); // добавляем пустой термин

            request[j] += str[i]; // вставляем букву символа
            end = false;
        }
        else if (str[i] == L'&' && str[i + 1] == L'&')
        {
            if (!end) // предыдущий термин не закончился
            {
                request[j] = parse(request[j]);    // добавляем термин
                j++;
                end = true;
            }

            request.push_back(L"&");              // добавляем конъюнкцию
            j++;

            i++;
        }
        else if (str[i] == L'|' && str[i + 1] == L'|')
        {
            if (!end) // предыдущий термин не закончился
            {
                request[j] = parse(request[j]); // добавляем термин
                j++;
                end = true;
            }

            request.push_back(L"|");           // добавляем дизъюнкцию
            j++;

            i++;
        }
        else if (str[i] == L'!')
        {
            if (!end) // предыдущий термин не закончился
            {
                request[j] = parse(request.at(j)); // добавляем термин
                j++;

                end = true;
            }
            
            request.push_back(L"!");             // добавляем отрицание
            j++;
        }
        else if (str[i] == L'(')
        {
            if (!end) // предыдущий терм не закончился
            {
                request[j] = parse(request[j]);       // добавляем термин
                j++;

                end = true;
            }

            request.push_back(L"(");                  // добавляем скобку
            j++;
        }
        else if (str[i] == L')')
        {
            if (!end) // предыдущий терм не закончился
            {
                request[j] = parse(request[j]);       // добавляем термин
                j++;

                end = true;
            }

            request.push_back(L")");                  // добавляем скобку
            j++;
        }
        else if (!end) // найден разделитель и термин не закончился
        {
            request[j] = parse(request[j]);       // добавляем термин
            j++;

            end = true;
        }
    }

    if (!end) // обработка конца строки для последней итерации
    {
        request[j] = parse(request[j]);       // добавляем термин
    }

    /* вставляем конъюнкцию */

    bool term_start = false,
         op = false;
    for (i = 0; i < request.size() - 1; i++)
    {
        if (is_term(request[i]) && is_term(request[i+1])  ||
            is_term(request[i]) && request[i + 1] == L"!" ||
            is_term(request[i]) && request[i + 1] == L"(" ||
             request[i] == L")" && request[i+1] == L"("   ||
             request[i] == L")" && is_term(request[i+1])  ||
             request[i] == L")" && request[i+1] == L"!")
        {
            request.insert(request.begin() + i + 1, L"&");
            i++;
        }
       
    }
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
    int i;
    wstring token;

    prequest.resize(0);
    stack<wstring> st = {};

    for (i = 0; i < irequest.size(); i++)
    {
        token = irequest[i];

        if (is_term(token))
        {
            prequest.push_back(token);
        }
        else if (token == L"!") // префиксная функция
        {
            st.push(token);
        }
        else if (token == L"(")
        {
            st.push(token);
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

bool find_posting_list(const wstring &term, vector<int> &posting_list, FILE *fp,
                       const dict<wstring, int> &terms)
{
    auto iter = terms.find(term);

    if (iter == terms.end())
    {
        posting_list.resize(0);
        return false;
    }

    uint n_docs;
    fseek(fp, sizeof(uint) + iter->second, SEEK_SET);

    fread(&n_docs, sizeof(uint), 1, fp);
    posting_list.resize(n_docs);
    fread(posting_list.data(), sizeof(int), n_docs, fp);

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

void get_response(const vector<wstring> &request, FILE *fp, const dict<wstring, int> &terms, uint n_docs,
                  vector<int> &response)
{
    stack<vector<int>> st = {};
    wstring token;
    vector<int> p1 = {},
                p2 = {}, 
                ans = {};
    int i;
   
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