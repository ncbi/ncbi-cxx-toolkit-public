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
#include <corelib/ncbiobj.hpp>

BEGIN_SCOPE(ncbi)

class CObjectOStream;
class CSerialObject;

BEGIN_SCOPE(objects)

class CSeq_entry;

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


    void cancel() {
        m_queue.cancel();
    }

    void clear() {
        m_queue.clear();
    }

    auto pop_front()
    {
        return m_queue.pop_front();
    }

    std::future<void> make_producer_task(TPullNextFunction pull_next_token, TProcessFunction process_func)
    {
        return std::async(std::launch::async, [this, pull_next_token, process_func]()
            {
                try
                {
                    TToken token;
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


class NCBI_XOBJWRITE_EXPORT CGenBankAsyncWriter
{
public:
    enum EDuplicateIdPolicy {
        eIgnore,
        eThrowImmediately,
        eReportAll
    };

    CGenBankAsyncWriter(CObjectOStream* o_stream, EDuplicateIdPolicy policy=eReportAll);
    virtual ~CGenBankAsyncWriter();

    // write any object traditionally
    void Write(CConstRef<CSerialObject> topobject);

    // write genbankset with multiple entries, could be seq_submit, seq_entry, bioseq_set
    // or any other object, get_next_entry will be called only for genbankset's
    using TGetNextFunction = std::function<CConstRef<CSeq_entry>()>;
    void Write(CConstRef<CSerialObject> topobject, TGetNextFunction get_next_entry);

    // the same as above, but write asyncronously
    void StartWriter(CConstRef<CSerialObject> topobject);
    void PushNextEntry(CConstRef<CSeq_entry> entry);
    void FinishWriter();
    void CancelWriter();

protected:
    CObjectOStream*    m_ostream = nullptr;
    EDuplicateIdPolicy m_DuplicateIdPolicy;
    CMessageQueue<CConstRef<CSeq_entry>> m_write_queue;
    std::future<void> m_writer_task;

};

template<class _Token>
class CGenBankAsyncWriterEx: public CGenBankAsyncWriter
{
public:
    using TToken            = _Token;
    using _Pipeline         = TAsyncPipeline<TToken>;
    using TProcessFunction  = typename _Pipeline::TProcessFunction;
    using TPullNextFunction = typename _Pipeline::TPullNextFunction;

    using CGenBankAsyncWriter::CGenBankAsyncWriter;

    void SetDepth(size_t depth) {
        m_pipeline.SetDepth(depth);
    }

    void WriteAsyncMT(CConstRef<CSerialObject> topobject,
            TPullNextFunction pull_next_token,
            TProcessFunction  process_func = {},
            TProcessFunction  chain_func   = {})
    {
        auto pull_next_task = m_pipeline.make_producer_task(pull_next_token, process_func);

        TGetNextFunction get_next_entry = [this, chain_func]() -> CConstRef<CSeq_entry>
        {
            auto token_future = m_pipeline.pop_front();
            if (!token_future.valid()) {
                return {};
            }

            TToken token;
            try {
                token = token_future.get(); // this can throw an exception that was caught within the thread
                if (chain_func) {
                    chain_func(token);
                }
            }
            catch(...) {
                m_pipeline.cancel(); 
                throw;
            }   

            return token;
        };

        Write(topobject, get_next_entry);
    }

    void WriteAsync2T(CConstRef<CSerialObject> topobject,
            TPullNextFunction pull_next_token,
            TProcessFunction  process_func = {},
            TProcessFunction  chain_func   = {})
    {
        StartWriter(topobject);
        try
        {
            TToken token;
            while ((pull_next_token(token)))
            {
                if (process_func)
                    process_func(token);

                PushNextEntry(token);

                if (chain_func)
                    chain_func(token);
            }
            FinishWriter();
        }
        catch(...)
        {
            CancelWriter();
            throw;
        }
    }

    void WriteAsyncST(CConstRef<CSerialObject> topobject,
            TPullNextFunction pull_next_token,
            TProcessFunction  process_func = {},
            TProcessFunction  chain_func   = {})
    {
        auto get_next_entry = [pull_next_token, process_func, chain_func]() -> CConstRef<CSeq_entry>
        {
            TToken token;

            if (!pull_next_token(token))
                return {};

            if (process_func)
                process_func(token);

            if (chain_func)
                chain_func(token);

            return token;
        };

        Write(topobject, get_next_entry);
    }


protected:
    _Pipeline m_pipeline;
};


END_SCOPE(objects)
END_SCOPE(ncbi)

#endif
