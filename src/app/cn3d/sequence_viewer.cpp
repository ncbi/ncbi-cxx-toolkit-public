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
* Revision 1.20  2000/10/16 20:03:07  thiessen
* working block creation
*
* Revision 1.19  2000/10/12 19:20:45  thiessen
* working block deletion
*
* Revision 1.18  2000/10/12 16:22:45  thiessen
* working block split
*
* Revision 1.17  2000/10/12 02:14:56  thiessen
* working block boundary editing
*
* Revision 1.16  2000/10/05 18:34:43  thiessen
* first working editing operation
*
* Revision 1.15  2000/10/04 17:41:30  thiessen
* change highlight color (cell background) handling
*
* Revision 1.14  2000/10/03 18:59:23  thiessen
* added row/column selection
*
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


static const wxColour highlightColor(255,255,0);  // yellow

// block marker string stuff
static const char
    blockLeftEdgeChar = '<',
    blockRightEdgeChar = '>',
    blockOneColumnChar = '^',
    blockInsideChar = '-';
static const std::string blockBoundaryStringTitle("(blocks)");

class SequenceDisplay;

////////////////////////////////////////////////////////////////////////////////
// SequenceViewerWindow is the top-level window that contains the
// SequenceViewerWidget.
////////////////////////////////////////////////////////////////////////////////

class SequenceViewerWindow : public wxFrame
{
    friend class SequenceDisplay;

public:
    SequenceViewerWindow(SequenceViewer *parent);
    ~SequenceViewerWindow(void);

    void NewAlignment(ViewableAlignment *newAlignment);

    // scroll to specific column
    void ScrollToColumn(int column) { viewerWidget->ScrollTo(column, -1); }

    // updates alignment (e.g. if width or # rows has changed);
    void UpdateAlignment(ViewableAlignment *alignment);

    void OnTitleView(wxCommandEvent& event);
    void OnEditMenu(wxCommandEvent& event);
    void OnMouseMode(wxCommandEvent& event);
    void OnJustification(wxCommandEvent& event);

    // menu identifiers
    enum {
        // view menu
        MID_SHOW_TITLES,
        MID_HIDE_TITLES,

        // edit menu
        MID_ENABLE_EDIT,
        MID_SPLIT_BLOCK,
        MID_MERGE_BLOCKS,
        MID_CREATE_BLOCK,
        MID_DELETE_BLOCK,
        MID_SYNC_STRUCS,
        MID_SYNC_STRUCS_ON,

        // mouse mode
        MID_SELECT_RECT,
        MID_SELECT_COLS,
        MID_SELECT_ROWS,
        MID_MOVE_ROW,
        MID_DRAG_HORIZ,

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
    void EnableEditorMenuItems(bool enabled);

    wxMenuBar *menuBar;

    SequenceViewerWidget::eMouseMode prevMouseMode;

public:
    void Refresh(void) { viewerWidget->Refresh(false); }

    bool IsEditingEnabled(void) const { return menuBar->IsChecked(MID_DRAG_HORIZ); }

    bool DoSplitBlock(void) const { return menuBar->IsChecked(MID_SPLIT_BLOCK); }
    void SplitBlockOff(void) {
        menuBar->Check(MID_SPLIT_BLOCK, false);
        SetCursor(wxNullCursor);
    }

    bool DoMergeBlocks(void) const { return menuBar->IsChecked(MID_MERGE_BLOCKS); }
    void MergeBlocksOff(void)
    {
        menuBar->Check(MID_MERGE_BLOCKS, false); 
        viewerWidget->SetMouseMode(prevMouseMode);
        SetCursor(wxNullCursor);
    }

    bool DoCreateBlock(void) const { return menuBar->IsChecked(MID_CREATE_BLOCK); }
    void CreateBlockOff(void)
    {
        menuBar->Check(MID_CREATE_BLOCK, false); 
        viewerWidget->SetMouseMode(prevMouseMode);
        SetCursor(wxNullCursor);
    }

    bool DoDeleteBlock(void) const { return menuBar->IsChecked(MID_DELETE_BLOCK); }
    void DeleteBlockOff(void) {
        menuBar->Check(MID_DELETE_BLOCK, false);
        SetCursor(wxNullCursor);
    }

    void SyncStructures(void) { Command(MID_SYNC_STRUCS); }
    bool AlwaysSyncStructures(void) const { return menuBar->IsChecked(MID_SYNC_STRUCS_ON); }
};

BEGIN_EVENT_TABLE(SequenceViewerWindow, wxFrame)
    EVT_CLOSE     (                                     SequenceViewerWindow::OnCloseWindow)
    EVT_MENU_RANGE(MID_SHOW_TITLES, MID_HIDE_TITLES,    SequenceViewerWindow::OnTitleView)
    EVT_MENU_RANGE(MID_ENABLE_EDIT, MID_SYNC_STRUCS_ON, SequenceViewerWindow::OnEditMenu)
    EVT_MENU_RANGE(MID_SELECT_RECT, MID_DRAG_HORIZ,     SequenceViewerWindow::OnMouseMode)
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
    wxString noHelp("");

    menuBar = new wxMenuBar;
    wxMenu *menu = new wxMenu;
    menu->Append(MID_SHOW_TITLES, "&Show Titles");
    //menu->Append(MID_HIDE_TITLES, "&Hide Titles");
    menuBar->Append(menu, "&View");

    menu = new wxMenu;
    menu->Append(MID_ENABLE_EDIT, "&Enable Editor", noHelp, true);
    menu->AppendSeparator();
    menu->Append(MID_SPLIT_BLOCK, "&Split Block", noHelp, true);
    menu->Append(MID_MERGE_BLOCKS, "&Merge Blocks", noHelp, true);
    menu->Append(MID_CREATE_BLOCK, "&Create Block", noHelp, true);
    menu->Append(MID_DELETE_BLOCK, "&Delete Block", noHelp, true);
    menu->Append(MID_SYNC_STRUCS, "Sync Structure &Colors");
    menu->Append(MID_SYNC_STRUCS_ON, "&Always Sync Structure Colors", noHelp, true);
    menuBar->Append(menu, "&Edit");

    menu = new wxMenu;
    menu->Append(MID_SELECT_RECT, "&Select Rectangle", noHelp, true);
    menu->Append(MID_SELECT_COLS, "Select &Columns", noHelp, true);
    menu->Append(MID_SELECT_ROWS, "Select &Rows", noHelp, true);
    menu->Append(MID_MOVE_ROW, "&Move Row", noHelp, true);
    menu->Append(MID_DRAG_HORIZ, "&Horizontal Drag", noHelp, true);
    menuBar->Append(menu, "&Mouse Mode");

    menu = new wxMenu;
    menu->Append(MID_LEFT, "&Left", noHelp, true);
    menu->Append(MID_RIGHT, "&Right", noHelp, true);
    menu->Append(MID_CENTER, "&Center", noHelp, true);
    menu->Append(MID_SPLIT, "&Split", noHelp, true);
    menuBar->Append(menu, "Unaligned &Justification");

    // set default initial modes
    viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectRectangle);
    menuBar->Check(MID_SELECT_RECT, true);
    viewer->SetUnalignedJustification(BlockMultipleAlignment::eSplit);
    menuBar->Check(MID_SPLIT, true);
    viewerWidget->TitleAreaOff();
    menuBar->Check(MID_SPLIT_BLOCK, false);
    menuBar->Check(MID_MERGE_BLOCKS, false);
    menuBar->Check(MID_CREATE_BLOCK, false);
    menuBar->Check(MID_DELETE_BLOCK, false);
    menuBar->Check(MID_SYNC_STRUCS_ON, true);
    EnableEditorMenuItems(false);

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
    if (viewer->IsEditableAlignment())
        menuBar->Enable(MID_ENABLE_EDIT, true);
    else
        menuBar->Enable(MID_ENABLE_EDIT, false);
    Show(true);
}

void SequenceViewerWindow::UpdateAlignment(ViewableAlignment *alignment)
{
    int vsX, vsY;   // to preserve scroll position
    viewerWidget->GetScroll(&vsX, &vsY);
    viewerWidget->AttachAlignment(alignment);
    viewerWidget->ScrollTo(vsX, vsY);
    viewer->messenger->PostRedrawSequenceViewers();
}

void SequenceViewerWindow::OnTitleView(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_SHOW_TITLES:
            viewerWidget->TitleAreaOn(); break;
        case MID_HIDE_TITLES:
            viewerWidget->TitleAreaOff(); break;
    }
}

void SequenceViewerWindow::OnEditMenu(wxCommandEvent& event)
{
    // check has already been toggled when this function is called, hence the '!'
    bool editorOn = !menuBar->IsChecked(MID_ENABLE_EDIT);

    switch (event.GetId()) {
        case MID_ENABLE_EDIT:
            if (editorOn) {
                TESTMSG("turning off editor");
                viewer->RemoveBlockBoundaryRow();
                if (menuBar->IsChecked(MID_DRAG_HORIZ)) Command(MID_SELECT_RECT);
            } else {
                TESTMSG("turning on editor");
                viewer->AddBlockBoundaryRow();
            }
            EnableEditorMenuItems(!editorOn);
            break;
        case MID_SPLIT_BLOCK:
            if (DoCreateBlock()) CreateBlockOff();
            if (DoMergeBlocks()) MergeBlocksOff();
            if (DoDeleteBlock()) DeleteBlockOff();
            if (menuBar->IsChecked(MID_SPLIT_BLOCK))
                SetCursor(*wxCROSS_CURSOR);
            else
                SplitBlockOff();
            break;
        case MID_MERGE_BLOCKS:
            if (DoSplitBlock()) SplitBlockOff();
            if (DoCreateBlock()) CreateBlockOff();
            if (DoDeleteBlock()) DeleteBlockOff();
            if (menuBar->IsChecked(MID_MERGE_BLOCKS)) {
                SetCursor(*wxCROSS_CURSOR);
                prevMouseMode = viewerWidget->GetMouseMode();
                viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectColumns);
            } else
                MergeBlocksOff();
            break;
        case MID_CREATE_BLOCK:
            if (DoSplitBlock()) SplitBlockOff();
            if (DoMergeBlocks()) MergeBlocksOff();
            if (DoDeleteBlock()) DeleteBlockOff();
            if (menuBar->IsChecked(MID_CREATE_BLOCK)) {
                SetCursor(*wxCROSS_CURSOR);
                prevMouseMode = viewerWidget->GetMouseMode();
                viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectColumns);
            } else
                CreateBlockOff();
            break;
        case MID_DELETE_BLOCK:
            if (DoSplitBlock()) SplitBlockOff();
            if (DoMergeBlocks()) MergeBlocksOff();
            if (DoCreateBlock()) CreateBlockOff();
            if (menuBar->IsChecked(MID_DELETE_BLOCK))
                SetCursor(*wxCROSS_CURSOR);
            else
                DeleteBlockOff();
            break;
        case MID_SYNC_STRUCS:
            viewer->RedrawAlignedMolecules();
            break;
        case MID_SYNC_STRUCS_ON:
            break;
    }
}

void SequenceViewerWindow::EnableEditorMenuItems(bool enabled)
{
    int i;
    for (i=MID_SPLIT_BLOCK; i<=MID_SYNC_STRUCS_ON; i++)
        menuBar->Enable(i, enabled);
    menuBar->Enable(MID_DRAG_HORIZ, enabled);
    if (!enabled) {
        SplitBlockOff();
        MergeBlocksOff();
        CreateBlockOff();
        DeleteBlockOff();
    }
}

void SequenceViewerWindow::OnMouseMode(wxCommandEvent& event)
{
    for (int i=MID_SELECT_RECT; i<=MID_DRAG_HORIZ; i++)
        menuBar->Check(i, (i == event.GetId()) ? true : false);

    switch (event.GetId()) {
        case MID_SELECT_RECT:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectRectangle); break;
        case MID_SELECT_COLS:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectColumns); break;
        case MID_SELECT_ROWS:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectRows); break;
        case MID_MOVE_ROW:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eDragVertical); break;
        case MID_DRAG_HORIZ:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eDragHorizontal); break;
    }
}

void SequenceViewerWindow::OnJustification(wxCommandEvent& event)
{
    for (int i=MID_LEFT; i<=MID_SPLIT; i++)
        menuBar->Check(i, (i == event.GetId()) ? true : false);

    switch (event.GetId()) {
        case MID_LEFT:
            viewer->SetUnalignedJustification(BlockMultipleAlignment::eLeft); break;
        case MID_RIGHT:
            viewer->SetUnalignedJustification(BlockMultipleAlignment::eRight); break;
        case MID_CENTER:
            viewer->SetUnalignedJustification(BlockMultipleAlignment::eCenter); break;
        case MID_SPLIT:
            viewer->SetUnalignedJustification(BlockMultipleAlignment::eSplit); break;
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
        char *character, Vector *color, bool *drawBackground,
        wxColour *cellBackgroundColor) const = 0;
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
        char *character, Vector *color,
        bool *drawBackground, wxColour *cellBackgroundColor) const;

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
        char *character, Vector *color,
        bool *drawBackground, wxColour *cellBackgroundColor) const;

    bool GetSequenceAndIndexAt(int column,
        const Sequence **sequenceHandle, int *index) const;

    const Sequence * GetSequence(void) const
        { return sequence; }
    
    void SelectedRange(int from, int to) const;
};

class DisplayRowFromString : public DisplayRow
{
public:
    const std::string title;
    std::string theString;
    const Vector stringColor, backgroundColor;
    const bool hasBackgroundColor;

    DisplayRowFromString(const std::string& s, const Vector color = Vector(0,0,0.5),
        const std::string& t = "", bool hasBG = false, Vector bgColor = (1,1,1)) : 
        theString(s), stringColor(color), title(t),
        hasBackgroundColor(hasBG), backgroundColor(bgColor) { }

    int Size() const { return theString.size(); }
    
    bool GetCharacterTraitsAt(int column,
        char *character, Vector *color,
        bool *drawBackground, wxColour *cellBackgroundColor) const;

    bool GetSequenceAndIndexAt(int column,
        const Sequence **sequenceHandle, int *index) const { return false; }

    const Sequence * GetSequence(void) const { return NULL; }

    void SelectedRange(int from, int to) const { } // do nothing
};


bool DisplayRowFromAlignment::GetCharacterTraitsAt(int column,
    char *character, Vector *color,
    bool *drawBackground, wxColour *cellBackgroundColor) const
{
    bool isHighlighted,
        result = alignment->GetCharacterTraitsAt(column, row, character, color, &isHighlighted);
    
    if (isHighlighted) {
        *drawBackground = true;
        *cellBackgroundColor = highlightColor;
    } else
        *drawBackground = false;

    return result;
}

bool DisplayRowFromSequence::GetCharacterTraitsAt(int column,
    char *character, Vector *color, bool *drawBackground, wxColour *cellBackgroundColor) const
{    
    if (column >= sequence->sequenceString.size())
        return false;

    *character = tolower(sequence->sequenceString[column]);
    if (sequence->molecule)
        *color = sequence->molecule->GetResidueColor(column);
    else
        color->Set(0,0,0);
    if (messenger->IsHighlighted(sequence, column)) {
        *drawBackground = true;
        *cellBackgroundColor = highlightColor;
    } else {
        *drawBackground = false;
    }

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
    char *character, Vector *color, bool *drawBackground, wxColour *cellBackgroundColor) const
{
    if (column >= theString.size()) return false;

    *character = theString[column];
    *color = stringColor;

    if (hasBackgroundColor) {
        *drawBackground = true;
        // convert vector color to wxColour
        cellBackgroundColor->Set(
            static_cast<unsigned char>((backgroundColor[0] + 0.000001) * 255),
            static_cast<unsigned char>((backgroundColor[1] + 0.000001) * 255),
            static_cast<unsigned char>((backgroundColor[2] + 0.000001) * 255)
        );
    } else {
        *drawBackground = false;
    }
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

    // these functions add a row to the end of the display, from various sources
    void AddRowFromAlignment(int row, BlockMultipleAlignment *fromAlignment);
    void AddRowFromSequence(const Sequence *sequence, Messenger *messenger);
    void AddRowFromString(const std::string& anyString);

    // generic row manipulation functions
    void AddRow(DisplayRow *row);
    void PrependRow(DisplayRow *row);
    void RemoveRow(DisplayRow *row);

private:
    
    Messenger *messenger;
    int startingColumn;
    typedef std::vector < DisplayRow * > RowVector;
    RowVector rows;

    int maxRowWidth;
    void UpdateMaxRowWidth(void);

    BlockMultipleAlignment *alignment;
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
        bool *drawBackground,                                   // special background color?
        wxColour *cellBackgroundColor
    ) const;
    
    // callbacks for ViewableAlignment
    void MouseOver(int column, int row) const;
    bool MouseDown(int column, int row, unsigned int controls);
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
    UpdateMaxRowWidth();
}

void SequenceDisplay::PrependRow(DisplayRow *row)
{
    rows.insert(rows.begin(), row);
    UpdateMaxRowWidth();
}

void SequenceDisplay::RemoveRow(DisplayRow *row)
{
    RowVector::iterator r, re = rows.end();
    for (r=rows.begin(); r!=re; r++) {
        if (*r == row) {
            rows.erase(r);
            delete row;
            break;
        }
    }
    UpdateMaxRowWidth();
}

void SequenceDisplay::UpdateMaxRowWidth(void)
{
    RowVector::iterator r, re = rows.end();
    maxRowWidth = 0;
    for (r=rows.begin(); r!=re; r++)
        if ((*r)->Size() > maxRowWidth) maxRowWidth = (*r)->Size();
    if (*viewerWindow) (*viewerWindow)->UpdateAlignment(this);
}

void SequenceDisplay::AddRowFromAlignment(int row, BlockMultipleAlignment *fromAlignment)
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

    const DisplayRowFromString *strRow = dynamic_cast<const DisplayRowFromString*>(displayRow);
    if (strRow && !strRow->title.empty()) {
        *title = strRow->title.c_str();
        color->Set(0,0,0);      // black
        return true;
    }
    
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
        char *character, wxColour *color, bool *drawBackground,
        wxColour *cellBackgroundColor) const
{
    if (row < 0 || row > rows.size()) {
        ERR_POST(Warning << "SequenceDisplay::GetCharacterTraitsAt() - row out of range");
        return false;
    }

    const DisplayRow *displayRow = rows[row];

    if (column >= displayRow->Size())
        return false;

    Vector colorVec;
    if (!displayRow->GetCharacterTraitsAt(
            column, character, &colorVec, drawBackground, cellBackgroundColor))
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

bool SequenceDisplay::MouseDown(int column, int row, unsigned int controls)
{
    TESTMSG("got MouseDown");
    controlDown = ((controls & ViewableAlignment::eControlDown) > 0);

    if (!controlDown && column == -1)
        messenger->RemoveAllHighlights(true);

    if (alignment && column >= 0) {
        if ((*viewerWindow)->DoSplitBlock()) {
            if (alignment->SplitBlock(column)) {
                (*viewerWindow)->SplitBlockOff();
                (*viewerWindow)->viewer->UpdateBlockBoundaryRow();
                if ((*viewerWindow)->AlwaysSyncStructures())
                    (*viewerWindow)->SyncStructures();
            }
            return false;
        }
        if ((*viewerWindow)->DoDeleteBlock()) {
            if (alignment->DeleteBlock(column)) {
                (*viewerWindow)->DeleteBlockOff();
                (*viewerWindow)->viewer->UpdateBlockBoundaryRow();
                if ((*viewerWindow)->AlwaysSyncStructures())
                    (*viewerWindow)->SyncStructures();

                // in case alignment width has changed
                UpdateMaxRowWidth();
            }
            return false;
        }
    }

    return true;
}

void SequenceDisplay::SelectedRectangle(int columnLeft, int rowTop, 
    int columnRight, int rowBottom)
{
    TESTMSG("got SelectedRectangle " << columnLeft << ',' << rowTop << " to "
        << columnRight << ',' << rowBottom);

    if (alignment) {
        if ((*viewerWindow)->DoMergeBlocks()) {
            if (alignment->MergeBlocks(columnLeft, columnRight)) {
                (*viewerWindow)->MergeBlocksOff();
                (*viewerWindow)->viewer->UpdateBlockBoundaryRow();
                if ((*viewerWindow)->AlwaysSyncStructures())
                    (*viewerWindow)->SyncStructures();
            }
            return;
        }
        if ((*viewerWindow)->DoCreateBlock()) {
            if (alignment->CreateBlock(columnLeft, columnRight)) {
                (*viewerWindow)->CreateBlockOff();
                (*viewerWindow)->viewer->UpdateBlockBoundaryRow();
                if ((*viewerWindow)->AlwaysSyncStructures())
                    (*viewerWindow)->SyncStructures();
            }
            return;
        }
    }

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
    if (rowFrom == rowTo && columnFrom == columnTo) return;
    if (rowFrom != rowTo && columnFrom != columnTo) return;     // ignore diagonal drag

    if (columnFrom != columnTo) {

        // process horizontal drag on special block boundary row
        DisplayRowFromString *strRow = dynamic_cast<DisplayRowFromString*>(rows[rowFrom]);
        if (strRow && alignment) {
            wxString title;
            wxColour ignored;
            if (GetRowTitle(rowFrom, &title, &ignored) && title == blockBoundaryStringTitle.c_str()) {
                char ch = strRow->theString[columnFrom];
                if (ch == blockRightEdgeChar || ch == blockLeftEdgeChar || ch == blockOneColumnChar) {
                    if (alignment->MoveBlockBoundary(columnFrom, columnTo)) {
                        (*viewerWindow)->viewer->UpdateBlockBoundaryRow();
                        if ((*viewerWindow)->AlwaysSyncStructures())
                            (*viewerWindow)->SyncStructures();
                    }
                }
            }
        }
        return;
    }

    // only allow reordering of alignment rows
//    DisplayRowFromAlignment *row;
//    if (dynamic_cast<DisplayRowFromAlignment*>(rows[rowTo]) == NULL) return;
//    if ((row = dynamic_cast<DisplayRowFromAlignment*>(rows[rowFrom])) == NULL) return;
    DisplayRow *row = rows[rowFrom];

    // use vertical drag to reorder row; move row so that it ends up in the 'rowTo' row
    RowVector::iterator r = rows.begin();
    int i;
    for (i=0; i<rowFrom; i++) r++; // get iterator for position rowFrom
    rows.erase(r);
    for (r=rows.begin(), i=0; i<rowTo; i++) r++; // get iterator for position rowTo
    rows.insert(r, row);

    messenger->PostRedrawSequenceViewers();
}


////////////////////////////////////////////////////////////////////////////////
// the implementation of the SequenceViewer, the interface to the viewer GUI
////////////////////////////////////////////////////////////////////////////////

SequenceViewer::SequenceViewer(Messenger *mesg) :
    viewerWindow(NULL), display(NULL), messenger(mesg), isEditableAlignment(false),
    blockBoundaryRow(NULL)
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

void SequenceViewer::DisplayAlignment(BlockMultipleAlignment *multiple)
{
    if (display) delete display;
    display = new SequenceDisplay(&viewerWindow, messenger);
    
    for (int row=0; row<multiple->NRows(); row++)
        display->AddRowFromAlignment(row, multiple);

    // set starting scroll to a few residues left of the first aligned block
    display->SetStartingColumn(multiple->GetFirstAlignedBlockPosition() - 5);

    isEditableAlignment = true;
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

    isEditableAlignment = false;
    NewAlignment(display);
}

void SequenceViewer::SetUnalignedJustification(BlockMultipleAlignment::eUnalignedJustification justification)
{
    if (display && display->alignment)
        (const_cast<BlockMultipleAlignment*>(display->alignment))->
            SetUnalignedJustification(justification);
}

void SequenceViewer::AddBlockBoundaryRow(void)
{
    if (!display || !display->alignment || !IsEditableAlignment() || blockBoundaryRow) return;

    blockBoundaryRow =
        new DisplayRowFromString("", Vector(0,0,0), blockBoundaryStringTitle, true, Vector(0.8,0.8,1));
    display->PrependRow(blockBoundaryRow);
    UpdateBlockBoundaryRow();

    viewerWindow->UpdateAlignment(display);
}

void SequenceViewer::UpdateBlockBoundaryRow(void)
{
    if (!display || !display->alignment || !IsEditableAlignment() || !blockBoundaryRow) return;

    blockBoundaryRow->theString.resize(display->alignment->AlignmentWidth());

    // fill out block boundary marker string
    int blockColumn, blockWidth;
    for (int i=0; i<display->alignment->AlignmentWidth(); i++) {
        display->alignment->GetAlignedBlockPosition(i, &blockColumn, &blockWidth);
        if (blockColumn >= 0 && blockWidth > 0) {
            if (blockWidth == 1)
                blockBoundaryRow->theString[i] = blockOneColumnChar;
            else if (blockColumn == 0)
                blockBoundaryRow->theString[i] = blockLeftEdgeChar;
            else if (blockColumn == blockWidth - 1)
                blockBoundaryRow->theString[i] = blockRightEdgeChar;
            else
                blockBoundaryRow->theString[i] = blockInsideChar;
        } else
            blockBoundaryRow->theString[i] = ' ';
    }
    messenger->PostRedrawSequenceViewers();
}

void SequenceViewer::RemoveBlockBoundaryRow(void)
{
    if (blockBoundaryRow) {
        display->RemoveRow(blockBoundaryRow);
        blockBoundaryRow = NULL;
        viewerWindow->UpdateAlignment(display);
    }
}

void SequenceViewer::RedrawAlignedMolecules(void) const
{
    for (int i=0; i<display->rows.size(); i++) {
        const Sequence *sequence = display->rows[i]->GetSequence();
        if (sequence && sequence->molecule)
            messenger->PostRedrawMolecule(sequence->molecule);
    }
}

END_SCOPE(Cn3D)

