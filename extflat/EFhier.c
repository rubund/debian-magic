/*
 * EFhier.c -
 *
 * Procedures for traversing the hierarchical representation
 * of a circuit.
 *
 *     ********************************************************************* 
 *     * Copyright (C) 1985, 1990 Regents of the University of California. * 
 *     * Permission to use, copy, modify, and distribute this              * 
 *     * software and its documentation for any purpose and without        * 
 *     * fee is hereby granted, provided that the above copyright          * 
 *     * notice appear in all copies.  The University of California        * 
 *     * makes no representations about the suitability of this            * 
 *     * software for any purpose.  It is provided "as is" without         * 
 *     * express or implied warranty.  Export of this software outside     * 
 *     * of the United States of America may require an export license.    * 
 *     *********************************************************************
 */

#ifndef lint
static char rcsid[] __attribute__ ((unused)) = "$Header: /usr/cvsroot/magic-7.5/extflat/EFhier.c,v 1.1.1.1 2006/04/10 22:03:13 tim Exp $";
#endif  /* not lint */

#include <stdio.h>
#include <math.h>

#include "utils/magic.h"
#include "utils/geometry.h"
#include "utils/geofast.h"
#include "utils/hash.h"
#include "utils/malloc.h"
#include "utils/utils.h"
#include "extflat/extflat.h"
#include "extflat/EFint.h"

/*
 * ----------------------------------------------------------------------------
 *
 * efHierSrUses --
 *
 * Visit all the children of hc->hc_use->use_def, keeping the transform
 * to flat coordinates and the hierarchical path from the root up to date.
 * For each child, calls the function 'func', which should be of the
 * following form:
 *
 *	int
 *	(*func)(hc, cdata)
 *	    HierContext *hc;
 *	    ClientData cdata;
 *	{
 *	}
 *
 * This procedure should return 0 normally, or 1 to abort the search.
 *
 * Hierarchical names:
 *	The current hierarchical prefix down to this point is given by the
 *	the HierName pointed to by hc->hc_hierName.  To construct a full
 *	hierarchical name from a name local to this def, we prepend a
 *	newly allocated HierName component to hc->hc_hierName.
 *
 * Results:
 *	Returns 0 if completed successfully, 1 if aborted.
 *
 * Side effects:
 *	Whatever (*func)() does.
 *
 * ----------------------------------------------------------------------------
 */

int
efHierSrUses(hc, func, cdata)
    HierContext *hc;
    int (*func)();
    ClientData cdata;
{
    int xlo, xhi, ylo, yhi, xbase, ybase, xsep, ysep;
    HierContext newhc;
    Transform t;
    Use *u;

    /* We should not descend into subcircuit "black boxes" */
//  if (hc->hc_use->use_def->def_flags & DEF_SUBCIRCUIT) return 0;

    for (u = hc->hc_use->use_def->def_uses; u; u = u->use_next)
    {
	newhc.hc_use = u;
	if (!IsArray(u))
	{
	    newhc.hc_hierName = efHNFromUse(&newhc, hc->hc_hierName);
	    GeoTransTrans(&u->use_trans, &hc->hc_trans, &newhc.hc_trans);
	    if ((*func)(&newhc, cdata))
		return (1);
	    continue;
	}

	/* Set up for iterating over all array elements */
	if (u->use_xlo <= u->use_xhi)
	    xlo = u->use_xlo, xhi = u->use_xhi, xsep = u->use_xsep;
	else
	    xlo = u->use_xhi, xhi = u->use_xlo, xsep = -u->use_xsep;
	if (u->use_ylo <= u->use_yhi)
	    ylo = u->use_ylo, yhi = u->use_yhi, ysep = u->use_ysep;
	else
	    ylo = u->use_yhi, yhi = u->use_ylo, ysep = -u->use_ysep;

	GeoTransTrans(&u->use_trans, &hc->hc_trans, &t);
	for (newhc.hc_x = xlo; newhc.hc_x <= xhi; newhc.hc_x++)
	    for (newhc.hc_y = ylo; newhc.hc_y <= yhi; newhc.hc_y++)
	    {
		xbase = xsep * (newhc.hc_x - u->use_xlo);
		ybase = ysep * (newhc.hc_y - u->use_ylo);
		GeoTransTranslate(xbase, ybase, &t, &newhc.hc_trans);
		newhc.hc_hierName = efHNFromUse(&newhc, hc->hc_hierName);
		if ((*func)(&newhc, cdata))
		    return (1);
	    }
    }

    return (0);
}

/*
 * ----------------------------------------------------------------------------
 *
 * efHierSrArray --
 *
 * Iterate over the subscripts in the Connection 'conn', deriving the
 * names conn_name1 and conn_name2 for each such subscript, calling
 * the supplied procedure for each.
 *
 * This procedure should be of the following form:
 *
 *	(*proc)(hc, name1, name2, conn, cdata)
 *	    HierContext *hc;
 *	    char *name1;	/# Fully-expanded first name #/
 *	    char *name2;	/# Fully-expanded 2nd name, or NULL #/
 *	    Connection *conn;
 *	    ClientData cdata;
 *	{
 *	}
 *
 * The procedure should return 0 normally, or 1 if it wants us to abort.
 *
 * Results:
 *	0 normally, or 1 if we were aborted.
 *
 * Side effects:
 *	Whatever those of 'proc' are.
 *
 * ----------------------------------------------------------------------------
 */

int
efHierSrArray(hc, conn, proc, cdata)
    HierContext *hc;
    Connection *conn;
    int (*proc)();
    ClientData cdata;
{
    char name1[1024], name2[1024];
    int i, j, i1lo, i2lo, j1lo, j2lo;
    ConnName *c1, *c2;

    /*
     * Only handle three cases:
     *  0 subscripts
     *	1 subscript
     *	2 subscripts
     */
    c1 = &conn->conn_1;
    c2 = &conn->conn_2;
    switch (c1->cn_nsubs)
    {
	case 0:
	    return (*proc)(hc, c1->cn_name, c2->cn_name, conn, cdata);
	    break;
	case 1:
	    i1lo = c1->cn_subs[0].r_lo, i2lo = c2->cn_subs[0].r_lo;
	    for (i = i1lo; i <= c1->cn_subs[0].r_hi; i++)
	    {
		(void) sprintf(name1, c1->cn_name, i);
		if (c2->cn_name)
		    (void) sprintf(name2, c2->cn_name, i - i1lo + i2lo);
		if ((*proc)(hc, name1, c2->cn_name ? name2 : (char *) NULL,
				conn, cdata))
		    return 1;
	    }
	    break;
	case 2:
	    i1lo = c1->cn_subs[0].r_lo, i2lo = c2->cn_subs[0].r_lo;
	    j1lo = c1->cn_subs[1].r_lo, j2lo = c2->cn_subs[1].r_lo;
#ifdef	notdef
	    (void) printf("[%d:%d,%d:%d] [%d:%d,%d:%d]\n",
		i1lo, c1->cn_subs[0].r_hi,
		j1lo, c1->cn_subs[1].r_hi,
		i2lo, c2->cn_subs[0].r_hi,
		j2lo, c2->cn_subs[1].r_hi);
#endif	/* notdef */
	    for (i = i1lo; i <= c1->cn_subs[0].r_hi; i++)
	    {
		for (j = j1lo; j <= c1->cn_subs[1].r_hi; j++)
		{
		    (void) sprintf(name1, c1->cn_name, i, j);
		    if (c2->cn_name)
			(void) sprintf(name2, c2->cn_name,
				    i - i1lo + i2lo, j - j1lo + j2lo);
		    if ((*proc)(hc,name1,c2->cn_name ? name2 : (char *) NULL,
				conn, cdata))
			return 1;
		}
	    }
	    break;
	default:
	    printf("Can't handle > 2 array subscripts\n");
	    break;
    }

    return 0;
}
