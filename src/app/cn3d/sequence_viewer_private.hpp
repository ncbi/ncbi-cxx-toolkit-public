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
*      Classes to hold sequence display info and GUI window
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2000/11/17 19:47:37  thiessen
* working show/hide alignment row
*
* Revision 1.2  2000/11/03 01:12:18  thiessen
* fix memory problem with alignment cloning
*
* Revision 1.1  2000/11/02 16:48:23  thiessen
* working editor undo; dynamic slave transforms
*
* ===========================================================================
*/

// THIS HEADER IS ONLY #INCLUDABLE FROM SEQUENCE_VIEWER.H - DON'T #INCLUDE FROM OTHER MODULES!

static const wxColour highlightColor(255,255,0);  // yellow

// block marker string stuff
static const char
    blockLeftEdgeChar = '<',
    blockRightEdgeChar = '>',
    blockOneColumnChar = '^',
    blockInsideChar = '-';
static const std::string blockBoundaryStringTitle("(blocks)");


////////////////////////////////////////////////////////////////////////////////
// SequenceViewerWindow is the top-level window that contains the
// SequenceViewerWidget.
////////////////////////////////////////////////////////////////////////////////

class SequenceDisplay;

class SequenceViewerWindow : public wxFrame
{
    friend class SequenceDisplay;

public:
    SequenceViewerWindow(SequenceViewer *parent);
    ~SequenceViewerWindow(void);

    // displays a new alignment
    void NewAlignment(ViewableAlignment *newAlignment);

    // updates alignment (e.g. if width or # rows has changed); doesn't change scroll
    void UpdateAlignment(ViewableAlignment *alignment);

    // scroll to specific column
    void ScrollToColumn(int column) { viewerWidget->ScrollTo(column, -1); }

    void OnTitleView(wxCommandEvent& event);
    void OnShowHideRows(wxCommandEvent& event);
    void OnEditMenu(wxCommandEvent& event);
    void OnMouseMode(wxCommandEvent& event);
    void OnJustification(wxCommandEvent& event);

    // menu identifiers
    enum {
        // view menu
        MID_SHOW_TITLES,
        MID_HIDE_TITLES,
        MID_SHOW_HIDE_ROWS,

        // edit menu
        MID_ENABLE_EDIT,
        MID_UNDO,
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

    BlockMultipleAlignment::eUnalignedJustification currentJustification;
    BlockMultipleAlignment::eUnalignedJustification GetCurrentJustification(void) const
        { return currentJustification; }

    // called before an operation (e.g., alignment editor enable) that requires
    // all rows of an alignment to be visible; 'false' return should abort that operation
    bool QueryShowAllRows(void);

public:
    // ask if user wants to save edits; return value indicates whether program should
    // continue after this dialog - i.e., returns false if user hits 'cancel';
    // program should then abort the operation that engendered this function call.
    // 'canCancel' tells whether or not a 'cancel' button is even displayed - thus
    // if 'canCancel' is false, the function will always return true.
    bool SaveDialog(bool canCancel);

    void Refresh(void) { viewerWidget->Refresh(false); }

    bool IsEditingEnabled(void) const { return menuBar->IsChecked(MID_DRAG_HORIZ); }

    void EnableUndo(bool enabled) { menuBar->Enable(MID_UNDO, enabled); }

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


////////////////////////////////////////////////////////////////////////////////
// The sequence view is composed of rows which can be from sequence, alignment,
// or any string - anything derived from DisplayRow, implemented below.
////////////////////////////////////////////////////////////////////////////////

class DisplayRow
{
public:
    virtual int Width(void) const = 0;
    virtual bool GetCharacterTraitsAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        char *character, Vector *color, bool *drawBackground,
        wxColour *cellBackgroundColor) const = 0;
    virtual bool GetSequenceAndIndexAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        const Sequence **sequence, int *index) const = 0;
    virtual const Sequence * GetSequence(void) const = 0;
    virtual void SelectedRange(int from, int to, BlockMultipleAlignment::eUnalignedJustification justification) const = 0;
    virtual DisplayRow * Clone(const BlockMultipleAlignment *newAlignment) const = 0;
};

class DisplayRowFromAlignment : public DisplayRow
{
public:
    const int row;
    const BlockMultipleAlignment * const alignment;

    DisplayRowFromAlignment(int r, const BlockMultipleAlignment *a) :
        row(r), alignment(a) { }

    int Width(void) const { return alignment->AlignmentWidth(); }

    DisplayRow * Clone(const BlockMultipleAlignment *newAlignment) const
        { return new DisplayRowFromAlignment(row, newAlignment); }
        
    bool GetCharacterTraitsAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        char *character, Vector *color, bool *drawBackground, wxColour *cellBackgroundColor) const;

    bool GetSequenceAndIndexAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        const Sequence **sequence, int *index) const
    {
		bool ignore;
        return alignment->GetSequenceAndIndexAt(column, row, justification, sequence, index, &ignore);
    }

    const Sequence * GetSequence(void) const
    {
        return alignment->GetSequenceOfRow(row);
    }

    void SelectedRange(int from, int to, BlockMultipleAlignment::eUnalignedJustification justification) const
    {
        alignment->SelectedRange(row, from, to, justification);
    }
};

class DisplayRowFromSequence : public DisplayRow
{
public:
    const Sequence * const sequence;
    Messenger *messenger;

    DisplayRowFromSequence(const Sequence *s, Messenger *mesg) :
        sequence(s), messenger(mesg) { }

    int Width(void) const { return sequence->sequenceString.size(); }
    
    DisplayRow * Clone(const BlockMultipleAlignment *newAlignment) const
        { return new DisplayRowFromSequence(sequence, messenger); }

    bool GetCharacterTraitsAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        char *character, Vector *color, bool *drawBackground, wxColour *cellBackgroundColor) const;

    bool GetSequenceAndIndexAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        const Sequence **sequenceHandle, int *index) const;

    const Sequence * GetSequence(void) const
        { return sequence; }
    
    void SelectedRange(int from, int to, BlockMultipleAlignment::eUnalignedJustification justification) const;
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

    int Width(void) const { return theString.size(); }
    
    DisplayRow * Clone(const BlockMultipleAlignment *newAlignment) const
        { return new DisplayRowFromString(
            theString, stringColor, title, hasBackgroundColor, backgroundColor); }
    
    bool GetCharacterTraitsAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        char *character, Vector *color, bool *drawBackground, wxColour *cellBackgroundColor) const;

    bool GetSequenceAndIndexAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        const Sequence **sequenceHandle, int *index) const { return false; }

    const Sequence * GetSequence(void) const { return NULL; }

    void SelectedRange(int from, int to, BlockMultipleAlignment::eUnalignedJustification justification) const { } // do nothing
};


////////////////////////////////////////////////////////////////////////////////
// The SequenceDisplay is the structure that holds the DisplayRows of the view.
// It's also derived from ViewableAlignment, in order to be plugged into a
// SequenceViewerWidget.
////////////////////////////////////////////////////////////////////////////////

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

    // create a new copy of this object
    SequenceDisplay * Clone(BlockMultipleAlignment *newAlignment) const;

private:
    
    Messenger *messenger;
    int startingColumn;
    typedef std::vector < DisplayRow * > RowVector;
    RowVector rows;

    int maxRowWidth;
    void UpdateMaxRowWidth(void);
    void UpdateAfterEdit(void);

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

