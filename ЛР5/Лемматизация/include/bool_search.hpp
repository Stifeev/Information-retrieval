#ifndef BOOL_SEARCH_HPP
#define BOOL_SEARCH_HPP

#include "defs.hpp"
#include "token_parse.hpp"
#include "docs_parse.hpp"
#include "quote_search.hpp"

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

bool is_quote(wstring &str)
{
    return str[0] == L'\"' || str[0] == L'/';
}

bool is_term(wstring &str)
{
    return !is_operator(str) && !is_bracket(str) && !is_quote(str);
}

bool string_to_infix_request(const wstring &str, vector<wstring> &request,
                             path path2request_parser)
{
    int i, j, len;

    request.resize(0);

    path path2request_parser_dir = path2request_parser.parent_path().parent_path().parent_path();
    wstring parse_request = path2request_parser_dir / (wstring(L"io\\i.txt"));
    wstring parse_response = path2request_parser_dir / (wstring(L"io\\o.txt"));

    FILE *fp = _wfopen(parse_request.c_str(),
                       L"w");
    fwprintf(fp, L"%s", str.c_str());
    fclose(fp);

    wstring command = path2request_parser.wstring() + wstring(L" ") + parse_request + wstring(L" ") + parse_response;

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
    int i, j,
        dist;
    wstring token;

    prequest.resize(0);
    stack<wstring> st = {};

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
        answer.resize(answer.size() + p1.size() - i);
        std::copy(p1.begin() + i, p1.end(), answer.begin() + answer.size() + i - p1.size());
    }

    if (j < p2.size())
    {
        answer.resize(answer.size() + p2.size() - j);
        std::copy(p2.begin() + j, p2.end(), answer.begin() + answer.size() + j - p2.size());
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