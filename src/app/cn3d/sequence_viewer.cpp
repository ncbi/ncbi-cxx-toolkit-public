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
#include "cn3d/alignment_manager.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/vector_math.hpp"
#include "cn3d/messenger.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

class SequenceViewerWindow;

class DisplayRow
{
public:
    virtual int Size() const = 0;
    virtual bool GetCharacterTraitsAt(int column,
        char *character, Vector *color, bool *isHighlighted) const = 0;
    virtual bool GetSequenceAndIndexAt(int column,
        const Sequence **sequence, int *index) const = 0;
};

class DisplayRowFromAlignment : public DisplayRow
{
public:
    const int row;
    const BlockMultipleAlignment * const alignment;

    DisplayRowFromAlignment(int r, const BlockMultipleAlignment *a) : row(r), alignment(a) { }

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
};

class DisplayRowFromSequence : public DisplayRow
{
public:
    const Sequence * const sequence;

    DisplayRowFromSequence(const Sequence *s) : sequence(s) { }

    int Size() const { return sequence->sequenceString.size(); }
    
    bool GetCharacterTraitsAt(int column,
        char *character, Vector *color, bool *isHighlighted) const;

    bool GetSequenceAndIndexAt(int column,
        const Sequence **sequenceHandle, int *index) const;
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
    *isHighlighted = false;
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
};

bool DisplayRowFromString::GetCharacterTraitsAt(int column,
    char *character, Vector *color, bool *isHighlighted) const
{
    if (column >= theString.size()) return false;

    *character = theString[column];
    *color = stringColor;
    *isHighlighted = false;
    return true;
}

// this is the class that interfaces between the alignment viewer and the underlying data. 
class SequenceDisplay : public ViewableAlignment
{
public:
    SequenceDisplay(SequenceViewerWindow * const *parentViewer);
    ~SequenceDisplay(void);

    // these functions add a row to the display, from various sources
    void AddRowFromAlignment(int row, const BlockMultipleAlignment *fromAlignment);
    void AddRowFromSequence(const Sequence *sequence);
    void AddRowFromString(const std::string& anyString);

private:

    void AddRow(DisplayRow *row);

    typedef std::vector < DisplayRow * > RowVector;
    RowVector rows;

    int maxRowWidth;

    const BlockMultipleAlignment *alignment;

    SequenceViewerWindow * const *viewerWindow;

public:
    // methods required by ViewableAlignment base class
    void GetSize(int *setColumns, int *setRows) const
        { *setColumns = maxRowWidth; *setRows = rows.size(); }
    bool GetCharacterTraitsAt(int column, int row,				// location
        char *character,										// character to draw
        wxColour *color,                                        // color of this character
        bool *isHighlighted
    ) const;
    
    // callbacks for ViewableAlignment
    void MouseOver(int column, int row) const;

    void SelectedRectangle(int columnLeft, int rowTop, 
        int columnRight, int rowBottom, unsigned int controls) const
        { TESTMSG("got SelectedRectangle " << columnLeft << ',' << rowTop << " to "
            << columnRight << ',' << rowBottom); }

    void DraggedCell(int columnFrom, int rowFrom,
        int columnTo, int rowTo, unsigned int controls) const
        { TESTMSG("got DraggedCell " << columnFrom << ',' << rowFrom << " to "
            << columnTo << ',' << rowTo); }
};

class SequenceViewerWindow : public wxFrame
{
public:
    SequenceViewerWindow(SequenceViewer *parent);
    ~SequenceViewerWindow(void);

    void NewAlignment(const ViewableAlignment *newAlignment);

    void OnMouseMode(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()

private:
    SequenceViewerWidget *viewerWidget;
    SequenceViewer *viewer;

public:
    void Refresh(void) { viewerWidget->Refresh(false); }
};


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
    TESTMSG("refreshing SequenceViewer");
    if (viewerWindow)
        viewerWindow->Refresh();
    else
        NewAlignment(display);
}

void SequenceViewer::NewAlignment(const ViewableAlignment *display)
{
    if (!viewerWindow) viewerWindow = new SequenceViewerWindow(this);
    viewerWindow->NewAlignment(display);
    messenger->PostRedrawSequenceViewers();
}

void SequenceViewer::DisplayAlignment(const BlockMultipleAlignment *multiple)
{
    if (display) delete display;
    display = new SequenceDisplay(&viewerWindow);
    
    for (int row=0; row<multiple->NSequences(); row++)
        display->AddRowFromAlignment(row, multiple);

    NewAlignment(display);
}

void SequenceViewer::DisplaySequences(const SequenceList *sequenceList)
{
    if (display) delete display;
    display = new SequenceDisplay(&viewerWindow);

    // populate each line of the display with one sequence, with blank lines inbetween
    SequenceList::const_iterator s, se = sequenceList->end();
    for (s=sequenceList->begin(); s!=se; s++) {
        if (s != sequenceList->begin()) display->AddRowFromString("");
        display->AddRowFromSequence(*s);
    }

    NewAlignment(display);
}


SequenceDisplay::SequenceDisplay(SequenceViewerWindow * const *parentViewerWindow) : 
    maxRowWidth(0), viewerWindow(parentViewerWindow), alignment(NULL)
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
    if (!fromAlignment || row < 0 || row >= fromAlignment->NSequences() ||
        (alignment && alignment != fromAlignment)) {
        ERR_POST(Error << "SequenceDisplay::AddRowFromAlignment() failed");
        return;
    }

    AddRow(new DisplayRowFromAlignment(row, fromAlignment));
}

void SequenceDisplay::AddRowFromSequence(const Sequence *sequence)
{
    if (!sequence) {
        ERR_POST(Error << "SequenceDisplay::AddRowFromSequence() failed");
        return;
    }

    AddRow(new DisplayRowFromSequence(sequence));
}

void SequenceDisplay::AddRowFromString(const std::string& anyString)
{
    AddRow(new DisplayRowFromString(anyString));
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
                        if (sequence->molecule) {
                            wxString pdbID(sequence->molecule->pdbID.data(),
                            sequence->molecule->pdbID.size());
                            if (sequence->molecule->name.size() > 0 && sequence->molecule->name[0] != ' ')
                                message.Printf("_%c, loc %i", sequence->molecule->name[0], index);
                            else
                                message.Printf(", loc %i", index);
                            message = pdbID + message;
                        } else {
                            message.Printf("gi %i, loc %i", sequence->gi, index);
                        }
                    }
                }
            }
        }
        (*viewerWindow)->SetStatusText(message, 0);
    }
}

enum {
    MID_SELECT,
    MID_DRAG,
    MID_DRAG_H,
    MID_DRAG_V
};

BEGIN_EVENT_TABLE(SequenceViewerWindow, wxFrame)
    EVT_MENU_RANGE(MID_SELECT,   MID_DRAG_V,      SequenceViewerWindow::OnMouseMode)
END_EVENT_TABLE()

SequenceViewerWindow::SequenceViewerWindow(SequenceViewer *parent) :
    wxFrame(NULL, -1, "Cn3D++ Sequence Viewer", wxPoint(0,500), wxSize(500,200)),
    viewerWidget(NULL), viewer(parent)
{
    // status bar with a single field
    CreateStatusBar(2);
    int widths[2] = { 150, -1 };
    SetStatusWidths(2, widths);

    viewerWidget = new SequenceViewerWidget(this);

    wxMenuBar *menuBar = new wxMenuBar;
    wxMenu *menu = new wxMenu;
    menu->Append(MID_SELECT, "&Select");
    menu->Append(MID_DRAG, "&Drag");
    menu->Append(MID_DRAG_H, "Drag &Horizontal");
    menu->Append(MID_DRAG_V, "Drag &Vertical");
    menuBar->Append(menu, "&Mouse Mode");

    SetMenuBar(menuBar);
}

SequenceViewerWindow::~SequenceViewerWindow(void)
{
    viewer->viewerWindow = NULL; // make sure SequenceViewer knows the GUI is gone
    viewerWidget->Destroy();
}

void SequenceViewerWindow::NewAlignment(const ViewableAlignment *newAlignment)
{
    viewerWidget->AttachAlignment(newAlignment);
    Show(true);
}

void SequenceViewerWindow::OnMouseMode(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_SELECT:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelect); break;
        case MID_DRAG:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eDrag); break;
        case MID_DRAG_H:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eDragHorizontal); break;
        case MID_DRAG_V:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eDragVertical); break;
    }
}

END_SCOPE(Cn3D)

