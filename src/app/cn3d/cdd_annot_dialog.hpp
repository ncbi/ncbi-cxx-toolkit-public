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
*      dialogs for annotating CDD's
*
* ===========================================================================
*/

#ifndef CN3D_CDD_ANNOT_DIALOG__HPP
#define CN3D_CDD_ANNOT_DIALOG__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>

#include <map>
#include <vector>

#include <objects/cdd/Align_annot_set.hpp>
#include <objects/cdd/Feature_evidence.hpp>
#include <objects/seqloc/Seq_interval.hpp>


BEGIN_SCOPE(Cn3D)

class StructureSet;
class BlockMultipleAlignment;
class Sequence;

class CDDAnnotateDialog : public wxDialog
{
public:
    // this is intended to be used as a non-modal dialog
    CDDAnnotateDialog(wxWindow *parent, CDDAnnotateDialog **handle, StructureSet *set);
    ~CDDAnnotateDialog(void);

private:

    CDDAnnotateDialog **dialogHandle;
    StructureSet *structureSet;
    ncbi::CRef < ncbi::objects::CAlign_annot_set > annotSet;

    // get highlighted+aligned intervals on master
    typedef std::list < ncbi::CRef < ncbi::objects::CSeq_interval > > IntervalList;
    void GetCurrentHighlightedIntervals(IntervalList *intervals);

    // action functions
    void NewAnnotation(void);
    void DeleteAnnotation(void);
    void EditAnnotation(void);
    void NewOrEditMotif(void);
    void DeleteMotif(void);
    void HighlightAnnotation(void);
    void HighlightMotif(void);
    void MoveAnnotation(bool moveUp);
    void NewEvidence(void);
    void DeleteEvidence(void);
    void EditEvidence(void);
    void ShowEvidence(void);
    void MoveEvidence(bool moveUp);

    // event callbacks
    void OnButton(wxCommandEvent& event);
    void OnSelection(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);

    // other utility functions
    void SetupGUIControls(int selectAnnot, int selectEvidence);
    bool HighlightInterval(const ncbi::objects::CSeq_interval& interval);

    DECLARE_EVENT_TABLE()
};

class CDDEvidenceDialog : public wxDialog
{
public:
    CDDEvidenceDialog(wxWindow *parent, const ncbi::objects::CFeature_evidence& initial);

private:
    bool changed, rerange;

    // event callbacks
    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnChange(wxCommandEvent& event);

    // utility functions
    void SetupGUIControls(void);

    DECLARE_EVENT_TABLE()

public:
    bool HasDataChanged(void) const { return changed; }
    bool GetData(ncbi::objects::CFeature_evidence *evidence);
};

class CDDTypedAnnotDialog : public wxDialog
{
public:
    //  If 'initial' is non-NULL, it defines the initial state of the dialog.
    CDDTypedAnnotDialog(wxWindow *parent, const ncbi::objects::CAlign_annot& initial, const std::string& title);

private:
    bool changed;
    bool isTypeDataRead;

    //  Each wxArrayString in TTypeNamesPair is ordered as it is to appear in pulldown widgets.
    //  The key of TPredefinedSites corresponds to the 'type' field in the Align_annot ASN.1 spec
    //  and is also used to define the position of the type in the wxChoice widget.
    typedef std::pair<wxString, wxArrayString> TTypeNamesPair;
    typedef std::map<int, TTypeNamesPair> TPredefinedSites;
    static TPredefinedSites predefinedSites;

    //  Populate the 'predefinedSites' map from the cd_utils::CStdAnnotTypes class.
    void PopulatePredefinedSitesMap();

    //  Reconfigure the combobox with the predefined entries for type 'newType'.
    void AdjustComboboxForType(int type);

    // event callbacks
    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnChange(wxCommandEvent& event);
    void OnTypeChoice(wxCommandEvent& event);

    // utility functions
    void SetupGUIControls(void);

    DECLARE_EVENT_TABLE()

public:
    bool HasDataChanged(void) const { return changed; }

    //  Returns true if data from the dialog was placed in 'alignAnnot'.
    bool GetData(ncbi::objects::CAlign_annot* alignAnnot);
};

END_SCOPE(Cn3D)

#endif // CN3D_CDD_ANNOT_DIALOG__HPP
