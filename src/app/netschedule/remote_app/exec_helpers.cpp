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
 * Authors:  Maxim Didenko
 *
 * File Description:  NetSchedule worker node sample
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbistre.hpp>
#include <corelib/stream_utils.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbireg.hpp>

#include <connect/ncbi_pipe.hpp>
#include <connect/services/grid_worker.hpp>

#include "exec_helpers.hpp"

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////
///
CRemoteAppParams::CRemoteAppParams()
    : m_MaxAppRunningTime(0), m_KeepAlivePeriod(0), m_FailOnNonZeroExit(false),
      m_RunInSeparateDir(false)
{
}

void CRemoteAppParams::Load(const string& sec_name, const IRegistry& reg)
{
    m_MaxAppRunningTime = 
        reg.GetInt(sec_name,"max_app_run_time",0,0,IRegistry::eReturn);
    
    m_KeepAlivePeriod = 
        reg.GetInt(sec_name,"keep_alive_period",0,0,IRegistry::eReturn);

    m_FailOnNonZeroExit =
        reg.GetBool(sec_name, "fail_on_non_zero_exit", false, 0, 
                    CNcbiRegistry::eReturn);

    m_RunInSeparateDir =
        reg.GetBool(sec_name, "run_in_separate_dir", false, 0, 
                    CNcbiRegistry::eReturn);

    if (m_RunInSeparateDir) {
        m_TempDir = reg.GetString(sec_name, "tmp_path", "." );
        if (!CDirEntry::IsAbsolutePath(m_TempDir)) {
            string tmp = CDir::GetCwd() 
                + CDirEntry::GetPathSeparator() 
                + m_TempDir;
            m_TempDir = CDirEntry::NormalizePath(tmp);
        }
    }

    m_AppPath = reg.GetString(sec_name, "app_path", "" );
    if (!CDirEntry::IsAbsolutePath(m_AppPath)) {
        string tmp = CDir::GetCwd() 
            + CDirEntry::GetPathSeparator() 
            + m_AppPath;
        m_AppPath = CDirEntry::NormalizePath(tmp);
    }
}

//////////////////////////////////////////////////////////////////////////////
///

bool CanExecRemoteApp(const CFile& file)
{
    CDirEntry::TMode user_mode  = 0;
    if (!file.GetMode(&user_mode))
        return false;
    if (user_mode & CDirEntry::fExecute)
        return true;
    return false;
}

//////////////////////////////////////////////////////////////////////////////
///
struct STmpDirGuard
{
    STmpDirGuard(const string& path) : m_Path(path) 
    { 
        if (!m_Path.empty()) {
            CDir dir(m_Path); 
            if (!dir.Exists()) 
                dir.CreatePath(); 
        }
    }
    ~STmpDirGuard() { if (!m_Path.empty()) CDir(m_Path).Remove(); }
    const string& m_Path;
};

//////////////////////////////////////////////////////////////////////////////
///
bool ExecRemoteApp(const string& cmd, 
                   const vector<string>& args,
                   CNcbiIstream& in, CNcbiOstream& out, CNcbiOstream& err,
                   int& exit_value,
                   CWorkerNodeJobContext& context,
                   int max_app_running_time,
                   int app_running_time,
                   int keep_alive_period,
                   const string& tmp_path,
                   const char* const env[])
{
    STmpDirGuard guard(tmp_path);

    CPipe pipe;
    EIO_Status st = pipe.Open(cmd.c_str(), args, CPipe::fStdErr_Open,tmp_path, env);
    if (st != eIO_Success)
        NCBI_THROW(CException, eInvalid, 
                   "Could not execute " + cmd + " file.");

    
    auto_ptr<CStopWatch> keep_alive;
    if (keep_alive_period > 0)
        keep_alive.reset(new CStopWatch(CStopWatch::eStart));

    auto_ptr<CStopWatch> running_time;
    if (max_app_running_time > 0 || app_running_time >> 0)
        running_time.reset(new CStopWatch(CStopWatch::eStart));

    bool canceled = false;
    bool out_done = false;
    bool err_done = false;
    bool in_done = false;
    
    const size_t buf_size = 4096;
    char buf[buf_size];
    size_t bytes_in_inbuf = 0;
    size_t total_bytes_written = 0;
    char inbuf[buf_size];

    CPipe::TChildPollMask mask = CPipe::fStdIn | CPipe::fStdOut | CPipe::fStdErr;

    STimeout wait_time = {1, 0};
    while (!out_done || !err_done) {
        EIO_Status rstatus;
        size_t bytes_read;

        CPipe::TChildPollMask rmask = pipe.Poll(mask, &wait_time);
        if (rmask & CPipe::fStdIn && !in_done) {
            if ( in.good() && bytes_in_inbuf == 0) {
                bytes_in_inbuf = CStreamUtils::Readsome(in, inbuf, buf_size);
                total_bytes_written = 0;
            }
        
            size_t bytes_written;
            if (bytes_in_inbuf > 0) {
                rstatus = pipe.Write(inbuf + total_bytes_written, bytes_in_inbuf,
                                     &bytes_written);
                if (rstatus != eIO_Success) {
                    ERR_POST("Not all the data is written to stdin of a child process.");
                    in_done = true;
                }
                total_bytes_written += bytes_written;
                bytes_in_inbuf -= bytes_written;
            }

            if ((!in.good() && bytes_in_inbuf == 0) || in_done) {
                pipe.CloseHandle(CPipe::eStdIn);
                mask &= ~CPipe::fStdIn;
            }

        } if (rmask & CPipe::fStdOut) {
            // read stdout
            if (!out_done) {
                rstatus = pipe.Read(buf, buf_size, &bytes_read);
                out.write(buf, bytes_read);
                if (rstatus != eIO_Success) {
                    out_done = true;
                    mask &= ~CPipe::fStdOut;
            }
        }


        } if (rmask & CPipe::fStdErr) {
            if (!err_done) {
                rstatus = pipe.Read(buf, buf_size, &bytes_read, CPipe::eStdErr);
                err.write(buf, bytes_read);
                if (rstatus != eIO_Success) {
                    err_done = true;
                    mask &= ~CPipe::fStdErr;
                }
            }
        }
        if (!CProcess(pipe.GetProcessHandle()).IsAlive())
            break;

        if (context.GetShutdownLevel() == 
            CNetScheduleClient::eShutdownImmidiate) {
            CProcess(pipe.GetProcessHandle()).Kill();
            canceled = true;
            break;
        }
        if (running_time.get()) {
            double elapsed = running_time->Elapsed();
            if (app_running_time > 0 &&  elapsed > (double)app_running_time) {
                CProcess(pipe.GetProcessHandle()).Kill();
                NCBI_THROW(CException, eUnknown, 
                      "The application's runtime has exceeded a time(" +
                       NStr::UIntToString(app_running_time) +
                      " sec) set by the client");
            }
            if ( max_app_running_time > 0 && elapsed > (double)max_app_running_time) {
                CProcess(pipe.GetProcessHandle()).Kill();
                NCBI_THROW(CException, eUnknown, 
                      "The application's runtime has exceeded a time(" +
                      NStr::UIntToString(max_app_running_time) +
                      " sec) set in the config file");
            }
        }

        if (keep_alive.get() 
            && keep_alive->Elapsed() > (double) keep_alive_period ) {
            context.JobDelayExpiration(keep_alive_period + 10);
            keep_alive->Restart();
        }

    }

    pipe.Close(&exit_value);
    return canceled;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/05/30 16:43:36  didenko
 * Moved the commonly used code to separate files.
 *
 * ===========================================================================
 */
