#ifndef stralloc_h
#define stralloc_h

typedef struct stralloc
{
    char *s;
    int len; // s已使用的空间大小
    int a;   // s的空间大小
} stralloc;

typedef struct stralloc_list
{
    stralloc *sa;
    int len;
    int a;
} stralloc_list;

int stralloc_starts(stralloc *sa, char *s);
int stralloc_ready(register stralloc *x, register unsigned int n);
int stralloc_readyplus(register stralloc *x, register unsigned int n);
int stralloc_copyb(stralloc *sa, char *s, unsigned int n);
int stralloc_copys(stralloc *sa, char *s);
int stralloc_copy(stralloc *sa_dst, stralloc *sa_src);
int stralloc_append(stralloc *x, char *i);
int stralloc_catb(stralloc *sa, char *s, unsigned int n);
int stralloc_cats(stralloc *sa, char *s);
int stralloc_cat(stralloc *sa_dst, stralloc *sa_src);
void stralloc_free(stralloc *x);

int stralloc_list_readyplus(register stralloc_list *x, register unsigned int n);
int stralloc_list_append(register stralloc_list *x, register stralloc *i);

#endif /* stralloc_h */