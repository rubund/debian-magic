
#ifndef	lint
static char rcsid[] __attribute__ ((unused)) = "$Header: /usr/cvsroot/magic-7.5/drc/DRCextend.c,v 1.8 2009/01/22 03:07:36 tim Exp $";
#endif	

#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "utils/magic.h"
#include "utils/malloc.h"
#include "utils/geometry.h"
#include "tiles/tile.h"
#include "utils/hash.h"
#include "database/database.h"
#include "windows/windows.h"
#include "dbwind/dbwind.h"
#include "dbwind/dbwtech.h"
#include "drc/drc.h"
#include "utils/signals.h"
#include "utils/stack.h"

Stack *DRCstack = (Stack *)NULL;

#define PUSHTILE(tp) \
    if ((tp)->ti_client == (ClientData) DRC_UNPROCESSED) { \
        (tp)->ti_client = (ClientData)  DRC_PENDING; \
        STACKPUSH((ClientData) (tp), DRCstack); \
    }

#ifdef NONMANHATTAN

/*
 *-------------------------------------------------------------------------
 *
 * drcCheckAngles --- checks whether a tile conforms to orthogonal-only
 *	geometry (90 degree angles only) or 45-degree geometry (x must
 *	be equal to y on all non-Manhattan tiles).
 *
 * Results: none
 *
 * Side Effects: may cause errors to be painted.
 *
 *-------------------------------------------------------------------------
 */


void
drcCheckAngles(tile, arg, cptr)
    Tile	*tile;
    struct drcClientData *arg;
    DRCCookie	*cptr;
{
    Rect rect;
    int ortho = (cptr->drcc_flags & 0x01);  /* 1 = orthogonal, 0 = 45s */

    if (IsSplit(tile))
    {
	if (ortho || (RIGHT(tile) - LEFT(tile)) != (TOP(tile) - BOTTOM(tile)))
	{
	    TiToRect(tile, &rect);
	    GeoClip(&rect, arg->dCD_clip);
	    if (!GEO_RECTNULL(&rect))
	    {
		arg->dCD_cptr = cptr;
		(*(arg->dCD_function)) (arg->dCD_celldef, &rect,
			arg->dCD_cptr, arg->dCD_clientData);
		(*(arg->dCD_errors))++;
	    }
	}
    }
}

#else

/* When non-Manhattan geometry is not compiled in, this routine	*/
/* is merely a placeholder.					*/

void
drcCheckAngles(tile, arg, cptr)
    Tile	*starttile;
    struct drcClientData *arg;
    DRCCookie	*cptr;
{
    /* Do nothing */
}

#endif	/* NONMANHATTAN */

/*
 *-------------------------------------------------------------------------
 *
 * drcCheckArea- checks to see that a collection of tiles of a given 
 *	type have more than a minimum area.
 *
 * Results: none
 *
 * Side Effects: may cause errors to be painted.
 *
 *-------------------------------------------------------------------------
 */

void
drcCheckArea(starttile,arg,cptr)
	Tile	*starttile;
	struct drcClientData	*arg;
	DRCCookie	*cptr;

{
    int			arealimit;
    int			area=0;
    TileTypeBitMask	*oktypes = &cptr->drcc_mask;
    Tile		*tile,*tp;
    Rect		*cliprect = arg->dCD_rect;

    arealimit = cptr->drcc_cdist;
     
    arg->dCD_cptr = cptr;
    if (DRCstack == (Stack *) NULL)
	DRCstack = StackNew(64);

    /* Mark this tile as pending and push it */
    PUSHTILE(starttile);

    while (!StackEmpty(DRCstack))
    {
	tile = (Tile *) STACKPOP(DRCstack);
	if (tile->ti_client != (ClientData)DRC_PENDING) continue;
	area += (RIGHT(tile)-LEFT(tile))*(TOP(tile)-BOTTOM(tile));
	tile->ti_client = (ClientData)DRC_PROCESSED;
	/* are we at the clip boundary? If so, skip to the end */
	if (RIGHT(tile) == cliprect->r_xtop ||
	    LEFT(tile) == cliprect->r_xbot ||
	    BOTTOM(tile) == cliprect->r_ybot ||
	    TOP(tile) == cliprect->r_ytop) goto forgetit;

        if (area >= arealimit) goto forgetit;

	/* Top */
	for (tp = RT(tile); RIGHT(tp) > LEFT(tile); tp = BL(tp))
	    if (TTMaskHasType(oktypes, TiGetBottomType(tp)))	PUSHTILE(tp);

	/* Left */
	for (tp = BL(tile); BOTTOM(tp) < TOP(tile); tp = RT(tp))
	    if (TTMaskHasType(oktypes, TiGetRightType(tp))) PUSHTILE(tp);

	/* Bottom */
	for (tp = LB(tile); LEFT(tp) < RIGHT(tile); tp = TR(tp))
	    if (TTMaskHasType(oktypes, TiGetTopType(tp))) PUSHTILE(tp);

	/* Right */
	for (tp = TR(tile); TOP(tp) > BOTTOM(tile); tp = LB(tp))
	    if (TTMaskHasType(oktypes, TiGetLeftType(tp))) PUSHTILE(tp);
     }

     if (area < arealimit)
     {
	 Rect	rect;
	 TiToRect(starttile,&rect);
	 GeoClip(&rect, arg->dCD_clip);
	 if (!GEO_RECTNULL(&rect)) {
	     (*(arg->dCD_function)) (arg->dCD_celldef, &rect,
		 arg->dCD_cptr, arg->dCD_clientData);
	     /***
	     DBWAreaChanged(arg->dCD_celldef,&rect, DBW_ALLWINDOWS, 
						    &DBAllButSpaceBits);
	     ***/
	     (*(arg->dCD_errors))++;
	 }
     }

forgetit:
     while (!StackEmpty(DRCstack)) tile = (Tile *) STACKPOP(DRCstack);

     /* reset the tiles */
     starttile->ti_client = (ClientData)DRC_UNPROCESSED;
     STACKPUSH(starttile, DRCstack);
     while (!StackEmpty(DRCstack))
     {
	tile = (Tile *) STACKPOP(DRCstack);

	/* Top */
	for (tp = RT(tile); RIGHT(tp) > LEFT(tile); tp = BL(tp))
	    if (tp->ti_client != (ClientData)DRC_UNPROCESSED)
	    {
	    	 tp->ti_client = (ClientData)DRC_UNPROCESSED;
		 STACKPUSH(tp,DRCstack);
	    }

	/* Left */
	for (tp = BL(tile); BOTTOM(tp) < TOP(tile); tp = RT(tp))
	    if (tp->ti_client != (ClientData)DRC_UNPROCESSED)
	    {
	    	 tp->ti_client = (ClientData)DRC_UNPROCESSED;
		 STACKPUSH(tp,DRCstack);
	    }

	/* Bottom */
	for (tp = LB(tile); LEFT(tp) < RIGHT(tile); tp = TR(tp))
	    if (tp->ti_client != (ClientData)DRC_UNPROCESSED)
	    {
	    	 tp->ti_client = (ClientData)DRC_UNPROCESSED;
		 STACKPUSH(tp,DRCstack);
	    }

	/* Right */
	for (tp = TR(tile); TOP(tp) > BOTTOM(tile); tp = LB(tp))
	    if (tp->ti_client != (ClientData)DRC_UNPROCESSED)
	    {
	    	 tp->ti_client = (ClientData)DRC_UNPROCESSED;
		 STACKPUSH(tp,DRCstack);
	    }

     }
}


/*
 *-------------------------------------------------------------------------
 *
 * drcCheckMaxwidth - checks to see that at least one dimension of a region
 *	does not exceed some amount (original version---for "bends_illegal"
 *	option only).
 *
 *  This should really be folded together with drcCheckArea, since the routines
 *	are nearly identical, but I'm feeling lazy, so I'm just duplicating
 *	the code for now.
 *
 * Results: 1 if within max bounds, 0 otherwise.
 *
 * Side Effects: may cause errors to be painted.
 *
 *-------------------------------------------------------------------------
 */

int
drcCheckMaxwidth(starttile,arg,cptr)
    Tile	*starttile;
    struct drcClientData	*arg;
    DRCCookie	*cptr;
{
    int			edgelimit;
    int			retval = 0;
    Rect		boundrect;
    TileTypeBitMask	*oktypes;
    Tile		*tile,*tp;

    oktypes = &cptr->drcc_mask;
    edgelimit = cptr->drcc_dist;
    arg->dCD_cptr = cptr;
    if (DRCstack == (Stack *) NULL)
	DRCstack = StackNew(64);

    /* Mark this tile as pending and push it */

    PUSHTILE(starttile);
    TiToRect(starttile,&boundrect);

    while (!StackEmpty(DRCstack))
    {
	tile = (Tile *) STACKPOP(DRCstack);
	if (tile->ti_client != (ClientData)DRC_PENDING) continue;
	tile->ti_client = (ClientData)DRC_PROCESSED;
	
	if (boundrect.r_xbot > LEFT(tile)) boundrect.r_xbot = LEFT(tile);
	if (boundrect.r_xtop < RIGHT(tile)) boundrect.r_xtop = RIGHT(tile);
	if (boundrect.r_ybot > BOTTOM(tile)) boundrect.r_ybot = BOTTOM(tile);
	if (boundrect.r_ytop < TOP(tile)) boundrect.r_ytop = TOP(tile);

        if (boundrect.r_xtop - boundrect.r_xbot > edgelimit &&
             boundrect.r_ytop - boundrect.r_ybot > edgelimit)
	{
	    while (!StackEmpty(DRCstack)) tile = (Tile *) STACKPOP(DRCstack);
	    break;
	}

	/* Top */
	for (tp = RT(tile); RIGHT(tp) > LEFT(tile); tp = BL(tp))
	    if (TTMaskHasType(oktypes, TiGetBottomType(tp)))	PUSHTILE(tp);

	/* Left */
	for (tp = BL(tile); BOTTOM(tp) < TOP(tile); tp = RT(tp))
	    if (TTMaskHasType(oktypes, TiGetRightType(tp))) PUSHTILE(tp);

	/* Bottom */
	for (tp = LB(tile); LEFT(tp) < RIGHT(tile); tp = TR(tp))
	    if (TTMaskHasType(oktypes, TiGetTopType(tp))) PUSHTILE(tp);

	/* Right */
	for (tp = TR(tile); TOP(tp) > BOTTOM(tile); tp = LB(tp))
	    if (TTMaskHasType(oktypes, TiGetLeftType(tp))) PUSHTILE(tp);
    }

    if (boundrect.r_xtop - boundrect.r_xbot > edgelimit &&
             boundrect.r_ytop - boundrect.r_ybot > edgelimit) 
    {
	Rect	rect;
	TiToRect(starttile,&rect);
	GeoClip(&rect, arg->dCD_clip);
	if (!GEO_RECTNULL(&rect)) {
	    (*(arg->dCD_function)) (arg->dCD_celldef, &rect,
			arg->dCD_cptr, arg->dCD_clientData);
	    (*(arg->dCD_errors))++;
	    retval = 1;
	}
	 
    }

    /* reset the tiles */
    starttile->ti_client = (ClientData)DRC_UNPROCESSED;
    STACKPUSH(starttile, DRCstack);
    while (!StackEmpty(DRCstack))
    {
	tile = (Tile *) STACKPOP(DRCstack);

	/* Top */
	for (tp = RT(tile); RIGHT(tp) > LEFT(tile); tp = BL(tp))
	    if (tp->ti_client != (ClientData)DRC_UNPROCESSED)
	    {
	    	 tp->ti_client = (ClientData)DRC_UNPROCESSED;
		 STACKPUSH(tp,DRCstack);
	    }

	/* Left */
	for (tp = BL(tile); BOTTOM(tp) < TOP(tile); tp = RT(tp))
	    if (tp->ti_client != (ClientData)DRC_UNPROCESSED)
	    {
	    	 tp->ti_client = (ClientData)DRC_UNPROCESSED;
		 STACKPUSH(tp,DRCstack);
	    }

	/* Bottom */
	for (tp = LB(tile); LEFT(tp) < RIGHT(tile); tp = TR(tp))
	    if (tp->ti_client != (ClientData)DRC_UNPROCESSED)
	    {
	    	 tp->ti_client = (ClientData)DRC_UNPROCESSED;
		 STACKPUSH(tp,DRCstack);
	    }

	/* Right */
	for (tp = TR(tile); TOP(tp) > BOTTOM(tile); tp = LB(tp))
	    if (tp->ti_client != (ClientData)DRC_UNPROCESSED)
	    {
	    	 tp->ti_client = (ClientData)DRC_UNPROCESSED;
		 STACKPUSH(tp,DRCstack);
	    }

    }
    return retval;
}


/*
 *-------------------------------------------------------------------------
 *
 * drcCheckRectSize- 
 *
 *	Checks to see that a collection of tiles of given 
 *	types have the proper size (max size and also even or odd size).
 *
 * Results: none
 *
 * Side Effects: may cause errors to be painted.
 *
 *-------------------------------------------------------------------------
 */

void
drcCheckRectSize(starttile, arg, cptr)
    Tile *starttile;
    struct drcClientData *arg;
    DRCCookie *cptr;
{
    int maxsize, even;
    TileTypeBitMask *oktypes = &cptr->drcc_mask;
    int width;
    int height;
    int errwidth;
    int errheight;
    Tile *t;
    bool error = FALSE;

    maxsize = cptr->drcc_dist;
    even = cptr->drcc_cdist;

    /* This code only has to work for rectangular regions, since we always
     * check for rectangular-ness using normal edge rules produced when
     * we read in the tech file.
     */
    arg->dCD_cptr = cptr;
    ASSERT(TTMaskHasType(oktypes, TiGetType(starttile)), "drcCheckRectSize");
    for (t = starttile; TTMaskHasType(oktypes, TiGetType(t)); t = TR(t)) 
	/* loop has empty body */ ;
    errwidth = width = LEFT(t) - LEFT(starttile);
    for (t = starttile; TTMaskHasType(oktypes, TiGetType(t)); t = RT(t)) 
	/* loop has empty body */ ;
    errheight = height = BOTTOM(t) - BOTTOM(starttile);
    ASSERT(width > 0 && height > 0, "drcCheckRectSize");

    if (width > maxsize) {error = TRUE; errwidth = (width - maxsize);}
    else if (height > maxsize) {error = TRUE; errheight = (height - maxsize);}
    else if (even >= 0) {
	/* meaning of "even" variable:  -1, any; 0, even; 1, odd */
	if (ABS(width - ((width/2)*2)) != even) {error = TRUE; errwidth = 1;}
	else if (ABS(height - ((height/2)*2)) != even) {error = TRUE; errheight = 1;}
    }

    if (error) {
	Rect rect;
	TiToRect(starttile, &rect);
	rect.r_xtop = rect.r_xbot + errwidth;
	rect.r_ytop = rect.r_ybot + errheight;
	GeoClip(&rect, arg->dCD_clip);
	if (!GEO_RECTNULL(&rect)) {
	    (*(arg->dCD_function)) (arg->dCD_celldef, &rect,
		arg->dCD_cptr, arg->dCD_clientData);
	    (*(arg->dCD_errors))++;
	}
	
    }
}

/*
 *-------------------------------------------------------------------------
 *
 * drcCanonicalMaxwidth - checks to see that at least one dimension of a
 *	rectangular region does not exceed some amount.
 *
 *	This differs from "CheckMaxwidth" in being more rigorous about
 *	determining where a region of max width might be found.  There
 *	is no "bend" rule here.  We check from the edge being observed
 *	and back, and adjust the bounds on the sides, forking as
 *	necessary to consider alternative arrangements of the interior
 *	rectangle.  A distance "dist" is passed to the routine.  We
 *	may push the interior rectangle back by up to this amount from
 *	the observed edge.  For "widespacing" rules, we check all
 *	interior regions that satisfy maxwidth and whose edge is
 *	within "dist" of the original edge.  For slotting requirement
 *	rules, "dist" is zero (inability to find a rectangle touching
 *	the original edge ensures that no such rectangle exists that
 *	can't be found touching a different edge).  Also, we only
 *	need to check one of the four possible edge combinations
 *	(this part of it is handled in the drcBasic code).
 *
 * Results:
 *	LinkedRect list of areas satisfying maxwidth.  There may be
 *	more than one rectangle, and rectangles may overlap.  It
 *	may make more sense to return only one rectangle, the union
 *	of all rectangles in the list.
 *
 * Side Effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

MaxRectsData *
drcCanonicalMaxwidth(starttile, dir, arg, cptr)
    Tile	*starttile;
    int		dir;			/* direction of rule */
    struct	drcClientData	*arg;
    DRCCookie	*cptr;
{
    int		    s, edgelimit;
    Tile	    *tile,*tp;
    TileTypeBitMask wrongtypes;
    static MaxRectsData *mrd = (MaxRectsData *)NULL;
    extern int	    findMaxRects();		/* forward declaration */
    Rect	    *boundrect, boundorig;

    /* Generate an initial array size of 8 for rlist and swap. */
    if (mrd == (MaxRectsData *)NULL)
    {
	mrd = (MaxRectsData *)mallocMagic(sizeof(MaxRectsData));
	mrd->rlist = (Rect *)mallocMagic(8 * sizeof(Rect));
	mrd->swap = (Rect *)mallocMagic(8 * sizeof(Rect));
	mrd->listdepth = 8;
    }
    boundrect = &(mrd->rlist[0]);
     
    edgelimit = cptr->drcc_dist;
    arg->dCD_cptr = cptr;

    TiToRect(starttile, boundrect);

    /* Determine area to be searched */

    switch (dir)
    {
	case GEO_NORTH:
	    boundrect->r_ytop = boundrect->r_ybot;
	    boundrect->r_xbot -= (edgelimit - 1);
	    boundrect->r_xtop += (edgelimit - 1);
	    boundrect->r_ytop += edgelimit;
	    break;

	case GEO_SOUTH:
	    boundrect->r_ybot = boundrect->r_ytop;
	    boundrect->r_xbot -= (edgelimit - 1);
	    boundrect->r_xtop += (edgelimit - 1);
	    boundrect->r_ybot -= edgelimit;
	    break;

	case GEO_EAST:
	    boundrect->r_xtop = boundrect->r_xbot;
	    boundrect->r_ybot -= (edgelimit - 1);
	    boundrect->r_ytop += (edgelimit - 1);
	    boundrect->r_xtop += edgelimit;
	    break;

	case GEO_WEST:
	    boundrect->r_xbot = boundrect->r_xtop;
	    boundrect->r_ybot -= (edgelimit - 1);
	    boundrect->r_ytop += (edgelimit - 1);
	    boundrect->r_xbot -= edgelimit;
	    break;

	case GEO_CENTER:
	    boundrect->r_xbot -= edgelimit;
	    boundrect->r_xtop += edgelimit;
	    boundrect->r_ybot -= edgelimit;
	    boundrect->r_ytop += edgelimit;
	    break;
    }

    /* Do an area search on boundrect to find all materials not	*/
    /* in oktypes.  Each such tile clips or subdivides		*/
    /* boundrect.  Any rectangles remaining after the search	*/
    /* satisfy the maxwidth rule.				*/

    mrd->entries = 1;
    mrd->maxdist = edgelimit;
    TTMaskCom2(&wrongtypes, &cptr->drcc_mask);
    boundorig = *boundrect;
    DBSrPaintArea(starttile, arg->dCD_celldef->cd_planes[cptr->drcc_plane],
		&boundorig, &wrongtypes, findMaxRects, mrd);
    if (mrd->entries == 0)
	return NULL;
    else
	return (MaxRectsData *)mrd;
}

int
findMaxRects(tile, mrd)
    Tile *tile;
    MaxRectsData *mrd;
{
    Rect area;
    Rect *rlist, *sr, *tmp;
    int s, entries = 0;

    TiToRect(tile, &area);
    rlist = mrd->swap;
    for (s = 0; s < mrd->entries; s++)
    {
	sr = &(mrd->rlist[s]);
	if (GEO_OVERLAP(sr, &area))
	{
	    /* Top */
	    if (sr->r_ytop >= area.r_ytop + mrd->maxdist)
	    {
		rlist[entries] = *sr;
		rlist[entries].r_ybot = area.r_ytop;
		entries++;
	    }
	    /* Bottom */
	    if (sr->r_ybot <= area.r_ybot - mrd->maxdist)
	    {
		rlist[entries] = *sr;
		rlist[entries].r_ytop = area.r_ybot;
		entries++;
	    }
	    /* Left */
	    if (sr->r_xbot <= area.r_xbot - mrd->maxdist)
	    {
		rlist[entries] = *sr;
		rlist[entries].r_xtop = area.r_xbot;
		entries++;
	    }
	    /* Right */
	    if (sr->r_xtop >= area.r_xtop + mrd->maxdist)
	    {
		rlist[entries] = *sr;
		rlist[entries].r_xbot = area.r_xtop;
		entries++;
	    }
	}
	else
	{
	    /* Copy sr to the new list */
	    rlist[entries] = *sr;
	    entries++;
	}

	/* If we have more rectangles than we allocated space for,	*/
	/* double the list size and continue.				*/

	if (entries > (mrd->listdepth - 4))
	{
	    Rect *newrlist;		

	    mrd->listdepth <<= 1;	/* double the list size */

	    newrlist = (Rect *)mallocMagic(mrd->listdepth * sizeof(Rect));
	    memcpy((void *)newrlist, (void *)mrd->rlist,
				(size_t)mrd->entries * sizeof(Rect));
	    // for (s = 0; s < entries; s++)
	    //	   newrlist[s] = mrd->rlist[s];
	    freeMagic(mrd->rlist);
	    mrd->rlist = newrlist;

	    newrlist = (Rect *)mallocMagic(mrd->listdepth * sizeof(Rect));
	    memcpy((void *)newrlist, (void *)mrd->swap,
				(size_t)entries * sizeof(Rect));
	    // for (s = 0; s < entries; s++)
	    //     newrlist[s] = mrd->swap[s];
	    freeMagic(mrd->swap);
	    mrd->swap = newrlist;

	    rlist = mrd->swap;
	}
    }

    /* copy rlist back to mrd by swapping "rlist" and "swap" */
    mrd->entries = entries;
    tmp = mrd->rlist;
    mrd->rlist = rlist;
    mrd->swap = tmp;

    if (entries > 0)
	return 0; 	/* keep the search going */
    else
	return 1;	/* no more max size areas; stop the search */
}

/*
 *-------------------------------------------------------------------------
 *
 * DRCMaxRectangle ---
 *
 *	This is a general-purpose routine to be called from anywhere
 *	that expands an area to the largest area rectangle containing
 *	tiles connected to the given types list.  This is kludged
 *	together to make use of DRC structures, but it is meant to
 *	be independent of the DRC code.  It can be called, for example,
 *	from the router code to expand a point label into the maximum
 *	size rectangular terminal.
 *
 * Results:
 *	Pointer to a rectangle containing the maximum size area found.
 *	This pointer should not be deallocated!
 *	
 *-------------------------------------------------------------------------
 */

Rect *
DRCMaxRectangle(def, startpoint, pNum, expandtypes, expanddist)
    CellDef *def;
    Point *startpoint;
    int pNum;				/* plane of types to expand */
    TileTypeBitMask *expandtypes;	/* types to expand in */
    int expanddist;
{
    MaxRectsData *mrd;
    struct drcClientData dcd;
    DRCCookie	cptr;
    Tile *starttile;
    TileType tt;
    Plane *plane;
    int rectArea;
    int maxarea = 0;
    int s, sidx = -1;
    Rect origrect;

    cptr.drcc_plane = pNum;
    cptr.drcc_dist = expanddist;
    cptr.drcc_mask = *expandtypes;

    dcd.dCD_celldef = def;
    dcd.dCD_cptr = &cptr;

    /* Find tile in def that surrounds or touches startpoint */
    plane = def->cd_planes[pNum];
    starttile = plane->pl_hint;
    GOTOPOINT(starttile, startpoint);
    mrd = drcCanonicalMaxwidth(starttile, GEO_CENTER, &dcd, &cptr);

    /* Return only the largest rectangle that contains starttile. */

    TiToRect(starttile, &origrect);

    for (s = 0; s < mrd->entries; s++)
    {
	if (GEO_SURROUND(&mrd->rlist[s], &origrect))
	{
	    rectArea = (mrd->rlist[s].r_xtop - mrd->rlist[s].r_xbot) *
			(mrd->rlist[s].r_ytop - mrd->rlist[s].r_ybot);
	    if (rectArea > maxarea)
	    {
		maxarea = rectArea;
		sidx = s;
	    }
	}
    }

    if (sidx < 0)
    {
	sidx = 0;
	mrd->rlist[0] = origrect;
    }
    return &(mrd->rlist[sidx]);
}
