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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/remote_app.hpp>
#include <connect/services/error_codes.hpp>

#include <corelib/ncbifile.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/ncbifile.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_Remote

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////
//
inline CNcbiOstream& WriteStrWithLen(CNcbiOstream& os, const string& str)
{
    os << str.size() << ' ' << str;
    return os;
}

inline CNcbiIstream& ReadStrWithLen(CNcbiIstream& is, string& str)
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

CBlobStreamHelper::~CBlobStreamHelper()
{
    try {
        Reset();
    } NCBI_CATCH_ALL_X(14, "CBlobStreamHelper::~CBlobStreamHelper()");
}

CNcbiOstream& CBlobStreamHelper::GetOStream(const string& fname /*= ""*/,
    EStdOutErrStorageType type /*= eBlobStorage*/,
    size_t max_inline_size /*= kMaxBlobInlineSize*/)
{
    if (!m_OStream.get()) {
        _ASSERT(!m_IStream.get());
        m_Writer.reset(new CStringOrBlobStorageWriter(
            max_inline_size, m_Storage, *m_Data));
        m_OStream.reset(new CWStream(m_Writer.get(),
            0, 0, CRWStreambuf::fLeakExceptions));
        m_OStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
        *m_OStream << (int) type << " ";
        WriteStrWithLen(*m_OStream, fname);
        if (!fname.empty() && type == eLocalFile) {
            m_OStream.reset(new CNcbiOfstream(fname.c_str()));
            m_Writer.reset();
            if (!m_OStream->good()) {
                NCBI_THROW(CFileException, eRelativePath,
                    "Cannot open " + fname + " for output");
            }
            m_OStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
        }
    }
    return *m_OStream.get();
}

CNcbiIstream& CBlobStreamHelper::GetIStream(string* fname /*= NULL*/,
    EStdOutErrStorageType* type /*= NULL*/)
{
    if (!m_IStream.get()) {
        _ASSERT(!m_OStream.get());
        auto_ptr<IReader> reader(
            new CStringOrBlobStorageReader(*m_Data, m_Storage, m_DataSize));
        m_IStream.reset(new CRStream(reader.release(),
            0,0,CRWStreambuf::fOwnReader
            | CRWStreambuf::fLeakExceptions));
        m_IStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
        string name;
        int tmp = (int)eBlobStorage;
        try {
            if (m_IStream->good())
                *m_IStream >> tmp;
            if (m_IStream->good())
                ReadStrWithLen(*m_IStream, name);
        } catch (...) {
            if (!m_IStream->eof()) {
                string msg =
                        "Job output does not match remote_app output format";
                ERR_POST_X(1, msg);
                m_IStream.reset(new CNcbiIstrstream(msg.c_str()));
            }
            return *m_IStream.get();
        }

        if (fname) *fname = name;
        if (type) *type = (EStdOutErrStorageType)tmp;
        if (!name.empty() && (EStdOutErrStorageType)tmp == eLocalFile) {
            auto_ptr<CNcbiIstream> fstr(new CNcbiIfstream(name.c_str()));
            if (fstr->good()) {
                m_IStream.reset(fstr.release());
                m_IStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
            } else {
                string msg = "Can not open " + name;
                msg += " for reading";
                ERR_POST_X(2, msg);
                m_IStream.reset(new CNcbiIstrstream(msg.c_str()));
            }
        }
    }
    return *m_IStream.get();
}

void CBlobStreamHelper::Reset()
{
    m_IStream.reset();

    if (m_OStream.get()) {
        m_OStream->flush();
        m_OStream.reset();
    }

    if (m_Writer.get() != NULL) {
        m_Writer->Close();
        m_Writer.reset();
    }
}
//////////////////////////////////////////////////////////////////////////////
//

CAtomicCounter CRemoteAppRequest::sm_DirCounter;

const string kLocalFSSign = "LFS";

CRemoteAppRequest::~CRemoteAppRequest()
{
    try {
        Reset();
    } NCBI_CATCH_ALL_X(15, "CRemoteAppRequest::~CRemoteAppRequest()");
}

void CRemoteAppRequest::Send(CNcbiOstream& os)
{
    m_StdIn.Reset();
    typedef map<string,string> TFmap;
    TFmap file_map;
    ITERATE(TFiles, it, GetFileNames()) {
        const string& fname = it->first;
        if (it->second == eLocalFile) {
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
            auto_ptr<CNcbiOstream> of(GetNetCacheAPI().CreateOStream(blobid));
            *of << inf.rdbuf();
            file_map[fname] = blobid;
        }
    }

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

void CRemoteAppRequest::Deserialize(CNcbiIstream& is)
{
    Reset();

    string cmdline;
    ReadStrWithLen(is, cmdline);
    SetCmdLine(cmdline);
    ReadStrWithLen(is, m_InBlobIdOrData);

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
            if (x_CopyLocalFile(fname, nfname))
                s_ReplaceArg(args, fname, nfname);
        } else {
            string nfname = GetWorkingDir() + CDirEntry::GetPathSeparator()
                + blobid;
            CNcbiOfstream of(nfname.c_str());
            if (of.good()) {
                auto_ptr<CNcbiIstream> is(GetNetCacheAPI().GetIStream(blobid));
                of << is->rdbuf();
                is.reset();
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
    m_ExlusiveMode = tmp != 0;
}

bool CRemoteAppRequest::x_CopyLocalFile(const string& old_fname,
                                               string& new_fname)
{
    return false;
}

void CRemoteAppRequest::Reset()
{
    m_CmdLine = "";
    m_Files.clear();
    m_AppRunTimeout = 0;
    x_RemoveWDir();

    m_StdIn.Reset();
    m_InBlobIdOrData = "";
    m_StdInDataSize = 0;
    m_ExlusiveMode = false;
}

void CRemoteAppRequest::x_CreateWDir()
{
    if (!m_TmpDirName.empty())
        return;
    m_TmpDirName = m_TmpDirPath + NStr::ULongToString(sm_DirCounter.Add(1));
    CDir wdir(m_TmpDirName);
    if (wdir.Exists())
        wdir.Remove();
    CDir(m_TmpDirName).CreatePath();
}

void CRemoteAppRequest::x_RemoveWDir()
{
    if (m_TmpDirName.empty())
        return;
    CDir dir(m_TmpDirName);
    if (dir.Exists())
        dir.Remove();
    m_TmpDirName = "";
}


//////////////////////////////////////////////////////////////////////////////
//

CRemoteAppRequest::CRemoteAppRequest(
        CNetCacheAPI::TInstance storage, size_t max_inline_size) :
    m_NetCacheAPI(storage),
    m_AppRunTimeout(0),
    m_TmpDirPath(CDir::GetCwd() + CDirEntry::GetPathSeparator()),
    m_StdIn(storage, m_InBlobIdOrData, m_StdInDataSize),
    m_StdInDataSize(0),
    m_StorageType(eBlobStorage),
    m_ExlusiveMode(false),
    m_MaxInlineSize(max_inline_size)
{
}

CRemoteAppResult::~CRemoteAppResult()
{
    try {
        Reset();
    } NCBI_CATCH_ALL_X(16, "CRemoteAppResult::~CRemoteAppResult()");
}

void CRemoteAppResult::Serialize(CNcbiOstream& os)
{
    m_StdOut.Reset();
    m_StdErr.Reset();
    WriteStrWithLen(os, m_OutBlobIdOrData);
    WriteStrWithLen(os, m_ErrBlobIdOrData);
    os << GetRetCode();
}
void CRemoteAppResult::Receive(CNcbiIstream& is)
{
    Reset();
    ReadStrWithLen(is, m_OutBlobIdOrData);
    ReadStrWithLen(is, m_ErrBlobIdOrData);
    int ret = -1; is >> ret; SetRetCode(ret);
}

void CRemoteAppResult::Reset()
{
    m_RetCode = -1;

    m_OutBlobIdOrData = "";
    m_OutBlobSize = 0;
    m_StdOut.Reset();

    m_ErrBlobIdOrData = "";
    m_ErrBlobSize = 0;
    m_StdErr.Reset();

    m_StdOutFileName = "";
    m_StdErrFileName = "";
    m_StorageType = eBlobStorage;
}


void TokenizeCmdLine(const string& cmdline, vector<string>& args)
{
    if (!cmdline.empty()) {
        string arg;

        for (size_t i = 0; i < cmdline.size();) {
            if (cmdline[i] == ' ') {
                if (!arg.empty()) {
                    args.push_back(arg);
                    arg.erase();
                }
                i++;
                continue;
            }
            if (cmdline[i] == '\'' || cmdline[i] == '"') {
                char quote = cmdline[i];
                while( ++i < cmdline.size() && cmdline[i] != quote )
                    arg += cmdline[i];
                args.push_back(arg);
                arg.erase();
                ++i;
                continue;
            }
            arg += cmdline[i++];
        }
        if (!arg.empty())
            args.push_back(arg);
    }
}


string JoinCmdLine(const vector<string>& args)
{
    string cmd_line;

    for (vector<string>::const_iterator it = args.begin();
            it != args.end(); ++it) {
        if (it != args.begin())
            cmd_line += ' ';

        if (it->find(" ") != string::npos)
            cmd_line += '\"' + *it + '\"';
        else
            cmd_line += *it;
    }
    return cmd_line;
}


END_NCBI_SCOPE
