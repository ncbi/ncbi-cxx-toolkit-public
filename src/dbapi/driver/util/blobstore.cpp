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
 */

#include <ncbi_pch.hpp>
#include <corelib/rwstream.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>
#include <util/compress/bzip2.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/util/blobstore.hpp>

USING_NCBI_SCOPE;

////////////////////////////////////////////////////////////////////////////////
bool CBlobWriter::storeBlob(void)
{
    try {
        m_Con->SendData(m_dMaker->BlobDescriptor(), m_Blob, m_LogIt);
        m_Blob.Truncate();
        return true;
    }
    catch (CDB_Exception& e) {
        CDB_UserHandler_Stream myExHandler(&cerr);

        myExHandler.HandleIt(&e);
        return false;
    }
}

CBlobWriter::CBlobWriter(CDB_Connection* con, IBlobDescriptorMaker* d_maker,
                         size_t image_limit, TFlags flags)
{
    m_Con= con;
    m_dMaker= d_maker;
    m_Limit= (image_limit > 1)? image_limit : (16*1024*1024);
    m_LogIt= ((flags & fLogBlobs) != 0);
    m_DelDesc= ((flags & fOwnDescr) != 0);
    m_DelCon=  ((flags & fOwnCon) != 0);
}

ERW_Result CBlobWriter::Write(const void* buf,
                              size_t      count,
                              size_t*     bytes_written)
{
    size_t n= m_Blob.Append(buf, count);
    if(bytes_written) *bytes_written= n;
    if(m_Blob.Size() > m_Limit) {
        // blob is off limit
        if( !storeBlob() ) {
            if (bytes_written != NULL) {
                *bytes_written = 0;
            }
            return eRW_Error;
        }
    }
    return eRW_Success;
}

ERW_Result CBlobWriter::Flush(void)
{
    if(m_Blob.Size() > 0) {
        if(!storeBlob()) return eRW_Error;
    }
    return eRW_Success;
}

CBlobWriter::~CBlobWriter()
{
    Flush();
    if(m_DelDesc) {
        m_dMaker->Fini();
        delete m_dMaker;
    }
    if(m_DelCon) {
        delete m_Con;
    }
}

////////////////////////////////////////////////////////////////////////////////
CBlobReader::CBlobReader(CDB_Result* res,
                         I_BaseCmd* cmd,
                         I_Connection* con)
: m_Res(res)
, m_Cmd(cmd)
, m_Con(con)
, m_ItemNum(0)
, m_AllDone(false)
{
}


CBlobReader::~CBlobReader()
{
    delete m_Res;
    delete m_Cmd;
    delete m_Con;
}


ERW_Result CBlobReader::Read(void* buf, size_t count, size_t* bytes_read)
{
    size_t n= 0;
    size_t rn= 0;
    ERW_Result r= eRW_Success;
    if(m_AllDone) {
        r= eRW_Eof;
    }
    else try {
        bool isNull;
        do {
            n= m_Res->ReadItem(buf, count, &isNull);
            if(n >= count) { // we did all we need
                if(bytes_read) *bytes_read= rn+n;
                return r;
            }
            // need to read the tail from other items
            rn+= n;
            count-= n;
            buf= ((char*)buf)+n;
            n= 0;

            if(m_ItemNum == m_Res->CurrentItemNo()) {
                // we should not be here, but just in case
                if(!m_Res->SkipItem()) m_ItemNum= -1;
                else ++m_ItemNum;
            }
            else m_ItemNum= m_Res->CurrentItemNo();
            if((m_ItemNum < 0 ||
                (unsigned int)m_ItemNum >= m_Res->NofItems()) &&
               m_Res->Fetch()) {
                m_ItemNum= 0; // starting next row
            }
        }
        while(m_ItemNum >= 0 &&
              (unsigned int)m_ItemNum < m_Res->NofItems());

        m_AllDone= true; // nothing to read any more
        r = eRW_Eof;

    } catch (CDB_Exception&) {
        m_AllDone= true;
        r = eRW_Error;
    }

    if(bytes_read) *bytes_read= n + rn;
    return r;
}

ERW_Result CBlobReader::PendingCount(size_t* count)
{
    if(count) *count= 0;
    return m_AllDone? eRW_Eof : eRW_NotImplemented;
}


////////////////////////////////////////////////////////////////////////////////
CBlobRetriever::CBlobRetriever(I_DriverContext* pCntxt,
                               const string& server,
                               const string& user,
                               const string& passwd,
                               const string& query)
{
    m_Conn= 0;
    m_Cmd= 0;
    m_Res= 0;
    try {
        m_Conn= pCntxt->Connect(server, user, passwd, 0, true);
        m_Cmd= m_Conn->LangCmd(query);
        m_Cmd->Send();
        while(m_Cmd->HasMoreResults()) {
            m_Res=m_Cmd->Result();
            if(!m_Res) continue;
            if(m_Res->ResultType() != eDB_RowResult) {
                delete m_Res;
                continue;
            }
            if(m_Res->Fetch()) {
                m_IsGood= true;
                return;
            }
        }
    } catch (CDB_Exception& e) {
        CDB_UserHandler_Stream myExHandler(&cerr);

        myExHandler.HandleIt(&e);
    }
    m_IsGood= false;
}

bool CBlobRetriever::Dump(ostream& s, ECompressMethod cm)
{
    if(m_IsGood) {
        unique_ptr<CBlobReader> bReader(new CBlobReader(m_Res));
        unique_ptr<CRStream> iStream(new CRStream(bReader.get()));
        unique_ptr<CCompressionStreamProcessor> zProc;

        switch(cm) {
        case eZLib:
            zProc.reset(new CCompressionStreamProcessor((CZipDecompressor*)(new CZipDecompressor),
                                                        CCompressionStreamProcessor::eDelete));
            break;
        case eBZLib:
            zProc.reset(new CCompressionStreamProcessor((CBZip2Decompressor*)(new CBZip2Decompressor),
                                                        CCompressionStreamProcessor::eDelete));
            break;
        default:
            zProc.reset();
        }

        if(zProc.get()) {
            unique_ptr<CCompressionIStream> zStream(new CCompressionIStream(*iStream, zProc.get()));
            s << zStream->rdbuf();
        }
        else {
            s << iStream->rdbuf();
        }

        m_IsGood = m_Res->Fetch();

        return true;
    }
    return false;
}

CBlobRetriever::~CBlobRetriever()
{
    delete m_Res;
    delete m_Cmd;
    delete m_Conn;
}

////////////////////////////////////////////////////////////////////////////////
CBlobLoader::CBlobLoader(I_DriverContext* pCntxt,
                         const string&    server,
                         const string&    user,
                         const string&    passwd,
                         IBlobDescriptorMaker* d_maker)
{
    m_Conn= 0;
    try {
        m_Conn= pCntxt->Connect(server, user, passwd, 0, true);
        m_dMaker= d_maker;
        m_IsGood= true;
        return;
    } catch (CDB_Exception& e) {
        CDB_UserHandler_Stream myExHandler(&cerr);

        myExHandler.HandleIt(&e);
    }
    m_IsGood= false;
}

bool CBlobLoader::Load(istream& s, ECompressMethod cm, size_t image_limit, bool log_it)
{
    if(m_IsGood && m_dMaker->Init(m_Conn)) {
        unique_ptr<CBlobWriter> bWriter(new CBlobWriter(m_Conn, m_dMaker, image_limit, log_it));
        unique_ptr<CWStream> oStream(new CWStream(bWriter.get()));
        unique_ptr<CCompressionStreamProcessor> zProc;

        switch(cm) {
        case eZLib:
            zProc.reset(new CCompressionStreamProcessor((CZipCompressor*)(new CZipCompressor),
                                                        CCompressionStreamProcessor::eDelete));
            break;
        case eBZLib:
            zProc.reset(new CCompressionStreamProcessor((CBZip2Compressor*)(new CBZip2Compressor),
                                                        CCompressionStreamProcessor::eDelete));
            break;
        default:
            zProc.reset();
        }

        if(zProc.get()) {
            unique_ptr<CCompressionOStream> zStream(new CCompressionOStream(*oStream, zProc.get()));
            *zStream << s.rdbuf();
        }
        else {
            *oStream << s.rdbuf();
        }

        return m_dMaker->Fini();
    }

    return false;

}


////////////////////////////////////////////////////////////////////////////////
CSimpleBlobStore::CSimpleBlobStore(const string& table_name,
                                   const string& key_col_name,
                                   const string& num_col_name,
                                   const string blob_column[],
                                   bool is_text,
                                   const CTempString& table_hint) :
    CSimpleBlobStore(table_name, key_col_name, num_col_name, blob_column,
                     TFlags(is_text ? fIsText : kDefaults), table_hint)
{
}

CSimpleBlobStore::CSimpleBlobStore(const string& table_name,
                                   const string& key_col_name,
                                   const string& num_col_name,
                                   const string blob_column[],
                                   TFlags flags,
                                   const CTempString& table_hint) :
    m_TableName(table_name), m_KeyColName(key_col_name),
    m_NumColName(num_col_name), m_TableHint(table_hint),
    m_RowNum(0), m_Desc(table_name, kEmptyStr, kEmptyStr), m_Flags(flags)
{
    m_Con= 0;
    m_Cmd= 0;
    for(m_nofDataCols= 0; !blob_column[m_nofDataCols].empty(); m_nofDataCols++);
    if(m_nofDataCols) {
        int i;
        string init_val((flags & fIsText) != 0 ? "' '" : "0x0");
        string data_names, data_values, updates;
        const char* delim = kEmptyCStr;
        m_DataColName= new string[m_nofDataCols];
        for(i= 0; i < m_nofDataCols; i++) {
            m_DataColName[i]= blob_column[i];
            data_names += delim + blob_column[i];
            data_values += delim + init_val;
            updates += ' ' + blob_column[i] + " = " + init_val + ',';
            delim = ", ";
        }
        updates[updates.size() - 1] = '\n';
#if 0
        // Full safety requires the WITH(HOLDLOCK), but Sybase doesn't
        // support locking hints in this context.
        m_sCMD  = "MERGE " + m_TableName + " WITH(HOLDLOCK) AS t\n";
        m_sCMD += "  USING (VALUES (" + data_values + "))\n";
        m_sCMD += "    AS s (" + data_names + ")\n";
        m_sCMD += "  ON t." + m_KeyColName + " = @key\n";
        m_sCMD += "    AND t." + m_NumColName + " = @n\n";
        m_sCMD += "  WHEN MATCHED THEN UPDATE SET" + updates;
        if ((flags & fPreallocated) == 0) {
            m_sCMD += "  WHEN NOT MATCHED THEN\n";
            m_sCMD += "    INSERT (" + m_KeyColName + ", " + m_NumColName;
            m_sCMD += ", " + data_names + ")\n";
            m_sCMD += "    VALUES (@key, @n, " + data_values + ")";
        }
#else
        m_sCMD  = "UPDATE " + m_TableName + " SET " + updates;
        m_sCMD += "  WHERE " + m_KeyColName + " = @key\n";
        m_sCMD += "    AND " + m_NumColName + " = @n\n";
        if ((flags & fPreallocated) == 0) {
            m_sCMD += "IF @@ROWCOUNT = 0\n";
            m_sCMD += "  INSERT INTO " + m_TableName + " (" + m_KeyColName;
            m_sCMD += ", " + m_NumColName + ", " + data_names + ")\n";
            m_sCMD += "    SELECT @key, @n, " + data_values + "\n";
            m_sCMD += "      WHERE NOT EXISTS (SELECT " + m_NumColName;
            m_sCMD += " FROM " + m_TableName + " WHERE ";
            m_sCMD += m_KeyColName + " = @key AND " + m_NumColName + " = @n)";
        }
#endif
    }
    else m_DataColName= 0;
}

CSimpleBlobStore::~CSimpleBlobStore()
{
    delete [] m_DataColName;
    delete m_Cmd;
}

bool CSimpleBlobStore::Init(CDB_Connection* con)
{
    m_Con= con;
    m_ImageNum= 0;
    if(m_Key.IsNULL() || (m_nofDataCols < 1)) return false;

    if ( !m_TableHint.empty()  &&  NStr::StartsWith(m_sCMD, "UPDATE ")) {
        impl::CConnection* conn_impl
            = dynamic_cast<impl::CConnection*>(&con->GetExtraFeatures());
        if (conn_impl != NULL
            && conn_impl->GetServerType() == CDBConnParams::eMSSqlServer) {
            string repl = ' ' + m_TableName + " WITH(" + m_TableHint + ") ";
            NStr::ReplaceInPlace(m_sCMD, ' ' + m_TableName + ' ', repl, 0, 2);
        }
    }

    m_Cmd= m_Con->LangCmd(m_sCMD);
    m_Cmd->SetParam("@key", &m_Key);
    m_Cmd->BindParam("@n", &m_RowNum);
    m_Cmd->Send(); // sending command to update/insert row
    m_Cmd->DumpResults(); // we don't expect any useful results
    CHECK_DRIVER_ERROR(m_Cmd->RowCount() != 1,
                       "No rows preallocated for key " + m_Key.AsString()
                       + " in table " + m_TableName,
                       1000030);
    return true;
}

/*
UPDATE T SET b1 = 0x0, ..., bn = 0x0 WHERE key = @key AND n = @n
IF @@ROWCOUNT = 0
  INSERT INTO T (key, n, b1, ..., bn)
    SELECT @key, @n, 0x0, ..., 0x0
      WHERE NOT EXISTS (SELECT n FROM T WHERE key = @key AND n = @n)
*/
I_BlobDescriptor& CSimpleBlobStore::BlobDescriptor(void)
{
    m_RowNum = (Int4)(m_ImageNum / m_nofDataCols);
    int i = m_ImageNum % m_nofDataCols;
    if(i == 0) { // prepare new row
        if(m_RowNum.Value() > 0) {
            m_Cmd->Send(); // sending command to update/insert row
            m_Cmd->DumpResults(); // we don't expect any useful results
            CHECK_DRIVER_ERROR(m_Cmd->RowCount() != 1,
                               "No more rows preallocated for key "
                               + m_Key.AsString() + " in table " + m_TableName,
                               1000040);
        }

        string s = m_KeyColName + "= '";
        s.append(m_Key.AsString());
        s += "' AND " + m_NumColName + "=";
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", (long) m_RowNum.Value());
        s.append(buf);

        m_Desc.SetSearchConditions(s);
    }
    m_Desc.SetColumnName(m_DataColName[i]);
    m_ImageNum++;
    return m_Desc;
}

bool CSimpleBlobStore::Fini(void)
{
    if(m_nofDataCols > 0) {
        delete m_Cmd;
        string table_and_hint = m_TableName;
        if ( !m_TableHint.empty() ) {
            impl::CConnection* conn_impl
                = dynamic_cast<impl::CConnection*>(&m_Con->GetExtraFeatures());
            if (conn_impl != NULL
                && conn_impl->GetServerType() == CDBConnParams::eMSSqlServer) {
                table_and_hint += " WITH(" + m_TableHint + ')';
            }
        }

        int i= m_ImageNum % m_nofDataCols;
        string clear_extras;
        if ((m_Flags & fPreallocated) == 0) {
            clear_extras = "DELETE " + table_and_hint;
        } else {
            clear_extras = "UPDATE " + table_and_hint + " SET";
            for (int j = 0;  j < m_nofDataCols;  ++j) {
                clear_extras += (j ? ", " : " ") + m_DataColName[j] + "=NULL";
            }
        }
        clear_extras += (" WHERE " + m_KeyColName + " = @key AND "
                         + m_NumColName + " > @n");
        if(i || m_ImageNum == 0) { // we need to clean-up the current row
            string s= "update " + table_and_hint + " set";
            for(int j= i; j < m_nofDataCols; j++) {
                s+= ((i != j)? ", ":" ") + m_DataColName[j] + " = NULL";
            }
            s+= " where " + m_KeyColName + " = @key AND " + m_NumColName +
                " = @n " + clear_extras;
            m_Cmd= m_Con->LangCmd(s);
        }
        else {
            m_Cmd= m_Con->LangCmd(clear_extras);
        }
        m_Cmd->SetParam("@key", &m_Key);
        m_Cmd->BindParam("@n", &m_RowNum);
        m_Cmd->Send(); // sending command
        m_Cmd->DumpResults(); // we don't expect any useful results
        delete m_Cmd;
        m_Cmd= 0;
        return true;
    }
    return false;
}


/***************************************************************************************
 * CBlobStoreBase - the abstract base interface to deal with reading and writing
 * BLOB data from a C++ application.
 */

CBlobStoreBase::CBlobStoreBase(const string& table_name,
                               ECompressMethod cm,
                               size_t image_limit,
                               bool log_it)
    : CBlobStoreBase(table_name, cm, image_limit,
                     CSimpleBlobStore::TFlags(
                         log_it ? CSimpleBlobStore::fLogBlobs
                         : CSimpleBlobStore::kDefaults))
{
}

CBlobStoreBase::CBlobStoreBase(const string& table_name,
                               ECompressMethod cm,
                               size_t image_limit,
                               CSimpleBlobStore::TFlags flags)
    :m_Table(table_name), m_Cm(cm), m_Limit(image_limit), m_Flags(flags),
     m_KeyColName(kEmptyStr), m_NumColName(kEmptyStr), m_ReadQuery(kEmptyStr),
     m_BlobColumn(NULL), m_NofBC(0)
{
}

CBlobStoreBase::~CBlobStoreBase()
{
    delete[] m_BlobColumn;
}

void
CBlobStoreBase::ReadTableDescr()
{
    if(m_BlobColumn) {
        delete[] m_BlobColumn;
        m_BlobColumn = NULL;
    }

    CDB_Connection* con = GetConn();

    /* derive information regarding the table */
    string sql = "SELECT * FROM " + m_Table + " WHERE 1=0";
    unique_ptr<CDB_LangCmd> lcmd(con->LangCmd(sql));
    if(!lcmd->Send()) {
        ReleaseConn(con);
        DATABASE_DRIVER_ERROR("Failed to send a command to the server: " + sql,
                              1000030);
    }

    unsigned int n;

    while(lcmd->HasMoreResults()) {
        CDB_Result* r= lcmd->Result();
        if(!r) continue;
        if(r->ResultType() == eDB_RowResult) {
            n= r->NofItems();
            if(n < 2) {
                delete r;
                continue;
            }

            m_BlobColumn= new string[n];

            for(unsigned int j= 0; j < n; j++) {
                switch (r->ItemDataType(j)) {
                case eDB_VarChar:
                case eDB_Char:
                case eDB_LongChar:
                    m_KeyColName= r->ItemName(j);
                    break;

                case eDB_Int:
                case eDB_SmallInt:
                case eDB_TinyInt:
                case eDB_BigInt:
                    m_NumColName= r->ItemName(j);
                    break;

                case eDB_Text:
                case eDB_VarCharMax:
                    m_Flags |= CSimpleBlobStore::fIsText;
                    m_BlobColumn[m_NofBC++] = r->ItemName(j);
                    break;

                case eDB_Image:
                case eDB_VarBinaryMax:
                    m_Flags &= ~CSimpleBlobStore::fIsText;
                    m_BlobColumn[m_NofBC++]= r->ItemName(j);
                    break;
                default:;
                }
            }
            m_BlobColumn[m_NofBC]= kEmptyStr;
            while(r->Fetch());
        }
        delete r;
    }

    lcmd.reset();
    ReleaseConn(con);

    if((m_NofBC < 1) || m_KeyColName.empty()) {
        DATABASE_DRIVER_ERROR( "Table "+m_Table+" cannot be used for BlobStore", 1000040 );
    }
}

void
CBlobStoreBase::SetTableDescr(const string& tableName,
                              const string& keyColName,
                              const string& numColName,
                              const string* blobColNames,
                              size_t nofBC,
                              bool isText)
{
    if(m_BlobColumn)
    {
        delete[] m_BlobColumn;
        m_BlobColumn = NULL;
    }

    m_ReadQuery = "";

    m_Table = tableName;
    m_KeyColName = keyColName;
    m_NumColName = numColName;
    m_NofBC = nofBC;
    if (isText) {
        m_Flags |= CSimpleBlobStore::fIsText;
    } else {
        m_Flags &= ~CSimpleBlobStore::fIsText;
    }

    if((m_NofBC < 1) || m_KeyColName.empty())
    {
        DATABASE_DRIVER_ERROR( "Table "+m_Table+" cannot be used for BlobStore", 1000040 );
    }

    m_BlobColumn = new string[m_NofBC+1];
    m_BlobColumn[m_NofBC] = kEmptyStr;

    for(size_t i=0; i<m_NofBC; ++i)
        m_BlobColumn[i] = blobColNames[i];
}

void
CBlobStoreBase::SetTextSizeServerSide(CDB_Connection* pConn, size_t textSize)
{
    string s("set TEXTSIZE ");
    s += NStr::NumericToString(textSize);
    unique_ptr<CDB_LangCmd> lcmd(pConn->LangCmd(s));

    if(!lcmd->Send())
    {
        DATABASE_DRIVER_ERROR("Failed to send a command to the server: " + s,
                              1000035);
    }

    while(lcmd->HasMoreResults())
    {
        unique_ptr<CDB_Result> r(lcmd->Result());
        if(!r.get()) continue;
        if(r->ResultType() == eDB_StatusResult)
        {
            while(r->Fetch())
            {
                CDB_Int status;
                r->GetItem(&status);
                if(status.Value() != 0)
                {
                    DATABASE_DRIVER_ERROR( "Wrong status for " + s, 1000036 );
                }
            }
        }
        else {
            while(r->Fetch());
        }
    }
}

void CBlobStoreBase::GenReadQuery(const CTempString& table_hint)
{
    m_ReadQuery = kEmptyStr;

    m_ReadQuery = "select ";
    for(size_t i= 0; i < m_NofBC; i++)
    {
        if(i) m_ReadQuery+= ", ";
        m_ReadQuery+= m_BlobColumn[i];
    }
    m_ReadQuery += " from " + m_Table;
    if ( !table_hint.empty() ) {
        impl::CConnection* conn_impl
            = dynamic_cast<impl::CConnection*>(&GetConn()->GetExtraFeatures());
        if (conn_impl != NULL
            &&  conn_impl->GetServerType() == CDBConnParams::eMSSqlServer) {
            m_ReadQuery += string(" WITH(") + table_hint + ')';
        }
    }
    m_ReadQuery += " where " + m_KeyColName + "=@blob_id";
    if(!m_NumColName.empty())
    {
        m_ReadQuery+= " order by " + m_NumColName + " ASC";
    }
}


bool CBlobStoreBase::Exists(const string& blob_id)
{
    CDB_Connection* con = GetConn();

    /* check the key */
    string sql = ("IF EXISTS(SELECT * FROM " + m_Table + " WHERE "
                  + m_KeyColName + "='" + blob_id + "') SELECT 1");
    unique_ptr<CDB_LangCmd> lcmd(con->LangCmd(sql));
    if(!lcmd->Send()) {
        lcmd.reset();
        ReleaseConn(con);
        DATABASE_DRIVER_ERROR("Failed to send a command to the server: " + sql,
                              1000030);
    }

    bool re = false;

    while(lcmd->HasMoreResults()) {
        unique_ptr<CDB_Result> r(lcmd->Result());
        if(!r.get()) continue;
        if(r->ResultType() == eDB_RowResult) {
            while(r->Fetch())
                re= true;
        }
    }

    lcmd.reset();
    ReleaseConn(con);

    return re;
}

void CBlobStoreBase::Delete(const string& blob_id)
{
    CDB_Connection* con = GetConn();

    /* check the key */
    string sql = ("DELETE " + m_Table + " WHERE " + m_KeyColName + "='"
                  + blob_id + "'");
    unique_ptr<CDB_LangCmd> lcmd(con->LangCmd(sql));
    if(!lcmd->Send()) {
        lcmd.reset();
        ReleaseConn(con);
        DATABASE_DRIVER_ERROR("Failed to send a command to the server: " + sql,
                              1000030);
    }

    lcmd->DumpResults();

    lcmd.reset();
    ReleaseConn(con);
}

istream* CBlobStoreBase::OpenForRead(const string& blob_id,
                                     const CTempString& table_hint)
{
    CDB_Connection* con = GetConn();

    if(m_ReadQuery.empty()) {
        GenReadQuery(table_hint);
    }

    unique_ptr<CDB_LangCmd> lcmd(con->LangCmd(m_ReadQuery));
    CDB_VarChar blob_key(blob_id);
    lcmd->BindParam("@blob_id", &blob_key);

    if(!lcmd->Send()) {
        lcmd.reset();
        ReleaseConn(con);
        DATABASE_DRIVER_ERROR("Failed to send a command to the server: "
                              + m_ReadQuery + " (with @blob_id = " + blob_id
                              + ')', 1000030);
    }

    while(lcmd->HasMoreResults()) {
        unique_ptr<CDB_Result> r(lcmd->Result());

        if(!r.get()) {
            continue;
        }

        if(r->ResultType() != eDB_RowResult) {
            continue;
        }

        if(r->Fetch()) {
            // creating a stream
            CBlobReader* bReader = new CBlobReader(r.release(), lcmd.release(), ReleaseConn(0) ? con : 0);
            unique_ptr<CRStream> iStream(new CRStream(bReader, 0, 0, CRWStreambuf::fOwnReader));
            unique_ptr<CCompressionStreamProcessor> zProc;

            switch(m_Cm) {
            case eZLib:
                zProc.reset(new CCompressionStreamProcessor((CZipDecompressor*)(new CZipDecompressor),
                                                             CCompressionStreamProcessor::eDelete));
                break;
            case eBZLib:
                zProc.reset(new CCompressionStreamProcessor((CBZip2Decompressor*)(new CBZip2Decompressor),
                                                             CCompressionStreamProcessor::eDelete));
                break;
            default:
                return iStream.release();
            }

            return new CCompressionIStream(*iStream.release(),
                                           zProc.release(),
                                           CCompressionStream::fOwnAll);
        }
    }

    lcmd.reset();
    ReleaseConn(con);

    return 0;
}

ostream* CBlobStoreBase::OpenForWrite(const string& blob_id,
                                      const CTempString& table_hint)
{
    CDB_Connection* con = GetConn();

    unique_ptr<CSimpleBlobStore> sbs
        (new CSimpleBlobStore(m_Table, m_KeyColName, m_NumColName,
                              m_BlobColumn, m_Flags, table_hint));
    sbs->SetKey(blob_id);
    // CBlobLoader* bload= new CBlobLoader(my_context, server_name, user_name, passwd, &sbs);
    if(sbs->Init(con)) {
        CBlobWriter::TFlags bwflags = CBlobWriter::fOwnDescr;
        if ((m_Flags & CSimpleBlobStore::fLogBlobs) != 0) {
            bwflags |= CBlobWriter::fLogBlobs;
        }
        if (ReleaseConn(0)) {
            bwflags |= CBlobWriter::fOwnCon;
        }
        CBlobWriter* bWriter = new CBlobWriter(con, sbs.release(), m_Limit,
                                               bwflags);
        unique_ptr<CWStream> oStream(new CWStream(bWriter, 0, 0,  CRWStreambuf::fOwnWriter));
        unique_ptr<CCompressionStreamProcessor> zProc;

        switch(m_Cm) {
        case eZLib:
            zProc.reset(new CCompressionStreamProcessor((CZipCompressor*)(new CZipCompressor),
                                                        CCompressionStreamProcessor::eDelete));
            break;
        case eBZLib:
            zProc.reset(new CCompressionStreamProcessor((CBZip2Compressor*)(new CBZip2Compressor),
                                                        CCompressionStreamProcessor::eDelete));
            break;
        default:
            return oStream.release();
        }

        return new CCompressionOStream(*oStream.release(),
                                       zProc.release(),
                                       CCompressionStream::fOwnAll);
    }

    ReleaseConn(con);

    return 0;
}



/***************************************************************************************
 * CBlobStoreStatic - the simple interface to deal with reading and writing
 * BLOB data from a C++ application.
 * It uses connection to DB from an external pool.
 */

CBlobStoreStatic::CBlobStoreStatic(CDB_Connection* pConn,
                                   const string& table_name,
                                   ECompressMethod cm,
                                   size_t image_limit,
                                   bool log_it)
    : CBlobStoreStatic(pConn, table_name, cm, image_limit,
                       CSimpleBlobStore::TFlags(
                           log_it ? CSimpleBlobStore::fLogBlobs
                           : CSimpleBlobStore::kDefaults))
{
}

CBlobStoreStatic::CBlobStoreStatic(CDB_Connection* pConn,
                                   const string& table_name,
                                   ECompressMethod cm,
                                   size_t image_limit,
                                   CSimpleBlobStore::TFlags flags)
    : CBlobStoreBase(table_name, cm, image_limit, flags),
      m_pConn(pConn)
{
    ReadTableDescr();
    SetTextSizeServerSide(m_pConn);
}

CBlobStoreStatic::CBlobStoreStatic(CDB_Connection* pConn,
                                   const string& tableName,
                                   const string& keyColName,
                                   const string& numColName,
                                   const string* blobColNames,
                                   size_t nofBC,
                                   bool isText,
                                   ECompressMethod cm,
                                   size_t image_limit,
                                   bool log_it)
    : CBlobStoreStatic(pConn, tableName, keyColName, numColName, blobColNames,
                       nofBC,
                       CSimpleBlobStore::TFlags(
                           (isText ? CSimpleBlobStore::fIsText
                            : CSimpleBlobStore::kDefaults) |
                           (log_it ? CSimpleBlobStore::fLogBlobs
                            : CSimpleBlobStore::kDefaults)),
                       cm, image_limit)
{
}

CBlobStoreStatic::CBlobStoreStatic(CDB_Connection* pConn,
                                   const string& tableName,
                                   const string& keyColName,
                                   const string& numColName,
                                   const string* blobColNames,
                                   size_t nofBC,
                                   CSimpleBlobStore::TFlags flags,
                                   ECompressMethod cm,
                                   size_t image_limit)
    :CBlobStoreBase("", cm, image_limit, flags),
     m_pConn(pConn)
{
    SetTableDescr(tableName, keyColName, numColName, blobColNames, nofBC,
                  (flags & CSimpleBlobStore::fIsText) != 0);
    SetTextSizeServerSide(m_pConn);
}


CDB_Connection*
CBlobStoreStatic::GetConn()
{
    if(!m_pConn)
        DATABASE_DRIVER_ERROR( "Bad connection to SQL server", 1000020 );
    return m_pConn;
}

CBlobStoreStatic::~CBlobStoreStatic()
{
}



/***************************************************************************************
 * CBlobStoreDynamic - the simple interface to deal with reading and writing
 * BLOB data from a C++ application.
 * It uses an internal connections pool and ask pool for a connection each time before
 * connection use.
 */


CBlobStoreDynamic::CBlobStoreDynamic(I_DriverContext* pCntxt,
                                     const string& server,
                                     const string& user,
                                     const string& passwd,
                                     const string& table_name,
                                     ECompressMethod cm,
                                     size_t image_limit,
                                     bool log_it)
    : CBlobStoreDynamic(pCntxt, server, user, passwd, table_name, cm,
                        image_limit,
                        CSimpleBlobStore::TFlags(
                            log_it ? CSimpleBlobStore::fLogBlobs
                            : CSimpleBlobStore::kDefaults))
{
}

CBlobStoreDynamic::CBlobStoreDynamic(I_DriverContext* pCntxt,
                                     const string& server,
                                     const string& user,
                                     const string& passwd,
                                     const string& table_name,
                                     ECompressMethod cm,
                                     size_t image_limit,
                                     CSimpleBlobStore::TFlags flags)
    :CBlobStoreBase(table_name, cm, image_limit, flags),
     m_Cntxt(pCntxt), m_Server(server), m_User(user), m_Passwd(passwd),
     m_Pool(server+user+table_name)
{
    if(!m_Cntxt)
    {
        DATABASE_DRIVER_ERROR( "Null pointer to driver context", 1000010 );
    }
    m_Cntxt->SetMaxBlobSize(image_limit>0 ? image_limit : 0x1FFFFE);
    ReadTableDescr();
}

CBlobStoreDynamic::~CBlobStoreDynamic()
{
}


CDB_Connection*
CBlobStoreDynamic::GetConn()
{
    if(!m_Cntxt)
    {
        DATABASE_DRIVER_ERROR( "Null pointer to driver context", 1000010 );
    }
    CDB_Connection* pConn =
        m_Cntxt->Connect(m_Server, m_User, m_Passwd, 0, true, m_Pool);
    if(!pConn)
    {
        DATABASE_DRIVER_ERROR( "Cannot open connection to SQL server", 1000020 );
    }
    SetTextSizeServerSide(pConn);

    return pConn;
}

bool
CBlobStoreDynamic::ReleaseConn(CDB_Connection* pConn)
{
    if(pConn)
        delete pConn;
    return true;
}

