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

    void OnDoubleClickSash(int x, int y);
};

#endif // WX_SEQUENCE_VIEWER_WIDGET__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.21  2005/04/22 13:43:01  thiessen
* add block highlighting and structure alignment based on highlighted positions only
*
* Revision 1.20  2003/02/03 19:20:06  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.19  2002/10/07 13:29:32  thiessen
* add double-click -> show row to taxonomy tree
*
* Revision 1.18  2002/08/15 22:13:16  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.17  2002/02/05 18:53:25  thiessen
* scroll to residue in sequence windows when selected in structure window
*
* Revision 1.16  2001/05/22 19:09:10  thiessen
* many minor fixes to compile/run on Solaris/GTK
*
* Revision 1.15  2001/05/03 14:38:32  thiessen
* put ViewableAlignment in its own (non-wx) header
*
* Revision 1.14  2001/03/17 14:06:52  thiessen
* more workarounds for namespace/#define conflicts
*
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
*/
