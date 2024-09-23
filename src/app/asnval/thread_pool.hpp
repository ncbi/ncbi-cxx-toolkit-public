#ifndef ASNVAL_THREAD_POOL_INCLUDED
#define ASNVAL_THREAD_POOL_INCLUDED

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
 * Author: Sergiy Gotvyanskyy
 *
 * File Description : asnval-specific thread pool
 *
 */


#include <optional>
#include <future>
#include <util/message_queue.hpp>
#include <objtools/writers/atomics.hpp>

namespace ncbi
{

template<typename _T>
class TPreAllocatedResourcePool
{
public:
    using TStack = TAtomicStack<_T>;
    using value_type = typename TStack::value_type;
    using TNode = typename TStack::TNode;

    TPreAllocatedResourcePool()
    {
    }

    ~TPreAllocatedResourcePool()
    {
        auto ptr = m_pool.release();
        if (ptr)
            delete[] ptr;
    }

    template<typename...TArgs>
    void init(unsigned pool_size, TArgs&&...args)
    {
        if (m_pool)
            throw std::runtime_error("Resource pool already initialized");

        m_pool_size = pool_size;
        m_pool.reset(new TNode[pool_size]);
        for (size_t i=0; i<m_pool_size; ++i)
        {
            TNode& rec  = m_pool.get()[i];
            rec->init(std::forward<TArgs>(args)...);
            m_stack.push_front(&rec);
        }
    }

    [[nodiscard]] value_type* allocate()
    {
        auto node = m_stack.pop_front();

        while (node == nullptr)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            while ((node = m_stack.pop_front()) == nullptr)
                m_cv.wait(lock);
        }

        value_type& v = *node;
        return &v;
    }

    void reuse(value_type* vv)
    {
        for (size_t i=0; i<m_pool_size; ++i)
        {
            TNode& node  = m_pool.get()[i];
            value_type& v = node;
            if (&v == vv)
            {
                m_stack.push_front(&node);
                m_cv.notify_one();
                return;
            }
        }
    }

private:
    size_t                  m_pool_size = 0;
    std::unique_ptr<TNode>  m_pool = nullptr;
    TStack                  m_stack;
    std::mutex              m_mutex;
    std::condition_variable m_cv;
};

class CThreadPoolCore
{
public:

    CThreadPoolCore(unsigned pool_size)
    {
        m_threads.init(pool_size, this);
    }

    ~CThreadPoolCore()
    {
        if (!m_cancelled)
            finish_threads();
    }

    void wait()
    {
        finish_threads();
    }

    // start task and return std::future which is used to deliver results
    template<typename _Function, typename...TArgs,
        typename _Token = std::invoke_result_t<_Function, TArgs...>,
        typename = std::enable_if_t<!std::is_void_v<_Token>>  // prevents void(...) functions
        >
    [[nodiscard]] auto launch(_Function&& f, TArgs&&...args)
    {
        std::shared_ptr<std::promise<_Token>> prom{new std::promise<_Token>};
        std::future<_Token>  fut = prom->get_future();

        auto deliver = [this, prom] (_Token&& result)
        {
            prom->set_value(std::move(result));
        };

        schedule(deliver, std::forward<_Function>(f), std::forward<TArgs>(args)...);

        return fut;
    }

    // places task in a pipeline, result delivered through 'deliver' method
    template<typename _Deliver, typename _Function, typename...TArgs,
        typename _Token = std::invoke_result_t<_Function, TArgs...>,
        typename = std::enable_if_t<!std::is_void_v<_Token>>,  // prevents void(...) functions
        typename = std::enable_if_t<std::is_void_v<std::invoke_result_t<_Deliver, _Token&&>>>
        >
    void schedule(_Deliver deliver, _Function f, TArgs&&...args)
    {
        auto task = make_task(f, std::forward<TArgs>(args)...);
        x_schedule<_Token>(task, deliver);
    }

    // cancel all workers and wait threads to finish
    // already started tasks are not aborted
    void finish_threads()
    {
        if (m_cancelled)
            throw std::runtime_error("Thread pool already cancelled");

        m_cancelled = true;

        while(m_running_threads.load()) {
            auto th = m_threads.allocate();
            th->join();
        }
    }

    template<typename _Function, typename...TArgs>
    static
    auto make_task(_Function f, TArgs&&...args)
    {
        if constexpr (sizeof...(TArgs) == 0) {
            return std::forward<_Function>(f);
        } else {
            using tuple_type = std::tuple<std::decay_t<TArgs>...>;
            tuple_type params{std::forward<TArgs>(args)...};
            static constexpr auto size = std::tuple_size<tuple_type>::value;

            auto task = [params, f] () {
                return x_call_func(f, params, std::make_index_sequence<size>{});
            };
            return task;
        }
    }

    bool is_cancelled() const { return m_cancelled; }

private:
    class TWorker;

    template<typename _Token>
    void x_schedule(std::function<_Token()> work, std::function<void(_Token&&)> deliver)
    {
        auto th = x_get_worker();
        th->set_work(std::move(work), deliver);
    }

    template<typename Function, typename Tuple, size_t ... I>
    static
    auto x_call_func(Function&& f, Tuple& t, std::index_sequence<I ...>)
    {
        return f(std::get<I>(t) ...);
    }

    void x_return_worker(TWorker* worker)
    {
        m_threads.reuse(worker);
    }

    [[nodiscard]] TWorker* x_get_worker()
    {
        if (m_cancelled)
            throw std::runtime_error("Thread pool already cancelled");

        return m_threads.allocate();
    }

    class TWorker
    {
    public:
        using TWork = std::function<void()>;

        TWorker()
        {
        }

        void join()
        {
            if (m_thread.valid()) {
                m_cv.notify_one();
                m_thread.wait();
            }
        }

        void init(CThreadPoolCore* owner)
        {
            m_owner = owner;
        }

        template<typename _Work, typename _Deliver>
        void set_work(_Work&& work, _Deliver&& deliver)
        {
            TWork intwork = [work, deliver]()
            {
                auto result = work();
                deliver(std::move(result));
            };
            x_setwork(std::move(intwork));
        }

    private:
        CThreadPoolCore*        m_owner = nullptr;
        std::optional<TWork>    m_work;
        std::mutex              m_mutex;
        std::condition_variable m_cv;
        std::future<void>       m_thread;

        void x_setwork(TWork&& work)
        {
            if (m_owner->m_cancelled)
                throw std::runtime_error("Thread pool already cancelled");

            if (!m_thread.valid()) {
                m_thread = std::async([this]()
                {
                    x_run();
                });
                m_owner->m_running_threads++;
            }

            {
                std::unique_lock<std::mutex> lock(m_mutex);

                if (!m_owner->m_cancelled) {
                    m_work = std::move(work);
                }
            }
            m_cv.notify_one();
        }

        void x_run()
        {
            while(!m_owner->m_cancelled)
            {
                std::optional<TWork> work;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait(lock, [this]()->bool
                    {
                        return m_work || m_owner->m_cancelled;
                    });

                    if (m_work) {
                        m_work.swap(work);
                    } else {
                        continue;
                    }
                }

                if (work) {
                    m_cv.notify_one();
                    work.value()();
                    m_owner->x_return_worker(this);
                }
            }
            m_owner->m_running_threads--;
        }
    };

    TPreAllocatedResourcePool<TWorker> m_threads; // the pool of thread workers
    std::atomic_bool         m_cancelled{false};
    std::atomic<unsigned>    m_running_threads{0};
};

template<class _T>
class TThreadPoolLimitedEx
{
public:
    using token_type = _T;

    using TWork = std::function<token_type()>;

    TThreadPoolLimitedEx(unsigned thread_pool_size):
        TThreadPoolLimitedEx(1'000'000, thread_pool_size, thread_pool_size) {}

    TThreadPoolLimitedEx(unsigned work_queue_size, unsigned thread_pool_size, unsigned product_queue_depth):
        m_thread_pool{thread_pool_size},
        m_product_queue{product_queue_depth},
        m_works{work_queue_size}
    {}

    ~TThreadPoolLimitedEx()
    {
        if (!m_thread_pool.is_cancelled())
            wait();
    }

    void wait()
    {
        if (m_push_thread.valid()) {
            m_push_thread.get();
        }
    }

    template<typename Function, typename...TArgs>
    void run_func(Function&& f, TArgs&&...args)
    {
        auto task = pool_type::make_task(std::forward<Function>(f), std::forward<TArgs>(args)...);
        run(task);
    }

    void run(TWork work)
    {
        x_try_start();
        m_works.push_back(std::move(work));
    }

    [[nodiscard]] std::optional<token_type> GetNext() {
        return m_product_queue.pop_front();
    }

    void request_stop() {
        m_works.push_back(std::nullopt);
    }

private:

    using pool_type = CThreadPoolCore;

    void x_try_start()
    {
        if (m_thread_pool.is_cancelled())
            throw std::runtime_error("Thread pool already cancelled");

        if (!m_push_thread.valid())
            m_push_thread = std::async([this](){ x_process_input(); });
    }

    void x_process_input()
    {
        auto deliver = [this] (token_type&& result)
        {
            m_product_queue.push_back(std::move(result));
        };

        while(true) {
            auto msg = m_works.pop_front();
            if (msg) {
                auto work = std::move(msg.value());
                m_thread_pool.schedule(deliver, std::move(work));
            } else {
                break;
            }
        }

        m_thread_pool.finish_threads();

        m_product_queue.push_back({});
    }

    pool_type m_thread_pool;
    CMessageQueue<std::optional<token_type>> m_product_queue; // the queue of threads products
    CMessageQueue<std::optional<TWork>>      m_works;         // the queue of work to do
    std::future<void>                        m_push_thread;
};

}

#endif

