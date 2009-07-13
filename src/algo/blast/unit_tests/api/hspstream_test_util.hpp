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
 * Author: Ilya Dondoshansky
 *
 */

/** @file hspstream_test_util.hpp
 * Auxiliary utilities needed for testing HSP stream interface.
 */

#ifndef _HSPSTREAM_TEST_UTIL_HPP
#define _HSPSTREAM_TEST_UTIL_HPP

#ifdef Main
#undef Main
#endif
 
#include <corelib/ncbithr.hpp>
#include <algo/blast/core/blast_hspstream.h>

USING_NCBI_SCOPE;

class CHspStreamWriteThread : public CThread
{
public:
    CHspStreamWriteThread(BlastHSPStream* hsp_stream, int index, 
                          int nthreads, int total, int nqueries);
    ~CHspStreamWriteThread();
protected:
    virtual void* Main(void);
    virtual void OnExit(void);
private:
    BlastHSPStream* m_ipHspStream;
    int m_iIndex;
    int m_iNumThreads;
    int m_iTotal;
    int m_iNumQueries;
};

// Function to set up an HSP list for hspstream unit tests.
BlastHSPList* 
setupHSPList(int score, int num_queries, int oid);


#endif // _HSPSTREAM_TEST_UTIL_HPP
