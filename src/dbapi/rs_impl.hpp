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
#include <corelib/rwstream.hpp>

#include "rw_impl.hpp"
#include "active_obj.hpp"
//#include "blobstream.hpp"

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

    virtual const CVariant& GetVariant(const CDBParamVariant& param);

    virtual void DisableBind(bool b);
    virtual void BindBlobToVariant(bool b);
  
    virtual size_t Read(void* buf, size_t size);
    virtual bool WasNull();
    virtual int GetColumnNo();
    virtual unsigned int GetTotalColumns();

    virtual void Close();
    virtual const IResultSetMetaData* GetMetaData(EOwnership ownership);

    virtual CNcbiIstream& GetBlobIStream(size_t buf_size);

    virtual CNcbiOstream& GetBlobOStream(size_t blob_size, 
                                         TBlobOStreamFlags flags,
                                         size_t buf_size);
    virtual CNcbiOstream& GetBlobOStream(size_t blob_size, 
                                         EAllowLog log_it,
                                         size_t buf_size)
    {
        return IResultSet::GetBlobOStream(blob_size, log_it, buf_size);
    }

    virtual CNcbiOstream& GetBlobOStream(IConnection *conn,
                                         size_t blob_size, 
                                         TBlobOStreamFlags flags,
                                         size_t buf_size);
    virtual CNcbiOstream& GetBlobOStream(IConnection *conn,
                                         size_t blob_size, 
                                         EAllowLog log_it,
                                         size_t buf_size)
    {
        return IResultSet::GetBlobOStream(conn, blob_size, log_it, buf_size);
    }

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
    
	CNcbiOstream& xGetBlobOStream(CDB_Connection *cdb_conn, 
		                          size_t blob_size,
                                  TBlobOStreamFlags flags,
                                  size_t buf_size,
					  			  bool destroy);

    void x_CacheItems(int last_num);

    class CConnection* m_conn;
    CDB_Result *m_rs;
    //CResultSetMetaDataImpl *m_metaData;
    vector<CVariant> m_data;
    CRStream *m_istr;
    CWStream *m_ostr;
    int m_column;
    bool m_bindBlob;
    bool m_disableBind;
    bool m_wasNull;
    CxBlobReader *m_rd;
    int m_totalRows;
    int m_LastVariantNum;

    enum ERowReadType {
        eReadUnknown,
        eReadVariant,
        eReadRaw
    };

    ERowReadType m_RowReadType;
};

//====================================================================

END_NCBI_SCOPE

#endif // _RS_IMPL_HPP_
