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
*      dialogs for editing program preferences
*
* ===========================================================================
*/

#ifndef CN3D_PREFERENCES_DIALOG__HPP
#define CN3D_PREFERENCES_DIALOG__HPP

#include <corelib/ncbistd.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>


BEGIN_SCOPE(Cn3D)

class IntegerSpinCtrl;
class FloatingPointSpinCtrl;

class PreferencesDialog : public wxDialog
{
public:
    PreferencesDialog(wxWindow *parent);
    ~PreferencesDialog(void);

private:
    // event callbacks
    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnCheckbox(wxCommandEvent& event);

    // utility functions

    // GUI elements
    IntegerSpinCtrl
        *iWormSegments, *iWormSides, *iBondSides, *iHelixSides, *iAtomSlices, *iAtomStacks,
        *iCacheSize, *iMaxStructs, *iFootRes;
    FloatingPointSpinCtrl *fSeparation;

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_PREFERENCES_DIALOG__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2003/11/15 16:08:36  thiessen
* add stereo
*
* Revision 1.6  2003/02/03 19:20:04  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.5  2003/01/31 17:18:58  thiessen
* many small additions and changes...
*
* Revision 1.4  2002/08/15 22:13:15  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.3  2002/04/27 16:32:13  thiessen
* fix small leaks/bugs found by BoundsChecker
*
* Revision 1.2  2001/10/30 02:54:13  thiessen
* add Biostruc cache
*
* Revision 1.1  2001/08/06 20:22:48  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
*/
