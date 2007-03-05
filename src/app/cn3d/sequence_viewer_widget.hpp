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
* ===========================================================================
*/

#ifndef WX_SEQUENCE_VIEWER_WIDGET__HPP
#define WX_SEQUENCE_VIEWER_WIDGET__HPP

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/splitter.h>

class ViewableAlignment;

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
        eSelectBlocks,      // select whole blocks
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

    // make character visible (in sequence area) if it's not already; if either coordinate
    // is < 0, will not scroll in that direction
    void MakeCharacterVisible(int column, int row) const;

    // turn on/off the title area
    void TitleAreaOn(void);
    void TitleAreaOff(void);
    void TitleAreaToggle(void);

    void Refresh(bool eraseBackground = TRUE, const wxRect *rect = NULL);

private:
    SequenceViewerWidget_SequenceArea *sequenceArea;
    SequenceViewerWidget_TitleArea *titleArea;

    void OnDoubleClick(wxSplitterEvent& event);

    DECLARE_EVENT_TABLE()
};

#endif // WX_SEQUENCE_VIEWER_WIDGET__HPP
