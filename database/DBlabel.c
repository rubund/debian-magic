/*
 * DBlabel.c --
 *
 * Label manipulation primitives.
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
static char rcsid[] __attribute__ ((unused)) = "$Header: /usr/cvsroot/magic-7.5/database/DBlabel.c,v 1.10 2008/02/10 19:06:31 tim Exp $";
#endif  /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/magic.h"
#include "utils/malloc.h"
#include "utils/geometry.h"
#include "tiles/tile.h"
#include "utils/hash.h"
#include "utils/utils.h"
#include "database/database.h"
#include "database/fonts.h"
#include "database/databaseInt.h"
#include "windows/windows.h"
#include "dbwind/dbwind.h"
#include "commands/commands.h"
#include "textio/textio.h"

static TileType DBPickLabelLayer(/* CellDef *def, Label *lab, int noreconnect */);

/* Globally-accessible font information */

MagicFont **DBFontList = NULL;
int DBNumFonts = 0;

/*
 * ----------------------------------------------------------------------------
 *
 * DBIsSubcircuit --
 *
 * Check if any labels in a CellDef declare port attributes, indicating
 * that the CellDef should be treated as a subcircuit.
 *
 * Results:
 *	TRUE if CellDef contains ports, FALSE otherwise.
 *
 * Side effects:
 *	None.
 *
 * ----------------------------------------------------------------------------
 */

bool
DBIsSubcircuit(cellDef)
    CellDef *cellDef;
{
    Label *lab;

    for (lab = cellDef->cd_labels; lab != NULL; lab = lab->lab_next)
	if (lab->lab_flags & PORT_DIR_MASK)
	    return TRUE;
 
    return FALSE;
}


/*
 * ----------------------------------------------------------------------------
 *
 * DBPutLabel --
 *
 * Place a rectangular label in the database, in a particular cell.
 *
 * It is the responsibility of higher-level routines to insure that
 * the material to which the label is being attached really exists at
 * this point in the cell, and that TT_SPACE is used if there is
 * no single material covering the label's entire area.  The routine
 * DBAdjustLabels is useful for this.
 *
 * Results:
 *	The return value is the actual alignment position used for
 *	the label.  This may be different from align, if align is
 *	defaulted.
 *
 * Side effects:
 *	Updates the label list in the CellDef to contain the label.
 *
 * ----------------------------------------------------------------------------
 */
int
DBPutLabel(cellDef, rect, align, text, type, flags)
    CellDef *cellDef;	/* Cell in which label is placed */
    Rect *rect;		/* Location of label; see above for description */
    int align;		/* Orientation/alignment of text.  If this is < 0,
			 * an orientation will be picked to keep the text
			 * inside the cell boundary.
			 */
    char *text;		/* Pointer to actual text of label */
    TileType type;	/* Type of tile to be labelled */
    int flags;		/* Label flags */
{
    Label *lab;
    int len, x1, x2, y1, y2, tmp, labx, laby;

    len = strlen(text) + sizeof (Label) - sizeof lab->lab_text + 1;
    lab = (Label *) mallocMagic ((unsigned) len);
    strcpy(lab->lab_text, text);

    /* Pick a nice alignment if the caller didn't give one.  If the
     * label is more than BORDER units from an edge of the cell,
     * use GEO_NORTH.  Otherwise, put the label on the opposite side
     * from the boundary, so it won't stick out past the edge of
     * the cell boundary.
     */
    
#define BORDER 5
    if (align < 0)
    {
	tmp = (cellDef->cd_bbox.r_xtop - cellDef->cd_bbox.r_xbot)/3;
	if (tmp > BORDER) tmp = BORDER;
	x1 = cellDef->cd_bbox.r_xbot + tmp;
	x2 = cellDef->cd_bbox.r_xtop - tmp;
	tmp = (cellDef->cd_bbox.r_ytop - cellDef->cd_bbox.r_ybot)/3;
	if (tmp > BORDER) tmp = BORDER;
	y1 = cellDef->cd_bbox.r_ybot + tmp;
	y2 = cellDef->cd_bbox.r_ytop - tmp;
	labx = (rect->r_xtop + rect->r_xbot)/2;
	laby = (rect->r_ytop + rect->r_ybot)/2;

	if (labx <= x1)
	{
	    if (laby <= y1) align = GEO_NORTHEAST;
	    else if (laby >= y2) align = GEO_SOUTHEAST;
	    else align = GEO_EAST;
	}
	else if (labx >= x2)
	{
	    if (laby <= y1) align = GEO_NORTHWEST;
	    else if (laby >= y2) align = GEO_SOUTHWEST;
	    else align = GEO_WEST;
	}
	else
	{
	    if (laby <= y1) align = GEO_NORTH;
	    else if (laby >= y2) align = GEO_SOUTH;
	    else align = GEO_NORTH;
	}
    }

    lab->lab_just = align;
    lab->lab_type = type;
    lab->lab_flags = flags;
    lab->lab_rect = *rect;
    lab->lab_next = NULL;
    if (cellDef->cd_labels == NULL)
	cellDef->cd_labels = lab;
    else
    {
	ASSERT(cellDef->cd_lastLabel->lab_next == NULL, "DBPutLabel");
	cellDef->cd_lastLabel->lab_next = lab;
    }
    cellDef->cd_lastLabel = lab;

    DBUndoPutLabel(cellDef, rect, align, text, type, flags);
    cellDef->cd_flags |= CDMODIFIED|CDGETNEWSTAMP;
    return align;
}

/*
 * ----------------------------------------------------------------------------
 *
 * DBEraseLabel --
 *
 * Delete labels attached to tiles of the indicated types that
 * are in the given area (as determined by the macro GEO_LABEL_IN_AREA).  
 * If this procedure is called as part of a command that also modifies paint, 
 * then the paint modifications should be done BEFORE calling here.
 *
 * Results:
 *	TRUE if any labels were deleted, FALSE otherwise.
 *
 * Side effects:
 *	This procedure tries to be clever in order to avoid deleting
 *	labels whenever possible.  If there's enough material on the
 *	label's attached layer so that the label can stay on its
 *	current layer, or if the label can be migrated to a layer that
 *	connects to its current layer, then the label is not deleted.
 *	Deleting up to the edge of a label won't cause the label
 *	to go away.  There's one final exception:  if the mask includes
 *	L_LABEL, then labels are deleted from all layers even if there's
 *	still enough material to keep them around.
 *
 * ----------------------------------------------------------------------------
 */

bool
DBEraseLabel(cellDef, area, mask)
    CellDef *cellDef;		/* Cell being modified */
    Rect *area;			/* Area from which labels are to be erased.
				 * This may be a point; any labels touching
				 * or overlapping it are erased.
				 */
    TileTypeBitMask *mask;	/* Mask of types from which labels are to
				 * be erased.
				 */
{
    Label *lab, *labPrev;
    bool erasedAny = FALSE;
    TileType newType;

    labPrev = NULL;
    lab = cellDef->cd_labels;
    while (lab != NULL)
    {
	if (!GEO_LABEL_IN_AREA(&lab->lab_rect, area)) goto nextLab;
	if (!TTMaskHasType(mask, L_LABEL))
	{
	    if (!TTMaskHasType(mask, lab->lab_type)) goto nextLab;

	    /* Labels on space always get deleted at this point, since
	     * there's no reasonable new layer to put them on.
	     */
	    if (!(lab->lab_type == TT_SPACE))
	    {
		newType = DBPickLabelLayer(cellDef, lab, 0);
		if (DBConnectsTo(newType, lab->lab_type)) goto nextLab;
	    }
	}

	if (labPrev == NULL)
	    cellDef->cd_labels = lab->lab_next;
	else labPrev->lab_next = lab->lab_next;
	if (cellDef->cd_lastLabel == lab)
	    cellDef->cd_lastLabel = labPrev;
	DBUndoEraseLabel(cellDef, &lab->lab_rect, lab->lab_just,
	    lab->lab_text, lab->lab_type, lab->lab_flags);
	DBWLabelChanged(cellDef, lab->lab_text, &lab->lab_rect,
	    lab->lab_just, DBW_ALLWINDOWS);
	freeMagic((char *) lab);
	lab = lab->lab_next;
	erasedAny = TRUE;
	continue;

	nextLab: labPrev = lab;
	lab = lab->lab_next;
    }

    if (erasedAny)
	cellDef->cd_flags |= CDMODIFIED|CDGETNEWSTAMP;
    return (erasedAny);
}

/*
 * ----------------------------------------------------------------------------
 *
 * DBEraseLabelsByContent --
 *
 * Erase any labels found on the label list for the given
 * CellDef that match the given specification.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies the label list for the argument CellDef.  The
 *	DBWind module is notified about any labels that were
 *	deleted.
 *
 * ----------------------------------------------------------------------------
 */

void
DBEraseLabelsByContent(def, rect, pos, type, text)
    CellDef *def;		/* Where to look for label to delete. */
    Rect *rect;			/* Coordinates of label.  If NULL, then
				 * labels are deleted regardless of coords.
				 */
    int pos;			/* Position of label.  If < 0, then
				 * labels are deleted regardless of position.
				 */
    TileType type;		/* Layer label is attached to.  If < 0, then
				 * labels are deleted regardless of type.
				 */
    char *text;			/* Text associated with label.  If NULL, then
				 * labels are deleted regardless of text.
				 */
{
    Label *lab, *labPrev;

#define	RECTEQUAL(r1, r2)	  ((r1)->r_xbot == (r2)->r_xbot \
				&& (r1)->r_ybot == (r2)->r_ybot \
				&& (r1)->r_xtop == (r2)->r_xtop \
				&& (r1)->r_ytop == (r2)->r_ytop)

    for (labPrev = NULL, lab = def->cd_labels;
	    lab != NULL;
	    labPrev = lab, lab = lab->lab_next)
    {
	nextCheck:
	if ((rect != NULL) && !(RECTEQUAL(&lab->lab_rect, rect))) continue;
	if ((type >= 0) && (type != lab->lab_type)) continue;
	if ((pos >= 0) && (pos != lab->lab_just)) continue;
	if ((text != NULL) && (strcmp(text, lab->lab_text) != 0)) continue;
	DBUndoEraseLabel(def, &lab->lab_rect, lab->lab_just,
	    lab->lab_text, lab->lab_type, lab->lab_flags);
	DBWLabelChanged(def, lab->lab_text, &lab->lab_rect,
	    lab->lab_just, DBW_ALLWINDOWS);
	if (labPrev == NULL)
	    def->cd_labels = lab->lab_next;
	else labPrev->lab_next = lab->lab_next;
	if (def->cd_lastLabel == lab)
	    def->cd_lastLabel = labPrev;
	freeMagic((char *) lab);

	/* Don't iterate through loop, since this will skip a label:
	 * just go back to top.  This is tricky!
	 */

	lab = lab->lab_next;
	if (lab == NULL) break;
	else goto nextCheck;
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * DBEraseLabelsByFunction --
 *
 * Erase any labels found on the label list for which the function returns
 * TRUE.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies the label list for the argument CellDef.  The
 *	DBWind module is notified about any labels that were
 *	deleted.
 *
 * ----------------------------------------------------------------------------
 */

void
DBEraseLabelsByFunction(def, func)
    CellDef *def;		/* Where to look for label to delete. */
    bool (*func)();		/* Function to call for each label.  If it
				 * returns TRUE, we delete the label.
				 *
				 * Function should be of the form:
				 *
				 *	bool func(lab)
				 *	    Label *lab;
				 *	{
				 *	    return XXX;
				 *	}
				 */
{
    Label *lab, *labPrev;

    for (labPrev = NULL, lab = def->cd_labels;
	    lab != NULL;
	    labPrev = lab, lab = lab->lab_next)
    {
	nextCheck:
	if (!(*func)(lab)) continue;
	DBUndoEraseLabel(def, &lab->lab_rect, lab->lab_just,
	    lab->lab_text, lab->lab_type, lab->lab_flags);
	DBWLabelChanged(def, lab->lab_text, &lab->lab_rect,
	    lab->lab_just, DBW_ALLWINDOWS);
	if (labPrev == NULL)
	    def->cd_labels = lab->lab_next;
	else labPrev->lab_next = lab->lab_next;
	if (def->cd_lastLabel == lab)
	    def->cd_lastLabel = labPrev;
	freeMagic((char *) lab);

	/* Don't iterate through loop, since this will skip a label:
	 * just go back to top.  This is tricky!
	 */

	lab = lab->lab_next;
	if (lab == NULL) break;
	else goto nextCheck;
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * DBReOrientLabel --
 *
 * 	Change the text positions of all labels underneath a given
 *	area in a given cell.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 * ----------------------------------------------------------------------------
 */

void
DBReOrientLabel(cellDef, area, newPos)
    CellDef *cellDef;		/* Cell whose labels are to be modified. */
    Rect *area;			/* All labels touching this area have their
				 * text positions changed.
				 */
    int newPos;			/* New text positions for all labels in
				 * the area, for example, GEO_NORTH.
				 */
{
    Label *lab;

    for (lab = cellDef->cd_labels; lab != NULL; lab = lab->lab_next)
    {
	if (GEO_TOUCH(area, &lab->lab_rect))
	{
	    DBUndoEraseLabel(cellDef, &lab->lab_rect, lab->lab_just,
		lab->lab_text, lab->lab_type, lab->lab_flags);
	    DBWLabelChanged(cellDef, lab->lab_text, &lab->lab_rect,
		lab->lab_just, DBW_ALLWINDOWS);
	    lab->lab_just = newPos;
	    DBUndoPutLabel(cellDef, &lab->lab_rect, lab->lab_just,
		lab->lab_text, lab->lab_type, lab->lab_flags);
	    DBWLabelChanged(cellDef, lab->lab_text, &lab->lab_rect,
		lab->lab_just, DBW_ALLWINDOWS);
	}
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * DBAdjustLabels --
 *
 * 	This procedure is called after paint has been modified
 *	in an area.  It finds all labels overlapping that area,
 *	and adjusts the layers they are attached to to reflect
 *	the changes in paint.  Thus, a layer will automatically
 *	migrate from poly to poly-metal-contact and back to
 *	poly if the contact layer is painted and then erased.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The layer attachments of labels may change.  For each
 *	such change, a message is output.
 *
 * ----------------------------------------------------------------------------
 */

void
DBAdjustLabels(def, area)
    CellDef *def;		/* Cell whose paint was changed. */
    Rect *area;			/* Area where paint was modified. */
{
    Label *lab;
    TileType newType;
    bool modified = FALSE;

    /* First, find each label that crosses the area we're
     * interested in.
     */
    
    for (lab = def->cd_labels; lab != NULL; lab = lab->lab_next)
    {
	if (!GEO_TOUCH(&lab->lab_rect, area)) continue;
	newType = DBPickLabelLayer(def, lab, 0);
	if (newType == lab->lab_type) continue;
	if (DBVerbose && ((def->cd_flags & CDINTERNAL) == 0)) {
	    TxPrintf("Moving label \"%s\" from %s to %s in cell %s.\n",
		    lab->lab_text, DBTypeLongName(lab->lab_type),
		    DBTypeLongName(newType), def->cd_name);
	};
	DBUndoEraseLabel(def, &lab->lab_rect, lab->lab_just,
		lab->lab_text, lab->lab_type, lab->lab_flags);
	lab->lab_type = newType;
	DBUndoPutLabel(def, &lab->lab_rect, lab->lab_just,
		lab->lab_text, lab->lab_type, lab->lab_flags);
	modified = TRUE;
    }

    if (modified) DBCellSetModified(def, TRUE);
}


/*
 * Extended version of DBAdjustLabels.  If noreconnect==0, 
 * this is supposed to be the same as DBAdjustlabels() above.
 */
void
DBAdjustLabelsNew(def, area, noreconnect)
    CellDef *def;	/* Cell whose paint was changed. */
    Rect *area;		/* Area where paint was modified. */
    int noreconnect; 	/* if 1, don't move label to a type that doesn't
			 * connect to the original type, delete instead
			 */
{
    Label *lab, *labPrev;
    TileType newType;
    bool modified = FALSE;

    /* First, find each label that crosses the area we're
     * interested in.
     */
    
    labPrev = NULL;
    lab = def->cd_labels;
    while (lab != NULL)
    {
	    if (!GEO_TOUCH(&lab->lab_rect, area)) {
		    goto nextLab;
	    }
	    newType = DBPickLabelLayer(def, lab, noreconnect);
	    if (newType == lab->lab_type) {
		    goto nextLab;
	    } 
	    if(newType < 0) {
		    TxPrintf("Deleting ambiguous-layer label \"%s\" from %s in cell %s.\n",
			     lab->lab_text, DBTypeLongName(lab->lab_type),
			     def->cd_name);
	    
		    if (labPrev == NULL)
			    def->cd_labels = lab->lab_next;
		    else 
			    labPrev->lab_next = lab->lab_next;
		    if (def->cd_lastLabel == lab)
			    def->cd_lastLabel = labPrev;
		    DBUndoEraseLabel(def, &lab->lab_rect, lab->lab_just,
				     lab->lab_text, lab->lab_type, lab->lab_flags);
		    DBWLabelChanged(def, lab->lab_text, &lab->lab_rect,
				    lab->lab_just, DBW_ALLWINDOWS);
		    freeMagic((char *) lab);
		    lab = lab->lab_next;
		    modified = TRUE;
		    continue;
	    } else {
		    if (DBVerbose && ((def->cd_flags & CDINTERNAL) == 0)) {
			    TxPrintf("Moving label \"%s\" from %s to %s in cell %s.\n",
				     lab->lab_text, DBTypeLongName(lab->lab_type),
				     DBTypeLongName(newType), def->cd_name);
		    }
		    DBUndoEraseLabel(def, &lab->lab_rect, lab->lab_just,
				     lab->lab_text, lab->lab_type, lab->lab_flags);
		    lab->lab_type = newType;
		    DBUndoPutLabel(def, &lab->lab_rect, lab->lab_just,
				   lab->lab_text, lab->lab_type, lab->lab_flags);
		    modified = TRUE;
	    }
    nextLab: 
	    labPrev = lab;
	    lab = lab->lab_next;
    }

    if (modified) DBCellSetModified(def, TRUE);
}


/*
 * ----------------------------------------------------------------------------
 *
 * DBPickLabelLayer --
 *
 * 	This procedure looks at the material around a label and
 *	picks a new layer for the label to be attached to.
 *
 * Results:
 *	Returns a tile type, which is a layer that completely
 *	covers the label's area.  If possible, the label's current
 *	layer is chosen, or if that's not possible, then a layer
 *	that connects to the label's current layer, or else any
 *	other mask layer.  If everything fails, TT_SPACE is returned.
 *
 * Side effects:
 *	None.  The label's layer is not changed by this procedure.
 *
 * ----------------------------------------------------------------------------
 */

/* The following variable(s) are shared between DBPickLabelLayer and
 * its search function dbPickFunc2.
 */

TileTypeBitMask *dbAdjustPlaneTypes;	/* Mask of all types in current
					 * plane being searched.
					 */
TileType
DBPickLabelLayer(def, lab, noreconnect)
    CellDef *def;		/* Cell definition containing label. */
    Label *lab;			/* Label for which a home must be found. */
    int noreconnect;        /* if 1, return -1 if rule 5 or 6 would succeed */
{
    TileTypeBitMask types[3], types2[3];
    Rect check1, check2;
    int i, plane;
    TileType choice1, choice2, choice3, choice4, choice5, choice6;
    extern int dbPickFunc1(), dbPickFunc2();

    /* Compute an array of three tile type masks.  The first is for
     * all of the types that are present everywhere underneath the
     * label. The second is for all types that are components of
     * tiles that completely cover the label's area.  The third
     * is for tile types that touch the label anywhere.  To make this
     * work correctly we have to consider four cases separately:
     * point labels, horizontal line labels, vertical line labels,
     * and rectangular labels.
     */

    if ((lab->lab_rect.r_xbot == lab->lab_rect.r_xtop)
	    && (lab->lab_rect.r_ybot == lab->lab_rect.r_ytop))
    {
	/* Point label.  Find out what layers touch the label and
	 * use this for all three masks.
	 */

	GEO_EXPAND(&lab->lab_rect, 1, &check1);
	types[0] = DBZeroTypeBits;
	for (i = PL_SELECTBASE; i < DBNumPlanes; i += 1)
	{
	    (void) DBSrPaintArea((Tile *) NULL, def->cd_planes[i],
		    &check1, &DBAllTypeBits, dbPickFunc1,
		    (ClientData) &types[0]);
	}
	types[1] = types[0];
	types[2] = types[0];
    }
    else if (lab->lab_rect.r_xbot == lab->lab_rect.r_xtop)
    {
	/* Vertical line label.  Search two areas, one on the
	 * left and one on the right.  For each side, compute
	 * the type arrays separately.  Then merge them together.
	 */
	
	check1 = lab->lab_rect;
	check2 = lab->lab_rect;
	check1.r_xbot -= 1;
	check2.r_xtop += 1;

	twoAreas:
	types[0] = DBAllButSpaceAndDRCBits;
	types[1] = DBAllButSpaceAndDRCBits;
	TTMaskZero(&types[2]);
	types2[0] = DBAllButSpaceAndDRCBits;
	types2[1] = DBAllButSpaceAndDRCBits;
	TTMaskZero(&types2[2]);
	for (i = PL_SELECTBASE; i < DBNumPlanes; i += 1)
	{
	    dbAdjustPlaneTypes = &DBPlaneTypes[i];
	    (void) DBSrPaintArea((Tile *) NULL, def->cd_planes[i],
		    &check1, &DBAllTypeBits, dbPickFunc2,
		    (ClientData) types);
	    (void) DBSrPaintArea((Tile *) NULL, def->cd_planes[i],
		    &check2, &DBAllTypeBits, dbPickFunc2,
		    (ClientData) types2);
	}
	TTMaskSetMask(&types[0], &types2[0]);
	TTMaskSetMask(&types[1], &types2[1]);
	TTMaskSetMask(&types[2], &types2[2]);
    }
    else if (lab->lab_rect.r_ybot == lab->lab_rect.r_ytop)
    {
	/* Horizontal line label.  Search two areas, one on the
	 * top and one on the bottom.  Use the code from above
	 * to handle.
	 */
	
	check1 = lab->lab_rect;
	check2 = lab->lab_rect;
	check1.r_ybot -= 1;
	check2.r_ytop += 1;
	goto twoAreas;
    }
    else
    {
	/* This is a rectangular label.  Same thing as for line labels,
	 * except there's only one area to search.
	 */
	
	types[0] = DBAllButSpaceAndDRCBits;
	types[1] = DBAllButSpaceAndDRCBits;
	TTMaskZero(&types[2]);
	for (i = PL_SELECTBASE; i < DBNumPlanes; i += 1)
	{
	    dbAdjustPlaneTypes = &DBPlaneTypes[i];
	    (void) DBSrPaintArea((Tile *) NULL, def->cd_planes[i],
		    &lab->lab_rect, &DBAllTypeBits, dbPickFunc2,
		    (ClientData) types);
	}
    }

    /* If the label's layer covers the label's area, use it.
     * Otherwise, look for a layer in the following order:
     * 1. A layer on the same plane as the original layer and that
     *    covers the label and connects to its original layer.
     * 2. A layer on the same plane as the original layer and that
     *    is a component of material that covers the label and
     *    connects to its original layer.
     * 3. A layer that covers the label and connects to the
     *    old layer.
     * 4. A layer that is a component of material that covers
     *    the label and connects to the old layer.
     * 5. A layer that covers the label.
     * 6. A layer that is a component of material that covers the label.
     * 7. Space.
     */
    
    if (TTMaskHasType(&types[0], lab->lab_type)) return lab->lab_type;
    plane = DBPlane(lab->lab_type);
    choice1 = choice2 = choice3 = choice4 = choice5 = choice6 = TT_SPACE;
    for (i = TT_SELECTBASE; i < DBNumUserLayers; i += 1)
    {
	if (!TTMaskHasType(&types[2], i)) continue;
	if (DBConnectsTo(i, lab->lab_type))
	{
	    if (DBPlane(i) == plane)
	    {
		if (TTMaskHasType(&types[0], i))
		{
		    choice1 = i;
		    continue;
		}
		else if (TTMaskHasType(&types[1], i))
		{
		    choice2 = i;
		    continue;
		}
	    }
	    if (TTMaskHasType(&types[0], i))
	    {
		choice3 = i;
		continue;
	    }
	    else if (TTMaskHasType(&types[1], i))
	    {
		choice4 = i;
		continue;
	    }
	}
	if (TTMaskHasType(&types[0], i))
	{
	    /* A type that connects to more than itself is preferred */
	    if (choice5 == TT_SPACE)
		choice5 = i;
	    else
	    {
		TileTypeBitMask ctest;
		TTMaskZero(&ctest);
		TTMaskSetMask(&ctest, &DBConnectTbl[i]);
		TTMaskClearType(&ctest, i);
		if (!TTMaskIsZero(&ctest))
		    choice5 = i;
		else if (TTMaskHasType(&types[1], i))
		    choice6 = i;
	    }
	    continue;
	}
	else if (TTMaskHasType(&types[1], i))
	{
	    choice6 = i;
	    continue;
	}
    }

    if (choice1 != TT_SPACE) return choice1;
    else if (choice2 != TT_SPACE) return choice2;
    else if (choice3 != TT_SPACE) return choice3;
    else if (choice4 != TT_SPACE) return choice4;
    else if (noreconnect) {
#ifdef notdef
	TxPrintf("DBPickLabelLayer \"%s\" (on %s at %d,%d) choice4=%s choice5=%s choice6=%s.\n",
		     lab->lab_text, 
		     DBTypeLongName(lab->lab_type),
		     lab->lab_rect.r_xbot,
		     lab->lab_rect.r_ytop,
		     DBTypeLongName(choice4),
		     DBTypeLongName(choice5),
		     DBTypeLongName(choice6));
#endif
	/* If the flag is set, don't cause a netlist change by moving a
	   the label.  So unless there's only space here, delete the label */
	if(choice5 == TT_SPACE && choice6 == TT_SPACE)
	    return TT_SPACE;
	else
 	    return -1;
    }
    else if (choice5 != TT_SPACE) return choice5;
    else return choice6;
}

/* Search function for DBPickLabelLayer:  just OR in the type of
 * any tiles (except space) to the mask passed as clientdata.
 * Always return 0 to keep the search alive.
 */

int
dbPickFunc1(tile, mask)
    Tile *tile;			/* Tile found. */
    TileTypeBitMask *mask;	/* Mask to be modified. */
{
    TileType type;

#ifdef NONMANHATTAN 
    if (IsSplit(tile)) 
	type = (SplitSide(tile)) ? SplitRightType(tile) : SplitLeftType(tile);
    else
#endif
    type = TiGetType(tile);

    if (type == TT_SPACE) return 0;
    TTMaskSetType(mask, type);
    return 0;
}

/* Another search function for DBPickLabelLayer.  For the first element
 * in the mask array, AND off all types on the current plane except the
 * given type.  For the second element, AND off all types on the current
 * plane except the ones that are components of this tile's type.  For
 * the third element of the array, just OR in the type of the current
 * tile.  A space tile ruins the whole plane so return 1 to abort the
 * search.  Otherwise return 0.
 */

int
dbPickFunc2(tile, mask)
    Tile *tile;			/* Tile found. */
    TileTypeBitMask *mask;	/* Mask to be modified. */
{
    TileType type;
    TileTypeBitMask tmp, *rMask;

#ifdef NONMANHATTAN 
    if (IsSplit(tile)) 
	type = (SplitSide(tile)) ? SplitRightType(tile) : SplitLeftType(tile);
    else
#endif
    type = TiGetType(tile);

    if (type == TT_SPACE)
    {
	/* Space means can't have any tile types on this plane. */

	TTMaskClearMask(&mask[0], dbAdjustPlaneTypes);
	TTMaskClearMask(&mask[1], dbAdjustPlaneTypes);
	return 1;
    }

    tmp = *dbAdjustPlaneTypes;
    TTMaskClearType(&tmp, type);
    TTMaskClearMask(&mask[0], &tmp);
    rMask = DBResidueMask(type);
    TTMaskClearMask(&tmp, rMask);
    TTMaskClearMask(&mask[1], &tmp);
    TTMaskSetType(&mask[2], type);
    return 0;
}

