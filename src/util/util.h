#ifndef UTIL_H
#define UTIL_H
#if !(defined lint) && (defined RCSHEADERS)
static char util_rcsid[] = "$Header: util.h,v 1.3 93/03/02 01:11:16 drickel Exp $";
static char util_copyright[] = "Copyright (C) 1993 Mentor Graphics Corporation";
#endif


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#ifndef DECL
#define DECL extern
#endif

#define TRUE 1
#define FALSE 0
#define NIL 0
#define FAIL 0
#define SUCCESS 1

#define OPEN_FAIL	0
#define OPEN_FILE	1
#define OPEN_PIPE	2

void *u_malloc(int size);
void *u_realloc(void *p, int size);
void *u_calloc(int nelem, int elsize);
char *u_strsave(const char *str);
void u_free(void *p);
long u_currenttime(void);
char *u_timestring(long time);
char *u_findhome(const char *argv0);

#define ZTNEW(typ,cnt) ((typ *)u_calloc((cnt),sizeof(typ)))
#define TNEW(typ,cnt) ((typ *)u_malloc(((cnt)*sizeof(typ))))

struct _hashtable;
struct _hashentry;

void u_crash(const char *fmt, ...);
int u_closer(FILE *fp, int mode);
int u_openr(const char *path, FILE **fpp);
struct _hashentry *hashfind(struct _hashtable *ht, const char *name);
#endif
