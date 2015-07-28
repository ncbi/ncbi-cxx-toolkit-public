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
 * Author:  Denis Vakatov
 *
 *
 */

/// @file  test_ncbicntr.cpp
/// TEST for:  CAtomicCounter API


#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;



class CTestAtomicApp : public CThreadedApp
{
public:
    virtual bool Thread_Run(int idx);
protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
private:
    CAtomicCounter  m_ACounter;
    int             m_NCounter;
};


static CAtomicCounter s_ACounter;


bool CTestAtomicApp::Thread_Run(int /*idx*/)
{
    for (unsigned int i = 0;  i < 400000;  i++) {
        switch (i % 4) {
        case 0:
            m_ACounter.Add(   1);
            s_ACounter.Add(   1);
            m_NCounter     += 1;
            break;
        case 1:
            m_ACounter.Add(-100);
            s_ACounter.Add(-100);
            m_NCounter   -= 100;
            break;
        case 2:
            m_ACounter.Add(  -1);
            s_ACounter.Add(  -1);
            m_NCounter     -= 1;
            break;
        case 3:
            m_ACounter.Add( 100);
            s_ACounter.Add( 100);
            m_NCounter   += 100;
            break;
        }
    }

    return true;
}


bool CTestAtomicApp::TestApp_Init(void)
{
    m_ACounter.Set(0);
    m_NCounter = 0;
    /* s_ACounter -- must self-init to zero */

    return true;
}


bool CTestAtomicApp::TestApp_Exit(void)
{
    LOG_POST("Member CAtomicCounter = " << m_ACounter.Get());
    LOG_POST("Static CAtomicCounter = " << s_ACounter.Get());
    LOG_POST("Non-Atomic Counter    = " << m_NCounter      );

    assert(m_ACounter.Get() == 0);
    assert(s_ACounter.Get() == 0);

    if (m_NCounter == 0) {
        ERR_POST(Warning << "Non-atomic counter looks atomic today!");
    }

    LOG_POST("Test completed successfully!");
    return true;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestAtomicApp().AppMain(argc, argv);
}
