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
*      dialog for user annotations
*
* ===========================================================================
*/

#ifndef CN3D_ANNOTATE_DIALOG__HPP
#define CN3D_ANNOTATE_DIALOG__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>

#include <map>
#include <vector>

#include "style_manager.hpp"


BEGIN_SCOPE(Cn3D)

class StructureSet;
class MoleculeIdentifier;

class AnnotateDialog : public wxDialog
{
public:
    AnnotateDialog(wxWindow *parent, StyleManager *manager, const StructureSet *set);

private:

    StyleManager *styleManager;
    const StructureSet *structureSet;

    typedef std::map < const MoleculeIdentifier * , std::vector < bool > > ResidueMap;
    ResidueMap highlightedResidues;
    bool HighlightsPresent(void) const { return (highlightedResidues.size() > 0); }

    // event callbacks
    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnSelection(wxCommandEvent& event);

    // GUI functions
    void ResetListBoxes(void);
    void SetButtonStates(void);

    // action functions
    void NewAnnotation(void);
    void EditAnnotation(void);
    void MoveAnnotation(void);
    void DeleteAnnotation(void);

    DECLARE_EVENT_TABLE()
};

class AnnotationEditorDialog : public wxDialog
{
public:
    AnnotationEditorDialog(wxWindow *parent,
        StyleSettings *settings, const StructureSet *set,
        const StyleManager::UserAnnotation& initialText);

private:

    StyleSettings *styleSettings;
    const StructureSet *structureSet;

    // event callbacks
    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_ANNOTATE_DIALOG__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2004/02/19 17:04:41  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.5  2003/02/03 19:20:00  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.4  2002/08/15 22:13:12  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.3  2001/08/06 20:22:48  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.2  2001/07/04 19:38:55  thiessen
* finish user annotation system
*
* Revision 1.1  2001/06/29 18:12:52  thiessen
* initial (incomplete) user annotation system
*
*/
