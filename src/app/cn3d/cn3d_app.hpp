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
*       log and application object for Cn3D
*
* ===========================================================================
*/

#ifndef CN3D_APP__HPP
#define CN3D_APP__HPP

#include <corelib/ncbistd.hpp>

#include <string>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <wx/cmdline.h>


BEGIN_SCOPE(Cn3D)

class StructureWindow;

// Define a new application type
class Cn3DApp: public wxApp /*wxGLApp*/
{
public:
    Cn3DApp();

private:
    bool OnInit(void);
    int OnExit(void);

    // for now, there is only one structure window
    StructureWindow *structureWindow;

#ifdef __WXMAC__
    void MacOpenFile(const wxString& fileName);
#endif

    // use wx command line parser
    wxCmdLineParser commandLine;

    // used for processing display updates when system is idle
    void OnIdle(wxIdleEvent& event);

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_APP__HPP
