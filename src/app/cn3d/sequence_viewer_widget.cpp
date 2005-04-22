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

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "sequence_viewer_widget.hpp"
#include "viewable_alignment.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////////
///////// These are definitions of the sequence and title drawing areas /////////
/////////////////////////////////////////////////////////////////////////////////

class SequenceViewerWidget_TitleArea : public wxWindow
{
public:
    // constructor (same as wxWindow)
    SequenceViewerWidget_TitleArea(
        wxWindow* parent,
        wxWindowID id = -1,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize
    );

    // destructor
    ~SequenceViewerWidget_TitleArea(void);

    void ShowTitles(ViewableAlignment *newAlignment);
    void SetCharacterFont(wxFont *font, int newCellHeight);
    void SetBackgroundColor(const wxColor& backgroundColor);

private:
    void OnPaint(wxPaintEvent& event);
    void OnMouseEvent(wxMouseEvent& event);

    const SequenceViewerWidget_SequenceArea *sequenceArea;

    ViewableAlignment *alignment;
    int highlightedTitleRow;

    wxColor currentBackgroundColor;
    wxFont *titleFont;
    int cellHeight, nTitles, maxTitleWidth;

    DECLARE_EVENT_TABLE()

public:
    int GetMaxTitleWidth(void) const { return maxTitleWidth; }
    void SetSequenceArea(const SequenceViewerWidget_SequenceArea *seqA)
        { sequenceArea = seqA; }

};

class SequenceViewerWidget_SequenceArea : public wxScrolledWindow
{
    friend class SequenceViewerWidget;

public:
    // constructor (same as wxScrolledWindow)
    SequenceViewerWidget_SequenceArea(
        wxWindow* parent,
        wxWindowID id = -1,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxHSCROLL | wxVSCROLL,
        const wxString& name = "scrolledWindow"
    );

    // destructor
    ~SequenceViewerWidget_SequenceArea(void);

    // stuff from parent widget
    bool AttachAlignment(ViewableAlignment *newAlignment, int initX, int initY);
    void SetMouseMode(SequenceViewerWidget::eMouseMode mode);
    void SetBackgroundColor(const wxColor& backgroundColor);
    void SetCharacterFont(wxFont *font);
    void SetRubberbandColor(const wxColor& rubberbandColor);

    ///// everything else below here is part of the actual implementation /////
private:

    void OnPaint(wxPaintEvent& event);
    void OnMouseEvent(wxMouseEvent& event);
    void OnScrollWin(wxScrollWinEvent& event);

    SequenceViewerWidget_TitleArea *titleArea;
    ViewableAlignment *alignment;

    wxBitmap *bitmap;    // for memoryDC/Blit

    wxFont *currentFont;            // character font
    wxColor currentBackgroundColor; // background for whole window
    wxColor currentRubberbandColor; // color of rubber band
    SequenceViewerWidget::eMouseMode mouseMode;           // mouse mode
    int cellWidth, cellHeight;      // dimensions of cells in pixels
    int areaWidth, areaHeight;      // dimensions of the alignment (virtual area) in cells

    void DrawCell(wxDC& dc, int x, int y, int vsX, int vsY, bool redrawBackground); // draw a single cell

    enum eRubberbandType {
        eSolid,
        eDot
    };
    eRubberbandType currentRubberbandType;
    void DrawLine(wxDC& dc, int x1, int y1, int x2, int y2);
    void DrawRubberband(wxDC& dc,                           // draw rubber band around cells
        int fromX, int fromY, int toX, int toY, int vsX, int vsY);
    void MoveRubberband(wxDC &dc, int fromX, int fromY,     // change rubber band
        int prevToX, int prevToY, int toX, int toY, int vsX, int vsY);
    void RemoveRubberband(wxDC& dc, int fromX, int fromY,   // remove rubber band
        int toX, int toY, int vsX, int vsY);

    DECLARE_EVENT_TABLE()

public:
    int GetCellHeight(void) const { return cellHeight; }
    int GetAreaHeight(void) const { return areaHeight; }
    void SetTitleArea(SequenceViewerWidget_TitleArea *titleA)
        { titleArea = titleA; }
    SequenceViewerWidget::eMouseMode GetMouseMode(void) const { return mouseMode; }
};


///////////////////////////////////////////////////////////////////////////
///////// This is the implementation of the sequence drawing area /////////
///////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(SequenceViewerWidget_SequenceArea, wxScrolledWindow)
    EVT_PAINT               (SequenceViewerWidget_SequenceArea::OnPaint)
    EVT_MOUSE_EVENTS        (SequenceViewerWidget_SequenceArea::OnMouseEvent)
    EVT_SCROLLWIN           (SequenceViewerWidget_SequenceArea::OnScrollWin)
END_EVENT_TABLE()

SequenceViewerWidget_SequenceArea::SequenceViewerWidget_SequenceArea(
        wxWindow* parent,
        wxWindowID id,
        const wxPoint& pos,
        const wxSize& size,
        long style,
        const wxString& name
    ) :
    wxScrolledWindow(parent, -1, pos, size, style, name),
    alignment(NULL), currentFont(NULL), titleArea(NULL), bitmap(NULL)
{
    // set default background color
    currentBackgroundColor = *wxWHITE;

    // set default mouse mode
    mouseMode = SequenceViewerWidget::eSelectRectangle;

    // set default rubber band color
    currentRubberbandColor = *wxRED;
}

SequenceViewerWidget_SequenceArea::~SequenceViewerWidget_SequenceArea(void)
{
    if (currentFont) delete currentFont;
    if (bitmap) delete bitmap;
}

bool SequenceViewerWidget_SequenceArea::AttachAlignment(
    ViewableAlignment *newAlignment, int initX, int initY)
{
    alignment = newAlignment;

    if (alignment) {
        // set size of virtual area
        alignment->GetSize(&areaWidth, &areaHeight);
        if (areaWidth <= 0 || areaHeight <= 0) return false;
//        TESTMSG(Info << "area height " << areaHeight);

        // "+1" to make sure last real column and row are always visible, even
        // if visible area isn't exact multiple of cell size
        SetScrollbars(
            cellWidth, cellHeight,
            areaWidth + 1, areaHeight + 1,
            initX, initY);

        alignment->MouseOver(-1, -1);

    } else {
        // remove scrollbars
//        areaWidth = areaHeight = 2;
//        SetScrollbars(10, 10, 2, 2, 0, 0);   //can't do this without crash on win98... ?
    }

    return true;
}

void SequenceViewerWidget_SequenceArea::SetBackgroundColor(const wxColor& backgroundColor)
{
    currentBackgroundColor = backgroundColor;
}

void SequenceViewerWidget_SequenceArea::SetCharacterFont(wxFont *font)
{
    if (!font) return;

    wxClientDC dc(this);
    dc.SetFont(wxNullFont);

    if (currentFont) delete currentFont;
    currentFont = font;

    dc.SetFont(*currentFont);
    wxCoord chW, chH;
    dc.SetMapMode(wxMM_TEXT);
    dc.GetTextExtent("A", &chW, &chH);
    cellWidth = chW + 1;
	cellHeight = chH;

    // need to reset scrollbars and virtual area size
    AttachAlignment(alignment, 0, 0);
}

void SequenceViewerWidget_SequenceArea::SetMouseMode(SequenceViewerWidget::eMouseMode mode)
{
    mouseMode = mode;
}

void SequenceViewerWidget_SequenceArea::SetRubberbandColor(const wxColor& rubberbandColor)
{
    currentRubberbandColor = rubberbandColor;
}

void SequenceViewerWidget_SequenceArea::OnPaint(wxPaintEvent& event)
{
    // adjust bitmap size to match client area size
    if (!bitmap || bitmap->GetWidth() != GetClientSize().GetWidth() ||
                   bitmap->GetHeight() != GetClientSize().GetHeight()) {
        if (bitmap) delete bitmap;
        bitmap = new wxBitmap(GetClientSize().GetWidth(), GetClientSize().GetHeight());
    }

    wxMemoryDC memDC;
    memDC.SelectObject(*bitmap);

    int vsX, vsY,
        updLeft, updRight, updTop, updBottom,
        firstCellX, firstCellY,
        lastCellX, lastCellY,
        x, y;
    static int prevVsY = -1;

    memDC.BeginDrawing();

    // set font for characters
    if (alignment) {
        memDC.SetFont(*currentFont);
        memDC.SetMapMode(wxMM_TEXT);
        // characters to be drawn transparently over background
        memDC.SetBackgroundMode(wxTRANSPARENT);

        // get upper left corner of visible area
        GetViewStart(&vsX, &vsY);  // returns coordinates in scroll units (cells)
//        TRACEMSG("vsX=" << vsX << " vsY=" << vsY);
        if (vsY != prevVsY) {
            if (titleArea) titleArea->Refresh();
            prevVsY = vsY;
        }
    }

    // get the update rect list, so that we can draw *only* the cells
    // in the part of the window that needs redrawing; update region
    // coordinates are relative to the visible part of the drawing area
    wxRegionIterator upd(GetUpdateRegion());

    for (; upd; ++upd) {
//        TRACEMSG("upd: x=" << upd.GetX() << " y=" << upd.GetY() <<
//            " w=" << upd.GetW() << " h=" << upd.GetH());

        // draw background
        memDC.SetPen(*(wxThePenList->
            FindOrCreatePen(currentBackgroundColor, 1, wxSOLID)));
        memDC.SetBrush(*(wxTheBrushList->
            FindOrCreateBrush(currentBackgroundColor, wxSOLID)));
        memDC.DrawRectangle(upd.GetX(), upd.GetY(), upd.GetW(), upd.GetH());

        if (!alignment) continue;

        // figure out which cells contain the corners of the update region

        // get coordinates of update region corners relative to virtual area
        updLeft = vsX*cellWidth + upd.GetX();
        updTop = vsY*cellHeight + upd.GetY();
        updRight = updLeft + upd.GetW() - 1;
        updBottom = updTop + upd.GetH() - 1;

        // firstCell[X,Y] is upper leftmost cell to draw, and is the cell
        // that contains the upper left corner of the update region
        firstCellX = updLeft / cellWidth;
        firstCellY = updTop / cellHeight;

        // lastCell[X,Y] is the lower rightmost cell displayed; including partial
        // cells if the visible area isn't an exact multiple of cell size. (It
        // turns out to be very difficult to only display complete cells...)
        lastCellX = updRight / cellWidth;
        lastCellY = updBottom / cellHeight;

        // restrict to size of virtual area, if visible area is larger
        // than the virtual area
        if (lastCellX >= areaWidth) lastCellX = areaWidth - 1;
        if (lastCellY >= areaHeight) lastCellY = areaHeight - 1;

        // draw cells
//        TESTMSG("drawing cells " << firstCellX << ',' << firstCellY
//            << " to " << lastCellX << ',' << lastCellY);
        for (y=firstCellY; y<=lastCellY; ++y) {
            for (x=firstCellX; x<=lastCellX; ++x) {
                DrawCell(memDC, x, y, vsX, vsY, false);
            }
        }
    }

    memDC.EndDrawing();

    // Blit from memory DC to paintDC to avoid flicker
    wxPaintDC paintDC(this);
    paintDC.Blit(0, 0, GetClientSize().GetWidth(), GetClientSize().GetHeight(), &memDC, 0,0, wxCOPY);
//    TRACEMSG("Blit 0, 0, " << GetClientSize().GetWidth() << ", " << GetClientSize().GetHeight());
}

void SequenceViewerWidget_SequenceArea::DrawCell(wxDC& dc, int x, int y, int vsX, int vsY, bool redrawBackground)
{
	char character;
    wxColor color, cellBackgroundColor;
    bool drawBackground, drawChar;

    drawChar = alignment->GetCharacterTraitsAt(x, y, &character, &color, &drawBackground, &cellBackgroundColor);

    // adjust x,y into visible area coordinates
    x = (x - vsX) * cellWidth;
    y = (y - vsY) * cellHeight;

    // if necessary, redraw background with appropriate color
    if ((drawChar && drawBackground) || redrawBackground) {
        if (drawChar && drawBackground) {
            dc.SetPen(*(wxThePenList->FindOrCreatePen(cellBackgroundColor, 1, wxSOLID)));
            dc.SetBrush(*(wxTheBrushList->FindOrCreateBrush(cellBackgroundColor, wxSOLID)));
        } else {
            dc.SetPen(*(wxThePenList->FindOrCreatePen(currentBackgroundColor, 1, wxSOLID)));
            dc.SetBrush(*(wxTheBrushList->FindOrCreateBrush(currentBackgroundColor, wxSOLID)));
        }
        dc.DrawRectangle(x, y, cellWidth, cellHeight);
    }

    if (!drawChar) return;

    // set character color
    dc.SetTextForeground(color);

    // measure character size
    wxString buf(character);
    wxCoord chW, chH;
    dc.GetTextExtent(buf, &chW, &chH);

    // draw character in the middle of the cell
    dc.DrawText(buf,
        x + (cellWidth - chW)/2,
        y + (cellHeight - chH)/2
    );
}

static inline void min_max(int a, int b, int *c, int *d)
{
    if (a <= b) {
        *c = a;
        *d = b;
    } else {
        *c = b;
        *d = a;
    }
}

void SequenceViewerWidget_SequenceArea::DrawLine(wxDC& dc, int x1, int y1, int x2, int y2)
{
    if (currentRubberbandType == eSolid) {
        dc.DrawLine(x1, y1, x2, y2);
    } else { // short-dashed line
        int i, ie;
        if (x1 == x2) { // vertical line
            min_max(y1, y2, &i, &ie);
            ie -= 1;
            for (; i<=ie; ++i)
                if (i%4 == 0) dc.DrawLine(x1, i, x1, i + 2);
        } else {        // horizontal line
            min_max(x1, x2, &i, &ie);
            ie -= 1;
            for (; i<=ie; ++i)
                if (i%4 == 0) dc.DrawLine(i, y1, i + 2, y1);
        }
    }
}

// draw a rubberband around the cells
void SequenceViewerWidget_SequenceArea::DrawRubberband(wxDC& dc, int fromX, int fromY, int toX, int toY, int vsX, int vsY)
{
    // find upper-left and lower-right corners
    int minX, minY, maxX, maxY;
    min_max(fromX, toX, &minX, &maxX);
    min_max(fromY, toY, &minY, &maxY);

    // convert to pixel coordinates of corners
    minX = (minX - vsX) * cellWidth;
    minY = (minY - vsY) * cellHeight;
    maxX = (maxX - vsX) * cellWidth + cellWidth - 1;
    maxY = (maxY - vsY) * cellHeight + cellHeight - 1;

    // set color
    dc.SetPen(*(wxThePenList->FindOrCreatePen(currentRubberbandColor, 1, wxSOLID)));

    // draw sides (should draw in order, due to pixel roundoff)
    if (mouseMode != SequenceViewerWidget::eSelectColumns && mouseMode != SequenceViewerWidget::eSelectBlocks)
        DrawLine(dc, minX, minY, maxX, minY);   // top
    if (mouseMode != SequenceViewerWidget::eSelectRows)
        DrawLine(dc, maxX, minY, maxX, maxY);   // right
    if (mouseMode != SequenceViewerWidget::eSelectColumns && mouseMode != SequenceViewerWidget::eSelectBlocks)
        DrawLine(dc, maxX, maxY, minX, maxY);   // bottom
    if (mouseMode != SequenceViewerWidget::eSelectRows)
        DrawLine(dc, minX, maxY, minX, minY);   // left
}

// move the rubber band to a new rectangle, erasing only the side(s) of the
// rectangle that is changing
void SequenceViewerWidget_SequenceArea::MoveRubberband(wxDC &dc, int fromX, int fromY,
    int prevToX, int prevToY, int toX, int toY, int vsX, int vsY)
{
    int i;

    if ((prevToX >= fromX && toX < fromX) ||
        (prevToX < fromX && toX >= fromX) ||
        (prevToY >= fromY && toY < fromY) ||
        (prevToY < fromY && toY >= fromY)) {
        // need to completely redraw if rectangle is "flipped"
        RemoveRubberband(dc, fromX, fromY, prevToX, prevToY, vsX, vsY);

    } else {
        int a, b;

        // erase moving bottom/top side if dragging up/down
        if (toY != prevToY) {
            min_max(fromX, prevToX, &a, &b);
            for (i=a; i<=b; ++i) DrawCell(dc, i, prevToY, vsX, vsY, true);
        }

        // erase partial top and bottom if dragging left by more than one
        a = -1; b = -2;
        if (fromX <= toX && toX < prevToX) {
            a = toX + 1;
            b = prevToX - 1;
        } else if (prevToX < toX && toX < fromX) {
            a = prevToX + 1;
            b = toX - 1;
        }
        for (i=a; i<=b; ++i) {
            DrawCell(dc, i, fromY, vsX, vsY, true);
            DrawCell(dc, i, prevToY, vsX, vsY, true);
        }

        // erase moving left/right side
        if (toX != prevToX) {
            min_max(fromY, prevToY, &a, &b);
            for (i=a; i<=b; ++i) DrawCell(dc, prevToX, i, vsX, vsY, true);
        }

        // erase partial left and right sides if dragging up/down by more than one
        a = -1; b = -2;
        if (fromY <= toY && toY < prevToY) {
            a = toY + 1;
            b = prevToY - 1;
        } else if (prevToY < toY && toY < fromY) {
            a = prevToY + 1;
            b = toY - 1;
        }
        for (i=a; i<=b; ++i) {
            DrawCell(dc, fromX, i, vsX, vsY, true);
            DrawCell(dc, prevToX, i, vsX, vsY, true);
        }
    }

    // redraw whole new one
    DrawRubberband(dc, fromX, fromY, toX, toY, vsX, vsY);
}

// redraw only those cells necessary to remove rubber band
void SequenceViewerWidget_SequenceArea::RemoveRubberband(wxDC& dc, int fromX, int fromY, int toX, int toY, int vsX, int vsY)
{
    int i, min, max;

    // remove top and bottom
    min_max(fromX, toX, &min, &max);
    for (i=min; i<=max; ++i) {
        DrawCell(dc, i, fromY, vsX, vsY, true);
        DrawCell(dc, i, toY, vsX, vsY, true);
    }
    // remove left and right
    min_max(fromY, toY, &min, &max);
    for (i=min+1; i<=max-1; ++i) {
        DrawCell(dc, fromX, i, vsX, vsY, true);
        DrawCell(dc, toX, i, vsX, vsY, true);
    }
}

void SequenceViewerWidget_SequenceArea::OnScrollWin(wxScrollWinEvent& event)
{
    // when scrolling happens via scrollbars (or movement keys), need to update status bar info.
    // So, fake a (non-moving) mouse wheel event, which will trigger MouseOver.
    wxMouseEvent fake(wxEVT_MOUSEWHEEL);
    fake.m_wheelRotation = 0;
    fake.m_wheelDelta = 120;
    fake.m_linesPerAction = 3;
    AddPendingEvent(fake);
    event.Skip();   // continue to process this event normally
}

void SequenceViewerWidget_SequenceArea::OnMouseEvent(wxMouseEvent& event)
{
    static const ViewableAlignment *prevAlignment = NULL;
    static int prevMOX = -1, prevMOY = -1;
    static bool dragging = false;

    if (!alignment) {
        prevAlignment = NULL;
        prevMOX = prevMOY = -1;
        dragging = false;
        return;
    }
    if (alignment != prevAlignment) {
        prevMOX = prevMOY = -1;
        prevAlignment = alignment;
        dragging = false;
    }

    // get coordinates of mouse when it's over the drawing area
    wxCoord mX, mY;
    event.GetPosition(&mX, &mY);

    // get current view window location
    int vsX, vsY;
    GetViewStart(&vsX, &vsY);

    // handle wheel events
    static wxCoord windowMX = 0, windowMY = 0;
    bool wheelEvent = (event.GetEventType() == wxEVT_MOUSEWHEEL);
    if (wheelEvent) {
        if (dragging || windowMX < 0 || windowMY < 0 ||
                windowMX >= GetClientSize().GetWidth() || windowMY >= GetClientSize().GetHeight())
            return;
        mX = windowMX;  // coords on mouse wheel event seem to be screen-relative, not window-relative
        mY = windowMY;
        static int accumulatedRotation = 0;
        accumulatedRotation -= event.GetWheelRotation();    // move wheel up -> scroll up
        int nDeltas = accumulatedRotation / event.GetWheelDelta();
        if (nDeltas != 0) {
            accumulatedRotation -= nDeltas * event.GetWheelDelta();
            int toY = vsY + nDeltas * event.GetLinesPerAction();
            if (toY < 0)
                toY = 0;
            else if (toY >= areaHeight)
                toY = areaHeight - 1;
            Scroll(-1, toY);
            GetViewStart(&vsX, &vsY);   // update vsY so that MouseOver is called on new position
        }
    } else {
        windowMX = mX;
        windowMY = mY;
    }

    // translate visible area coordinates to cell coordinates
    int cellX, cellY, MOX, MOY;
    cellX = MOX = vsX + mX / cellWidth;
    cellY = MOY = vsY + mY / cellHeight;

    // if the mouse is leaving the window, use cell coordinates of most
    // recent known mouse-over cell
    if (event.Leaving()) {
        cellX = prevMOX;
        cellY = prevMOY;
        MOX = MOY = -1;
    }

    // do MouseOver if not in the same cell (or outside area) as last time
    if (MOX >= areaWidth || MOY >= areaHeight)
        MOX = MOY = -1;
    if (MOX != prevMOX || MOY != prevMOY)
        alignment->MouseOver(MOX, MOY);
    prevMOX = MOX;
    prevMOY = MOY;

    if (wheelEvent)
        return;

    // adjust for column/row selection
    if (mouseMode == SequenceViewerWidget::eSelectColumns || mouseMode == SequenceViewerWidget::eSelectBlocks)
        cellY = vsY + GetClientSize().GetHeight() / cellHeight;
    else if (mouseMode == SequenceViewerWidget::eSelectRows)
        cellX = vsX + GetClientSize().GetWidth() / cellWidth;

    // limit coordinates of selection to virtual area
    if (cellX < 0) cellX = 0;
    else if (cellX >= areaWidth) cellX = areaWidth - 1;
    if (cellY < 0) cellY = 0;
    else if (cellY >= areaHeight) cellY = areaHeight - 1;


    // keep track of position of selection start, as well as last
    // cell dragged to during selection
    static int fromX, fromY, prevToX, prevToY;

    // limit dragging movement if necessary
    if (dragging) {
        if (mouseMode == SequenceViewerWidget::eDragHorizontal) cellY = fromY;
        if (mouseMode == SequenceViewerWidget::eDragVertical) cellX = fromX;
    }

    // process beginning of selection
    if (event.LeftDown()) {

        // find out which (if any) control keys are down at this time
        unsigned int controls = 0;
        if (event.ShiftDown()) controls |= ViewableAlignment::eShiftDown;
#ifdef __WXMAC__
        if (event.MetaDown())      // control key + mouse doesn't work on Mac?
#else
        if (event.ControlDown())
#endif
            controls |= ViewableAlignment::eControlDown;
        if (event.AltDown() || event.MetaDown()) controls |= ViewableAlignment::eAltOrMetaDown;

        // send MouseDown message; don't start selection if MouseDown returns false
        // or if not inside display area
        if (alignment->MouseDown(MOX, MOY, controls) && MOX != -1) {
            prevToX = fromX = cellX;
            prevToY = fromY = cellY;
            dragging = true;

            TRACEMSG("drawing initial rubberband");
            wxClientDC dc(this);
            dc.BeginDrawing();
            currentRubberbandType =
                (mouseMode == SequenceViewerWidget::eSelectRectangle ||
                 mouseMode == SequenceViewerWidget::eSelectColumns ||
                 mouseMode == SequenceViewerWidget::eSelectRows ||
                 mouseMode == SequenceViewerWidget::eSelectBlocks) ? eDot : eSolid;

            if (mouseMode == SequenceViewerWidget::eSelectColumns || mouseMode == SequenceViewerWidget::eSelectBlocks) {
                fromY = vsY;
                prevToY = cellY;
                DrawRubberband(dc, fromX, fromY, fromX, cellY, vsX, vsY);
            } else if (mouseMode == SequenceViewerWidget::eSelectRows) {
                fromX = vsX;
                prevToX = cellX;
                DrawRubberband(dc, fromX, fromY, cellX, fromY, vsX, vsY);
            } else {
                DrawRubberband(dc, fromX, fromY, fromX, fromY, vsX, vsY);
            }
            dc.EndDrawing();
        }
    }

    // process end of selection, on mouse-up, or if the mouse leaves the window
    else if (dragging && (event.LeftUp() || event.Leaving() || event.Entering())) {
        if (!event.LeftUp()) {
            cellX = prevToX;
            cellY = prevToY;
        }

        dragging = false;
        wxClientDC dc(this);
        dc.BeginDrawing();
        dc.SetFont(*currentFont);

        // remove rubberband
        if (mouseMode == SequenceViewerWidget::eSelectRectangle ||
            mouseMode == SequenceViewerWidget::eSelectColumns ||
            mouseMode == SequenceViewerWidget::eSelectRows ||
            mouseMode == SequenceViewerWidget::eSelectBlocks)
        {
            RemoveRubberband(dc, fromX, fromY, cellX, cellY, vsX, vsY);
        } else {
            DrawCell(dc, fromX, fromY, vsX, vsY, true);
            if (cellX != fromX || cellY != fromY)
                DrawCell(dc, cellX, cellY, vsX, vsY, true);
        }
        dc.EndDrawing();

        // adjust for column/row selection
        if (mouseMode == SequenceViewerWidget::eSelectColumns || mouseMode == SequenceViewerWidget::eSelectBlocks) {
            fromY = 0;
            cellY = areaHeight - 1;
        } else if (mouseMode == SequenceViewerWidget::eSelectRows) {
            fromX = 0;
            cellX = areaWidth - 1;
        }

        // do appropriate callback
        if (mouseMode == SequenceViewerWidget::eSelectRectangle ||
            mouseMode == SequenceViewerWidget::eSelectColumns ||
            mouseMode == SequenceViewerWidget::eSelectRows ||
            mouseMode == SequenceViewerWidget::eSelectBlocks)
        {
            alignment->SelectedRectangle(
                (fromX < cellX) ? fromX : cellX,
                (fromY < cellY) ? fromY : cellY,
                (cellX > fromX) ? cellX : fromX,
                (cellY > fromY) ? cellY : fromY);
        } else {
            alignment->DraggedCell(fromX, fromY, cellX, cellY);
        }
    }

    // process continuation of selection - redraw rectangle
    else if (dragging && (cellX != prevToX || cellY != prevToY)) {

        wxClientDC dc(this);
        dc.BeginDrawing();
        dc.SetFont(*currentFont);
        currentRubberbandType = eDot;
        if (mouseMode == SequenceViewerWidget::eSelectRectangle ||
            mouseMode == SequenceViewerWidget::eSelectColumns ||
            mouseMode == SequenceViewerWidget::eSelectRows ||
            mouseMode == SequenceViewerWidget::eSelectBlocks)
        {
            MoveRubberband(dc, fromX, fromY, prevToX, prevToY, cellX, cellY, vsX, vsY);
        } else {
            if (prevToX != fromX || prevToY != fromY)
                DrawCell(dc, prevToX, prevToY, vsX, vsY, true);
            if (cellX != fromX || cellY != fromY) {
                DrawRubberband(dc, cellX, cellY, cellX, cellY, vsX, vsY);
            }
        }
        dc.EndDrawing();
        prevToX = cellX;
        prevToY = cellY;
    }
}


////////////////////////////////////////////////////////////////////////
///////// This is the implementation of the title drawing area /////////
////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(SequenceViewerWidget_TitleArea, wxWindow)
    EVT_PAINT               (SequenceViewerWidget_TitleArea::OnPaint)
    EVT_MOUSE_EVENTS        (SequenceViewerWidget_TitleArea::OnMouseEvent)
END_EVENT_TABLE()

SequenceViewerWidget_TitleArea::SequenceViewerWidget_TitleArea(
        wxWindow* parent,
        wxWindowID id,
        const wxPoint& pos,
        const wxSize& size) :
    wxWindow(parent, id, pos, size, wxNO_3D),
    titleFont(NULL), cellHeight(0), maxTitleWidth(20), alignment(NULL),
    sequenceArea(NULL), highlightedTitleRow(-1)
{
    currentBackgroundColor = *wxWHITE;
}

SequenceViewerWidget_TitleArea::~SequenceViewerWidget_TitleArea(void)
{
    if (titleFont) delete titleFont;
}

void SequenceViewerWidget_TitleArea::ShowTitles(ViewableAlignment *newAlignment)
{
    alignment = newAlignment;
    highlightedTitleRow = -1;
    if (!alignment) return;

    // set font
    wxClientDC dc(this);
    dc.SetFont(*titleFont);
    dc.SetMapMode(wxMM_TEXT);

    int i;
    wxString title;
    wxColor color;
    wxCoord tW, tH;

    // get maximum width of any title
    alignment->GetSize(&i, &nTitles);
    if (nTitles <= 0) return;
    maxTitleWidth = 20;
    for (i=0; i<nTitles; ++i) {
        if (!alignment->GetRowTitle(i, &title, &color)) continue;
        // measure title size
        dc.GetTextExtent(title, &tW, &tH);
        if (tW > maxTitleWidth) maxTitleWidth = tW;
    }
}

void SequenceViewerWidget_TitleArea::SetCharacterFont(wxFont *font, int newCellHeight)
{
    if (!font) return;

    if (titleFont) delete titleFont;
    titleFont = font;
    cellHeight = newCellHeight;

    ShowTitles(alignment);
}

void SequenceViewerWidget_TitleArea::SetBackgroundColor(const wxColor& backgroundColor)
{
    currentBackgroundColor = backgroundColor;
}

void SequenceViewerWidget_TitleArea::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);

    int vsX, vsY,
        updTop, updBottom,
        firstRow, lastRow, row;

    dc.BeginDrawing();

    // set font for characters
    if (alignment) {
        dc.SetFont(*titleFont);
        dc.SetMapMode(wxMM_TEXT);
        // characters to be drawn transparently over background
        dc.SetBackgroundMode(wxTRANSPARENT);

        // get upper left corner of visible area
        sequenceArea->GetViewStart(&vsX, &vsY);  // returns coordinates in scroll units (cells)
    }

    // get the update rect list, so that we can draw *only* the cells
    // in the part of the window that needs redrawing; update region
    // coordinates are relative to the visible part of the drawing area
    wxRegionIterator upd(GetUpdateRegion());

    for (; upd; ++upd) {

//        if (upd.GetW() == GetClientSize().GetWidth() &&
//            upd.GetH() == GetClientSize().GetHeight())
//            TESTMSG("repainting whole SequenceViewerWidget_TitleArea");

        // draw background
        dc.SetPen(*(wxThePenList->
            FindOrCreatePen(currentBackgroundColor, 1, wxSOLID)));
        dc.SetBrush(*(wxTheBrushList->
            FindOrCreateBrush(currentBackgroundColor, wxSOLID)));
        dc.DrawRectangle(upd.GetX(), upd.GetY(), upd.GetW(), upd.GetH());

        if (!alignment) continue;

        // figure out which cells contain the corners of the update region

        // get coordinates of update region corners relative to virtual area
        updTop = vsY*cellHeight + upd.GetY();
        updBottom = updTop + upd.GetH() - 1;
        firstRow = updTop / cellHeight;
        lastRow = updBottom / cellHeight;

        // restrict to size of virtual area, if visible area is larger
        // than the virtual area
        if (lastRow >= nTitles) lastRow = nTitles - 1;

        // draw titles
        wxString title;
        wxColor color;
        wxCoord tW, tH;
        for (row=firstRow; row<=lastRow; ++row) {

            if (!alignment->GetRowTitle(row, &title, &color)) continue;

            // set character color
            if (row == highlightedTitleRow)
                dc.SetTextForeground(*wxRED);
            else
                dc.SetTextForeground(color);

            // draw title centered vertically in the cell
            dc.GetTextExtent(title, &tW, &tH);
            dc.DrawText(title, 0, cellHeight * (row - vsY) + (cellHeight - tH)/2);
        }
    }

    dc.EndDrawing();
}

void SequenceViewerWidget_TitleArea::OnMouseEvent(wxMouseEvent& event)
{
    static const ViewableAlignment *prevAlignment = NULL;
    static int prevMOY = -1;

    if (!alignment) {
        prevAlignment = NULL;
        prevMOY = -1;
        return;
    }
    if (alignment != prevAlignment) {
        prevMOY = -1;
        prevAlignment = alignment;
    }

    // get coordinates of mouse when it's over the drawing area
    wxCoord mX, mY;
    event.GetPosition(&mX, &mY);

    // translate visible area coordinates to cell coordinates
    int vsX, vsY, MOY;
    sequenceArea->GetViewStart(&vsX, &vsY);
    MOY = vsY + mY / cellHeight;

    // if the mouse is leaving the window or is outside area
    if (event.Leaving() || MOY >= sequenceArea->GetAreaHeight())
        MOY = -1;

    // do MouseOver if not in the same cell as last time
    if (MOY != prevMOY)
        alignment->MouseOver(-1, MOY);
    prevMOY = MOY;

    // process button press
    if (event.LeftDown()) {
        // find out which (if any) control keys are down at this time
        unsigned int controls = 0;
        if (event.ShiftDown()) controls |= ViewableAlignment::eShiftDown;
#ifdef __WXMAC__
        if (event.MetaDown())      // control key + mouse doesn't work on Mac?
#else
        if (event.ControlDown())
#endif
            controls |= ViewableAlignment::eControlDown;
        if (event.AltDown() || event.MetaDown()) controls |= ViewableAlignment::eAltOrMetaDown;

        // send MouseDown message
        alignment->MouseDown(-1, MOY, controls);

        // (temporarily) highlight this title
        highlightedTitleRow = MOY;
        Refresh();
    }
}

//////////////////////////////////////////////////////////////////////
///////// This is the implementation of SequenceViewerWidget /////////
//////////////////////////////////////////////////////////////////////

SequenceViewerWidget::SequenceViewerWidget(
        wxWindow* parent,
        wxWindowID id,
        const wxPoint& pos,
        const wxSize& size) :
    wxSplitterWindow(parent, -1, wxPoint(0,0), parent->GetClientSize(), wxSP_3DBORDER | wxNO_3D)
{
    sequenceArea = new SequenceViewerWidget_SequenceArea(this);
    titleArea = new SequenceViewerWidget_TitleArea(this);

    sequenceArea->SetTitleArea(titleArea);
    titleArea->SetSequenceArea(sequenceArea);

    SplitVertically(titleArea, sequenceArea);
}

SequenceViewerWidget::~SequenceViewerWidget(void)
{
}

void SequenceViewerWidget::OnDoubleClickSash(int x, int y)
{
    Unsplit(titleArea);
}

void SequenceViewerWidget::TitleAreaOn(void)
{
    if (!IsSplit()) {
        titleArea->Show(true);
        SplitVertically(titleArea, sequenceArea, titleArea->GetMaxTitleWidth() + 10);
    }
}

void SequenceViewerWidget::TitleAreaOff(void)
{
    if (IsSplit())
        Unsplit(titleArea);
}

void SequenceViewerWidget::TitleAreaToggle(void)
{
    if (IsSplit())
        Unsplit(titleArea);
    else {
        titleArea->Show(true);
        SplitVertically(titleArea, sequenceArea, titleArea->GetMaxTitleWidth() + 10);
    }
}

bool SequenceViewerWidget::AttachAlignment(ViewableAlignment *newAlignment, int initX, int initY)
{
    // do titles first, since on Mac sequence area update causes title redraw
    titleArea->ShowTitles(newAlignment);
    sequenceArea->AttachAlignment(newAlignment, initX, initY);

    SetSashPosition(titleArea->GetMaxTitleWidth() + 10, true);
	return true;
}

void SequenceViewerWidget::SetMouseMode(eMouseMode mode)
{
    sequenceArea->SetMouseMode(mode);
}

SequenceViewerWidget::eMouseMode SequenceViewerWidget::GetMouseMode(void) const
{
    return sequenceArea->GetMouseMode();
}

void SequenceViewerWidget::SetBackgroundColor(const wxColor& backgroundColor)
{
    sequenceArea->SetBackgroundColor(backgroundColor);
    titleArea->SetBackgroundColor(backgroundColor);
}

void SequenceViewerWidget::SetCharacterFont(wxFont *font)
{
    wxFont *newFont = new wxFont(font->GetPointSize(), font->GetFamily(),
        wxNORMAL, wxNORMAL, false, font->GetFaceName(), font->GetDefaultEncoding());
    sequenceArea->SetCharacterFont(newFont);
    newFont = new wxFont(font->GetPointSize(), font->GetFamily(),
        wxITALIC, wxNORMAL, false, font->GetFaceName(), font->GetDefaultEncoding());
    titleArea->SetCharacterFont(newFont, sequenceArea->GetCellHeight());
    delete font;

    // toggle title area twice to reset the sash to the right width - crude but effective
    TitleAreaToggle();
    TitleAreaToggle();
    Refresh(true);
}

void SequenceViewerWidget::SetRubberbandColor(const wxColor& rubberbandColor)
{
    sequenceArea->SetRubberbandColor(rubberbandColor);
}

void SequenceViewerWidget::ScrollTo(int column, int row)
{
    sequenceArea->Scroll(column, row);
}

void SequenceViewerWidget::GetScroll(int *vsX, int *vsY) const
{
    sequenceArea->GetViewStart(vsX, vsY);
}

void SequenceViewerWidget::MakeCharacterVisible(int column, int row) const
{
    int vsX, vsY, nCells;
    sequenceArea->GetViewStart(&vsX, &vsY);

    // scroll horizontally if necessary
    if (column >= 0) {
        nCells = sequenceArea->GetClientSize().GetWidth() / sequenceArea->cellWidth;
        if (column < vsX || column >= vsX + nCells) {
            vsX = column - nCells / 2;
            if (vsX < 0) vsX = 0;
        }
    }

    // scroll vertically if necessary
    if (row >= 0) {
        nCells = sequenceArea->GetClientSize().GetHeight() / sequenceArea->cellHeight;
        if (row < vsY || row >= vsY + nCells) {
            vsY = row - nCells / 2;
            if (vsY < 0) vsY = 0;
        }
    }

    sequenceArea->Scroll(vsX, vsY);
}

void SequenceViewerWidget::Refresh(bool eraseBackground, const wxRect *rect)
{
    sequenceArea->Refresh(eraseBackground, rect);
    titleArea->Refresh(eraseBackground, rect);
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.43  2005/04/22 13:43:01  thiessen
* add block highlighting and structure alignment based on highlighted positions only
*
* Revision 1.42  2004/05/22 15:45:53  thiessen
* support mouse wheel; update status bar on scroll
*
* Revision 1.41  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.40  2004/03/15 18:32:03  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.39  2004/02/19 17:05:10  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.38  2003/02/03 19:20:06  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.37  2003/01/22 14:47:30  thiessen
* cache PSSM in BlockMultipleAlignment
*
* Revision 1.36  2002/10/07 13:29:32  thiessen
* add double-click -> show row to taxonomy tree
*
* Revision 1.35  2002/08/15 22:13:16  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.34  2002/04/27 16:32:14  thiessen
* fix small leaks/bugs found by BoundsChecker
*
* Revision 1.33  2002/04/26 13:46:38  thiessen
* comment out all blast/pssm methods
*
* Revision 1.32  2002/02/22 14:24:01  thiessen
* sort sequences in reject dialog ; general identifier comparison
*
* Revision 1.31  2002/02/21 12:26:30  thiessen
* fix row delete bug ; remember threader options
*
* Revision 1.30  2002/02/05 18:53:25  thiessen
* scroll to residue in sequence windows when selected in structure window
*
* Revision 1.29  2001/11/01 19:01:45  thiessen
* use meta key instead of ctrl on Mac
*
* Revision 1.28  2001/09/26 15:27:54  thiessen
* tweak sequence viewer widget for wx2.3.2, tweak cdd annotation
*
* Revision 1.27  2001/08/14 17:18:22  thiessen
* add user font selection, store in registry
*
* Revision 1.26  2001/07/19 19:14:38  thiessen
* working CDD alignment annotator ; misc tweaks
*
* Revision 1.25  2001/06/07 19:05:38  thiessen
* functional (although incomplete) render settings panel ; highlight title - not sequence - upon mouse click
*
* Revision 1.24  2001/05/31 18:47:09  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.23  2001/05/31 14:32:33  thiessen
* tweak font handling
*
* Revision 1.22  2001/05/22 19:09:31  thiessen
* many minor fixes to compile/run on Solaris/GTK
*
* Revision 1.21  2001/05/03 14:39:14  thiessen
* put ViewableAlignment in its own (non-wx) header
*
* Revision 1.20  2001/05/02 16:35:15  thiessen
* launch entrez web page on sequence identifier
*
* Revision 1.19  2001/04/18 15:46:53  thiessen
* show description, length, and PDB numbering in status line
*
* Revision 1.18  2001/04/04 00:27:15  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.17  2000/12/21 23:42:16  thiessen
* load structures from cdd's
*
* Revision 1.16  2000/11/02 16:56:02  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.15  2000/10/16 20:03:07  thiessen
* working block creation
*
* Revision 1.14  2000/10/12 16:22:45  thiessen
* working block split
*
* Revision 1.13  2000/10/05 18:34:43  thiessen
* first working editing operation
*
* Revision 1.12  2000/10/04 17:41:31  thiessen
* change highlight color (cell background) handling
*
* Revision 1.11  2000/10/03 18:59:23  thiessen
* added row/column selection
*
* Revision 1.10  2000/10/02 23:25:23  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.9  2000/09/20 22:22:28  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.8  2000/09/14 14:55:35  thiessen
* add row reordering; misc fixes
*
* Revision 1.7  2000/09/12 01:47:39  thiessen
* fix minor but obscure bug
*
* Revision 1.6  2000/09/11 22:57:33  thiessen
* working highlighting
*
* Revision 1.5  2000/09/11 01:46:16  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.4  2000/09/09 14:35:15  thiessen
* fix wxWin problem with large alignments
*
* Revision 1.3  2000/09/07 21:41:40  thiessen
* fix return type of min_max
*
* Revision 1.2  2000/09/07 17:37:35  thiessen
* minor fixes
*
* Revision 1.1  2000/09/03 18:46:49  thiessen
* working generalized sequence viewer
*
* Revision 1.2  2000/08/30 23:46:28  thiessen
* working alignment display
*
* Revision 1.1  2000/08/30 19:48:42  thiessen
* working sequence window
*
*/
