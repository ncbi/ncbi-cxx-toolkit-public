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

#include <connect/services/remote_job.hpp>

BEGIN_NCBI_SCOPE


static inline CNcbiOstream& s_Write(CNcbiOstream& os, const string& str)
{
    os << str.size() << ' ' << str;
    return os;
}

static inline CNcbiIstream& s_Read(CNcbiIstream& is, string& str)
{
    string::size_type len;
    is >> len;
    vector<char> buf(len+1);
    is.read(&buf[0], len+1);
    str.assign(buf.begin()+1, buf.end());
    return is;
}

//////////////////////////////////////////////////////////////////////////////
//

class CRemoteJobRequest_Impl
{
public:
    explicit CRemoteJobRequest_Impl(IBlobStorageFactory& factory)
        : m_InBlob(factory.CreateInstance()), m_StdIn(NULL)
    {}

    ~CRemoteJobRequest_Impl() 
    {
        m_InBlob->Reset();
    }
    CNcbiOstream& GetStdInForWrite() 
    { 
        if (!m_StdIn) {
            _ASSERT(m_InBlobKey.empty());
            m_StdIn = &m_InBlob->CreateOStream(m_InBlobKey);
        }
        return *m_StdIn; 
    }
    CNcbiIstream& GetStdInForRead() 
    { 
        _ASSERT(!m_InBlobKey.empty());
        return m_InBlob->GetIStream(m_InBlobKey); 
    }
    void SetCmdLine(const string& cmdline) { m_CmdLine = cmdline; }
    const string& GetCmdLine() const { return m_CmdLine; }

    void AddFileForTransfer(const string& fname) 
    { 
        m_Files.insert(fname); 
    }

    void Serialize(CNcbiOstream& os) const;
    void Deserialize(CNcbiIstream& is);

    void CleanUp();
    void Reset();

private:
    static CAtomicCounter sm_DirCounter;
    static string sm_TmpDirPath;
    
    auto_ptr<IBlobStorage> m_InBlob;
    string m_InBlobKey;
    CNcbiOstream* m_StdIn;
    string m_CmdLine;

    string m_TmpDirName;
    typedef set<string> TFiles;
    TFiles m_Files;
};

CAtomicCounter CRemoteJobRequest_Impl::sm_DirCounter;
string CRemoteJobRequest_Impl::sm_TmpDirPath = ".";


void CRemoteJobRequest_Impl::Serialize(CNcbiOstream& os) const
{
    m_InBlob->Reset();
    s_Write(os, m_InBlobKey);
    s_Write(os, m_CmdLine);
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
    os << file_map.size() << ' ';
    ITERATE(TFmap, itf, file_map) {
        s_Write(os, itf->first);
        s_Write(os, itf->second);
    }
}
void CRemoteJobRequest_Impl::Deserialize(CNcbiIstream& is)
{
    Reset();
    s_Read(is, m_InBlobKey);
    s_Read(is, m_CmdLine);
    int fcount = 0;
    vector<string> args;
    is >> fcount;
    if ( fcount > 0 )
        NStr::Tokenize(m_CmdLine, " ", args);
    for( int i = 0; i < fcount; ++i) {
        if (i == 0) {
            m_TmpDirName = sm_TmpDirPath + CDirEntry::GetPathSeparator() +
                NStr::UIntToString(sm_DirCounter.Add(1));
            CDir(m_TmpDirName).CreatePath();           
        }
        string blobid, fname;
        s_Read(is, blobid);
        s_Read(is, fname);
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
}

void CRemoteJobRequest_Impl::CleanUp()
{
    if (!m_TmpDirName.empty()) {
        CDir(m_TmpDirName).Remove();
        m_TmpDirName = "";
    }
}

void CRemoteJobRequest_Impl::Reset()
{
    m_InBlob->Reset();
    m_InBlobKey = "";
    m_CmdLine = "";
    m_Files.clear();
    m_StdIn = NULL;
}
CRemoteJobRequest_Submitter::
CRemoteJobRequest_Submitter(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteJobRequest_Impl(factory))
{
}

CRemoteJobRequest_Submitter::~CRemoteJobRequest_Submitter()
{
}

void CRemoteJobRequest_Submitter::AddFileForTransfer(const string& fname)
{
    m_Impl->AddFileForTransfer(fname);
}

void CRemoteJobRequest_Submitter::Send(CNcbiOstream& os)
{
    m_Impl->Serialize(os);    
    m_Impl->Reset();
}


CNcbiOstream& CRemoteJobRequest_Submitter::GetStdIn()
{
    return m_Impl->GetStdInForWrite();
}
void CRemoteJobRequest_Submitter::SetCmdLine(const string& cmdline)
{
    m_Impl->SetCmdLine(cmdline);
}

CRemoteJobRequest_Executer::
CRemoteJobRequest_Executer(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteJobRequest_Impl(factory))
{
}
CRemoteJobRequest_Executer::~CRemoteJobRequest_Executer()
{
}

CNcbiIstream& CRemoteJobRequest_Executer::GetStdIn()
{
    return m_Impl->GetStdInForRead();
}
const string& CRemoteJobRequest_Executer::GetCmdLine() const 
{
    return m_Impl->GetCmdLine();
}

void CRemoteJobRequest_Executer::Receive(CNcbiIstream& is)
{
    m_Impl->Deserialize(is);
}

void CRemoteJobRequest_Executer::CleanUp()
{
    m_Impl->CleanUp();
}

//////////////////////////////////////////////////////////////////////////////
//

class CRemoteJobResult_Impl;

class CRemoteJobResult_Impl
{
public:
    explicit CRemoteJobResult_Impl(IBlobStorageFactory& factory)
        : m_OutBlob(factory.CreateInstance()), m_StdOut(NULL), 
          m_ErrBlob(factory.CreateInstance()), m_StdErr(NULL),
          m_RetCode(-1)
    {}
    ~CRemoteJobResult_Impl()
    {
        m_OutBlob->Reset();
        m_ErrBlob->Reset();
    }    

    CNcbiOstream& GetStdOutForWrite() 
    { 
        if (!m_StdOut) {
            _ASSERT(m_OutBlobKey.empty());
            m_StdOut = &m_OutBlob->CreateOStream(m_OutBlobKey);
        }
        return *m_StdOut; 
    }
    CNcbiIstream& GetStdOutForRead() 
    { 
        _ASSERT(!m_OutBlobKey.empty());
        return m_OutBlob->GetIStream(m_OutBlobKey); 
    }

    CNcbiOstream& GetStdErrForWrite() { 
        if (!m_StdErr) {
            _ASSERT(m_ErrBlobKey.empty());
            m_StdErr = &m_ErrBlob->CreateOStream(m_ErrBlobKey);
        }
        return *m_StdErr; 
    }
    CNcbiIstream& GetStdErrForRead() 
    { 
        _ASSERT(!m_ErrBlobKey.empty());
        return m_ErrBlob->GetIStream(m_ErrBlobKey); 
    }

    int GetRetCode() const { return m_RetCode; }
    void SetRetCode(int ret_code) { m_RetCode = ret_code; }

    void Serialize(CNcbiOstream& os) const;
    void Deserialize(CNcbiIstream& is);

private:
    auto_ptr<IBlobStorage> m_OutBlob;
    string m_OutBlobKey;
    CNcbiOstream* m_StdOut;
    auto_ptr<IBlobStorage> m_ErrBlob;
    string m_ErrBlobKey;
    CNcbiOstream* m_StdErr;
    int m_RetCode;

};

void CRemoteJobResult_Impl::Serialize(CNcbiOstream& os) const
{
    m_OutBlob->Reset();
    s_Write(os, m_OutBlobKey);
    m_ErrBlob->Reset();
    s_Write(os, m_ErrBlobKey);
    os << m_RetCode;
}
void CRemoteJobResult_Impl::Deserialize(CNcbiIstream& is)
{
    m_OutBlob->Reset();
    s_Read(is, m_OutBlobKey);
    m_StdOut = NULL;
    m_ErrBlob->Reset();
    s_Read(is, m_ErrBlobKey);
    m_StdErr = NULL;
    m_RetCode = -1;
    is >> m_RetCode;
}



CRemoteJobResult_Executer::
CRemoteJobResult_Executer(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteJobResult_Impl(factory))
{
}
CRemoteJobResult_Executer::~CRemoteJobResult_Executer()
{
}

CNcbiOstream& CRemoteJobResult_Executer::GetStdOut()
{
    return m_Impl->GetStdOutForWrite();
}
CNcbiOstream& CRemoteJobResult_Executer::GetStdErr()
{
    return m_Impl->GetStdErrForWrite();
}

void CRemoteJobResult_Executer::SetRetCode(int ret_code)
{
    m_Impl->SetRetCode(ret_code);
}

void CRemoteJobResult_Executer::Send(CNcbiOstream& os)
{
    m_Impl->Serialize(os);
}


CRemoteJobResult_Submitter::
CRemoteJobResult_Submitter(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteJobResult_Impl(factory))
{
}
CRemoteJobResult_Submitter::~CRemoteJobResult_Submitter()
{
}

CNcbiIstream& CRemoteJobResult_Submitter::GetStdOut()
{
    return m_Impl->GetStdOutForRead();
}
CNcbiIstream& CRemoteJobResult_Submitter::GetStdErr()
{
    return m_Impl->GetStdErrForRead();
}
int CRemoteJobResult_Submitter::GetRetCode() const
{
    return m_Impl->GetRetCode();
}

void CRemoteJobResult_Submitter::Receive(CNcbiIstream& is)
{
    m_Impl->Deserialize(is);
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.1  2006/03/07 17:17:12  didenko
 * Added facility for running external applications throu NetSchedule service
 *
 * ===========================================================================
 */
 
