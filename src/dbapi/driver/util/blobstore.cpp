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
#include <util/rwstream.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>
#include <util/compress/bzip2.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <dbapi/driver/util/blobstore.hpp>

USING_NCBI_SCOPE;

bool CBlobWriter::storeBlob(void)
{
    try {
        m_Con->SendData(m_dMaker->ItDescriptor(), m_Blob, m_LogIt);
        m_Blob.Truncate();
        return true;
    }
    catch (CDB_Exception& e) {
        CDB_UserHandler_Stream myExHandler(&cerr);

        myExHandler.HandleIt(&e);
        return false;
    }
}

CBlobWriter::CBlobWriter(CDB_Connection* con, ItDescriptorMaker* d_maker,
                         size_t image_limit, TFlags flags)
{
    m_Con= con;
    m_dMaker= d_maker;
    m_Limit= (image_limit > 1)? image_limit : (16*1024*1024);
    m_LogIt= ((flags & fLogBlobs) != 0);
    m_DelDesc= ((flags & fOwnCon) != 0);
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
        if(!storeBlob()) return eRW_Error;
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

    } catch (CDB_Exception&) {
        m_AllDone= true;
    }

    if(bytes_read) *bytes_read= n + rn;
    return r;
}

ERW_Result CBlobReader::PendingCount(size_t* count)
{
    if(count) *count= 0;
    return m_AllDone? eRW_Eof : eRW_NotImplemented;
}

CBlobReader::~CBlobReader()
{
    if(m_Cmd) {
        delete m_Res;
        delete m_Cmd;
    }
    if(m_Con) {
        delete m_Con;
    }
}

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
        CBlobReader* bReader= new CBlobReader(m_Res);
        CRStream* iStream= new CRStream(bReader);
        CCompressionStreamProcessor* zProc;

        switch(cm) {
        case eZLib:
            zProc= new CCompressionStreamProcessor((CZipDecompressor*)(new CZipDecompressor),
                                                   CCompressionStreamProcessor::eDelete);
            break;
        case eBZLib:
            zProc=  new CCompressionStreamProcessor((CBZip2Decompressor*)(new CBZip2Decompressor),
                                                    CCompressionStreamProcessor::eDelete);
            break;
        default:
            zProc= 0;
        }
        if(zProc) {
            CCompressionIStream* zStream= new CCompressionIStream(*iStream, zProc);
            s << zStream->rdbuf();
            delete zStream;
            delete zProc;
        }
        else {
            s << iStream->rdbuf();
        }
        delete iStream;
        delete bReader;

        m_IsGood= m_Res->Fetch();
        return true;
    }
    return false;
}

CBlobRetriever::~CBlobRetriever()
{
    if(m_Res) delete m_Res;
    if(m_Cmd) delete m_Cmd;
    if(m_Conn) delete m_Conn;
}

CBlobLoader::CBlobLoader(I_DriverContext* pCntxt,
                         const string&    server,
                         const string&    user,
                         const string&    passwd,
                         ItDescriptorMaker* d_maker)
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
        CBlobWriter* bWriter= new CBlobWriter(m_Conn, m_dMaker, image_limit, log_it);
        CWStream* oStream= new CWStream(bWriter);
        CCompressionStreamProcessor* zProc;

        switch(cm) {
        case eZLib:
            zProc= new CCompressionStreamProcessor((CZipCompressor*)(new CZipCompressor),
                                                   CCompressionStreamProcessor::eDelete);
            break;
        case eBZLib:
            zProc=  new CCompressionStreamProcessor((CBZip2Compressor*)(new CBZip2Compressor),
                                                    CCompressionStreamProcessor::eDelete);
            break;
        default:
            zProc= 0;
        }
        if(zProc) {
            CCompressionOStream* zStream= new CCompressionOStream(*oStream, zProc);
            *zStream << s.rdbuf();
            delete zStream;
            delete zProc;
        }
        else {
            *oStream << s.rdbuf();
        }
        delete oStream;
        delete bWriter;

        return m_dMaker->Fini();
    }
    return false;

}
CSimpleBlobStore::CSimpleBlobStore(const string& table_name,
                                   const string& key_col_name,
                                   const string& num_col_name,
                                   const string blob_column[],
                                   bool is_text) :
    m_TableName(table_name), m_KeyColName(key_col_name),
    m_NumColName(num_col_name), m_RowNum(0), m_Desc(table_name)
{
    m_Con= 0;
    m_Cmd= 0;
    for(m_nofDataCols= 0; !blob_column[m_nofDataCols].empty(); m_nofDataCols++);
    if(m_nofDataCols) {
        int i;
        string init_val(is_text? "' '" : "0x0");
        m_DataColName= new string[m_nofDataCols];
        for(i= 0; i < m_nofDataCols; i++) {
            m_DataColName[i]= blob_column[i];
        }
        m_sCMD= "if exists(select * from " + m_TableName +
            " where " + m_KeyColName + " = @key AND " + m_NumColName +
            " = @n) update " + m_TableName + " set";
        for(i= 0; i < m_nofDataCols; i++) {
            m_sCMD+= (i? ", ":" ") + m_DataColName[i] + "=" + init_val;
        }
        m_sCMD+= " where " + m_KeyColName + " = @key AND " + m_NumColName +
            " = @n else insert " + m_TableName + "(" + m_KeyColName + "," +
            m_NumColName;
        for(i= 0; i < m_nofDataCols; i++) {
            m_sCMD+= ", " + m_DataColName[i];
        }
        m_sCMD+= ")values(@key,@n";
        for(i= 0; i < m_nofDataCols; i++) {
            m_sCMD+= "," + init_val;
        }
        m_sCMD+=")";
    }
    else m_DataColName= 0;
}

CSimpleBlobStore::~CSimpleBlobStore()
{
    if(m_DataColName) delete[]m_DataColName;
    if(m_Cmd) delete m_Cmd;
}

bool CSimpleBlobStore::Init(CDB_Connection* con)
{
    m_Con= con;
    m_ImageNum= 0;
    if(m_Key.IsNULL() || (m_nofDataCols < 1)) return false;
    m_Cmd= m_Con->LangCmd(m_sCMD, 2);
    m_Cmd->SetParam("@key", &m_Key);
    m_Cmd->BindParam("@n", &m_RowNum);
    m_Cmd->Send(); // sending command to update/insert row
    m_Cmd->DumpResults(); // we don't expect any useful results
    return true;
}

/*
if exists(select key from T where key= @key and n = @n)
begin
   update T set b1=0x0, ... bn= 0x0 where key= @key and n= @n
end
else
   insert T(key, n, b1, ...,bn) values(@key, @n, 0x0, ...,0x0)
*/
I_ITDescriptor& CSimpleBlobStore::ItDescriptor(void)
{
    m_RowNum= (Int4)(m_ImageNum / m_nofDataCols);
    int i= m_ImageNum % m_nofDataCols;
    if(i == 0) { // prepare new row
        if(m_RowNum.Value() > 0) {
            m_Cmd->Send(); // sending command to update/insert row
            m_Cmd->DumpResults(); // we don't expect any useful results
        }

        string s= m_KeyColName + "= '";
        s.append(m_Key.Value());
        s+= "' AND " + m_NumColName + "=";
        char buf[32];
        sprintf(buf, "%ld", (long) m_RowNum.Value());
        s.append(buf);

        m_Desc.SetSearchConditions(s);
    }
    m_Desc.SetColumn(m_DataColName[i]);
    m_ImageNum++;
    return m_Desc;
}

bool CSimpleBlobStore::Fini(void)
{
    if(m_nofDataCols > 0) {
        delete m_Cmd;
        int i= m_ImageNum % m_nofDataCols;
        if(i || m_RowNum.Value() == 0) { // we need to clean-up the current row
            string s= "update " + m_TableName + " set";
            for(int j= i; j < m_nofDataCols; j++) {
                s+= ((i != j)? ", ":" ") + m_DataColName[j] + " = NULL";
            }
            s+= " where " + m_KeyColName + " = @key AND " + m_NumColName +
                " = @n delete " + m_TableName + " where " + m_KeyColName +
                " = @key AND " + m_NumColName + " > @n";
            m_Cmd= m_Con->LangCmd(s, 2);
        }
        else {
            string s= "delete " + m_TableName + " where " + m_KeyColName +
                " = @key AND " + m_NumColName + " > @n";
            m_Cmd= m_Con->LangCmd(s, 2);
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
 * the image/text data from a C++ application.
 */


CBlobStoreBase::CBlobStoreBase(const string& table_name,
                               ECompressMethod cm,
                               size_t image_limit,
                               bool log_it)
    :m_Table(table_name), m_Cm(cm), m_Limit(image_limit),
     m_LogIt(log_it), m_IsText(false),
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
    if(m_BlobColumn)
    {
        delete[] m_BlobColumn;
        m_BlobColumn = NULL;
    }

    CDB_Connection* con = GetConn();

    /* derive information regarding the table */
    CDB_LangCmd* lcmd= con->LangCmd("select * from "+m_Table+" where 1=0");
    if(!lcmd->Send()) {
        ReleaseConn(con);
        throw CDB_ClientEx(eDB_Error, 1000030, "CBlobStoreBase",
                           "Failed to send a command to the server");
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

                case eDB_Text: m_IsText= true;
                case eDB_Image:
                    m_BlobColumn[m_NofBC++]= r->ItemName(j);
                default:;
                }
            }
            m_BlobColumn[m_NofBC]= kEmptyStr;
            while(r->Fetch());
        }
        delete r;
    }
    delete lcmd;
    ReleaseConn(con);

    if((m_NofBC < 1) || m_KeyColName.empty()) {
        throw CDB_ClientEx(eDB_Error, 1000040, "CBlobStoreBase",
                           "Table "+m_Table+" can not be used for BlobStore");
    }
}

void
CBlobStoreBase::SetTableDescr(const string& tableName,
                              const string& keyColName,
                              const string& numColName,
                              const string* blobColNames,
                              unsigned nofBC,
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
    m_IsText = isText;

    if((m_NofBC < 1) || m_KeyColName.empty())
    {
        throw CDB_ClientEx(eDB_Error, 1000040, "CBlobStoreBase",
                           "Table "+m_Table+" can not be used for BlobStore");
    }

    m_BlobColumn = new string[m_NofBC+1];
    m_BlobColumn[m_NofBC] = kEmptyStr;

    for(unsigned i=0; i<m_NofBC; ++i)
        m_BlobColumn[i] = blobColNames[i];
}

void
CBlobStoreBase::SetTextSizeServerSide(CDB_Connection* pConn, size_t textSize)
{
    string s("set TEXTSIZE ");
    s += NStr::UIntToString(textSize);
    CDB_LangCmd* lcmd = pConn->LangCmd(s.c_str(), 0);

    if(!lcmd->Send())
    {
        delete lcmd;
        throw CDB_ClientEx(eDB_Error, 1000035,
                           "CBlobStoreBase::SetTextSizeServerSide",
                           "Failed to send a command to the server");
    }

    while(lcmd->HasMoreResults())
    {
        CDB_Result* r= lcmd->Result();
        if(!r) continue;
        if(r->ResultType() == eDB_StatusResult)
        {
            while(r->Fetch())
            {
                CDB_Int status;
                r->GetItem(&status);
                if(status.Value() != 0)
                {
                    delete r;
                    delete lcmd;
                    throw CDB_ClientEx(eDB_Error, 1000036,
                                       "CBlobStoreBase::SetTextSizeServerSide",
                                       "Wrong status");
                }
            }
        }
        else
            while(r->Fetch());
        delete r;
    }
}

void CBlobStoreBase::GenReadQuery()
{
    m_ReadQuery = kEmptyStr;

    m_ReadQuery = "select ";
    for(unsigned i= 0; i < m_NofBC; i++)
    {
        if(i) m_ReadQuery+= ", ";
        m_ReadQuery+= m_BlobColumn[i];
    }
    m_ReadQuery+= " from " + m_Table + " where " + m_KeyColName + "=@blob_id";
    if(!m_NumColName.empty())
    {
        m_ReadQuery+= " order by " + m_NumColName + " ASC";
    }
}


bool CBlobStoreBase::Exists(const string& blob_id)
{
    CDB_Connection* con = GetConn();

    /* check the key */
    CDB_LangCmd* lcmd = con->LangCmd("if EXISTS(select * from "+m_Table+" where "+
                                     m_KeyColName+"='"+blob_id+"') select 1");
    if(!lcmd->Send()) {
        delete lcmd;
        ReleaseConn(con);
        throw CDB_ClientEx(eDB_Error, 1000030, "CBlobStoreBase::Exists",
                           "Failed to send a command to the server");
    }

    bool re= false;

    while(lcmd->HasMoreResults()) {
        CDB_Result* r= lcmd->Result();
        if(!r) continue;
        if(r->ResultType() == eDB_RowResult) {
            while(r->Fetch())
                re= true;
        }
        delete r;
    }
    delete lcmd;
    ReleaseConn(con);
    return re;
}

void CBlobStoreBase::Delete(const string& blob_id)
{
    CDB_Connection* con = GetConn();

    /* check the key */
    CDB_LangCmd* lcmd= con->LangCmd("delete "+m_Table+" where "+
                                    m_KeyColName+"='"+blob_id+"'");
    if(!lcmd->Send()) {
        delete lcmd;
        ReleaseConn(con);
        throw CDB_ClientEx(eDB_Error, 1000030, "CBlobStoreBase::delete",
                           "Failed to send a command to the server");
    }

    lcmd->DumpResults();

    delete lcmd;
    ReleaseConn(con);
}

istream* CBlobStoreBase::OpenForRead(const string& blob_id)
{
    CDB_Connection* con = GetConn();

    if(m_ReadQuery.empty())
        GenReadQuery();

    CDB_LangCmd* lcmd= con->LangCmd(m_ReadQuery, 1);
    CDB_VarChar blob_key(blob_id);
    lcmd->BindParam("@blob_id", &blob_key);

    if(!lcmd->Send()) {
        delete lcmd;
        ReleaseConn(con);
        throw CDB_ClientEx(eDB_Error, 1000030, "CBlobStoreBase::OpenForRead",
                           "Failed to send a command to the server");
    }

    while(lcmd->HasMoreResults()) {
        CDB_Result* r= lcmd->Result();
        if(!r) continue;
        if(r->ResultType() != eDB_RowResult) {
            delete r;
            continue;
        }
        if(r->Fetch()) {
            // creating a stream
            CBlobReader* bReader= new CBlobReader(r, lcmd, ReleaseConn(0) ? con : 0);
            CRStream* iStream= new CRStream(bReader, 0, 0, CRWStreambuf::fOwnReader);
            CCompressionStreamProcessor* zProc;
            switch(m_Cm) {

            case eZLib:
                zProc= new CCompressionStreamProcessor((CZipDecompressor*)(new CZipDecompressor),
                                                       CCompressionStreamProcessor::eDelete);
                break;
            case eBZLib:
                zProc=  new CCompressionStreamProcessor((CBZip2Decompressor*)(new CBZip2Decompressor),
                                                        CCompressionStreamProcessor::eDelete);
                break;
            default:
                return iStream;
            }
            CCompressionIStream* zStream= new CCompressionIStream(*iStream, zProc,
                                                                  CCompressionStream::fOwnAll);

            return zStream;
        }
    }
    delete lcmd;
    ReleaseConn(con);
    return 0;
}

ostream* CBlobStoreBase::OpenForWrite(const string& blob_id)
{
    CDB_Connection* con = GetConn();

    CSimpleBlobStore* sbs= new CSimpleBlobStore(m_Table, m_KeyColName, m_NumColName, m_BlobColumn,
                                                m_IsText);
    sbs->SetKey(blob_id);
    // CBlobLoader* bload= new CBlobLoader(my_context, server_name, user_name, passwd, &sbs);
    if(sbs->Init(con)) {
        CBlobWriter* bWriter= new CBlobWriter(con, sbs, m_Limit,
                                              CBlobWriter::fOwnDescr |
                                              (m_LogIt? CBlobWriter::fLogBlobs : 0) |
                                              (ReleaseConn(0) ? CBlobWriter::fOwnCon : 0));
        CWStream* oStream= new CWStream(bWriter, 0, 0,  CRWStreambuf::fOwnWriter);
        CCompressionStreamProcessor* zProc;

        switch(m_Cm) {
        case eZLib:
            zProc= new CCompressionStreamProcessor((CZipCompressor*)(new CZipCompressor),
                                                   CCompressionStreamProcessor::eDelete);
            break;
        case eBZLib:
            zProc=  new CCompressionStreamProcessor((CBZip2Compressor*)(new CBZip2Compressor),
                                                    CCompressionStreamProcessor::eDelete);
            break;
        default:
            return oStream;
        }
        CCompressionOStream* zStream= new CCompressionOStream(*oStream, zProc, CCompressionStream::fOwnAll);
        return zStream;
    }
    ReleaseConn(con);
    return 0;
}



/***************************************************************************************
 * CBlobStoreStatic - the simple interface to deal with reading and writing
 * the image/text data from a C++ application.
 * It uses connection to DB from an external pool.
 */


CBlobStoreStatic::CBlobStoreStatic(CDB_Connection* pConn,
                                   const string& table_name,
                                   ECompressMethod cm,
                                   size_t image_limit,
                                   bool log_it)
    : CBlobStoreBase(table_name, cm, image_limit, log_it),
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
                                   unsigned nofBC,
                                   bool isText,
                                   ECompressMethod cm,
                                   size_t image_limit,
                                   bool log_it)
    :CBlobStoreBase("", cm, image_limit, log_it),
     m_pConn(pConn)
{
    SetTableDescr(tableName, keyColName, numColName,
                  blobColNames, nofBC, isText);
    SetTextSizeServerSide(m_pConn);
}


CDB_Connection*
CBlobStoreStatic::GetConn()
{
    if(!m_pConn)
        throw CDB_ClientEx(eDB_Error, 1000020, "CBlobStoreStatic",
                           "Bad connection to SQL server");
    return m_pConn;
}

CBlobStoreStatic::~CBlobStoreStatic()
{
}



/***************************************************************************************
 * CBlobStoreDynamic - the simple interface to deal with reading and writing
 * the image/text data from a C++ application.
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
    :CBlobStoreBase(table_name, cm, image_limit, log_it),
     m_Cntxt(pCntxt), m_Server(server), m_User(user), m_Passwd(passwd),
     m_Pool(server+user+table_name)
{
    if(!m_Cntxt)
    {
        throw CDB_ClientEx(eDB_Error, 1000010, "CBlobStoreDynamic",
                           "Null pointer to driver context");
    }
    m_Cntxt->SetMaxTextImageSize(image_limit>0 ? image_limit : 0x1FFFFE);
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
        throw CDB_ClientEx(eDB_Error, 1000010, "CBlobStoreDynamic",
                           "Null pointer to driver context");
    }
    CDB_Connection* pConn =
        m_Cntxt->Connect(m_Server, m_User, m_Passwd, 0, true, m_Pool);
    if(!pConn)
    {
        throw CDB_ClientEx(eDB_Error, 1000020, "CBlobStoreDynamic",
                           "Can not open connection to SQL server");
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
