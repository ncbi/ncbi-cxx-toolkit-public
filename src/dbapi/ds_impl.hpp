#ifndef _DS_IMPL_HPP_
#define _DS_IMPL_HPP_

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
*
*
*
*/

#include <dbapi/dbapi.hpp>
#include "active_obj.hpp"
#include "dbexception.hpp"

BEGIN_NCBI_SCOPE


//=================================================================
class CDataSource : public CActiveObject,
                    public IDataSource
{
public:

    enum EAllowParse { eNoParse, eDoParse };

    CDataSource(I_DriverContext *ctx);

protected:
    virtual ~CDataSource();

public:
    virtual void SetLoginTimeout(unsigned int i);
    virtual void SetLogStream(CNcbiOstream* out);

    virtual CDB_MultiEx* GetErrorAsEx();
    virtual string GetErrorInfo();

    int GetLoginTimeout() const {
        return m_loginTimeout;
    }

    virtual I_DriverContext* GetDriverContext();

    void UsePool(bool use) {
        m_poolUsed = use;
    }

    bool IsPoolUsed() {
        return m_poolUsed;
    }

    virtual IConnection* CreateConnection(EOwnership ownership);

    // Implement IEventListener interface
    virtual void Action(const CDbapiEvent& e);

    class CToMultiExHandler* GetHandler();

private:
    int m_loginTimeout;
    I_DriverContext *m_context;
    bool m_poolUsed;
    class CToMultiExHandler *m_multiExH;
};

//====================================================================
END_NCBI_SCOPE
/*
* $Log$
* Revision 1.9  2005/04/04 13:03:56  ssikorsk
* Revamp of DBAPI exception class CDB_Exception
*
* Revision 1.8  2004/07/28 18:36:13  kholodov
* Added: setting ownership for connection objects
*
* Revision 1.7  2003/02/10 17:20:15  kholodov
* Modified: made desctructor non-public
*
* Revision 1.6  2002/11/27 17:14:51  kholodov
* Modified: CToMulitExHandler definition moved to err_handler.hpp.
*
* Revision 1.5  2002/09/30 19:16:27  kholodov
* Added: public GetHandler() method
*
* Revision 1.4  2002/09/23 18:35:24  kholodov
* Added: GetErrorInfo() and GetErrorAsEx() methods.
*
* Revision 1.3  2002/09/18 18:49:27  kholodov
* Modified: class declaration and Action method to reflect
* direct inheritance of CActiveObject from IEventListener
*
* Revision 1.2  2002/04/15 19:11:42  kholodov
* Changed GetContext() -> GetDriverContext
*
* Revision 1.1  2002/01/30 14:51:23  kholodov
* User DBAPI implementation, first commit
*
*/
#endif // _ARRAY_HPP_
