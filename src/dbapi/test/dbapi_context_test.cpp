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
 * Author: Sergey Sikorskiy
 *
 * File Description: DBAPI unit-test
 *
 * ===========================================================================
 */

#include "dbapi_unit_test_pch.hpp"

#include <corelib/ncbithr.hpp>


BEGIN_NCBI_SCOPE

////////////////////////////////////////////////////////////////////////////////
class CContextThread : public CThread
{
public:
    CContextThread(CDriverManager& dm, const CTestArguments* args);

protected:
    virtual ~CContextThread(void);
    virtual void* Main(void);
    virtual void  OnExit(void);

    const CTestArguments& GetArgs(void) const
    {
        return *m_Args;
    }

private:
    CRef<const CTestArguments> m_Args;

    CDriverManager&         m_DM;
    IDataSource*            m_DS;
};

///////////////////////////////////////////////////////////////////////////////
CContextThread::CContextThread(CDriverManager& dm,
                               const CTestArguments* args) :
    m_Args(args),
    m_DM(dm),
    m_DS(NULL)
{
}


CContextThread::~CContextThread(void)
{
}


void*
CContextThread::Main(void)
{
    try {
        m_DS = m_DM.MakeDs(GetArgs().GetConnParams());
        return m_DS;
    } catch (const CDB_ClientEx& ex) {
        // Ignore it ...
        _TRACE(ex);
    } catch (...) {
        _ASSERT(false);
    }

    return NULL;
}


void  CContextThread::OnExit(void)
{
    if (m_DS) {
        m_DM.DestroyDs(m_DS);
    }
}


NCBITEST_AUTO_INIT()
{
    CommonInit();
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_DriverContext_One)
{
    enum {eNumThreadsMax = 5};
    void* ok;
    int succeeded_num = 0;
    CRef<CContextThread> thr[eNumThreadsMax];

    // Spawn more threads
    for (int i = 0; i < eNumThreadsMax; ++i) {
        thr[i] = new CContextThread(GetDM(), &GetArgs());
        // Allow threads to run even in single thread environment
        thr[i]->Run(CThread::fRunAllowST);
    }

    // Wait for all threads
    for (unsigned int i = 0; i < eNumThreadsMax; ++i) {
        thr[i]->Join(&ok);
        if (ok != NULL) {
            ++succeeded_num;
        }
    }

    BOOST_CHECK(succeeded_num >= 1);
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_DriverContext_Many)
{
    enum {eNumThreadsMax = 5};
    void* ok;
    int succeeded_num = 0;
    CRef<CContextThread> thr[eNumThreadsMax];

    // Spawn more threads
    for (int i = 0; i < eNumThreadsMax; ++i) {
        thr[i] = new CContextThread(GetDM(), &GetArgs());
        // Allow threads to run even in single thread environment
        thr[i]->Run(CThread::fRunAllowST);
    }

    // Wait for all threads
    for (unsigned int i = 0; i < eNumThreadsMax; ++i) {
        thr[i]->Join(&ok);
        if (ok != NULL) {
            ++succeeded_num;
        }
    }

    BOOST_CHECK_EQUAL(succeeded_num, int(eNumThreadsMax));
}


END_NCBI_SCOPE

