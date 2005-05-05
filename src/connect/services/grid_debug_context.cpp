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
                                             INetScheduleStorageFactory& factory)
{
    sm_Instance.reset(new CGridDebugContext(mode,factory));
    return *sm_Instance;
}

CGridDebugContext* CGridDebugContext::GetInstance()
{
    return sm_Instance.get();
}

CGridDebugContext::CGridDebugContext(eMode mode, 
                                     INetScheduleStorageFactory& factory)
    : m_Mode(mode), m_StorageFactory(factory)
{
    TPid cur_pid = CProcess::GetCurrentPid();
    m_SPid = NStr::UIntToString(cur_pid);
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
    auto_ptr<INetScheduleStorage> storage(CreateStorage());
    m_Blobs.clear();
    ITERATE(vector<string>, it, vfiles) {
        string fname = NStr::TruncateSpaces(*it);
        CNcbiIfstream ifile( fname.c_str() );
        if (!ifile.good())
            continue;
        string blob_id;
        CNcbiOstream& os = storage->CreateOStream(blob_id);
        char buf[1024];
        while( !ifile.eof() ) {
            ifile.read(buf, sizeof(buf));
            os.write(buf, ifile.gcount());
        }       
        storage->Reset();
        m_Blobs[blob_id] = fname;
    }
    m_CurrentJob = m_Blobs.begin();
    return m_Blobs.size();
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
    auto_ptr<INetScheduleStorage> storage(CreateStorage());
    size_t blob_size = 0;
    CNcbiIstream& is = storage->GetIStream(blob_id, &blob_size);
    CNcbiOfstream ofile( fname.c_str() );
    char buf[1024];
    while( !is.eof() ) {
        is.read(buf, sizeof(buf));
        ofile.write(buf, is.gcount());
    }
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.1  2005/05/05 15:18:51  didenko
 * Added debugging facility to worker nodes
 *
 * ===========================================================================
 */
 
