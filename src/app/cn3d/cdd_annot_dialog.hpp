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
* Revision 1.1  2001/07/12 17:34:22  thiessen
* change domain mapping ; add preliminary cdd annotation GUI
*
* ===========================================================================
*/

#ifndef CN3D_CDD_ANNOT_DIALOG__HPP
#define CN3D_CDD_ANNOT_DIALOG__HPP

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#if defined(__WXMSW__)
#include <wx/msw/winundef.h>
#endif

#include <wx/wx.h>

#include <map>
#include <vector>

#include <objects/cdd/Align_annot_set.hpp>
#include <objects/cdd/Feature_evidence.hpp>


BEGIN_SCOPE(Cn3D)

class StructureSet;

class CDDAnnotateDialog : public wxDialog
{
public:
    CDDAnnotateDialog(wxWindow *parent, StructureSet *set);

private:

    StructureSet *structureSet;

    // edit a copy of the data; flag changes
    ncbi::CRef < ncbi::objects::CAlign_annot_set > annotSet;
    bool changed;

    // event callbacks
    void OnCloseWindow(wxCommandEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnSelection(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

class CDDEvidenceDialog : public wxDialog
{
public:
    CDDEvidenceDialog(wxWindow *parent);

private:
    bool changed;

    // event callbacks
    void OnCloseWindow(wxCommandEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnSelection(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()

public:
    bool DataChanged(void) const { return changed; }
    bool GetData(ncbi::objects::CFeature_evidence *evidence) const;
};

END_SCOPE(Cn3D)

#endif // CN3D_CDD_ANNOT_DIALOG__HPP
