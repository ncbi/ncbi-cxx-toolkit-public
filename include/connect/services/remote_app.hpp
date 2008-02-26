#ifndef CONNECT_SERVICES__REMOTE_APP_HPP
#define CONNECT_SERVICES__REMOTE_APP_HPP

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

#include <connect/connect_export.h>

#include <vector>

BEGIN_NCBI_SCOPE

const string kLocalFSSign = "LFS";
/// Remote Application Request (client side)
///
/// It is used by a client application which wants to run a remote application
/// through NetSchedule infrastructure and should be used in conjunction with
/// CGridJobSubmitter class
///
class NCBI_XCONNECT_EXPORT IRemoteAppRequest
{
public:
    enum ETrasferType {
        eBlobStorage,  ///< transfer files througth blobstorage
        eLocalFS       ///< copy files througth local FS
    };

    virtual ~IRemoteAppRequest();

    /// Get an output stream to write data to a remote application stdin
    virtual CNcbiOstream& GetStdIn() = 0;

    /// Set a remote application command line.
    /// Cmdline should not contain a remote program name, just its arguments
    virtual void SetCmdLine(const string& cmd) = 0;

    /// Transfer a file to an application executer side.
    /// It only makes sense to transfer a file if its name also mentioned in
    /// the command line for the remote application. When the file is transfered
    /// the the executer side it gets stored to a temprary directory and then its
    /// original name in the command line will be replaced with the new temprary name.
    virtual void AddFileForTransfer(const string& fname,
                                    ETrasferType tt = eBlobStorage ) = 0;

    virtual void SetAppRunTimeout(unsigned int sec) = 0;

    /// Serialize a request to a given stream. After call to this method the instance
    /// cleans itself an it can be reused.
    virtual void Send(CNcbiOstream& os) = 0;
};

/// Remote Application Request (application executer side)
///
/// It is used by a grid worker node to get parameters for a remote application.
///
class NCBI_XCONNECT_EXPORT IRemoteAppRequest_Executer
{
public:
    virtual ~IRemoteAppRequest_Executer();

    /// Get a stdin stream for a remote application
    virtual CNcbiIstream& GetStdIn() = 0;

    /// Get a commnad line for a remote application
    virtual const string& GetCmdLine() const = 0;

    virtual unsigned int GetAppRunTimeout() const = 0;

    /// Deserialize a request from a given stream.
    virtual void Receive(CNcbiIstream& is) = 0;

    virtual void Reset() = 0;

    virtual void Log(const string& prefix) = 0;
};


/// Remote Application Result (client side)
///
/// It is used by a client application to get results for a remote applicaion
/// and should be used in conjunction with CGridJobStatus
///
class NCBI_XCONNECT_EXPORT IRemoteAppResult
{
public:
    virtual ~IRemoteAppResult();

    /// Get a remote application stdout
    virtual CNcbiIstream& GetStdOut() = 0;

    /// Get a remote application stderr
    virtual CNcbiIstream& GetStdErr() = 0;

    /// Get a remote application return code
    virtual int           GetRetCode() const = 0;

    /// Deserialize a result from a given stream.
    virtual void Receive(CNcbiIstream& is) = 0;
};

/// Remote Application Result (application executer side)
///
/// It is used by a grid worker node to send results of a
/// finished remote application to the clien.
class NCBI_XCONNECT_EXPORT IRemoteAppResult_Executer
{
public:
    virtual ~IRemoteAppResult_Executer();

    /// Get a stream to put remote application's stdout to
    virtual CNcbiOstream& GetStdOut() = 0;

    /// Get a stream to put remote application's stderr to
    virtual CNcbiOstream& GetStdErr() = 0;

    /// Set a remote application's return code
    virtual void SetRetCode(int ret_code) = 0;

    /// Serialize a result to a given stream. After call to this method the instance
    /// cleans itself an it can be reused.
    virtual void Send(CNcbiOstream& os) = 0;

    virtual void Reset() = 0;

    virtual void Log(const string& prefix) = 0;
};


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

NCBI_XCONNECT_EXPORT
void TokenizeCmdLine(const string& cmdline, vector<string>& args);

NCBI_XCONNECT_EXPORT
string JoinCmdLine(const vector<string>& args);

END_NCBI_SCOPE

#endif // CONNECT_SERVICES__REMOTE_APP_HPP
