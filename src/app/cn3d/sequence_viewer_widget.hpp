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
* Revision 1.13  2000/11/02 16:48:23  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.12  2000/10/16 20:02:17  thiessen
* working block creation
*
* Revision 1.11  2000/10/12 16:39:31  thiessen
* fix MouseDown return value
*
* Revision 1.10  2000/10/12 16:22:37  thiessen
* working block split
*
* Revision 1.9  2000/10/05 18:34:35  thiessen
* first working editing operation
*
* Revision 1.8  2000/10/04 17:40:47  thiessen
* rearrange STL #includes
*
* Revision 1.7  2000/10/03 18:59:55  thiessen
* added row/column selection
*
* Revision 1.6  2000/10/02 23:25:08  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.5  2000/09/14 14:55:26  thiessen
* add row reordering; misc fixes
*
* Revision 1.4  2000/09/11 22:57:55  thiessen
* working highlighting
*
* Revision 1.3  2000/09/09 14:35:35  thiessen
* fix wxWin problem with large alignments
*
* Revision 1.2  2000/09/07 17:37:42  thiessen
* minor fixes
*
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
#include <wx/splitter.h>


// This is the generic interface to a viewable alignment; it provides a minimal
// set of functions that are required to build the display. Any objects to be
// displayed in the SequenceViewerWidget must be derived from ViewableAlignment,
// and must implement these functions.

class ViewableAlignment
{
public:
    // should set the overall size of the display, in columns (width) and rows (height)
    virtual void GetSize(int *columns, int *rows) const = 0;

    // should set a title string and color for a row; if the return value is false,
    // the title area will be left blank.
    virtual bool GetRowTitle(int row, wxString *title, wxColour *color) const = 0;

    // should set the character and its traits to display at the given location;
    // both columns and rows are numbered from zero. If the return value is false,
    // the cell for this character will be left blank. If drawBackground is true,
    // then the cell's background will be redrawn with the given color (e.g.,
    // for highlights).
    virtual bool GetCharacterTraitsAt(int column, int row,      // location
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


// This is the GUI class itself. It is a wxSplitterWindow - a viewing area where
// characters are drawn, with right and bottom scrollbars that will automatically
// be added if necessary. Note that it is *not* a complete top-level window!
// Thus, it can be embedded in other windows, and more than one can exist in a window.
// If you want a separate top-level sequence viewing window, then simply put this
// object into a wxFrame (with whatever menus you want to attach to it).

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

class SequenceViewerWidget_SequenceArea;
class SequenceViewerWidget_TitleArea;

class SequenceViewerWidget : public wxSplitterWindow
{
public:
    // constructor
    SequenceViewerWidget(
        wxWindow* parent,
        wxWindowID id = -1,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize
    );

    // destructor
    ~SequenceViewerWidget(void);

    // to provide a ViewableAlignment to show. It is passed as a pointer, but
    // is not "owned" by the SequenceViewerWidget, and will not be deconstructed
    // even if the SequenceViewerWidget is destroyed. Returns false on error.
    bool AttachAlignment(ViewableAlignment *newAlignment, int initX = 0, int initY = 0);

    // sets mouse mode. If eSelect, dragging the mouse with the left button
    // down will cause a rubberband to be drawn around a block of characters,
    // and the ViewableAlignment's SelectedRectangle() callback will be called
    // upon button-up. If eDrag, two individual cells are rubberbanded: one where
    // the mouse was upon button-down, and one where the mouse is while
    // being dragged; DraggedCell() will be called upon button-up. In any case,
    // selection is limited to coordinates given by ViewableAlignment's GetSize(),
    // even if the window is larger.
    enum eMouseMode {
        eSelectRectangle,   // select rectangle of cells
        eSelectColumns,     // select whole columns
        eSelectRows,        // select whole rows
        eDrag,              // move one cell
        eDragHorizontal,    // move one cell only horizontally
        eDragVertical       // move one cell only vertically
    };
    void SetMouseMode(eMouseMode mode);
    eMouseMode GetMouseMode(void) const;

    // sets the background color for the window after refresh
    void SetBackgroundColor(const wxColor& backgroundColor);

    // set the font for the characters; refreshes automatically. This font
    // object will be owned and destroyed by the SequenceViewerWidget
    void SetCharacterFont(wxFont *font);

    // set the rubber band color after refresh
    void SetRubberbandColor(const wxColor& rubberbandColor);

    // scroll sequence area to specific column or row; if either is -1, will
    // not scroll in that direction
    void ScrollTo(int column, int row);

    // get current uppermost row & leftmost column
    void GetScroll(int *vsX, int *vsY) const;

    // turn on/off the title area
    void TitleAreaOn(void);
    void TitleAreaOff(void);
    void TitleAreaToggle(void);
    
private:
    SequenceViewerWidget_SequenceArea *sequenceArea;
    SequenceViewerWidget_TitleArea *titleArea;

    void OnDoubleClickSash(int x, int y);
};

#endif // WX_SEQUENCE_VIEWER_WIDGET__HPP
