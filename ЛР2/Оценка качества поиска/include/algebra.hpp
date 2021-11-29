#ifndef ALGEBRA_HPP
#define ALGEBRA_HPP

#include "defs.hpp"

double sum(const double *a, int n)
{
    double res = 0;
    for (int i = 0; i < n; i++)
    {
        res += a[i];
    }
    return res;
}

double mean(const double *a, int n)
{
    double res = 0;
    for (int i = 0; i < n; i++)
    {
        res += a[i];
    }
    return n > 0 ? res / n : 0;
}

#endif 
