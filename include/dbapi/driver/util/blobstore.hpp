#ifndef DBAPI_UTILS___BLOBSTORE__HPP
#define DBAPI_UTILS___BLOBSTORE__HPP

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
 * Author:  Vladimir Soussov
 *
 * File Description:  Blob store classes
 *
 */

#include <util/reader_writer.hpp>
#include <dbapi/driver/public.hpp>


BEGIN_NCBI_SCOPE

enum ECompressMethod {
    eNone,
    eZLib,
    eBZLib
};

class CBlobReader : public IReader {
 public:
    CBlobReader(CDB_Result* res) {
        m_Res= res;
        m_ItemNum= 0;
        m_AllDone= false;
    }
    /// Read as many as count bytes into a buffer pointed
    /// to by buf argument.  Store the number of bytes actually read,
    /// or 0 on EOF or error, via the pointer "bytes_read", if provided.
    /// Special case:  if count passed as 0, then the value of
    /// buf is ignored, and the return value is always eRW_Success, but
    /// no change is actually done to the state of input device.
    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0);
    
    /// Return the number of bytes ready to be read from input
    /// device without blocking.  Return 0 if no such number is
    /// available (in case of an error or EOF).
    virtual ERW_Result PendingCount(size_t* count);

    virtual ~CBlobReader() {}
 private:
    CBlobReader();
    CDB_Result* m_Res;
    int m_ItemNum;
    bool m_AllDone;
};

class ItDescriptorMaker {
 public:
    virtual bool Init(CDB_Connection* con)= 0;
    virtual I_ITDescriptor& ItDescriptor(void)= 0;
    virtual bool Fini(void)= 0;
};

class CBlobWriter : public IWriter
{
public:
    CBlobWriter(CDB_Connection* con, 
                ItDescriptorMaker*    desc_func, 
                size_t          image_limit= 0x7FFFFFFF, 
                bool            log_it = false);
    /// Write up to count bytes from the buffer pointed to by
    /// buf argument onto output device.  Store the number
    /// of bytes actually written, or 0 if either count was
    /// passed as 0 (buf is ignored in this case) or an error occured,
    /// via the "bytes_written" pointer, if provided.
    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    /// Flush pending data (if any) down to output device.
    virtual ERW_Result Flush(void);

    virtual ~CBlobWriter();
 private:
    CBlobWriter();
    bool storeBlob(void);
    CDB_Image m_Blob;
    ItDescriptorMaker *m_dMaker;
    size_t m_Limit;
    CDB_Connection* m_Con;
    bool m_LogIt;
};

class CBlobRetriever {
 public:
    CBlobRetriever(I_DriverContext* pCntxt,
                   const string& server, 
                   const string& user,
                   const string& passwd,
                   const string& query);
    bool IsReady() const {
        return m_IsGood;
    }
    bool Dump(ostream& s, ECompressMethod cm= eNone);
    ~CBlobRetriever();
 private:
    CBlobRetriever();
    CDB_Connection* m_Conn;
    CDB_LangCmd* m_Cmd;
    CDB_Result* m_Res;
    bool m_IsGood;
};

class CBlobLoader {
 public:
    CBlobLoader(I_DriverContext* pCntxt,
                const string& server, 
                const string& user,
                const string& passwd,
                ItDescriptorMaker* d_maker
                );
    bool IsReady() const {
        return m_IsGood;
    }
    bool Load(istream& s, ECompressMethod cm= eNone, 
              size_t image_limit= 0, bool log_it= false);
    ~CBlobLoader() {
        if(m_Conn) delete m_Conn;
    }
 private:
    CBlobLoader();
    CDB_Connection* m_Conn;
    ItDescriptorMaker* m_dMaker;
    bool m_IsGood;
};

/***************************************************************************************
 * The SimpleBlobStore is a ready to use implementation of ItDescriptorMaker
 * it uses a table of the form:
 * create table TABLE_NAME (
 * ID varchar(n),
 * NUM int,
 * DATA1 image NULL, ... DATAn image NULL)
 *
 */
class CMY_ITDescriptor : public CDB_ITDescriptor {
 public:
    CMY_ITDescriptor(const string& table_name) : 
        CDB_ITDescriptor(table_name, kEmptyStr, kEmptyStr)
    {};
    void SetColumn(const string& col_name) {
        m_ColumnName= col_name;
    }
    void SetSearchConditions(const string& s) {
        m_SearchConditions= s;
    }
};


class CSimpleBlobStore : public ItDescriptorMaker {
 public:
    CSimpleBlobStore(const string& table_name,
                     const string& key_col_name,
                     const string& num_col_name,
                     const string blob_column[]);
    void SetKey(const string& key) {
        if(!key.empty())
            m_Key= key;
    }
    virtual bool Init(CDB_Connection* con);
    virtual I_ITDescriptor& ItDescriptor(void);
    virtual bool Fini(void);
    virtual ~CSimpleBlobStore();
 protected:
    string m_TableName;
    string m_KeyColName;
    string m_NumColName;
    string m_sCMD;
    string* m_DataColName;
    CDB_Connection* m_Con;
    CDB_LangCmd*    m_Cmd;
    int m_nofDataCols;
    int m_ImageNum;
    CDB_VarChar m_Key;
    CDB_Int m_RowNum;
    CMY_ITDescriptor m_Desc;
};


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/05/03 16:47:10  soussov
 * initial commit
 *
 *
 * ===========================================================================
 */

#endif  /* DBAPI_UTILS___BLOBSTORE__HPP */
