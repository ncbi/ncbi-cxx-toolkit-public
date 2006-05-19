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

#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/remote_job.hpp>

BEGIN_NCBI_SCOPE

const size_t kMaxBlobInlineSize = 500;
//////////////////////////////////////////////////////////////////////////////
//
static inline CNcbiOstream& s_Write(CNcbiOstream& os, const string& str)
{
    os << str.size() << ' ' << str;
    return os;
}

static inline CNcbiIstream& s_Read(CNcbiIstream& is, string& str)
{
    string::size_type len;
    if (!is.good()) return is;
    is >> len;
    if (!is.good()) return is;
    vector<char> buf(len+1);
    is.read(&buf[0], len+1);
    str.assign(buf.begin()+1, buf.end());
    return is;
}

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
    }

    CNcbiOstream& GetOStream(const string& fname = "", 
                             EStdOutErrStorageType type = eBlobStorage)
    {
        if (!m_OStream.get()) {
            _ASSERT(!m_IStream.get());
            auto_ptr<IWriter> writer( 
                        new CStringOrBlobStorageWriter(kMaxBlobInlineSize,
                                                       *m_Storage,
                                                       *m_Data)
                        );
            m_OStream.reset(new CWStream(writer.release(), 
                                         0, 0, CRWStreambuf::fOwnWriter));
            *m_OStream << (int) type << " ";
            s_Write(*m_OStream, fname);
            if (!fname.empty() && type == eLocalFile) {                   
                m_OStream.reset(new CNcbiOfstream(fname.c_str()));
                if (!m_OStream->good()) {
                    NCBI_THROW(CFileException, eRelativePath, 
                               "Cannot open " + fname + " for output");
                }
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
                                         0,0,CRWStreambuf::fOwnReader));
            string name;
            int tmp = (int)eBlobStorage;
            if (m_IStream->good())
                *m_IStream >> tmp;
            if (m_IStream->good())
                s_Read(*m_IStream, name);
            if (fname) *fname = name;
            if (type) *type = (EStdOutErrStorageType)tmp;
            if (!name.empty() && (EStdOutErrStorageType)tmp == eLocalFile) {
                auto_ptr<CNcbiIstream> fstr(new CNcbiIfstream(name.c_str()));
                if (fstr->good()) {
                    m_IStream.reset(fstr.release());
                } else {
                    static string msg;
                    msg = "Can not open " + name + " of input";                   
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

class CRemoteAppRequest_Impl
{
public:
    explicit CRemoteAppRequest_Impl(IBlobStorageFactory& factory)
        : m_InBlob(factory.CreateInstance()), m_StdInDataSize(0),
          m_StorageType(eBlobStorage), m_AppRunTimeout(0),
          m_ExlusiveMode(false)
    {
        m_StdIn.reset(new CBlobStreamHelper(*m_InBlob, 
                                            m_InBlobIdOrData,
                                            m_StdInDataSize));
    }

    ~CRemoteAppRequest_Impl() 
    {
    }

    CNcbiOstream& GetStdInForWrite() 
    { 
        return m_StdIn->GetOStream();
    }
    CNcbiIstream& GetStdInForRead() 
    { 
        return m_StdIn->GetIStream();
    }

    void SetCmdLine(const string& cmdline) { m_CmdLine = cmdline; }
    const string& GetCmdLine() const { return m_CmdLine; }

    void SetAppRunTimeout(unsigned int sec) { m_AppRunTimeout = sec; }
    unsigned int GetAppRunTimeout() const { return m_AppRunTimeout; }
    
    void SetExclusiveMode(bool on_off) { m_ExlusiveMode = on_off; }
    bool IsExclusiveMode() const { return m_ExlusiveMode; }

    void AddFileForTransfer(const string& fname) 
    { 
        m_Files.insert(fname); 
    }
    const string& GetWorkingDir() const { return m_TmpDirName; }

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

    void Serialize(CNcbiOstream& os);
    void Deserialize(CNcbiIstream& is);

    void Reset();

private:
    static CAtomicCounter sm_DirCounter;
    static string sm_TmpDirPath;

    void x_CreateWDir();
    void x_RemoveWDir();
    
    auto_ptr<IBlobStorage>   m_InBlob;
    size_t                   m_StdInDataSize;
    mutable auto_ptr<CBlobStreamHelper>   m_StdIn;

    string m_InBlobIdOrData;
    string m_CmdLine;

    string m_TmpDirName;
    typedef set<string> TFiles;
    TFiles m_Files;
    string m_StdErrFileName;
    string m_StdOutFileName;
    EStdOutErrStorageType m_StorageType;
    unsigned int m_AppRunTimeout;
    bool m_ExlusiveMode;
};

CAtomicCounter CRemoteAppRequest_Impl::sm_DirCounter;
string CRemoteAppRequest_Impl::sm_TmpDirPath = ".";


void CRemoteAppRequest_Impl::Serialize(CNcbiOstream& os)
{
    m_StdIn->Reset();
    typedef map<string,string> TFmap;
    TFmap file_map;
    ITERATE(TFiles, it, m_Files) {
        const string& fname = *it;
        CFile file(fname);
        string blobid;
        if (file.Exists()) {
            CNcbiIfstream inf(fname.c_str());
            if (inf.good()) {
                CNcbiOstream& of = m_InBlob->CreateOStream(blobid);
                of << inf.rdbuf();
                m_InBlob->Reset();
                file_map[blobid] = fname;
            }
        }
    }

    m_InBlob->Reset();

    s_Write(os, m_CmdLine);
    s_Write(os, m_InBlobIdOrData);

    os << file_map.size() << ' ';
    ITERATE(TFmap, itf, file_map) {
        s_Write(os, itf->first);
        s_Write(os, itf->second);
    }
    s_Write(os, m_StdOutFileName);
    s_Write(os, m_StdErrFileName);
    os << (int)m_StorageType << " ";
    os << m_AppRunTimeout << " ";
    os << (int)m_ExlusiveMode;
    Reset();
}

void CRemoteAppRequest_Impl::x_CreateWDir()
{
    if( !m_TmpDirName.empty() )
        return;
    m_TmpDirName = sm_TmpDirPath + CDirEntry::GetPathSeparator() +
        NStr::UIntToString(sm_DirCounter.Add(1));
    CDir wdir(m_TmpDirName);
    if (wdir.Exists())
        wdir.Remove();
    CDir(m_TmpDirName).CreatePath();
}

void CRemoteAppRequest_Impl::x_RemoveWDir()
{
    if (m_TmpDirName.empty())
        return;
    CDir dir(m_TmpDirName);
    if (dir.Exists())
        dir.Remove();
    m_TmpDirName = "";
}

void CRemoteAppRequest_Impl::Deserialize(CNcbiIstream& is)
{
    Reset();
    
    s_Read(is,m_CmdLine);
    s_Read(is,m_InBlobIdOrData);

    int fcount = 0;
    vector<string> args;
    if (!is.good()) return;
    is >> fcount;
    if ( fcount > 0 )
        NStr::Tokenize(m_CmdLine, " ", args);


    for( int i = 0; i < fcount; ++i) {
        if ( i == 0 )
            x_CreateWDir();

        string blobid, fname;
        s_Read(is, blobid);
        s_Read(is, fname);
        if (!is.good()) return;
        CFile file(fname);
        string nfname = m_TmpDirName + CDirEntry::GetPathSeparator() 
            + file.GetName();
        CNcbiOfstream of(nfname.c_str());
        if (of.good()) {
            CNcbiIstream& is = m_InBlob->GetIStream(blobid);
            of << is.rdbuf();
            m_InBlob->Reset();
            for(vector<string>::iterator it = args.begin();
                it != args.end(); ++it) {
                string& arg = *it;
                SIZE_TYPE pos = NStr::Find(arg, fname);
                if (pos == NPOS)
                    continue;
                if ( (pos == 0 || !isalnum((unsigned char)arg[pos-1]) )
                     && pos + fname.size() == arg.size())
                    arg = NStr::Replace(arg, fname, nfname);
            }
        }
    }
    if ( fcount > 0 ) {
        m_CmdLine = "";
        for(vector<string>::const_iterator it = args.begin();
            it != args.end(); ++it) {
            if (it != args.begin())
                m_CmdLine += ' ';
            m_CmdLine += *it;
        }
    }
    s_Read(is, m_StdOutFileName);
    s_Read(is, m_StdErrFileName);
    if (!is.good()) return;
    int tmp;
    is >> tmp;
    m_StorageType = (EStdOutErrStorageType)tmp;
    if (!is.good()) return;
    is >> m_AppRunTimeout;
    if (!is.good()) return;
    is >> tmp;
    m_ExlusiveMode = (bool)tmp;

}

void CRemoteAppRequest_Impl::Reset()
{
    m_StdIn->Reset();
    m_InBlobIdOrData = "";
    m_StdInDataSize = 0;
    m_CmdLine = "";
    m_Files.clear();
    m_AppRunTimeout = 0;
    m_ExlusiveMode = false;
    
    x_RemoveWDir();
}

CRemoteAppRequest::
CRemoteAppRequest(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppRequest_Impl(factory))
{
}

CRemoteAppRequest::~CRemoteAppRequest()
{
}

void CRemoteAppRequest::AddFileForTransfer(const string& fname)
{
    m_Impl->AddFileForTransfer(fname);
}

void CRemoteAppRequest::Send(CNcbiOstream& os)
{
    m_Impl->Serialize(os);    
}

void CRemoteAppRequest::RequestExclusiveMode() 
{
    m_Impl->SetExclusiveMode(true);
}

CNcbiOstream& CRemoteAppRequest::GetStdIn()
{
    return m_Impl->GetStdInForWrite();
}
void CRemoteAppRequest::SetCmdLine(const string& cmdline)
{
    m_Impl->SetCmdLine(cmdline);
}
void CRemoteAppRequest::SetAppRunTimeout(unsigned int sec)
{
    m_Impl->SetAppRunTimeout(sec);
}

void CRemoteAppRequest::SetStdOutErrFileNames(const string& stdout_fname,
                                              const string& stderr_fname,
                                              EStdOutErrStorageType storage_type)
{
    m_Impl->SetStdOutErrFileNames(stdout_fname,stderr_fname,storage_type);
}

CRemoteAppRequest_Executer::
CRemoteAppRequest_Executer(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppRequest_Impl(factory))
{
}
CRemoteAppRequest_Executer::~CRemoteAppRequest_Executer()
{
}

CNcbiIstream& CRemoteAppRequest_Executer::GetStdIn()
{
    return m_Impl->GetStdInForRead();
}
const string& CRemoteAppRequest_Executer::GetCmdLine() const 
{
    return m_Impl->GetCmdLine();
}
unsigned int CRemoteAppRequest_Executer::GetAppRunTimeout() const 
{
    return m_Impl->GetAppRunTimeout();
}

const string& CRemoteAppRequest_Executer::GetWorkingDir() const 
{ 
    return m_Impl->GetWorkingDir(); 
}

const string& CRemoteAppRequest_Executer::GetStdOutFileName() const 
{ 
    return m_Impl->GetStdOutFileName(); 
}
const string& CRemoteAppRequest_Executer::GetStdErrFileName() const 
{ 
    return m_Impl->GetStdErrFileName();
}
bool CRemoteAppRequest_Executer::IsExclusiveModeRequested() const
{
    return m_Impl->IsExclusiveMode();
}

EStdOutErrStorageType CRemoteAppRequest_Executer::GetStdOutErrStorageType() const
{ 
    return m_Impl->GetStdOutErrStorageType(); 
}

void CRemoteAppRequest_Executer::Receive(CNcbiIstream& is)
{
    m_Impl->Deserialize(is);
}

void CRemoteAppRequest_Executer::Reset()
{
    m_Impl->Reset();
}

//////////////////////////////////////////////////////////////////////////////
//

class CRemoteAppResult_Impl;

class CRemoteAppResult_Impl
{
public:
    explicit CRemoteAppResult_Impl(IBlobStorageFactory& factory)
        : m_OutBlob(factory.CreateInstance()), m_OutBlobSize(0), 
          m_ErrBlob(factory.CreateInstance()), m_ErrBlobSize(0),
          m_RetCode(-1), m_StorageType(eBlobStorage)
    {
        m_StdOut.reset(new CBlobStreamHelper(*m_OutBlob,
                                             m_OutBlobIdOrData,
                                             m_OutBlobSize));
        m_StdErr.reset(new CBlobStreamHelper(*m_ErrBlob,
                                             m_ErrBlobIdOrData,
                                             m_ErrBlobSize));
    }
    ~CRemoteAppResult_Impl()
    {
    }    

    CNcbiOstream& GetStdOutForWrite() 
    { 
        return m_StdOut->GetOStream(m_StdOutFileName,m_StorageType);
    }
    CNcbiIstream& GetStdOutForRead() 
    { 
        return m_StdOut->GetIStream(&m_StdOutFileName,&m_StorageType);
    }

    CNcbiOstream& GetStdErrForWrite() 
    { 
        return m_StdErr->GetOStream(m_StdErrFileName,m_StorageType);
    }
    CNcbiIstream& GetStdErrForRead() 
    { 
        return m_StdErr->GetIStream(&m_StdErrFileName,&m_StorageType);
    }

    int GetRetCode() const { return m_RetCode; }
    void SetRetCode(int ret_code) { m_RetCode = ret_code; }

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

private:
    auto_ptr<IBlobStorage> m_OutBlob;
    string m_OutBlobIdOrData;
    size_t m_OutBlobSize;
    mutable auto_ptr<CBlobStreamHelper> m_StdOut;
    string m_StdOutFileName;

    auto_ptr<IBlobStorage> m_ErrBlob;
    string m_ErrBlobIdOrData;
    size_t m_ErrBlobSize;
    mutable auto_ptr<CBlobStreamHelper> m_StdErr;
    int m_RetCode;
    string m_StdErrFileName;
    EStdOutErrStorageType m_StorageType;    

};

void CRemoteAppResult_Impl::Serialize(CNcbiOstream& os)
{
    m_StdOut->Reset();
    m_StdErr->Reset();
    s_Write(os, m_OutBlobIdOrData);
    s_Write(os, m_ErrBlobIdOrData);
    os << m_RetCode;
    Reset();
}
void CRemoteAppResult_Impl::Deserialize(CNcbiIstream& is)
{
    Reset();
    s_Read(is, m_OutBlobIdOrData);
    s_Read(is, m_ErrBlobIdOrData);
    m_RetCode = -1;
    is >> m_RetCode;
}

void CRemoteAppResult_Impl::Reset()
{
    m_OutBlobIdOrData = "";
    m_OutBlobSize = 0;
    m_StdOut->Reset();

    m_ErrBlobIdOrData = "";
    m_ErrBlobSize = 0;
    m_StdErr->Reset();
    m_RetCode = -1;

    m_StdOutFileName = "";
    m_StdErrFileName = "";
    m_StorageType = eBlobStorage;
}



CRemoteAppResult_Executer::
CRemoteAppResult_Executer(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppResult_Impl(factory))
{
}
CRemoteAppResult_Executer::~CRemoteAppResult_Executer()
{
}

CNcbiOstream& CRemoteAppResult_Executer::GetStdOut()
{
    return m_Impl->GetStdOutForWrite();
}
CNcbiOstream& CRemoteAppResult_Executer::GetStdErr()
{
    return m_Impl->GetStdErrForWrite();
}

void CRemoteAppResult_Executer::SetRetCode(int ret_code)
{
    m_Impl->SetRetCode(ret_code);
}

void CRemoteAppResult_Executer::SetStdOutErrFileNames(const string& stdout_fname,
                                                      const string& stderr_fname,
                                                      EStdOutErrStorageType type)
{
    m_Impl->SetStdOutErrFileNames(stdout_fname, stderr_fname, type);
}

void CRemoteAppResult_Executer::Send(CNcbiOstream& os)
{
    m_Impl->Serialize(os);
}


CRemoteAppResult::
CRemoteAppResult(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppResult_Impl(factory))
{
}
CRemoteAppResult::~CRemoteAppResult()
{
}

CNcbiIstream& CRemoteAppResult::GetStdOut()
{
    return m_Impl->GetStdOutForRead();
}
CNcbiIstream& CRemoteAppResult::GetStdErr()
{
    return m_Impl->GetStdErrForRead();
}
int CRemoteAppResult::GetRetCode() const
{
    return m_Impl->GetRetCode();
}

const string& CRemoteAppResult::GetStdOutFileName() const 
{ 
    return m_Impl->GetStdOutFileName(); 
}
const string& CRemoteAppResult::GetStdErrFileName() const 
{ 
    return m_Impl->GetStdErrFileName();
}
EStdOutErrStorageType CRemoteAppResult::GetStdOutErrStorageType() const
{ 
    return m_Impl->GetStdOutErrStorageType(); 
}

void CRemoteAppResult::Receive(CNcbiIstream& is)
{
    m_Impl->Deserialize(is);
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.8  2006/05/19 13:37:45  didenko
 * Improved error checking
 *
 * Revision 6.7  2006/05/15 15:26:53  didenko
 * Added support for running exclusive jobs
 *
 * Revision 6.6  2006/05/10 19:54:21  didenko
 * Added JobDelayExpiration method to CWorkerNodeContext class
 * Added keep_alive_period and max_job_run_time parmerter to the config
 * file of remote_app
 *
 * Revision 6.5  2006/05/08 15:16:42  didenko
 * Added support for an optional saving of a remote application's stdout
 * and stderr into files on a local file system
 *
 * Revision 6.4  2006/05/03 20:03:52  didenko
 * Improved exceptions handling
 *
 * Revision 6.3  2006/03/16 15:13:59  didenko
 * Remaned CRemoteJob... to CRemoteApp...
 * + Comments
 *
 * Revision 6.2  2006/03/15 17:30:12  didenko
 * Added ability to use embedded NetSchedule job's storage as a job's 
 * input/output data instead of using it as a NetCache blob key. This reduces 
 * a network traffic and increases job submittion speed.
 *
 * Revision 6.1  2006/03/07 17:17:12  didenko
 * Added facility for running external applications throu NetSchedule service
 *
 * ===========================================================================
 */
 
