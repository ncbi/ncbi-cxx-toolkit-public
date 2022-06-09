#ifndef __NCBI_MESSAGE_QUEUE_HPP__
#define __NCBI_MESSAGE_QUEUE_HPP__
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
* Authors:  Sergiy Gotvyanskyy
*
* File Description:
*   Asynchronous message queue
*
*/

#include <corelib/ncbistl.hpp>
#include <cassert>
#include <deque>
#include <mutex>
#include <condition_variable>


BEGIN_NCBI_NAMESPACE;

template<class _T, class _Trottle>
class CMessageQueue;

class CMessageQueueReady
{
public:
    template<class _Q>
    bool operator()(const _Q&)
    {
        return true;
    }
};
template<size_t N>
class CMessageQueueSizeLimit
{
public:
    template<class _Q>
    bool operator()(const _Q& mq)
    {
        if (mq.get_queue_unsafe().size() > N)
            return false;
        return true;
    }
};

template<class _T, class _Trottle = 
    //CMessageQueueReady 
    CMessageQueueSizeLimit<10>
    >
class CMessageQueue
{
public:
    using value_type = _T;
    using container_type = std::deque<value_type>;
    virtual ~CMessageQueue()
    {
        //std::cout << "CMessageQueue::~CMessageQueue " << m_extrem << std::endl;
    }
    void PostMessage(const value_type& msg)
    {
        value_type copied(msg);
        PostMessage(std::move(copied));
    }
    void PostMessage(value_type&& msg)
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]()->bool
            {
                return m_trottle(*this);
            });
            m_queue.push_back(std::move(msg));
            if (m_extrem < m_queue.size())
                m_extrem = m_queue.size();
        }
        m_cv.notify_all();
    }
    value_type RetrieveMessage()
    {
        value_type message;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]()->bool
            {
                return !m_queue.empty();
            });
            assert(!m_queue.empty());
            message = std::move(m_queue.front());
            m_queue.pop_front();
        }
        m_cv.notify_all();
        return message;
    }
    const container_type& get_queue_unsafe() const
    {
        return m_queue;
    }
protected:
    _Trottle m_trottle;
    container_type m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    size_t m_extrem = 0;
};

END_NCBI_NAMESPACE;

#endif
