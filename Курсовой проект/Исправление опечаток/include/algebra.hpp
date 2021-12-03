#ifndef ALGEBRA_HPP
#define ALGEBRA_HPP

#include "defs.hpp"

template<typename T>
T sum(const T *a, int n)
{
    T res = 0;
    for (int i = 0; i < n; i++)
    {
        res += a[i];
    }
    return res;
}

template<typename T>
double mean(const T *a, int n)
{
    double res = 0;
    for (int i = 0; i < n; i++)
    {
        res += a[i];
    }
    return n > 0 ? res / n : 0;
}

#endif 
