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
*      Classes to interface with sequence viewer GUI
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2000/10/02 23:25:22  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.12  2000/09/20 22:22:27  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.11  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.10  2000/09/14 14:55:34  thiessen
* add row reordering; misc fixes
*
* Revision 1.9  2000/09/12 01:47:38  thiessen
* fix minor but obscure bug
*
* Revision 1.8  2000/09/11 22:57:33  thiessen
* working highlighting
*
* Revision 1.7  2000/09/11 14:06:29  thiessen
* working alignment coloring
*
* Revision 1.6  2000/09/11 01:46:16  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.5  2000/09/08 20:16:55  thiessen
* working dynamic alignment views
*
* Revision 1.4  2000/09/07 17:37:35  thiessen
* minor fixes
*
* Revision 1.3  2000/09/03 18:46:49  thiessen
* working generalized sequence viewer
*
* Revision 1.2  2000/08/30 23:46:28  thiessen
* working alignment display
*
* Revision 1.1  2000/08/30 19:48:42  thiessen
* working sequence window
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include <wx/wx.h>

#include "cn3d/sequence_viewer.hpp"
#include "cn3d/sequence_viewer_widget.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/vector_math.hpp"
#include "cn3d/messenger.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)


////////////////////////////////////////////////////////////////////////////////
// SequenceViewerWindow is the top-level window that contains the
// SequenceViewerWidget.
////////////////////////////////////////////////////////////////////////////////

class SequenceViewerWindow : public wxFrame
{
public:
    SequenceViewerWindow(SequenceViewer *parent);
    ~SequenceViewerWindow(void);

    void NewAlignment(ViewableAlignment *newAlignment);

    void OnMouseMode(wxCommandEvent& event);
    void OnJustification(wxCommandEvent& event);

    // menu identifiers
    enum {
        // mouse mode
        MID_SELECT,
        MID_MOVE_ROW,

        // unaligned justification
        MID_LEFT,
        MID_RIGHT,
        MID_CENTER,
        MID_SPLIT
    };

    DECLARE_EVENT_TABLE()

private:
    SequenceViewerWidget *viewerWidget;
    SequenceViewer *viewer;

    void OnCloseWindow(wxCloseEvent& event);

public:
    // scroll over to a given column
    void ScrollToColumn(int column) { viewerWidget->ScrollToColumn(column); };

    void Refresh(void) { viewerWidget->Refresh(false); }
};

BEGIN_EVENT_TABLE(SequenceViewerWindow, wxFrame)
    EVT_CLOSE     (                                     SequenceViewerWindow::OnCloseWindow)
    EVT_MENU_RANGE(MID_SELECT,      MID_MOVE_ROW,       SequenceViewerWindow::OnMouseMode)
    EVT_MENU_RANGE(MID_LEFT,        MID_SPLIT,          SequenceViewerWindow::OnJustification)
END_EVENT_TABLE()

SequenceViewerWindow::SequenceViewerWindow(SequenceViewer *parent) :
    wxFrame(NULL, -1, "Cn3D++ Sequence Viewer", wxPoint(0,500), wxSize(1000,200)),
    viewerWidget(NULL), viewer(parent)
{
    // status bar with a single field
    CreateStatusBar(2);
    int widths[2] = { 150, -1 };
    SetStatusWidths(2, widths);

    viewerWidget = new SequenceViewerWidget(this);
    viewerWidget->SetMouseMode(SequenceViewerWidget::eSelect);

    wxMenuBar *menuBar = new wxMenuBar;
    wxMenu *menu = new wxMenu;
    menu->Append(MID_SELECT, "&Select");
    menu->Append(MID_MOVE_ROW, "Move &Row");
    menuBar->Append(menu, "&Mouse Mode");

    menu = new wxMenu;
    menu->Append(MID_LEFT, "&Left");
    menu->Append(MID_RIGHT, "&Right");
    menu->Append(MID_CENTER, "&Center");
    menu->Append(MID_SPLIT, "&Split");
    menuBar->Append(menu, "Unaligned &Justification");

    SetMenuBar(menuBar);
}

SequenceViewerWindow::~SequenceViewerWindow(void)
{
}

void SequenceViewerWindow::OnCloseWindow(wxCloseEvent& event)
{
    viewer->viewerWindow = NULL; // make sure SequenceViewer knows the GUI is gone
    Destroy();
}

void SequenceViewerWindow::NewAlignment(ViewableAlignment *newAlignment)
{
    viewerWidget->AttachAlignment(newAlignment);
    Show(true);
}

void SequenceViewerWindow::OnMouseMode(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_SELECT:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelect); break;
        case MID_MOVE_ROW:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eDragVertical); break;
    }
}

void SequenceViewerWindow::OnJustification(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_LEFT:
            viewer->SetUnalignedJustification(BlockMultipleAlignment::eLeft);
            break;
        case MID_RIGHT:
            viewer->SetUnalignedJustification(BlockMultipleAlignment::eRight);
            break;
        case MID_CENTER:
            viewer->SetUnalignedJustification(BlockMultipleAlignment::eCenter);
            break;
        case MID_SPLIT:
            viewer->SetUnalignedJustification(BlockMultipleAlignment::eSplit);
            break;
    }
    viewer->messenger->PostRedrawSequenceViewers();
}


////////////////////////////////////////////////////////////////////////////////
// The sequence view is composed of rows which can be from sequence, alignment,
// or any string - anything derived from DisplayRow, implemented below.
////////////////////////////////////////////////////////////////////////////////

class DisplayRow
{
public:
    virtual int Size() const = 0;
    virtual bool GetCharacterTraitsAt(int column,
        char *character, Vector *color, bool *isHighlighted) const = 0;
    virtual bool GetSequenceAndIndexAt(int column,
        const Sequence **sequence, int *index) const = 0;
    virtual const Sequence * GetSequence(void) const = 0;
    virtual void SelectedRange(int from, int to) const = 0;
};

class DisplayRowFromAlignment : public DisplayRow
{
public:
    const int row;
    const BlockMultipleAlignment * const alignment;

    DisplayRowFromAlignment(int r, const BlockMultipleAlignment *a) :
        row(r), alignment(a) { }

    int Size() const { return alignment->AlignmentWidth(); }

    bool GetCharacterTraitsAt(int column,
        char *character, Vector *color, bool *isHighlighted) const
    {
        return alignment->GetCharacterTraitsAt(column, row, character, color, isHighlighted);
    }

    bool GetSequenceAndIndexAt(int column,
        const Sequence **sequence, int *index) const
    {
		bool ignore;
        return alignment->GetSequenceAndIndexAt(column, row, sequence, index, &ignore);
    }

    const Sequence * GetSequence(void) const
    {
        return alignment->GetSequenceOfRow(row);
    }

    void SelectedRange(int from, int to) const
    {
        alignment->SelectedRange(row, from, to);
    }
};

class DisplayRowFromSequence : public DisplayRow
{
public:
    const Sequence * const sequence;
    Messenger *messenger;

    DisplayRowFromSequence(const Sequence *s, Messenger *mesg) :
        sequence(s), messenger(mesg) { }

    int Size() const { return sequence->sequenceString.size(); }
    
    bool GetCharacterTraitsAt(int column,
        char *character, Vector *color, bool *isHighlighted) const;

    bool GetSequenceAndIndexAt(int column,
        const Sequence **sequenceHandle, int *index) const;

    const Sequence * GetSequence(void) const
        { return sequence; }
    
    void SelectedRange(int from, int to) const;
};

class DisplayRowFromString : public DisplayRow
{
public:
    const std::string theString;
    const Vector stringColor;

    DisplayRowFromString(const std::string& s, const Vector color = Vector(0,0,0.5)) : 
        theString(s), stringColor(color) { }

    int Size() const { return theString.size(); }
    
    bool GetCharacterTraitsAt(int column,
        char *character, Vector *color, bool *isHighlighted) const;

    bool GetSequenceAndIndexAt(int column,
        const Sequence **sequenceHandle, int *index) const { return false; }

    const Sequence * GetSequence(void) const { return NULL; }

    void SelectedRange(int from, int to) const { } // do nothing
};


bool DisplayRowFromSequence::GetCharacterTraitsAt(int column,
    char *character, Vector *color, bool *isHighlighted) const
{    
    if (column >= sequence->sequenceString.size())
        return false;

    *character = tolower(sequence->sequenceString[column]);
    if (sequence->molecule)
        *color = sequence->molecule->GetResidueColor(column);
    else
        color->Set(0,0,0);
    *isHighlighted = messenger->IsHighlighted(sequence, column);
    return true;
}

bool DisplayRowFromSequence::GetSequenceAndIndexAt(int column,
    const Sequence **sequenceHandle, int *index) const
{
    if (column >= sequence->sequenceString.size())
        return false;

    *sequenceHandle = sequence;
    *index = column;
    return true;
}

void DisplayRowFromSequence::SelectedRange(int from, int to) const
{
    int len = sequence->sequenceString.size();
    
    // skip if selected outside range
    if (from < 0 && to < 0) return;
    if (from >= len && to >= len) return;

    // trim to within range
    if (from < 0) from = 0;
    else if (from >= len) from = len - 1;
    if (to < 0) to = 0;
    else if (to >= len) to = len - 1;

    messenger->AddHighlights(sequence, from, to);
}

bool DisplayRowFromString::GetCharacterTraitsAt(int column,
    char *character, Vector *color, bool *isHighlighted) const
{
    if (column >= theString.size()) return false;

    *character = theString[column];
    *color = stringColor;
    *isHighlighted = false;
    return true;
}


////////////////////////////////////////////////////////////////////////////////
// The SequenceDisplay is the structure that holds the DisplayRows of the view.
// It's also derived from ViewableAlignment, in order to be plugged into a
// SequenceViewerWidget.
////////////////////////////////////////////////////////////////////////////////

class SequenceViewerWindow;

class SequenceDisplay : public ViewableAlignment
{
    friend class SequenceViewer;

public:
    SequenceDisplay(SequenceViewerWindow * const *parentViewer, Messenger *messenger);
    ~SequenceDisplay(void);

    // these functions add a row to the display, from various sources
    void AddRowFromAlignment(int row, const BlockMultipleAlignment *fromAlignment);
    void AddRowFromSequence(const Sequence *sequence, Messenger *messenger);
    void AddRowFromString(const std::string& anyString);

private:
    
    void AddRow(DisplayRow *row);

    Messenger *messenger;
    int startingColumn;
    typedef std::vector < DisplayRow * > RowVector;
    RowVector rows;
    int maxRowWidth;
    const BlockMultipleAlignment *alignment;
    SequenceViewerWindow * const *viewerWindow;
    bool controlDown;

public:
    // column to scroll to when this display is first shown
    void SetStartingColumn(int column) { startingColumn = column; }
    int GetStartingColumn(void) const { return startingColumn; }

    // methods required by ViewableAlignment base class
    void GetSize(int *setColumns, int *setRows) const
        { *setColumns = maxRowWidth; *setRows = rows.size(); }
    bool GetRowTitle(int row, wxString *title, wxColour *color) const;
    bool GetCharacterTraitsAt(int column, int row,              // location
        char *character,										// character to draw
        wxColour *color,                                        // color of this character
        bool *isHighlighted
    ) const;
    
    // callbacks for ViewableAlignment
    void MouseOver(int column, int row) const;
    void MouseDown(int column, int row, unsigned int controls);
    void SelectedRectangle(int columnLeft, int rowTop, int columnRight, int rowBottom);
    void DraggedCell(int columnFrom, int rowFrom, int columnTo, int rowTo);
};

SequenceDisplay::SequenceDisplay(SequenceViewerWindow * const *parentViewerWindow,
    Messenger *mesg) : 
    maxRowWidth(0), viewerWindow(parentViewerWindow), alignment(NULL), startingColumn(0),
    messenger(mesg)
{
}

SequenceDisplay::~SequenceDisplay(void)
{
    for (int i=0; i<rows.size(); i++) delete rows[i];
}

void SequenceDisplay::AddRow(DisplayRow *row)
{
    rows.push_back(row);
    if (row->Size() > maxRowWidth) maxRowWidth = row->Size();
}

void SequenceDisplay::AddRowFromAlignment(int row, const BlockMultipleAlignment *fromAlignment)
{
    if (!fromAlignment || row < 0 || row >= fromAlignment->NRows() ||
        (alignment && alignment != fromAlignment)) {
        ERR_POST(Error << "SequenceDisplay::AddRowFromAlignment() failed");
        return;
    }
    alignment = fromAlignment;  // to make sure all alignment rows are from same alignment

    AddRow(new DisplayRowFromAlignment(row, fromAlignment));
}

void SequenceDisplay::AddRowFromSequence(const Sequence *sequence, Messenger *messenger)
{
    if (!sequence) {
        ERR_POST(Error << "SequenceDisplay::AddRowFromSequence() failed");
        return;
    }

    AddRow(new DisplayRowFromSequence(sequence, messenger));
}

void SequenceDisplay::AddRowFromString(const std::string& anyString)
{
    AddRow(new DisplayRowFromString(anyString));
}

bool SequenceDisplay::GetRowTitle(int row, wxString *title, wxColour *color) const
{
    const DisplayRow *displayRow = rows[row];
    const Sequence *sequence = displayRow->GetSequence();
    if (!sequence) return false;

    // set title
    if (sequence->molecule) {
        wxString pdbID(sequence->molecule->pdbID.data(), sequence->molecule->pdbID.size());
        if (sequence->molecule->name.size() > 0 && 
            sequence->molecule->pdbChain != Molecule::NOT_SET &&
            sequence->molecule->name[0] != ' ') {
            wxString chain;
            chain.Printf("_%c", sequence->molecule->name[0]);
            pdbID += chain;
        }
        *title = pdbID;
    } else {
        title->clear();
        title->Printf("gi %i", sequence->gi);
    }

    // set color
    const DisplayRowFromAlignment *alnRow = dynamic_cast<const DisplayRowFromAlignment*>(displayRow);
    if (alnRow && alnRow->alignment->IsMaster(sequence))
        color->Set(255,0,0);    // red
    else
        color->Set(0,0,0);      // black

    return true;
}

bool SequenceDisplay::GetCharacterTraitsAt(int column, int row,
        char *character, wxColour *color, bool *isHighlighted) const
{
    if (row < 0 || row > rows.size()) {
        ERR_POST(Warning << "SequenceDisplay::GetCharacterTraitsAt() - row out of range");
        return false;
    }

    const DisplayRow *displayRow = rows[row];

    if (column >= displayRow->Size())
        return false;

    Vector colorVec;
    if (!displayRow->GetCharacterTraitsAt(column, character, &colorVec, isHighlighted))
        return false;

    // convert vector color to wxColour
    color->Set(
        static_cast<unsigned char>((colorVec[0] + 0.000001) * 255),
        static_cast<unsigned char>((colorVec[1] + 0.000001) * 255),
        static_cast<unsigned char>((colorVec[2] + 0.000001) * 255)
    );

    return true;
}

void SequenceDisplay::MouseOver(int column, int row) const
{
    if (*viewerWindow) {
        wxString message;
        if (column >= 0 && row >= 0) {
            const DisplayRow *displayRow = rows[row];
            if (column < displayRow->Size()) {
                const Sequence *sequence;
                int index;
                if (displayRow->GetSequenceAndIndexAt(column, &sequence, &index)) {
                    if (index >= 0) {
                        wxString title;
                        wxColour color;
                        if (GetRowTitle(row, &title, &color)) {
                            message.Printf(", loc %i", index);
                            message = title + message;
                        }
                    }
                }
            }
        }
        (*viewerWindow)->SetStatusText(message, 0);
    }
}

void SequenceDisplay::MouseDown(int column, int row, unsigned int controls)
{
    TESTMSG("got MouseDown");
    controlDown = ((controls & ViewableAlignment::eControlDown) > 0);

    if (!controlDown && column == -1)
        messenger->RemoveAllHighlights(true);
}

void SequenceDisplay::SelectedRectangle(int columnLeft, int rowTop, 
    int columnRight, int rowBottom)
{
    TESTMSG("got SelectedRectangle " << columnLeft << ',' << rowTop << " to "
        << columnRight << ',' << rowBottom);

    if (!controlDown)
        messenger->RemoveAllHighlights(true);

    for (int i=rowTop; i<=rowBottom; i++)
        rows[i]->SelectedRange(columnLeft, columnRight);
}

void SequenceDisplay::DraggedCell(int columnFrom, int rowFrom,
    int columnTo, int rowTo)
{
    TESTMSG("got DraggedCell " << columnFrom << ',' << rowFrom << " to "
        << columnTo << ',' << rowTo);
    if (rowFrom == rowTo || columnFrom != columnTo) return; // ignore all but vertical drag

    // only allow reordering of alignment rows
    DisplayRowFromAlignment *alnRow;
    if (dynamic_cast<DisplayRowFromAlignment*>(rows[rowTo]) == NULL) return;
    if ((alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[rowFrom])) == NULL) return;

    // use vertical drag to reorder row; move row so that it ends up in the 'rowTo' row
    RowVector::iterator r = rows.begin();
    int i;
    for (i=0; i<rowFrom; i++) r++; // get iterator for position rowFrom
    rows.erase(r);
    for (r=rows.begin(), i=0; i<rowTo; i++) r++; // get iterator for position rowTo
    rows.insert(r, alnRow);

    messenger->PostRedrawSequenceViewers();
}


////////////////////////////////////////////////////////////////////////////////
// the implementation of the SequenceViewer, the interface to the viewer GUI
////////////////////////////////////////////////////////////////////////////////

SequenceViewer::SequenceViewer(Messenger *mesg) :
    viewerWindow(NULL), display(NULL), messenger(mesg)
{
}

SequenceViewer::~SequenceViewer(void)
{
    DestroyGUI();
    if (display) delete display;
}

void SequenceViewer::ClearGUI(void)
{
    if (viewerWindow) viewerWindow->NewAlignment(NULL);
    if (display) {
        delete display;
        display = NULL;
    }
}

void SequenceViewer::DestroyGUI(void)
{
    if (viewerWindow) {
        viewerWindow->Destroy();
        viewerWindow = NULL;
    }
}

void SequenceViewer::Refresh(void)
{
    if (viewerWindow)
        viewerWindow->Refresh();
    else
        NewAlignment(display);
}

void SequenceViewer::NewAlignment(SequenceDisplay *display)
{
    if (display) {
        if (!viewerWindow) viewerWindow = new SequenceViewerWindow(this);
        viewerWindow->NewAlignment(display);
        viewerWindow->ScrollToColumn(display->GetStartingColumn());
        viewerWindow->Show(true);
        messenger->PostRedrawSequenceViewers();
    }
}

void SequenceViewer::DisplayAlignment(const BlockMultipleAlignment *multiple)
{
    if (display) delete display;
    display = new SequenceDisplay(&viewerWindow, messenger);
    
    for (int row=0; row<multiple->NRows(); row++)
        display->AddRowFromAlignment(row, multiple);

    // set starting scroll to a few residues left of the first aligned block
    display->SetStartingColumn(multiple->GetFirstAlignedBlockPosition() - 5);

    NewAlignment(display);
}

void SequenceViewer::DisplaySequences(const SequenceList *sequenceList)
{
    if (display) delete display;
    display = new SequenceDisplay(&viewerWindow, messenger);

    // populate each line of the display with one sequence, with blank lines inbetween
    SequenceList::const_iterator s, se = sequenceList->end();
    for (s=sequenceList->begin(); s!=se; s++) {
        if (s != sequenceList->begin()) display->AddRowFromString("");
        display->AddRowFromSequence(*s, messenger);
    }

    NewAlignment(display);
}

void SequenceViewer::SetUnalignedJustification(BlockMultipleAlignment::eUnalignedJustification justification)
{
    if (display && display->alignment)
        (const_cast<BlockMultipleAlignment*>(display->alignment))->
            SetUnalignedJustification(justification);
}

END_SCOPE(Cn3D)

