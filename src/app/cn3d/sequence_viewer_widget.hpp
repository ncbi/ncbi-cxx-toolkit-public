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
*      Classes to display sequences and alignments with wxWindows front end
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/09/03 18:45:57  thiessen
* working generalized sequence viewer
*
* Revision 1.2  2000/08/30 23:45:36  thiessen
* working alignment display
*
* Revision 1.1  2000/08/30 19:49:05  thiessen
* working sequence window
*
* ===========================================================================
*/

#ifndef WX_SEQUENCE_VIEWER_WIDGET__HPP
#define WX_SEQUENCE_VIEWER_WIDGET__HPP

#include <wx/wx.h>


// This is the generic interface to a viewable alignment; it provides a minimal
// set of functions that are required to build the display. Any objects to be
// displayed in the SequenceViewerWidget must be derived from ViewableAlignment,
// and must implement these functions.

class ViewableAlignment
{
public:
    // should set the overall size of the display, in columns (width) and rows (height)
    virtual void GetSize(int *columns, int *rows) const = 0;

    // should set the character and its traits to display at the given location;
    // both columns and rows are numbered from zero. If the return value is false,
    // the cell for this character will be left blank. If 'isHighlighted' is set
    // to true, the character will be drawn with the character color against
    // a highlight-color background.
    virtual bool GetCharacterTraitsAt(int column, int row,      // location
        char *character,                                        // character to draw
        wxColour *color,                                        // color of this character
        bool *isHighlighted                                     // highlighed residue?
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

    // this is the callback when the the widget is in eSelect mode; it gives the
    // corners of the rectangle of cells selected.
    virtual void SelectedRectangle(
        int columnLeft, int rowTop, 
        int columnRight, int rowBottom,
        unsigned int controls) const { }

    // this is the callback when the widget is in eDrag mode; it gives two cells,
    // one where the mouse button-down occurred, and one where mouse-up occurred.
    virtual void DraggedCell(
        int columnFrom, int rowFrom,
        int columnTo, int rowTo,
        unsigned int controls) const { }
};


// This is the GUI class itself. It is a wxScrolledWindow - a viewing area where
// characters are drawn, with right and bottom scrollbars that will automatically
// be added if necessary. Note that it is *not* a complete top-level window!
// If you want a separate sequence viewing window, then simply put this object
// into a wxFrame (with whatever menus you want to attach to it).

// A note on memory and processor efficiency: the SequenceViewerWidget does *not*
// keep an entire array of characters in memory, nor does it keep a canvas of the
// entire pixel image of the alignment in memory, thus it should be able to display
// large alignments without incurring NxN memory usage. However, this means that
// the display must be created dynamically, by calls to the ViewableAlignment's
// member functions, as appropriate. It only calls these functions on the
// minimum number of positions needed to fill out the display, including on
// scrolling operations: if you scroll the view one column to the right, only
// the rightmost column is redrawn, and thus the number of calls to GetCharacterAt()
// is equal only to the number of rows in the visible region. Hopefully this will
// provide a reasonable compromise between memory and CPU efficiency.

class SequenceViewerWidget : public wxScrolledWindow
{
public:
    // constructor (same as wxScrolledWindow)
    SequenceViewerWidget(
        wxWindow* parent,
        wxWindowID id = -1,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxHSCROLL | wxVSCROLL,
        const wxString& name = "scrolledWindow"
    );

    // to provide a ViewableAlignment to show. It is passed as a pointer, but
    // is not "owned" by the SequenceViewerWidget, and will not be deconstructed
    // even if the SequenceViewerWidget is destroyed. Returns false on error.
    bool AttachAlignment(const ViewableAlignment *newAlignment);

    // sets mouse mode. If eSelect, dragging the mouse with the left button
    // down will cause a rubberband to be drawn around a block of characters,
    // and the ViewableAlignment's SelectedRectangle() callback will be called
    // upon button-up. If eDrag, two individual cells are rubberbanded: one where
    // the mouse was when upon button-down, and one where the mouse is while
    // being dragged; DraggedCell() will be called upon button-up. In any case,
    // selection is limited to coordinates given by ViewavleAlignment's GetSize(),
    // even if window is larger.
    enum eMouseMode {
        eSelect,            // select rectangle of cells
        eDrag,              // move one cell
        eDragHorizontal,    // move one cell only horizontally
        eDragVertical       // move one cell only vertically
    };
    void SetMouseMode(eMouseMode mode);

    // sets the background color for the window
    void SetBackgroundColor(const wxColor& backgroundColor);

    // set the background color for highlighted characters; does not refresh -
    // must refresh manually to see immediate change
    void SetHighlightColor(const wxColor& highlightColor);

    // set the font for the characters; refreshes immediately
    void SetCharacterFont(const wxFont& font);

    // set the rubber band color
    void SetRubberBandColor(const wxColor& rubberBandColor);
    
    ///// everything else below here is part of the actual implementation /////
private:

    void OnDraw(wxDC& dc);
    void OnMouseEvent(wxMouseEvent& event);

    const ViewableAlignment *alignment;

    wxFont currentFont;             // character font
    wxColor currentBackgroundColor; // background for whole window
    wxColor currentHighlightColor;  // background for highlighted chars
    wxColor currentRubberBandColor; // color of rubber band
    eMouseMode mouseMode;           // mouse mode
    int cellWidth, cellHeight;      // dimensions of cells in pixels
    int areaWidth, areaHeight;      // dimensions of the alignment (virtual area) in cells

    enum eRubberBandType {
        eSolid,
        eDot
    };
    eRubberBandType currentRubberBandType;
    void DrawLine(wxDC& dc, int x1, int y1, int x2, int y2);

    void DrawCell(wxDC& dc, int x, int y, bool redrawBackground = false); // draw a single cell
    void DrawRubberBand(wxDC& dc,                           // draw rubber band around cells
        int fromX, int fromY, int toX, int toY);
    void MoveRubberBand(wxDC &dc, int fromX, int fromY,     // change rubber band
        int prevToX, int prevToY, int toX, int toY);
    void RemoveRubberBand(wxDC& dc, int fromX, int fromY,   // remove rubber band
        int toX, int toY);

    DECLARE_EVENT_TABLE()
};

#endif // WX_SEQUENCE_VIEWER_GUI__HPP
