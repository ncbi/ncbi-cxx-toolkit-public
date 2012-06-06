#ifndef NETCACHE__TASK_SERVER__HPP
#define NETCACHE__TASK_SERVER__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description: 
 */


namespace intr = boost::intrusive;


BEGIN_NCBI_SCOPE


typedef Uint2 TSrvThreadNum;
typedef Uint1 TSrvTaskFlags;


class CNcbiRegistry;
class CSrvSocketFactory;
class CSrvTime;


struct SSrvRCUList_tag;
typedef intr::slist_member_hook<intr::tag<SSrvRCUList_tag> >   TSrvRCUListHook;

class CSrvRCUUser
{
public:
    void CallRCU(void);
    virtual void ExecuteRCU(void) = 0;

    CSrvRCUUser(void);
    virtual ~CSrvRCUUser(void);

private:
    CSrvRCUUser(const CSrvRCUUser&);
    CSrvRCUUser& operator= (const CSrvRCUUser&);

public:
    TSrvRCUListHook m_RCUListHook;
};


typedef intr::member_hook<CSrvRCUUser,
                          TSrvRCUListHook,
                          &CSrvRCUUser::m_RCUListHook>  TSrvRCUListOpt;
typedef intr::slist<CSrvRCUUser,
                    TSrvRCUListOpt,
                    intr::cache_last<true>,
                    intr::constant_time_size<true> >    TSrvRCUList;


struct SSrvShutdownList_tag;
typedef intr::slist_member_hook<intr::tag<SSrvShutdownList_tag> >   TSrvSDListHook;


class CSrvShutdownCallback
{
public:
    CSrvShutdownCallback(void);
    virtual ~CSrvShutdownCallback(void);

    virtual bool ReadyForShutdown(void) = 0;

public:
    TSrvSDListHook m_SDListHook;
};


typedef intr::member_hook<CSrvShutdownCallback,
                          TSrvSDListHook,
                          &CSrvShutdownCallback::m_SDListHook>  TSDListHookOpt;
typedef intr::slist<CSrvShutdownCallback,
                    TSDListHookOpt,
                    intr::constant_time_size<false>,
                    intr::cache_last<false> >                   TShutdownList;


enum ESrvShutdownType {
    eSrvSlowShutdown,
    eSrvFastShutdown
};


class CTaskServer
{
public:
    static bool Initialize(int& argc, const char** argv);
    static void Finalize(void);
    static void Run(void);

    static const CNcbiRegistry& GetConfRegistry(void);
    static CSrvTime GetStartTime(void);

    static bool AddListeningPort(Uint2 port, CSrvSocketFactory* factory);
    static void AddShutdownCallback(CSrvShutdownCallback* callback);
    static void RequestShutdown(ESrvShutdownType shutdown_type);

    static bool IsRunning(void);
    static bool IsInShutdown(void);
    static bool IsInSoftShutdown(void);
    static bool IsInHardShutdown(void);

    static TSrvThreadNum GetMaxRunningThreads(void);
    static TSrvThreadNum GetCurThreadNum(void);

    static const string& GetHostName(void);
    static Uint4 GetIPByHost(const string& host);
    static string IPToString(Uint4 ip);
    static string GetHostByIP(Uint4 ip);

private:
    CTaskServer(const CTaskServer&);
};


END_NCBI_SCOPE

#endif /* NETCACHE__TASK_SERVER__HPP */
