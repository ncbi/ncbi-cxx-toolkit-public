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
*       dialog for CDD splash screen
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/cdd/Cdd_descr_set.hpp>
#include <objects/cdd/Cdd_descr.hpp>
#include <objects/cdd/Align_annot_set.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/cdd/Align_annot.hpp>
#include <objects/cdd/Feature_evidence.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/mmdb1/Mmdb_id.hpp>
#include <objects/mmdb3/Biostruc_feature_set.hpp>
#include <objects/mmdb3/Biostruc_feature_set_descr.hpp>
#include <objects/mmdb3/Biostruc_feature.hpp>

#include "cdd_splash_dialog.hpp"
#include "structure_window.hpp"
#include "structure_set.hpp"
#include "cn3d_tools.hpp"
#include "chemical_graph.hpp"
#include "molecule_identifier.hpp"
#include "sequence_set.hpp"


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ header from cdd_splash_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

#include <wx/image.h>
#include <wx/statline.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <wx/notebook.h>
#include <wx/grid.h>

// Declare window functions

#define ID_TEXT 10000
#define ID_ST_NAME 10001
#define ID_T_DESCR 10002
#define ID_B_ANNOT 10003
#define ID_B_REF 10004
#define ID_B_DONE 10005
wxSizer *SetupCDDSplashDialog( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

#define DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERRORMSG("Can't find window with id " << id); \
        return; \
    }

BEGIN_EVENT_TABLE(CDDSplashDialog, wxDialog)
    EVT_CLOSE       (       CDDSplashDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    CDDSplashDialog::OnButton)
END_EVENT_TABLE()

CDDSplashDialog::CDDSplashDialog(StructureWindow *cn3dFrame,
    StructureSet *structureSet, CDDSplashDialog **parentHandle,
    wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos) :
        wxDialog(parent, id, title, pos, wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
        sSet(structureSet), structureWindow(cn3dFrame), handle(parentHandle)
{
    if (!structureSet) {
        Destroy();
        return;
    }

    // construct the panel
    wxPanel *panel = new wxPanel(this, -1);
    wxSizer *topSizer = SetupCDDSplashDialog(panel, false);

    // fill in info
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tName, ID_ST_NAME, wxStaticText)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tDescr, ID_T_DESCR, wxTextCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bAnnot, ID_B_ANNOT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bRef, ID_B_REF, wxButton)

    const string& cddName = structureSet->GetCDDName();
    if (cddName.size() > 0)
        tName->SetLabel(cddName.c_str());

    const string& cddDescr = structureSet->GetCDDDescription();
    if (cddDescr.size() > 0) {
        int i, j;
        for (i=j=0; i<cddDescr.size(); ++i, ++j) {
            if (j > 60 && cddDescr[i] == ' ') {
                *tDescr << '\n';
                j = 0;
            } else
                *tDescr << cddDescr[i];
        }
        *tDescr << "\n\n";
    }

    // summarize annotations
    const CAlign_annot_set *annots = structureSet->GetCDDAnnotSet();
    if (annots && annots->Get().size() > 0) {
        *tDescr << "Annotation summary:\n\n";
        CAlign_annot_set::Tdata::const_iterator a, ae = annots->Get().end();
        for (a=annots->Get().begin(); a!=ae; ++a) {
            *tDescr << ((*a)->IsSetDescription() ? (*a)->GetDescription() : string("")).c_str()
                << "; evidence:\n";
            if ((*a)->IsSetEvidence()) {
                CAlign_annot::TEvidence::const_iterator e, ee = (*a)->GetEvidence().end();
                for (e=(*a)->GetEvidence().begin(); e!=ee; ++e) {
                    if ((*e)->IsComment())
                        *tDescr << "  comment: " << (*e)->GetComment().c_str() << '\n';
                    else if ((*e)->IsReference() && (*e)->GetReference().IsPmid())
                        *tDescr << "  PubMed " << (*e)->GetReference().GetPmid().Get() << '\n';
                    else if ((*e)->IsBsannot()) {
                        *tDescr << "  structure:";
                        if ((*e)->GetBsannot().GetFeatures().size() > 0 &&
                            (*e)->GetBsannot().GetFeatures().front()->IsSetDescr() &&
                            (*e)->GetBsannot().GetFeatures().front()->IsSetDescr() &&
                            (*e)->GetBsannot().GetFeatures().front()->GetDescr().front()->IsName())
                            *tDescr << ' ' <<
                                (*e)->GetBsannot().GetFeatures().front()->GetDescr().front()->GetName().c_str();
                        if ((*e)->GetBsannot().IsSetId() && (*e)->GetBsannot().GetId().front()->IsMmdb_id()) {
                            int mmdbID = (*e)->GetBsannot().GetId().front()->GetMmdb_id().Get();
                            StructureSet::ObjectList::const_iterator o, oe = structureSet->objects.end();
                            for (o=structureSet->objects.begin(); o!=oe; ++o) {
                                if ((*o)->mmdbID == mmdbID) {
                                    *tDescr << " (" << (*o)->pdbID.c_str() << ')';
                                    break;
                                }
                            }
                        }
                        *tDescr << '\n';
                    }
                }
            }
            *tDescr << '\n';
        }
    }

    // summarize structures
    if (structureSet->objects.size() > 0) {
        *tDescr << "Structure summary:\n";
        StructureSet::ObjectList::const_iterator o, oe = structureSet->objects.end();
        for (o=structureSet->objects.begin(); o!=oe; ++o) {
            *tDescr << "\nPDB " << (*o)->pdbID.c_str() << " (MMDB " << (*o)->mmdbID << ")\n";

            // make lists of biopolymer chains and heterogens
            typedef list < string > ChainList;
            ChainList chainList;
            typedef map < string , int > HetList;
            HetList hetList;
            ChemicalGraph::MoleculeMap::const_iterator m, me = (*o)->graph->molecules.end();
            for (m=(*o)->graph->molecules.begin(); m!=me; ++m) {
                if (m->second->IsProtein() || m->second->IsNucleotide()) {
                    wxString descr;
                    SequenceSet::SequenceList::const_iterator
                        s, se = structureSet->sequenceSet->sequences.end();
                    for (s=structureSet->sequenceSet->sequences.begin(); s!=se; ++s) {
                        if ((*s)->identifier == m->second->identifier) {
                            descr.Printf("%s: gi %i (%s)", m->second->identifier->ToString().c_str(),
                                (*s)->identifier->gi, (*s)->description.c_str());
                            break;
                        }
                    }
                    if (s == se)
                        descr.Printf("%s: gi %i", m->second->identifier->ToString().c_str(),
                            m->second->identifier->gi);
                    chainList.push_back(descr.c_str());
                } else if (m->second->IsHeterogen()) {
                    // get name from local graph name of first (should be only) residue
                    const string& name = m->second->residues.find(1)->second->nameGraph;
                    HetList::iterator n = hetList.find(name);
                    if (n == hetList.end())
                        hetList[name] = 1;
                    else
                        ++(n->second);
                }
            }
            chainList.sort();

            // print chains
            ChainList::const_iterator c, ce = chainList.end();
            for (c=chainList.begin(); c!=ce; ++c)
                *tDescr << "    " << c->c_str() << '\n';

            // print hets
            if (hetList.size() > 0) {
                *tDescr << "Heterogens: ";
                HetList::const_iterator h, he = hetList.end();
                for (h=hetList.begin(); h!=he; ++h) {
                    if (h != hetList.begin())
                        *tDescr << ", ";
                    wxString descr;
                    if (h->second > 1)
                        descr.Printf("%s (x%i)", h->first.c_str(), h->second);
                    else
                        descr = h->first.c_str();
                    *tDescr << descr;
                }
                *tDescr << '\n';
            }
        }
    }

    const CCdd_descr_set *cddRefs = structureSet->GetCDDDescrSet();
    CCdd_descr_set::Tdata::const_iterator d, de = cddRefs->Get().end();
    for (d=cddRefs->Get().begin(); d!=de; ++d) {
        if ((*d)->IsReference() && (*d)->GetReference().IsPmid())
            break;
    }
    if (d == de)    // if no PMID references found
        bRef->Enable(false);

    const CAlign_annot_set *cddAnnot = structureSet->GetCDDAnnotSet();
    if (cddAnnot->Get().size() == 0)
        bAnnot->Enable(false);

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->Fit(panel);
    topSizer->SetSizeHints(this);
}

CDDSplashDialog::~CDDSplashDialog(void)
{
    if (handle && *handle) *handle = NULL;
}

// same as hitting done
void CDDSplashDialog::OnCloseWindow(wxCloseEvent& event)
{
    Destroy();
}

void CDDSplashDialog::OnButton(wxCommandEvent& event)
{
    if (event.GetId() == ID_B_ANNOT) {
        structureWindow->ShowCDDAnnotations();
    }

    else if (event.GetId() == ID_B_REF) {
        structureWindow->ShowCDDReferences();
    }

    else if (event.GetId() == ID_B_DONE) {
        Destroy();
    }
}

END_SCOPE(Cn3D)

//////////////////////////////////////////////////////////////////////////////////////////////////
// The following are taken *without* modification from wxDesigner C++ code generated from
// cdd_splash_dialog.wdr.
//////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *SetupCDDSplashDialog( wxWindow *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxFlexGridSizer *item1 = new wxFlexGridSizer( 2, 0, 0 );
    item1->AddGrowableCol( 1 );

    wxStaticText *item2 = new wxStaticText( parent, ID_TEXT, "Name:", wxDefaultPosition, wxDefaultSize, 0 );
    item1->Add( item2, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item3 = new wxStaticText( parent, ID_ST_NAME, "", wxDefaultPosition, wxDefaultSize, 0 );
    item1->Add( item3, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxTextCtrl *item4 = new wxTextCtrl( parent, ID_T_DESCR, "", wxDefaultPosition, wxSize(-1,100), wxTE_MULTILINE|wxTE_READONLY );
    item0->Add( item4, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxBoxSizer *item5 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item6 = new wxButton( parent, ID_B_ANNOT, "Show Annotations Panel", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item6, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item7 = new wxButton( parent, ID_B_REF, "Show References Panel", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item7, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item5->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item8 = new wxButton( parent, ID_B_DONE, "Dismiss", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item8, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item0->Add( item5, 0, wxALIGN_CENTRE|wxALL, 5 );

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


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.11  2004/03/15 18:16:33  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.10  2004/02/19 17:04:45  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.9  2003/03/13 14:26:18  thiessen
* add file_messaging module; split cn3d_main_wxwin into cn3d_app, cn3d_glcanvas, structure_window, cn3d_tools
*
* Revision 1.8  2003/02/03 19:20:02  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.7  2002/12/19 19:15:27  thiessen
* cosmetic fixes
*
* Revision 1.6  2002/12/19 18:52:41  thiessen
* add structure summary
*
* Revision 1.5  2002/12/07 01:38:36  thiessen
* fix header problem
*
* Revision 1.4  2002/09/18 14:12:00  thiessen
* add annotations summary to overview
*
* Revision 1.3  2002/08/15 22:13:13  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.2  2002/04/09 23:59:09  thiessen
* add cdd annotations read-only option
*
* Revision 1.1  2002/04/09 14:38:24  thiessen
* add cdd splash screen
*
*/
