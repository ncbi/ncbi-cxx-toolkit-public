#ifndef _ERROR_HANDLER_HPP_
#define _ERROR_HANDLER_HPP_

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
* File Name:  $Id$
*
* Author:  Michael Kholodov
*
* File Description:  DataSource implementation
*
*/

#include <dbapi/dbapi.hpp>

BEGIN_NCBI_SCOPE

class CToMultiExHandler : public CDB_UserHandler
{
public:
    CToMultiExHandler();
    virtual ~CToMultiExHandler();

    // Return TRUE (i.e. always process the "ex").
    virtual bool HandleIt(CDB_Exception* ex);

    CDB_MultiEx* GetMultiEx() {
        return m_ex.get();
    }

    void ReplaceMultiEx() {
        m_ex.reset( new CDB_MultiEx(DIAG_COMPILE_INFO, 0) );
    }

private:
    auto_ptr<CDB_MultiEx> m_ex;
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/04/04 13:03:56  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.2  2004/11/01 22:58:01  kholodov
 * Modified: CDBMultiEx object is replace instead of whole handler
 *
 * Revision 1.1  2002/11/25 15:18:30  kholodov
 * First commit
 * ===========================================================================
 */

#endif // _ARRAY_HPP_
