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
* Authors:  Chris Lanczycki
*
* File Description:
*      dialogs for annotating CDD's with interactions from IBIS
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <algorithm>

#include <corelib/ncbistd.hpp>

#include <objects/cdd/Align_annot.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include "remove_header_conflicts.hpp"

#include "cdd_ibis_annot_dialog.hpp"
#include "structure_set.hpp"
#include "messenger.hpp"
#include "alignment_manager.hpp"
#include "block_multiple_alignment.hpp"
#include "sequence_set.hpp"
#include "cn3d_tools.hpp"
//#include "chemical_graph.hpp"
//#include "opengl_renderer.hpp"
//#include "show_hide_manager.hpp"
#include "asn_reader.hpp"


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ header from cdd_ibis_annot_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

#include <wx/artprov.h>
#include <wx/image.h>
#include <wx/imaglist.h>
#include <wx/statline.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <wx/notebook.h>
#include <wx/grid.h>
#include <wx/toolbar.h>
#include <wx/tglbtn.h>

// Declare window functions

extern wxSizer *ibisIntStaticBoxSizer;
#define ID_TEXT_INT 10000
#define ID_C_TYPE 10001
#define ID_LC_INT 10002
#define ID_B_ADD_INT 10003
#define ID_B_LAUNCH_INT 10004
#define ID_B_HIGHLIGHT_INT 10005
#define ID_LB_ANNOT 10006
#define ID_B_DELETE_ANNOT 10007
#define ID_B_HIGHLIGHT_ANNOT 10008
#define ID_B_HIGHLIGHT_OLAP 10009
#define ID_B_HIGHLIGHT_NONOLAP_ANNOT 10010
#define ID_B_HIGHLIGHT_NONOLAP_INTN 10011
wxSizer *SetupIbisAnnotationDialog( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

static const string IBIS_EVIDENCE_COMMENT = "Interaction site inferred from IBIS";


//  IBISInteraction members and methods

const Sequence* IBISInteraction::m_querySequence = NULL;
const MoleculeIdentifier* IBISInteraction::m_queryMolecule = NULL;
const int IBISInteraction::NOT_ASSIGNED = -1;

void IBISInteraction::SetQuerySequence(const Sequence* querySequence) { 
    m_queryMolecule = NULL;
    m_querySequence = querySequence;
    if (m_querySequence) m_queryMolecule = m_querySequence->identifier;
}

bool IBISInteraction::QueryMoleculeHasChain()
{
    bool result = false;
    if (m_queryMolecule) {
        result = (m_queryMolecule->pdbChain != MoleculeIdentifier::VALUE_NOT_SET && m_queryMolecule->pdbChain != ' ');
    }
    return result;
}

bool IBISInteraction::IsIbisIntType(int integer, IBISInteraction::eIbisInteractionType* ibisType)
{
    bool result = true;
    eIbisInteractionType type = eIbisOther;

    switch (integer) {
        case eIbisNoTypeAssigned:
            type = eIbisNoTypeAssigned;
            break;
        case eIbisProteinDNA:
            type = eIbisProteinDNA;
            break;
        case eIbisProteinRNA:
            type = eIbisProteinRNA;
            break;
        case eIbisProteinCombo_DNA_RNA:
            type = eIbisProteinCombo_DNA_RNA;
            break;
        case eIbisProteinProtein:
            type = eIbisProteinProtein;
            break;
        case eIbisProteinPeptide:
            type = eIbisProteinPeptide;
            break;
        case eIbisProteinIon:
            type = eIbisProteinIon;
            break;
        case eIbisProteinChemical:
            type = eIbisProteinChemical;
            break;
        default:
            INFOMSG("type " << integer << " is not a valid Ibis interaction type");
            result = false;
            break;
    };

    if (result && ibisType) {
        *ibisType = type;
    }

    return result;
}

string IBISInteraction::IbisIntTypeToString(IBISInteraction::eIbisInteractionType ibisType) 
{
    string result = "Unknown";
    switch (ibisType) {
        case eIbisNoTypeAssigned:
            result = "Unassigned";
            break;
        case eIbisProteinDNA:
        case eIbisProteinRNA:
        case eIbisProteinCombo_DNA_RNA:
            result = "NucAcid";
            break;
        case eIbisProteinProtein:
            result = "Protein";
            break;
        case eIbisProteinPeptide:
            result = "Peptide";
            break;
        case eIbisProteinIon:
            result = "Ion";
            break;
        case eIbisProteinChemical:
            result = "Chemical";
            break;
//        case ???:
//            cddType = eCddPosttransMod;
//            break;
        default:
            break;
    };
    return result;
}

IBISInteraction::eCddInteractionType IBISInteraction::IbisIntTypeToCddIntType(IBISInteraction::eIbisInteractionType ibisType)
{
    eCddInteractionType cddType = eCddOther;
    switch (ibisType) {
        case eIbisNoTypeAssigned:
            cddType = eCddNoTypeAssigned;
            break;
        case eIbisProteinDNA:
        case eIbisProteinRNA:
        case eIbisProteinCombo_DNA_RNA:
            cddType = eCddNABinding;
            break;
        case eIbisProteinProtein:
        case eIbisProteinPeptide:
            cddType = eCddPolypeptideBinding;
            break;
        case eIbisProteinIon:
            cddType = eCddIonBinding;
            break;
        case eIbisProteinChemical:
            cddType = eCddChemicalBinding;
            break;
//        case ???:
//            cddType = eCddPosttransMod;
//            break;
        default:
            INFOMSG("IBIS interaction type " << ibisType << " not mapped to a Cdd interaction type");
            break;
    };
    return cddType;
}

IBISInteraction::eIbisInteractionType IBISInteraction::CddIntTypeToIbisIntType(IBISInteraction::eCddInteractionType cddType)
{
    eIbisInteractionType ibisType = eIbisOther;
    switch (cddType) {
        case eCddNoTypeAssigned:
            ibisType = eIbisNoTypeAssigned;
            break;
        case eCddPolypeptideBinding:
            ibisType = eIbisProteinProtein;
            break;
        case eCddNABinding:
            ibisType = eIbisProteinCombo_DNA_RNA;
            break;
        case eCddIonBinding:
            ibisType = eIbisProteinIon;
            break;
        case eCddChemicalBinding:
            ibisType = eIbisProteinChemical;
            break;
//        case eCddPosttransMod:
//            ibisType = ???;
//            break;
        default:
            INFOMSG("Cdd interaction type " << cddType << " not mapped to an IBIS interaction type");
            break;
    };
    return ibisType;
}


IBISInteraction::IBISInteraction(const ncbi::objects::CSeq_feat& seqfeat)
{
    m_seqfeat.Assign(seqfeat);
    Initialize();
}

void IBISInteraction::Initialize(void) 
{
    int intValue;
    string label;

    m_rowId = NOT_ASSIGNED;
    if (m_seqfeat.IsSetId() && m_seqfeat.GetId().IsLocal() && m_seqfeat.GetId().GetLocal().IsId()) {
        m_rowId = m_seqfeat.GetId().GetLocal().GetId();
    }

    m_desc = kEmptyStr;
    if (m_seqfeat.GetData().IsRegion()) {
        m_desc = m_seqfeat.GetData().GetRegion();
    }

    //  The IBIS seqfeat by convention has zero-based positions.  
    //  Also, to ensure the use of consistent identifiers, because 
    //  IBIS may have a GI different than in the CD for the query
    //  structure, switch to the Seq-id found in the query sequence 
    //  [if the query actually worked, then we should end up w/ the PDB accession].
    bool inRange = true;
    TSeqPos from, to;
    CSeq_loc& location = m_seqfeat.SetLocation();
    if (location.IsInt()) {
        from = location.SetInt().SetFrom();
        to = location.SetInt().SetTo();
        location.SetInt().SetFrom(from);
        location.SetInt().SetTo(to);
        if (m_querySequence) 
            m_querySequence->FillOutSeqId(&(location.SetInt().SetId()));
        
    } else if (location.IsPacked_int()) {
        CPacked_seqint::Tdata::iterator s, se = location.SetPacked_int().Set().end();
        for (s = location.SetPacked_int().Set().begin(); s!=se; ++s) {
            from = (**s).SetFrom();
            to = (**s).SetTo();
            (**s).SetFrom(from);
            (**s).SetTo(to);
            if (m_querySequence) 
                m_querySequence->FillOutSeqId(&((**s).SetId()));
        }
    } else {
        ERRORMSG("interaction '" << m_desc << "' has unexpected location type");
    }

    if (!inRange) {
        ERRORMSG("interaction '" << m_desc << "' has out-of-range positions");
    }


    m_type = IBISInteraction::eIbisNoTypeAssigned;
    m_mmdbId = NOT_ASSIGNED;
    m_sdiId = NOT_ASSIGNED;
    m_score = NOT_ASSIGNED;
    m_nIntRes = NOT_ASSIGNED;
    m_nMembers = NOT_ASSIGNED;
    m_nUnqChem = NOT_ASSIGNED;
    m_nUnqIons = NOT_ASSIGNED;
    m_pid = (double) NOT_ASSIGNED;
    m_isObs = false;
    m_isFilt = false;
    m_isSing = false;

    if (m_seqfeat.IsSetExcept()) {
        m_isFilt = !(m_seqfeat.GetExcept());
    }

    if (m_seqfeat.IsSetExt()) {

        int intType = m_seqfeat.GetExt().GetType().GetId();
        m_type = IBISInteraction::eIbisOther;
        switch (intType) {
            case 1:
                m_type = eIbisProteinDNA;
                break;
            case 2:
                m_type = eIbisProteinRNA;
                break;
            case 3:
                m_type = eIbisProteinProtein;
                break;
            case 6:
                m_type = eIbisProteinChemical;
                break;
            case 7:
                m_type = eIbisProteinPeptide;
                break;
            case 8:
                m_type = eIbisProteinIon;
                break;
            case 12:
                m_type = eIbisProteinCombo_DNA_RNA;
                break;
            default:
                WARNINGMSG("IBIS feature data with unexpected type " << intType);
                break;
        };

        const CSeq_feat::TExt::TData& userObjectData = m_seqfeat.GetExt().GetData();
        CSeq_feat::TExt::TData::const_iterator uoCit = userObjectData.begin();
        CSeq_feat::TExt::TData::const_iterator uoEnd = userObjectData.end();
        for (; uoCit != uoEnd; ++uoCit) {

            if ((*uoCit)->GetLabel().IsStr()) {
                label = (*uoCit)->GetLabel().GetStr();
            } else {
                ERRORMSG("IBIS feature data with non-string label is skipped");
                continue;
            }

            if ((*uoCit)->GetData().IsInt()) {
                intValue = (*uoCit)->GetData().GetInt();
            } else {
                ERRORMSG("IBIS feature data with non-integral type and label '" << label << "' is skipped");
                continue;
            }

            if (label == "mmdbId") {
                m_mmdbId = intValue;
            } else if (label == "sdiId") {
                m_sdiId = intValue;
            } else if (label == "nMembers") {
                m_nMembers = intValue;
            } else if (label == "nInterfaceRes") {
                m_nIntRes = intValue;
            } else if (label == "nUniqueChemicals") {
                m_nUnqChem = intValue;
            } else if (label == "nUniqueIons") {
                m_nUnqIons = intValue;
            } else if (label == "comb_scr") {
                m_score = intValue;
            } else if (label == "avgIdent") {
                m_pid = (double) intValue/1000000.0;
            } else if (label == "isObserverd") {
                m_isObs = (intValue != 0);
            } else {
                ERRORMSG("IBIS feature data with unexpected label '" << label << "' is skipped");
            }
        }
    }
}

ncbi::CRef < ncbi::objects::CAlign_annot > IBISInteraction::ToAlignAnnot(void) const
{
    bool isLocOK = false;
    string commentEvid;
    CRef < CAlign_annot > annot(new CAlign_annot());
    annot->SetType(IbisIntTypeToCddIntType(m_type));
    annot->SetDescription(m_desc);

    // fill out location; isLocOK = true means it was a packed-int
    const ncbi::objects::CSeq_loc& loc = GetLocation(isLocOK);
    if (isLocOK) {
        CRef< CPacked_seqint > packedInt(new CPacked_seqint());
        packedInt->Assign(loc.GetPacked_int());  
        annot->SetLocation().SetPacked_int(*packedInt);

        //  add a boilerplate comment as evidence 
        CAlign_annot::TEvidence& evidence = annot->SetEvidence();
        CRef < CFeature_evidence > boilerplateEvidence(new CFeature_evidence());
        if (m_queryMolecule) {
            commentEvid = IBIS_EVIDENCE_COMMENT + " for query ";
            commentEvid += m_queryMolecule->ToString();
        } else {
            commentEvid = IBIS_EVIDENCE_COMMENT + " for an unspecified query structure";
        }
        
        CTimeFormat timeFormat = CTimeFormat::GetPredefined(CTimeFormat::eISO8601_DateTimeMin);
        commentEvid += " [created " + CTime(CTime::eCurrent).AsString(timeFormat) + "]";
        boilerplateEvidence->SetComment(commentEvid);
        evidence.push_back(boilerplateEvidence);

        //  keep the IBIS id for convenience of the GUI, although it
        //  is *not* a robust identifier for this interaction and may
        //  not be valid next time the IBIS query is made.
        /*
        CRef < CFeature_evidence > idEvidence(new CFeature_evidence());
        CObject_id& localFeatId = idEvidence->SetSeqfeat().SetId().SetLocal();
        localFeatId.SetId(m_rowId);
        evidence.push_back(idEvidence);
        */

    } else {
        ERRORMSG("error converting interaction '" << m_desc << "' to an annotation!");
        annot.Reset();
    }

    return annot;
}

const ncbi::objects::CSeq_loc& IBISInteraction::GetLocation(bool& isOK) const
{
    bool result = m_seqfeat.IsSetLocation();
    if (result) {
        if (!m_seqfeat.GetLocation().IsPacked_int()) {
            result = false;
            ERRORMSG("IBIS feature has unexpected location type");
        }
    } else {
        ERRORMSG("could not extract IBIS feature location");
    }
    isOK = result;
    return m_seqfeat.GetLocation();
}

const SeqPosSet& IBISInteraction::GetPositions(void) const
{
    //  Lazy-initialize the positions.
    if (m_positions.empty()) {
        bool isOK = false;
        TSeqPos pos, from, to;
        const CSeq_loc& location = GetLocation(isOK);
        if (isOK && location.IsInt()) {
            from = location.GetInt().GetFrom();
            to = location.GetInt().GetTo();
            for (pos = from; pos <= to; ++pos) {
                m_positions.insert(pos);
            }
        } else if (isOK && location.IsPacked_int()) {
            CPacked_seqint::Tdata::const_iterator s, se = location.GetPacked_int().Get().end();
            for (s = location.GetPacked_int().Get().begin(); s!=se; ++s) {
                from = (**s).GetFrom();
                to = (**s).GetTo();
                for (pos = from; pos <= to; ++pos) {
                    m_positions.insert(pos);
                }
            }
        }
    }
    return m_positions;
}

bool IBISInteraction::GetFootprint(int& from, int& to) const
{
    bool result;
    const CSeq_loc& seqloc = GetLocation(result);

    from = NOT_ASSIGNED;
    to = NOT_ASSIGNED;
    if (result) {
        const CPacked_seqint::Tdata& intervals = seqloc.GetPacked_int().Get();
        if (intervals.size() > 0) {
            from = (int) intervals.front()->GetFrom();
            to = (int) intervals.back()->GetTo();
        } else {
            result = false;
            ERRORMSG("IBIS feature has no defined intervals");
        }        
    }
    return result;
}

bool IBISInteraction::GetGi(int& gi) const
{
    bool result, idsMatch;
    const CSeq_loc& seqloc = GetLocation(result);

    gi = NOT_ASSIGNED;
    if (result) {
        const CPacked_seqint::Tdata& intervals = seqloc.GetPacked_int().Get();
        CPacked_seqint::Tdata::const_iterator it = intervals.begin(), itEnd = intervals.end();
        if (it != itEnd) {

            const CSeq_id& firstId = (*it)->GetId();
            ++it;

            //  Validate that each interval has the same 'id'.
            idsMatch = true;
            for (; idsMatch && it != itEnd; ++it) {
                idsMatch = firstId.Match((*it)->GetId());                
            }

            if (idsMatch) {
                if (firstId.IsGi()) {
                    gi = firstId.GetGi();
                } else {
                    result = false;
                    ERRORMSG("unexpected sequence identifier used for IBIS feature location (expected a GI)");
                }
            } else {
                result = false;
                ERRORMSG("inconsistent sequence ids used to give IBIS feature location");
            }
        } else {
            result = false;
            ERRORMSG("IBIS feature has no defined intervals");
        }        
    }
    return result;
}

//  end IBISInteraction methods
///////////////////////////////


#define DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERRORMSG("Can't find window with id " << id); \
        return; \
    }

#define DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERRORMSG("Can't find window with id " << id); \
        return false; \
    }

BEGIN_EVENT_TABLE(IBISAnnotateDialog, wxDialog)
    EVT_BUTTON      (-1,    IBISAnnotateDialog::OnButton)
    EVT_CHOICE      (-1,    IBISAnnotateDialog::OnChoice)
    EVT_LIST_ITEM_SELECTED (-1,    IBISAnnotateDialog::OnListCtrlSelection)
    EVT_LISTBOX     (-1,    IBISAnnotateDialog::OnListBoxSelection)
    EVT_CLOSE       (       IBISAnnotateDialog::OnCloseWindow)
END_EVENT_TABLE()

IBISAnnotateDialog::IBISAnnotateDialog(wxWindow *parent, IBISAnnotateDialog **handle, StructureSet *set) :
    wxDialog(parent, -1, "IBIS Annotations", wxPoint(400, 100), wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
    dialogHandle(handle), structureSet(set), annotSet(set->GetCDDAnnotSet()), m_images(NULL)
{
    if (annotSet.Empty()) {
        Destroy();
        return;
    }

    // get interaction data from IBIS
    PopulateInteractionData();

    // construct the panel; put the query ID in the static box title.
    wxSizer *topSizer = SetupIbisAnnotationDialog(this, false);
    if (ibisIntStaticBoxSizer && IBISInteraction::GetQueryMolecule()) {
        wxString buf;
        buf.Printf("IBIS Interactions for query %s", IBISInteraction::GetQueryMolecule()->ToString().c_str());
        try {
            wxStaticBoxSizer* sbs = dynamic_cast<wxStaticBoxSizer*>(ibisIntStaticBoxSizer);
            if (sbs) sbs->GetStaticBox()->SetLabel(buf);
        } catch (...) {
        }
    }


    // call sizer stuff
    topSizer->Fit(this);
    topSizer->SetSizeHints(this);
    SetClientSize(topSizer->GetMinSize());

    // Dynamically configure the possible choices based on
    // the available interactions.  Start off with 'All' selected
    // unless there are no interactions.
    int choiceId;
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(intTypeFilter, ID_C_TYPE, wxChoice)
    if (vecIbisInteractions.size() > 0) {
        if (!HasInteractionData(IBISInteraction::eIbisProteinProtein)) {
            choiceId = intTypeFilter->FindString(wxT("Protein-Protein"));
            if (choiceId != wxNOT_FOUND) {
                intTypeFilter->Delete(choiceId);
            }
        }
        if (!HasInteractionData(IBISInteraction::eIbisProteinDNA) &&
            !HasInteractionData(IBISInteraction::eIbisProteinRNA) &&
            !HasInteractionData(IBISInteraction::eIbisProteinCombo_DNA_RNA)) {
            choiceId = intTypeFilter->FindString(wxT("Protein-NucAcid"));
            if (choiceId != wxNOT_FOUND) {
                intTypeFilter->Delete(choiceId);
            }
        }
        if (!HasInteractionData(IBISInteraction::eIbisProteinChemical)) {
            choiceId = intTypeFilter->FindString(wxT("Protein-Chemical"));
            if (choiceId != wxNOT_FOUND) {
                intTypeFilter->Delete(choiceId);
            }
        }
        if (!HasInteractionData(IBISInteraction::eIbisProteinPeptide)) {
            choiceId = intTypeFilter->FindString(wxT("Protein-Peptide"));
            if (choiceId != wxNOT_FOUND) {
                intTypeFilter->Delete(choiceId);
            }
        }
        if (!HasInteractionData(IBISInteraction::eIbisProteinIon)) {
            choiceId = intTypeFilter->FindString(wxT("Protein-Ion"));
            if (choiceId != wxNOT_FOUND) {
                intTypeFilter->Delete(choiceId);
            }
        }

    }
    intTypeFilter->SetSelection(0);


    // initialize the image list
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(interactions, ID_LC_INT, wxListCtrl)
    wxIcon infoIcon = wxArtProvider::GetIcon(wxART_INFORMATION);  //, wxART_FRAME_ICON, wxSize(16, 16));
    wxIcon warnIcon = wxArtProvider::GetIcon(wxART_WARNING);    
    wxIcon questionIcon = wxArtProvider::GetIcon(wxART_QUESTION);    
    wxIcon bookmarkIcon = wxArtProvider::GetIcon(wxART_ADD_BOOKMARK);    
    wxIcon tickMarkIcon = wxArtProvider::GetIcon(wxART_TICK_MARK);  //, wxART_FRAME_ICON, wxSize(16, 16));
    if (tickMarkIcon.Ok()) {
        m_images = new wxImageList(16, 16, true);
        /*int imageIndex =*/ m_images->Add(infoIcon);
        m_images->Add(warnIcon);
        m_images->Add(questionIcon);
        m_images->Add(bookmarkIcon);
        m_images->Add(tickMarkIcon);
        interactions->SetImageList(m_images, wxIMAGE_LIST_SMALL);
    }

    // initialize the column headings in the list control
    if (interactions->GetWindowStyleFlag() & wxLC_REPORT) {
        wxListItem itemCol;
        itemCol.SetText(wxT("Annot Overlap?"));
        itemCol.SetAlign(wxLIST_FORMAT_LEFT);
        interactions->InsertColumn(0, itemCol);

        itemCol.SetText(wxT("Type"));
        itemCol.SetAlign(wxLIST_FORMAT_LEFT);
        interactions->InsertColumn(1, itemCol);

        itemCol.SetText(wxT("Observed?"));
        itemCol.SetAlign(wxLIST_FORMAT_RIGHT);
        interactions->InsertColumn(2, itemCol);

        itemCol.SetText(wxT("Filtered on Web?"));
        itemCol.SetAlign(wxLIST_FORMAT_RIGHT);
        interactions->InsertColumn(3, itemCol);

        itemCol.SetText(wxT("Description"));
        itemCol.SetAlign(wxLIST_FORMAT_LEFT);
        interactions->InsertColumn(4, itemCol);

        itemCol.SetText(wxT("# Res. on Query"));
        itemCol.SetAlign(wxLIST_FORMAT_CENTER);
        interactions->InsertColumn(5, itemCol);

        itemCol.SetText(wxT("Clust. Size"));
        itemCol.SetAlign(wxLIST_FORMAT_CENTER);
        interactions->InsertColumn(6, itemCol);

        itemCol.SetText(wxT("Avg. Clust. %Id"));
        itemCol.SetAlign(wxLIST_FORMAT_CENTER);
        interactions->InsertColumn(7, itemCol);

        itemCol.SetText(wxT("IBIS Score"));
        itemCol.SetAlign(wxLIST_FORMAT_CENTER);
        interactions->InsertColumn(8, itemCol);
    }


    // set initial GUI state, with no selections
    SetupGUIControls(-1, -1);
}


IBISAnnotateDialog::~IBISAnnotateDialog(void)
{
    delete m_images;

    // reset the query sequence and molecule to NULL
    IBISInteraction::SetQuerySequence(NULL);

    // so owner knows that this dialog has been destroyed
    if (dialogHandle && *dialogHandle) *dialogHandle = NULL;
    TRACEMSG("destroyed IBISAnnotateDialog");
}

void IBISAnnotateDialog::OnCloseWindow(wxCloseEvent& event)
{
    Destroy();
}

//  Returns -1 if there are no selected items
//  (instead of this method, can use wxListCtrl::GetSelectedItemCount() 
//   directly to test if a selection exists)
long GetFirstSelectedListCtrlItemId(const wxListCtrl& listCtrl)
{
    return listCtrl.GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
}

//  Returns the number of selections, and the selected ids.
//  Needed only if the listCtrl allows multiple selections.
unsigned int GetAllSelectedListCtrlItemIds(const wxListCtrl& listCtrl, vector<long>& ids)
{
    long item = listCtrl.GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

    ids.clear();
    while ( item != -1 ) {
        ids.push_back(item);
        item = listCtrl.GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    }
    return ids.size();
}

/*  Conventions/formatting of ibis.cgi calls:

https://www.ncbi.nlm.nih.gov/Structure/ibis/ibis.cgi?sqat&range=1,332&mmdbid=51257&molid=1&asc=1&web=1

Arguments:
sqat:   trigger output SeqAnnot Asn1 data
range:  residue range from, to (requires ones-based positions)
mmdbid: query Mmdb Id
molid:  molecule Id, (default value: 1)  ** no general mapping to Chain ID of the structure!!! **
asc:    ascii output (default is binary)
web:    for web display (default is text output without web content header)

If no interaction found, the cgi should return a SeqAnnot with an
empty ftable:

Seq-annot ::= {
  data ftable {
  }
}
*/
typedef struct {
    int mmdbId;
    int molId;         //  warning:  molId doesn't map easily onto pdb chain code (e.g., 1rzr, molId=1 --> chain G)
    TSeqPos from, to;
    string pdb;
} IbisQuery;

static bool GetInteractionDataFromIbis(CSeq_annot& ibisSeqAnnot, IbisQuery& query) 
{
    bool result = false;

    if (query.from > query.to) {
        ERRORMSG("Error calling ibis.cgi:  from (" << query.from << ") > to (" << query.to << ").");
        return result;
    } else if (query.mmdbId == MoleculeIdentifier::VALUE_NOT_SET) {
        ERRORMSG("Error calling ibis.cgi:  mmdbId not set.");
        return result;
    } else if (query.molId == MoleculeIdentifier::VALUE_NOT_SET) {
        ERRORMSG("Error calling ibis.cgi:  molId not set..");
        return result;
    }

    // set up URL
    string args, err,
        host = "www.ncbi.nlm.nih.gov",
        path = "/Structure/ibis/ibis.cgi";
    CNcbiOstrstream argstr;
    argstr << "sqat&range=" << (query.from+1) << ',' << (query.to+1) << "&mmdbid=" << query.mmdbId
        << "&molid=" << query.molId
        << "&asc=1&web=1"; 
    args = (string) CNcbiOstrstreamToString(argstr);

    // connect to ibis.cgi
    // 1) try to get interaction data using the mmdb-id; if the mmdb-id is obsolete,
    // ibis.cgi will return an empty seq-annot.  
    // 2) in case of empty seq-annot, re-query with the pdb accession
    // [the second query corresponds to the *current* mmdb-id for that pdb, and
    //  runs the risk that the returned interactions are mapped onto an updated
    //  version of the sequence, not the sequence referenced by the mmdb-id in the CD]. 
    INFOMSG("try to load IBIS alignment data by mmdb-id from " << host << path << '?' << args);
    result = GetAsnDataViaHTTPS(host, path, args, &ibisSeqAnnot, &err, false);
    if (!result) {
        ERRORMSG("Error getting interaction data from ibis.cgi:\n" << err);
    } else {
        if (!ibisSeqAnnot.GetData().IsFtable() || ibisSeqAnnot.GetData().GetFtable().empty()) {
            ibisSeqAnnot.Reset();
            if (query.pdb.length() > 0) {

                CNcbiOstrstream argstrPdb;
                argstrPdb << "sqat&range=" << (query.from+1) << ',' << (query.to+1) << "&search=" << query.pdb
                    << "&asc=1&web=1";
                args = (string) CNcbiOstrstreamToString(argstrPdb);

                INFOMSG("try to load IBIS alignment data by pdb accession from " << host << path << '?' << args);
                err.erase();
                result = GetAsnDataViaHTTPS(host, path, args, &ibisSeqAnnot, &err, false);
                if (!result) {
                    ERRORMSG("Error getting interaction data by pdb accession from ibis.cgi:\n" << err);
                } else {
                    if (!ibisSeqAnnot.GetData().IsFtable() || ibisSeqAnnot.GetData().GetFtable().empty()) {
                        ibisSeqAnnot.Reset();
                        INFOMSG("    ibis.cgi has no interaction data for MMDB-ID " << query.mmdbId << ", molecule " << query.molId << " or PDB accession " << query.pdb);
                    } else {    
                        INFOMSG("    successfully loaded interaction data from ibis.cgi for PDB accession " << query.pdb);
                    }
                }
            } else {
                INFOMSG("    ibis.cgi has no interaction data for MMDB-ID " << query.mmdbId << ", molecule " << query.molId << " (no known PDB accession)");
            }
        } else {
            INFOMSG("    successfully loaded interaction data from ibis.cgi for MMDB-ID " << query.mmdbId);
        }
    }
/*
#ifdef _DEBUG
    WriteASNToFile("ibisannot.dat.txt", ibisSeqAnnot, false, &err);
#endif
*/
    return result;
}


void IBISAnnotateDialog::PopulateInteractionData(void)
{
    IbisQuery query;

    //  get Sequence object for master, and pull info out of MoleculeIdentifier
    const BlockMultipleAlignment *alignment = structureSet->alignmentManager->GetCurrentMultipleAlignment();
    const Sequence *master = alignment->GetMaster();

    vecIbisInteractions.clear();
    if (master->identifier && master->identifier->HasStructure()) {

        ncbi::CRef< ncbi::objects::CSeq_annot > ibisInteractions(new CSeq_annot);
        if (ibisInteractions.NotEmpty()) {

            //  Set the static query variables in the IBISInteraction class
            IBISInteraction::SetQuerySequence(master);

            query.mmdbId = master->identifier->mmdbID;
            query.molId = master->identifier->moleculeID;

            query.pdb = kEmptyStr;
            if (master->identifier->pdbID.size() > 0) {
                query.pdb = master->identifier->pdbID;
                if (IBISInteraction::QueryMoleculeHasChain())  //master->identifier->pdbChain != MoleculeIdentifier::VALUE_NOT_SET && master->identifier->pdbChain != ' ')
                    query.pdb += (char) master->identifier->pdbChain;
            }

            // find first aligned residue
            query.from = 0;
            while (query.from < master->Length() && !alignment->IsAligned(0U, query.from))
                ++query.from;
                   
            // find last aligned residue
            query.to = master->Length() -1;
            while (query.to >= query.from && !alignment->IsAligned(0U, query.to))
                --query.to;

            // no extra message; errors are posted from inside the function.
            if (!GetInteractionDataFromIbis(*ibisInteractions, query)) {
                ibisInteractions.Reset();
            } else if (ibisInteractions.NotEmpty()&& ibisInteractions->GetData().IsFtable() && (ibisInteractions->GetData().GetFtable().size() > 0)) {

                const CSeq_annot::TData::TFtable& seqfeats = ibisInteractions->GetData().GetFtable();
                CSeq_annot::TData::TFtable::const_iterator fit = seqfeats.begin(), fend = seqfeats.end();
                for (; fit != fend; ++fit) {
                    if (fit->NotEmpty()) {
                        CRef< IBISInteraction > ibisInt(new IBISInteraction(**fit));
                        vecIbisInteractions.push_back(ibisInt);
                    }
                }

            }
        } else {
            ERRORMSG("Error:  allocation failure before trying to contact ibis.cgi.");
        }
    }

}

bool IBISAnnotateDialog::HasInteractionData(void) const
{
    return (vecIbisInteractions.size() > 0);
}

bool IBISAnnotateDialog::HasInteractionData(IBISInteraction::eIbisInteractionType type) const
{
    bool result = false;
    unsigned int num = vecIbisInteractions.size();
    for (unsigned i = 0; i < num && !result; ++i) {
        if (vecIbisInteractions[i]->GetType() == type) {
            result = true;
        }
    }
    return result;
}

unsigned int IBISAnnotateDialog::GetIntervalsForSet(const SeqPosSet& positions, IntervalList& intervals)
{
    intervals.clear();
    if (positions.size() == 0) return 0;

    const Sequence *master = structureSet->alignmentManager->GetCurrentMultipleAlignment()->GetMaster();
    SeqPosSet::const_iterator pcit = positions.begin(), pend = positions.end();
    TSeqPos first = *pcit;
    TSeqPos last = first;

    while (pcit != pend) {
        ++pcit;
        // find last in contiguous stretch
        if (pcit != pend && (*pcit == last + 1)) {
            ++last;
        } else {
            // create Seq-interval
            CRef < CSeq_interval > interval(new CSeq_interval());
            interval->SetFrom(first);
            interval->SetTo(last);
            master->FillOutSeqId(&(interval->SetId()));
            intervals.push_back(interval);
            if (pcit != pend) {
                first = *pcit;
                last = first;
            }
        }

    }
    return positions.size();
}


void IBISAnnotateDialog::OnButton(wxCommandEvent& event)
{
    int eventId = event.GetId();
    switch (eventId) {

        // IBIS interaction buttons
        case ID_B_ADD_INT:
            MakeAnnotationFromInteraction();
            break;
        case ID_B_HIGHLIGHT_INT:
            HighlightInteraction();
            break;
        case ID_B_LAUNCH_INT:
            LaunchIbisWebPage();
            break;

        // annotation buttons
        case ID_B_DELETE_ANNOT:
            DeleteAnnotation();
            break;
        case ID_B_HIGHLIGHT_ANNOT:
        case ID_B_HIGHLIGHT_OLAP:
        case ID_B_HIGHLIGHT_NONOLAP_ANNOT:
        case ID_B_HIGHLIGHT_NONOLAP_INTN:
            HighlightAnnotation(eventId);
            break;
        default:
            event.Skip();
    }
}

void IBISAnnotateDialog::OnChoice(wxCommandEvent& event)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(interactions, ID_LC_INT, wxListCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_LB_ANNOT, wxListBox)

    long item = GetFirstSelectedListCtrlItemId(*interactions);

    SetupGUIControls((int) item, annots->GetSelection());
    HighlightInteraction();
}

void IBISAnnotateDialog::OnListCtrlSelection(wxListEvent& event)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(interactions, ID_LC_INT, wxListCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_LB_ANNOT, wxListBox)

    wxString eventText = event.GetText();
    //long eventData = event.GetData();
    //const wxListItem& eventItem = event.GetItem();

    SetupGUIControls(event.GetIndex(), annots->GetSelection(), eRemakeListBox);
    HighlightInteraction();
}
void IBISAnnotateDialog::OnListBoxSelection(wxCommandEvent& event)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(interactions, ID_LC_INT, wxListCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_LB_ANNOT, wxListBox)

    int intSel = GetFirstSelectedListCtrlItemId(*interactions);
    SetupGUIControls(intSel, annots->GetSelection(), eDontRemakeControls);
    HighlightAnnotation(ID_B_HIGHLIGHT_ANNOT);
}

void InsertItemInListView(int vecIndex, const CRef<IBISInteraction>& ibisInt, wxListCtrl& listCtrl)
{
    if (ibisInt.Empty()) return;

    long itemIndex;
    wxString buf;
    buf.Printf(wxT("(%d)  %s  [%d in cluster (%4.1f %%id); %d residues]"), (int) ibisInt->GetType(), 
        ibisInt->GetDesc().c_str(), ibisInt->GetNumMembers(), ibisInt->GetAverageIdentity(), ibisInt->GetNumInterfaceResidues());
    itemIndex = listCtrl.InsertItem(vecIndex, buf, -1);
    if (itemIndex >= 0) {
        listCtrl.SetItemData(itemIndex, vecIndex);
    }
}

void InsertItemInReportView(int vecIndex, const CRef<IBISInteraction>& ibisInt, wxListCtrl& listCtrl)
{
    if (ibisInt.Empty()) return;

    //  column order:  annotOverlap/IBIStype/observed/filtered/descr/# res/clust size/avg ident/score

    wxString buf;
    buf.Printf(wxT("%s "), IBISInteraction::IbisIntTypeToString(ibisInt->GetType()).c_str());

    long itemIndex = listCtrl.InsertItem(vecIndex, "", -1);
    if (itemIndex >= 0) {
        listCtrl.SetItemData(itemIndex, vecIndex);

        listCtrl.SetItem(itemIndex, 1, buf, -1);

        if (ibisInt->IsObserved()) {
            listCtrl.SetItem(itemIndex, 2, "", 3);
        }
        if (ibisInt->IsFiltered()) {
            listCtrl.SetItem(itemIndex, 3, "", 2);
        }

        buf.Printf(wxT("%s"), ibisInt->GetDesc().c_str());
        listCtrl.SetItem(itemIndex, 4, buf);

        //  GetNumInterfaceResidues() refers to all residues in the 
        //  IBIS-defined interaction that the query hit.
        //  GetPositions.size() gets the number of interface residues that
        //  were found to overlap the query and are in the Seq-loc.
        //  GetPositions.size() <= GetNumInterfaceResidues()
//        buf.Printf(wxT("%d"), ibisInt->GetNumInterfaceResidues());
        buf.Printf(wxT("%d"), (int) ibisInt->GetPositions().size());
        listCtrl.SetItem(itemIndex, 5, buf);

        buf.Printf(wxT("%d"), ibisInt->GetNumMembers());
        listCtrl.SetItem(itemIndex, 6, buf);
        buf.Printf(wxT("%4.1f"), ibisInt->GetAverageIdentity());
        listCtrl.SetItem(itemIndex, 7, buf);
        if (ibisInt->GetScore() != IBISInteraction::NOT_ASSIGNED) {
            buf.Printf(wxT("%4.1f"), (double) ibisInt->GetScore()/10000.0);
        } else {
            buf = wxT("n/a");
        }
        listCtrl.SetItem(itemIndex, 8, buf);
    }
}

bool DoesInteractionMatchChoice(const wxString& choiceStr, IBISInteraction::eIbisInteractionType ibisIntType)
{
    bool result = false;
    if (choiceStr == wxT("All")) {
        result = (ibisIntType != IBISInteraction::eIbisNoTypeAssigned &&
                  ibisIntType != IBISInteraction::eIbisOther);
    } else if (choiceStr.Contains(wxT("Protein-Protein"))) {
        result = (ibisIntType == IBISInteraction::eIbisProteinProtein);
    } else if (choiceStr.Contains(wxT("Protein-NucAcid"))) {
        result = ((ibisIntType == IBISInteraction::eIbisProteinDNA) ||
                  (ibisIntType == IBISInteraction::eIbisProteinRNA) ||
                  (ibisIntType == IBISInteraction::eIbisProteinCombo_DNA_RNA));
    } else if (choiceStr.Contains(wxT("Protein-Chemical"))) {
        result = (ibisIntType == IBISInteraction::eIbisProteinChemical);
    } else if (choiceStr.Contains(wxT("Protein-Peptide"))) {
        result = (ibisIntType == IBISInteraction::eIbisProteinPeptide);
    } else if (choiceStr.Contains(wxT("Protein-Ion"))) {
        result = (ibisIntType == IBISInteraction::eIbisProteinIon);
    }
    return result;
}

void GetPositionsForAlignAnnot(const CAlign_annot& annot, SeqPosSet& positions, bool zeroBased)
{
    positions.clear();

    TSeqPos offset, pos, from, to;
    const CSeq_loc& annotSeqLoc = annot.GetLocation();
    CPacked_seqint::Tdata::const_iterator s, se;

    offset = (zeroBased) ? 0 : 1;

    if (annotSeqLoc.IsInt()) {
        from = annotSeqLoc.GetInt().GetFrom() + offset;
        to = annotSeqLoc.GetInt().GetTo() + offset;
        for (pos = from; pos <= to; ++pos) {
            positions.insert(pos);
        }
    } else if (annotSeqLoc.IsPacked_int()) {
        se = annotSeqLoc.GetPacked_int().Get().end();
        for (s = annotSeqLoc.GetPacked_int().Get().begin(); s!=se; ++s) {
            from = (**s).GetFrom() + offset;
            to = (**s).GetTo() + offset;
            for (pos = from; pos <= to; ++pos) {
                positions.insert(pos);
            }
        }
    }

}

static unsigned int GetSetIntersection(const SeqPosSet& set1, const SeqPosSet& set2, SeqPosSet& intersection)
{
    intersection.clear();
    if (set1.empty() || set2.empty()) return 0;

    set_intersection(set1.begin(), set1.end(),
                     set2.begin(), set2.end(),
                     inserter(intersection, intersection.begin()));

    return intersection.size();
}

void IBISAnnotateDialog::GetAnnotIbisOverlaps(AnnotIbisOverlapMap& aioMap) const
{
    unsigned int i;
    unsigned int nInteractions = vecIbisInteractions.size();
    if (nInteractions == 0) return;

    SeqPosSet annotPositions, overlapPositions;
    AlignAnnotInfo annotInfo;
    vector< SeqPosSet >& overlaps = annotInfo.overlaps;
    CAlign_annot_set::Tdata::const_iterator a, ae = annotSet->Get().end();

    aioMap.clear();
    overlaps.resize(nInteractions);
    for (a=annotSet->Get().begin(); a!=ae; ++a) {

        annotInfo.ptr = (void *) a->GetPointer();

        annotPositions.clear();
        if (a->NotEmpty()) {
            GetPositionsForAlignAnnot(**a, annotPositions, true);
        }
        annotInfo.nRes = annotPositions.size();

        for (i = 0; i < nInteractions; ++i) {
            const SeqPosSet& interactionPositions = vecIbisInteractions[i]->GetPositions();
            GetSetIntersection(interactionPositions, annotPositions, overlapPositions);
            overlaps[i] = overlapPositions;
        }

        aioMap.insert(AnnotIbisOverlapMap::value_type((unsigned long) annotInfo.ptr, annotInfo));
    }
}

void IBISAnnotateDialog::GetAnnotIbisOverlaps(const IBISInteraction& interaction, const CAlign_annot& annot, SeqPosSet& overlaps) const
{
    SeqPosSet annotPositions;
    const SeqPosSet& interactionPositions = interaction.GetPositions();

    overlaps.clear();
    GetPositionsForAlignAnnot(annot, annotPositions, true);
    GetSetIntersection(interactionPositions, annotPositions, overlaps);
}

void IBISAnnotateDialog::GetAnnotIbisNonOverlaps(const IBISInteraction& interaction, const CAlign_annot& annot, SeqPosSet& nonOverlaps, bool onAnnotation) const
{
    SeqPosSet annotPositions, annotPositionOverlaps;
    SeqPosSet::iterator setIt, setEnd;
    const SeqPosSet& interactionPositions = interaction.GetPositions();

    nonOverlaps.clear();
    GetPositionsForAlignAnnot(annot, annotPositions, true);
    GetSetIntersection(interactionPositions, annotPositions, annotPositionOverlaps);

    //  Now, find those positions in the appropriate set not in 'annotPositionOverlaps'
    if (onAnnotation) {
        nonOverlaps.insert(annotPositions.begin(), annotPositions.end());
    } else {
        nonOverlaps.insert(interactionPositions.begin(), interactionPositions.end());
    }

    setEnd = annotPositionOverlaps.end();
    for (setIt = annotPositionOverlaps.begin(); setIt != setEnd; ++setIt) {
        nonOverlaps.erase(*setIt);
    }
}

void IBISAnnotateDialog::SetupGUIControls(int selectInteraction, int selectAnnot, unsigned int updateFlags) 
{
    static const double overlapThreshold = 99.0;

    // get GUI control pointers
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(intTypeFilter, ID_C_TYPE, wxChoice)

    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(interactions, ID_LC_INT, wxListCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bAddInt, ID_B_ADD_INT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bLaunchInt, ID_B_LAUNCH_INT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bHighlightInt, ID_B_HIGHLIGHT_INT, wxButton)

    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_LB_ANNOT, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bDelAnnot, ID_B_DELETE_ANNOT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bHighlightAnnot, ID_B_HIGHLIGHT_ANNOT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bHighlightOlap, ID_B_HIGHLIGHT_OLAP, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bHighlightNoOlapA, ID_B_HIGHLIGHT_NONOLAP_ANNOT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bHighlightNoOlapI, ID_B_HIGHLIGHT_NONOLAP_INTN, wxButton)

    bool readOnly, isIbisAnnot, isFullInteractionAnnotated;
    double overlapPercentage;
    unsigned int i, ibisIndex, nInteractions, nItems, nOverlaps, nRes, nResIntn;
    int selectedVecIndex = -1;

    CAlign_annot *annotPtr = NULL;
    //IBISInteraction::eIbisInteractionType ibisType = IBISInteraction::eIbisOther;
    wxString annotDescStr, choiceSelectionStr;
    set<unsigned int> ibisIndexWithOverlap;
    set<unsigned int>::iterator ibisIndexEnd;
    AnnotIbisOverlapMap overlapMap;
    AnnotIbisOverlapMap::iterator olapIt, olapEnd;

    nInteractions = vecIbisInteractions.size();
    choiceSelectionStr = intTypeFilter->GetStringSelection();

    //  Find out which user annotations overlap IBIS interactions,
    //  and which of of the IBIS interactions overlap some user annotation.
    GetAnnotIbisOverlaps(overlapMap);
    olapEnd = overlapMap.end();
    for (olapIt = overlapMap.begin(); olapIt != olapEnd; ++olapIt) {
        
        if (olapIt->second.nRes == 0) continue;

        vector< SeqPosSet >& olaps = olapIt->second.overlaps;
        for (i = 0; i < olaps.size(); ++i) {
            if (olaps[i].size() > 0) { 
                ibisIndexWithOverlap.insert(i);
            }
        }
    }
    ibisIndexEnd = ibisIndexWithOverlap.end();

    // Don't select 'selectInteraction' unless it matches the choice selection;
    // since item index need not equal the vector index, get the selected vector index.
    if (selectInteraction >= 0) {
        selectedVecIndex = interactions->GetItemData(selectInteraction);
        if (selectedVecIndex < 0 || selectedVecIndex >= (int) nInteractions) {
            selectedVecIndex = -1;
            selectInteraction = -1;
        } else if (!DoesInteractionMatchChoice(choiceSelectionStr, vecIbisInteractions[selectedVecIndex]->GetType())) {
            selectedVecIndex = -1;
            selectInteraction = -1;
        }
    }

    // If remaking a control, a select index into it may be obsolete so reset it.
    if (updateFlags & eRemakeListCtrl) {
        selectInteraction = -1;
        selectAnnot = -1;
    } else if (updateFlags & eRemakeListBox) {
        selectAnnot = -1;
    }

    //  If there's no selected interaction, there will be no selected annotation.
    nResIntn = 0;
    if (selectedVecIndex < 0) {
        selectAnnot = -1;
    } else {
        nResIntn = vecIbisInteractions[selectedVecIndex]->GetPositions().size();
    }

    //
    // Setup the wxListCtrl to display the possibly-filtered interactions.
    //

    interactions->Hide();

    if (updateFlags & eRemakeListCtrl) {
        interactions->DeleteAllItems();  // columns are retained
        if (interactions->GetWindowStyleFlag() & wxLC_LIST) {
            for (i = 0; i < nInteractions; ++i) {
                if (DoesInteractionMatchChoice(choiceSelectionStr, vecIbisInteractions[i]->GetType())) {
                    InsertItemInListView(i, vecIbisInteractions[i], *interactions);
                }
            }
        } else if (interactions->GetWindowStyleFlag() & wxLC_REPORT) {

            for (i = 0; i < nInteractions; ++i) {
                if (DoesInteractionMatchChoice(choiceSelectionStr, vecIbisInteractions[i]->GetType())) {
                    InsertItemInReportView(i, vecIbisInteractions[i], *interactions);
                }
            }
        }

        //  ***  using SetItemState generates a selection event
        //       so be careful about remaking the viewer and then select 
        //       an item in this method due to event recursion possible
        //       w/ OnListCtrlSelection
        //  ***
        //  To set the selection after remaking the list control, need to 
        //  find which new item index corresponds to the selectedVecIndex.
        if (selectedVecIndex >= 0) {
            this->SetEvtHandlerEnabled(false);
            nItems = interactions->GetItemCount();
            for (i = 0; i < nItems; ++i) {
                if ((int) interactions->GetItemData(i) == selectedVecIndex) {
                    selectInteraction = i;
                    interactions->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
                    break;
                }
            }
            this->SetEvtHandlerEnabled(true);
        }
    }

    //  Turn on image for images with overlapping annotations.
    nItems = interactions->GetItemCount();
    for (i = 0; i < nItems; ++i) {
        ibisIndex = (unsigned int) interactions->GetItemData(i);
        if (ibisIndexWithOverlap.find(ibisIndex) != ibisIndexEnd) {
            interactions->SetItemImage(i, 4);
        }
    }

    for (i = 0; i < (unsigned int) interactions->GetColumnCount(); ++i) {
        if (i <= 4) {
            interactions->SetColumnWidth(i, wxLIST_AUTOSIZE);  //  type/annotOverlap/observed/filtered/description
        } else {
            interactions->SetColumnWidth(i, wxLIST_AUTOSIZE_USEHEADER);
        }
    }

    interactions->Show();

    if (selectInteraction >= 0) {
        interactions->EnsureVisible(selectInteraction);
    }

    //
    // Set up wxListBox with only those annotations that overlap IBIS selection
    // 
    
    isFullInteractionAnnotated = false;
    if (updateFlags & eRemakeListBox || selectedVecIndex < 0) 
        annots->Clear();

    if (selectedVecIndex >= 0 && (ibisIndexWithOverlap.find(selectedVecIndex) != ibisIndexEnd)) {

        for (olapIt = overlapMap.begin(); olapIt != olapEnd; ++olapIt) {
            nOverlaps = olapIt->second.overlaps[selectedVecIndex].size();
            nRes = olapIt->second.nRes;
            annotPtr = reinterpret_cast<CAlign_annot*>(olapIt->second.ptr);
            if (nOverlaps > 0 && annotPtr) {

                overlapPercentage = (nRes > 0) ? (100.0*nOverlaps)/((double) nRes) : 0.0;
                isFullInteractionAnnotated = (overlapPercentage >= overlapThreshold);

                if (updateFlags & eRemakeListBox) {
                    if (annotPtr->IsSetDescription()) {
                        annotDescStr.Printf("%s (%d residues; %4.1f%% are overlapped)", annotPtr->GetDescription().c_str(), nRes, overlapPercentage);
                    } else {
                        annotDescStr.Printf("<no description> (%d residues; %4.1f%% are overlapped)", nRes, overlapPercentage);
                    }
                    annots->Append(annotDescStr, olapIt->second.ptr);
                }
            }
        }
    }

    if (annots->GetCount() == 0) {
        selectAnnot = -1;
    }

    nOverlaps = 0;
    nRes = 0;      // number of residues in selected annotation
    isIbisAnnot = false;
    if (selectAnnot >= 0) {
        annots->SetSelection(selectAnnot);
        annotPtr = reinterpret_cast<CAlign_annot*>(annots->GetClientData(selectAnnot));
        if (annotPtr) {
            isIbisAnnot = annotPtr->IsSetType();
            olapIt = overlapMap.find((unsigned long) annotPtr);
            if (selectedVecIndex >= 0 && olapIt != olapEnd) {
                nOverlaps = olapIt->second.overlaps[selectedVecIndex].size();
                nRes = olapIt->second.nRes;
            }
        }
    } else 
        annots->SetSelection(wxNOT_FOUND);

    /*   probably not required
    if (annots->GetCount() > 0) {
        annotPtr = reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
        annots->SetFirstItem(pos);
    }
    */

    // set button states
    RegistryGetBoolean(REG_ADVANCED_SECTION, REG_CDD_ANNOT_READONLY, &readOnly);

    bAddInt->Enable(selectInteraction >= 0 && !isFullInteractionAnnotated && !readOnly);
    bLaunchInt->Enable(selectInteraction >= 0);
    bHighlightInt->Enable(selectInteraction >= 0);

    bDelAnnot->Enable(selectAnnot >= 0 && isIbisAnnot && !readOnly);
    bHighlightAnnot->Enable(selectAnnot >= 0);
    bHighlightOlap->Enable(selectAnnot >= 0 && nOverlaps > 0 && (nOverlaps < nRes));

    //  enable only if there are residues not overlapped in the annotation/interaction
    bHighlightNoOlapA->Enable(selectAnnot >= 0 && nRes > 0 && (nOverlaps < nRes));
    bHighlightNoOlapI->Enable(selectAnnot >= 0 && nResIntn > 0 && (nOverlaps < nResIntn));

    interactions->SetFocus();
}

void IBISAnnotateDialog::LaunchIbisWebPage(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(interactions, ID_LC_INT, wxListCtrl)

    bool launchGeneralUrl = false;
    long selectedItem = GetFirstSelectedListCtrlItemId(*interactions);
    wxString url, pdbQuery;

    // get selected interaction
    int index = interactions->GetItemData(selectedItem);
    if (index < 0 || index >= (int) vecIbisInteractions.size()) {
        ERRORMSG("IBISAnnotateDialog::LaunchIbisWebPage() - error getting IBIS interaction with index " << index);
        return;
    }

    const CRef<IBISInteraction>& ibisInt = vecIbisInteractions[index];

    //  if only want a general page for a given pdb id:
    //  https://www.ncbi.nlm.nih.gov/Structure/ibis/ibis.cgi?search=1y7oa
    //  
    //  but to open a page on the exact selected interaction, use something like this:
    //  https://www.ncbi.nlm.nih.gov/Structure/ibis/ibis.cgi?id=131080&type=7&rowid=116103

    if (ibisInt.NotEmpty()) {

        //  If we can't build a specific URL for the selected interaction, open the general page.
        if (ibisInt->GetType() == IBISInteraction::eIbisOther || ibisInt->GetType() == IBISInteraction::eIbisNoTypeAssigned) {
            launchGeneralUrl = true;
        } else if (ibisInt->GetSdiId() == IBISInteraction::NOT_ASSIGNED || ibisInt->GetRowId() == IBISInteraction::NOT_ASSIGNED) {
            launchGeneralUrl = true;
        }

        if (launchGeneralUrl) {
            if (IBISInteraction::GetQueryMolecule()) {
                if (IBISInteraction::QueryMoleculeHasChain()) {
                    pdbQuery.Printf("%s%c", IBISInteraction::GetQueryMolecule()->pdbID.c_str(), (char) IBISInteraction::GetQueryMolecule()->pdbChain);
                } else {
                    pdbQuery.Printf("%s", IBISInteraction::GetQueryMolecule()->pdbID.c_str());
                }
                url.Printf("https://www.ncbi.nlm.nih.gov/Structure/ibis/ibis.cgi?search=%s", pdbQuery.c_str());
            }
        } else {
            url.Printf("https://www.ncbi.nlm.nih.gov/Structure/ibis/ibis.cgi?id=%d&type=%d&rowid=%d", ibisInt->GetSdiId(), (int) ibisInt->GetType(), ibisInt->GetRowId());
        }
    }

    if (url.length() > 0) {
        LaunchWebPage(url.c_str());
    } else {
        ERRORMSG("IBISAnnotateDialog::LaunchIbisWebPage() - problem building URL for IBIS interaction with index " << index);
    }
}

void IBISAnnotateDialog::HighlightInteraction(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(interactions, ID_LC_INT, wxListCtrl)

    bool okay = true, isLocationOK = false;
    long selectedItem = GetFirstSelectedListCtrlItemId(*interactions);

    // If there are no interactions, leave any existing highlighting.
    // If interactions exist, clear any existing highlighting to avoid
    // confusion that it may belong to some interaction in the list.
    if (interactions->GetItemCount() == 0) {
        return;
    } else if (selectedItem < 0) {
        GlobalMessenger()->RemoveAllHighlights(true);
        return;
    }

    // get selection
    int index = interactions->GetItemData(selectedItem);
    if (index < 0 || index >= (int) vecIbisInteractions.size()) {
        ERRORMSG("IBISAnnotateDialog::HighlightAnnotation() - error getting IBIS interaction with index " << index);
        return;
    }

    const CRef<IBISInteraction>& ibisInt = vecIbisInteractions[index];
    const CSeq_loc& location = ibisInt->GetLocation(isLocationOK);

    //  if isLocationOK is true, 'location' will be of packed-int type.
    if (isLocationOK) {

        // highlight annotation's intervals
        GlobalMessenger()->RemoveAllHighlights(true);
        if (location.IsInt()) {
            okay = HighlightInterval(location.GetInt());
        } else if (location.IsPacked_int()) {
            CPacked_seqint::Tdata::const_iterator s,
                se = location.GetPacked_int().Get().end();
            for (s=location.GetPacked_int().Get().begin(); s!=se; ++s) {
                if (!HighlightInterval(**s)) 
                    okay = false;
            }
        }

        //  No way to tell if HighlightInterval failed due to out-of-range or seqid mismatch
        if (!okay) {
            wxMessageBox("This interaction specifies master residues outside the aligned blocks;"
                         " see the message log for details.", "IBIS Annotation Warning",
                         wxOK | wxCENTRE | wxICON_WARNING, this);
        }
    }

}

void IBISAnnotateDialog::MakeAnnotationFromInteraction(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(interactions, ID_LC_INT, wxListCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_LB_ANNOT, wxListBox)

    //  to start, ignore highlights and just use the location
    //  found in the interaction object
/*
    IntervalList intervals;
    GetCurrentHighlightedIntervals(&intervals);
    if (intervals.size() == 0) {
        ERRORMSG("No aligned+highlighted master residues!");
        return;
    }
    */

    long itemId = GetFirstSelectedListCtrlItemId(*interactions);
    if (itemId == -1) return;

    // get description from user
    CRef< IBISInteraction >& selectedInt = vecIbisInteractions[interactions->GetItemData(itemId)];
    wxString defText = selectedInt->GetDesc().c_str(); //interactions->GetItemText(itemId);
    wxString descr = wxGetTextFromUser(
        "Enter a description for the new annotation:", "Description", defText);
    if (descr.size() == 0) return;

    // create a new annotation
    CRef< CAlign_annot > annot(new CAlign_annot());
    annot = selectedInt->ToAlignAnnot();
    annot->SetDescription(WX_TO_STD(descr));

    // fill out location ONLY IF USING THE Cn3D HIGHLIGHTS
    /*  
    if (intervals.size() == 1) {
        annot->SetLocation().SetInt(*(intervals.front()));
    } else {
        CPacked_seqint *packed = new CPacked_seqint();
        packed->Set() = intervals;  // copy list
        annot->SetLocation().SetPacked_int(*packed);
    }
    */

    // add to annotation list
    annotSet->Set().push_back(annot);
    structureSet->SetDataChanged(StructureSet::eUserAnnotationData);

    // update GUI
    SetupGUIControls(itemId, annotSet->Get().size() - 1, eRemakeListBox);
}

void IBISAnnotateDialog::DeleteAnnotation(void)
{
    // get selection
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_LB_ANNOT, wxListBox)
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;

    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERRORMSG("IBISAnnotateDialog::DeleteAnnotation() - error getting annotation pointer");
        return;
    }

    // confirm with user
    int confirm = wxMessageBox("This will remove the selected (putatively IBIS-derived) annotation\n"
        "and all evidence associated with it.\nIs this correct?", "Confirm", wxOK | wxCANCEL | wxCENTRE, this);
    if (confirm != wxOK) return;

    // actually delete the annotation
    CAlign_annot_set::Tdata::iterator a, ae = annotSet->Set().end();
    for (a=annotSet->Set().begin(); a!=ae; ++a) {
        if (*a == selectedAnnot) {
            annotSet->Set().erase(a);
            structureSet->SetDataChanged(StructureSet::eUserAnnotationData);
            break;
        }
    }

    // update GUI
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(interactions, ID_LC_INT, wxListCtrl)
    int selectedItem = (int) GetFirstSelectedListCtrlItemId(*interactions);

    SetupGUIControls(selectedItem, -1, eRemakeListBox);
}


bool IBISAnnotateDialog::HighlightInterval(const ncbi::objects::CSeq_interval& interval)
{
    const BlockMultipleAlignment *alignment = structureSet->alignmentManager->GetCurrentMultipleAlignment();
//    return alignment->HighlightAlignedColumnsOfMasterRange(interval.GetFrom(), interval.GetTo());

    // make sure annotation sequence matches master sequence
    bool highlightResult;
    bool idMatchResult = true;
    const Sequence *master = alignment->GetMaster();
    if (!master->identifier->MatchesSeqId(interval.GetId())) {
        WARNINGMSG("interval Seq-id " << interval.GetId().GetSeqIdString() << " does not match master Seq-id");
        idMatchResult = false;
    }

    highlightResult = alignment->HighlightAlignedColumnsOfMasterRange(interval.GetFrom(), interval.GetTo());
    return (idMatchResult && highlightResult);
}

void IBISAnnotateDialog::HighlightAnnotation(int eventId)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(interactions, ID_LC_INT, wxListCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_LB_ANNOT, wxListBox)

    // get selection
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;
    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERRORMSG("IBISAnnotateDialog::HighlightAnnotation() - error getting annotation pointer");
        return;
    }

    bool okay = true;
    if (eventId == ID_B_HIGHLIGHT_ANNOT) {
        // highlight annotation's complete set of intervals
        GlobalMessenger()->RemoveAllHighlights(true);
        if (selectedAnnot->GetLocation().IsInt()) {
            okay = HighlightInterval(selectedAnnot->GetLocation().GetInt());
        } else if (selectedAnnot->GetLocation().IsPacked_int()) {
            CPacked_seqint::Tdata::iterator s,
                se = selectedAnnot->SetLocation().SetPacked_int().Set().end();
            for (s=selectedAnnot->SetLocation().SetPacked_int().Set().begin(); s!=se; ++s) {
                if (!HighlightInterval(**s))
                    okay = false;
            }
        }
    } else if (eventId >= ID_B_HIGHLIGHT_OLAP && eventId <= ID_B_HIGHLIGHT_NONOLAP_INTN) {

        // Highlight that subset of the annotation which overlaps the selected interaction
        // Even if there's no selected item, or it isn't mapped properly to
        // a known interaction, clear existing highlighting to avoid confusion.
        long selectedItem = GetFirstSelectedListCtrlItemId(*interactions);
        int index = interactions->GetItemData(selectedItem);
        GlobalMessenger()->RemoveAllHighlights(true);
        if (selectedItem < 0) {
            return;
        } else if (index < 0 || index >= (int) vecIbisInteractions.size()) {
            ERRORMSG("IBISAnnotateDialog::HighlightAnnotation() - error getting IBIS interaction " << index << " for overlap calculation");
            return;
        }

        SeqPosSet overlaps;
        IntervalList intervals;
        IntervalList::iterator intIt, intEnd;
        if (eventId == ID_B_HIGHLIGHT_OLAP) {
            GetAnnotIbisOverlaps(*vecIbisInteractions[index], *selectedAnnot, overlaps);
        } else {
            GetAnnotIbisNonOverlaps(*vecIbisInteractions[index], *selectedAnnot, overlaps, (eventId == ID_B_HIGHLIGHT_NONOLAP_ANNOT));
        }
        GetIntervalsForSet(overlaps, intervals);
        intEnd = intervals.end();
        for (intIt = intervals.begin(); intIt != intEnd; ++intIt) {
            if (!HighlightInterval(**intIt))
                okay = false;
        }
    } else {
        ERRORMSG("IBISAnnotateDialog::HighlightAnnotation() - invalid event id " << eventId);
    }

    if (!okay)
        wxMessageBox("WARNING: this annotation specifies master residues outside the aligned blocks;"
            " see the message log for details.", "Annotation Error",
            wxOK | wxCENTRE | wxICON_ERROR, this);
}

END_SCOPE(Cn3D)


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ code from cdd_ibis_annot_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *ibisIntStaticBoxSizer;
wxSizer *SetupIbisAnnotationDialog( wxWindow *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxFlexGridSizer *item1 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item1->AddGrowableCol( 1 );

    wxStaticBox *item3 = new wxStaticBox( parent, -1, wxT("IBIS Interactions") );
    wxStaticBoxSizer *item2 = new wxStaticBoxSizer( item3, wxVERTICAL );
    ibisIntStaticBoxSizer = item2;

    wxFlexGridSizer *item4 = new wxFlexGridSizer( 1, 0, 0, 0 );

    wxStaticText *item5 = new wxStaticText( parent, ID_TEXT_INT, wxT("Interaction Type Filter"), wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item5, 0, wxALIGN_CENTER|wxALL, 5 );

    wxString strs6[] = 
    {
        wxT("All"), 
        wxT("Protein-Protein"), 
        wxT("Protein-NucAcid"), 
        wxT("Protein-Chemical"), 
        wxT("Protein-Peptide"), 
        wxT("Protein-Ion")
    };
    wxChoice *item6 = new wxChoice( parent, ID_C_TYPE, wxDefaultPosition, wxSize(100,-1), 6, strs6, 0 );
    item4->Add( item6, 0, wxALIGN_CENTER|wxALL, 5 );

    item2->Add( item4, 0, wxALIGN_CENTER|wxALL, 5 );

    wxListCtrl *item7 = new wxListCtrl( parent, ID_LC_INT, wxDefaultPosition, wxSize(450,150), wxLC_REPORT|wxLC_SINGLE_SEL|wxSUNKEN_BORDER );
    item2->Add( item7, 0, wxALIGN_CENTER|wxALL, 5 );

    wxFlexGridSizer *item8 = new wxFlexGridSizer( 3, 0, 0 );

    wxButton *item9 = new wxButton( parent, ID_B_ADD_INT, wxT("Make IBIS Annotation"), wxDefaultPosition, wxDefaultSize, 0 );
    item8->Add( item9, 0, wxALIGN_CENTER|wxALL, 5 );

    wxButton *item10 = new wxButton( parent, ID_B_LAUNCH_INT, wxT("Launch"), wxDefaultPosition, wxDefaultSize, 0 );
    item8->Add( item10, 0, wxALIGN_CENTER|wxALL, 5 );

    wxButton *item11 = new wxButton( parent, ID_B_HIGHLIGHT_INT, wxT("Highlight"), wxDefaultPosition, wxDefaultSize, 0 );
    item8->Add( item11, 0, wxALIGN_CENTER|wxALL, 5 );

    item2->Add( item8, 0, wxALIGN_CENTER|wxALL, 5 );

    item1->Add( item2, 0, wxALIGN_CENTER|wxALL, 5 );

    wxStaticBox *item13 = new wxStaticBox( parent, -1, wxT("Overlapping Annotations") );
    wxStaticBoxSizer *item12 = new wxStaticBoxSizer( item13, wxVERTICAL );

    item12->Add( 20, 10, 0, wxALIGN_CENTER|wxALL, 5 );

    wxString strs14[] = 
    {
        wxT("ListItem")
    };
    wxListBox *item14 = new wxListBox( parent, ID_LB_ANNOT, wxDefaultPosition, wxSize(275,100), 1, strs14, wxLB_SINGLE );
    item12->Add( item14, 0, wxALIGN_CENTER|wxALL, 5 );

    wxFlexGridSizer *item15 = new wxFlexGridSizer( 3, 0, 0 );

    wxButton *item16 = new wxButton( parent, ID_B_DELETE_ANNOT, 
        wxT("Delete \n")
        wxT("IBIS Annotation"),
        wxDefaultPosition, wxDefaultSize, 0 );
    item15->Add( item16, 0, wxALIGN_CENTER|wxALL, 5 );

    wxButton *item17 = new wxButton( parent, ID_B_HIGHLIGHT_ANNOT, 
        wxT("Highlight\n")
        wxT("Annotation"),
        wxDefaultPosition, wxDefaultSize, 0 );
    item15->Add( item17, 0, wxALIGN_CENTER|wxALL, 5 );

    wxButton *item18 = new wxButton( parent, ID_B_HIGHLIGHT_OLAP, 
        wxT("Highlight\n")
        wxT("Overlap"),
        wxDefaultPosition, wxDefaultSize, 0 );
    item15->Add( item18, 0, wxALIGN_CENTER|wxALL, 5 );

    item12->Add( item15, 0, wxALIGN_CENTER|wxALL, 5 );

    wxFlexGridSizer *item19 = new wxFlexGridSizer( 2, 0, 0 );

    wxButton *item20 = new wxButton( parent, ID_B_HIGHLIGHT_NONOLAP_ANNOT, 
        wxT("Highlight Non-Overlap\n")
        wxT("of Annotation"),
        wxDefaultPosition, wxDefaultSize, 0 );
    item19->Add( item20, 0, wxALIGN_CENTER|wxALL, 5 );

    wxButton *item21 = new wxButton( parent, ID_B_HIGHLIGHT_NONOLAP_INTN, 
        wxT("Highlight Non-Overlap\n")
        wxT("of Interaction"),
        wxDefaultPosition, wxDefaultSize, 0 );
    item19->Add( item21, 0, wxALIGN_CENTER|wxALL, 5 );

    item12->Add( item19, 0, wxALIGN_CENTER|wxALL, 5 );

    item1->Add( item12, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item0->Add( item1, 0, wxALIGN_CENTER|wxALL, 5 );

    if (set_sizer)
    {
        parent->SetSizer( item0 );
        if (call_fit)
            item0->SetSizeHints( parent );
    }
    
    return item0;
}
