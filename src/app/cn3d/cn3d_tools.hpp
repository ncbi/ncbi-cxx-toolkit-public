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
*      Miscellaneous utility functions
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/05/15 14:57:48  thiessen
* add cn3d_tools; bring up log window when threading starts
*
* ===========================================================================
*/

#ifndef CN3D_TOOLS__HPP
#define CN3D_TOOLS__HPP

#include <corelib/ncbistl.hpp>

#include <string>

class wxFrame;


BEGIN_SCOPE(Cn3D)

// strings for various directories (implemented in cn3d_main_wxwin.cpp)
const std::string& GetWorkingDir(void); // current working directory
const std::string& GetUserDir(void);    // directory of latest user-selected file
const std::string& GetProgramDir(void); // directory where Cn3D executable lives
const std::string& GetDataDir(void);    // 'data' directory with external data files

// bring the log window forward (implemented in cn3d_main_wxwin.cpp)
extern void RaiseLogWindow(void);

// top-level window (the main structure window) (implemented in cn3d_main_wxwin.cpp)
extern wxFrame * GlobalTopWindow(void);

END_SCOPE(Cn3D)

#endif // CN3D_TOOLS__HPP
