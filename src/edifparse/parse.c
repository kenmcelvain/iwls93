#ifndef lint
static char rcsid[] = "$Header: parse.c,v 1.4 93/02/22 12:01:26 kenm Exp $";
static char copyright[] = "Copyright (C) 1993 Mentor Graphics Corporation";
#endif

#undef DECL
#include "util.h"
#include "token.h"
#include "parse.h"

extern int epk_ignore(void);
int ep_parse(void);

typedef struct _ep_ctx {
	char *pre;
	struct _ep_ctx *next;
} ep_ctx;

static ep_ctx *ctxstk = NIL;
static char keybuf_b[1024];
static char *keybuf;

#define HSIZE 128 /* must be power of 2 */
static ep_keyword *ep_keytab[HSIZE] = { 0 };

#define NSIZE 1024
static ep_name *ep_nametab[NSIZE] = { 0 };


void ep_perr(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "Parsing Error, line %d: ", ep_line);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	fflush(stderr);
	ep_ecnt++;
	return;
}

int ep_perr_ignore(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "Parsing Error, line %d: ", ep_line);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	fflush(stderr);
	ep_ecnt++;
	return(epk_ignore());
}

//
// Edif keywords will be prefixed by a context string so that
// keywords can have different actions in different contexts.
//
void ep_setctx(const char *pre)
{
	ep_ctx *ctx;
	ctx = TNEW(ep_ctx, 1);

	*keybuf = 0;
	ctx->pre = u_strsave(keybuf_b);
	ctx->next = ctxstk;
	ctxstk = ctx;
	strcpy(keybuf_b, pre);
	keybuf = keybuf_b + strlen(keybuf_b);
}

void ep_pushctx(const char *pre)
{
	ep_ctx *ctx;

	ctx = TNEW(ep_ctx, 1);
	*keybuf = 0;
	ctx->pre = u_strsave(keybuf_b);
	ctx->next = ctxstk;
	ctxstk = ctx;
	strcpy(keybuf, pre);
	keybuf += strlen(keybuf);
}

void ep_popctx(void)
{
	ep_ctx *ctx;

	ctx = ctxstk;
	if(!ctx) u_crash("ep_popctx: parsing context stack empty");
	ctxstk = ctx->next;
	strcpy(keybuf_b, ctx->pre);
	keybuf = keybuf_b + strlen(keybuf_b);
	u_free(ctx->pre);
	u_free(ctx);
}

/*
 * hash table routines for keyword lookup
 */
int myhash(const char *str, int tsize)
{
	register unsigned int sum, sum2;

	sum = sum2 = 0;
	while(*str) {
		sum += sum + sum + *str++ ;
		if(!*str) break;
		sum2 += *str++;
	}
	sum ^= sum2;
	sum &= tsize - 1;
	return(sum);
}

ep_keyword *ep_insertkey(const char *name)
{
	int h;
	ep_keyword *k;
	k = ZTNEW(ep_keyword, 1);
	k->name = u_strsave(name);
	h = myhash(name, HSIZE);
	k->hashnext = ep_keytab[h];
	ep_keytab[h] = k;
	return(k);
}

ep_keyword *ep_findkey(const char *name)
{
	ep_keyword *k;
	int h;

	strcpy(keybuf, name);
	h = myhash(keybuf_b, HSIZE);
	for(k = ep_keytab[h]; k; k=k->hashnext) {
		if(!strcmp(k->name, keybuf_b)) {
			while(k->alias) k = k->alias;
			return(k);
		}
	}
	return(NIL);
}

ep_keyword *ep_ignorekey(const char *name)
{
	ep_keyword *k;
	k = ep_insertkey(name);
	k->f = epk_ignore;
	k->operands = "c";
	k->data = NIL;
	k->alias = NIL;
	return(k);
}

ep_keyword *ep_addkey(const char *name, char *args, int (*f)())
//char *name;
//char *args;
//int (*f)();
{
	ep_keyword *k;
	k = ep_insertkey(name);
	k->f = f;
	k->operands = args;
	k->data = NIL;
	k->alias = NIL;
	return(k);
}

void ep_clearnametab(void)
{
	int h;
	ep_name *n, *nn;

	for(h = 0; h < NSIZE; h++) {
		nn = ep_nametab[h];
		ep_nametab[h] = NIL;
		while(nn) {
			n = nn;
			nn = n->hashnext;
			u_free((void *)n);
		}
	}
}

ep_name *ep_getname(const char *str)
{
	int h, len;
	ep_name *n;

	h = myhash(str, NSIZE);
	for(n = ep_nametab[h]; n; n=n->hashnext) {
		if(!strcmp(n->str, str)) return(n);
	}
	len = strlen(str);
	n = u_malloc(sizeof(ep_name) - NAMESTRSIZE + len + 1);
	n->flags = 0;
	n->hashnext = ep_nametab[h]; 
	ep_nametab[h] = n;
	strcpy(n->str, str);
	return(n);
}

int ep_getarg(const char *keystr)
{
	ep_gettoken();
	switch(ep_tkind) {
	case T_LPAR:
		return(ep_parse());
	case T_RPAR:
		ep_perr("%s: Not enough arguments", keystr);
		ep_tkind = T_NULL;
		return(FAIL);
	default:
		return(SUCCESS);
	}
}


int ep_getoptarg(const char *keystr)
{
	ep_gettoken();
	switch(ep_tkind) {
	case T_LPAR:
		return(ep_parse());
	case T_RPAR:
		ep_tkind = T_END;
		return(SUCCESS);
	default:
		return(SUCCESS);
	}
}

int ep_getnamearg(const char *keystr)
{
	ep_gettoken();
	if(ep_tkind == T_LPAR) ep_parse();
	if(ep_tkind != T_NAME) {
		if(ep_tkind == T_RPAR) {
			ep_perr("%s: Not enough arguments", keystr);
		} else {
			ep_perr("%s: expected string argument");
		}
		ep_tkind = T_NULL;
		return(FAIL);
	}
	return(SUCCESS);
}

#define EP_MAXARGS 10

int ep_parse(void) {
	long a[EP_MAXARGS]; /* argument collection array */
	int argpos, ap1, count;
	char *ad, *keystr, *cp;
	int ch;
	int status, extraargsok;
	int (*f)();
	ep_keyword *key;

	if(ep_tkind != T_LPAR) {
		ep_perr("Expected ("); /*)*/
		return(FAIL);
	}
	ep_gettoken();
	if(ep_tkind != T_NAME) {
		ep_perr("Expected keyword");
		return(FAIL);
	}
	ep_tkey = NIL;
	cp = ep_tbuf;
	while(1) {
		ch = *cp;
		if(!ch) break;
		if(isupper(ch)) *cp = tolower(ch);
		cp++;
	}
	key = ep_findkey(ep_tbuf);
	if(!key) {
		ep_perr("Unrecognized keyword %s", ep_tbuf);
		ep_ignorekey(keybuf_b);
		key = ep_findkey(ep_tbuf);
	}
	keystr = key->name;
	extraargsok = FALSE;
	for(argpos = 0, ad = key->operands; *ad; ad++, argpos = ap1) {
		ap1 = argpos + 1;
		switch(*ad) {
		case 'i': /* integer arg */
			if(FAIL == ep_getarg(keystr)) return(FAIL);
			if(ep_tkind != T_INT) {
				ep_perr("%s:  Expected integer argument in pos %d", keystr, ap1);
				return(epk_ignore());
			}
			a[argpos] = ep_tint;
			break;
		case 'f': /* float arg */
			if(FAIL == ep_getarg(keystr)) return(FAIL);
			if(ep_tkind != T_DOUBLE) {
				ep_perr("%s:  Expected number argument in pos %d", keystr, ap1);
				return(epk_ignore());
			}
			a[argpos] = (long)&ep_tdouble;
			break;
		case 'q': /* quoted string arg */
			if(FAIL == ep_getarg(keystr)) return(FAIL);
			if(ep_tkind != T_QSTR) {
				ep_perr("%s:  Expected quoted string argument in pos %d", keystr, ap1);
				return(epk_ignore());
			}
			a[argpos] = (long)u_strsave(ep_tbuf);
			break;
		case 'n': /* name arg */
			if(FAIL == ep_getarg(keystr)) return(FAIL);
			if(ep_tkind != T_NAME) {
				ep_perr("%s:  Expected name argument in pos %d", keystr, ap1);
				return(epk_ignore());
			}
			a[argpos] = (long)ep_tname;
			break;
		case 'c': /* do immediate call, with additional args waiting */
			extraargsok = TRUE;
			ap1 = argpos; /* don't move */
			break;
		case 'k':
			a[argpos] = (long)key->name;
			break;
		case 'd':
			if(FAIL == ep_getarg(keystr)) return(FAIL);
			if(ep_tkind != T_DIRECTION) {
				ep_perr("%s:  Expected direction argument in pos %d", keystr, ap1);
				return(epk_ignore());
			}
			a[argpos] = (long)ep_tint;
			break;
		case 'p': /* property */
			if(FAIL == ep_getarg(keystr)) return(FAIL);
			if(ep_tkind != T_PROPERTY) {
				ep_perr("%s:  Expected property argument in pos %d", keystr, ap1);
				return(epk_ignore());
			}
			a[argpos] = (long)ep_property;
			break;
		case 'C': /* cellref */
			if(FAIL == ep_getarg(keystr)) return(FAIL);
			if(ep_tkind != T_CREF) {
				ep_perr("%s:  Expected cell ref argument in pos %d", keystr, ap1);
				return(epk_ignore());
			}
			a[argpos] = (long)&ep_tcref;
			break;
		case 'V': /* viewref */
			if(FAIL == ep_getarg(keystr)) return(FAIL);
			if(ep_tkind != T_VREF) {
				ep_perr("%s:  Expected view ref argument in pos %d", keystr, ap1);
				return(epk_ignore());
			}
			a[argpos] = (long)&ep_tvref;
			break;
		case 'L': /* libref */
			if(FAIL == ep_getarg(keystr)) return(FAIL);
			if(ep_tkind != T_LREF) {
				ep_perr("%s:  Expected view ref argument in pos %d", keystr, ap1);
				return(epk_ignore());
			}
			a[argpos] = (long)&ep_tlref;
			break;
		default:
			u_crash("Unknown arg type %c for key %s\n",
				*ad, keystr);
		}
	}
	if(!extraargsok) {
		ep_gettoken();
		if(ep_tkind != T_RPAR) {
			ep_tokenpushed = TRUE;
			ep_perr("%s: Extra arguments", keystr);
			return(epk_ignore());
		}
	}
	f = key->f;
	/* now do the call */
	/* this is technically non portable */
	switch(argpos) {
	case 0:  status = (*f)(); break;
	case 1:  status = (*f)(a[0]); break;
	case 2:  status = (*f)(a[0],a[1]); break;
	case 3:  status = (*f)(a[0],a[1],a[2]); break;
	case 4:  status = (*f)(a[0],a[1],a[2],a[3]); break;
	case 5:  status = (*f)(a[0],a[1],a[2],a[3],a[4]); break;
	case 6:  status = (*f)(a[0],a[1],a[2],a[3],a[4],a[5]); break;
	case 7:  status = (*f)(a[0],a[1],a[2],a[3],a[4],a[5],a[6]); break;
	case 8:  status = (*f)(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]); break;
	case 9:  status = (*f)(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8]); break;
	case 10: status = (*f)(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9]); break;
	}
	if(status == FAIL) {
		ep_tkind = T_NULL;
	} else {
		ep_tkey = key;
	}
	return(status);
}

void ep_init(void) {
	ep_line = 1;
	ep_ecnt = 0;
	ep_ediflevel = 0;
	ctxstk = NIL;
	keybuf = keybuf_b;
	epk_init();

	ep_tlref.library = ep_tcref.library = ep_tvref.library = NIL;
	ep_tlref.cell = ep_tcref.cell = ep_tvref.cell = NIL;
	ep_tlref.view = ep_tcref.view = ep_tvref.view = NIL;
}

void ep_startparse(FILE *fp) {

	ep_init();
	ep_tokenpushed = FALSE;
	e_fp = fp;
	ep_gettoken();
	ep_parse();
}
