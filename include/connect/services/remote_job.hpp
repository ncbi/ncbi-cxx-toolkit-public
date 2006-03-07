#ifndef CONNECT_SERVICES__REMOTE_JOB_HPP
#define CONNECT_SERVICES__REMOTE_JOB_HPP

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

#include <connect/connect_export.h>

BEGIN_NCBI_SCOPE

class IBlobStorageFactory;
class CRemoteJobRequest_Impl;

class NCBI_XCONNECT_EXPORT CRemoteJobRequest_Submitter 
{
public:
    explicit CRemoteJobRequest_Submitter(IBlobStorageFactory& factory);
    ~CRemoteJobRequest_Submitter();

    CNcbiOstream& GetStdIn();
    void SetCmdLine(const string& cmd);

    void AddFileForTransfer(const string& fname);

    void Send(CNcbiOstream& os);

private:
    auto_ptr<CRemoteJobRequest_Impl> m_Impl;
};

class NCBI_XCONNECT_EXPORT CRemoteJobRequest_Executer
{
public:
    explicit CRemoteJobRequest_Executer(IBlobStorageFactory& factory);
    ~CRemoteJobRequest_Executer();

    CNcbiIstream& GetStdIn();
    const string& GetCmdLine() const;

    void Receive(CNcbiIstream& is);
    void CleanUp();

private:

    auto_ptr<CRemoteJobRequest_Impl> m_Impl;
};


class CRemoteJobResult_Impl;

class NCBI_XCONNECT_EXPORT CRemoteJobResult_Executer
{
public:
    explicit CRemoteJobResult_Executer(IBlobStorageFactory& factory);
    ~CRemoteJobResult_Executer();

    CNcbiOstream& GetStdOut();
    CNcbiOstream& GetStdErr();

    void SetRetCode(int ret_code);

    void Send(CNcbiOstream& os);

private:

    auto_ptr<CRemoteJobResult_Impl> m_Impl;
};


class NCBI_XCONNECT_EXPORT CRemoteJobResult_Submitter
{
public:
    explicit CRemoteJobResult_Submitter(IBlobStorageFactory& factory);
    ~CRemoteJobResult_Submitter();

    CNcbiIstream& GetStdOut();
    CNcbiIstream& GetStdErr();
    int           GetRetCode() const;

    void Receive(CNcbiIstream& is);

private:

    auto_ptr<CRemoteJobResult_Impl> m_Impl;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/03/07 17:17:12  didenko
 * Added facility for running external applications throu NetSchedule service
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES__REMOTE_JOB_HPP
