#ifndef PRIOQ_H
#define PRIOQ_H

typedef struct prioq_elt
{
    long dt;
    unsigned long id;
} prioq_elt_t;

typedef struct prioq
{
    prioq_elt_t *p;
    unsigned int len;
    unsigned int size;
} prioq, prioq_t;

int prioq_insert(prioq *pq, struct prioq_elt *pe);

int prioq_min(prioq *pq, struct prioq_elt *pe);

void prioq_del_min(prioq *pq);

#endif