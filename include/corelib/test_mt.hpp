#ifndef TEST_MT__HPP
#define TEST_MT__HPP

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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Wrapper for testing modules in MT environment
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// Globals

const unsigned int   c_NumThreadsMin = 1;
const unsigned int   c_NumThreadsMax = 500;
const int            c_SpawnByMin    = 1;
const int            c_SpawnByMax    = 100;

extern unsigned int  s_NumThreads;
extern int           s_SpawnBy;


/////////////////////////////////////////////////////////////////////////////
//  Test application
//
//    Core application class for MT-tests
//

class CThreadedApp : public CNcbiApplication
{
public:
    // Override constructor to initialize the application
    CThreadedApp(void);
    ~CThreadedApp(void);

    // Functions to be called by the test thread's constructor, Main(),
    // OnExit() and destructor. All methods should return "true" on
    // success, "false" on failure.
    virtual bool Thread_Init(int idx);
    virtual bool Thread_Run(int idx);
    virtual bool Thread_Exit(int idx);
    virtual bool Thread_Destroy(int idx);

protected:
    // Override this method to execute code before running threads.
    // Return "true" on success.
    virtual bool TestApp_Init(void);

    // Override this method to execute code after all threads termination.
    // Return "true" on success.
    virtual bool TestApp_Exit(void);

private:
    void Init(void);
    int Run(void);
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/04/23 13:12:28  gouriano
 * test_mt.hpp moved into another location
 *
 * Revision 6.5  2002/04/16 18:49:07  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 6.4  2002/04/11 20:00:46  ivanov
 * Returned standard assert() vice CORE_ASSERT()
 *
 * Revision 6.3  2002/04/10 18:38:51  ivanov
 * Moved CVS log to end of file. Changed assert() to CORE_ASSERT()
 *
 * Revision 6.2  2001/05/17 15:05:08  lavr
 * Typos corrected
 *
 * Revision 6.1  2001/04/06 15:53:08  grichenk
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* TEST_MT__HPP */
