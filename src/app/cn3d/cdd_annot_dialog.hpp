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
* Revision 1.7  2001/10/14 09:27:27  thiessen
* add cdd evidence move up/down
*
* Revision 1.6  2001/10/01 16:03:58  thiessen
* make CDD annotation window non-modal; add SetWindowTitle to viewers
*
* Revision 1.5  2001/09/26 15:27:58  thiessen
* tweak sequence viewer widget for wx2.3.2, tweak cdd annotation
*
* Revision 1.4  2001/08/27 00:06:35  thiessen
* add structure evidence to CDD annotation
*
* Revision 1.3  2001/08/06 20:22:48  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.2  2001/07/19 19:12:46  thiessen
* working CDD alignment annotator ; misc tweaks
*
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
#include <corelib/ncbiobj.hpp>

#if defined(__WXMSW__)
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
    void HighlightAnnotation(void);
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

END_SCOPE(Cn3D)

#endif // CN3D_CDD_ANNOT_DIALOG__HPP
