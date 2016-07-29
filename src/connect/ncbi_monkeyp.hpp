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

#ifndef CONNECT___NCBI_MONKEYP__HPP
#define CONNECT___NCBI_MONKEYP__HPP

#   include <connect/ncbi_monkey.hpp>

BEGIN_NCBI_SCOPE


/* A special class which opens access to writing to Monkey Action Log*/


#ifdef NCBI_MONKEY_TESTS


using namespace std;

/* Generate mock function declarations like:
 *
 *   bool g_MonkeyMock_GetInterceptedRecv();
 *   void g_MonkeyMock_SetInterceptedRecv(const bool& val);
 */
#define MONKEY_MOCK_MACRO()                                                    \
DECLARE_MONKEY_MOCK(bool,       InterceptedRecv,    false);                    \
DECLARE_MONKEY_MOCK(bool,       InterceptedSend,    false);                    \
DECLARE_MONKEY_MOCK(bool,       InterceptedConnect, false);                    \
DECLARE_MONKEY_MOCK(bool,       InterceptedPoll,    false);                    \
DECLARE_MONKEY_MOCK(string,     LastRecvContent,    "");                       \
DECLARE_MONKEY_MOCK(string,     LastSendContent,    "");                       \
DECLARE_MONKEY_MOCK(EIO_Status, LastRecvStatus,     eIO_Success);              \
DECLARE_MONKEY_MOCK(EIO_Status, LastSendStatus,     eIO_Success);              \
DECLARE_MONKEY_MOCK(EIO_Status, LastConnectStatus,  eIO_Success);              \
DECLARE_MONKEY_MOCK(EIO_Status, LastPollStatus,     eIO_Success);


#undef DECLARE_MONKEY_MOCK
#define DECLARE_MONKEY_MOCK(ty,name,def_val/*not used*/)                       \
    ty g_MonkeyMock_Get ## name();                                             \
    void g_MonkeyMock_Set ## name(const ty& val);

MONKEY_MOCK_MACRO()
void g_Monkey_Foo();


class CMonkeySpy
{

};


class CMonkeyMocks
{
public:
    static void Reset();
    static string GetMainMonkeySection();
    static void SetMainMonkeySection(string section);
private:
    static string MainMonkeySection;
};


class CMonkeyMockCleanup
{
public:
    CMonkeyMockCleanup()
    {
        CMonkeyMocks::Reset();
    }
    ~CMonkeyMockCleanup()
    {
        CMonkeyMocks::Reset();
    }
};


#endif /* #ifdef NCBI_MONKEY_TESTS */

END_NCBI_SCOPE

#endif /* CONNECT___NCBI_MONKEYP__HPP */