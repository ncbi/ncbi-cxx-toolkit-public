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

/// @file hspstream_queue.hpp
/// C++ implementation of the BlastHSPStream interface for producing results 
/// on the fly.

#ifndef ALGO_BLAST_API___HSPSTREAM_QUEUE_HPP
#define ALGO_BLAST_API___HSPSTREAM_QUEUE_HPP

#include <corelib/ncbimtx.hpp>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_hspstream.h>
#include <list>

BEGIN_NCBI_SCOPE

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_SCOPE(blast)

/// Data structure for the queue implementation of BlastHSPStream
class NCBI_XBLAST_EXPORT CBlastHSPListQueueData
{
public:
    /// Constructor
    CBlastHSPListQueueData();
    /// Destructor
    ~CBlastHSPListQueueData();
    /// Read from the queue
    int Read(BlastHSPList** hsp_list );
    /// Write to the queue
    int Write(BlastHSPList** hsp_list);
    /// Close the queue for writing
    void Close();
    /// Check if wait on semaphore is needed
    bool NeedWait();
private:
    std::list<BlastHSPList*> m_resultsQueue; /**< Queue in a form of a linked 
                                                list of HSP lists. */
    bool m_writingDone; /**< Has writing to the queue finished? */
    CFastMutex m_Mutex; /**< Mutex for writing to the queue. */
    CSemaphore* m_Sema; /**< Semaphore fore reading from the queue. */
};

/// Function to initialize a queue HSP stream.
NCBI_XBLAST_EXPORT
BlastHSPStream* Blast_HSPListCQueueInit();

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2005/03/30 16:21:27  jcherry
* Added export specifier
*
* Revision 1.4  2005/03/29 15:58:54  dondosha
* Added blast scope
*
* Revision 1.3  2004/08/11 11:59:07  ivanov
* Added export specifier NCBI_XBLAST_EXPORT
*
* Revision 1.2  2004/06/22 16:24:39  dondosha
* Added doxygen comments
*
* Revision 1.1  2004/06/15 16:25:23  dondosha
* Implementation of an HSP list queue
*
*
* ===========================================================================
*/
#endif /* ALGO_BLAST_API___HSPSTREAM_QUEUE_HPP */
