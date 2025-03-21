#ifndef lint
static char rcsid[] = "$Header: wr_edif.c,v 1.2 93/02/22 12:29:56 kenm Exp $";
static char copyright[] = "Copyright (C) 1993 Mentor Graphics Corporation";
#endif

#include <stdio.h>
#include "nets.h"

typedef struct _viewlist {
	struct _viewlist *next;
	view *vp;
} viewlist;

typedef struct _celllist {
	struct _celllist *next;
	struct _viewlist *views;
	cell *cellp;
} celllist;

typedef struct _liblist {
	struct _liblist *next;
	struct _celllist *cells;
	library *lp;
} liblist;

static liblist *libs;

static viewlist *findviewref(celllist *cc, view *vp)
{
	viewlist *vv;

	for(vv = cc->views; vv; vv=vv->next) {
		if(vv->vp == vp) break;
	}
	return(vv);
}

static celllist *findcellref(liblist *ll, view *vp)
{
	celllist *cc;

	for(cc = ll->cells; cc; cc=cc->next) {
		if(cc->cellp == vp->cellp) break;
	}
	return(cc);
}

static liblist *findlibref(view *vp)
{
	liblist *ll;

	for(ll = libs; ll; ll=ll->next) {
		if(ll->lp == vp->cellp->lp) break;
	}
	return(ll);
}

static void collectviews(view *vp)
{
	liblist *ll;
	celllist *cc;
	viewlist *vv;

	ll = findlibref(vp);
	if(ll) cc = findcellref(ll, vp);
	else {
		ll = TNEW(liblist, 1);
		ll->lp = vp->cellp->lp;
		ll->next = libs;
		ll->cells = NIL;
		libs = ll;
		cc = NIL;
	}
	if(cc) vv = findviewref(cc, vp);
	else {
		cc = TNEW(celllist, 1);
		cc->cellp = vp->cellp;
		cc->next = ll->cells;
		ll->cells = cc;
		cc->views = NIL;
		vv = NIL;
	}
	if(vv) return;
	vv = TNEW(viewlist, 1);
	vv->vp = vp;
	vv->next = cc->views;
	cc->views = vv;

	/* now we have to scan the instances in the netlist */

	{
		hashtable *iht;
		int hidx;
		instance *ip;

		iht = &vp->u.nl.insthash;
		foreachentry(iht, hidx, instance *, ip) {
			collectviews(ip->instof);
		}
	}
}

static void printproplist(FILE *fp, property *p, const char *spaces)
{
	int i;
	double f;
	for(; p; p=p->next) {
		fprintf(fp, "%s(property %s ", spaces, p->name);
		switch(p->kind) {
		case 'i': fprintf(fp, "(integer %d))\n", p->u.i); break;
		case 's': fprintf(fp, "(string \"%s\"))\n", p->u.s); break;
		case 'f':
			/* This isn't right */
			f = p->u.f;
			f *= 10000.0;
			if(f < 0) f -= .5;
			else f += .5;
			i = (int) f;
			fprintf(fp, "(number (e %d %d)))\n", i, -4);
			break;
		}
	}
}

void writeedif(FILE *fp, view *topvp)
{
	liblist *ll;
	library *lp;
	celllist *cc;
	cell *cellp;
	viewlist *vv;
	view *vp;
	int pidx, iidx, nidx;
	instance *pip, *ip;
	net *np;
	conn *cnp;
	char *ds;
	property *p;
	hashtable *pht, *iht, *nht;

	libs = NIL;
	collectviews(topvp);

fputs("\
(edif netlist\n\
  (edifVersion 2 0 0)\n\
  (edifLevel 0)\n\
  (keywordMap (keywordLevel 0))\n\
  (status\n\
    (written\n\
      (timeStamp 1993 1 25 0 53 46)\n\
      (author \"iwls93\")\n\
      (program \"e2fmt\")\n\
    )\n\
  )\n\
", fp);
	for(ll = libs; ll; ll=ll->next) {
		lp = ll->lp;
		fprintf(fp, "  (%s %s\n",
			lp->external ? "external" : "library", lp->h.name);
		fprintf(fp, "    (edifLevel 0)\n");
		fprintf(fp, "    (technology\n");
		fprintf(fp, "      (numberDefinition)\n");
		fprintf(fp, "      (simulationInfo\n");
		fprintf(fp, "        (logicValue H (booleanMap (true)))\n");
		fprintf(fp, "        (logicValue L (booleanMap (false)))\n");
		fprintf(fp, "      )\n");
		fprintf(fp, "    )\n");
		for(cc = ll->cells; cc; cc = cc->next) {
			cellp = cc->cellp;
			fprintf(fp, "    (cell %s\n", cellp->h.name);
			fprintf(fp, "      (cellType generic)\n");
			for(vv = cc->views; vv; vv=vv->next) {
				vp = vv->vp;
				fprintf(fp, "      (view %s\n", vp->h.name);
				fprintf(fp, "        (interface\n");
				pht = &vp->u.nl.porthash;
				foreachentry(pht, pidx, instance *, pip) {
					switch(pip->instof->u.portdir) {
					case 'i': ds = "input"; break;
					case 'o': ds = "output"; break;
					default:  ds = "inout"; break;
					}
					if(pip->proplist) {
						fprintf(fp, "          (port %s (direction %s)\n",
							pip->h.name, ds);
						printproplist(fp, pip->proplist,
						    "                  ");
						fprintf(fp, "          )\n");
					} else {
						fprintf(fp, "          (port %s (direction %s))\n", pip->h.name, ds);
					}
				}
				fprintf(fp, "        )\n");
				if(vp->cellp->lp->external) {
					fprintf(fp, "      )\n");
					continue;
				}
				fprintf(fp, "        (contents\n");
				iht = &vp->u.nl.insthash;
				foreachentry(iht, iidx, instance *, ip) {
					fprintf(fp, "          (instance %s\n", ip->h.name);
					fprintf(fp, "            (viewRef %s (cellRef %s (libraryRef %s)))\n", ip->h.name, ip->instof->h.name, ip->instof->cellp->h.name, ip->instof->cellp->lp->h.name);
					printproplist(fp, ip->proplist,
					    "                    ");
					fprintf(fp, "          )\n");
				}
				nht = &vp->u.nl.nethash;
				foreachentry(nht, nidx, net *, np) {
					fprintf(fp, "          (net %s\n", np->h.name);
					fprintf(fp, "            (joined\n");
					for(cnp = np->conns; cnp; cnp=cnp->nnext) {
						fprintf(fp, "              (portRef ");
						if(cnp->ip->instof->viewtype == VIEW_PORT) {
							fprintf(fp, "%s)\n", cnp->ip->h.name);
						} else {
							fprintf(fp, "%s (instanceRef %s))\n",
							   cnp->port->h.name, cnp->ip->h.name);
						}
					}
					fprintf(fp, "            )\n");
					printproplist(fp, np->proplist,
					   "              ");
					fprintf(fp, "          )\n");
				}
				fprintf(fp, "        )\n");
				printproplist(fp, vp->proplist,"        ");
				fprintf(fp, "      )\n");
			}
			printproplist(fp, cellp->proplist,"      ");
			fprintf(fp, "    )\n");
		}
		printproplist(fp, lp->proplist,"    ");
		fprintf(fp, "  )\n");
	}
	fprintf(fp, ")\n");
}
