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

/** @file hspstream_queue.cpp
 * C++ implementation of the BlastHSPStream interface for producing 
 * BLAST results on the fly.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <algo/blast/api/hspstream_queue.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Constructor for the queue HSP stream data class
CBlastHSPListQueueData::CBlastHSPListQueueData()
{
    // Set maximal count to INT4_MAX, since we don't want to bound the 
    // number of HSP lists saved in the queue.
    m_Sema = new CSemaphore(0, INT4_MAX);
    m_writingDone = false;
}

/// Destructor for the queue HSP stream data class
CBlastHSPListQueueData::~CBlastHSPListQueueData()
{
    // Delete any HSP lists remaining in the queue.
    ITERATE(list<BlastHSPList*>, itr, m_resultsQueue) {
        Blast_HSPListFree(*itr);
    } 
    
    delete m_Sema;
}

/// Checks whether wait on a semaphore is needed before reading from the 
/// results queue.
bool CBlastHSPListQueueData::NeedWait()
{
   CFastMutexGuard g(m_Mutex);
   return (!m_writingDone && m_resultsQueue.empty());
}

/** Read an HSP list from the results queue. Wait on a semaphore until there
 * is something to read, or until queue is closed for writing.
 * @param hsp_list The read HSP list [out]
 * @return Status: success, error or end of reading.
 */
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

/** Insert an HSP list into the queue.
 * @param hsp_list The HSP list to insert. [in]
 * @return Status: success or error, if writing is not allowed.
 */
int CBlastHSPListQueueData::Write(BlastHSPList** hsp_list)
{
    /* If input is Null, don't do anything, but return success */
    if (*hsp_list == NULL)
        return kBlastHSPStream_Success;
    /* If input HSP list is empty, free it and return success */
    if ((*hsp_list)->hspcnt == 0) {
        *hsp_list = Blast_HSPListFree(*hsp_list);
        return kBlastHSPStream_Success;
    }    

    CFastMutexGuard g(m_Mutex);
    if (m_writingDone)
        return kBlastHSPStream_Error;
    
    m_resultsQueue.push_back(*hsp_list);
    *hsp_list = NULL;
    m_Sema->Post();
    return kBlastHSPStream_Success;
}

/// Close the queue for writing.
void CBlastHSPListQueueData::Close()
{
    CFastMutexGuard g(m_Mutex);
    m_writingDone = true;
    /* Increment the semaphore count so the reading thread can get out of 
     * the waiting state and check the m_writingDone variable. */
    m_Sema->Post();
}

extern "C" {

/** Deallocate memory for the HSP stream.
 * @param hsp_stream HSP stream to free. [in]
 * @return NULL.
 */
BlastHSPStream* BlastHSPListCQueueFree(BlastHSPStream* hsp_stream) 
{
   CBlastHSPListQueueData* stream_data = 
      (CBlastHSPListQueueData*) GetData(hsp_stream);

   delete stream_data;
   sfree(hsp_stream);
   return NULL;
}

/** Read from an HSP stream.
 * @param hsp_stream Stream to read from [in]
 * @param hsp_list The read HSP list [out]
 * @return Status: success, error or end of reading.
 */
int BlastHSPListCQueueRead(BlastHSPStream* hsp_stream, 
                           BlastHSPList** hsp_list) 
{
   CBlastHSPListQueueData* stream_data = 
      (CBlastHSPListQueueData*) GetData(hsp_stream);

   return stream_data->Read(hsp_list);
}

/** Write to the HSP stream. 
 * @param hsp_stream Stream to write to [in]
 * @param hsp_list HSP list to write to the stream [in]
 * @return Status: success or error.
 */
int BlastHSPListCQueueWrite(BlastHSPStream* hsp_stream, 
                            BlastHSPList** hsp_list)
{
   CBlastHSPListQueueData* stream_data = 
      (CBlastHSPListQueueData*) GetData(hsp_stream);

   return stream_data->Write(hsp_list);
}

/** Close the HSP stream for future writing.
 * @param hsp_stream Stream to close [in]
 */
void BlastHSPListCQueueClose(BlastHSPStream* hsp_stream)
{
   CBlastHSPListQueueData* stream_data = 
      (CBlastHSPListQueueData*) GetData(hsp_stream);
   stream_data->Close();
}

/** Initialize the function pointers and data structure for a BlastHSPStream.
 * @param hsp_stream HSP stream to initialize [in] [out]
 * @param args Pointer to an instance of the CBlastHSPListQueueData class. [in]
 * @return Filled HSP stream.
 */
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

/** Initialize a BlastHSPStream with a queue data structure.
 * @return The initialized HSP stream.
 */
BlastHSPStream* Blast_HSPListCQueueInit()
{
    CBlastHSPListQueueData* stream_data = 
        new CBlastHSPListQueueData();

    BlastHSPStreamNewInfo info;

    info.constructor = &BlastHSPListCQueueNew;
    info.ctor_argument = (void*)stream_data;

    return BlastHSPStreamNew(&info);
}

END_SCOPE(blast)
END_NCBI_SCOPE
