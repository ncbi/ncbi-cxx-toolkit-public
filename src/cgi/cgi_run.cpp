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
 * Author: Eugene Vasilchenko, Denis Vakatov
 *
 * File Description:
 *   This is a stub for the Fast-CGI loop function
 *   used in "cgiapp.cpp"::CCgiApplication::Run().
 *   NOTE:  see also the real Fast-CGI loop function in "fcgi_run.cpp".
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.5  2004/05/17 20:56:50  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.4  2003/03/24 16:14:31  ucko
 * +IsFastCGI (always false)
 *
 * Revision 1.3  2003/01/23 19:59:02  kuznets
 * CGI logging improvements
 *
 * Revision 1.2  2001/06/13 21:04:36  vakatov
 * Formal improvements and general beautifications of the CGI lib sources.
 *
 * Revision 1.1  1999/12/17 03:55:03  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <cgi/cgiapp.hpp>

BEGIN_NCBI_SCOPE

bool CCgiApplication::IsFastCGI(void) const
{
    return false;
}

bool CCgiApplication::x_RunFastCGI(int* /*result*/, unsigned /*def_iter*/)
{
    return false;
}

END_NCBI_SCOPE
