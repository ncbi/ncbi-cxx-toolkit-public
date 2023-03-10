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

class TMessageQueueNLimit
{
public:
    TMessageQueueNLimit() = default;
    TMessageQueueNLimit(size_t _limit) : m_limit{_limit} {};
    template<class _Q>
    bool operator()(const _Q& mq) const
    {
        return (mq.size() < m_limit);
    }
    size_t m_limit = 1;
};


template<class _T, class _Trottle>
class TMessageQueue
{
public:
    using value_type = _T;
    using container_type = std::deque<value_type>;
    static constexpr bool type_requirements =
        std::is_default_constructible<value_type>::value &&
        std::is_move_constructible<value_type>::value &&
        std::is_move_assignable<value_type>::value;

    static_assert(type_requirements, "_T does not satisfy requirements");

    TMessageQueue() = default;
    TMessageQueue(const _Trottle& _tr) : m_trottle{_tr} {};
    ~TMessageQueue() {}
    void push_back(value_type msg)
    { // notice passing value_type as a value

        if (m_canceled){
            return;
        }

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]()->bool
            { // the user supposed to limit the m_queue size by some measure
                const auto& const_ref = m_queue;
                return m_trottle(const_ref);
            });

            if (!m_canceled) {
                m_queue.push_back(std::move(msg));
                if (m_extrem < m_queue.size())
                    m_extrem = m_queue.size();
            }
        }
        m_cv.notify_all();
    }

    void cancel()
    {
        m_canceled = true;
        clear();
    }

    void clear()
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            //m_cv.wait(lock, []{return true;});
            m_queue.clear();
        }
        m_cv.notify_all();
    }

    value_type pop_front()
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
    _Trottle& Trottle() { return m_trottle; }
private:
    _Trottle                m_trottle;
    container_type          m_queue;
    std::mutex              m_mutex;
    std::condition_variable m_cv;
    size_t                  m_extrem = 0;
    atomic_bool             m_canceled{false};
};

template<class _T>
using CMessageQueue = TMessageQueue<_T, TMessageQueueNLimit>;

END_NCBI_NAMESPACE;

#endif
