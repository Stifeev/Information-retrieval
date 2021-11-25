#ifndef TOKEN_PARSE_HPP
#define TOKEN_PARSE_HPP

#include "defs.hpp"

wstring lower_case(wstring &token)
{
    /* 
    * Понизить регистр 
    */
    int i;
    wstring term = L"";

    for (i = 0; i < token.size(); i++)
    {
        term += towlower(token[i]);
    }

    return term;
}

wstring upper_case(wstring &token)
{
    /*
    * Понизить регистр
    */
    int i;
    wstring term = L"";

    for (i = 0; i < token.size(); i++)
    {
        term += towupper(token[i]);
    }

    return term;
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

#endif