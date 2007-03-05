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
    virtual const I_DriverContext* GetDriverContext() const;


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

#endif // _ARRAY_HPP_
