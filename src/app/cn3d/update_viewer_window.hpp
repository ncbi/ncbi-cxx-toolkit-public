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
*      implementation of GUI part of update viewer
*
* ===========================================================================
*/

#ifndef CN3D_UPDATE_VIEWER_WINDOW__HPP
#define CN3D_UPDATE_VIEWER_WINDOW__HPP

#include "viewer_window_base.hpp"

#include <wx/spinctrl.h>


BEGIN_SCOPE(Cn3D)

class UpdateViewer;
class SequenceDisplay;

class UpdateViewerWindow : public ViewerWindowBase
{
    friend class SequenceDisplay;

public:
    UpdateViewerWindow(UpdateViewer *parentUpdateViewer);
    ~UpdateViewerWindow(void);

    void EnableDerivedEditorMenuItems(bool enabled);

    bool SaveDialog(bool prompt, bool canCancel);

    void SetWindowTitle(void);

private:
    UpdateViewer *updateViewer;

    // menu identifiers - additional items beyond base class items
    enum {
        MID_DELETE_ALL_BLOCKS = START_VIEWER_WINDOW_DERIVED_MID,
        MID_DELETE_BLOCKS_ALL_ROWS,
        MID_SORT_UPDATES,
        MID_SORT_UPDATES_IDENTIFIER,
        MID_SORT_UPDATES_PSSM,
        MID_IMPORT_SEQUENCES,
        MID_IMPORT_STRUCTURE,
        MID_THREAD_ONE,
        MID_THREAD_ALL,
        MID_BLAST_ONE,
        MID_BLAST_PSSM_ONE,
        MID_BLAST_NEIGHBOR,
        MID_BLOCKALIGN_ONE,
        MID_BLOCKALIGN_ALL,
        MID_EXTEND_ONE,
        MID_EXTEND_ALL,
        MID_SET_REGION,
        MID_RESET_REGIONS,
        MID_MERGE_ONE,
        MID_MERGE_NEIGHBOR,
        MID_MERGE_ALL,
        MID_DELETE_ONE,
        MID_DELETE_ALL
    };

    void OnCloseWindow(wxCloseEvent& event);
    void OnSortUpdates(wxCommandEvent& event);
    void OnRunThreader(wxCommandEvent& event);
    void OnRunBlast(wxCommandEvent& event);
    void OnBlockAlign(wxCommandEvent& event);
    void OnExtend(wxCommandEvent& event);
    void OnSetRegion(wxCommandEvent& event);
    void OnMerge(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void OnImport(wxCommandEvent& event);

    void DeleteAllBlocksOff(void)
    {
        menuBar->Check(MID_DELETE_ALL_BLOCKS, false);
        SetCursor(wxNullCursor);
    }
    void ThreadSingleOff(void)
    {
        menuBar->Check(MID_THREAD_ONE, false);
        SetCursor(wxNullCursor);
    }
    void BlastSingleOff(void)
    {
        menuBar->Check(MID_BLAST_ONE, false);
        SetCursor(wxNullCursor);
    }
    void BlastPSSMSingleOff(void)
    {
        menuBar->Check(MID_BLAST_PSSM_ONE, false);
        SetCursor(wxNullCursor);
    }
    void BlastNeighborSingleOff(void)
    {
        menuBar->Check(MID_BLAST_NEIGHBOR, false);
        SetCursor(wxNullCursor);
    }
    void BlockAlignSingleOff(void)
    {
        menuBar->Check(MID_BLOCKALIGN_ONE, false);
        SetCursor(wxNullCursor);
    }
    void ExtendSingleOff(void)
    {
        menuBar->Check(MID_EXTEND_ONE, false);
        SetCursor(wxNullCursor);
    }
    void SetRegionOff(void)
    {
        menuBar->Check(MID_SET_REGION, false);
        SetCursor(wxNullCursor);
    }
    void MergeSingleOff(void)
    {
        menuBar->Check(MID_MERGE_ONE, false);
        SetCursor(wxNullCursor);
    }
    void MergeNeighborOff(void)
    {
        menuBar->Check(MID_MERGE_NEIGHBOR, false);
        SetCursor(wxNullCursor);
    }
    void DeleteSingleOff(void)
    {
        menuBar->Check(MID_DELETE_ONE, false);
        SetCursor(wxNullCursor);
    }

    SequenceViewerWidget::eMouseMode GetMouseModeForCreateAndMerge(void)
    {
        return SequenceViewerWidget::eSelectRectangle;
    }

    DECLARE_EVENT_TABLE()

public:
    bool DoDeleteAllBlocks(void) const { return menuBar->IsChecked(MID_DELETE_ALL_BLOCKS); }
    bool DoThreadSingle(void) const { return menuBar->IsChecked(MID_THREAD_ONE); }
    bool DoBlastSingle(void) const { return menuBar->IsChecked(MID_BLAST_ONE); }
    bool DoBlastPSSMSingle(void) const { return menuBar->IsChecked(MID_BLAST_PSSM_ONE); }
    bool DoBlastNeighborSingle(void) const { return menuBar->IsChecked(MID_BLAST_NEIGHBOR); }
    bool DoBlockAlignSingle(void) const { return menuBar->IsChecked(MID_BLOCKALIGN_ONE); }
    bool DoExtendSingle(void) const { return menuBar->IsChecked(MID_EXTEND_ONE); }
    bool DoSetRegion(void) const { return menuBar->IsChecked(MID_SET_REGION); }
    bool DoMergeSingle(void) const { return menuBar->IsChecked(MID_MERGE_ONE); }
    bool DoMergeNeighbor(void) const { return menuBar->IsChecked(MID_MERGE_NEIGHBOR); }
    bool DoDeleteSingle(void) const { return menuBar->IsChecked(MID_DELETE_ONE); }

    void CancelDerivedSpecialModesExcept(int id)
    {
        if (id != MID_DELETE_ALL_BLOCKS && DoDeleteAllBlocks()) DeleteAllBlocksOff();
        if (id != MID_THREAD_ONE && DoThreadSingle()) ThreadSingleOff();
        if (id != MID_BLAST_ONE && DoBlastSingle()) BlastSingleOff();
        if (id != MID_BLAST_PSSM_ONE && DoBlastPSSMSingle()) BlastPSSMSingleOff();
        if (id != MID_BLAST_NEIGHBOR && DoBlastNeighborSingle()) BlastNeighborSingleOff();
        if (id != MID_BLOCKALIGN_ONE && DoBlockAlignSingle()) BlockAlignSingleOff();
        if (id != MID_EXTEND_ONE && DoExtendSingle()) ExtendSingleOff();
        if (id != MID_SET_REGION && DoSetRegion()) SetRegionOff();
        if (id != MID_MERGE_ONE && DoMergeSingle()) MergeSingleOff();
        if (id != MID_MERGE_NEIGHBOR && DoMergeNeighbor()) MergeNeighborOff();
        if (id != MID_DELETE_ONE && DoDeleteSingle()) DeleteSingleOff();
    }
};


///// dialog used to get threader options from user /////

class ThreaderOptions;
class FloatingPointSpinCtrl;
class IntegerSpinCtrl;
class Sequence;

class ThreaderOptionsDialog : public wxDialog
{
public:
    ThreaderOptionsDialog(wxWindow* parent, const ThreaderOptions& initialOptions);
    ~ThreaderOptionsDialog(void);

    // set the ThreaderOptions from values in the panel; returns true if all values are valid
    bool GetValues(ThreaderOptions *options);

private:
    wxStaticBox *box;
    wxButton *bOK, *bCancel;
    FloatingPointSpinCtrl *fpWeight, *fpLoops;
    IntegerSpinCtrl *iStarts, *iResults, *iCutoff;
    wxCheckBox *bMerge, *bFreeze;

    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

class RegionDialog : public wxDialog
{
public:
    RegionDialog(wxWindow* parentFrame, const Sequence *sequence, int initialFrom, int initialTo);
    ~RegionDialog(void);

    // get the values in the panel; returns true if all values are valid
    bool GetValues(int *from, int *to);

private:
    IntegerSpinCtrl *iFrom, *iTo;

    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_UPDATE_VIEWER_WINDOW__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.40  2006/05/31 19:21:32  thiessen
* add sort by pssm score
*
* Revision 1.39  2004/10/04 17:00:54  thiessen
* add expand/restrict highlights, delete all blocks/all rows in updates
*
* Revision 1.38  2004/09/23 10:31:14  thiessen
* add block extension algorithm
*
* Revision 1.37  2004/02/19 17:05:21  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.36  2003/09/25 15:14:09  thiessen
* add Reset All Regions command
*
* Revision 1.35  2003/02/03 19:20:08  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.34  2003/01/31 17:18:59  thiessen
* many small additions and changes...
*
* Revision 1.33  2003/01/29 01:41:06  thiessen
* add merge neighbor instead of merge near highlight
*
* Revision 1.32  2003/01/23 20:03:05  thiessen
* add BLAST Neighbor algorithm
*
* Revision 1.31  2002/09/19 12:51:08  thiessen
* fix block aligner / update bug; add distance select for other molecules only
*
* Revision 1.30  2002/09/16 21:24:58  thiessen
* add block freezing to block aligner
*
* Revision 1.29  2002/09/09 13:38:23  thiessen
* separate save and save-as
*
* Revision 1.28  2002/08/13 20:46:38  thiessen
* add global block aligner
*
* Revision 1.27  2002/08/01 01:55:16  thiessen
* add block aligner options dialog
*
* Revision 1.26  2002/07/26 15:28:49  thiessen
* add Alejandro's block alignment algorithm
*
* Revision 1.25  2002/06/05 14:28:41  thiessen
* reorganize handling of window titles
*
* Revision 1.24  2002/05/17 19:10:27  thiessen
* preliminary range restriction for BLAST/PSSM
*
* Revision 1.23  2002/05/07 20:22:47  thiessen
* fix for BLAST/PSSM
*
* Revision 1.22  2002/05/02 18:40:25  thiessen
* do BLAST/PSSM for debug builds only, for testing
*
* Revision 1.21  2002/04/27 16:32:16  thiessen
* fix small leaks/bugs found by BoundsChecker
*
* Revision 1.20  2002/04/26 13:46:45  thiessen
* comment out all blast/pssm methods
*
* Revision 1.19  2002/03/28 14:06:02  thiessen
* preliminary BLAST/PSSM ; new CD startup style
*
* Revision 1.18  2002/02/13 14:53:30  thiessen
* add update sort
*
* Revision 1.17  2002/02/12 17:19:23  thiessen
* first working structure import
*
* Revision 1.16  2001/09/27 15:36:48  thiessen
* decouple sequence import and BLAST
*
* Revision 1.15  2001/09/18 03:09:39  thiessen
* add preliminary sequence import pipeline
*
* Revision 1.14  2001/08/06 20:22:49  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.13  2001/06/21 02:01:07  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.12  2001/06/14 18:59:27  thiessen
* left out 'class' in 'friend ...' statments
*
* Revision 1.11  2001/06/01 21:48:02  thiessen
* add terminal cutoff to threading
*
* Revision 1.10  2001/05/31 18:46:28  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.9  2001/05/23 17:43:29  thiessen
* change dialog implementation to wxDesigner; interface changes
*
* Revision 1.8  2001/05/17 18:34:01  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.7  2001/05/02 13:46:16  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.6  2001/04/19 12:58:25  thiessen
* allow merge and delete of individual updates
*
* Revision 1.5  2001/04/12 18:09:40  thiessen
* add block freezing
*
* Revision 1.4  2001/04/04 00:27:22  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.3  2001/03/22 00:32:36  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.2  2001/03/13 01:24:16  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.1  2001/03/09 15:48:44  thiessen
* major changes to add initial update viewer
*
*/
