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
 * Author:  Denis Vakatov, Aleksey Grichenko
 *
 * File Description:
 *   Multi-threading -- fast mutexes
 *
 *   MUTEX:
 *      CInternalMutex   -- platform-dependent mutex functionality
 *
 */

#include <corelib/ncbimtx.hpp>
#include "ncbidbg_p.hpp"

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CInternalMutex::
//

CInternalMutex::CInternalMutex(void)
{
    // Create platform-dependent mutex handle
#if defined(NCBI_WIN32_THREADS)
    xncbi_Validate((m_Handle = CreateMutex(NULL, FALSE, NULL)) != NULL,
                   "CInternalMutex::CInternalMutex() -- error creating mutex");
#elif defined(NCBI_POSIX_THREADS)
    xncbi_Validate(pthread_mutex_init(&m_Handle, 0) == 0,
                   "CInternalMutex::CInternalMutex() -- error creating mutex");
#endif
    m_Initialized = true;
    return;
}


CInternalMutex::~CInternalMutex(void)
{
    // Destroy system mutex handle
#if defined(NCBI_WIN32_THREADS)
    xncbi_Verify(CloseHandle(m_Handle) != 0);
#elif defined(NCBI_POSIX_THREADS)
    xncbi_Verify(pthread_mutex_destroy(&m_Handle) == 0);
#endif
    m_Initialized = false;
    return;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2002/04/11 21:08:02  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.2  2001/12/13 19:45:36  gouriano
 * added xxValidateAction functions
 *
 * Revision 1.1  2001/03/26 20:31:13  vakatov
 * Initial revision (moved code from "ncbithr.cpp")
 *
 * ===========================================================================
 */
