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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2001/08/27 00:06:23  thiessen
* add structure evidence to CDD annotation
*
* Revision 1.3  2001/08/06 20:22:00  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.2  2001/07/19 19:14:38  thiessen
* working CDD alignment annotator ; misc tweaks
*
* Revision 1.1  2001/07/12 17:35:15  thiessen
* change domain mapping ; add preliminary cdd annotation GUI
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include <objects/cdd/Align_annot.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/mmdb1/Biostruc_annot_set.hpp>
#include <objects/mmdb1/Biostruc_descr.hpp>
#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/mmdb1/Mmdb_id.hpp>
#include <objects/mmdb3/Biostruc_feature_set.hpp>
#include <objects/mmdb3/Biostruc_feature_set_descr.hpp>
#include <objects/mmdb3/Biostruc_feature.hpp>
#include <objects/mmdb3/Chem_graph_pntrs.hpp>
#include <objects/mmdb3/Residue_pntrs.hpp>
#include <objects/mmdb3/Residue_interval_pntr.hpp>

#include "cn3d/cdd_annot_dialog.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/chemical_graph.hpp"


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ header from cdd_annot_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

#include <wx/image.h>
#include <wx/statline.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <wx/notebook.h>

// Declare window functions

#define ID_L_ANNOT 10000
#define ID_B_NEW_ANNOT 10001
#define ID_B_DEL_ANNOT 10002
#define ID_B_EDIT_ANNOT 10003
#define ID_B_HIGHLIGHT 10004
#define ID_L_EVID 10005
#define ID_B_NEW_EVID 10006
#define ID_B_DEL_EVID 10007
#define ID_B_EDIT_EVID 10008
#define ID_B_SHOW 10009
#define ID_B_DONE 10010
#define ID_B_CANCEL 10011
wxSizer *SetupCDDAnnotDialog( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_R_COMMENT 10012
#define ID_ST_COMMENT 10013
#define ID_T_COMMENT 10014
#define ID_LINE 10015
#define ID_R_PMID 10016
#define ID_ST_PMID 10017
#define ID_T_PMID 10018
#define ID_R_STRUCTURE 10019
#define ID_ST_STRUCTURE 10020
#define ID_T_STRUCTURE 10021
#define ID_B_EDIT_OK 10022
#define ID_B_EDIT_CANCEL 10023
wxSizer *SetupEvidenceDialog( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

// a "structure evidence" biostruc-annot-set has these qualities - basically, a specific
// comment tag as the first entry in the set's description, and a 'name' descr in the feature set
static const std::string STRUCTURE_EVIDENCE_COMMENT = "Used as Structure Evidence for CDD Annotation";
#define IS_STRUCTURE_EVIDENCE_BSANNOT(evidence) \
    ((evidence).IsBsannot() && \
     (evidence).GetBsannot().IsSetDescr() && \
     (evidence).GetBsannot().GetDescr().size() > 0 && \
     (evidence).GetBsannot().GetDescr().front()->IsOther_comment() && \
     (evidence).GetBsannot().GetDescr().front()->GetOther_comment() == STRUCTURE_EVIDENCE_COMMENT && \
     (evidence).GetBsannot().GetFeatures().size() > 0 && \
     (evidence).GetBsannot().GetFeatures().front()->IsSetDescr() && \
     (evidence).GetBsannot().GetFeatures().front()->GetDescr().size() > 0 && \
     (evidence).GetBsannot().GetFeatures().front()->GetDescr().front()->IsName())


#define DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERR_POST(Error << "Can't find window with id " << id); \
        return; \
    }

#define DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERR_POST(Error << "Can't find window with id " << id); \
        return false; \
    }

BEGIN_EVENT_TABLE(CDDAnnotateDialog, wxDialog)
    EVT_CLOSE       (       CDDAnnotateDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    CDDAnnotateDialog::OnButton)
    EVT_LISTBOX     (-1,    CDDAnnotateDialog::OnSelection)
END_EVENT_TABLE()

CDDAnnotateDialog::CDDAnnotateDialog(wxWindow *parent, StructureSet *set) :
    wxDialog(parent, -1, "CDD Annotations", wxPoint(400, 100), wxDefaultSize,
        wxCAPTION | wxSYSTEM_MENU), // not resizable
    structureSet(set), changed(false)
{
    // copy the existing annot set, or make a new one
    annotSet.Reset(set->GetCopyOfCDDAnnotSet());
    if (annotSet.IsNull())
        annotSet.Reset(new CAlign_annot_set());

    // fill out alignment information
    alignment = structureSet->alignmentManager->GetCurrentMultipleAlignment();
    master = alignment->GetMaster();

    // find intervals of aligned residues of the master sequence that are currently highlighted
    int first = 0, last = 0;
    while (first < master->Length()) {
        // find first highlighted residue
        while (first < master->Length() &&
               !(GlobalMessenger()->IsHighlighted(master, first) &&
                 alignment->IsAligned(0, first))) first++;
        if (first >= master->Length()) break;
        // find last in contiguous stretch of highlighted residues
        last = first;
        while (last + 1 < master->Length() &&
               GlobalMessenger()->IsHighlighted(master, last + 1) &&
               alignment->IsAligned(0, last + 1)) last++;
        // create Seq-interval
        CRef < CSeq_interval > interval(new CSeq_interval());
        interval->SetFrom(first);
        interval->SetTo(last);
        master->FillOutSeqId(&(interval->SetId()));
        intervals.push_back(interval);
        first = last + 2;
    }

    // construct the panel
    wxSizer *topSizer = SetupCDDAnnotDialog(this, false);

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->SetSizeHints(this);

    // set initial GUI state
    SetupGUIControls(0, 0);
}

// same as hitting done button
void CDDAnnotateDialog::OnCloseWindow(wxCloseEvent& event)
{
    // set data in structureSet if changed
    if (changed)
        structureSet->SetCDDAnnotSet((annotSet->Get().size() > 0) ? annotSet.GetPointer() : NULL);

    // close dialog
    EndModal(wxOK);
}

void CDDAnnotateDialog::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case ID_B_DONE: {
            wxCloseEvent fake;
            OnCloseWindow(fake);   // handle on-exit stuff there
            break;
        }
        case ID_B_CANCEL:
            EndModal(wxCANCEL);
            break;

        // annotation buttons
        case ID_B_NEW_ANNOT:
            NewAnnotation();
            break;
        case ID_B_DEL_ANNOT:
            DeleteAnnotation();
            break;
        case ID_B_EDIT_ANNOT:
            EditAnnotation();
            break;
        case ID_B_HIGHLIGHT:
            HighlightAnnotation();
            break;

        // evidence buttons
        case ID_B_NEW_EVID:
            NewEvidence();
            break;
        case ID_B_DEL_EVID:
            DeleteEvidence();
            break;
        case ID_B_EDIT_EVID:
            EditEvidence();
            break;
        case ID_B_SHOW:
            ShowEvidence();
            break;

        default:
            event.Skip();
    }
}

void CDDAnnotateDialog::OnSelection(wxCommandEvent& event)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)

    if (event.GetEventObject() == annots)
        SetupGUIControls(annots->GetSelection(), 0);
    else
        SetupGUIControls(annots->GetSelection(), evids->GetSelection());
}

void CDDAnnotateDialog::SetupGUIControls(int selectAnnot, int selectEvidence)
{
    // get GUI control pointers
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bNewAnnot, ID_B_NEW_ANNOT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bDelAnnot, ID_B_DEL_ANNOT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bEditAnnot, ID_B_EDIT_ANNOT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bHighlight, ID_B_HIGHLIGHT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bNewEvid, ID_B_NEW_EVID, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bDelEvid, ID_B_DEL_EVID, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bEditEvid, ID_B_EDIT_EVID, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bShow, ID_B_SHOW, wxButton)

    // fill out annots listbox
    annots->Clear();
    CAlign_annot *selectedAnnot = NULL;
    CAlign_annot_set::Tdata::const_iterator a, ae = annotSet->Get().end();
    for (a=annotSet->Get().begin(); a!=ae; a++) {
        if ((*a)->IsSetDescription())
            annots->Append((*a)->GetDescription().c_str(), a->GetPointer());
        else
            annots->Append("(no description)", a->GetPointer());
    }
    if (selectAnnot < annots->GetCount())
        annots->SetSelection(selectAnnot);
    else if (annots->GetCount() > 0)
        annots->SetSelection(0);
    if (annots->GetCount() > 0)
        selectedAnnot = reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));

    // fill out evidence listbox
    evids->Clear();
    CFeature_evidence *selectedEvid = NULL;
    if (selectedAnnot && selectedAnnot->IsSetEvidence()) {
        CAlign_annot::TEvidence::const_iterator e, ee = selectedAnnot->GetEvidence().end();
        for (e=selectedAnnot->GetEvidence().begin(); e!=ee; e++) {
            // get evidence "title" depending on type
            wxString evidTitle;
            if ((*e)->IsComment())
                evidTitle.Printf("Comment: %s", (*e)->GetComment().c_str());
            else if ((*e)->IsReference() && (*e)->GetReference().IsPmid())
                evidTitle.Printf("PMID: %i", (*e)->GetReference().GetPmid().Get());
            else if (IS_STRUCTURE_EVIDENCE_BSANNOT(**e))
                evidTitle.Printf("Structure: %s",
                    (*e)->GetBsannot().GetFeatures().front()->GetDescr().front()->GetName().c_str());
            else
                evidTitle = "(unknown type)";
            evids->Append(evidTitle, e->GetPointer());
        }
        if (selectEvidence < evids->GetCount())
            evids->SetSelection(selectEvidence);
        else if (evids->GetCount() > 0)
            evids->SetSelection(0);
        if (evids->GetCount() > 0)
            selectedEvid = reinterpret_cast<CFeature_evidence*>(evids->GetClientData(evids->GetSelection()));
    }

    // set button states
    bNewAnnot->Enable(intervals.size() > 0);
    bDelAnnot->Enable(selectedAnnot != NULL);
    bEditAnnot->Enable(selectedAnnot != NULL);
    bHighlight->Enable(selectedAnnot != NULL);
    bNewEvid->Enable(selectedAnnot != NULL);
    bDelEvid->Enable(selectedEvid != NULL);
    bEditEvid->Enable(selectedEvid != NULL);
    bShow->Enable(selectedEvid != NULL &&
        ((selectedEvid->IsReference() && selectedEvid->GetReference().IsPmid()) ||
         IS_STRUCTURE_EVIDENCE_BSANNOT(*selectedEvid)));
}

void CDDAnnotateDialog::NewAnnotation(void)
{
    if (intervals.size() == 0) {
        ERR_POST(Warning << "CDDAnnotateDialog::NewAnnotation() - no aligned+highlighted residues!");
        return;
    }

    // get description from user
    wxString descr = wxGetTextFromUser(
        "Enter a description for the new annotation:", "Description");
    if (descr.size() == 0) return;

    // create a new annotation
    CRef < CAlign_annot > annot(new CAlign_annot());
    annot->SetDescription(descr.c_str());

    // fill out location
    if (intervals.size() == 1) {
        annot->SetLocation().SetInt(intervals.front());
    } else {
        CRef < CPacked_seqint > packed(new CPacked_seqint());
        packed->Set() = intervals;  // copy list
        annot->SetLocation().SetPacked_int(packed);
    }

    // add to annotation list
    annotSet->Set().push_back(annot);
    changed = true;

    // update GUI
    SetupGUIControls(annotSet->Get().size() - 1, 0);
}

void CDDAnnotateDialog::DeleteAnnotation(void)
{
    // get selection
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;
    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERR_POST(Error << "CDDAnnotateDialog::DeleteAnnotation() - error getting annotation pointer");
        return;
    }

    // confirm with user
    int confirm = wxMessageBox("This will remove the selected annotation and all the\n"
        "evidence associated with it. Is this correct?", "Confirm", wxOK | wxCANCEL | wxCENTRE, this);
    if (confirm != wxOK) return;

    // actually delete the annotation
    CAlign_annot_set::Tdata::iterator a, ae = annotSet->Set().end();
    for (a=annotSet->Set().begin(); a!=ae; a++) {
        if (*a == selectedAnnot) {
            annotSet->Set().erase(a);
            changed = true;
            break;
        }
    }

    // update GUI
    SetupGUIControls(0, 0);
}

void CDDAnnotateDialog::EditAnnotation(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)

    // get selection
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;
    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERR_POST(Error << "CDDAnnotateDialog::EditAnnotation() - error getting annotation pointer");
        return;
    }

    if (intervals.size() > 0) {
        // move the annotation?
        int move = wxMessageBox("Do you want to move the annotation to the currently\n"
            "highlighted and aligned residues?", "Move?", wxYES_NO | wxCANCEL | wxCENTRE, this);
        if (move == wxCANCEL)
            return;
        else if (move == wxYES) {
            // change location
            if (intervals.size() == 1) {
                selectedAnnot->SetLocation().SetInt(intervals.front());
            } else {
                CRef < CPacked_seqint > packed(new CPacked_seqint());
                packed->Set() = intervals;  // copy list
                selectedAnnot->SetLocation().SetPacked_int(packed);
            }
            changed = true;
        }
    }

    // edit description
    wxString initial;
    if (selectedAnnot->IsSetDescription()) initial = selectedAnnot->GetDescription().c_str();
    wxString descr = wxGetTextFromUser(
        "Enter a description for the new annotation:", "Description", initial);
    if (descr.size() > 0 && descr != selectedAnnot->GetDescription().c_str()) {
        selectedAnnot->SetDescription(descr.c_str());
        changed = true;
    }

    // update GUI
    SetupGUIControls(annots->GetSelection(), evids->GetSelection());
}

bool CDDAnnotateDialog::HighlightInterval(const ncbi::objects::CSeq_interval& interval)
{
    // make sure annotation sequence matches master sequence
    if (!IsAMatch(master, interval.GetId())) {
        ERR_POST(Error << "CDDAnnotateDialog::HighlightInterval() - interval Seq-id/master sequence mismatch");
        return false;
    }

    // do the highlighting
    bool okay = alignment->HighlightAlignedColumnsOfMasterRange(interval.GetFrom(), interval.GetTo());
    if (!okay)
        ERR_POST(Error << "CDDAnnotateDialog::HighlightInterval() - problem highlighting from "
            << interval.GetFrom() << " to " << interval.GetTo());
    return okay;
}

void CDDAnnotateDialog::HighlightAnnotation(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bNewAnnot, ID_B_NEW_ANNOT, wxButton)

    // get selection
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;
    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERR_POST(Error << "CDDAnnotateDialog::HighlightAnnotation() - error getting annotation pointer");
        return;
    }

    // highlight annotation's intervals, and reset class 'intervals' list to new highlights
    GlobalMessenger()->RemoveAllHighlights(false);
    intervals.clear();
    if (selectedAnnot->GetLocation().IsInt()) {
        if (HighlightInterval(selectedAnnot->GetLocation().GetInt())) {
            CRef < CSeq_interval > intRef(&(selectedAnnot->SetLocation().SetInt()));
            intervals.push_back(intRef);
        }
    } else if (selectedAnnot->GetLocation().IsPacked_int()) {
        CPacked_seqint::Tdata::iterator s,
            se = selectedAnnot->SetLocation().SetPacked_int().Set().end();
        for (s=selectedAnnot->SetLocation().SetPacked_int().Set().begin(); s!=se; s++) {
            if (!HighlightInterval(**s)) break;
            CRef < CSeq_interval > intRef(&(**s));
            intervals.push_back(intRef);
        }
    }

    // enable 'new' button if necessary
    bNewAnnot->Enable(intervals.size() > 0);
}

void CDDAnnotateDialog::NewEvidence(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)

    // get selected annotation
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;
    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERR_POST(Error << "CDDAnnotateDialog::NewEvidence() - error getting annotation pointer");
        return;
    }

    // create default comment evidence
    CRef < CFeature_evidence > newEvidence(new CFeature_evidence());
    newEvidence->SetComment("");

    // bring up evidence editor
    CDDEvidenceDialog dialog(this, *newEvidence);
    int result = dialog.ShowModal();

    // add new evidence
    if (result == wxOK) {
        dialog.GetData(newEvidence.GetPointer());
        selectedAnnot->SetEvidence().push_back(newEvidence);
        SetupGUIControls(annots->GetSelection(), selectedAnnot->GetEvidence().size() - 1);
        changed = true;
    }
}

void CDDAnnotateDialog::DeleteEvidence(void)
{
    // get selected annotation
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;
    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERR_POST(Error << "CDDAnnotateDialog::DeleteEvidence() - error getting annotation pointer");
        return;
    }

    // get selected evidence
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)
    if (evids->GetCount() == 0 || evids->GetSelection() < 0) return;
    CFeature_evidence *selectedEvidence =
        reinterpret_cast<CFeature_evidence*>(evids->GetClientData(evids->GetSelection()));
    if (!selectedEvidence) {
        ERR_POST(Error << "CDDAnnotateDialog::DeleteEvidence() - error getting evidence pointer");
        return;
    }

    // confirm with user
    int confirm = wxMessageBox("This will remove the selected evidence from\n"
        "the selected annotation. Is this correct?", "Confirm", wxOK | wxCANCEL | wxCENTRE, this);
    if (confirm != wxOK) return;

    // delete evidence from annotation's list
    CAlign_annot::TEvidence::iterator e, ee = selectedAnnot->SetEvidence().end();
    for (e=selectedAnnot->SetEvidence().begin(); e!=ee; e++) {
        if (*e == selectedEvidence) {
            selectedAnnot->SetEvidence().erase(e);
            changed = true;
            break;
        }
    }
    if (e == ee) {
        ERR_POST(Error << "CDDAnnotateDialog::DeleteEvidence() - evidence pointer not found in annotation");
        return;
    }

    // update GUI
    SetupGUIControls(annots->GetSelection(), 0);
}

void CDDAnnotateDialog::EditEvidence(void)
{
    // get selected evidence
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)
    if (evids->GetCount() == 0 || evids->GetSelection() < 0) return;
    CFeature_evidence *selectedEvidence =
        reinterpret_cast<CFeature_evidence*>(evids->GetClientData(evids->GetSelection()));
    if (!selectedEvidence) {
        ERR_POST(Error << "CDDAnnotateDialog::DeleteEvidence() - error getting evidence pointer");
        return;
    }

    // bring up evidence editor
    CDDEvidenceDialog dialog(this, *selectedEvidence);
    int result = dialog.ShowModal();

    // update evidence
    if (result == wxOK && dialog.HasDataChanged()) {
        dialog.GetData(selectedEvidence);
        changed = true;
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
        SetupGUIControls(annots->GetSelection(), evids->GetSelection());
    }
}

static void HighlightResidues(const StructureSet *set, const CBiostruc_annot_set& annot)
{
    try {
        if (!annot.IsSetId() || annot.GetId().size() == 0 || !annot.GetId().front()->IsMmdb_id())
            throw "no MMDB ID found in annotation";
        int mmdbID = annot.GetId().front()->GetMmdb_id().Get();

        // find the first object with the annotation's mmdbID
        StructureSet::ObjectList::const_iterator o, oe = set->objects.end();
        for (o=set->objects.begin(); o!=oe; o++) {
            if ((*o)->mmdbID == mmdbID) {

                // iterate over molecule/residue intervals
                if (annot.GetFeatures().size() > 0 &&
                    annot.GetFeatures().front()->GetFeatures().size() > 0 &&
                    annot.GetFeatures().front()->GetFeatures().front()->IsSetLocation() &&
                    annot.GetFeatures().front()->GetFeatures().front()->GetLocation().IsSubgraph() &&
                    annot.GetFeatures().front()->GetFeatures().front()->GetLocation().
                        GetSubgraph().IsResidues() &&
                    annot.GetFeatures().front()->GetFeatures().front()->GetLocation().
                        GetSubgraph().GetResidues().IsInterval() &&
                    annot.GetFeatures().front()->GetFeatures().front()->GetLocation().
                        GetSubgraph().GetResidues().GetInterval().size() > 0)
                {
                    GlobalMessenger()->RemoveAllHighlights(true);

                    CResidue_pntrs::TInterval::const_iterator i, ie =
                        annot.GetFeatures().front()->GetFeatures().front()->GetLocation().
                            GetSubgraph().GetResidues().GetInterval().end();
                    for (i=annot.GetFeatures().front()->GetFeatures().front()->GetLocation().
                            GetSubgraph().GetResidues().GetInterval().begin(); i!=ie; i++) {

                        // find molecule with moleculeID
                        ChemicalGraph::MoleculeMap::const_iterator m, me = (*o)->graph->molecules.end();
                        for (m=(*o)->graph->molecules.begin(); m!=me; m++) {
                            if (m->second->id == (*i)->GetMolecule_id().Get()) {

                                // highlight residues in interval
                                for (int r=(*i)->GetFrom().Get(); r<=(*i)->GetTo().Get(); r++)
                                    GlobalMessenger()->ToggleHighlight(m->second, r);
                                break;
                            }
                        }
                        if (m == (*o)->graph->molecules.end())
                            throw "molecule with given ID not found";
                    }
                    return; // finished!
                }
                throw "unrecognized annotation structure";
            }
        }
        throw "no object of annotation's MMDB ID";

    } catch (const char *err) {
        ERR_POST(Error << "HighlightResidues() - " << err);
    }
    return;
}

void CDDAnnotateDialog::ShowEvidence(void)
{
    // get selected evidence
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)
    if (evids->GetCount() == 0 || evids->GetSelection() < 0) return;
    CFeature_evidence *selectedEvidence =
        reinterpret_cast<CFeature_evidence*>(evids->GetClientData(evids->GetSelection()));
    if (!selectedEvidence) {
        ERR_POST(Error << "CDDAnnotateDialog::ShowEvidence() - error getting evidence pointer");
        return;
    }

    // launch URL given PMID
    if (selectedEvidence->IsReference() && selectedEvidence->GetReference().IsPmid()) {
        wxString url;
        url.Printf("http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?"
            "cmd=Retrieve&db=PubMed&dopt=Abstract&list_uids=%i",
            selectedEvidence->GetReference().GetPmid().Get());
        LaunchWebPage(url);
    }

    // highlight residues if structure evidence
    else if (IS_STRUCTURE_EVIDENCE_BSANNOT(*selectedEvidence)) {
        HighlightResidues(structureSet, selectedEvidence->GetBsannot());
    }

    else {
        ERR_POST(Error << "CDDAnnotateDialog::ShowEvidence() - can't show that evidence type");
    }
}


///// CDDEvidenceDialog stuff /////

BEGIN_EVENT_TABLE(CDDEvidenceDialog, wxDialog)
    EVT_CLOSE       (       CDDEvidenceDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    CDDEvidenceDialog::OnButton)
    EVT_RADIOBUTTON (-1,    CDDEvidenceDialog::OnChange)
    EVT_TEXT        (-1,    CDDEvidenceDialog::OnChange)
END_EVENT_TABLE()

CDDEvidenceDialog::CDDEvidenceDialog(wxWindow *parent, const ncbi::objects::CFeature_evidence& initial) :
    wxDialog(parent, -1, "CDD Annotations", wxPoint(400, 100), wxDefaultSize,
        wxCAPTION | wxSYSTEM_MENU), // not resizable
    changed(false)
{
    // construct the panel
    wxSizer *topSizer = SetupEvidenceDialog(this, false);

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->SetSizeHints(this);

    // set initial states
    if (initial.IsComment()) {
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(rComment, ID_R_COMMENT, wxRadioButton)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tComment, ID_T_COMMENT, wxTextCtrl)
        rComment->SetValue(true);
        tComment->SetValue(initial.GetComment().c_str());
        tComment->SetFocus();
    } else if (initial.IsReference() && initial.GetReference().IsPmid()) {
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(rPMID, ID_R_PMID, wxRadioButton)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tPMID, ID_T_PMID, wxTextCtrl)
        rPMID->SetValue(true);
        wxString pmid;
        pmid.Printf("%i", initial.GetReference().GetPmid().Get());
        tPMID->SetValue(pmid);
        tPMID->SetFocus();
    } else if (IS_STRUCTURE_EVIDENCE_BSANNOT(initial)) {
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(rSTRUCTURE, ID_R_STRUCTURE, wxRadioButton)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tSTRUCTURE, ID_T_STRUCTURE, wxTextCtrl)
        rSTRUCTURE->SetValue(true);
        tSTRUCTURE->SetValue(
            initial.GetBsannot().GetFeatures().front()->GetDescr().front()->GetName().c_str());
        // automatically pick up highlights if structure evidence selected
        changed = true;
    } else {
        ERR_POST(Error << "CDDEvidenceDialog::CDDEvidenceDialog() - "
            "don't (yet) know how to edit this evidence type");
    }
    SetupGUIControls();
}

// same as hitting cancel button
void CDDEvidenceDialog::OnCloseWindow(wxCloseEvent& event)
{
    EndModal(wxCANCEL);
}

void CDDEvidenceDialog::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case ID_B_EDIT_OK: {
            CFeature_evidence test;
            if (GetData(&test)) EndModal(wxOK);
            break;
        }
        case ID_B_EDIT_CANCEL:
            EndModal(wxCANCEL);
            break;
        default:
            event.Skip();
    }
}

void CDDEvidenceDialog::OnChange(wxCommandEvent& event)
{
    changed = true;
    SetupGUIControls();
}

void CDDEvidenceDialog::SetupGUIControls(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(rComment, ID_R_COMMENT, wxRadioButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(stComment, ID_ST_COMMENT, wxStaticText)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tComment, ID_T_COMMENT, wxTextCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(rPMID, ID_R_PMID, wxRadioButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(stPMID, ID_ST_PMID, wxStaticText)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tPMID, ID_T_PMID, wxTextCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(rSTRUCTURE, ID_R_STRUCTURE, wxRadioButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(stSTRUCTURE, ID_ST_STRUCTURE, wxStaticText)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tSTRUCTURE, ID_T_STRUCTURE, wxTextCtrl)

    stComment->Enable(rComment->GetValue());
    tComment->Enable(rComment->GetValue());
    stPMID->Enable(rPMID->GetValue());
    tPMID->Enable(rPMID->GetValue());
    stSTRUCTURE->Enable(rSTRUCTURE->GetValue());
    tSTRUCTURE->Enable(rSTRUCTURE->GetValue());
}

bool CDDEvidenceDialog::GetData(ncbi::objects::CFeature_evidence *evidence)
{
    DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(rComment, ID_R_COMMENT, wxRadioButton)
    DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(rPMID, ID_R_PMID, wxRadioButton)
    DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(rSTRUCTURE, ID_R_STRUCTURE, wxRadioButton)

    if (rComment->GetValue()) {
        DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(tComment, ID_T_COMMENT, wxTextCtrl)
        if (tComment->GetValue().size() > 0) {
            evidence->SetComment(tComment->GetValue().c_str());
            return true;
        } else {
            ERR_POST(Error << "CDDEvidenceDialog::GetData() - comment must not be zero-length");
            return false;
        }
    }

    else if (rPMID->GetValue()) {
        DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(tPMID, ID_T_PMID, wxTextCtrl)
        unsigned long pmid;
        if (tPMID->GetValue().ToULong(&pmid)) {
            evidence->SetReference().SetPmid().Set(pmid);
            return true;
        } else {
            ERR_POST(Error << "CDDEvidenceDialog::GetData() - PMID must be a positive integer");
            return false;
        }
    }

    else if (rSTRUCTURE->GetValue()) {
        DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(tSTRUCTURE, ID_T_STRUCTURE, wxTextCtrl)
        CRef < CBiostruc_annot_set > ref(GlobalMessenger()->
            CreateBiostrucAnnotSetForHighlightsOnSingleObject());
        if (ref.NotEmpty()) {
            evidence->SetBsannot(ref);
            CRef < CBiostruc_descr > descr(new CBiostruc_descr());
            descr->SetOther_comment(STRUCTURE_EVIDENCE_COMMENT);
            evidence->SetBsannot().SetDescr().push_front(descr);
            CRef < CBiostruc_feature_set_descr > name(new CBiostruc_feature_set_descr());
            name->SetName(tSTRUCTURE->GetValue().c_str());
            evidence->SetBsannot().SetFeatures().front()->SetDescr().push_front(name);
            return true;
        } else {
            ERR_POST(Error << "CDDEvidenceDialog::GetData() - error setting up structure evidence data");
            return false;
        }
    }

    ERR_POST(Error << "CDDEvidenceDialog::GetData() - unknown evidence type");
    return false;
}

END_SCOPE(Cn3D)


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ code from cdd_annot_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *SetupCDDAnnotDialog( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxFlexGridSizer *item1 = new wxFlexGridSizer( 1, 0, 0, 0 );

    wxStaticBox *item3 = new wxStaticBox( parent, -1, "Annotations" );
    wxStaticBoxSizer *item2 = new wxStaticBoxSizer( item3, wxVERTICAL );

    wxString *strs4 = (wxString*) NULL;
    wxListBox *item4 = new wxListBox( parent, ID_L_ANNOT, wxDefaultPosition, wxSize(-1,100), 0, strs4, wxLB_SINGLE );
    item2->Add( item4, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxGridSizer *item5 = new wxGridSizer( 2, 0, 0 );

    wxButton *item6 = new wxButton( parent, ID_B_NEW_ANNOT, "New", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item7 = new wxButton( parent, ID_B_DEL_ANNOT, "Delete", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item7, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item8 = new wxButton( parent, ID_B_EDIT_ANNOT, "Edit", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item8, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item9 = new wxButton( parent, ID_B_HIGHLIGHT, "Highlight", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item9, 0, wxALIGN_CENTRE|wxALL, 5 );

    item2->Add( item5, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item2, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticBox *item11 = new wxStaticBox( parent, -1, "Evidence" );
    wxStaticBoxSizer *item10 = new wxStaticBoxSizer( item11, wxVERTICAL );

    wxString *strs12 = (wxString*) NULL;
    wxListBox *item12 = new wxListBox( parent, ID_L_EVID, wxDefaultPosition, wxSize(-1,100), 0, strs12, wxLB_SINGLE );
    item10->Add( item12, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxGridSizer *item13 = new wxGridSizer( 2, 0, 0 );

    wxButton *item14 = new wxButton( parent, ID_B_NEW_EVID, "New", wxDefaultPosition, wxDefaultSize, 0 );
    item13->Add( item14, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item15 = new wxButton( parent, ID_B_DEL_EVID, "Delete", wxDefaultPosition, wxDefaultSize, 0 );
    item13->Add( item15, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item16 = new wxButton( parent, ID_B_EDIT_EVID, "Edit", wxDefaultPosition, wxDefaultSize, 0 );
    item13->Add( item16, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item17 = new wxButton( parent, ID_B_SHOW, "Show", wxDefaultPosition, wxDefaultSize, 0 );
    item13->Add( item17, 0, wxALIGN_CENTRE|wxALL, 5 );

    item10->Add( item13, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item10, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item1, 0, wxALIGN_CENTRE, 5 );

    wxBoxSizer *item18 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item19 = new wxButton( parent, ID_B_DONE, "Done", wxDefaultPosition, wxDefaultSize, 0 );
    item19->SetDefault();
    item18->Add( item19, 0, wxALIGN_CENTRE|wxALL, 5 );

    item18->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item20 = new wxButton( parent, ID_B_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item18->Add( item20, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item18, 0, wxALIGN_CENTRE|wxALL, 5 );

    if (set_sizer)
    {
        parent->SetAutoLayout( TRUE );
        parent->SetSizer( item0 );
        if (call_fit)
        {
            item0->Fit( parent );
            item0->SetSizeHints( parent );
        }
    }

    return item0;
}

wxSizer *SetupEvidenceDialog( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Evidence Options" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item3->AddGrowableCol( 2 );

    wxRadioButton *item4 = new wxRadioButton( parent, ID_R_COMMENT, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item4, 0, wxALIGN_BOTTOM|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    wxStaticText *item5 = new wxStaticText( parent, ID_ST_COMMENT, "Comment:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item5, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxTextCtrl *item6 = new wxTextCtrl( parent, ID_T_COMMENT, "", wxDefaultPosition, wxSize(150,-1), 0 );
    item3->Add( item6, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item3, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    wxStaticLine *item7 = new wxStaticLine( parent, ID_LINE, wxDefaultPosition, wxSize(20,-1), wxLI_HORIZONTAL );
    item1->Add( item7, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item8 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item8->AddGrowableCol( 2 );

    wxRadioButton *item9 = new wxRadioButton( parent, ID_R_PMID, "", wxDefaultPosition, wxDefaultSize, 0 );
    item8->Add( item9, 0, wxALIGN_BOTTOM|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    wxStaticText *item10 = new wxStaticText( parent, ID_ST_PMID, "Reference (PubMed ID):", wxDefaultPosition, wxDefaultSize, 0 );
    item8->Add( item10, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxTextCtrl *item11 = new wxTextCtrl( parent, ID_T_PMID, "", wxDefaultPosition, wxDefaultSize, 0 );
    item8->Add( item11, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item8, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    wxStaticLine *item12 = new wxStaticLine( parent, ID_LINE, wxDefaultPosition, wxSize(20,-1), wxLI_HORIZONTAL );
    item1->Add( item12, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item13 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item13->AddGrowableCol( 2 );

    wxRadioButton *item14 = new wxRadioButton( parent, ID_R_STRUCTURE, "", wxDefaultPosition, wxDefaultSize, 0 );
    item13->Add( item14, 0, wxALIGN_BOTTOM|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    wxStaticText *item15 = new wxStaticText( parent, ID_ST_STRUCTURE, "Structure:", wxDefaultPosition, wxDefaultSize, 0 );
    item13->Add( item15, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxTextCtrl *item16 = new wxTextCtrl( parent, ID_T_STRUCTURE, "", wxDefaultPosition, wxSize(80,-1), 0 );
    item13->Add( item16, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item13, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item17 = new wxFlexGridSizer( 1, 0, 0, 0 );

    wxButton *item18 = new wxButton( parent, ID_B_EDIT_OK, "OK", wxDefaultPosition, wxDefaultSize, 0 );
    item18->SetDefault();
    item17->Add( item18, 0, wxALIGN_CENTRE|wxALL, 5 );

    item17->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item19 = new wxButton( parent, ID_B_EDIT_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item17->Add( item19, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item17, 0, wxALIGN_CENTRE|wxALL, 5 );

    if (set_sizer)
    {
        parent->SetAutoLayout( TRUE );
        parent->SetSizer( item0 );
        if (call_fit)
        {
            item0->Fit( parent );
            item0->SetSizeHints( parent );
        }
    }

    return item0;
}

