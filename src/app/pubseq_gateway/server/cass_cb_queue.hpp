#ifndef CASS_CB_QUEUE__HPP
#define CASS_CB_QUEUE__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */


#include <atomic>
#include <vector>
#include <stdexcept>
#include <bit>
#include <cassert>
using namespace std;


struct SPSGSCallbackPayload
{
    weak_ptr<void>  m_Fetch;
};


// The queue will double its capacity as needed
class CPSGS_CallbackQueue
{
    private:
        mutex                           m_QueueMutex;
        vector<SPSGSCallbackPayload>    m_Buffer;

        size_t                          m_Capacity;
        size_t                          m_Mask;

        // Using simple linear indices that wrap naturally via bitmasking
        size_t                          m_WriteIndex{0};
        size_t                          m_ReadIndex{0};

        // Internal helper to resize the buffer when a historic burst happens
        void x_GrowQueue(void)
        {
            size_t      old_capacity = m_Capacity;
            size_t      new_capacity = old_capacity * 2;

            vector<SPSGSCallbackPayload>    new_buffer(new_capacity);
            size_t                          new_mask = new_capacity - 1;

            // Copy elements from the old ring buffer to the beginning of the new ring buffer
            size_t      write_ptr = 0;
            for (size_t  i = m_ReadIndex; i != m_WriteIndex; ++i) {
                new_buffer[write_ptr & new_mask] = m_Buffer[i & m_Mask];
                write_ptr++;
            }

            m_Buffer = move(new_buffer);
            m_Capacity = new_capacity;
            m_Mask = new_mask;
            m_ReadIndex = 0;
            m_WriteIndex = write_ptr;
        }

    public:
        explicit CPSGS_CallbackQueue(size_t  requested_capacity)
        {
            // Ensure capacity is a power of two for lightning-fast bitmask wrapping
            m_Capacity = 1;
            while (m_Capacity < requested_capacity) {
                m_Capacity <<= 1;
            }
            m_Mask = m_Capacity - 1;
            m_Buffer.resize(m_Capacity);
        }

        // Called concurrently by Cassandra threads targeting this specific worker
        void Push(weak_ptr<void>  fetch)
        {
            lock_guard<mutex>   lock(m_QueueMutex);

            // If the ring buffer is completely full, grow it exponentially
            if (m_WriteIndex - m_ReadIndex >= m_Capacity) {
                x_GrowQueue();
            }

            m_Buffer[m_WriteIndex & m_Mask] = {fetch};
            ++m_WriteIndex;
        }

        // Called EXCLUSIVELY by the single worker thread that owns this instance
        bool Pop(SPSGSCallbackPayload &  out_payload)
        {
            lock_guard<mutex>   lock(m_QueueMutex);

            if (m_ReadIndex == m_WriteIndex) {
                return false; // Queue is empty
            }

            out_payload = m_Buffer[m_ReadIndex & m_Mask];

            // Explicitly clear weak_ptr immediately to drop control blocks early
            m_Buffer[m_ReadIndex & m_Mask].m_Fetch.reset();

            ++m_ReadIndex;
            return true;
        }
};

#endif

