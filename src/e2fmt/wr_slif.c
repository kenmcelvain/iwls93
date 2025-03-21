#ifndef lint
static char rcsid[] = "$Header: wr_slif.c,v 1.4 93/05/16 18:28:55 kenm Exp $";
static char copyright[] = "Copyright (C) 1993 Mentor Graphics Corporation";
#endif

#include <stdio.h>
#include "nets.h"

int linepos = 0;

void fold(FILE *fp)
{
	if(ftell(fp) - linepos > 70) {
		fputs("\\\n    ", fp);
		linepos = ftell(fp);
	}
}

char *pinnetname(instance *ip, const char *pname)
{
	conn *ptp;
	for(ptp = ip->ports; ptp; ptp=ptp->inext) {
		if(!strcmp(ptp->port->h.name, pname)) {
			return(ptp->np->h.name);
		}
	}
	return("UNKNOWN_PIN");
}

void printports(FILE *fp, hashtable *ht, const char *cmd, int ptype)
{
	int hidx, cnt;
	instance *ip;

	cnt = 0;
	foreachentry(ht, hidx, instance *, ip) {
		if(ip->instof->u.portdir == ptype) cnt++;
	}
	if(!cnt) return;
	linepos = ftell(fp);
	fprintf(fp,"%s ", cmd);
	foreachentry(ht, hidx, instance *, ip) {
		if(ip->instof->u.portdir == ptype) {
			fold(fp);
			fprintf(fp, " %s", ip->h.name);
		}
	}
	fputs(";\n", fp);
}

void writeslif(FILE *fp, view *vp)
{
	netlist *nl;
	int hidx,  i, j, xsize, t, first;
	instance *pip, *ip;
	hashtable *iht;
	conn *ptp;
	int cnt;
	char *nname;
	char pbuf[32];

	nl = &vp->u.nl;
	fprintf(fp, "###########################################\n");
	fprintf(fp, ".model %s\n", vp->cellp->h.name);
	printports(fp, &nl->porthash, ".inputs", 'i');
	printports(fp, &nl->porthash, ".outputs", 'o');
	printports(fp, &nl->porthash, ".inouts", 'b');
	iht = &nl->porthash;
	foreachentry(iht, hidx, instance *, ip) {
		nname = ip->ports->np->h.name;
		if(!strcmp(nname, ip->h.name)) continue;
		switch(ip->instof->u.portdir) {
		case 'i':
			fprintf(fp, "   %s = %s;\n", nname, ip->h.name);
			break;
		case 'o':
			fprintf(fp, "   %s = %s;\n", ip->h.name, nname);
			break;
		default:
			fprintf(fp, ".net %s %s;\n", ip->h.name, nname);
			break;
		}
	}
	iht = &nl->insthash;
	foreachentry(iht, hidx, instance *, ip) {
		fprintf(fp, "# instance %s\n", ip->h.name);
		if(ip->instof->primtype == PRIM_NOTPRIM) {
			linepos = ftell(fp);
			fprintf(fp, ".call %s %s (", ip->h.name,
				ip->instof->cellp->h.name);
			first = TRUE;
			for(ptp = ip->ports; ptp; ptp=ptp->inext) {
				if(ptp->port->instof->u.portdir == 'i') {
				    if(!first) fputc(',', fp);
				    first = FALSE;
				    fold(fp);
				    fputs(ptp->port->h.name, fp);
				}
			}
			fputs("; ", fp);
			first = TRUE;
			for(ptp = ip->ports; ptp; ptp=ptp->inext) {
				if(ptp->port->instof->u.portdir == 'b') {
				    if(!first) fputc(',', fp);
				    first = FALSE;
				    fold(fp);
				    fputs(ptp->port->h.name, fp);
				}
			}
			fputs("; ", fp);
			first = TRUE;
			for(ptp = ip->ports; ptp; ptp=ptp->inext) {
				if(ptp->port->instof->u.portdir == 'o') {
				    if(!first) fputc(',', fp);
				    first = FALSE;
				    fold(fp);
				    fputs(ptp->port->h.name, fp);
				}
			}
			fputs(");\n", fp);
			continue;
		}
		fprintf(fp, "  %s = ", pinnetname(ip, "out"));
		for(cnt = -1, ptp = ip->ports; ptp; ptp=ptp->inext) cnt++;
		switch(ip->instof->primtype) {
		case PRIM_TRUE:
			fputs(" 1;\n", fp);
			break;
		case PRIM_FALSE:
			fputs(" 0;\n", fp);
			break;
		case PRIM_AND:
			for(i = 0; i < cnt; i++) {
				if(i) fputs(" * ", fp);
				sprintf(pbuf, "in%d", i);
				fputs(pinnetname(ip,pbuf), fp);
			}
			fputs(";\n", fp);
			break;
		case PRIM_OR:
			for(i = 0; i < cnt; i++) {
				if(i) fputs(" + ", fp);
				sprintf(pbuf, "in%d", i);
				fputs(pinnetname(ip,pbuf), fp);
			}
			fputs(";\n", fp);
			break;
		case PRIM_INV:
			fprintf(fp, "%s';\n", pinnetname(ip,"in"));
			break;
		case PRIM_BUF:
			fprintf(fp, "%s;\n", pinnetname(ip,"in"));
			break;
		case PRIM_XOR:
			xsize = 1 << cnt;
			first = TRUE;
			for(i = 0; i < xsize; i++) {
				j = 0; t = i;
				while(t) { t = t & (t - 1); j++; }
				if(j & 1) {
					if(!first) {
						fputs(" +\n    ", fp);
					}
					first = FALSE;
					for(j = 0; j < cnt; j++) {
						if(j) fputs(" * ", fp);
						sprintf(pbuf, "in%d", j);
						fputs(pinnetname(ip, pbuf), fp);
						if(!(i & (1 << j))) {
							fputc('\'', fp);
						}
					}
				}
			}
			fputs(";\n", fp);
			break;
		case PRIM_MUX2:
			fprintf(fp, "%s * %s + %s' * %s;\n",
				pinnetname(ip, "sel"), pinnetname(ip, "in0"),
				pinnetname(ip, "sel"), pinnetname(ip, "in1"));
			break;
		case PRIM_TRI:
			fprintf(fp, "@ T(%s, %s);\n",
				pinnetname(ip, "in"), pinnetname(ip, "en"));
			break;
		case PRIM_DFF:
			fprintf(fp, "@ D(%s, %s);\n",
				pinnetname(ip, "in"), pinnetname(ip, "clk"));
			break;
		case PRIM_DFFS:
			fprintf(fp, "@ DS(%s, %s, %s);\n",
				pinnetname(ip, "in"), pinnetname(ip, "clk"),
				pinnetname(ip, "set"));
			break;
		case PRIM_DFFR:
			fprintf(fp, "@ DR(%s, %s, %s);\n",
				pinnetname(ip, "in"), pinnetname(ip, "clk"),
				pinnetname(ip, "reset"));
			break;
		case PRIM_DFFSR:
			fprintf(fp, "@ DSR(%s, %s, %s, %s);\n",
				pinnetname(ip, "in"), pinnetname(ip, "clk"),
				pinnetname(ip, "set"), pinnetname(ip, "reset"));
			break;
		case PRIM_LATCH:
			fprintf(fp, "@ L(%s, %s);\n",
				pinnetname(ip, "in"), pinnetname(ip, "clk"));
			break;
		case PRIM_LATCHS:
			fprintf(fp, "@ LS(%s, %s, %s);\n",
				pinnetname(ip, "in"), pinnetname(ip, "clk"),
				pinnetname(ip, "set"));
			break;
		case PRIM_LATCHR:
			fprintf(fp, "@ LR(%s, %s, %s);\n",
				pinnetname(ip, "in"), pinnetname(ip, "clk"),
				pinnetname(ip, "reset"));
			break;
		case PRIM_LATCHSR:
			fprintf(fp, "@ LSR(%s, %s, %s, %s);\n",
				pinnetname(ip, "in"), pinnetname(ip, "clk"),
				pinnetname(ip, "set"), pinnetname(ip, "reset"));
			break;
		default:
			u_crash("Unknown primitive %s", ip->instof->cellp->h.name);
		}
	}
	fprintf(fp, ".endmodel %s;\n", vp->cellp->h.name);
	foreachentry(iht, hidx, instance *, ip) {
		if(ip->instof->primtype == PRIM_NOTPRIM) writeslif(fp, ip->instof);
	}
}
