#ifndef _CURSOR_IMPL_HPP_
#define _CURSOR_IMPL_HPP_

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
* File Description:  Array template class
*
*
* $Log$
* Revision 1.8  2004/04/22 14:22:25  kholodov
* Added: Cancel()
*
* Revision 1.7  2004/04/08 15:56:58  kholodov
* Multiple bug fixes and optimizations
*
* Revision 1.6  2002/10/21 20:38:08  kholodov
* Added: GetParentConn() method to get the parent connection from IStatement,
* ICallableStatement and ICursor objects.
* Fixed: Minor fixes
*
* Revision 1.5  2002/09/18 18:49:27  kholodov
* Modified: class declaration and Action method to reflect
* direct inheritance of CActiveObject from IEventListener
*
* Revision 1.4  2002/08/26 15:35:56  kholodov
* Added possibility to disable transaction log
* while updating BLOBs
*
* Revision 1.3  2002/07/08 16:06:37  kholodov
* Added GetBlobOStream() implementation
*
* Revision 1.2  2002/02/08 21:29:55  kholodov
* SetDataBase() restored, connection cloning algorithm changed
*
* Revision 1.1  2002/01/30 14:51:22  kholodov
* User DBAPI implementation, first commit
*
*
*
*/

#include <dbapi/dbapi.hpp>
#include "active_obj.hpp"
#include <corelib/ncbistre.hpp>
#include "dbexception.hpp"
#include "blobstream.hpp"
#include <map>

BEGIN_NCBI_SCOPE

class CCursor : public CActiveObject, 
                public ICursor
{
public:
    CCursor(const string& name,
            const string& sql,
            int nofArgs,
            int batchSize,
            CConnection* conn);

    virtual ~CCursor();

    virtual void SetParam(const CVariant& v, 
                          const string& name);

    virtual IResultSet* Open();
    ostream& GetBlobOStream(unsigned int col,
                            size_t blob_size, 
                            EAllowLog log_it,
                            size_t buf_size);
    virtual void Update(const string& table, const string& updateSql);
    virtual void Delete(const string& table);
    virtual void Cancel();
    virtual void Close();

    virtual IConnection* GetParentConn();

    // Interface IEventListener implementation
    virtual void Action(const CDbapiEvent& e);

protected:

    CDB_CursorCmd* GetCursorCmd() { return m_cmd; }

    void FreeResources();

private:
    int m_nofArgs;
    typedef map<string, CVariant*> Parameters;
    Parameters m_params;
    CDB_CursorCmd* m_cmd;
    CConnection* m_conn;
    ostream *m_ostr;
  
};

//====================================================================
END_NCBI_SCOPE

#endif // _CURSOR_IMPL_HPP_
