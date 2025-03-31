#ifndef CGI___FCGIAPP_ST__HPP
#define CGI___FCGIAPP_ST__HPP

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
* Author: Denis Vakatov
*
* File Description:
*   Base class for Fast-CGI applications
*/

#include <cgi/cgiapp.hpp>

/** @addtogroup CGIBase
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//  CFastCgiApplication::
//

class NCBI_XCGI_EXPORT CFastCgiApplication : public CCgiApplication
{
public:
    CFastCgiApplication(const SBuildInfo& build_info = NCBI_SBUILDINFO_DEFAULT()) : CCgiApplication(build_info) {}


protected:
    bool IsFastCGI(void) const override;
    bool x_RunFastCGI(int* result, unsigned int def_iter = 10) override;
};


END_NCBI_SCOPE


/* @} */


#endif // CGI___FCGIAPP_ST__HPP
