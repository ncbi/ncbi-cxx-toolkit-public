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
 * Author:  Ilya Dondoshansky
 *
 */

/** @file hspstream_queue.c
 * C++ implementation of the BlastHSPStream interface for producing 
 * BLAST results on the fly.
 */

static char const rcsid[] = 
    "$Id$";

#include <ncbi_pch.hpp>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/api/hspstream_queue.hpp>

USING_NCBI_SCOPE;

CBlastHSPListQueueData::CBlastHSPListQueueData()
{
    // Set maximal count to INT4_MAX, since we don't want to bound the 
    // number of HSP lists saved in the queue.
    m_Sema = new CSemaphore(0, INT4_MAX);
    m_writingDone = false;
}

CBlastHSPListQueueData::~CBlastHSPListQueueData()
{
    // Delete any HSP lists remaining in the queue.
    ITERATE(list<BlastHSPList*>, itr, m_resultsQueue) {
        Blast_HSPListFree(*itr);
    } 
    
    delete m_Sema;
}

bool CBlastHSPListQueueData::NeedWait()
{
   CFastMutexGuard g(m_Mutex);
   return (!m_writingDone && m_resultsQueue.empty());
}

int CBlastHSPListQueueData::Read(BlastHSPList** hsp_list)
{
   int status = kBlastHSPStream_Error;
   
   while (NeedWait()) {
       /* Decrement the semaphore count to 0, then wait for it to be 
        * incremented. */
       m_Sema->Wait();
   }
   // Now lock the mutex and read from the queue
   CFastMutexGuard g(m_Mutex);
   if (m_resultsQueue.empty()) {
       *hsp_list = NULL;
       status = kBlastHSPStream_Eof;
   } else {
       *hsp_list = m_resultsQueue.front();
       m_resultsQueue.pop_front();
       status = kBlastHSPStream_Success;
   }
   return status;
}

int CBlastHSPListQueueData::Write(BlastHSPList** hsp_list)
{
    /* If input is empty, don't do anything, but return success */
    if (*hsp_list == NULL)
        return kBlastHSPStream_Success;
    
    CFastMutexGuard g(m_Mutex);
    if (m_writingDone)
        return kBlastHSPStream_Error;
    
    m_resultsQueue.push_back(*hsp_list);
    *hsp_list = NULL;
    m_Sema->Post();
    return kBlastHSPStream_Success;
}

void CBlastHSPListQueueData::Close()
{
    CFastMutexGuard g(m_Mutex);
    m_writingDone = true;
    /* Increment the semaphore count so the reading thread can get out of 
     * the waiting state and check the m_writingDone variable. */
    m_Sema->Post();
}

extern "C" {

BlastHSPStream* BlastHSPListCQueueFree(BlastHSPStream* hsp_stream) 
{
   CBlastHSPListQueueData* stream_data = 
      (CBlastHSPListQueueData*) GetData(hsp_stream);

   delete stream_data;
   sfree(hsp_stream);
   return NULL;
}

int BlastHSPListCQueueRead(BlastHSPStream* hsp_stream, 
                           BlastHSPList** hsp_list) 
{
   CBlastHSPListQueueData* stream_data = 
      (CBlastHSPListQueueData*) GetData(hsp_stream);

   return stream_data->Read(hsp_list);
}

int BlastHSPListCQueueWrite(BlastHSPStream* hsp_stream, 
                            BlastHSPList** hsp_list)
{
   CBlastHSPListQueueData* stream_data = 
      (CBlastHSPListQueueData*) GetData(hsp_stream);

   return stream_data->Write(hsp_list);
}

void BlastHSPListCQueueClose(BlastHSPStream* hsp_stream)
{
   CBlastHSPListQueueData* stream_data = 
      (CBlastHSPListQueueData*) GetData(hsp_stream);
   stream_data->Close();
}

BlastHSPStream* 
BlastHSPListCQueueNew(BlastHSPStream* hsp_stream, void* args) 
{
    BlastHSPStreamFunctionPointerTypes fnptr;

    fnptr.dtor = &BlastHSPListCQueueFree;
    SetMethod(hsp_stream, eDestructor, fnptr);
    fnptr.method = &BlastHSPListCQueueRead;
    SetMethod(hsp_stream, eRead, fnptr);
    fnptr.method = &BlastHSPListCQueueWrite;
    SetMethod(hsp_stream, eWrite, fnptr);
    fnptr.closeFn = &BlastHSPListCQueueClose;
    SetMethod(hsp_stream, eClose, fnptr);

    SetData(hsp_stream, args);
    return hsp_stream;
}

}   // end extern "C"

BlastHSPStream* Blast_HSPListCQueueInit()
{
    CBlastHSPListQueueData* stream_data = 
        new CBlastHSPListQueueData();

    BlastHSPStreamNewInfo info;

    info.constructor = &BlastHSPListCQueueNew;
    info.ctor_argument = (void*)stream_data;

    return BlastHSPStreamNew(&info);
}
