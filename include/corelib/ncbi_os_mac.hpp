#ifndef NCBI_OS_MAC__HPP
#define NCBI_OS_MAC__HPP

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

#include <corelib/ncbistd.hpp>

#if !defined(NCBI_OS_MAC)
#  error "ncbi_os_mac.hpp must be used on MAC platforms only"
#endif


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//   COSErrException_Mac
//

class COSErrException_Mac : public runtime_error
{
    OSErr m_OSErr;
    
public:
    // Report description of "os_err" (along with "what" if specified)
    COSErrException_Mac(const OSErr&  os_err,
                        const string& what = kEmptyStr) THROWS_NONE;
    ~COSErrException_Mac(void) THROWS_NONE;
    const OSErr& GetOSErr(void) const THROWS_NONE { return m_OSErr; }
};


END_NCBI_SCOPE



/* --------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2001/11/19 18:06:58  juran
 * Mac OS-specific header.
 * Initial check-in.
 *
 * ==========================================================================
 */

#endif  /* NCBI_OS_MAC__HPP */
