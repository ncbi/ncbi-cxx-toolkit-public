#ifndef CORELIB___TEST_MT__HPP
#define CORELIB___TEST_MT__HPP

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
 *
 */

/// @file test_mt.hpp
///   Wrapper for testing modules in MT environment


#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>



BEGIN_NCBI_SCOPE

/** @addtogroup MTWrappers
 *
 * @{
 */

/////////////////////////////////////////////////////////////////////////////
// Globals

/// Minimum number of threads.
const unsigned int k_NumThreadsMin = 1;

/// Maximum number of threads.
const unsigned int k_NumThreadsMax = 500;

/// Minimum number of spawn by threads.
const int k_SpawnByMin = 1;

/// Maximum number of spawn by threads.
const int k_SpawnByMax = 100;

extern unsigned int  s_NumThreads;
extern int           s_SpawnBy;


/////////////////////////////////////////////////////////////////////////////
//  Test application
//
//    Core application class for MT-tests
//



/////////////////////////////////////////////////////////////////////////////
///
/// CThreadedApp --
///
/// Basic NCBI threaded application class.
///
/// Defines the high level behavior of an NCBI threaded application.

class NCBI_TEST_MT_EXPORT CThreadedApp : public CNcbiApplication
{
public:
    /// Constructor.
    ///
    /// Override constructor to initialize the application.
    CThreadedApp(void);

    /// Destructor.
    ~CThreadedApp(void);

    // Functions to be called by the test thread's constructor, Main(),
    // OnExit() and destructor. All methods should return "true" on
    // success, "false" on failure.

    /// Initialize the thread.
    ///
    /// Called from thread's constructor.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool Thread_Init(int idx);

    /// Run the thread.
    ///
    /// Called from thread's Main() method.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool Thread_Run(int idx);

    /// Exit the thread.
    ///
    /// Called from thread's OnExit() method.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool Thread_Exit(int idx);

    /// Destroy the thread.
    ///
    /// Called from thread's destroy() method.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool Thread_Destroy(int idx);

protected:

    /// Thread group description parameters.
    struct SThreadGroup {
        /// Number of threads in the group.
        unsigned int number_of_threads;
        /// TRUE, if the group has intra-group sync point.
        bool has_sync_point;
    };

    /// Override this method to add your custom arguments.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool TestApp_Args( CArgDescriptions& args);

    /// Override this method to execute code before running threads.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool TestApp_Init(void);

    /// Override this method to execute code after all threads terminate.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool TestApp_Exit(void);

    /////////////////////////////////////////////////////////////////////////
    // The following API has to do with thread groups synchronization

    /// Wait until other threads in group reach the same sync point
    /// The call may be ignored by the application
    void TestApp_IntraGroupSyncPoint(void);

    /// Wait until ALL threads reach a call of TestApp_GlobalSyncPoint().
    ///
    /// @note                   Calling TestApp_GlobalSyncPoint() 
    ///                         automatically awakes all threads that would 
    ///                         be otherwise awaken only with 
    ///                         TestApp_DelayedStartSyncPoint().
    /// @note                   You can use this method if you want threads
    ///                         to be synchronized across different branches
    ///                         of code. The only thing to keep in mind is 
    ///                         that EVERY thread has to reach 
    ///                         TestApp_GlobalSyncPoint() for execution 
    ///                         to be continued.
    void TestApp_GlobalSyncPoint(void);

    /// Set allowed range of delayed-start sync points.
    /// The real number will be chosen randomly.
    /// Note:   Specifying wrong number will cause inner assert() to fail
    void SetNumberOfDelayedStartSyncPoints(unsigned int num_min,
                                           unsigned int num_max);
    
    /// When some thread reaches call of this function, it will with 
    /// some probability start other threads that were sleeping since 
    /// the beginning of the test. Please note that new threads 
    /// will start from the beginning of Thread_Run(), not from anywhere
    /// in the middle.
    ///
    /// @param name
    ///   Name can be anything you like, providing other
    ///   threads use the same name. The test application distinguishes
    ///   delayed-start sync points by name.
    void TestApp_DelayedStartSyncPoint(const string& name);

private:

    /// Initialize the app.
    void Init(void);

    /// Run the threads.
    int Run(void);

    void x_InitializeThreadGroups(void);
    void x_PrintThreadGroups(void);
    unsigned int x_InitializeDelayedStart(void);
    void x_StartThreadGroup(unsigned int count);

    CFastMutex m_AppMutex;
    set<string> m_Reached;
    unsigned int m_Min, m_Max;
    volatile unsigned int m_NextGroup;
    vector<unsigned int> m_Delayed;
    vector<SThreadGroup> m_ThreadGroups;

protected:
    unsigned int m_LogMsgCount;
};


END_NCBI_SCOPE


/* @} */

#endif  /* TEST_MT__HPP */
