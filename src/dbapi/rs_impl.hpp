#ifndef _RS_IMPL_HPP_
#define _RS_IMPL_HPP_

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
* File Description:  Resultset implementation
*
*
*
*/

#include <dbapi/dbapi.hpp>

#include "rw_impl.hpp"
#include "active_obj.hpp"
//#include "array.hpp"
//#include "dbexception.hpp"
#include "blobstream.hpp"

#include <vector>

BEGIN_NCBI_SCOPE

class CResultSet : public CActiveObject, 
                   public IResultSet
{
public:
    CResultSet(class CConnection* conn, CDB_Result *rs);

    virtual ~CResultSet();
  
    void Init();
    virtual EDB_ResType GetResultType();

    virtual bool Next();

    virtual const CVariant& GetVariant(unsigned int idx);
    virtual const CVariant& GetVariant(const string& colName);

    virtual void DisableBind(bool b);
    virtual void BindBlobToVariant(bool b);
  
    virtual size_t Read(void* buf, size_t size);
    virtual bool WasNull();
    virtual int GetColumnNo();
    virtual unsigned int GetTotalColumns();

    virtual void Close();
    virtual const IResultSetMetaData* GetMetaData();
    virtual istream& GetBlobIStream(size_t buf_size);
    virtual ostream& GetBlobOStream(size_t blob_size, 
                                    EAllowLog log_it,
                                    size_t buf_size);

    virtual IReader* GetBlobReader();

    // Interface IEventListener implementation
    virtual void Action(const CDbapiEvent& e);

    CDB_Result* GetCDB_Result() {
        return m_rs;
    }

    void Invalidate() {
        delete m_rs;
        m_rs = 0;
        m_totalRows = -1;
    }

    int GetTotalRows() {
        return m_totalRows;
    }

protected:
    
    int GetColNum(const string& name);
    void CheckIdx(unsigned int idx);

    bool IsBindBlob() {
        return m_bindBlob;
    }

    bool IsDisableBind() {
        return m_disableBind;
    }

    void FreeResources();

private:
    
    class CConnection* m_conn;
    CDB_Result *m_rs;
    //CResultSetMetaDataImpl *m_metaData;
    vector<CVariant> m_data;
    CBlobIStream *m_istr;
    CBlobOStream *m_ostr;
    int m_column;
    bool m_bindBlob;
    bool m_disableBind;
    bool m_wasNull;
    CBlobReader *m_rd;
    int m_totalRows;

};

//====================================================================

END_NCBI_SCOPE
/*
* $Log$
* Revision 1.17  2004/07/21 18:43:58  kholodov
* Added: separate row counter for resultsets
*
* Revision 1.16  2004/07/20 17:49:17  kholodov
* Added: IReader/IWriter support for BLOB I/O
*
* Revision 1.15  2004/04/12 14:25:33  kholodov
* Modified: resultset caching scheme, fixed single connection handling
*
* Revision 1.14  2004/04/08 15:56:58  kholodov
* Multiple bug fixes and optimizations
*
* Revision 1.13  2003/02/12 15:52:32  kholodov
* Added: WasNull() method
*
* Revision 1.12  2002/12/16 18:56:50  kholodov
* Fixed: memory leak in CStatement object
*
* Revision 1.11  2002/11/25 15:15:50  kholodov
* Removed: dynamic array module (array.hpp, array.cpp), using
* STL vector instead to keep bound column data.
*
* Revision 1.10  2002/10/31 22:37:05  kholodov
* Added: DisableBind(), GetColumnNo(), GetTotalColumns() methods
* Fixed: minor errors, diagnostic messages
*
* Revision 1.9  2002/10/21 20:38:08  kholodov
* Added: GetParentConn() method to get the parent connection from IStatement,
* ICallableStatement and ICursor objects.
* Fixed: Minor fixes
*
* Revision 1.8  2002/09/18 18:49:27  kholodov
* Modified: class declaration and Action method to reflect
* direct inheritance of CActiveObject from IEventListener
*
* Revision 1.7  2002/09/16 19:34:41  kholodov
* Added: bulk insert support
*
* Revision 1.6  2002/08/26 15:35:56  kholodov
* Added possibility to disable transaction log
* while updating BLOBs
*
* Revision 1.5  2002/07/08 16:08:19  kholodov
* Modified: moved initialization code to Init() method
*
* Revision 1.4  2002/06/11 16:22:53  kholodov
* Fixed the incorrect declaration of GetMetaData() method
*
* Revision 1.3  2002/05/14 19:53:17  kholodov
* Modified: Read() returns 0 to signal end of column
*
* Revision 1.2  2002/04/05 19:29:50  kholodov
* Modified: GetBlobIStream() returns one and the same object, which is created
* on the first call.
* Added: GetVariant(const string& colName) to retrieve column value by
* column name
*
* Revision 1.1  2002/01/30 14:51:23  kholodov
* User DBAPI implementation, first commit
*
*
*/
#endif // _RS_IMPL_HPP_
