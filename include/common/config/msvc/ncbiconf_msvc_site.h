#ifndef CORELIB_CONFIG___NCBICONF_MSVC_SITE__H
#define CORELIB_CONFIG___NCBICONF_MSVC_SITE__H

/* $Id$
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
* Author:  Andrei Gourianov
*
* File Description:
*   Include ncbiconf_msvc_site.$(ConfigurationName).h
*
*/

#if defined(_DEBUG)
#  if defined(_MT)
#    if defined(_DLL)
#      include "ncbiconf_msvc_site.DebugDLL.h"
#    else
#      include "ncbiconf_msvc_site.DebugMT.h"
#    endif
#  else
#      include "ncbiconf_msvc_site.Debug.h"
#  endif
#else
#  if defined(_MT)
#    if defined(_DLL)
#      include "ncbiconf_msvc_site.ReleaseDLL.h"
#    else
#      include "ncbiconf_msvc_site.ReleaseMT.h"
#    endif
#  else
#      include "ncbiconf_msvc_site.Release.h"
#  endif
#endif

#endif // CORELIB_CONFIG___NCBICONF_MSVC_SITE__H
