/*
 * DBpaint2.c --
 *
 * More paint and erase primitives
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
static char rcsid[] __attribute__ ((unused)) = "$Header: /usr/cvsroot/magic-7.5/database/DBpaint2.c,v 1.15 2008/05/18 14:46:42 tim Exp $";
#endif  /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "utils/magic.h"
#include "utils/geometry.h"
#include "tiles/tile.h"
#include "utils/hash.h"
#include "database/database.h"
#include "database/databaseInt.h"

/*
 * ----------------------------------------------------------------------------
 * DBPaint --
 *
 * Paint a rectangular area with a specific tile type.
 * All paint tile planes in cellDef are painted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies potentially all paint tile planes in cellDef.
 * ----------------------------------------------------------------------------
 */

void
DBPaint (cellDef, rect, type)
    CellDef  * cellDef;		/* CellDef to modify */
    Rect     * rect;		/* Area to paint */
    TileType   type;		/* Type of tile to be painted */
{
    int pNum;
    PaintUndoInfo ui;
    TileType  loctype = type;   /* Local value of tile type */

#ifdef NONMANHATTAN 
    if (type & TT_DIAGONAL)
	loctype = (type & TT_SIDE) ?
		(type & TT_RIGHTMASK) >> 14 : (type & TT_LEFTMASK);
#endif

    cellDef->cd_flags |= CDMODIFIED|CDGETNEWSTAMP;
    ui.pu_def = cellDef;
    for (pNum = PL_PAINTBASE; pNum < DBNumPlanes; pNum++)
	if (DBPaintOnPlane(loctype, pNum))
	{
	    ui.pu_pNum = pNum;
#ifdef NONMANHATTAN
	    DBNMPaintPlane(cellDef->cd_planes[pNum], type, rect,
			DBStdPaintTbl(loctype, pNum), &ui);
#else
	    DBPaintPlane(cellDef->cd_planes[pNum], rect,
			DBStdPaintTbl(type, pNum), &ui);
#endif
	}

    /* Resolve images over all their planes.  This allows the	*/
    /* definition of composite types generated from types on	*/
    /* different planes, as well as relaxing the constraints on	*/
    /* the "compose" section of the technology file.		*/

    if (loctype < DBNumUserLayers)
    {
	TileTypeBitMask *rMask, tMask;
	TileType itype;
	int dbResolveImages();

	for (itype = TT_SELECTBASE; itype < DBNumUserLayers; itype++)
	{
	    /* Ignore self, or infinite looping will result */
	    if (itype == loctype) continue;

	    rMask = DBResidueMask(itype);
	    if (TTMaskHasType(rMask, loctype))
	    {
		TTMaskZero(&tMask);
		TTMaskSetType(&tMask, itype);
		for (pNum = PL_PAINTBASE; pNum < DBNumPlanes; pNum++)
		{
		    if (DBPaintOnPlane(itype, pNum))
		    {
#ifdef NONMANHATTAN
			DBSrPaintNMArea((Tile *)NULL, cellDef->cd_planes[pNum],
				type, rect, &tMask, dbResolveImages,
				(ClientData)cellDef);
#else
			DBSrPaintArea((Tile *)NULL, cellDef->cd_planes[pNum],
				rect, &tMask, dbResolveImages,
				(ClientData)cellDef);
#endif
    		    }
		}
    	    }
	}
    }
}

/*
 * dbResolveImages ---
 *
 *	This callback function is called from DBSrPaintArea and
 *	makes a recursive call to DBPaint() for tiles that have
 *	images on multiple planes but which may not have been
 *	painted on all of those planes due to restrictions of
 *	the paint table.
 */

int
dbResolveImages(tile, cellDef)
    Tile *tile;
    CellDef *cellDef;
{
    Rect rect;

    TiToRect(tile, &rect);

    /* Recursive call back to DBPaint---this will ensure that   */
    /* all of the planes of the image type are painted.         */

    DBPaint(cellDef, &rect, TiGetTypeExact(tile));
    return 0;
}


/*
 * ----------------------------------------------------------------------------
 * DBErase --
 *
 * Erase a specific tile type from a rectangular area.
 * The plane in which tiles of the given type reside is modified
 * in cellDef.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies potentially all paint tile planes in cellDef.
 * ----------------------------------------------------------------------------
 */

void
DBErase (cellDef, rect, type)
    CellDef  * cellDef;		/* Cell to modify */
    Rect     * rect;		/* Area to paint */
    TileType   type;		/* Type of tile to be painted */
{
    int pNum;
    PaintUndoInfo ui;
    TileType   loctype = type;  /* Local value of tile type */

#ifdef NONMANHATTAN
    if (type & TT_DIAGONAL)
	loctype = (type & TT_SIDE) ?
		(type & TT_RIGHTMASK) >> 14 : (type & TT_LEFTMASK);
#endif

    cellDef->cd_flags |= CDMODIFIED|CDGETNEWSTAMP;
    ui.pu_def = cellDef;
    if (loctype == TT_SPACE)
    {
	/*
	 * Erasing space is the same as erasing everything under
	 * the rectangle.
	 */
	for (pNum = PL_PAINTBASE; pNum < DBNumPlanes; pNum++)
	{
	    ui.pu_pNum = pNum;
#ifdef NONMANHATTAN
	    DBNMPaintPlane(cellDef->cd_planes[pNum], type, rect,
			DBStdPaintTbl(loctype, pNum), &ui);
#else
	    DBPaintPlane(cellDef->cd_planes[pNum], rect,
			DBStdPaintTbl(TT_SPACE, pNum), &ui);
#endif
	}
    }
    else
    {
	/*
	 * Ordinary type is being erased.
	 * Generate the erase on all planes in cellDef.
	 */
	for (pNum = PL_PAINTBASE; pNum < DBNumPlanes; pNum++)
	    if (DBEraseOnPlane(loctype, pNum))
	    {
		ui.pu_pNum = pNum;
#ifdef NONMANHATTAN
		DBNMPaintPlane(cellDef->cd_planes[pNum], type, rect,
			    DBStdEraseTbl(loctype, pNum), &ui);
#else
		DBPaintPlane(cellDef->cd_planes[pNum], rect,
			    DBStdEraseTbl(type, pNum), &ui);
#endif
	    }
    }
}

/*
 * ----------------------------------------------------------------------------
 * DBPaintMask --
 *
 * Paint a rectangular area with all tile types specified in the
 * mask supplied.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies potentially all paint tile planes in cellDef.
 * ----------------------------------------------------------------------------
 */

void
DBPaintMask(cellDef, rect, mask)
    CellDef	*cellDef;	/* CellDef to modify */
    Rect	*rect;		/* Area to paint */
    TileTypeBitMask *mask;	/* Mask of types to be erased */
{
    TileType t;

    for (t = TT_SPACE + 1; t < DBNumTypes; t++)
	if (TTMaskHasType(mask, t))
	    DBPaint(cellDef, rect, t);
}

/*
 * ----------------------------------------------------------------------------
 * DBPaintValid --
 *
 * Paint a rectangular area with all tile types specified in the
 * mask supplied, ANDed with valid layers.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies potentially all paint tile planes in cellDef.
 * ----------------------------------------------------------------------------
 */

void
DBPaintValid(cellDef, rect, mask, dinfo)
    CellDef	*cellDef;	/* CellDef to modify */
    Rect	*rect;		/* Area to paint */
    TileTypeBitMask *mask;	/* Mask of types to be erased */
    TileType 	dinfo;		/* If non-zero, then rect is a triangle and
				 * dinfo contains side and direction information
				 */
{
    TileType t, tt, tloc, dloc;
    TileTypeBitMask rmask, mmask, *tMask;

#ifdef NONMANHATTAN
    dloc = dinfo & (TT_DIAGONAL | TT_SIDE | TT_DIRECTION);
#endif

    TTMaskZero(&mmask);
    TTMaskSetMask(&mmask, mask);

    /* Decompose stacked contacts */

    for (t = DBNumUserLayers; t < DBNumTypes; t++) 
	if (TTMaskHasType(mask, t))
	    TTMaskSetMask(&mmask, DBResidueMask(t));

    /* Remove any inactive layers */

    TTMaskAndMask(&mmask, &DBActiveLayerBits);

    /* If any residue of a contact is not in the active layers	*/
    /* list, then paint only the valid residue layers.		*/
 
    for (t = TT_SELECTBASE; t < DBNumUserLayers; t++)
	if (TTMaskHasType(&mmask, t))
	    if (DBIsContact(t))
	    {
		tMask = DBResidueMask(t);
		TTMaskAndMask3(&rmask, tMask, &DBActiveLayerBits);
		if (TTMaskEqual(&rmask, tMask))
		{
#ifdef NONMANHATTAN
		    tloc = dloc | ((dinfo & TT_DIAGONAL) ? ((dinfo & TT_SIDE) ?
				(t << 14) : t) : t);
#endif
		    DBPaint(cellDef, rect, tloc);
		}
		else if (!TTMaskIsZero(&rmask))
		{
		    for (tt = TT_SPACE + 1; tt < DBNumTypes; tt++)
			if (TTMaskHasType(&rmask, tt))
			{
#ifdef NONMANHATTAN
			    tloc = dloc | ((dinfo & TT_DIAGONAL) ?
					((dinfo & TT_SIDE) ? (tt << 14) : tt) : tt);
			    DBPaint(cellDef, rect, tloc);
#else
			    DBPaint(cellDef, rect, tt);
#endif
			}
		}
	    }
	    else
	    {
#ifdef NONMANHATTAN
		tloc = dloc | ((dinfo & TT_DIAGONAL) ? ((dinfo & TT_SIDE) ?
				(t << 14) : t) : t);
#endif
		DBPaint(cellDef, rect, tloc);
	    }
}

/*
 * ----------------------------------------------------------------------------
 * DBEraseMask --
 *
 * Erase a rectangular area with all tile types specified in the
 * mask supplied.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies potentially all paint tile planes in cellDef.
 * ----------------------------------------------------------------------------
 */

void
DBEraseMask(cellDef, rect, mask)
    CellDef	*cellDef;	/* CellDef to modify */
    Rect	*rect;		/* Area to erase */
    TileTypeBitMask *mask;	/* Mask of types to be erased */
{
    TileType t;

    for (t = DBNumTypes - 1; t >= TT_SELECTBASE; t--)
	if (TTMaskHasType(mask, t))
	    DBErase(cellDef, rect, t);
}

/*
 * ----------------------------------------------------------------------------
 * DBEraseValid --
 *
 * Erase a rectangular area with all tile types specified in the
 * mask supplied, ANDed with valid layers.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies potentially all paint tile planes in cellDef.
 * ----------------------------------------------------------------------------
 */

void
DBEraseValid(cellDef, rect, mask, dinfo)
    CellDef	*cellDef;	/* CellDef to modify */
    Rect	*rect;		/* Area to erase */
    TileTypeBitMask *mask;	/* Mask of types to be erased */
    TileType 	dinfo;		/* w/Non-Manhattan geometry, "rect" is a
				 * triangle and dinfo holds side & direction
				 */
{
    TileType t, tt, tloc, dloc;
    TileTypeBitMask rmask, mmask, *tMask;

#ifdef NONMANHATTAN
    dloc = dinfo & (TT_DIAGONAL | TT_SIDE | TT_DIRECTION);
#else
    tloc = t;
#endif

    /* Remove any inactive layers */

    TTMaskAndMask3(&mmask, mask, &DBActiveLayerBits);

    /* If any residue of a contact is not in the active layers	*/
    /* list, then erase the contact in multiple passes.		*/
 
    for (t = TT_SELECTBASE; t < DBNumUserLayers; t++)
	if (TTMaskHasType(&mmask, t))
	    if (DBIsContact(t))
	    {
		tMask = DBResidueMask(t);
		TTMaskAndMask3(&rmask, tMask, &DBActiveLayerBits);
		if (TTMaskEqual(&rmask, tMask))
		{
#ifdef NONMANHATTAN
		    tloc = dloc | ((dinfo & TT_DIAGONAL) ? ((dinfo & TT_SIDE) ?
				(t << 14) : t) : t);
#endif
		    DBErase(cellDef, rect, tloc);
		}
		else if (!TTMaskIsZero(&rmask))
		{
		    for (tt = TT_SELECTBASE; tt < DBNumUserLayers; tt++)
			if (TTMaskHasType(&rmask, tt))
			{
#ifdef NONMANHATTAN
			    tloc = dloc | ((dinfo & TT_DIAGONAL) ?
					((dinfo & TT_SIDE) ?  (tt << 14) : tt) : tt);
			    DBErase(cellDef, rect, tloc);
#else		
			    DBErase(cellDef, rect, tt);
#endif
			}
		}
	    }
	    else
	    {
#ifdef NONMANHATTAN
		tloc = dloc | ((dinfo & TT_DIAGONAL) ? ((dinfo & TT_SIDE) ?
			 (t << 14) : t) : t);
#endif
		DBErase(cellDef, rect, tloc);
	    }
}
