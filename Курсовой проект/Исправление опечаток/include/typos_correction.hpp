#ifndef TYPOS_CORRECTION_HPP
#define TYPOS_CORRECTION_HPP

#include "defs.hpp"
#include "search.hpp"

double LevenshteinDistance(const wstring &s1, const wstring &s2, vector<double> &work)
{
    int m = s1.size() + 1,
        n = s2.size() + 1;

    work.resize(m * n);
    vector<double> &M = work;

#define ij2i(i, j) ((i) * n + (j))

    M[ij2i(s1.size(), s2.size())] = 0;

    int i, j;

    for (i = 1; i <= s1.size(); i++)
    {
        M[ij2i(i, 0)] = i;
    }

    for (j = 1; j <= s2.size(); j++)
    {
        M[ij2i(0, j)] = j;
    }

    for (i = 1; i <= s1.size(); i++)
    {
        for (j = 1; j <= s2.size(); j++)
        {
            M[ij2i(i, j)] = min3(M[ij2i(i - 1, j - 1)] + (s1[i - 1] == s2[j - 1] ? 0 : 1),
                                 M[ij2i(i - 1, j)] + 1,
                                 M[ij2i(i, j - 1)] + 1);
        }
    }

    return M[ij2i(s1.size(), s2.size())];

#undef ij2i
}

double DamerauLevenshteinDistance(const wstring &S, const wstring &T, vector<double> &work, dict<wchar, int> iwork,
                                  double deleteCost = 1, 
                                  double insertCost = 1, 
                                  double replaceCost = 1, 
                                  double transposeCost = 0.8)
{
    int M = S.size(),
        N = T.size();

    // Обработка крайних случаев
    if (S == L"")
        if (T == L"")
            return 0;
        else
            return N;
    else if (T == L"")
        return M;

    work.resize((M + 2) * (N + 2));
    double *D = work.data();

#define ij2i(i, j) ((i) * (N + 2) + (j))

    double INF = (M + N) * max4(deleteCost, insertCost, replaceCost, transposeCost);

    // База индукции
    D[ij2i(0, 0)] = INF;

    int i, j;
    for (i = 0; i <= M; i++)
    {
        D[ij2i(i + 1, 1)] = i * deleteCost;
        D[ij2i(i + 1, 0)] = INF;
        for (j = 0; j <= N; j++)
        {
            D[ij2i(1, j + 1)] = j * insertCost;
            D[ij2i(0, j + 1)] = INF;
        }
    }

    int diff = 0;
    dict<wchar, int> &lastPosition = iwork;
    lastPosition.clear();
   
    for (i = 0; i < S.size(); i++)
    {
        if (std::find(T.begin(), T.end(), S[i]) != T.end())
        {
            lastPosition[S[i]] = 0;
        }
    }

    int last,
        i_, j_;
    for (i = 1; i <= M; i++)
    {
        last = 0;
        for (j = 1; j <= N; j++)
        {
            i_ = lastPosition[T[j - 1]];
            j_ = last;
            if (S[i - 1] == T[j - 1])
            {
                D[ij2i(i + 1,j + 1)] = D[ij2i(i, j)];
                last = j;
            }
            else
            {
                D[ij2i(i + 1, j + 1)] = min3(D[ij2i(i, j)] + replaceCost, 
                                             D[ij2i(i + 1, j)] + insertCost, 
                                             D[ij2i(i, j + 1)] + deleteCost);
            }
            D[ij2i(i + 1, j + 1)] = min(D[ij2i(i + 1, j + 1)], 
                                        D[ij2i(i_, j_)] + (i - i_ - 1) * deleteCost + transposeCost + (j - j_ - 1) * insertCost);
        }
        lastPosition[S[i - 1]] = i;
    }

    return D[ij2i(M, N)];

#undef ij2i
}

void correct_typos(const vector<wstring> &request, vector<wstring> &correct_request, 
                   const vector<wstring> &lemma_request, int n_docs, dict<wstring, item> &terms,
                   const vector<pair<wstring, int>> &terms_list, double tetta = 0.5, int num_threads=4)
{
    int i, j, k, 
        terms_list_size,
        max_df;
    double min_dist, dist;

    terms_list_size = terms_list.size();

    struct item
    {
        int index;
        double dist;
        int df;
    };
    vector<item> min_terms(num_threads);
    wstring term;

    correct_request = vector<wstring>(request.size());

    for (i = 0; i < request.size(); i++) // цикл по терминам из запроса
    {
        if (
            is_term(request[i]) && 
            (in(lemma_request[i], terms) && terms[lemma_request[i]].df < 30 ||
             !in(lemma_request[i], terms))
            )
        {
            term = request[i];
           
            #pragma omp parallel num_threads(num_threads)                               \
                                 shared(terms_list, min_terms)                          \
                                 private(i, min_dist, dist, max_df)                     \
                                 firstprivate(terms_list_size, term, n_docs)  
            {
                int id = omp_get_thread_num(),
                    offset = ceil(terms_list_size / (double)omp_get_num_threads()),
                    n;
                vector<double> work(10);
                dict<wchar, int> iwork(10);

                n = min((id + 1) * offset, terms_list_size);

                min_dist = INFINITY;
                max_df = 0;

                for (i = id * offset; i < n; i++)
                {
                    if(term == terms_list[i].first) continue; // такой же термин не учитываем

                    dist = tetta * DamerauLevenshteinDistance(term, terms_list[i].first, work, iwork) + 
                           (1 - tetta) * (-log10(terms_list[i].second / (double)n_docs));

                    if (dist < min_dist)
                    {
                        min_terms[id] = { i, dist, terms_list[i].second };
                        min_dist = dist;
                        max_df = terms_list[i].second;
                    }
                    else if (dist == min_dist)
                    {
                        if (max_df < terms_list[i].second)
                        {
                            min_terms[id] = { i, dist, terms_list[i].second };
                            max_df = terms_list[i].second;
                        }
                    }
                }
            }

            min_dist = INFINITY;
            max_df = 0;
            k = -1;

            for (j = 0; j < min_terms.size(); j++)
            {
                if (min_terms[j].dist < min_dist)
                {
                    min_dist = min_terms[j].dist;
                    max_df = min_terms[j].df;
                    k = min_terms[j].index;
                }
                else if (min_terms[j].dist == min_dist)
                {
                    if (max_df < min_terms[j].df)
                    {
                        max_df = min_terms[j].df;
                        k = min_terms[j].index;
                    }
                }
            }

            if (min_dist != INFINITY && min_dist != 0)
            {
                correct_request[i] = terms_list[k].first;
            }
            else
            {
                correct_request[i] = request[i];
            }
        }
        else
        {
            correct_request[i] = request[i];
        }
    }
}

#endif