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

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

class SequenceViewerWindow;

// this is the class that holds the sequences/alignment formatted for display -
// the equivalent of DDV's ParaG. 
class SequenceDisplay : public ViewableAlignment
{
public:
    SequenceDisplay(SequenceViewerWindow * const *parentViewer);
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
    void AddRow(const Sequence *sequence, RowOfCells *row);

private:
    typedef std::vector < const Sequence * > SequenceVector;
    SequenceVector sequences;

    typedef std::vector < RowOfCells * > RowList;
    RowList rows;

    int maxRowWidth;

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


SequenceViewer::SequenceViewer(void) : viewerWindow(NULL), display(NULL)
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
    if (viewerWindow)
        viewerWindow->Refresh();
    else
        NewAlignment(display);
}

void SequenceViewer::NewAlignment(const ViewableAlignment *display)
{
    if (!viewerWindow) viewerWindow = new SequenceViewerWindow(this);
    viewerWindow->NewAlignment(display);
}

static inline int RangeLength(const MultipleAlignment::Range& range)
{
    return (range.to - range.from + 1);
}

static const char UNALIGNED_GAP_CHAR = '~';

static void AddRightJustifiedUAR(SequenceDisplay::RowOfCells *row, int startAddingAt, 
    int length, const MultipleAlignment::Range& range, const Sequence *seq)
{
    int i, rangeLength = RangeLength(range);
    int padLength = length - rangeLength;
    for (i=0; i<padLength; i++) {
        row->at(startAddingAt + i).character = UNALIGNED_GAP_CHAR;
        row->at(startAddingAt + i).index = -1;
    }
    startAddingAt += padLength;
    for (i=0; i<rangeLength; i++) {
        row->at(startAddingAt + i).character = tolower(seq->sequenceString[range.from + i]);
        row->at(startAddingAt + i).index = range.from + i;
    }
}

static void AddLeftJustifiedUAR(SequenceDisplay::RowOfCells *row, int startAddingAt, 
    int length, const MultipleAlignment::Range& range, const Sequence *seq)
{
    int i, rangeLength = RangeLength(range);
    int padLength = length - rangeLength;
    for (i=0; i<rangeLength; i++) {
        row->at(startAddingAt + i).character = tolower(seq->sequenceString[range.from + i]);
        row->at(startAddingAt + i).index = range.from + i;
    }
    startAddingAt += rangeLength;
    for (i=0; i<padLength; i++) {
        row->at(startAddingAt + i).character = UNALIGNED_GAP_CHAR;
        row->at(startAddingAt + i).index = -1;
    }
}

void SequenceViewer::DisplayAlignment(const MultipleAlignment *multiple)
{
    if (display) delete display;
    display = new SequenceDisplay(&viewerWindow);

    SequenceDisplay::RowOfCells *row;

    typedef std::list < MultipleAlignment::Range > UnalignedRegionList;
    typedef std::list < UnalignedRegionList > UnalignedRegionLists;
    UnalignedRegionLists URs;
    typedef std::vector < int > MaxUnalignedLengths;
    MaxUnalignedLengths ULs(multiple->NBlocks() + 1, 0);

    // for each sequence, get the unaligned residues between each block (and at ends).
    // Also keep track of the maximum length of each unaligned region, across
    // all sequences, for padding with unaligned gaps.
    SequenceList::const_iterator s, se = multiple->sequences->end();
    MultipleAlignment::BlockList::const_iterator b, be = multiple->blocks.end(), bp1;
    int i = 0, length, j;
    for (s=multiple->sequences->begin(); s!=se; s++, i++) {

        // check for null-alignment
        if (multiple->NBlocks() == 0) {
            row = new SequenceDisplay::RowOfCells((*s)->Length());
            for (j=0; j < (*s)->Length(); j++) {
                row->at(j).character = tolower((*s)->sequenceString[j]);
                row->at(j).index = j;
            }
            display->AddRow(*s, row);
        }

        // real alignment
        else {
            URs.resize(URs.size() + 1);
            UnalignedRegionList& UR = URs.back();
            j = 0;

            for (b=multiple->blocks.begin(); b!=be; b++) {
                bp1 = b; bp1++;

                // left tail
                if (b == multiple->blocks.begin()) {
                    length = b->at(i).from;
                    UR.resize(UR.size() + 1);
                    UR.back().from = 0;
                    UR.back().to = length - 1;
                    if (length > ULs[j]) ULs[j] = length;
                    j++;
                }

                // right tail
                if (bp1 == multiple->blocks.end()) {
                    length = (*s)->sequenceString.size() - 1 - b->at(i).to;
                    UR.resize(UR.size() + 1);
                    UR.back().from = b->at(i).to + 1;
                    UR.back().to = UR.back().from + length - 1;
                    if (length > ULs[j]) ULs[j] = length;
                    break;
                }

                // between blocks
                else {
                    length = bp1->at(i).from - b->at(i).to - 1;
                    UR.resize(UR.size() + 1);
                    UR.back().from = b->at(i).to + 1;
                    UR.back().to = UR.back().from + length - 1;
                    if (length > ULs[j]) ULs[j] = length;
                    j++;
                }
            }
        }
    }

    // now we have all the info needed to create padded alignment strings
    if (multiple->NBlocks() > 0) {

        // calculate total alignment length
        int alignmentLength = 0;
        for (b=multiple->blocks.begin(), i=0; i<multiple->NBlocks(); b++, i++) {
            alignmentLength += ULs[i] + RangeLength(b->front());
        }
        alignmentLength += ULs[i]; // right tail

        // create display row for each sequence
        UnalignedRegionLists::const_iterator URs_i = URs.begin();
        for (i=0, s=multiple->sequences->begin(); s!=se; s++, URs_i++, i++) {
            UnalignedRegionList::const_iterator UR_i = URs_i->begin();
            MaxUnalignedLengths::const_iterator UL_i = ULs.begin();

            // allocate new display row
            row = new SequenceDisplay::RowOfCells(alignmentLength);

            int startAddingAt = 0, blockSize;
            for (b=multiple->blocks.begin(); b!=be; b++, UR_i++, UL_i++) {

                // add left tail
                if (b == multiple->blocks.begin()) { // always right-justified for left tail
                    AddRightJustifiedUAR(row, 0, *UL_i, *UR_i, *s);
                }
                // add unaligned region to the left of the current block
                else {
                    // should eventually have dynamic justification choice here...
                    AddRightJustifiedUAR(row, startAddingAt, *UL_i, *UR_i, *s);
                }
                startAddingAt += *UL_i;

                // add block
                blockSize = RangeLength(b->front());
                for (j=0; j<blockSize; j++) {
                    row->at(startAddingAt + j).character =
                        toupper((*s)->sequenceString[b->at(i).from + j]);
                    row->at(startAddingAt + j).index = b->at(i).from + j;
                }
                startAddingAt += blockSize;
            }

            // add right tail
            AddLeftJustifiedUAR(row, startAddingAt, *UL_i, *UR_i, *s);

            display->AddRow(*s, row);
        }
        TESTMSG("alignment display size: " << row->size() << 'x' << multiple->sequences->size());
    }

    NewAlignment(display);
}

void SequenceViewer::DisplaySequences(const SequenceList *sequenceList)
{
    if (display) delete display;
    display = new SequenceDisplay(&viewerWindow);

    // populate each line of the display with one sequence - in lower case to
    // emphasize that they're unaligned
    SequenceList::const_iterator s, se = sequenceList->end();
    SequenceDisplay::RowOfCells *row;
    int i;
    for (s=sequenceList->begin(); s!=se; s++) {
        if (s != sequenceList->begin()) display->AddRow(NULL, NULL); // put blank lines inbetween
        row = new SequenceDisplay::RowOfCells((*s)->Length());
        for (i=0; i < (*s)->Length(); i++) {
            row->at(i).character = tolower((*s)->sequenceString[i]);
            row->at(i).index = i;
        }
        display->AddRow(*s, row);
    }

    NewAlignment(display);
}


SequenceDisplay::SequenceDisplay(SequenceViewerWindow * const *parentViewerWindow) : 
    maxRowWidth(0), viewerWindow(parentViewerWindow)
{
}

SequenceDisplay::~SequenceDisplay(void)
{
    for (int i=0; i<rows.size(); i++) delete rows[i];
}

void SequenceDisplay::AddRow(const Sequence *sequence, RowOfCells *row)
{
    sequences.push_back(sequence);

    rows.resize(rows.size() + 1);
    rows.back() = row;

    if (row && row->size() > maxRowWidth) maxRowWidth = row->size();
}

bool SequenceDisplay::GetCharacterTraitsAt(int column, int row,
        char *character, wxColour *color, bool *isHighlighted) const
{
    const RowOfCells *cells = rows[row];
    if (!cells || column >= cells->size()) return false;

    // set character
    const Cell& cell = cells->at(column);
    *character = cell.character;

    // set color
    const Sequence *seq = sequences[row];
    if (seq && seq->molecule && cell.index >= 0) {
        Vector colorVec = seq->molecule->GetResidueColor(cell.index);
        color->Set(
            static_cast<unsigned char>((colorVec[0] + 0.000001) * 255),
            static_cast<unsigned char>((colorVec[1] + 0.000001) * 255),
            static_cast<unsigned char>((colorVec[2] + 0.000001) * 255)
        );
    } else {
        color->Set(0.5, 0.5, 0.5); // gray
    }

    // set highlighting
    *isHighlighted = false;

    return true;
}

void SequenceDisplay::MouseOver(int column, int row) const
{
    if (*viewerWindow) {
        wxString message;

        if (column != -1 && row != -1) {
            const Sequence *seq = sequences[row];
            if (seq) {
                const RowOfCells *cells = rows[row];
                if (cells && column < cells->size()) {
                    const Cell& cell = cells->at(column);
                    if (cell.index >= 0) {
                        message.Printf("gi %i, loc %i", seq->gi, cell.index);
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

