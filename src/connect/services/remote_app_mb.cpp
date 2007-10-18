/*  $Id$
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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/blob_storage.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/ncbifile.hpp>

#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/remote_app_mb.hpp>
#include <connect/services/error_codes.hpp>

#include "remote_app_impl.hpp"


#define NCBI_USE_ERRCODE_X   ConnServ_Remote

BEGIN_NCBI_SCOPE

const size_t kMaxBlobInlineSize = 500;

//////////////////////////////////////////////////////////////////////////////
//
class CBlobStreamHelper
{
public:
    CBlobStreamHelper(IBlobStorage& storage, string& data, size_t& data_size)
        : m_Storage(&storage), m_Data(&data), m_DataSize(&data_size)
    {
    }

    ~CBlobStreamHelper()
    {
        try {
            Reset();
        } NCBI_CATCH_ALL_X(14, "CBlobStreamHelper::~CBlobStreamHelper()");
    }

    CNcbiOstream& GetOStream(const string& fname = "", 
                             EStdOutErrStorageType type = eBlobStorage,
                             size_t max_inline_size = kMaxBlobInlineSize)
    {
        if (!m_OStream.get()) {
            _ASSERT(!m_IStream.get());
            auto_ptr<IWriter> writer( 
                        new CStringOrBlobStorageWriter(max_inline_size,
                                                       *m_Storage,
                                                       *m_Data)
                        );
            m_OStream.reset(new CWStream(writer.release(), 
                                         0, 0, CRWStreambuf::fOwnWriter
                                             | CRWStreambuf::fLogExceptions));
            m_OStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
            *m_OStream << (int) type << " ";
            WriteStrWithLen(*m_OStream, fname);
            if (!fname.empty() && type == eLocalFile) {                   
                m_OStream.reset(new CNcbiOfstream(fname.c_str()));
                if (!m_OStream->good()) {
                    NCBI_THROW(CFileException, eRelativePath, 
                               "Cannot open " + fname + " for output");
                }
                m_OStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
            }
        }
        return *m_OStream; 
    }

    CNcbiIstream& GetIStream(string* fname = NULL, 
                             EStdOutErrStorageType* type = NULL) 
    {
        if (!m_IStream.get()) {
            _ASSERT(!m_OStream.get());
            auto_ptr<IReader> reader(
                       new CStringOrBlobStorageReader(*m_Data,
                                                      *m_Storage,
                                                      m_DataSize,
                                                      IBlobStorage::eLockWait)
                       );
            m_IStream.reset(new CRStream(reader.release(),
                                         0,0,CRWStreambuf::fOwnReader
                                           | CRWStreambuf::fLogExceptions));
            m_IStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
            string name;
            int tmp = (int)eBlobStorage;
            static string msg;
            try {
                if (m_IStream->good())
                    *m_IStream >> tmp;
                if (m_IStream->good())
                    ReadStrWithLen(*m_IStream, name);
            } catch (...) {
                if (!m_IStream->eof()) {
                    msg = "The Blob data does not match a remote app data";
                    ERR_POST_X(1, msg);
                    m_IStream.reset(new CNcbiIstrstream(msg.c_str()));
                }
                return *m_IStream;
            }
            
            if (fname) *fname = name;
            if (type) *type = (EStdOutErrStorageType)tmp;
            if (!name.empty() && (EStdOutErrStorageType)tmp == eLocalFile) {
                auto_ptr<CNcbiIstream> fstr(new CNcbiIfstream(name.c_str()));
                if (fstr->good()) {
                    m_IStream.reset(fstr.release());
                    m_IStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
                } else {
                    msg = "Can not open " + name + " of input";                   
                    ERR_POST_X(2, msg);
                    m_IStream.reset(new CNcbiIstrstream(msg.c_str()));
                }
            }
        }
        return *m_IStream; 
    }
    void Reset()
    {
        m_IStream.reset();
        m_OStream.reset();
    }
private:
    IBlobStorage* m_Storage;
    auto_ptr<CNcbiIstream> m_IStream;
    auto_ptr<CNcbiOstream> m_OStream;
    string* m_Data;
    size_t* m_DataSize;
};
//////////////////////////////////////////////////////////////////////////////
//

class CRemoteAppRequestMB_Impl : public IRemoteAppRequest_Impl
{
public:
    explicit CRemoteAppRequestMB_Impl(IBlobStorage* storage, size_t max_inline_size) 
        : IRemoteAppRequest_Impl(storage),
          m_StdInDataSize(0),
          m_StorageType(eBlobStorage),
          m_ExlusiveMode(false),
          m_MaxInlineSize(max_inline_size)
    {
        m_StdIn.reset(new CBlobStreamHelper(GetInBlob(), 
                                            m_InBlobIdOrData,
                                            m_StdInDataSize));
    }

    ~CRemoteAppRequestMB_Impl() 
    {
        try {
            Reset();
        } NCBI_CATCH_ALL_X(15, "CRemoteAppRequestMB_Impl::~CRemoteAppRequestMB_Impl()");

    }

    CNcbiOstream& GetStdInForWrite() 
    { 
        return m_StdIn->GetOStream("", eBlobStorage, m_MaxInlineSize);
    }
    CNcbiIstream& GetStdInForRead() 
    { 
        return m_StdIn->GetIStream();
    }

    
    void SetExclusiveMode(bool on_off) { m_ExlusiveMode = on_off; }
    bool IsExclusiveMode() const { return m_ExlusiveMode; }


    void SetStdOutErrFileNames(const string& stdout_fname,
                               const string& stderr_fname,
                               EStdOutErrStorageType type)
    {
        m_StdOutFileName = stdout_fname;
        m_StdErrFileName = stderr_fname;         
        m_StorageType = type;
    }

    const string& GetStdOutFileName() const { return m_StdOutFileName; }
    const string& GetStdErrFileName() const { return m_StdErrFileName; }
    EStdOutErrStorageType GetStdOutErrStorageType() const
    { return m_StorageType; }

    const string& GetInBlobIdOrData() const { return m_InBlobIdOrData; }

    void SetMaxInlineSize(size_t max_inline_size) { m_MaxInlineSize = max_inline_size; }

    void Serialize(CNcbiOstream& os);
    void Deserialize(CNcbiIstream& is);

    void Reset();

        
private:
    
    size_t                   m_StdInDataSize;
    mutable auto_ptr<CBlobStreamHelper>   m_StdIn;

    string m_InBlobIdOrData;

    string m_TmpDirName;
    string m_StdErrFileName;
    string m_StdOutFileName;
    EStdOutErrStorageType m_StorageType;
    bool m_ExlusiveMode;
    size_t m_MaxInlineSize;

    bool x_CopyLocalFile(const string& old_fname, string& new_fname);

};



void CRemoteAppRequestMB_Impl::Serialize(CNcbiOstream& os)
{
    m_StdIn->Reset();
    typedef map<string,string> TFmap;
    TFmap file_map;
    ITERATE(TFiles, it, GetFileNames()) {
        const string& fname = it->first;
        if (it->second == IRemoteAppRequest::eLocalFS) {
            file_map[fname] = kLocalFSSign;
            continue;
        }
        CFile file(fname);
        string blobid;
        if (!file.Exists()) {
            LOG_POST_X(3, Warning << "File :\"" << fname << "\" does not exist.");
            continue;
        }
        if (NStr::Find(GetCmdLine(), fname) == NPOS) {
            LOG_POST_X(4, Warning << "File :\"" << fname << "\" is not found in cmdline. Skipping.");
            continue;
        }
        
        CNcbiIfstream inf(fname.c_str());
        if (inf.good()) {
            CNcbiOstream& of = GetInBlob().CreateOStream(blobid);
            of << inf.rdbuf();
            GetInBlob().Reset();
            file_map[fname] = blobid;
        }
    }

    GetInBlob().Reset();

    WriteStrWithLen(os, GetCmdLine());
    WriteStrWithLen(os, m_InBlobIdOrData);

    os << file_map.size() << ' ';
    ITERATE(TFmap, itf, file_map) {
        WriteStrWithLen(os, itf->first);
        WriteStrWithLen(os, itf->second);
    }
    WriteStrWithLen(os, m_StdOutFileName);
    WriteStrWithLen(os, m_StdErrFileName);
    os << (int)m_StorageType << " ";
    os << GetAppRunTimeout() << " ";
    os << (int)m_ExlusiveMode;
    Reset();
}

static void s_ReplaceArg( vector<string>& args, const string& old_fname,
                          const string& new_fname)
{
    for(vector<string>::iterator it = args.begin();
        it != args.end(); ++it) {
        string& arg = *it;
        SIZE_TYPE pos = NStr::Find(arg, old_fname);
        if (pos == NPOS)
            return;
        if ( (pos == 0 || !isalnum((unsigned char)arg[pos-1]) )
             && pos + old_fname.size() == arg.size())
            arg = NStr::Replace(arg, old_fname, new_fname);
    }
}

void CRemoteAppRequestMB_Impl::Deserialize(CNcbiIstream& is)
{
    Reset();
    
    string cmdline; ReadStrWithLen(is,cmdline); SetCmdLine(cmdline);
    ReadStrWithLen(is,m_InBlobIdOrData);

    int fcount = 0;
    vector<string> args;
    if (!is.good()) return;
    is >> fcount;
    if ( fcount > 0 )
        TokenizeCmdLine(GetCmdLine(), args);


    for( int i = 0; i < fcount; ++i) {
        if ( i == 0 )
            x_CreateWDir();

        string blobid, fname;
        ReadStrWithLen(is, fname);
        ReadStrWithLen(is, blobid);
        if (!is.good()) return;
        if (blobid == kLocalFSSign) {
            string nfname;
            if ( x_CopyLocalFile(fname, nfname) ) 
                s_ReplaceArg(args, fname, nfname);
        } else {
            string nfname = GetWorkingDir() + CDirEntry::GetPathSeparator() 
                + blobid;
            CNcbiOfstream of(nfname.c_str());
            if (of.good()) {
                CNcbiIstream& is = GetInBlob().GetIStream(blobid);
                of << is.rdbuf();
                GetInBlob().Reset();
                s_ReplaceArg(args, fname, nfname);
            }
        }
    }
    if ( fcount > 0 ) {
        SetCmdLine(JoinCmdLine(args));
    }

    ReadStrWithLen(is, m_StdOutFileName);
    ReadStrWithLen(is, m_StdErrFileName);
    if (!is.good()) return;
    int tmp;
    is >> tmp;
    m_StorageType = (EStdOutErrStorageType)tmp;
    if (!is.good()) return;
    is >> tmp; SetAppRunTimeout(tmp); 
    if (!is.good()) return;
    is >> tmp;
    m_ExlusiveMode = (bool)tmp;

}

bool CRemoteAppRequestMB_Impl::x_CopyLocalFile(const string& old_fname, 
                                               string& new_fname)
{
    return false;
}

void CRemoteAppRequestMB_Impl::Reset()
{
    IRemoteAppRequest_Impl::Reset();
    m_StdIn->Reset();
    m_InBlobIdOrData = "";
    m_StdInDataSize = 0;
    m_ExlusiveMode = false;
}

CRemoteAppRequestMB::
CRemoteAppRequestMB(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppRequestMB_Impl(factory.CreateInstance(),kMaxBlobInlineSize))
{
}

CRemoteAppRequestMB::~CRemoteAppRequestMB()
{
}

void CRemoteAppRequestMB::AddFileForTransfer(const string& fname, ETrasferType tt)
{
    m_Impl->AddFileForTransfer(fname, tt);
}

void CRemoteAppRequestMB::Send(CNcbiOstream& os)
{
    m_Impl->Serialize(os);    
}

/*
void CRemoteAppRequestMB::RequestExclusiveMode() 
{
    m_Impl->SetExclusiveMode(true);
}
*/
CNcbiOstream& CRemoteAppRequestMB::GetStdIn()
{
    return m_Impl->GetStdInForWrite();
}
void CRemoteAppRequestMB::SetCmdLine(const string& cmdline)
{
    m_Impl->SetCmdLine(cmdline);
}
void CRemoteAppRequestMB::SetAppRunTimeout(unsigned int sec)
{
    m_Impl->SetAppRunTimeout(sec);
}

void CRemoteAppRequestMB::SetStdOutErrFileNames(const string& stdout_fname,
                                                const string& stderr_fname,
                                                EStdOutErrStorageType storage_type)
{
    m_Impl->SetStdOutErrFileNames(stdout_fname,stderr_fname,storage_type);
}

void CRemoteAppRequestMB::SetMaxInputSize(size_t max_input_size)
{
    m_Impl->SetMaxInlineSize(max_input_size);
}

CRemoteAppRequestMB_Executer::
CRemoteAppRequestMB_Executer(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppRequestMB_Impl(factory.CreateInstance(),kMaxBlobInlineSize))
{
}
CRemoteAppRequestMB_Executer::~CRemoteAppRequestMB_Executer()
{
}

CNcbiIstream& CRemoteAppRequestMB_Executer::GetStdIn()
{
    return m_Impl->GetStdInForRead();
}
const string& CRemoteAppRequestMB_Executer::GetCmdLine() const 
{
    return m_Impl->GetCmdLine();
}
unsigned int CRemoteAppRequestMB_Executer::GetAppRunTimeout() const 
{
    return m_Impl->GetAppRunTimeout();
}

const string& CRemoteAppRequestMB_Executer::GetWorkingDir() const 
{ 
    return m_Impl->GetWorkingDir(); 
}

const string& CRemoteAppRequestMB_Executer::GetStdOutFileName() const 
{ 
    return m_Impl->GetStdOutFileName(); 
}
const string& CRemoteAppRequestMB_Executer::GetStdErrFileName() const 
{ 
    return m_Impl->GetStdErrFileName();
}
/*
bool CRemoteAppRequest_Executer::IsExclusiveModeRequested() const
{
    return m_Impl->IsExclusiveMode();
}
*/
EStdOutErrStorageType CRemoteAppRequestMB_Executer::GetStdOutErrStorageType() const
{ 
    return m_Impl->GetStdOutErrStorageType(); 
}

const string& CRemoteAppRequestMB_Executer::GetInBlobIdOrData() const
{
    return m_Impl->GetInBlobIdOrData();
}


void CRemoteAppRequestMB_Executer::Receive(CNcbiIstream& is)
{
    m_Impl->Deserialize(is);
}

void CRemoteAppRequestMB_Executer::Reset()
{
    m_Impl->Reset();
}

void CRemoteAppRequestMB_Executer::Log(const string& prefix)
{
    if (!m_Impl->GetInBlobIdOrData().empty()) {
        LOG_POST_X(5, prefix
                   << " Input data: " << m_Impl->GetInBlobIdOrData());
    }
    LOG_POST_X(6, prefix
               << " Args: " << m_Impl->GetCmdLine());
    if (!m_Impl->GetStdOutFileName().empty()) {
        LOG_POST_X(7, prefix
                   << " StdOutFile: " << m_Impl->GetStdOutFileName());
    }
    if (!m_Impl->GetStdErrFileName().empty()) {
        LOG_POST_X(8, prefix
                   << " StdErrFile: " << m_Impl->GetStdErrFileName());
    }
}


//////////////////////////////////////////////////////////////////////////////
//

class CRemoteAppResultMB_Impl : public IRemoteAppResult_Impl
{
public:
    CRemoteAppResultMB_Impl(IBlobStorage* out, IBlobStorage* err, size_t max_inline_size)
        : IRemoteAppResult_Impl(out, err),
          m_OutBlobSize(0), m_ErrBlobSize(0),
          m_StorageType(eBlobStorage),
          m_MaxInlineSize(max_inline_size)
    {
        m_StdOut.reset(new CBlobStreamHelper(GetOutBlob(),
                                             m_OutBlobIdOrData,
                                             m_OutBlobSize));
        m_StdErr.reset(new CBlobStreamHelper(GetErrBlob(),
                                             m_ErrBlobIdOrData,
                                             m_ErrBlobSize));
    }
    ~CRemoteAppResultMB_Impl()
    {
        try {
            Reset();
        } NCBI_CATCH_ALL_X(16, "CRemoteAppResultMB_Impl::~CRemoteAppResultMB_Impl()");
    }    

    CNcbiOstream& GetStdOutForWrite() 
    { 
        return m_StdOut->GetOStream(m_StdOutFileName,m_StorageType, m_MaxInlineSize);
    }
    CNcbiIstream& GetStdOutForRead() 
    { 
        return m_StdOut->GetIStream(&m_StdOutFileName,&m_StorageType);
    }

    CNcbiOstream& GetStdErrForWrite() 
    { 
        return m_StdErr->GetOStream(m_StdErrFileName,m_StorageType, m_MaxInlineSize);
    }
    CNcbiIstream& GetStdErrForRead() 
    { 
        return m_StdErr->GetIStream(&m_StdErrFileName,&m_StorageType);
    }


    void Serialize(CNcbiOstream& os);
    void Deserialize(CNcbiIstream& is);

    void Reset();

    void SetStdOutErrFileNames(const string& stdout_fname,
                               const string& stderr_fname,
                               EStdOutErrStorageType type)
    {
        m_StdOutFileName = stdout_fname;
        m_StdErrFileName = stderr_fname;         
        m_StorageType = type;
    }
    const string& GetStdOutFileName() const { return m_StdOutFileName; }
    const string& GetStdErrFileName() const { return m_StdErrFileName; }
    EStdOutErrStorageType GetStdOutErrStorageType() const
    { return m_StorageType; }
    const string& GetOutBlobIdOrData() const { return m_OutBlobIdOrData; }
    const string& GetErrBlobIdOrData() const { return m_ErrBlobIdOrData; }

    void SetMaxInlineSize(size_t max_inline_size) { m_MaxInlineSize = max_inline_size; }

private:
    string m_OutBlobIdOrData;
    size_t m_OutBlobSize;
    mutable auto_ptr<CBlobStreamHelper> m_StdOut;
    string m_StdOutFileName;

    string m_ErrBlobIdOrData;
    size_t m_ErrBlobSize;
    mutable auto_ptr<CBlobStreamHelper> m_StdErr;
    string m_StdErrFileName;
    EStdOutErrStorageType m_StorageType;
    size_t m_MaxInlineSize;

};

void CRemoteAppResultMB_Impl::Serialize(CNcbiOstream& os)
{
    m_StdOut->Reset();
    m_StdErr->Reset();
    WriteStrWithLen(os, m_OutBlobIdOrData);
    WriteStrWithLen(os, m_ErrBlobIdOrData);
    os << GetRetCode();
}
void CRemoteAppResultMB_Impl::Deserialize(CNcbiIstream& is)
{
    Reset();
    ReadStrWithLen(is, m_OutBlobIdOrData);
    ReadStrWithLen(is, m_ErrBlobIdOrData);
    int ret = -1; is >> ret; SetRetCode(ret);
}

void CRemoteAppResultMB_Impl::Reset()
{
    IRemoteAppResult_Impl::Reset();
    m_OutBlobIdOrData = "";
    m_OutBlobSize = 0;
    m_StdOut->Reset();

    m_ErrBlobIdOrData = "";
    m_ErrBlobSize = 0;
    m_StdErr->Reset();

    m_StdOutFileName = "";
    m_StdErrFileName = "";
    m_StorageType = eBlobStorage;
}



CRemoteAppResultMB_Executer::
CRemoteAppResultMB_Executer(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppResultMB_Impl(factory.CreateInstance(), 
                                         factory.CreateInstance(),
                                         kMaxBlobInlineSize))
{
}
CRemoteAppResultMB_Executer::~CRemoteAppResultMB_Executer()
{
}

CNcbiOstream& CRemoteAppResultMB_Executer::GetStdOut()
{
    return m_Impl->GetStdOutForWrite();
}
CNcbiOstream& CRemoteAppResultMB_Executer::GetStdErr()
{
    return m_Impl->GetStdErrForWrite();
}

void CRemoteAppResultMB_Executer::SetRetCode(int ret_code)
{
    m_Impl->SetRetCode(ret_code);
}
const string& CRemoteAppResultMB_Executer::GetOutBlobIdOrData() const
{
    return m_Impl->GetOutBlobIdOrData();
}
const string& CRemoteAppResultMB_Executer::GetErrBlobIdOrData() const
{
    return m_Impl->GetErrBlobIdOrData();
}

void CRemoteAppResultMB_Executer::SetStdOutErrFileNames(const string& stdout_fname,
                                                        const string& stderr_fname,
                                                        EStdOutErrStorageType type)
{
    m_Impl->SetStdOutErrFileNames(stdout_fname, stderr_fname, type);
}

void CRemoteAppResultMB_Executer::Send(CNcbiOstream& os)
{
    m_Impl->Serialize(os);
}
void CRemoteAppResultMB_Executer::Reset()
{
    m_Impl->Reset();
}

void CRemoteAppResultMB_Executer::Log(const string& prefix)
{
    if (!m_Impl->GetOutBlobIdOrData().empty()) {
        LOG_POST_X(9, prefix
                   << " Out data: " << m_Impl->GetOutBlobIdOrData());
    }
    if (!m_Impl->GetErrBlobIdOrData().empty()) {
        LOG_POST_X(10, prefix
                   << " Err data: " << m_Impl->GetErrBlobIdOrData());
    }
}

void CRemoteAppResultMB_Executer::SetMaxOutputSize(size_t max_output_size)
{
    m_Impl->SetMaxInlineSize(max_output_size);
}


CRemoteAppResultMB::
CRemoteAppResultMB(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppResultMB_Impl(factory.CreateInstance(), 
                                         factory.CreateInstance(),
                                         kMaxBlobInlineSize))
{
}
CRemoteAppResultMB::~CRemoteAppResultMB()
{
}

CNcbiIstream& CRemoteAppResultMB::GetStdOut()
{
    return m_Impl->GetStdOutForRead();
}
CNcbiIstream& CRemoteAppResultMB::GetStdErr()
{
    return m_Impl->GetStdErrForRead();
}
int CRemoteAppResultMB::GetRetCode() const
{
    return m_Impl->GetRetCode();
}

const string& CRemoteAppResultMB::GetStdOutFileName() const 
{ 
    return m_Impl->GetStdOutFileName(); 
}
const string& CRemoteAppResultMB::GetStdErrFileName() const 
{ 
    return m_Impl->GetStdErrFileName();
}
EStdOutErrStorageType CRemoteAppResultMB::GetStdOutErrStorageType() const
{ 
    return m_Impl->GetStdOutErrStorageType(); 
}

void CRemoteAppResultMB::Receive(CNcbiIstream& is)
{
    m_Impl->Deserialize(is);
}


END_NCBI_SCOPE
