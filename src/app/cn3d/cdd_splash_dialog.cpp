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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/04/09 14:38:24  thiessen
* add cdd splash screen
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include <objects/cdd/Cdd_descr_set.hpp>
#include <objects/cdd/Cdd_descr.hpp>
#include <objects/cdd/Align_annot_set.hpp>
#include <objects/pub/Pub.hpp>

#include "cn3d/cdd_splash_dialog.hpp"
#include "cn3d/cn3d_main_wxwin.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/cn3d_tools.hpp"


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
        ERR_POST(Error << "Can't find window with id " << id); \
        return; \
    }

BEGIN_EVENT_TABLE(CDDSplashDialog, wxDialog)
    EVT_CLOSE       (       CDDSplashDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    CDDSplashDialog::OnButton)
END_EVENT_TABLE()

CDDSplashDialog::CDDSplashDialog(Cn3DMainFrame *cn3dFrame, StructureSet *structureSet,
    wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos) :
        wxDialog(parent, id, title, pos, wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
        sSet(structureSet), structureWindow(cn3dFrame)
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

    const std::string& cddName = structureSet->GetCDDName();
    if (cddName.size() > 0)
        tName->SetLabel(cddName.c_str());

    const std::string& cddDescr = structureSet->GetCDDDescription();
    if (cddDescr.size() > 0) {
        int i, j;
        for (i=j=0; i<cddDescr.size(); i++, j++) {
            if (j > 60 && cddDescr[i] == ' ') {
                *tDescr << '\n';
                j = 0;
            } else
                *tDescr << cddDescr[i];
        }
    }

    const CCdd_descr_set *cddRefs = structureSet->GetCDDDescrSet();
    CCdd_descr_set::Tdata::const_iterator d, de = cddRefs->Get().end();
    for (d=cddRefs->Get().begin(); d!=de; d++) {
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

// same as hitting done
void CDDSplashDialog::OnCloseWindow(wxCloseEvent& event)
{
    Destroy();
}

void CDDSplashDialog::OnButton(wxCommandEvent& event)
{
    if (event.GetId() == ID_B_ANNOT) {
        structureWindow->Command(Cn3DMainFrame::MID_ANNOT_CDD);
    }

    else if (event.GetId() == ID_B_REF) {
        structureWindow->Command(Cn3DMainFrame::MID_EDIT_CDD_REFERENCES);
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

