#ifndef TOKEN_PARSE_HPP
#define TOKEN_PARSE_HPP

#include <string>

using std::wstring;

typedef wstring (*wstr_to_wstr)(wstring &);

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

#endif