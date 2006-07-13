#ifndef CONNECT_SERVICES__REMOTE_APP_MB_HPP
#define CONNECT_SERVICES__REMOTE_APP_MB_HPP

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
 * Authors:  Maxim Didneko,
 *
 * File Description:  
 *
 */

#include <corelib/ncbistre.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiobj.hpp>

#include <connect/connect_export.h>
#include <connect/services/remote_app.hpp>

BEGIN_NCBI_SCOPE

class IBlobStorageFactory;
class CRemoteAppRequestMB_Impl;

enum EStdOutErrStorageType {
    eLocalFile = 0,
    eBlobStorage
};

/// Remote Application Request (client side)
///
/// It is used by a client application which wants to run a remote application
/// through NetSchedule infrastructure and should be used in conjunction with 
/// CGridJobSubmitter class
///
class NCBI_XCONNECT_EXPORT CRemoteAppRequestMB : 
    public CObject,
    public IRemoteAppRequest
{
public:
    explicit CRemoteAppRequestMB(IBlobStorageFactory& factory);
    ~CRemoteAppRequestMB();

    /// Get an output stream to write data to a remote application stdin
    CNcbiOstream& GetStdIn();

    /// Set a remote application command line. 
    /// Cmdline should not contain a remote program name, just its arguments
    void SetCmdLine(const string& cmd);

    void SetAppRunTimeout(unsigned int sec);
    //    void RequestExclusiveMode();
    
    /// Transfer a file to an application executer side.
    /// It only makes sense to transfer a file if its name also mentioned in
    /// the command line for the remote application. When the file is transfered 
    /// the the executer side it gets stored to a temprary directory and then its
    /// original name in the command line will be replaced with the new temprary name.
    void AddFileForTransfer(const string& fname);

    void SetStdOutErrFileNames(const string& stdout_fname,
                               const string& stderr_fname,
                               EStdOutErrStorageType storage_type = eLocalFile);

    /// Serialize a request to a given stream. After call to this method the instance
    /// cleans itself an it can be reused.
    void Send(CNcbiOstream& os);

private:
    CRemoteAppRequestMB(const CRemoteAppRequestMB&);
    CRemoteAppRequestMB& operator=(const CRemoteAppRequestMB&);

    auto_ptr<CRemoteAppRequestMB_Impl> m_Impl;
};

/// Remote Application Request (application executer side)
/// 
/// It is used by a grid worker node to get parameters for a remote application.
///
class NCBI_XCONNECT_EXPORT CRemoteAppRequestMB_Executer : public IRemoteAppRequest_Executer
{
public:
    explicit CRemoteAppRequestMB_Executer(IBlobStorageFactory& factory);
    ~CRemoteAppRequestMB_Executer();

    /// Get a stdin stream for a remote application
    CNcbiIstream& GetStdIn();

    /// Get a commnad line for a remote application
    const string& GetCmdLine() const;

    unsigned int GetAppRunTimeout() const;
    //    bool IsExclusiveModeRequested() const;
    const string& GetWorkingDir() const;
    const string& GetInBlobIdOrData() const;

    /// Deserialize a request from a given stream.
    void Receive(CNcbiIstream& is);

    const string& GetStdOutFileName() const;
    const string& GetStdErrFileName() const;
    EStdOutErrStorageType GetStdOutErrStorageType() const;

    void Reset();

    void Log(const string& prefix);

private:
    CRemoteAppRequestMB_Executer(const CRemoteAppRequestMB_Executer &);
    CRemoteAppRequestMB_Executer& operator=(const CRemoteAppRequestMB_Executer&);

    auto_ptr<CRemoteAppRequestMB_Impl> m_Impl;
};


class CRemoteAppResultMB_Impl;

/// Remote Application Result (client side)
///
/// It is used by a client application to get results for a remote applicaion
/// and should be used in conjunction with CGridJobStatus
///
class NCBI_XCONNECT_EXPORT CRemoteAppResultMB : 
    public CObject,
    public IRemoteAppResult
{
public:
    explicit CRemoteAppResultMB(IBlobStorageFactory& factory);
    ~CRemoteAppResultMB();

    /// Get a remote application stdout
    CNcbiIstream& GetStdOut();

    /// Get a remote application stderr
    CNcbiIstream& GetStdErr();

    /// Get a remote application return code
    int           GetRetCode() const;

    const string& GetStdOutFileName() const;
    const string& GetStdErrFileName() const;
    EStdOutErrStorageType GetStdOutErrStorageType() const;

    /// Deserialize a result from a given stream.
    void Receive(CNcbiIstream& is);

private:
    CRemoteAppResultMB(const CRemoteAppResultMB&);
    CRemoteAppResultMB& operator=(const CRemoteAppResultMB&);

    auto_ptr<CRemoteAppResultMB_Impl> m_Impl;
};

/// Remote Application Result (application executer side)
/// 
/// It is used by a grid worker node to send results of a 
/// finished remote application to the clien.
class NCBI_XCONNECT_EXPORT CRemoteAppResultMB_Executer : public IRemoteAppResult_Executer
{
public:
    explicit CRemoteAppResultMB_Executer(IBlobStorageFactory& factory);
    ~CRemoteAppResultMB_Executer();

    /// Get a stream to put remote application's stdout to
    CNcbiOstream& GetStdOut();

    /// Get a stream to put remote application's stderr to
    CNcbiOstream& GetStdErr();
    
    /// Set a remote application's return code
    void SetRetCode(int ret_code);

    void SetStdOutErrFileNames(const string& stdout_fname,
                               const string& stderr_fname,
                               EStdOutErrStorageType type);


    /// Serialize a result to a given stream. After call to this method the instance
    /// cleans itself an it can be reused.
    void Send(CNcbiOstream& os);

    const string& GetOutBlobIdOrData() const;
    const string& GetErrBlobIdOrData() const;

    void Reset();

    void Log(const string& prefix);

private:
    CRemoteAppResultMB_Executer(const CRemoteAppResultMB_Executer&);
    CRemoteAppResultMB_Executer& operator=(const CRemoteAppResultMB_Executer&);

    auto_ptr<CRemoteAppResultMB_Impl> m_Impl;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/07/13 14:32:38  didenko
 * Modified the implemention of remote application's request and result classes
 *
 * Revision 1.9  2006/06/28 16:01:42  didenko
 * Redone job's exlusivity processing
 *
 * Revision 1.8  2006/06/19 19:41:05  didenko
 * Spelling fix
 *
 * Revision 1.7  2006/06/19 13:36:27  didenko
 * added logging information
 *
 * Revision 1.6  2006/05/15 15:26:53  didenko
 * Added support for running exclusive jobs
 *
 * Revision 1.5  2006/05/10 19:54:21  didenko
 * Added JobDelayExpiration method to CWorkerNodeContext class
 * Added keep_alive_period and max_job_run_time parmerter to the config
 * file of remote_app
 *
 * Revision 1.4  2006/05/08 15:16:42  didenko
 * Added support for an optional saving of a remote application's stdout
 * and stderr into files on a local file system
 *
 * Revision 1.3  2006/03/16 15:13:59  didenko
 * Remaned CRemoteJob... to CRemoteApp...
 * + Comments
 *
 * Revision 1.2  2006/03/15 17:30:11  didenko
 * Added ability to use embedded NetSchedule job's storage as a job's input/output
 * data instead of using it as a NetCache blob key. This reduces network traffic 
 * and increases job submittion speed.
 *
 * Revision 1.1  2006/03/07 17:17:12  didenko
 * Added facility for running external applications throu NetSchedule service
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES__REMOTE_APP_MB_HPP
