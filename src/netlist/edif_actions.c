#ifndef lint
static char rcsid[] = "$Header: edif_actions.c,v 1.2 93/02/22 12:04:48 kenm Exp $";
static char copyright[] = "Copyright (C) 1993 Mentor Graphics Corporation";
#endif

#include "nets.h"

int ex_verbose = FALSE;

static library *curlib = NIL;
static cell *curcell = NIL;
static view *curview = NIL;
static int viewredef = FALSE;
static net *curnet = NIL;

static void ex_dprintf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if(ex_verbose) {
		fprintf(stderr, "DBG: ");
		vfprintf(stderr, fmt, args);
		fflush(stderr);
	}
	va_end(args);
}

int ex_startlibrary(const char *libtype, const char *libname)
{
	library *lp;
	ex_dprintf("Starting library %s(%s)\n", libname, libtype);
	lp = (library *)hashfind(&libhash, libname);
	if(!lp) {
		lp = ZTNEW(library, 1);
		lp->h.name = u_strsave(libname);
		hashinit(&lp->cellhash);
		hashinsert(&libhash, lp);
		lp->external = !strcasecmp(libtype, "external");
	}
	curlib = lp;
	return(SUCCESS);
}

int ex_endlibrary(property *plist)
{
	property *p;
	ex_dprintf("Ending library %s\n", curlib->h.name);
	while(plist) {
		p = plist;
		plist = p->next;
		p->next = curlib->proplist;
		curlib->proplist = p;
	}
	curlib = NIL;
	return(SUCCESS);
}

int ex_startcell(const char *cellname)
{
	cell *cellp;
	ex_dprintf("Starting cell %s\n", cellname);
	cellp = (cell *)hashfind(&curlib->cellhash, cellname);
	if(!cellp) {
		cellp = ZTNEW(cell, 1);
		cellp->h.name = u_strsave(cellname);
		cellp->lp = curlib;
		hashinit(&cellp->viewhash);
		hashinsert(&curlib->cellhash, cellp);
	}
	curcell = cellp;
	return(SUCCESS);
}

int ex_endcell(property *plist)
{
	property *p;
	ex_dprintf("Ending cell %s\n", curcell->h.name);
	while(plist) {
		p = plist;
		plist = p->next;
		p->next = curcell->proplist;
		curcell->proplist = p;
	}
	curcell = NIL;
	return(SUCCESS);
}

int ex_design(const char *designname, const char *libname, const char *cellname)
{
	ex_dprintf("design %s (cellref %s (libraryref %s))\n",
		designname, libname, cellname);
	return(SUCCESS);
}

int ex_startview(const char *viewname)
{
	view *vp;
	ex_dprintf("startview %s\n", viewname);
	vp = (view *)hashfind(&curcell->viewhash, viewname);
	if(!vp) {
		vp = ZTNEW(view, 1);
		vp->h.name = u_strsave(viewname);
		hashinsert(&curcell->viewhash, vp);
		hashinit(&vp->u.nl.porthash);
		hashinit(&vp->u.nl.insthash);
		hashinit(&vp->u.nl.nethash);
		vp->cellp = curcell;
		vp->proplist = NIL;
		setprimtype(vp);
		viewredef = FALSE;
	} else {
		viewredef = TRUE;
	}
	curview = vp;
	return(SUCCESS);
}

int ex_endview(property *plist)
{
	property *p;
	ex_dprintf("endview %s\n", curview->h.name);
	while(plist) {
		p = plist;
		plist = p->next;
		p->next = curview->proplist;
		curview->proplist = p;
	}
	fixnc(curview);
	curview = NIL;
	return(SUCCESS);
}

int ex_port(const char *name, int dir, property *plist)
{
	conn *ptp;
	instance *ip;
	instance *ptpip;

	ex_dprintf("port %s %c\n", name, dir);
	if(viewredef) {
		if(hashfind(&curview->u.nl.porthash, name)) return(SUCCESS);
		fprintf(stderr, "Mismatched port names in redef of %s:%s.%s\n",
			curlib->h.name, curcell->h.name, curview->h.name);
		return(FAIL);
	}
	ip = ZTNEW(instance, 1);
	ip->h.name = u_strsave(name);
	ip->proplist = plist;
	switch(dir) {
	case 'i':
		ip->instof = &inport;
		ptpip = &outportinst;
		break;
	case 'o':
		ip->instof = &outport;
		ptpip = &inportinst;
		break;
	case 'b':
		ip->instof = &biport;
		ptpip = &biportinst;
		break;
	}
	ip->owner = curview;

	ip->ports = ptp = ZTNEW(conn, 1);
	ptp->np = NIL;
	ptp->ip = ip;
	ptp->port = ptpip;
	ptp->inext = NIL;
	ptp->nnext = NIL;

	hashinsert(&curview->u.nl.porthash, ip);
	return(SUCCESS);
}

int ex_instance(const char *name, const char *libname, const char *cellname, const char *viewname, property *plist)
{
	instance *ip, *pip;
	view *vp;
	hashtable *ph;
	conn *ptp, **tptp;
	int hidx;

	ex_dprintf("instance %s of %s:%s.%s\n", name, libname, cellname, viewname);
	if(hashfind(&curview->u.nl.insthash, name)) {
		fprintf(stderr, "Duplicate instance name %s in view %s:%s.%s\n",
			name, curlib->h.name, curcell->h.name, curview->h.name);
		return(FAIL);
	}
	vp = findview(findcell(findlibrary(libname), cellname), viewname);
	if(!vp) {
		fprintf(stderr, "Instance %s of nonexistant view %s:%s.%s\n",
			name, libname, cellname, viewname);
		return(FAIL);
	}
	ip = ZTNEW(instance, 1);
	ip->h.name = u_strsave(name);
	ip->instof = vp;
	ip->owner = curview;
	ip->ports = NIL;
	ip->proplist = plist;

	ph = &vp->u.nl.porthash;
	tptp = &ip->ports;
	foreachentry(ph, hidx, instance *, pip) {
		ptp = ZTNEW(conn, 1);
		ptp->np = NIL;
		ptp->ip = ip;
		ptp->port = pip;
		ptp->inext = NIL;
		ptp->nnext = NIL;
		*tptp = ptp;
		tptp = &ptp->inext;
	}
	hashinsert(&curview->u.nl.insthash, ip);

	return(SUCCESS);
}

int ex_startnet(const char *name)
{
	net *np;
	ex_dprintf("net %s\n", name);
	if(hashfind(&curview->u.nl.nethash, name)) {
		fprintf(stderr, "Duplicate net name %s in view %s:%s.%s\n",
			name, curlib->h.name, curcell->h.name, curview->h.name);
		return(FAIL);
	}
	np = ZTNEW(net, 1);
	np->h.name = u_strsave(name);
	np->conns = NIL;
	np->owner = curview;
	hashinsert(&curview->u.nl.nethash, np);
	curnet = np;
	return(SUCCESS);
}

int ex_endnet(property *plist)
{
	ex_dprintf("endnet %s\n", curnet->h.name);
	curnet->proplist = plist;
	curnet = NIL;
	return(SUCCESS);
}

int ex_join(const char *pname, const char *iname)
{
	instance *ip, *pip;
	conn *ptp;

	if(!curnet) return(SUCCESS);
	ex_dprintf(" join to %s.%s\n", iname ? iname : "$", pname);
	if(iname) {
		ip = (instance *)hashfind(&curview->u.nl.insthash, iname);
		if(!ip) {
			fprintf(stderr, "join for net %s to %s.%s failed - no such instance\n",
				curnet->h.name, iname, pname);
			return(FAIL);
		}
		for(ptp = ip->ports; ptp; ptp=ptp->inext) {
			if(!strcmp(ptp->port->h.name, pname)) break;
		}
		if(!ptp) {
			fprintf(stderr, "join for net %s to %s.%s failed - no such port\n",
				curnet->h.name, iname, pname);
			return(FAIL);
		}
	} else {
		ip = (instance *)hashfind(&curview->u.nl.porthash, pname);
		if(!ip) {
			fprintf(stderr, "join for net %s to $.%s failed\n",
				curnet->h.name, pname);
			return(FAIL);
		}
		ptp = ip->ports;
	}
	if(ptp->np) {
		fprintf(stderr, "join for net %s to %s.%s failed - already connected\n",
			curnet->h.name, iname, pname);
		
	}
	hookup(ptp, curnet);
	return(SUCCESS);
}
