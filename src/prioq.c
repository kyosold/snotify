#include "prioq.h"
#include "alloc.h"

int prioq_readplus(register prioq *x, register unsigned int n)
{
    register unsigned int i;
    if (x->p)
    {
        i = x->size;
        n += x->len;
        if (n > i)
        {
            x->size = 100 + n + (n >> 3);
            if (alloc_re(&x->p, i * sizeof(struct prioq_elt), x->size * sizeof(struct prioq_elt)))
                return 1;
            x->size = i;
            return 0;
        }
        return 1;
    }
    x->len = 0;
    return !!(x->p = (struct prioq_elt *)alloc((x->size = n) * sizeof(struct prioq_elt)));
}

int prioq_insert(prioq *pq, struct prioq_elt *pe)
{
    int i, j;
    if (!prioq_readplus(pq, 1))
        return 0;
    j = pq->len++;
    while (j)
    {
        i = (j - 1) / 2;
        if (pq->p[i].dt <= pe->dt)
            break;
        pq->p[j] = pq->p[i];
        j = i;
    }
    pq->p[j] = *pe;
    return 1;
}

int prioq_min(prioq *pq, struct prioq_elt *pe)
{
    if (!pq->p)
        return 0;
    if (!pq->len)
        return 0;
    *pe = pq->p[0];
    return 1;
}

void prioq_del_min(prioq *pq)
{
    int i, j, n;
    if (!pq->p)
        return;
    n = pq->len;
    if (!n)
        return;
    i = 0;
    --n;
    for (;;)
    {
        j = i + i + 2;
        if (j > n)
            break;
        if (pq->p[j - 1].dt <= pq->p[j].dt)
            --j;
        if (pq->p[n].dt <= pq->p[j].dt)
            break;
        pq->p[i] = pq->p[j];
        i = j;
    }
    pq->p[i] = pq->p[n];
    pq->len = n;
}
