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

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/blob_storage.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbiexec.hpp>

#include <util/stream_utils.hpp>

#include <connect/ncbi_pipe.hpp>
#include <connect/services/grid_worker_app.hpp>
#include <connect/services/remote_job.hpp>

#include <vector>
#include <algorithm>

USING_NCBI_SCOPE;

    
///////////////////////////////////////////////////////////////////////


static bool s_CanExecute(const CFile& file)
{
    CDirEntry::TMode user_mode  = 0;
    if (!file.GetMode(&user_mode))
        return false;
    if (user_mode & CDirEntry::fExecute)
        return true;
    return false;
}

static bool s_Exec(const string& cmd, const vector<string>& args,
                   CNcbiIstream& in, CNcbiOstream& out, CNcbiOstream& err,
                   int& exit_value,
                   CWorkerNodeJobContext& context)
{
    CPipe pipe;

    if (pipe.Open(cmd.c_str(), args, CPipe::fStdErr_Open) != eIO_Success)
        NCBI_THROW(CException, eInvalid, 
                   "Could not execute " + cmd + " file.");

    
    bool canceled = false;
    bool out_done = false;
    bool err_done = false;
    
    const size_t buf_size = 4096;
    char buf[buf_size];
    size_t bytes_in_inbuf = 0;
    size_t total_bytes_written = 0;
    char inbuf[buf_size];

#if defined(NCBI_OS_MSWIN)
    STimeout short_time = {0, 1};
    pipe.SetTimeout(eIO_Read, &short_time);
    pipe.SetTimeout(eIO_Write, &short_time);
    
    while (!out_done || !err_done) {

        // write stdin
        if (in.good() && bytes_in_inbuf == 0) {
            bytes_in_inbuf = CStreamUtils::Readsome(in, inbuf, buf_size);
            total_bytes_written = 0;
        }
        
        size_t bytes_written;
        if (bytes_in_inbuf > 0) {
            pipe.Write(inbuf + total_bytes_written, bytes_in_inbuf,
                       &bytes_written);
            total_bytes_written += bytes_written;
            bytes_in_inbuf -= bytes_written;
        }

        if ((!in.good() || in.eof()) && bytes_in_inbuf == 0) {
            pipe.CloseHandle(CPipe::eStdIn);
        }

        EIO_Status rstatus;
        size_t bytes_read;
     
        // read stdout
        if (!out_done) {
            rstatus = pipe.Read(buf, buf_size, &bytes_read);
            out.write(buf, bytes_read);
            if (rstatus != eIO_Success && rstatus != eIO_Timeout) {
                out_done = true;
            }
        }
        
        // read stderr
        if (!err_done) {
            rstatus = pipe.Read(buf, buf_size, &bytes_read, CPipe::eStdErr);
            err.write(buf, bytes_read);
            if (rstatus != eIO_Success && rstatus != eIO_Timeout) {
                err_done = true;
            }
        }

        if (context.GetShutdownLevel() == 
            CNetScheduleClient::eShutdownImmidiate) {
            CProcess(pipe.GetProcessHandle()).Kill();
            canceled = true;
            break;
        }
    }
#elif defined(NCBI_OS_UNIX)

    CPipe::TChildPollMask mask = CPipe::fStdIn | CPipe::fStdOut | CPipe::fStdErr;

    STimeout wait_time = {1, 0};
    while (!out_done || !err_done) {
        EIO_Status rstatus;
        size_t bytes_read;

        CPipe::TChildPollMask rmask = pipe.Poll(mask, &wait_time);
        if (rmask & CPipe::fStdIn) {
            if (in.good() && !in.eof() && bytes_in_inbuf == 0) {
                bytes_in_inbuf = CStreamUtils::Readsome(in, inbuf, buf_size);
                total_bytes_written = 0;
            }
        
            size_t bytes_written;
            if (bytes_in_inbuf > 0) {
                pipe.Write(inbuf + total_bytes_written, bytes_in_inbuf,
                           &bytes_written);
                total_bytes_written += bytes_written;
                bytes_in_inbuf -= bytes_written;
            }

            if ((!in.good() || in.eof()) && bytes_in_inbuf == 0) {
                pipe.CloseHandle(CPipe::eStdIn);
                mask &= ~CPipe::fStdIn;
            }

        } else if (rmask & CPipe::fStdOut) {
            // read stdout
            if (!out_done) {
                rstatus = pipe.Read(buf, buf_size, &bytes_read);
                out.write(buf, bytes_read);
                if (rstatus != eIO_Success) {
                    out_done = true;
                    mask &= ~CPipe::fStdOut;
            }
        }


        } else if (rmask & CPipe::fStdErr) {
            if (!err_done) {
                rstatus = pipe.Read(buf, buf_size, &bytes_read, CPipe::eStdErr);
                err.write(buf, bytes_read);
                if (rstatus != eIO_Success) {
                    err_done = true;
                    mask &= ~CPipe::fStdErr;
                }
            }
        } else {
        }

        if (!CProcess(pipe.GetProcessHandle()).IsAlive())
            //            LOG_POST("Process is not alive : " << pipe.GetProcessHandle() );
            break;

        if (context.GetShutdownLevel() == 
            CNetScheduleClient::eShutdownImmidiate) {
            CProcess(pipe.GetProcessHandle()).Kill();
            canceled = true;
            break;
        }

    }

#endif  /* NCBI_OS_UNIX | NCBI_OS_MSWIN */

    //    LOG_POST("About to close : " << pipe.GetProcessHandle() );
    pipe.Close(&exit_value);

    // return the value with which the process exited
    return canceled;
}

/// NetSchedule sample job
///
/// Job reads an array of doubles, sorts it and returns data back
/// to the client as a BLOB.
///
class CRemoteAppJob : public IWorkerNodeJob
{
public:
    CRemoteAppJob(const IWorkerNodeInitContext& context) :
        m_Factory(context.GetConfig())
    {
        const IRegistry& reg = context.GetConfig();
        m_AppPath = reg.GetString("remote_app", "app_path", "" );
        CFile file(m_AppPath);
        if (!file.Exists())
            NCBI_THROW(CException, eInvalid, 
                       "File : " + m_AppPath + " doesn't exists.");
        if (!s_CanExecute(file))
            NCBI_THROW(CException, eInvalid, 
                       "Could not execute " + m_AppPath + " file.");

    }

    virtual ~CRemoteAppJob() {} 

    int Do(CWorkerNodeJobContext& context) 
    {
        LOG_POST( CTime(CTime::eCurrent).AsString() 
                  << ": " << context.GetJobKey() + " " + context.GetJobInput());

        CRemoteJobRequest_Executer request(m_Factory);
        request.Receive(context.GetIStream());
        vector<string> args;
        NStr::Tokenize(request.GetCmdLine(), " ", args);

        CRemoteJobResult_Executer result(m_Factory);

        int ret = 0;
        bool canceled = s_Exec(m_AppPath, args, 
                               request.GetStdIn(), 
                               result.GetStdOut(), 
                               result.GetStdErr(),
                               ret,
                               context);

        request.CleanUp();
        result.SetRetCode(ret); 
        result.Send(context.GetOStream());

        LOG_POST( CTime(CTime::eCurrent).AsString() 
                  << ": Job " << context.GetJobKey() << " is done. ");
        if (!canceled)
            context.CommitJob();
        return 0;
    }
private:

    string m_AppPath;

    CBlobStorageFactory m_Factory;

};

class CRemoteAppIdleTask : public IWorkerNodeIdleTask
{
public:
    CRemoteAppIdleTask(const IWorkerNodeInitContext& context) 
    {
        const IRegistry& reg = context.GetConfig();
        m_AppCmd = reg.GetString("remote_app", "idle_app_cmd", "" );
        if (m_AppCmd.empty())
            throw runtime_error("Idle application is not set.");
    }
    virtual ~CRemoteAppIdleTask() {}

    virtual void Run(CWorkerNodeIdleTaskContext& context)
    {
        if (!m_AppCmd.empty())
            CExec::System(m_AppCmd.c_str());
    }
private:
    string m_AppCmd;
};



/////////////////////////////////////////////////////////////////////////////
//  Routine magic spells

NCBI_WORKERNODE_MAIN_EX(CRemoteAppJob, CRemoteAppIdleTask, 1.0.0);

//NCBI_WORKERNODE_MAIN(CRemoteAppJob, 1.0.1);



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/03/07 18:00:18  didenko
 * Spelling fix
 *
 * Revision 1.1  2006/03/07 17:39:59  didenko
 * Added a workernode for running external applications
 *
 * ===========================================================================
 */

