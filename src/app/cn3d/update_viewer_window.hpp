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
#include <algo/structure/wx_tools/wx_tools.hpp>


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
    ncbi::FloatingPointSpinCtrl *fpWeight, *fpLoops;
    ncbi::IntegerSpinCtrl *iStarts, *iResults, *iCutoff;
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
    ncbi::IntegerSpinCtrl *iFrom, *iTo;

    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_UPDATE_VIEWER_WINDOW__HPP
