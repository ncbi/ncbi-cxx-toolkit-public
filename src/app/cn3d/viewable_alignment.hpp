/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Authors:  Paul Thiessen
*
* File Description:
*      base class for information to be displayed in SequenceViewerWidget
*
* ===========================================================================
*/

#ifndef WX_VIEWABLE_ALIGNMENT__HPP
#define WX_VIEWABLE_ALIGNMENT__HPP

// This is the generic interface to a viewable alignment; it provides a minimal
// set of functions that are required to build the display. Any objects to be
// displayed in the SequenceViewerWidget must be derived from ViewableAlignment,
// and must implement these functions.

class wxString;
class wxColour;

class ViewableAlignment
{
public:
    virtual ~ViewableAlignment(void) { }
    
    // should set the overall size of the display, in columns (width) and rows (height)
    virtual void GetSize(unsigned int *columns, unsigned int *rows) const = 0;

    // should set a title string and color for a row; if the return value is false,
    // the title area will be left blank.
    virtual bool GetRowTitle(unsigned int row, wxString *title, wxColour *color) const = 0;

    // should set the character and its traits to display at the given location;
    // both columns and rows are numbered from zero. If the return value is false,
    // the cell for this character will be left blank. If drawBackground is true,
    // then the cell's background will be redrawn with the given color (e.g.,
    // for highlights).
    virtual bool GetCharacterTraitsAt(unsigned int column, unsigned int row,      // location
        char *character,                                        // character to draw
        wxColour *color,                                        // color of this character
        bool *drawBackground,                                   // special background color?
        wxColour *cellBackgroundColor                           // background color
    ) const = 0;


    ///// the following funtions are optional; they do nothing as-is, but can  /////
    ///// be overridden to provide some user-defined behaviour on these events /////

    // this is called when the mouse is moved over a cell; (-1,-1) means
    // the mouse is not over the character grid. This is only called called once
    // when the mouse enters a cell, or leaves the grid - it is not repeated if
    // the mouse is dragged around inside a single cell.
    virtual void MouseOver(int column, int row) const { }

    // these are used in the the following feedback functions to tell whether
    // control keys were down at the time of (the beginning of) selection. The
    // 'controls' item may be any bitwise combination of the following:
    enum eControlKeys {
        eShiftDown      = 0x01,
        eControlDown    = 0x02,
        eAltOrMetaDown  = 0x04
    };

    // this is called when the mouse-down event occurs (i.e., at the
    // beginning of selection), saying where the mouse was at the time and
    // with what control keys (see eControlKeys above). If 'false' is returned,
    // then no selection will ensue; if 'true', selection acts normally.
    virtual bool MouseDown(int column, int row, unsigned int controls) { return true; }

    // this is the callback when the the widget is in eSelect mode; it gives the
    // corners of the rectangle of cells selected.
    virtual void SelectedRectangle(
        int columnLeft, int rowTop,
        int columnRight, int rowBottom) { }

    // this is the callback when the widget is in eDrag mode; it gives two cells,
    // one where the mouse button-down occurred, and one where mouse-up occurred.
    virtual void DraggedCell(
        int columnFrom, int rowFrom,
        int columnTo, int rowTo) { }
};

#endif // WX_VIEWABLE_ALIGNMENT__HPP
