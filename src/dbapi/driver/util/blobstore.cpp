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
#include "blobstore.hpp"

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
                         size_t image_limit, bool log_it)
{
    m_Con= con;
    m_dMaker= d_maker;
    m_Limit= (image_limit > 1)? image_limit : (16*1024*1024);
    m_LogIt= log_it;
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
            if((m_ItemNum < 0 || m_ItemNum >= m_Res->NofItems())&& m_Res->Fetch()) {
                m_ItemNum= 0; // starting next row
            }
        }
        while(m_ItemNum >= 0 && m_ItemNum < m_Res->NofItems());

        m_AllDone= true; // nothing to read any more

    } catch (CDB_Exception& e) {
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
                                  const string blob_column[]) :
    m_TableName(table_name), m_KeyColName(key_col_name), 
    m_NumColName(num_col_name), m_Desc(table_name)
{
    m_Con= 0;
    m_Cmd= 0;
    for(m_nofDataCols= 0; !blob_column[m_nofDataCols].empty(); m_nofDataCols++);
    if(m_nofDataCols) {
        int i;
        m_DataColName= new string[m_nofDataCols];
        for(i= 0; i < m_nofDataCols; i++) {
            m_DataColName[i]= blob_column[i];
        }
        m_sCMD= "if exists(select * from " + m_TableName +
            " where " + m_KeyColName + " = @key AND " + m_NumColName +
            " = @n) update " + m_TableName + " set";
        for(i= 0; i < m_nofDataCols; i++) {
            m_sCMD+= (i? ", ":" ") + m_DataColName[i] + " = 0x0";
        }
        m_sCMD+= " where " + m_KeyColName + " = @key AND " + m_NumColName +
            " = @n else insert " + m_TableName + "(" + m_KeyColName + "," +
            m_NumColName;
        for(i= 0; i < m_nofDataCols; i++) {
            m_sCMD+= ", " + m_DataColName[i];
        }
        m_sCMD+= ")values(@key,@n";
        for(i= 0; i < m_nofDataCols; i++) {
            m_sCMD+= ", 0x0";
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
        m_Cmd->Send(); // sending command to update/insert row
        m_Cmd->DumpResults(); // we don't expect any useful results

        string s= m_KeyColName + "= '";
        s.append(m_Key.Value());
        s+= "' AND " + m_NumColName + "=";
        char buf[32];
        sprintf(buf,"%ld",m_RowNum.Value());
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
        if(i) { // we need to clean-up the current row
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
