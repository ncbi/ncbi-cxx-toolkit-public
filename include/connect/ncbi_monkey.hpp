/* $Id$
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
* Author:  Dmitriy Elisov
*
* File Description:
*   Chaos Monkey - a library that is hooked with ncbi_socket.c and introduces
*   problems in network connectivity - losses, bad data, delays.
*   difficulties
*
*/

#ifndef CONNECT___NCBI_MONKEY__HPP
#define CONNECT___NCBI_MONKEY__HPP

#include "../src/connect/ncbi_priv.h"

#ifdef NCBI_MONKEY
#   define NCBI_MONKEY_TESTS /* For Unit Testing */

#   include <corelib/ncbithr.hpp>
#   include <corelib/ncbiexpt.hpp>
#   include <corelib/ncbiapp.hpp>
#   include <util/random_gen.hpp>
#   include "ncbi_socket.h"
#   include "../src/connect/ncbi_socketp.h"
#   include "ncbi_socket.hpp"
#   include <corelib/ncbistr.hpp>

#   include <vector>
#   include <string>
#   include <sstream>


BEGIN_NCBI_SCOPE

using namespace std;

enum EMonkeyActionType {
    eMonkey_Recv,
    eMonkey_Send,
    eMonkey_Poll,
    eMonkey_Connect
};


class NCBI_XNCBI_EXPORT CMonkeyException : public CException
{
public:
    enum EErrCode {
        e_MonkeyInvalidArgs = 0   /**< Incorrect arguments                 */
       ,e_MonkeyUnknown     = 1   /**< Not classified as anything above    */
    };

    /** Get original status code and status message from LBOS in a string */
    virtual const char* what(void)  const throw();

private:
    unsigned short m_StatusCode;
    string         m_Message;

    NCBI_EXCEPTION_DEFAULT(CMonkeyException, CException);
};


/** A special class which opens access to Monkey Seed 
 *  (only for Monkey rules and plans) */
class CMonkeySeedKey
{
    friend class CMonkeySeedAccessor;
    CMonkeySeedKey() = default;
    ~CMonkeySeedKey() = default;
    /* No copying allowed */
    CMonkeySeedKey(const CMonkeySeedKey&) = delete;
    CMonkeySeedKey& operator=(const CMonkeySeedKey&) = delete;
};


/** Base class for Monkey methods that are allowed to write to MonkeyLog */
class CMonkeySeedAccessor
{
protected: 
    static const CMonkeySeedKey& Key(void);
};


/** Common functionality for all rule types:
 *  - Runs order (vector<double> m_Runs)
 *  - How to repeat runs order (ERepeatType m_RepeatType) 
 *  - After how many runs repeat order (size_t m_RunsSize) 
 */
class CMonkeyRuleBase : public CMonkeySeedAccessor
{
public:
    enum ERepeatType {
        eMonkey_RepeatAgain,
        eMonkey_RepeatLast,
        eMonkey_RepeatNone
    };
    enum ERunMode {
        eMonkey_RunProbability,
        eMonkey_RunNumber
    };
    enum ERunFormat {
        eMonkey_RunSequence,
        eMonkey_RunRanges
    };

    /** Add socket */
    void AddSocket(MONKEY_SOCKTYPE sock) ;

    /** Check that this rule will trigger on this run (host and port have 
     * already been successfully matched if this check is run) */
    bool CheckRun(MONKEY_SOCKTYPE sock,
                  unsigned short  probability_left = 100) const;

    /** Get probability that the rule will run next time */
    unsigned short GetProbability(MONKEY_SOCKTYPE sock) const;
protected:
    CMonkeyRuleBase(EMonkeyActionType     action_type,
                    const vector<string>& name_value);
    int /* EIO_Status or -1 */ GetReturnStatus(void) const;
    unsigned long  GetDelay(void) const;

    /* Iterate m_Runs */
    void IterateRun(MONKEY_SOCKTYPE sock);
private:
    void           x_ReadRuns     (const string& runs);
    void           x_ReadEIOStatus(const string& eIOStatus_str);
    int                          m_ReturnStatus;
    ERepeatType                  m_RepeatType;
    unsigned long                m_Delay;
    vector<double>               m_Runs;
    EMonkeyActionType            m_ActionType;
    map<MONKEY_SOCKTYPE, size_t> m_RunPos;
    /** If there are no-interception runs before repeating the cycle,
     * we know that from m_RunsSize */
    size_t         m_RunsSize;
    /** Whether trigger settings are stored as probabilities for each run, 
        or as numbers of runs when the rule should trigger */
    ERunMode       m_RunMode;
};


/** Common functionality for CMonkeyWriteRule and CMonkeyReadRule:
 *  - Predefined text or random garbage? (bool m_Garbage)
 *  - Predefined text (string m_Text)
 *  - Text length (unsigned int m_TextLength)
 *  - How to fill response with text if text predefined (EFillType m_FillType)
 *  - What EIO_Status to return (EIO_Status m_ReturnStatus)
 */
class CMonkeyRWRuleBase : public CMonkeyRuleBase
{
public:
    enum EFillType {
        eMonkey_FillRepeat,
        eMonkey_FillLastLetter
    };
protected:
    CMonkeyRWRuleBase(EMonkeyActionType           action_type, 
                      const vector<string>& name_value);
    string    GetText     (void) const;
    size_t    GetTextLength(void) const;
    bool      GetGarbage   (void) const;
    EFillType GetFillType   (void) const;
private:
    void      x_ReadFill(const string& fill_str);
    string    m_Text;
    size_t    m_TextLength;
    bool      m_Garbage;
    EFillType m_FillType;
};


class CMonkeyWriteRule : public CMonkeyRWRuleBase
{
public:
    CMonkeyWriteRule(const vector<string>& name_value);

    MONKEY_RETTYPE  Run(MONKEY_SOCKTYPE        sock,
                        const MONKEY_DATATYPE  data,
                        MONKEY_LENTYPE         size,
                        int                    flags,
                        SOCK*                  sock_ptr);
};


class CMonkeyReadRule : public CMonkeyRWRuleBase
{
public:
    CMonkeyReadRule(const vector<string>& name_value);

    MONKEY_RETTYPE Run(MONKEY_SOCKTYPE sock,
                       MONKEY_DATATYPE buf,
                       MONKEY_LENTYPE  size,
                       int             flags, 
                       SOCK*           sock_ptr);
private:
    /* A socket that gets timeouts*/
    SOCK m_TimeoutingSocket;
};


class CMonkeyConnectRule : public CMonkeyRuleBase
{
public:
    CMonkeyConnectRule(const vector<string>& name_value);
    
    int Run(MONKEY_SOCKTYPE        sock,
            const struct sockaddr* name,
            MONKEY_SOCKLENTYPE     namelen);
private:
    bool m_Allow;
};


class CMonkeyPollRule : public CMonkeyRuleBase
{
public:
    CMonkeyPollRule(const vector<string>& name_value);
    
    bool  Run(size_t*     n,
              SOCK*       sock,
              EIO_Status* return_status);
private:
    map<SOCK, struct timeval> x_Delays;
    bool m_Ignore;
};


class CMonkeyPlan : public CMonkeySeedAccessor
{
public:
/* 
 * write1=text=garbage;return_status=eIO_Closed;text_length=500;runs=100%,0%,50.5%,100%,repeat
 * write2=text='garbage';text_length=500;fill=last_letter;runs=0%,50%,25%,100%,32%,...
 * read=text='read_garbage';text_length=500;fill=repeat;runs=50%;return_status=eIO_Closed
 * connect=allow=no;runs=1,2,3,4,100
 * poll=delay=2000;ignore=yes,runs=1
 */
    CMonkeyPlan(const string& section);

    bool Match (const string&  host, 
                unsigned short port, 
                const string&  url,
                unsigned short probability_left = 100);
    /** Guarantees to have not run send() if returned false. Returning true
        can mean either that send() was run or that send() was simulated to be 
        unsuccessful */
    bool WriteRule     (MONKEY_SOCKTYPE        sock,
                        const MONKEY_DATATYPE  data,
                        MONKEY_LENTYPE         size,
                        int                    flags,
                        MONKEY_RETTYPE*        bytes_written,
                        SOCK*                  sock_ptr);

    bool ReadRule      (MONKEY_SOCKTYPE        sock,
                        MONKEY_DATATYPE        buf,
                        MONKEY_LENTYPE         size,
                        int                    flags,
                        MONKEY_RETTYPE*        bytes_read,
                        SOCK*                  sock_ptr);

    bool ConnectRule   (MONKEY_SOCKTYPE        sock,
                        const struct sockaddr* name,
                        MONKEY_SOCKLENTYPE     namelen,
                        int*                   result);

    bool PollRule      (size_t*                n,
                        SOCK*                  sock,
                        EIO_Status*            return_status);

    unsigned short GetProbabilty(void);
private:
    /**  */
    template<class Rule_Ty>
    void x_LoadRules(const string&    section,
                     const string&    rule_type_str, 
                     vector<Rule_Ty>& container);


    void                           x_ReadPortRange(const string&    conf);
    unsigned short                 m_Probability;
    string                         m_HostRegex;
    /** Port ranges are stored in pairs.
     * odd item - from, even - till (both - inclusive). So for one element
     * in range "from" and "till" are the same. Example:
     * for range "8080-8085, 8089"
     * m_PortRanges = { 8080, 8085, 8089, 8089 } */
    vector<unsigned short>         m_PortRanges;
    vector<CMonkeyWriteRule>       m_WriteRules;
    vector<CMonkeyReadRule>        m_ReadRules;
    vector<CMonkeyConnectRule>     m_ConnectRules;
    vector<CMonkeyPollRule>        m_PollRules;
    /** Only for debugging purposes */
    string                         m_Name;
};


enum EMonkeyHookSwitch {
    eMonkeyHookSwitch_Disabled = 0,
    eMonkeyHookSwitch_Enabled  = 1
};


typedef void (*FMonkeyHookSwitch)(EMonkeyHookSwitch hook_switch);


class CMonkey : public CMonkeySeedAccessor
{
public:
    /** This method is not for public use.
     * CMonkeySeedLogKey is a special class that allows only some certain 
     * class to call this method */
    int GetRand(const CMonkeySeedKey& /* key */);

    /** To be able to reproduce Chaos Monkey behavior, you need to tag each
    * thread with a unique token. Then you can ask Chaos Monkey to reproduce
    * behavior from a previous run, if Monkey saved what it did to a file */
    bool RegisterThread(int token);

    int GetSeed(void);
    void SetSeed(int seed);
    static CMonkey* Instance(void);
    void ReloadConfig(const string& config = "");
    bool IsEnabled(void);

    MONKEY_RETTYPE Send(MONKEY_SOCKTYPE        sock,
                        const MONKEY_DATATYPE  data,
                        MONKEY_LENTYPE         size,
                        int                    flags,
                        SOCK*                  sock_ptr);

    MONKEY_RETTYPE Recv(MONKEY_SOCKTYPE        sock,
                        MONKEY_DATATYPE        buf,
                        MONKEY_LENTYPE         size,
                        int                    flags,
                        SOCK*                  sock_ptr);

    int Connect(MONKEY_SOCKTYPE        sock,
                const struct sockaddr* name,
                MONKEY_SOCKLENTYPE     namelen);

    bool Poll(size_t*      n,
              SSOCK_Poll** polls,
              EIO_Status*  return_status);
    /* Not a real Chaos Monkey interceptor, just a function to remove the socket 
     * that was closed from m_KnownSockets */
    void Close(MONKEY_SOCKTYPE sock);

    static void MonkeyHookSwitchSet(FMonkeyHookSwitch hook_switch_func);
private:
    /* No one can create a separate instance of CMonkey*/
    CMonkey(void);
    /** Return plan for the socket, new or already assigned one. If the socket
     *  is ignored by Chaos Monkey, NULL is returned. 
     * @param[in] match_host 
     *  Tell whether host has to be matched, too. In case of poll() on 
     *  listening socket it is impossible to know host before accept(), so 
     *  the check is omitted. */
    CMonkeyPlan* x_FindPlan(MONKEY_SOCKTYPE sock,  const string& hostname,
                            const string& host_IP, unsigned short port);
    /** Sockets that are already assigned to a plan (or known to be ignored) */
    map<SOCKET, CMonkeyPlan*> m_KnownSockets;
    static CMonkey*           sm_Instance;
    vector<CMonkeyPlan>       m_Plans;
    double                    m_Probability;
    bool                      m_Enabled;
    static FMonkeyHookSwitch  sm_HookSwitch;
    CRef<CTls<int>>           m_TlsToken;
    CRef<CTls<vector<int> > > m_TlsRandList;
    CRef<CTls<int> >          m_TlsRandListPos;
    unsigned int              m_Seed;
    /** Remember registered tokens for threads to avoid collisions */
    set<int>                  m_RegisteredTokens;
};


END_NCBI_SCOPE

#endif /* #ifdef NCBI_MONKEY */

#endif /* CONNECT___NCBI_MONKEY__HPP */