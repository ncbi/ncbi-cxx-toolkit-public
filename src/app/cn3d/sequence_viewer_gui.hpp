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
* Revision 1.2  2000/08/30 23:45:36  thiessen
* working alignment display
*
* Revision 1.1  2000/08/30 19:49:05  thiessen
* working sequence window
*
* ===========================================================================
*/

#ifndef CN3D_SEQUENCE_VIEWER_GUI__HPP
#define CN3D_SEQUENCE_VIEWER_GUI__HPP

// this header should only be #included from sequence_viewer.cpp. For general
// sequence viewer interface functions, use sequence_viewer.hpp. The first
// include file in sequence_viewer.cpp is <wx/wx.h>, so this should already
// have been set up before this header is read. This also (intentionally)
// prevents this header from being used by other modules.

#include <vector>

#include <corelib/ncbiobj.hpp>


class Cn3D::Sequence;
class Cn3D::SequenceViewer;
class SequenceViewerGUI;

// this is the class that holds the sequences/alignment formatted for display -
// the equivalent of DDV's ParaG. 
class SequenceDisplay
{
    friend class SequenceDrawingArea;

public:
    SequenceDisplay(void);
    ~SequenceDisplay(void);

    // each Cell is a character to display, and an index into the sequence;
    // we could just look up the character from the index, but it's more
    // efficient just to set it statically, as characters don't change once
    // the display is created. (Color does change, so that will be looked up
    // dynamically.) The index must be -1 if this isn't a sequence cell (e.g.
    // a separator, ruler, gap, identifier, etc.).
    typedef struct {
        char character;
        int index;
    } Cell;
    typedef std::vector < Cell > RowOfCells;

    // add a (pre-constructed) RowOfCells to this display. The row passed in will
    // be "owned" by the SequenceDisplay, which will handle its deconstuction.
    void AddRow(const Cn3D::Sequence *sequence, RowOfCells *row);

private:
    typedef std::vector < const Cn3D::Sequence * > SequenceVector;
    SequenceVector sequences;

    typedef std::vector < RowOfCells * > RowList;
    RowList rows;

    int maxRowWidth;
};


// these are the definition for the actual sequence display window
class SequenceDrawingArea;

class SequenceViewerGUI : public wxFrame
{
public:
    SequenceViewerGUI(Cn3D::SequenceViewer *parent);
    ~SequenceViewerGUI(void);

    void NewDisplay(const SequenceDisplay *newDisplay); 

    DECLARE_EVENT_TABLE()

private:
    SequenceDrawingArea *drawingArea;
	Cn3D::SequenceViewer *viewerParent;
};

class SequenceDrawingArea : public wxScrolledWindow
{
public:
    SequenceDrawingArea(SequenceViewerGUI *parent);

    void NewDisplay(const SequenceDisplay *newDisplay);

    void OnDraw(wxDC& dc);

    DECLARE_EVENT_TABLE()

private:
    const SequenceDisplay *display;

    wxFont font;

    int cellWidth, cellHeight;  // dimensions of cells in pixels
    int areaWidth, areaHeight;  // dimensions of SequenceDrawingArea in cells

    // draw single cell
    void DrawCell(wxDC& dc, int x, int y);

};

#endif // CN3D_SEQUENCE_VIEWER_GUI__HPP
