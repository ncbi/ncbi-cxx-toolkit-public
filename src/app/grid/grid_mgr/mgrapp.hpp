#ifndef NCBI_GRID_MGR_APP__HPP
#define NCBI_GRID_MGR_APP__HPP

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
 * Author:  Maxim Didenko
 *
 *     
 */

#include <cgi/cgiapp.hpp>
#include <html/commentdiag.hpp>

#define GRIDMGR_VERSION_MAJOR 1
#define GRIDMGR_VERSION_MINOR 0
#define GRIDMGR_VERSION_PATCH 0

BEGIN_NCBI_SCOPE

//
// class CGridMgrApp
//

class CGridMgrApp : public CCgiApplication
{
public:
    CGridMgrApp() {
        SetVersion(CVersionInfo(
            GRIDMGR_VERSION_MAJOR,
            GRIDMGR_VERSION_MINOR,
            GRIDMGR_VERSION_PATCH));
    }

    virtual void Init(void) {

        CCgiApplication::Init();
        //   HTML body (as comments) -- using CGI arg "&diag-destination=comments"
        RegisterDiagFactory("comments", new CCommentDiagFactory);

    }

    virtual CNcbiResource* LoadResource(void);

    // handle request
    virtual int ProcessRequest(CCgiContext& context);

};

END_NCBI_SCOPE


#endif /* NCBI_GRID_MGR_APP__HPP */


