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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Mac specifics
 *
 */

#include <corelib/ncbi_os_mac.hpp>

extern bool g_Mac_SpecialEnvironment = false;

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//   COSErrException_Mac
//

static string s_OSErr_ComposeMessage(const OSErr& os_err, const string& what)
{
    string message;
    if ( !what.empty() ) {
        message = what + ": ";
    }

    message += os_err;

    return message;
}


COSErrException_Mac::COSErrException_Mac(const OSErr&  os_err,
                                         const string& what) THROWS_NONE
    : runtime_error(s_OSErr_ComposeMessage(os_err, what)),
      m_OSErr(os_err)
{
    return;
}


COSErrException_Mac::~COSErrException_Mac(void) THROWS_NONE
{
    return;
}


END_NCBI_SCOPE


/* --------------------------------------------------------------------------
 * $Log$
 * Revision 1.2  2001/12/03 22:00:55  juran
 * Add g_Mac_SpecialEnvironment global.
 *
 * Revision 1.1  2001/11/19 18:11:08  juran
 * Implements Mac OS-specific header.
 * Inital check-in.
 *
 * ==========================================================================
 */
