#ifndef NETS_H
#define NETS_H

#if !(defined lint) && (defined RCSHEADERS)
static char nets_rcsid[] = "$Header: nets.h,v 1.2 93/02/22 12:04:55 kenm Exp $";
static char nets_copyright[] = "Copyright (C) 1993 Mentor Graphics Corporation";
#endif

#include "util.h"
#include "hash.h"
#include "eprop.h"

/*
 * data is an opaque pointer used by applications borrowing
 * this data structure.
 */
typedef struct _library {
	hashentry h;
	hashtable cellhash;
	int external;
	property *proplist;
	char *data;
} library;

typedef struct _cell {
	hashentry h;
	hashtable viewhash;
	library *lp;
	property *proplist;
	char *data;
} cell;

typedef struct _netlist {
	hashtable nethash;
	hashtable insthash;
	hashtable porthash;
	property *proplist;
} netlist;

typedef struct _view {
	hashentry h;
	char viewtype;
#define VIEW_NETLIST 0
#define VIEW_PORT 1
	char primtype;
#define PRIM_NOTPRIM	0
#define PRIM_TRUE	1	/* out */
#define PRIM_FALSE	2	/* out */
#define PRIM_DC		3	/* out */
#define PRIM_AND	4	/* out, in0, in1 ... */
#define PRIM_OR		5	/* out, in0, in1 ... */
#define PRIM_XOR	6	/* out, in0, in1 ... */
#define PRIM_INV	7	/* out, in */
#define PRIM_BUF	8	/* out, in */
#define PRIM_MUX2	9	/* out, in0, in1, sel */
#define PRIM_TRI	10	/* out, in, en */
#define PRIM_LATCH	11	/* out, in, clk */
#define PRIM_LATCHR	12	/* out, in, clk, reset */
#define PRIM_LATCHS	13	/* out, in, clk, set */
#define PRIM_LATCHSR	14	/* out, in, clk, set, reset */
#define PRIM_DFF	15	/* out, in, clk */
#define PRIM_DFFR	16	/* out, in, clk, reset */
#define PRIM_DFFS	17	/* out, in, clk, set */
#define PRIM_DFFSR	18	/* out, in, clk, set, reset */

	cell *cellp;
	union {
		netlist nl;
		int portdir;
	} u;
	property *proplist;
	char *data;
} view;

typedef struct _conn {
	struct _net *np;	/* owning net */
	struct _instance *ip;	/* owning instance */
	struct _instance *port;	/* port of thing the ip is an instance of */
	struct _conn *inext;	/* next portinstance on instance */
	struct _conn *nnext;	/* next conn on net */
	short flags;
#define C_I 1
#define C_O 2
	union {
		short simval;
	} u;
	property *proplist;
	char *data;
} conn;

typedef struct _net {
	hashentry h;
	conn *conns;
	view *owner;
	union {
		struct _net *rep;
		int simval;
	} u;
	property *proplist;
	char *data;
} net;

typedef struct _instance {
	hashentry h;
	view *instof;
	view *owner;
	conn *ports;
	union {
		struct _instance *next;
		struct {
			short val;
			short init;
		} state;
	} u;
	property *proplist;
	char *data;
} instance;

#define ISPORT(ip) (ip->instof->viewtype == VIEW_PORT)

DECL hashtable libhash;
DECL view inport, outport, biport;
DECL instance inportinst, outportinst, biportinst;

extern library *findlibrary();
extern cell *findcell();
extern view *findview();
extern void hookup();
extern void unhook();
extern net *addnet();
extern instance *addinstance();
extern void mergenets();
extern int isprimitive();
extern void deleteinstance();

void netlist_init(void);
void setprimtype(view *vp);
void fixnc(view *vp);
void flatten(view *vp, int (*testfunc)());
void dissolve(instance *tip);
int hashdelete_f(hashtable *ht, hashentry *nhe);
void ep_clearnametab(void);

void writeblif(FILE *fp, view *vp);
void writeslif(FILE *fp, view *vp);
void writeedif(FILE *fp, view *topvp);

#endif
