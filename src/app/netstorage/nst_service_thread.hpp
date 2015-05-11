#ifndef NETSTORAGE_SERVICE_THREAD__HPP
#define NETSTORAGE_SERVICE_THREAD__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description: NetStorage service thread
 *
 *
 */

#include <util/thread_nonstop.hpp>


BEGIN_NCBI_SCOPE

class CNetStorageServer;


// Thread class, does some routine periodic operations
class CNetStorageServiceThread : public CThreadNonStop
{
public:
    CNetStorageServiceThread(CNetStorageServer &  server) :
        CThreadNonStop(1),    // Once in 1 seconds
        m_Server(server),
        m_LastConfigFileCheck(0)
    {}

    virtual void DoJob(void);
    virtual void *  Main(void)
    {
        SetCurrentThreadName("netstoraged_st");
        return CThreadNonStop::Main();
    }

private:
    void  x_CheckConfigFile(void);

private:
    CNetStorageServiceThread(const CNetStorageServiceThread &);
    CNetStorageServiceThread &  operator=(const CNetStorageServiceThread &);

private:
    CNetStorageServer &     m_Server;
    time_t                  m_LastConfigFileCheck;
};


END_NCBI_SCOPE

#endif /* NETSTORAGE_SERVICE_THREAD__HPP */

