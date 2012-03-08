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
 *    NetSchedule Worker Node implementation
 */

#include <ncbi_pch.hpp>

#include "grid_debug_context.hpp"

#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbistr.hpp>

#include <vector>

BEGIN_NCBI_SCOPE

/// @internal
auto_ptr<CGridDebugContext> CGridDebugContext::sm_Instance;

/// @internal
CGridDebugContext& CGridDebugContext::Create(eMode mode,
    CNetCacheAPI::TInstance netcache_api)
{
    sm_Instance.reset(new CGridDebugContext(mode, netcache_api));
    return *sm_Instance;
}

CGridDebugContext* CGridDebugContext::GetInstance()
{
    return sm_Instance.get();
}

CGridDebugContext::CGridDebugContext(eMode mode,
                                     CNetCacheAPI::TInstance netcache_api)
    : m_Mode(mode), m_NetCacheAPI(netcache_api)
{
    TPid cur_pid = CProcess::GetCurrentPid();
    m_SPid = NStr::ULongToString(cur_pid);
}

CGridDebugContext:: ~CGridDebugContext()
{
}

void CGridDebugContext::SetRunName(const string& name)
{
    m_RunName = name;
}

int CGridDebugContext::SetExecuteList(const string& files)
{
    vector<string> vfiles;
    NStr::Tokenize(files, " ,;", vfiles);
    CNetCacheAPI storage(GetNetCacheAPI());
    m_Blobs.clear();
    ITERATE(vector<string>, it, vfiles) {
        string fname = NStr::TruncateSpaces(*it);
        CNcbiIfstream ifile( fname.c_str() );
        if (!ifile.good())
            continue;
        string blob_id;
        auto_ptr<IWriter> writer(storage.PutData(&blob_id));
        char buf[1024];
        while (!ifile.eof()) {
            ifile.read(buf, sizeof(buf));
            writer->Write(buf, (size_t)ifile.gcount());
        }
        writer->Flush();
        m_Blobs[blob_id] = fname;
    }
    m_CurrentJob = m_Blobs.begin();
    return (int) m_Blobs.size();
}

bool CGridDebugContext::GetNextJob(string& job_key, string& blob_id)
{
    job_key = "";
    blob_id = "";
    if (m_CurrentJob == m_Blobs.end())
        return false;
    job_key = "DBG_" + m_CurrentJob->second;
    blob_id = m_CurrentJob->first;
    ++m_CurrentJob;
    return true;
}

string CGridDebugContext::GetLogFileName() const
{
    _ASSERT(!m_RunName.empty());
    string log_file_name;
    if (m_Mode == eGDC_Gather) {
        log_file_name = m_RunName + '.' + m_SPid + ".log";
    } else if (m_Mode == eGDC_Execute) {
        log_file_name = m_RunName + '.' + m_SPid + ".execute.log";
    }
    return log_file_name;
}


void CGridDebugContext::DumpInput(const string& blob_id, unsigned int job_number)
{
    if (m_Mode != eGDC_Gather)
        return;
    string fname = m_RunName + '.' + m_SPid + '.' +
                   NStr::UIntToString(job_number) + ".inp";
    x_DumpBlob(blob_id, fname);
}

void CGridDebugContext::DumpOutput(const string& job_key,
                                   const string& blob_id,
                                   unsigned int job_number)
{
    string fname;
    if (m_Mode == eGDC_Gather) {
        fname = m_RunName + '.' + m_SPid + '.' +
            NStr::UIntToString(job_number) + ".out";
    }
    else if (m_Mode == eGDC_Execute) {
        if (job_key.find("DBG_") == 0)
            fname = job_key.substr(4) + "__";
        else
            fname = "???_" + m_RunName + '.';
        fname += m_SPid + ".execute.out";
    }
    else return;
    x_DumpBlob(blob_id, fname);
}

void CGridDebugContext::DumpProgressMessage(const string& job_key,
                                            const string& msg,
                                            unsigned int job_number)
{
    string fname;
    if (m_Mode == eGDC_Gather) {
        fname= m_RunName + '.' + m_SPid + '.' +
                   NStr::UIntToString(job_number) + ".msg";
    }
    else if (m_Mode == eGDC_Execute) {
        if (job_key.find("DBG_") == 0)
            fname = job_key.substr(4) + "__";
        else
            fname = "???_" + m_RunName + '.';
        fname += m_SPid + ".execute.msg";
    }
    else return;
    CNcbiOfstream ofile( fname.c_str(), IOS_BASE::out | IOS_BASE::app);
    ofile << msg << endl;
}

void CGridDebugContext::x_DumpBlob(const string& blob_id, const string& fname)
{
    size_t blob_size = 0;
    auto_ptr<IReader> reader(GetNetCacheAPI().GetReader(blob_id, &blob_size));
    CNcbiOfstream ofile(fname.c_str());
    char buf[1024];
    size_t bytes_read;
    while (reader->Read(buf, sizeof(buf), &bytes_read) == eRW_Success)
        ofile.write(buf, bytes_read);
}

END_NCBI_SCOPE
