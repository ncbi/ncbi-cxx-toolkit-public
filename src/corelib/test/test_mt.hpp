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
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2001/04/06 15:53:08  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <assert.h>

BEGIN_NCBI_SCOPE


// Verification function to be used in user-provided tests
#ifndef _DEBUG
inline
void s_Verify(bool expr)
{
    if ( !expr ) {
        throw runtime_error("Test failed");
    }
}
#else
#define s_Verify( expr ) assert( expr )
#endif


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
    // Ovverride constructor to initialize the application
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

#endif  /* TEST_MT__HPP */
