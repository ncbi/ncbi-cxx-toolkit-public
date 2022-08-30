#ifndef _ASYNC_WRITERS_HPP_
#define _ASYNC_WRITERS_HPP_

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
*   Asynchronous writers
*
*/

#include <functional>
#include <future>
#include <util/message_queue.hpp>
#include <objmgr/seq_entry_handle.hpp>

BEGIN_NCBI_SCOPE

class CObjectOStream;
class CSerialObject;

BEGIN_objects_SCOPE

class CSeq_entry;
class CSeq_submit;
class CScope;

struct NCBI_XOBJWRITE_EXPORT TAsyncToken
{
    CRef<CSeq_entry>  entry;
    CRef<CSeq_submit> submit;
    CRef<CSeq_entry>  top_entry;
    CRef<CScope>      scope;
    CSeq_entry_Handle seh;
};


template<typename _token>
class TAsyncPipeline
{
public:
    using TToken  = _token;
    using TFuture = std::future<TToken>;
    using TProcessFunction  = std::function<void(TToken&)>;
    using TPullNextFunction = std::function<bool(TToken&)>;
    using TProcessingQueue  = CMessageQueue<TFuture>;
    TAsyncPipeline() {}
    virtual ~TAsyncPipeline() {}

    void SetDepth(size_t n) {
        m_queue.Trottle().m_limit = n;
    }

    void PostData(TToken data, TProcessFunction process_func)
    {
        TFuture fut = std::async(std::launch::async, [process_func](TToken _data) -> TToken
            {
                process_func(_data);
                return _data;
            },
            data);
        m_queue.push_back(std::move(fut));
    }

    void PostException(std::exception_ptr _excp_ptr)
    {
        m_queue.clear();
        std::promise<TToken> exc_prom;
        std::future<TToken> fut = exc_prom.get_future();
        exc_prom.set_exception(_excp_ptr);
        m_queue.push_back(std::move(fut));
    }

    void Complete()
    {
        m_queue.push_back({});
    }

    auto ReceiveData(TProcessFunction consume_func)
    {
        return std::async(std::launch::async, [consume_func, this]()
        {
            while(true)
            {
                auto token_future = x_pop_front();
                if (!token_future.valid())
                    break;
                auto token = token_future.get(); // this can throw an exception that was caught within the thread
                //if (chain_func) chain_func(token);
                if (consume_func) {
                    consume_func(token);
                }
            }
        });
    }

protected:
    auto x_pop_front()
    {
        return m_queue.pop_front();
    }

    std::future<void> x_make_producer_task(TPullNextFunction pull_next_token, TProcessFunction process_func)
    {
        return std::async(std::launch::async, [this, pull_next_token, process_func]()
            {
                try
                {
                    TAsyncToken token;
                    while ((pull_next_token(token)))
                    {
                        PostData(token, process_func);
                    }
                    Complete();
                }
                catch(...)
                {
                    PostException(std::current_exception());
                }
            }
        );
    }

protected:
    TProcessingQueue m_queue {5};
};


class NCBI_XOBJWRITE_EXPORT CGenBankAsyncWriter: public TAsyncPipeline<TAsyncToken>
{
public:
    using _MyBase = TAsyncPipeline<TAsyncToken>;
    using _MyBase::TProcessFunction;
    using _MyBase::TPullNextFunction;
    using _MyBase::TToken;

    enum EDuplicateIdPolicy {
        eIgnore,
        eThrowImmediately,
        eReportAll
    };

    CGenBankAsyncWriter(CObjectOStream* o_stream, EDuplicateIdPolicy policy=eReportAll);
    virtual ~CGenBankAsyncWriter();

    void Write(CConstRef<CSerialObject> topobject);

    void WriteAsyncMT(CConstRef<CSerialObject> topobject,
            TPullNextFunction pull_next_token,
            TProcessFunction process_func,
            TProcessFunction chain_func);

    void WriteAsync2T(CConstRef<CSerialObject> topobject,
            TPullNextFunction pull_next_token,
            TProcessFunction process_func,
            TProcessFunction chain_func);

    void WriteAsyncST(CConstRef<CSerialObject> topobject,
            TPullNextFunction pull_next_token,
            TProcessFunction process_func,
            TProcessFunction chain_func);
protected:

    void x_write(CConstRef<CSerialObject> topobject, TPullNextFunction get_next_token);

private:
    CObjectOStream* m_ostream = nullptr;
    EDuplicateIdPolicy m_DuplicateIdPolicy;
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif
