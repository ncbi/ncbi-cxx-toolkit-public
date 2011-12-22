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

/** @file hspstream_test_util.cpp
 * Auxiliary utilities needed for testing HSP stream interface.
 */

#include <ncbi_pch.hpp>
#include "hspstream_test_util.hpp"
#include <algo/blast/core/blast_hits.h>

BlastHSPList* 
setupHSPList(int score, int num_queries, int oid)
{
    BlastHSPList* hsp_list = Blast_HSPListNew(0);
    int index;
    for (index = 0; index < num_queries; ++index) {
        hsp_list->hsp_array[index] = Blast_HSPNew();
        hsp_list->hsp_array[index]->context = index;
        hsp_list->hsp_array[index]->score = score;
    }
    hsp_list->hspcnt = num_queries;
    hsp_list->oid = oid;
    return hsp_list;
}

CHspStreamWriteThread::CHspStreamWriteThread(BlastHSPStream* hsp_stream, 
                       int index, int nthreads, int total, int nqueries)
    : m_ipHspStream(hsp_stream), m_iIndex(index), m_iNumThreads(nthreads),
      m_iTotal(total), m_iNumQueries(nqueries)
{
}

CHspStreamWriteThread::~CHspStreamWriteThread()
{
}

void* CHspStreamWriteThread::Main(void)
{
    const int max_score = 100;
    int index;
    int status;
    BlastHSPList* hsp_list;

    for (index = m_iIndex; index < m_iTotal; index += m_iNumThreads) {
        hsp_list = 
            setupHSPList(rand() % max_score, m_iNumQueries, index);
        status = BlastHSPStreamWrite(m_ipHspStream, &hsp_list);
        if (status != kBlastHSPStream_Success)
            abort();
        ASSERT(hsp_list == NULL);
    }
    return NULL;
}

void CHspStreamWriteThread::OnExit(void)
{ 
}
