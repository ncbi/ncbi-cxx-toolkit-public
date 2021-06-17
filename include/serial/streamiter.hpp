#ifndef STREAMITER__HPP
#define STREAMITER__HPP

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
* Authors: Andrei Gourianov, Alexander Astashyn
*
* File Description:
*   Input stream iterators
* Please note:
*   This API requires multi-threading
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <serial/objistr.hpp>
#include <serial/objectio.hpp>

#include <queue>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

#if defined(NCBI_THREADS)

namespace ns_ObjectIStreamFilterIterator {
template<typename TRoot>
TMemberIndex xxx_MemberIndex(const string& mem_name);
}
template<typename...>
class CObjectIStreamAsyncIterator;

/////////////////////////////////////////////////////////////////////////////
///  CObjectIStreamIterator
///
///  Synchronously read multiple same-type data objects from an input stream
///  with optional filtering.
///  @sa CObjectIStreamAsyncIterator
///
///  The algorithm assumes that the input stream on its top level consists
///  exclusively of one or more serial objects of type TRoot.
///
///  There are two flavors of this template:
///  - CObjectIStreamIterator<TRoot> - iterate through the top-level serial
///    objects of type TRoot.
///  - CObjectIStreamIterator<TRoot, TChild> - iterate through serial objects
///    of type TChild which are contained within the top-level serial objects
///    of type TRoot.
///
///  Usage:
///  @code
///
///  CObjectIStream istr ....;
///
///  for (CSeq_entry& obj : CObjectIStreamIterator<CSeq_entry>(istr)) {
///      // ...do something with "obj" here...
///  }
///
///  for (CBioseq& obj : CObjectIStreamIterator<CSeq_entry,CBioseq>(istr)) {
///      // ...do something with "obj" here...
///  }
///
///
///  CObjectIStreamIterator<CSeq_entry> it(istr);
///  CObjectIStreamIterator<CSeq_entry> eos;
///  for_each (it, eos, [](CSeq_entry& obj) { ... });
///
///  CObjectIStreamIterator<CSeq_entry,CBioseq> it(istr);
///  CObjectIStreamIterator<CSeq_entry,CBioseq> eos;
///  for_each (it, eos, [](CBioseq& obj) { ... });
///
///
///  for (CObjectIStreamIterator<CSeq_entry> it(istr);  it.IsValid();  ++it) {
///      CSeq_entry& obj = *it;
///      // ...do something with "obj" here...
///  }
///
///  for (CObjectIStreamIterator<CSeq_entry,CBioseq> it(istr);
///       it.IsValid();  ++it) {
///      CRef<CBioseq> obj(&*it);
///      // ...do something with "obj" here...
///  }
///
///  for (CObjectIStreamIterator<CSeq_entry, string> it(istr);
///       it.IsValid();  ++it) {
///      string& obj = *it;
///  }
///
///  with filtering (only CTaxon1_data objects with optional 'org' member set are valid):
///    CObjectIStreamIterator<CTaxon1_data> i(istr, eNoOwnership, 
///        CObjectIStreamIterator<CTaxon1_data>::CParams().FilterByMember("org",
///            [](const CObjectIStream& istr, CTaxon1_data& obj,
///               TMemberIndex mem_index, CObjectInfo* mem, void* extra)->bool {
///               return mem != nullptr;
///            }));
///
///  @endcode
///
///  @attention
///   Input iterators only guarantee validity for single pass algorithms:
///   once an input iterator has been incremented, all copies of its previous
///   value may be invalidated. It is still possible to keep data objects
///   for future use by placing them into CRef containers, when applicable.

template<typename...>
class CObjectIStreamIterator
{
public:

    /// Object member filtering function
    ///
    /// @param istr
    ///   Serial object stream
    /// @param obj
    ///   Object being checked. It is being populated and is incomplete.
    /// @param mem_index
    ///   Member index
    /// @param mem
    ///   Member information. If mem is nullptr, the member is missing in the stream.
    /// @param extra
    ///   Extra information provided by the caller when constructing iterator.
    ///
    /// @attention
    ///   When using filtering with CObjectIStreamAsyncIterator, please note
    ///   that the function may be called from different threads.
    ///   Synchronization of access to shared data, if required, is the responsibility of the client.
    template<typename TObj>
    using FMemberFilter = function<bool(const CObjectIStream& istr, TObj& obj,
                                        TMemberIndex mem_index, CObjectInfo* mem,
                                        void* extra)>;
    /// Filtering parameters
    template<typename TObj>
    class CParams
    {
    public:
        CParams(void)
            : m_Index(kInvalidMember)
            , m_FnFilter(nullptr)
            , m_Extra(nullptr) {
        }
        /// Filter by member index
        CParams& FilterByMember(TMemberIndex index, FMemberFilter<TObj> fn, void* extra = nullptr) {
            m_Index = index; m_FnFilter = fn; m_Extra = extra; return *this;
        }
        /// Filter by member name
        CParams& FilterByMember(const string& mem_name, FMemberFilter<TObj> fn, void* extra = nullptr) {
            m_Index = ns_ObjectIStreamFilterIterator::xxx_MemberIndex<TObj>(mem_name);
            m_FnFilter = fn; m_Extra = extra; return *this;
        }

    private:
//        void xxx_MemberIndex(const string& mem_name);
        TMemberIndex        m_Index;
        FMemberFilter<TObj> m_FnFilter;
        void*               m_Extra;
        template<typename...> friend class CObjectIStreamIterator;
        template<typename...> friend class CObjectIStreamAsyncIterator;
    };

    /// Construct iterator upon an object serialization stream
    ///
    /// @param istr
    ///   Serial object stream
    /// @param own_istr
    ///   eTakeOwnership means that the input stream will be deleted
    ///   automatically when the iterator gets destroyed
    /// @param params
    ///   Filtering parameters (default is no filtering)
    template<typename TObj>
    CObjectIStreamIterator( CObjectIStream& istr,
                            EOwnership deleteInStream   = eNoOwnership,
                            const CParams<TObj>& params = CParams<TObj>()) = delete;

    /// Construct end-of-stream (invalid) iterator
    /// @sa IsValid()
    CObjectIStreamIterator(void) = delete;

    // Copy-ctor and assignment
    CObjectIStreamIterator(const CObjectIStreamIterator&);
    CObjectIStreamIterator& operator=(const CObjectIStreamIterator&);

    /// Advance to the next data object
    CObjectIStreamIterator& operator++(void);

    // Comparison
    bool operator==(const CObjectIStreamIterator&) const;
    bool operator!=(const CObjectIStreamIterator&) const;

    /// Check whether the iterator points to a data
    /// TRUE if the iterator is constructed upon a serialization stream AND
    /// if it's not end-of-stream or error-in-stream  
    bool IsValid(void) const;

    /// Return the underlying serial object stream
    const CObjectIStream& GetObjectIStream(void) const;

    /// Return data object which is currently pointed to by the iterator.
    /// Throw an exception is the iterator does not point to a data, i.e.
    /// if IsValid() is FALSE.
    template<typename TObj>  TObj& operator*();

    /// Return pointer to data object which is currently pointed to by the
    /// iterator.
    /// Return NULL is the iterator does not point to a data, i.e.
    /// if IsValid() is FALSE.
    template<typename TObj>  TObj* operator->();

    /// Return self
    CObjectIStreamIterator& begin(void);

    /// Construct and return end-of-stream iterator
    CObjectIStreamIterator  end(void);

    // dtor
    ~CObjectIStreamIterator();
};


/////////////////////////////////////////////////////////////////////////////
///   CObjectIStreamAsyncIterator
///
///  Asynchronously read multiple same-type data objects from an input stream
///  with optional filtering
///  @sa CObjectIStreamIterator
///
///  The algorithm assumes that the input stream on its top level consists
///  exclusively of one or more serial objects of type TRoot.
///
///  There are two flavors of this template:
///  - CObjectIStreamAsyncIterator<TRoot> - iterate through the top-level
///    serial objects of type TRoot.
///  - CObjectIStreamAsyncIterator<TRoot, TChild> - iterate through serial
///    objects of type TChild which are contained within the top-level serial
///    objects of type TRoot.
///
///  @attention
///    This iterator supports only the TChild types that are derived from
///    CSerialObject class
///
///  Usage:
///  @code
///
///  CObjectIStream istr ....;
///
///  for (CSeq_entry& obj : CObjectIStreamAsyncIterator<CSeq_entry>(istr))
///  {
///      // ...do something with "obj" here...
///  }
///
///  for (CBioseq& obj : CObjectIStreamAsyncIterator<CSeq_entry,CBioseq>(istr))
///  {
///      // ...do something with "obj" here...
///  }
///
///
///  CObjectIStreamAsyncIterator<CSeq_entry> it(istr);
///  CObjectIStreamAsyncIterator<CSeq_entry> eos;
///  for_each (it, eos, [](CSeq_entry& obj) { ... });
///
///  CObjectIStreamAsyncIterator<CSeq_entry,CBioseq> it(istr);
///  CObjectIStreamAsyncIterator<CSeq_entry,CBioseq> eos;
///  for_each (it, eos, [](CBioseq& obj) { ... });
///
///
///  for (CObjectIStreamAsyncIterator<CSeq_entry> it(istr);
///       it.IsValid();  ++it) {
///      CSeq_entry& obj = *it;
///      // ...do something with "obj" here...
///  }
///
///  for (CObjectIStreamAsyncIterator<CSeq_entry,CBioseq> it(istr);
///       it.IsValid();  ++it) {
///      CRef<CBioseq> obj(&*it);
///      // ...do something with "obj" here...
///  }
///
///  @endcode
///
///  To speed up reading, the iterator offloads data reading, pre-parsing and
///  parsing into separate threads. If the data stream contains numerous TRoot
///  data records CObjectIStreamAsyncIterator can give up to 2-4 times speed-up
///  (wall-clock wise) comparing to the synchronous processing (such as with
///  CObjectIStreamIterator) of the same data.
///
///  The reader has to read the whole object into memory. If such objects are
///  relatively small, then there will be several objects read into a single
///  buffer, which is good. If data object is big it still goes into a single
///  buffer no matter how big the object is.
///  To limit memory consumption, use MaxTotalRawSize parameter.
///
///  The iterator does its job asynchronously. It starts working immediately
///  after its creation and stops only when it is destroyed.
///  Even if you do not use it, it still works in the background, reading and
///  parsing the data.
///
///  @attention
///   Input iterators only guarantee validity for single pass algorithms:
///   once an input iterator has been incremented, all copies of its previous
///   value may be invalidated. It is still possible to keep data objects
///   for future use by placing them into CRef containers, when applicable.

template<typename...>
class CObjectIStreamAsyncIterator
{
public:

    /// Asynchronous parsing parameters
    template<typename TObj>
    class CParams : public CObjectIStreamIterator<>::CParams<TObj>
    {
    public:
        using CParent       = CObjectIStreamIterator<>::CParams<TObj>;
        template<typename TR>
        using FMemberFilter = CObjectIStreamIterator<>::FMemberFilter<TR>;

        CParams(void)
            : m_ThreadPolicy(launch::async)
            , m_MaxParserThreads (16)
            , m_MaxTotalRawSize  (16 * 1024 * 1024)
            , m_MinRawBufferSize (128 * 1024)
            , m_SameThread(false) {
        }

        /// Filter by member index
        CParams& FilterByMember(TMemberIndex index, FMemberFilter<TObj> fn, void* extra = nullptr) {
            CParent::FilterByMember(index, fn, extra); return *this;
        }

        /// Filter by member name
        CParams& FilterByMember(const string& mem_name, FMemberFilter<TObj> fn, void* extra = nullptr) {
            CParent::FilterByMember(mem_name, fn, extra); return *this;
        }

        /// Parsing thread launch policy
        CParams& LaunchPolicy(launch policy) {
            m_ThreadPolicy = policy; return *this;
        }

        /// Maximum number of parsing threads
        CParams& MaxParserThreads(unsigned max_parser_threads) {
            m_MaxParserThreads = max_parser_threads;  return *this;
        }

        /// Total size of raw data buffers is allowed to grow to this value
        CParams& MaxTotalRawSize(size_t max_total_raw_size) {
            m_MaxTotalRawSize = max_total_raw_size;  return *this;
        }
        
        /// Single raw data memory buffer size should be at least this big
        CParams& MinRawBufferSize(size_t min_raw_buffer_size) {
            m_MinRawBufferSize = min_raw_buffer_size;  return *this;
        }

        /// Raw data read and its pre-parsing (storing the raw data pertaining
        /// to a single object and putting it into the parsing queue) to be
        /// done in the same thread.
        /// @note
        ///  The default is to do these two tasks in two separate threads,
        ///  which in some cases can give an additional 10-20% performance
        ///  boost, wall-clock time wise.
        CParams& ReadAndSkipInTheSameThread(bool same_thread) {
            m_SameThread = same_thread;  return *this;
        }

    private:
        launch    m_ThreadPolicy;
        unsigned  m_MaxParserThreads;
        size_t    m_MaxTotalRawSize;
        size_t    m_MinRawBufferSize;
        bool      m_SameThread;

        template<typename...> friend class CObjectIStreamAsyncIterator;
    };


    /// Construct iterator upon an object serialization stream
    ///
    /// @param istr
    ///   Serial object stream
    /// @param own_istr
    ///   eTakeOwnership means that the input stream will be deleted
    ///   automatically when the iterator gets destroyed
    /// @param params
    ///   Parsing algorithm's parameters
    /// @param params
    ///   Filtering and parsing parameters (default is no filtering)
    template<typename TObj>
    CObjectIStreamAsyncIterator(CObjectIStream& istr,
                                EOwnership own_istr         = eNoOwnership,
                                const CParams<TObj>& params = CParams<TObj>()) = delete;

    /// Construct end-of-stream (invalid) iterator
    /// @sa IsValid()
    CObjectIStreamAsyncIterator(void) = delete;

    // Copy-ctor and assignment
    CObjectIStreamAsyncIterator(const CObjectIStreamAsyncIterator&);
    CObjectIStreamAsyncIterator& operator=(const CObjectIStreamAsyncIterator&);

    /// Advance to the next data object
    CObjectIStreamAsyncIterator& operator++(void);

    // Comparison
    bool operator==(const CObjectIStreamAsyncIterator&) const;
    bool operator!=(const CObjectIStreamAsyncIterator&) const;

    /// Check whether the iterator points to a data
    /// TRUE if the iterator is constructed upon a serialization stream AND
    /// if it's not end-of-stream or error-in-stream  
    bool IsValid(void) const;

    /// Return data object which is currently pointed to by the iterator.
    /// Throw an exception is the iterator does not point to a data, i.e.
    /// if IsValid() is FALSE.
    template<typename TObj>  TObj& operator*();

    /// Return pointer to data object which is currently pointed to by the
    /// iterator.
    /// Return NULL is the iterator does not point to a data, i.e.
    /// if IsValid() is FALSE.
    template<typename TObj>  TObj* operator->();

    /// Return self
    CObjectIStreamAsyncIterator& begin(void);

    /// Construct and return end-of-stream iterator
    CObjectIStreamAsyncIterator  end(void);

    // dtor
    ~CObjectIStreamAsyncIterator();
};



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/// template specializations and implementation

/////////////////////////////////////////////////////////////////////////////
///  CObjectIStreamIterator<TRoot>

template<typename TRoot>
class CObjectIStreamIterator<TRoot>
    : public iterator< input_iterator_tag, TRoot, ptrdiff_t, TRoot*, TRoot& >
{
public:
    using CParams = CObjectIStreamIterator<>::CParams<TRoot>;

    CObjectIStreamIterator( CObjectIStream& istr,
                            EOwnership deleteInStream = eNoOwnership,
                            const CParams& params     = CParams());

    CObjectIStreamIterator(void);
    CObjectIStreamIterator(const CObjectIStreamIterator&);
    CObjectIStreamIterator& operator=(const CObjectIStreamIterator&);
    ~CObjectIStreamIterator();

    CObjectIStreamIterator& operator++(void);
    bool operator==(const CObjectIStreamIterator&) const;
    bool operator!=(const CObjectIStreamIterator&) const;
    bool IsValid(void) const;
    const CObjectIStream& GetObjectIStream(void) const;

    TRoot& operator*();
    TRoot* operator->();

    CObjectIStreamIterator& begin(void);
    CObjectIStreamIterator  end(void);

protected:
    struct CData
    {
        CData(CObjectIStream& istr, EOwnership deleteInStream,
              const CParams& params, TTypeInfo tinfo);
        ~CData(void);

        void x_BeginRead(void);
        void x_EndRead(void);
        void x_AcceptData(CObjectIStream& in, const CObjectInfo& type);
        void x_Next(void);
        bool x_NextNoFilter(const CObjectInfo& objinfo);
        bool x_NextSeqWithFilter(const CObjectInfo& objinfo);
        bool x_NextChoiceWithFilter(const CObjectInfo& objinfo);
        bool x_NextContainerWithFilter(const CObjectInfo& objinfo);

        CObjectIStream*    m_Istr;
        EOwnership         m_Own;
        CObjectTypeInfo    m_ValueType;
        CObjectInfo        m_Value;
        bool               m_HasReader;
        bool               m_EndOfData;
        CParams            m_Params;
        mutex              m_ReaderMutex;
        condition_variable m_ReaderCv;
        thread             m_Reader;
        exception_ptr      m_ReaderExpt;
        enum EFilter {
            eNone,
            eOneSeq,
            eOneRandom,
            eAllSeq,
            eAllRandom,
            eOneChoice,
            eAllChoice,
            eOneContainer,
            eAllContainer
        } m_FilterType;

        template<typename TR>
        class x_CObjectIStreamIteratorHook : public CSkipObjectHook
        {
        public:
            x_CObjectIStreamIteratorHook(
                typename CObjectIStreamIterator<TR>::CData* pthis)
                    : m_This(pthis) {
            }
            virtual void SkipObject(CObjectIStream& in, const CObjectTypeInfo& type) override {
                m_This->x_AcceptData(in,CObjectInfo(type.GetTypeInfo()));
            }
        private:
            typename CObjectIStreamIterator<TR>::CData* m_This;
        };
    };

    CObjectIStreamIterator( CObjectIStream& istr,
        const CParams& params, EOwnership deleteInStream);
    void x_ReaderThread(void);
    shared_ptr<CData> m_Data;
};

/////////////////////////////////////////////////////////////////////////////
///  CObjectIStreamIterator<TRoot, TChild>

template<typename TRoot, typename TChild>
class CObjectIStreamIterator<TRoot, TChild>
    : public CObjectIStreamIterator<TChild>
{
public:
    using CParams = CObjectIStreamIterator<>::CParams<TChild>;

    CObjectIStreamIterator( CObjectIStream& istr,
                            EOwnership deleteInStream  = eNoOwnership,
                            const CParams& params      = CParams());

    CObjectIStreamIterator(void);
    CObjectIStreamIterator(const CObjectIStreamIterator&);
    CObjectIStreamIterator& operator=(const CObjectIStreamIterator&);
    ~CObjectIStreamIterator();

    CObjectIStreamIterator& operator++(void);
    CObjectIStreamIterator& begin(void);
    CObjectIStreamIterator  end(void);

protected:
    using CParent       = CObjectIStreamIterator<TChild>;
    void x_ReaderThread(void);

    template<typename TR>
    class x_CObjectIStreamIteratorReadHook : public CReadObjectHook
    {
    public:
        x_CObjectIStreamIteratorReadHook(
            typename CObjectIStreamIterator<TR>::CData* pthis)
                : m_This(pthis) {
        }
        virtual void ReadObject(CObjectIStream& in, const CObjectInfo& type) override {
            m_This->x_AcceptData(in,type);
        }
    private:
        typename CObjectIStreamIterator<TR>::CData* m_This;
    };
};


/////////////////////////////////////////////////////////////////////////////
// helpers

namespace ns_ObjectIStreamFilterIterator {

template<typename TRoot>
typename enable_if< is_base_of< CSerialObject, TRoot>::value, TTypeInfo>::type
xxx_GetTypeInfo(void)
{
    return TRoot::GetTypeInfo();
}

template<typename TRoot>
//typename enable_if< !is_base_of< CSerialObject, TRoot>::value, TTypeInfo>::type
typename enable_if< is_pod<TRoot>::value || is_convertible<TRoot, std::string>::value, TTypeInfo>::type
xxx_GetTypeInfo(void)
{
    return CStdTypeInfo<TRoot>::GetTypeInfo();
}

template<typename TRoot>
TMemberIndex xxx_MemberIndex(const string& mem_name) 
{
    TTypeInfo tinfo = xxx_GetTypeInfo<TRoot>();
    ETypeFamily type = tinfo->GetTypeFamily();
    if (type == eTypeFamilyClass || type == eTypeFamilyChoice) {
        const CClassTypeInfoBase* cinfo = CTypeConverter<CClassTypeInfoBase>::SafeCast(tinfo);
        return cinfo->GetItems().Find(mem_name);
    }
    return kInvalidMember;
}

} // ns_ObjectIStreamFilterIterator

#if 0
template<typename...>
template<typename TObj>
void
CObjectIStreamIterator<>::CParams<TObj>::xxx_MemberIndex(const string& mem_name) {
    TTypeInfo tinfo = ns_ObjectIStreamFilterIterator::xxx_GetTypeInfo<TObj>();
    ETypeFamily type = tinfo->GetTypeFamily();
    if (type == eTypeFamilyClass || type == eTypeFamilyChoice) {
        const CClassTypeInfoBase* cinfo = CTypeConverter<CClassTypeInfoBase>::SafeCast(tinfo);
        return cinfo->GetItems().Find(mem_name);
    }
    return kInvalidMember;
}
#endif


/////////////////////////////////////////////////////////////////////////////
///  CObjectIStreamIterator<TRoot> implementation

template<typename TRoot>
CObjectIStreamIterator<TRoot>::CObjectIStreamIterator(void)
    : m_Data(nullptr) {
}

template<typename TRoot>
CObjectIStreamIterator<TRoot>::CObjectIStreamIterator( 
    CObjectIStream& istr, const CParams& params, EOwnership deleteInStream)
    : m_Data( new CData(istr, deleteInStream, params,
        ns_ObjectIStreamFilterIterator::xxx_GetTypeInfo<TRoot>())) {
}

template<typename TRoot>
CObjectIStreamIterator<TRoot>::CObjectIStreamIterator( 
    CObjectIStream& istr, EOwnership deleteInStream, const CParams& params)
    : CObjectIStreamIterator(istr, params, deleteInStream)
{
    if (m_Data->m_FilterType != CData::eNone && !m_Data->m_EndOfData) {
        m_Data->m_HasReader = true;
        m_Data->m_Reader = thread( mem_fun<void, CObjectIStreamIterator<TRoot> >(
            &CObjectIStreamIterator<TRoot>::x_ReaderThread), this);
    }
    ++(*this);
}

template<typename TRoot>
CObjectIStreamIterator<TRoot>::CObjectIStreamIterator(
    const CObjectIStreamIterator& v) : m_Data(v.m_Data) {
}

template<typename TRoot>
CObjectIStreamIterator<TRoot>&
CObjectIStreamIterator<TRoot>::operator=(const CObjectIStreamIterator& v) {
    m_Data = v.m_Data;
    return *this;
}

template<typename TRoot>
CObjectIStreamIterator<TRoot>::~CObjectIStreamIterator() {
}

template<typename TRoot>
CObjectIStreamIterator<TRoot>::CData::CData(
    CObjectIStream& istr, EOwnership deleteInStream,
    const CParams& params, TTypeInfo tinfo)
    : m_Istr(&istr), m_Own(deleteInStream)
    , m_ValueType(tinfo), m_Value(tinfo), m_HasReader(false)
    , m_EndOfData(m_Istr->EndOfData()), m_Params(params)
{
    ETypeFamily type = tinfo->GetTypeFamily();
    if (type != eTypeFamilyClass && type != eTypeFamilyChoice && type != eTypeFamilyContainer) {
        m_Params.m_FnFilter = nullptr;
    }
    m_FilterType = eNone;
    if (m_Params.m_FnFilter) {
        if (type == eTypeFamilyClass) {
            const CClassTypeInfo* cinfo = CTypeConverter<CClassTypeInfo>::SafeCast(tinfo);
            if (cinfo->Implicit()) {
                const CItemInfo* itemInfo =
                    cinfo->GetItems().GetItemInfo(cinfo->GetItems().FirstIndex());
                if (itemInfo->GetTypeInfo()->GetTypeFamily() == eTypeFamilyContainer) {
                    m_FilterType = m_Params.m_Index != kInvalidMember ? eOneContainer : eAllContainer;
                }
            }
            if (m_FilterType == eNone) {
                bool is_random = cinfo->RandomOrder();
                if (m_Params.m_Index != kInvalidMember) {
                    m_FilterType = is_random ? eOneRandom : eOneSeq;
                } else {
                    m_FilterType = is_random ? eAllRandom : eAllSeq;
                }
            }
        } else if (type == eTypeFamilyChoice) {
            m_FilterType = m_Params.m_Index != kInvalidMember ? eOneChoice : eAllChoice;
        } else if (type == eTypeFamilyContainer) {
            m_FilterType = m_Params.m_Index != kInvalidMember ? eOneContainer : eAllContainer;
        }
    }
}

template<typename TRoot>
CObjectIStreamIterator<TRoot>::CData::~CData(void) {
    if (m_Reader.joinable()) {
        m_EndOfData = true;
        m_ReaderCv.notify_all();
        m_Reader.join();
    }
    if (m_Istr && m_Own == eTakeOwnership) {
        delete m_Istr;
    }
}

template<typename TRoot>
void
CObjectIStreamIterator<TRoot>::CData::x_BeginRead(void) {
    unique_lock<mutex> lck( m_ReaderMutex);
    while (m_Value.GetObjectPtr() != nullptr) {
        m_ReaderCv.wait(lck);
    }
}

template<typename TRoot>
void
CObjectIStreamIterator<TRoot>::CData::x_EndRead(void) {
    m_Value = CObjectInfo();
    m_EndOfData = true;
    m_ReaderCv.notify_one();
}

template<typename TRoot>
void
CObjectIStreamIterator<TRoot>::x_ReaderThread() {
    m_Data->x_BeginRead();
    try {
        m_Data->m_ValueType.SetLocalSkipHook(*(m_Data->m_Istr), new typename CData::template x_CObjectIStreamIteratorHook<TRoot>(m_Data.get()));
        while (Serial_FilterSkip(*(m_Data->m_Istr),m_Data->m_ValueType))
            ;
    } catch (...) {
        if (!m_Data->m_EndOfData) {
            m_Data->m_ReaderExpt = current_exception();
        }
    }
    m_Data->x_EndRead();
}

template<typename TRoot, typename TChild>
void
CObjectIStreamIterator<TRoot,TChild>::x_ReaderThread() {
    this->m_Data->x_BeginRead();
    try {
        this->m_Data->m_ValueType.SetLocalSkipHook(*(this->m_Data->m_Istr), new typename CParent::CData::template x_CObjectIStreamIteratorHook<TChild>(this->m_Data.get()));
        this->m_Data->m_ValueType.SetLocalReadHook(*(this->m_Data->m_Istr), new x_CObjectIStreamIteratorReadHook<TChild>(this->m_Data.get()));
        while (Serial_FilterSkip(*(this->m_Data->m_Istr),CObjectTypeInfo(CType<TRoot>())))
            ;
    } catch (...) {
        if (!this->m_Data->m_EndOfData) {
            this->m_Data->m_ReaderExpt = current_exception();
        }
    }
    this->m_Data->x_EndRead();
}

template<typename TRoot>
void
CObjectIStreamIterator<TRoot>::CData::x_AcceptData(
    CObjectIStream& in, const CObjectInfo& objinfo)
{
    if (m_Istr->EndOfData()) {
        m_EndOfData = true;
    } else {
        bool res = false;
        switch ( m_FilterType) {
        default:
        case eNone:
            res = x_NextNoFilter(objinfo);
            break;
        case eOneSeq:
        case eOneRandom:
        case eAllSeq:
        case eAllRandom:
            res = x_NextSeqWithFilter(objinfo);
            break;
        case eOneChoice:
        case eAllChoice:
            res = x_NextChoiceWithFilter(objinfo);
            break;
        case eOneContainer:
        case eAllContainer:
            res = x_NextContainerWithFilter(objinfo);
            break;
        }
        if (res) {
            unique_lock<mutex> lck(m_ReaderMutex);
            m_Value = objinfo;
            m_ReaderCv.notify_one();
            while (m_Value.GetObjectPtr() != nullptr) {
                if (m_EndOfData) {
                    NCBI_THROW( CEofException, eEof,
                        "CObjectIStreamIterator: abort data parsing");
                }
                m_ReaderCv.wait(lck);
            }
        } else {
            in.SetDiscardCurrObject();
        }
    }
}

template<typename TRoot>
void
CObjectIStreamIterator<TRoot>::CData::x_Next(void) {
    unique_lock<mutex> lck(m_ReaderMutex);
    m_Value = CObjectInfo();
    m_ReaderCv.notify_one();
    while (m_Value.GetObjectPtr() == nullptr && !m_EndOfData) {
        m_ReaderCv.wait(lck);
    }
    if (m_ReaderExpt) {
        rethrow_exception(m_ReaderExpt);
    }
}

template<typename TRoot>
bool
CObjectIStreamIterator<TRoot>::CData::x_NextNoFilter(const CObjectInfo& objinfo)
{
    objinfo.GetTypeInfo()->DefaultReadData(*m_Istr, objinfo.GetObjectPtr());
    return true;
}

template<typename TRoot>
bool
CObjectIStreamIterator<TRoot>::CData::x_NextSeqWithFilter(const CObjectInfo& objinfo)
{
    TMemberIndex mi = kInvalidMember;
    set<TMemberIndex> done;
    bool checked = false;
    bool valid = true;
    TRoot& obj = *CTypeConverter<TRoot>::SafeCast(objinfo.GetObjectPtr());

    for ( CIStreamClassMemberIterator i(*m_Istr, objinfo); i; ++i ) {

        TMemberIndex mi_now = (*i).GetMemberIndex();
        CObjectInfoMI minfo(objinfo, mi_now);

        if (valid) {
// before read - validate missing members
            switch (m_FilterType) {
            case eOneRandom:
            case eAllRandom:
            default:
                break;
            case eOneSeq:
                if (mi_now > m_Params.m_Index && !checked) {
                    valid = m_Params.m_FnFilter( *m_Istr, obj, m_Params.m_Index, nullptr, m_Params.m_Extra);
                    checked = true;
                }
                break;
            case eAllSeq:
                for (++mi; valid && mi < mi_now; ++mi) {
                    valid = m_Params.m_FnFilter( *m_Istr, obj, mi, nullptr, m_Params.m_Extra);
                }
                break;
            }
        }

// if still valid
        if (valid) {
// read next member
            i.ReadClassMember(objinfo);

// after read - validate member
            switch (m_FilterType) {
            default: break;
            case eOneSeq:
            case eOneRandom:
                if (mi_now == m_Params.m_Index) {
                    CObjectInfo oi = minfo.GetMember().GetTypeFamily() == eTypeFamilyPointer ?
                        minfo.GetMember().GetPointedObject() : minfo.GetMember();
                    valid = m_Params.m_FnFilter( *m_Istr, obj, mi_now, &oi, m_Params.m_Extra);
                    checked = true;
                }
                break;
            case eAllRandom:
                done.insert(mi_now);
                // no break
                NCBI_FALLTHROUGH;
            case eAllSeq:
                {
                    CObjectInfo oi = minfo.GetMember().GetTypeFamily() == eTypeFamilyPointer ?
                        minfo.GetMember().GetPointedObject() : minfo.GetMember();
                    valid = m_Params.m_FnFilter( *m_Istr, obj, mi_now, &oi, m_Params.m_Extra);
                }
                break;
            }
        } else {
// object invalid - skip remaining members
            i.SkipClassMember();
        }
        mi = mi_now;
    }

// finally - validate missing members
    if (valid) {
        switch (m_FilterType) {
        default: break;
        case eOneSeq:
        case eOneRandom:
            if (!checked) {
                valid = m_Params.m_FnFilter( *m_Istr, obj, m_Params.m_Index, nullptr, m_Params.m_Extra);
            }
            break;
        case eAllSeq:
            {
                TMemberIndex mi_last = objinfo.GetClassTypeInfo()->GetItems().LastIndex() + 1;
                for (++mi; valid && mi < mi_last; ++mi) {
                    valid = m_Params.m_FnFilter( *m_Istr, obj, mi, nullptr, m_Params.m_Extra);
                }
            }
            break;
        case eAllRandom:
            {
                mi = objinfo.GetClassTypeInfo()->GetItems().FirstIndex();
                TMemberIndex mi_last = objinfo.GetClassTypeInfo()->GetItems().LastIndex() + 1;
                for (; valid && mi < mi_last; ++mi) {
                    if (done.find(mi) == done.end()) {
                        valid = m_Params.m_FnFilter( *m_Istr, obj, mi, nullptr, m_Params.m_Extra);
                    }
                }
            }
            break;
        }
    }
    return valid;
}

template<typename TRoot>
bool
CObjectIStreamIterator<TRoot>::CData::x_NextChoiceWithFilter(const CObjectInfo& objinfo)
{
    bool valid = true;
    objinfo.GetTypeInfo()->DefaultReadData(*m_Istr, objinfo.GetObjectPtr());
    TRoot& obj = *CTypeConverter<TRoot>::SafeCast(objinfo.GetObjectPtr());
    CObjectInfoCV cv = objinfo.GetCurrentChoiceVariant();
    TMemberIndex i = cv.GetVariantIndex();
    if (i == m_Params.m_Index) {
        CObjectInfo oi = cv.GetVariant().GetTypeFamily() == eTypeFamilyPointer ?
            cv.GetVariant().GetPointedObject() : cv.GetVariant();
        valid = m_Params.m_FnFilter( *m_Istr, obj, i, &oi, m_Params.m_Extra);
    } else {
        valid = m_Params.m_FnFilter( *m_Istr, obj, m_Params.m_Index, nullptr, m_Params.m_Extra);
    }
    return valid;
}

template<typename TRoot>
bool
CObjectIStreamIterator<TRoot>::CData::x_NextContainerWithFilter(const CObjectInfo& objinfo)
{
    TMemberIndex mi = kInvalidMember;
    bool valid = true;
    TRoot& obj = *CTypeConverter<TRoot>::SafeCast(objinfo.GetObjectPtr());

    for ( CIStreamContainerIterator i(*m_Istr, objinfo); i; ++i ) {
        if (valid) {
            CObjectInfo oi(i.ReadElement(objinfo.GetObjectPtr()));
            ++mi;
            if (oi.GetObjectPtr()) {
                if (m_FilterType == eAllContainer || (m_FilterType == eOneContainer && mi == m_Params.m_Index)) {
                    CObjectInfo oe = oi.GetTypeFamily() == eTypeFamilyPointer ? oi.GetPointedObject() : oi;
                    valid = m_Params.m_FnFilter( *m_Istr, obj, mi, &oe, m_Params.m_Extra);
                }
            }
        } else {
            i.SkipElement();
        }
    }
    return valid;
}

template<typename TRoot>
CObjectIStreamIterator<TRoot>&
CObjectIStreamIterator<TRoot>::operator++(void) {
    if (m_Data.get() != nullptr) {
        if (!m_Data->m_HasReader) {
            if (m_Data->m_Istr->EndOfData()) {
                m_Data.reset();
            } else {
                m_Data->m_Value = CObjectInfo(m_Data->m_ValueType);
                m_Data->m_Istr->Read(m_Data->m_Value);
            }
        } else {
            m_Data->x_Next();
            if (m_Data->m_EndOfData) {
                m_Data.reset();
            }
        }
    }
    return *this;
}

template<typename TRoot>
bool
CObjectIStreamIterator<TRoot>::operator==(
    const CObjectIStreamIterator& v) const {
    return m_Data.get() == v.m_Data.get();
}

template<typename TRoot>
bool
CObjectIStreamIterator<TRoot>::operator!=(
    const CObjectIStreamIterator& v) const {
    return m_Data.get() != v.m_Data.get();
}

template<typename TRoot>
bool CObjectIStreamIterator<TRoot>::IsValid() const {
    return m_Data.get() != nullptr && m_Data->m_Value.GetObjectPtr() != nullptr;
}

template<typename TRoot>
const CObjectIStream&
CObjectIStreamIterator<TRoot>::GetObjectIStream(void) const {
    return *(m_Data->m_Istr);
}

template<typename TRoot>
TRoot&
CObjectIStreamIterator<TRoot>::operator*() {
    return *(TRoot*)(m_Data->m_Value.GetObjectPtr());
}

template<typename TRoot>
TRoot*
CObjectIStreamIterator<TRoot>::operator->() {
    return IsValid() ? (TRoot*)m_Data->m_Value.GetObjectPtr() : nullptr;
}

template<typename TRoot>
CObjectIStreamIterator<TRoot>&
CObjectIStreamIterator<TRoot>::begin(void) {
    return *this;
}

template<typename TRoot>
CObjectIStreamIterator<TRoot>
CObjectIStreamIterator<TRoot>::end(void) {
    return CObjectIStreamIterator<TRoot>();
}


/////////////////////////////////////////////////////////////////////////////
///  CObjectIStreamIterator<TRoot, TChild> implementation

template<typename TRoot, typename TChild>
CObjectIStreamIterator<TRoot, TChild>::CObjectIStreamIterator(void)
    : CParent() {
}

template<typename TRoot, typename TChild>
CObjectIStreamIterator<TRoot, TChild>::CObjectIStreamIterator( 
    CObjectIStream& istr, EOwnership deleteInStream, const CParams& params)
    : CParent(istr, params, deleteInStream)
{
    if (!this->m_Data->m_EndOfData) {
        this->m_Data->m_HasReader = true;
        this->m_Data->m_Reader = thread( mem_fun<void, CObjectIStreamIterator<TRoot,TChild> >(
            &CObjectIStreamIterator<TRoot,TChild>::x_ReaderThread), this);
    }
    ++(*this);
}

template<typename TRoot, typename TChild>
CObjectIStreamIterator<TRoot, TChild>::CObjectIStreamIterator(
    const CObjectIStreamIterator& v) : CParent(v) {
}

template<typename TRoot, typename TChild>
CObjectIStreamIterator<TRoot, TChild>&
CObjectIStreamIterator<TRoot, TChild>::operator=(
    const CObjectIStreamIterator& v) {
    CParent::operator=(v);
    return *this;
}

template<typename TRoot, typename TChild>
CObjectIStreamIterator<TRoot, TChild>::~CObjectIStreamIterator() {
}

template<typename TRoot, typename TChild>
CObjectIStreamIterator<TRoot, TChild>&
CObjectIStreamIterator<TRoot, TChild>::operator++(void) {
    CParent::operator++();
    return *this;
}

template<typename TRoot, typename TChild>
CObjectIStreamIterator<TRoot, TChild>&
CObjectIStreamIterator<TRoot, TChild>::begin(void) {
    return *this;
}

template<typename TRoot, typename TChild>
CObjectIStreamIterator<TRoot, TChild>
CObjectIStreamIterator<TRoot, TChild>::end(void) {
    return CObjectIStreamIterator<TRoot, TChild>();
}


/////////////////////////////////////////////////////////////////////////////
///   CObjectIStreamAsyncIterator<TRoot>

template<typename TRoot>
class CObjectIStreamAsyncIterator<TRoot>
    : public iterator< input_iterator_tag, TRoot, ptrdiff_t, TRoot*, TRoot& >
{
public:
    using CParams = CObjectIStreamAsyncIterator<>::CParams<TRoot>;

    CObjectIStreamAsyncIterator( CObjectIStream& istr,
                                 EOwnership deleteInStream = eNoOwnership,
                                 const CParams& params     =  CParams());
    CObjectIStreamAsyncIterator(void);
    CObjectIStreamAsyncIterator(const CObjectIStreamAsyncIterator&);
    CObjectIStreamAsyncIterator& operator=(const CObjectIStreamAsyncIterator&);
    ~CObjectIStreamAsyncIterator();

    CObjectIStreamAsyncIterator& operator++(void);
    bool operator==(const CObjectIStreamAsyncIterator&) const;
    bool operator!=(const CObjectIStreamAsyncIterator&) const;
    bool IsValid(void) const;

    TRoot& operator*();
    TRoot* operator->();

    CObjectIStreamAsyncIterator& begin(void);
    CObjectIStreamAsyncIterator  end(void);


protected:
    typedef queue< CRef<TRoot> > TObjectsQueue;
#if NCBI_COMPILER_MSVC && _MSC_VER < 1900
    typedef function<TObjectsQueue(CRef<CByteSource>, ESerialDataFormat, const CParams&, TObjectsQueue)> FParserFunction;
#else
    typedef function<TObjectsQueue(CRef<CByteSource>, ESerialDataFormat, const CParams&, TObjectsQueue&&)> FParserFunction;
#endif
    CObjectIStreamAsyncIterator( CObjectIStream& istr,
                                 EOwnership deleteInStream,
                                 FParserFunction parser,
                                 const CParams& params);
private: 
    static TObjectsQueue sx_ClearGarbageAndParse(
            CRef<CByteSource> bytesource,  ESerialDataFormat format,
            const CParams& params,
#if NCBI_COMPILER_MSVC && _MSC_VER < 1900
            TObjectsQueue garbage
#else
            TObjectsQueue&& garbage
#endif
            );
    
    struct CData {
        CData(CObjectIStream& istr, EOwnership deleteInStream, FParserFunction parser,
            const CParams& params);
        ~CData(void);

        using future_queue_t  = future<TObjectsQueue>;
        using futures_queue_t = queue<future_queue_t>;

        void x_UpdateObjectsQueue();
        void x_UpdateFuturesQueue();
        CRef< CByteSource > x_GetNextData(void);
        void x_ReaderThread(void);

        TObjectsQueue m_ObjectsQueue; // current queue of objects
        TObjectsQueue m_GarbageQueue; // popped so-far from objects-queue
        futures_queue_t m_FuturesQueue; // queue-of-futures-of-object-queues

        CObjectIStream* m_Istr;
        EOwnership      m_Own;
        FParserFunction m_Parser;
        size_t          m_ParserCount;
        size_t          m_RawBufferSize;
        size_t          m_MaxRawSize;
        size_t          m_CurrentRawSize;
        launch          m_Policy;
        bool            m_EndOfData;
        CParams         m_Params;

        mutex                        m_ReaderMutex;
        condition_variable           m_ReaderCv;
        thread                       m_Reader;
        queue< CRef< CByteSource > > m_ReaderData;
        queue< size_t >              m_ReaderDataSize;
    };
    shared_ptr<CData> m_Data;
};


/////////////////////////////////////////////////////////////////////////////
///  CObjectIStreamAsyncIterator<TRoot, TChild>

template<typename TRoot, typename TChild>
class CObjectIStreamAsyncIterator<TRoot, TChild>
    : public CObjectIStreamAsyncIterator<TChild>
{
public:
    using CParams = CObjectIStreamAsyncIterator<>::CParams<TChild>;

    CObjectIStreamAsyncIterator( CObjectIStream& istr,
                                 EOwnership deleteInStream = eNoOwnership,
                                 const CParams& params     = CParams());
    CObjectIStreamAsyncIterator(void);
    CObjectIStreamAsyncIterator(const CObjectIStreamAsyncIterator&);
    CObjectIStreamAsyncIterator& operator=(const CObjectIStreamAsyncIterator&);
    ~CObjectIStreamAsyncIterator();

    CObjectIStreamAsyncIterator& operator++(void);
    CObjectIStreamAsyncIterator& begin(void);
    CObjectIStreamAsyncIterator  end(void);


private: 
    using CParent       = CObjectIStreamAsyncIterator<TChild>;
    using TObjectsQueue = typename CParent::TObjectsQueue;

    static TObjectsQueue sx_ClearGarbageAndParse(
            CRef<CByteSource> bytesource,  ESerialDataFormat format,
            const CParams& params,
#if NCBI_COMPILER_MSVC && _MSC_VER < 1900
            TObjectsQueue garbage
#else
            TObjectsQueue&& garbage
#endif
            );
};

/////////////////////////////////////////////////////////////////////////////
///  CObjectIStreamAsyncIterator<TRoot> implementation

template<typename TRoot>
CObjectIStreamAsyncIterator<TRoot>::CObjectIStreamAsyncIterator(void)
    : m_Data(nullptr)
{
}

template<typename TRoot>
CObjectIStreamAsyncIterator<TRoot>::CObjectIStreamAsyncIterator(
        CObjectIStream& istr, EOwnership deleteInStream,
        const CParams& params)
    : CObjectIStreamAsyncIterator( istr, deleteInStream,
        &CObjectIStreamAsyncIterator<TRoot>::sx_ClearGarbageAndParse, params)
{
}

template<typename TRoot>
CObjectIStreamAsyncIterator<TRoot>::CObjectIStreamAsyncIterator(
    CObjectIStream& istr,  EOwnership deleteInStream,
    FParserFunction parser,  const CParams& params)
    : m_Data(new CData(istr, deleteInStream, parser, params))
{
    ++(*this);
}

template<typename TRoot>
CObjectIStreamAsyncIterator<TRoot>::CObjectIStreamAsyncIterator(
    const CObjectIStreamAsyncIterator& v)
        : m_Data(v.m_Data)
{
}

template<typename TRoot>
CObjectIStreamAsyncIterator<TRoot>&
CObjectIStreamAsyncIterator<TRoot>::operator=(
    const CObjectIStreamAsyncIterator& v) {
    m_Data = v.m_Data;
    return *this;
}

template<typename TRoot>
CObjectIStreamAsyncIterator<TRoot>::~CObjectIStreamAsyncIterator() {
}

template<typename TRoot>
CObjectIStreamAsyncIterator<TRoot>&
CObjectIStreamAsyncIterator<TRoot>::operator++() {
    if (m_Data.get() != nullptr) {
        do {
            m_Data->x_UpdateFuturesQueue();
            m_Data->x_UpdateObjectsQueue();
        } while (!IsValid() && !m_Data->m_EndOfData);
        if (!IsValid()) {
            m_Data.reset();
        }
    }
    return *this;
}

template<typename TRoot>
bool
CObjectIStreamAsyncIterator<TRoot>::operator==(
    const CObjectIStreamAsyncIterator& v) const {
    return m_Data.get() == v.m_Data.get();
}

template<typename TRoot>
bool
CObjectIStreamAsyncIterator<TRoot>::operator!=(
    const CObjectIStreamAsyncIterator& v) const {
    return m_Data.get() != v.m_Data.get();
}

template<typename TRoot>
bool CObjectIStreamAsyncIterator<TRoot>::IsValid() const {
    return m_Data.get() != nullptr && !m_Data->m_ObjectsQueue.empty();
}

template<typename TRoot>
TRoot&
CObjectIStreamAsyncIterator<TRoot>::operator*() {
    return m_Data->m_ObjectsQueue.front().GetObject();
}

template<typename TRoot>
TRoot*
CObjectIStreamAsyncIterator<TRoot>::operator->() {
    return IsValid() ? m_Data->m_ObjectsQueue.front().GetPointer() : nullptr;
}

template<typename TRoot>
CObjectIStreamAsyncIterator<TRoot>&
CObjectIStreamAsyncIterator<TRoot>::begin(void) {
    return *this;
}

template<typename TRoot>
CObjectIStreamAsyncIterator<TRoot>
CObjectIStreamAsyncIterator<TRoot>::end(void) {
    return CObjectIStreamAsyncIterator<TRoot>();
}


template<typename TRoot>
typename CObjectIStreamAsyncIterator<TRoot>::TObjectsQueue 
CObjectIStreamAsyncIterator<TRoot>::sx_ClearGarbageAndParse(
        CRef<CByteSource> bytesource, 
        ESerialDataFormat format,
        const CParams& params,
#if NCBI_COMPILER_MSVC && _MSC_VER < 1900
        TObjectsQueue garbage
#else
        TObjectsQueue&& garbage
#endif
        )
{
    {{
        TObjectsQueue dummy;
        swap(garbage, dummy);
        // garbage now gets destroyed, if last-reference
    }}

    // deserialize objects from bytesource
    unique_ptr<CObjectIStream> istr { CObjectIStream::Create(format, *bytesource) };

    TObjectsQueue queue;
    if (params.m_FnFilter) {
        for (TRoot& object : CObjectIStreamIterator<TRoot>( *istr, eNoOwnership, params)) {
            queue.push( CRef<TRoot>(&object));
        }
    } else {
        while(!istr->EndOfData()) {
            CRef<TRoot> object(new TRoot);
            istr->Read(&*object, object->GetThisTypeInfo());
            queue.push(object);
        }
    }
    return queue;
}

template<typename TRoot>
CObjectIStreamAsyncIterator<TRoot>::CData::CData(
        CObjectIStream& istr, EOwnership deleteInStream, FParserFunction parser,
        const CParams& params)
    : m_Istr(&istr)
    , m_Own(deleteInStream)
    , m_Parser(parser) 
    , m_ParserCount(   params.m_MaxParserThreads != 0 ? params.m_MaxParserThreads : 16)
    , m_RawBufferSize( params.m_MinRawBufferSize)
    , m_MaxRawSize(    params.m_SameThread ? 0 : params.m_MaxTotalRawSize)
    , m_CurrentRawSize(0)
    , m_Policy(params.m_ThreadPolicy)
    , m_EndOfData(m_Istr->EndOfData())
    , m_Params(params)
{
    if (m_MaxRawSize != 0 && !m_EndOfData) {
        m_Reader = thread(
            mem_fun<void, CObjectIStreamAsyncIterator<TRoot>::CData >(
                &CObjectIStreamAsyncIterator<TRoot>::CData::x_ReaderThread), this);
    }
}

template<typename TRoot>
CObjectIStreamAsyncIterator<TRoot>::CData::~CData() {
    if (m_Reader.joinable()) {
        m_EndOfData = true;
        m_ReaderCv.notify_all();
        m_Reader.join();
    }
    if (m_Istr && m_Own == eTakeOwnership) {
        delete m_Istr;
    }
}

// m_GarbageQueue processing:
//
// When the current object (the one returned by operator*())
// goes out of scope, if it is the last reference, the
// destructor of the object will be called from main
// thread, which is an expensive operation, which
// we want to offload to a different thread - "here are some
// objects - just let them go out of scope"
//
// So before calling m_ObjectsQueue pop we'll save the
// current object in the garbage-queue to prevent it from being
// destructed at this time, and will pass the garbage
// queue to sx_ClearGarbageAndParse (executed asynchrously), 
// where the destructors of the garbage-objects will be called
// (as apporpriate, as determined by CRefs going out of scope)

template<typename TRoot>
void
CObjectIStreamAsyncIterator<TRoot>::CData::x_UpdateObjectsQueue()
{
    // bring the next objects up front; save the garbage
    if(!m_ObjectsQueue.empty()) {
        m_GarbageQueue.push( m_ObjectsQueue.front());
        m_ObjectsQueue.pop();
    }

    // unpack the next objects-queue from futures-queue if empty
    if(    m_ObjectsQueue.empty() 
        && !m_FuturesQueue.empty()) 
    {
        m_ObjectsQueue = m_FuturesQueue.front().get();
        m_FuturesQueue.pop();
    }
}

template<typename TRoot>
void
CObjectIStreamAsyncIterator<TRoot>::CData::x_UpdateFuturesQueue()
{
    // nothing to deserialize, or already full
    if( m_FuturesQueue.size() >= m_ParserCount) {
        return;
    }
    if (m_EndOfData ||
        // no raw data ready yet, but we still have work to do
        (m_MaxRawSize != 0 && m_ReaderData.empty() && !m_FuturesQueue.empty())) {
        return;
    }
    CRef< CByteSource > data = x_GetNextData();
    if (data.IsNull()) {
        m_EndOfData = true;
        return;
    }

#if 0
    // for reference / profiling: clearing the garbage-queue
    // from this thread will make the processing considerably slower.
    // Instead, we'll pass the garbage to the async call below.
    if(false) {
            TObjectsQueue dummy;
            swap(m_GarbageQueue, dummy);
    }
#endif

    // launch async task to deserialize objects 
    // from the skipped bytes in delay-buffer, and 
    // also pass the garbage queue for destruction
    // of contents.
    // note: we can't move m_GarbageQueue directly
    // as it lacks ::clear() method that could restore
    // it to a valid empty state after move.
        
    TObjectsQueue tmp_garbage_queue;
    swap(m_GarbageQueue, tmp_garbage_queue);

    m_FuturesQueue.push( async( m_Policy, m_Parser,
        data,  m_Istr->GetDataFormat(), m_Params, move(tmp_garbage_queue)));
}

template<typename TRoot>
CRef< CByteSource >
CObjectIStreamAsyncIterator<TRoot>::CData::x_GetNextData(void)
{
    if (m_MaxRawSize == 0) {
        // read raw data in this (main) thread
        if (m_Istr->EndOfData()) {
            return CRef< CByteSource >();
        }
        const CNcbiStreampos endpos = 
            m_Istr->GetStreamPos()  + (CNcbiStreampos)(m_RawBufferSize);
        CStreamDelayBufferGuard guard(*m_Istr);
        do {
            m_Istr->SkipAnyContentObject();
        } while( !m_Istr->EndOfData() && m_Istr->GetStreamPos() < endpos);
        return guard.EndDelayBuffer();
    }

    // get raw data prepared by reader
    unique_lock<mutex> lck(m_ReaderMutex);
    while (m_ReaderData.empty()) {
        m_ReaderCv.wait(lck);
    }
    CRef< CByteSource > data = m_ReaderData.front();
    m_ReaderData.pop();
    m_CurrentRawSize -= m_ReaderDataSize.front();
    m_ReaderDataSize.pop();
    m_ReaderCv.notify_one();
    return data;
}

template<typename TRoot>
void
CObjectIStreamAsyncIterator<TRoot>::CData::x_ReaderThread(void)
{
    // Skip over some objects in stream without parsing, up to buffer_size.
    while (!m_Istr->EndOfData()) {
        const CNcbiStreampos startpos = m_Istr->GetStreamPos();
        const CNcbiStreampos endpos = 
            startpos  + (CNcbiStreampos)(m_RawBufferSize);

        CStreamDelayBufferGuard guard(*(m_Istr));
        try {
            do {
                m_Istr->SkipAnyContentObject();
            } while( !m_Istr->EndOfData() && m_Istr->GetStreamPos() < endpos);
        } catch (...) {
        }

        size_t this_buffer_size = m_Istr->GetStreamPos() - startpos;
        CRef< CByteSource > data = guard.EndDelayBuffer();
        {
            unique_lock<mutex> lck(m_ReaderMutex);
            // make sure we do not consume too much memory
            while (!m_EndOfData && m_CurrentRawSize >= m_MaxRawSize) {
                m_ReaderCv.wait(lck);
            }
            if (m_EndOfData) {
                break;
            }
            m_ReaderData.push( data);
            m_ReaderDataSize.push( this_buffer_size);
            m_CurrentRawSize += this_buffer_size;
            m_ReaderCv.notify_one();
        }
    }
    CRef< CByteSource > data;
    m_ReaderMutex.lock();
    m_ReaderData.push( data);
    m_ReaderDataSize.push(0);
    m_ReaderMutex.unlock();
    m_ReaderCv.notify_one();
}


/////////////////////////////////////////////////////////////////////////////
///  CObjectIStreamAsyncIterator<TRoot,TChild> implementation

template<typename TRoot, typename TChild>
CObjectIStreamAsyncIterator<TRoot, TChild>::CObjectIStreamAsyncIterator(void)
    : CParent()
{
}

template<typename TRoot, typename TChild>
CObjectIStreamAsyncIterator<TRoot, TChild>::CObjectIStreamAsyncIterator(
        CObjectIStream& istr, EOwnership deleteInStream,
        const CParams& params)
    : CParent(istr, deleteInStream,
        &CObjectIStreamAsyncIterator<TRoot, TChild>::sx_ClearGarbageAndParse, params)
{
}

template<typename TRoot, typename TChild>
CObjectIStreamAsyncIterator<TRoot, TChild>::CObjectIStreamAsyncIterator(
    const CObjectIStreamAsyncIterator& v)
        : CParent(v)
{
}

template<typename TRoot, typename TChild>
CObjectIStreamAsyncIterator<TRoot, TChild>&
CObjectIStreamAsyncIterator<TRoot, TChild>::operator=(
    const CObjectIStreamAsyncIterator& v) {
    CParent::operator=(v);
    return *this;
}

template<typename TRoot, typename TChild>
CObjectIStreamAsyncIterator<TRoot, TChild>::~CObjectIStreamAsyncIterator() {
}

template<typename TRoot, typename TChild>
CObjectIStreamAsyncIterator<TRoot, TChild>&
CObjectIStreamAsyncIterator<TRoot, TChild>::operator++() {
    CParent::operator++();
    return *this;
}

template<typename TRoot, typename TChild>
CObjectIStreamAsyncIterator<TRoot, TChild>&
CObjectIStreamAsyncIterator<TRoot, TChild>::begin(void) {
    return *this;
}

template<typename TRoot, typename TChild>
CObjectIStreamAsyncIterator<TRoot, TChild>
CObjectIStreamAsyncIterator<TRoot, TChild>::end(void) {
    return CObjectIStreamAsyncIterator<TRoot, TChild>();
}

template<typename TRoot, typename TChild>
typename CObjectIStreamAsyncIterator<TRoot, TChild>::TObjectsQueue 
CObjectIStreamAsyncIterator<TRoot, TChild>::sx_ClearGarbageAndParse(
        CRef<CByteSource> bytesource, 
        ESerialDataFormat format,
        const CParams& params,
#if NCBI_COMPILER_MSVC && _MSC_VER < 1900
        TObjectsQueue garbage
#else
        TObjectsQueue&& garbage
#endif
        )
{
    {{
        TObjectsQueue dummy;
        swap(garbage, dummy);
        // garbage now gets destroyed, if last-reference
    }}

    // deserialize objects from bytesource
    unique_ptr<CObjectIStream> istr { CObjectIStream::Create(format, *bytesource) };
    TObjectsQueue queue;
    for (TChild& object : CObjectIStreamIterator<TRoot, TChild>( *istr, eNoOwnership, params)) {
        queue.push( CRef<TChild>(&object));
    }
    return queue;
}




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// Iterate over objects in input stream
//
// IMPORTANT: the following API requires multi-threading
//
// IMPORTANT: this API is deprecated, use CObjectIStreamIterator instead (defined above)


template<typename TRoot, typename TObject>
class CIStreamIteratorThread_Base;

// Helper hook class
template<typename TRoot, typename TObject>
class CIStreamObjectHook : public CSerial_FilterObjectsHook<TObject>
{
public:
    CIStreamObjectHook(CIStreamIteratorThread_Base<TRoot,TObject>& thr)
        : m_Reader(thr)
    {
    }
    virtual void Process(const TObject& obj) override;
private:
    CIStreamIteratorThread_Base<TRoot,TObject>& m_Reader;
};

// Helper thread class
template<typename TRoot, typename TObject>
class CIStreamIteratorThread_Base : public CThread
{
public:
    CIStreamIteratorThread_Base(CObjectIStream& in, EOwnership deleteInStream)
        : m_In(in), m_Resume(0,1), m_Ready(0,1), m_Obj(0),
          m_Ownership(deleteInStream), m_Stop(false), m_Failed(false)
    {
    }
    // Resume thread, wait for the next object
    void Next(void)
    {
        m_Obj = 0;
        if (!m_Stop && !m_In.EndOfData()) {
            m_Resume.Post();
            m_Ready.Wait();
            if (m_Failed) {
                NCBI_THROW(CSerialException,eFail,
                             "invalid data object received");
            }
        }
    }
    // Request stop: thread is no longer needed
    void Stop(void)
    {
        m_Stop = true;
        m_Resume.Post();
        Join(0);
    }
    void Fail(void)
    {
        m_Failed = true;
        SetObject(0);
    }
    // Object is ready: suspend thread
    void SetObject(const TObject* obj)
    {
        m_Obj = obj;
        m_Ready.Post();
        m_Resume.Wait();
        if (m_Stop) {
            Exit(0);
        }
    }
    const TObject* GetObject(void) const
    {
        return m_Obj;
    }
protected:
    ~CIStreamIteratorThread_Base(void)
    {
        if (m_Ownership == eTakeOwnership) {
            delete &m_In;
        }
    }
    virtual void* Main(void)
    {
        return 0;
    }
protected:
    CObjectIStream&      m_In;
    CSemaphore           m_Resume;
    CSemaphore           m_Ready;
    const TObject*       m_Obj;
    EOwnership           m_Ownership;
    bool                 m_Stop;
    bool                 m_Failed;
};

// Reading thread for serial objects
template<typename TRoot, typename TObject>
class CIStreamObjectIteratorThread
    : public CIStreamIteratorThread_Base< TRoot,TObject >
{
public:
    CIStreamObjectIteratorThread(CObjectIStream& in, EOwnership deleteInStream)
        : CIStreamIteratorThread_Base< TRoot,TObject >(in, deleteInStream)
    {
    }
protected:
    ~CIStreamObjectIteratorThread(void)
    {
    }
    virtual void* Main(void) override
    {
        this->m_Resume.Wait();
        // Enumerate objects of requested type
        try {
            Serial_FilterObjects< TRoot >( this->m_In,
                new CIStreamObjectHook< TRoot, TObject >(*this));
            this->SetObject(0);
        } catch (CException& e) {
            NCBI_REPORT_EXCEPTION("In CIStreamObjectIteratorThread",e);
            this->Fail();
        }
        return 0;
    }
};

// Reading thread for std objects
template<typename TRoot, typename TObject>
class CIStreamStdIteratorThread
    : public CIStreamIteratorThread_Base< TRoot,TObject >
{
public:
    CIStreamStdIteratorThread(CObjectIStream& in, EOwnership deleteInStream)
        : CIStreamIteratorThread_Base< TRoot,TObject >(in, deleteInStream)
    {
    }
protected:
    ~CIStreamStdIteratorThread(void)
    {
    }
    virtual void* Main(void) override
    {
        this->m_Resume.Wait();
        // Enumerate objects of requested type
        try {
            Serial_FilterStdObjects< TRoot >( this->m_In,
                new CIStreamObjectHook< TRoot, TObject >(*this));
            this->SetObject(0);
        } catch (CException& e) {
            NCBI_REPORT_EXCEPTION("In CIStreamStdIteratorThread",e);
            this->Fail();
        }
        return 0;
    }
};

template<typename TRoot, typename TObject>
inline
void CIStreamObjectHook<TRoot,TObject>::Process(const TObject& obj)
{
    m_Reader.SetObject(&obj);
}

// Stream iterator base class
template<typename TRoot, typename TObject>
class CIStreamIterator_Base
{
public:
    void operator++()
    {
        m_Reader->Next();
    }
    void operator++(int)
    {
        m_Reader->Next();
    }
    const TObject& operator* (void) const
    {
        return *(m_Reader->GetObject());
    }
    const TObject* operator-> (void) const
    {
        return m_Reader->GetObject();
    }
    bool IsValid(void) const
    {
        return m_Reader->GetObject() != 0;
    }
protected:
    CIStreamIterator_Base()
        : m_Reader(nullptr)
    {
    }
    ~CIStreamIterator_Base(void)
    {
        if (m_Reader) {
            m_Reader->Stop();
        }
    }
private:
    // prohibit copy
    CIStreamIterator_Base(const CIStreamIterator_Base<TRoot,TObject>& v);
    // prohibit assignment
    CIStreamIterator_Base<TRoot,TObject>& operator=(
        const CIStreamIterator_Base<TRoot,TObject>& v);

protected:
    CIStreamIteratorThread_Base< TRoot, TObject > *m_Reader;
};

/// Stream iterator for serial objects
///
/// Usage:
///    CObjectIStream* is = CObjectIStream::Open(...);
///    CIStreamObjectIterator<CRootClass,CObjectClass> i(*is);
///    for ( ; i.IsValid(); ++i) {
///        const CObjectClass& obj = *i;
///        ...
///    }
/// IMPORTANT:
///     This API requires multi-threading!

template<typename TRoot, typename TObject>
class CIStreamObjectIterator
    : public CIStreamIterator_Base< TRoot, TObject>
{
public:
    CIStreamObjectIterator(CObjectIStream& in, EOwnership deleteInStream = eNoOwnership)
    {
        // Create reading thread, wait until it finds the next object
        this->m_Reader =
            new CIStreamObjectIteratorThread< TRoot, TObject >(in, deleteInStream);
        this->m_Reader->Run();
        this->m_Reader->Next();
    }
    ~CIStreamObjectIterator(void)
    {
    }
};

/// Stream iterator for standard type objects
///
/// Usage:
///    CObjectIStream* is = CObjectIStream::Open(...);
///    CIStreamStdIterator<CRootClass,string> i(*is);
///    for ( ; i.IsValid(); ++i) {
///        const string& obj = *i;
///        ...
///    }
/// IMPORTANT:
///     This API requires multi-threading!

template<typename TRoot, typename TObject>
class CIStreamStdIterator
    : public CIStreamIterator_Base< TRoot, TObject>
{
public:
    CIStreamStdIterator(CObjectIStream& in, EOwnership deleteInStream = eNoOwnership)
    {
        // Create reading thread, wait until it finds the next object
        this->m_Reader =
            new CIStreamStdIteratorThread< TRoot, TObject >(in,deleteInStream);
        this->m_Reader->Run();
        this->m_Reader->Next();
    }
    ~CIStreamStdIterator(void)
    {
    }
};

#endif // NCBI_THREADS


/* @} */

END_NCBI_SCOPE

#endif  /* STREAMITER__HPP */
