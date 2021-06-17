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
 * Authors:  Dmitry Kazimirov, Rafael Sadyrov
 *
 * File Description:  Simple test of NetStorage API (Common parts
 *                    shared between the RPC-based and the direct
 *                    access implementations).
 *
 */

#include <ncbi_pch.hpp>

// Older Boost versions don't yet trust Clang to support variadic macros
#ifdef __clang__
#  define BOOST_PP_VARIADICS 1
#endif

#include <misc/netstorage/netstorage.hpp>

#include <corelib/request_ctx.hpp>
#include <corelib/test_boost.hpp>

#include <util/random_gen.hpp>

#include <boost/preprocessor.hpp>

#include <algorithm>
#include <random>
#include <thread>
#include <chrono>
#include <type_traits>
#include <locale>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


#define APP_NAME                "test_netstorage"

// Configuration parameters
NCBI_PARAM_DECL(string, netstorage_api, service_name);
NCBI_PARAM_DEF(string, netstorage_api, service_name, "ST_UnitTest");
typedef NCBI_PARAM_TYPE(netstorage_api, service_name) TNetStorage_ServiceName;

NCBI_PARAM_DECL(string, netcache_api, service_name);
NCBI_PARAM_DEF(string, netcache_api, service_name, "NC_UnitTest");
typedef NCBI_PARAM_TYPE(netcache_api, service_name) TNetCache_ServiceName;

NCBI_PARAM_DECL(string, netstorage_api, app_domain);
NCBI_PARAM_DEF(string, netstorage_api, app_domain, "nst_test");
typedef NCBI_PARAM_TYPE(netstorage_api, app_domain) TNetStorage_AppDomain;

NCBI_PARAM_DECL(string, filetrack, site);
NCBI_PARAM_DEF(string, filetrack, site, "dev");
typedef NCBI_PARAM_TYPE(filetrack, site) TFileTrack_Site;

NCBI_PARAM_DECL(string, netstorage_api, my_ncbi_id);
NCBI_PARAM_DEF(string, netstorage_api, my_ncbi_id, "none");
typedef NCBI_PARAM_TYPE(netstorage_api, my_ncbi_id) TNetStorage_MyNcbiId;

NCBI_PARAM_DECL(string, filetrack, ft_token);
NCBI_PARAM_DEF(string, filetrack, ft_token, "eafe32733ade877a24555a9df15edcca42512040");
typedef NCBI_PARAM_TYPE(filetrack, ft_token) TFileTrack_Token;


// ENetStorageObjectLocation to type mapping
typedef integral_constant<ENetStorageObjectLocation, eNFL_Unknown>
    TLocationRelocated;
typedef integral_constant<ENetStorageObjectLocation, eNFL_NotFound>
    TLocationNotFound;
typedef integral_constant<ENetStorageObjectLocation, eNFL_NetCache>
    TLocationNetCache;
typedef integral_constant<ENetStorageObjectLocation, eNFL_FileTrack>
    TLocationFileTrack;


struct SCtx
{
    string desc;
    long line;

    SCtx() : line(0) {}
    SCtx(const string& d) : desc(d), line(0) {}
    SCtx& operator()(const string& d) { desc = d; return *this; }
    const SCtx& operator()(long l) { line = l; return *this; }
};

#define OUTPUT_CTX(ctx) ctx.desc << '[' << ctx.line << "]: "

#define BOOST_ERROR_CTX(message, ctx) \
    BOOST_ERROR(OUTPUT_CTX(ctx) << message)
                    
#define BOOST_CHECK_CTX(predicate, ctx) \
    BOOST_CHECK_MESSAGE(predicate, OUTPUT_CTX(ctx) << #predicate " failed")

#define BOOST_REQUIRE_CTX(predicate, ctx) \
    BOOST_REQUIRE_MESSAGE(predicate, OUTPUT_CTX(ctx) << #predicate " failed")

#define BOOST_CHECK_THROW_CTX(expression, exception, ctx) \
    do { \
        try { \
            expression; \
            BOOST_ERROR_CTX("exception " #exception " is expected", ctx); \
        } \
        catch (exception&) {} \
        catch (...) { \
            BOOST_ERROR_CTX("an unexpected exception is caught", ctx); \
        } \
    } while (0)

#define BOOST_CHECK_NO_THROW_CTX(expression, ctx) \
    do { \
        try { \
            expression; \
        } \
        catch (...) { \
            BOOST_ERROR_CTX("an unexpected exception is caught", ctx); \
            throw; \
        } \
    } while (0)

// Hierarchy for GetInfo() tests

template <class TLocation>
class CGetInfoBase : protected CNetStorageObjectInfo
{
public:
    typedef CGetInfoBase TGetInfoBase; // Self

    CGetInfoBase(const SCtx& ctx, CNetStorageObject object, Uint8 size)
        : CNetStorageObjectInfo(object.GetInfo()),
          m_Ctx(ctx),
          m_Size(size)
    {}

    virtual ~CGetInfoBase() {}

    void Check(bool loc_info)
    {
        CheckGetLocation();
        CheckGetObjectLocInfo(loc_info);
        CheckGetCreationTime();
        CheckGetSize();
        CheckGetStorageSpecificInfo();
        CheckToJSON();
    }

protected:
    const SCtx m_Ctx;

private:
    virtual void CheckGetLocation()
    {
        BOOST_CHECK_CTX(GetLocation() == TLocation::value, m_Ctx);
    }

    virtual void CheckGetObjectLocInfo(bool loc_info)
    {
        CJsonNode node = GetObjectLocInfo();

        if (loc_info) {
            if (node) {
                m_ObjectLocInfoRepr = node.Repr();
                BOOST_CHECK_CTX(m_ObjectLocInfoRepr.size(), m_Ctx);
            } else {
                BOOST_ERROR_CTX("GetObjectLocInfo() == NULL", m_Ctx);
            }
        } else {
            if (node) {
                BOOST_ERROR_CTX("GetObjectLocInfo() != NULL", m_Ctx);
            }
        }
    }

    virtual void CheckGetCreationTime()
    {
        BOOST_CHECK_CTX(GetCreationTime() != CTime(), m_Ctx);
    }

    virtual void CheckGetSize()
    {
        BOOST_CHECK_CTX(GetSize() == m_Size, m_Ctx);
    }

    virtual void CheckGetStorageSpecificInfo()
    {
        if (CJsonNode node = GetStorageSpecificInfo()) {
            BOOST_CHECK_CTX(node.Repr().size(), m_Ctx);
        } else {
            BOOST_ERROR_CTX("!GetStorageSpecificInfo()", m_Ctx);
        }
    }

    virtual void CheckToJSON()
    {
        if (CJsonNode node = ToJSON()) {
            if (m_ObjectLocInfoRepr.size()) {
                string repr = node.Repr();
                BOOST_CHECK_CTX(NPOS != NStr::Find(repr, m_ObjectLocInfoRepr), m_Ctx);
            }
        } else {
            BOOST_ERROR_CTX("!ToJSON()", m_Ctx);
        }
    }

    Uint8 m_Size;
    string m_ObjectLocInfoRepr;
};

template <class TLocation>
class CGetInfo : public CGetInfoBase<TLocation>
{
public:
    CGetInfo(const SCtx& ctx, CNetStorageObject object, Uint8 size)
        : CGetInfoBase<TLocation>(ctx, object, size)
    {
    }
};

template <>
class CGetInfo<TLocationNotFound> : public CGetInfoBase<TLocationNotFound>
{
public:
    CGetInfo(const SCtx& ctx, CNetStorageObject object, Uint8 = 0U)
        : TGetInfoBase(ctx, object, 0U)
    {}

private:
    void CheckGetCreationTime()
    {
        BOOST_CHECK_CTX(GetCreationTime() == CTime(), m_Ctx);
    }

    void CheckGetStorageSpecificInfo()
    {
        BOOST_CHECK_CTX(!GetStorageSpecificInfo(), m_Ctx);
    }
};

template <>
class CGetInfo<TLocationRelocated> : public CGetInfo<TLocationNotFound>
{
public:
    CGetInfo(const SCtx& ctx, CNetStorageObject object, Uint8 = 0U)
        : CGetInfo<TLocationNotFound>(ctx, object, 0U)
    {}
};


class CGetSize : private CNetStorageObject
{
public:
    CGetSize(const SCtx& ctx, CNetStorageObject object, Uint8 size)
        : CNetStorageObject(object),
          m_Ctx(ctx),
          m_Size(size)
    {}

    template <class TLocation>
    void Check()
    {
        BOOST_CHECK_CTX(GetSize() == m_Size, m_Ctx);
    }

private:
    const SCtx m_Ctx;
    Uint8 m_Size;
};

template <>
void CGetSize::Check<TLocationNotFound>()
{
    BOOST_CHECK_THROW_CTX(GetSize(), CNetStorageException, m_Ctx);
}

template <>
void CGetSize::Check<TLocationRelocated>()
{
    BOOST_CHECK_THROW_CTX(GetSize(), CNetStorageException, m_Ctx);
}

struct GetRandom
{
    // 777 for making data only partially fill internal buffers
    static const size_t DataSize = 20 * 1024 * 1024 + 777;

private:
    // Convenience class for random generator
    struct SSingleton
    {
        static const size_t shift = 100;

        CRandom r;
        default_random_engine generator;
        uniform_int_distribution<size_t> distribution;
        vector<char> data;

        static SSingleton& instance()
        {
            static SSingleton rs;
            return rs;
        }

    private:
        SSingleton()
            : distribution(0, shift)
        {
            r.Randomize();

            typedef numeric_limits<char> limits;
            uniform_int_distribution<short> range(limits::min(), limits::max());
            auto random_char = [&]() { return static_cast<char>(range(generator)); };

            const size_t size = DataSize + shift;
            data.reserve(size);
            generate_n(back_inserter(data), size, random_char);
        }
    };

public:
    operator CRandom&()
    {
        return SSingleton::instance().r;
    }

    static default_random_engine& GetGenerator()
    {
        return SSingleton::instance().generator;
    }

    // Convenience function to generate a unique key for CNetStorageByKey
    static string GetUniqueKey()
    {
        CRandom& random = GetRandom();
        return NStr::NumericToString(time(NULL)) + "t" +
            NStr::NumericToString(random.GetRandUint8());
    }

    static const char* GetData()
    {
        SSingleton& s = SSingleton::instance();
        return s.data.data() + s.distribution(s.generator);
    }
};


// An extended IReader interface
class IExtReader : public IReader
{
public:
    virtual void Close() = 0;

private:
    // This is not needed
    ERW_Result PendingCount(size_t*)
    {
        BOOST_ERROR("PendingCount() is not implemented");
        return eRW_NotImplemented;
    }
};

// A reader that is used to read from a string
class CStrReader : public IExtReader
{
public:
    CStrReader() : m_Ptr(NULL) {}
    CStrReader(const string& str) : m_Str(str), m_Ptr(NULL) {}

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0)
    {
        Init();

        if (!m_Remaining) return eRW_Eof;

        size_t to_read = 0;

        if (buf) {
            to_read = count < m_Remaining ? count : m_Remaining;
            memcpy(buf, m_Ptr, to_read);
            m_Ptr += to_read;
            m_Remaining -= to_read;
        }

        if (bytes_read) *bytes_read = to_read;
        return eRW_Success;
    }

    void Close() { m_Ptr = NULL; }

protected:
    string m_Str;
    const char* m_Ptr;
    size_t m_Remaining;

private:
    void Init()
    {
        if (!m_Ptr) {
            DoInit();
            m_Ptr = m_Str.data();
            m_Remaining = m_Str.size();
        }
    }

    virtual void DoInit() {};
};


// Empty data to test APIs
class CEmpData : public CStrReader
{
public:
    template <class TNetStorage>
    CEmpData(TNetStorage&, TNetStorageFlags)
        : CStrReader(string())
    {}

    size_t Size() { return 0; }
};

// String data to test APIs
class CStrData : public CStrReader
{
public:
    template <class TNetStorage>
    CStrData(TNetStorage&, TNetStorageFlags)
        : CStrReader("The quick brown fox jumps over the lazy dog")
    {}

    size_t Size() { return m_Str.size(); }
};

// Large volume data to test APIs
class CRndData : public IExtReader
{
public:
    struct SSource
    {
        const char* const data;
        SSource() : data(GetRandom::GetData()) {}
    };

    template <class TNetStorage>
    CRndData(TNetStorage&, TNetStorageFlags, double multiplier = 1.0)
        : m_Source(),
          m_Begin(m_Source.data),
          m_Size(static_cast<size_t>(min(multiplier, 1.0) * GetRandom::DataSize)),
          m_Length(m_Size)
    {}

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0)
    {
        if (!m_Length) return eRW_Eof;

        size_t read = m_Length < count ? m_Length : count;
        if (buf) memcpy(buf, m_Begin, read);
        m_Begin += read;
        m_Length -= read;
        if (bytes_read) *bytes_read = read;
        return eRW_Success;
    }

    void Close()
    {
        m_Begin = m_Source.data;
        m_Length = m_Size;
    }

    size_t Size() { return m_Size; }

private:
    SSource m_Source;
    const char* m_Begin;
    size_t m_Size;
    size_t m_Length;
};

// NetStorage data as a source to test APIs (simultaneous access to NetStorage)
class CNstData : public IExtReader
{
public:
    CNstData(CCombinedNetStorage& netstorage, TNetStorageFlags flags)
        : m_Object(netstorage.Create(flags))
    {
        Init();
        m_CleanUp = [&]() mutable
            {
                netstorage.Remove(m_Object.GetLoc());
            };
    }

    CNstData(CCombinedNetStorageByKey& netstorage, TNetStorageFlags flags)
        : m_Object(netstorage.Open(GetRandom::GetUniqueKey(), flags))
    {
        Init();
        m_CleanUp = [&]() mutable
            {
                CNetStorageObjectLoc loc(nullptr, m_Object.GetLoc());
                netstorage.Remove(loc.GetShortUniqueKey());
            };
    }

    ~CNstData()
    {
        m_CleanUp();
    }

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0)
    {
        if (m_Object.Eof()) return eRW_Eof;

        size_t read = m_Object.Read(buf, count);
        if (bytes_read) *bytes_read = read;
        return eRW_Success;
    }

    void Close() { m_Object.Close(); }
    size_t Size() { return m_Object.GetSize(); }

private:
    void Init()
    {
        // Create a NetStorage object to use as a source
        CRndData::SSource source;
        m_Object.Write(source.data, GetRandom::DataSize);
        m_Object.Close();
    }

    CNetStorageObject m_Object;
    function<void()> m_CleanUp;
};


// NetStorage reader/writer base class
template <class TBase>
class CNetStorageRW : public TBase
{
public:
    typedef CNetStorageRW TNetStorageRW; // Self

    CNetStorageRW(CNetStorageObject o) : m_Object(o) {}
    void Close() { m_Object.Close(); }

protected:
    ERW_Result Finalize(size_t in, size_t* out, ERW_Result result)
    {
        if (out) *out = in;
        return result;
    }

    CNetStorageObject m_Object;
};

// A reading/writing interface
template <class TImpl>
class CApi
{
public:
    typedef typename TImpl::CWriter TWriter;

    CApi(CNetStorageObject o) : m_Reader(o), m_Writer(o) {}
    IExtReader& GetReader() { return m_Reader; } 

private:
    typename TImpl::CReader m_Reader;
    typename TImpl::CWriter m_Writer;
};

// String reading/writing interface
struct SStrApiImpl
{
    class CReader : public CNetStorageRW<CStrReader>
    {
    public:
        CReader(CNetStorageObject o) : TNetStorageRW(o) {}
    private:
        void DoInit() { m_Object.Read(&m_Str); }
    };

    class CWriter : public CNetStorageRW<IWriter>
    {
    public:
        CWriter(CNetStorageObject o) : TNetStorageRW(o) {}
        ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0)
        {
            m_Output.append(static_cast<const char*>(buf), count);
            return Finalize(count, bytes_written, eRW_Success);
        }
        ERW_Result Flush(void)
        {
            m_Object.Write(m_Output);
            m_Output.clear();
            return eRW_Success;
        }

    private:
        string m_Output;
    };
};


// Buffer reading/writing interface
struct SBufApiImpl
{
    class CReader : public CNetStorageRW<IExtReader>
    {
    public:
        CReader(CNetStorageObject o) : TNetStorageRW(o) {}

        ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0)
        {
            if (m_Object.Eof()) return eRW_Eof;
            size_t read = m_Object.Read(buf, count);
            return Finalize(read, bytes_read, eRW_Success);
        }
    };

    class CWriter : public CNetStorageRW<IWriter>
    {
    public:
        CWriter(CNetStorageObject o) : TNetStorageRW(o) {}
        ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0)
        {
            m_Object.Write(buf, count);
            return Finalize(count, bytes_written, eRW_Success);
        }
        ERW_Result Flush(void) { return eRW_Success; }
    };
};


// GetReader()/GetWriter interface
struct SIrwApiImpl
{
    class CReader : public CNetStorageRW<IExtReader>
    {
    public:
        CReader(CNetStorageObject o)
            : TNetStorageRW(o),
              m_Impl(o.GetReader()),
              m_Result(eRW_Success)
        {}

        ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0)
        {
            if (m_Result == eRW_Eof) return eRW_Eof;
            return m_Result = m_Impl.Read(buf, count, bytes_read);
        }

    private:
        IReader& m_Impl;
        ERW_Result m_Result;
    };

    class CWriter : public CNetStorageRW<IWriter>
    {
    public:
        CWriter(CNetStorageObject o)
            : TNetStorageRW(o),
              m_Impl(o.GetWriter())
        {}

        ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0)
        {
            return m_Impl.Write(buf, count, bytes_written);
        }

        ERW_Result Flush(void) { return m_Impl.Flush(); }

    private:
        IWriter& m_Impl;
    };
};


// GetRWStream() interface
struct SIosApiImpl
{
    class CReader : public CNetStorageRW<IExtReader>
    {
    public:
        CReader(CNetStorageObject o)
            : TNetStorageRW(o),
              m_Impl(o.GetRWStream())
        {}

        ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0)
        {
            if (m_Impl->eof()) return eRW_Eof;

            if (!m_Impl->read(static_cast<char*>(buf), count)) {
                if (!m_Impl->eof()) {
                    return eRW_Error;
                }
            }

            return Finalize(m_Impl->gcount(), bytes_read, eRW_Success);
        }

    private:
        unique_ptr<CNcbiIostream> m_Impl;
    };

    class CWriter : public CNetStorageRW<IWriter>
    {
    public:
        CWriter(CNetStorageObject o)
            : TNetStorageRW(o),
              m_Impl(o.GetRWStream())
        {}

        ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0)
        {
            if (!m_Impl->write(static_cast<const char*>(buf), count)) {
                return eRW_Error;
            }

            return Finalize(count, bytes_written, eRW_Success);
        }

        ERW_Result Flush(void) { return m_Impl->flush() ? eRW_Success : eRW_Error; }

    private:
        unique_ptr<CNcbiIostream> m_Impl;
    };
};

// A helper to read from API/expected data
struct SReadHelperBase
{
    static const size_t kSize = 128 * 1024;
    IExtReader& source;
    char buf[kSize];
    size_t length;

    SReadHelperBase(IExtReader& s)
        : source(s),
          length(0),
          m_Result(eRW_Success)
    {}

    void Close(const SCtx& ctx)
    {
        BOOST_CHECK_NO_THROW_CTX(source.Close(), ctx);
    }

    bool operator()()
    {
        if (length) return true;

        // Read random length data
        CRandom& random = GetRandom();
        size_t buf_size = 1 + random.GetRandIndexSize_t(kSize);
        m_Result = source.Read(buf, buf_size, &length);

        switch (m_Result) {
        case eRW_Success:
        case eRW_Eof:
            return true;
        default:
            return false;
        }
    }

    void operator-=(size_t l)
    {
        length -= l;
        memmove(buf, buf + l, length);
    }

    // NB: Source can be non-empty and yet has no data yet
    bool Empty() { return m_Result == eRW_Eof && !length; }
    bool HasData() { return length; }

protected:
    ERW_Result m_Result;
};

template <class TApi, class TLocation>
struct SReadHelper : SReadHelperBase
{
    SReadHelper(IExtReader& s) : SReadHelperBase(s) {}

    bool operator()(const SCtx& ctx)
    {
        BOOST_CHECK_NO_THROW_CTX(return Read(ctx), ctx);
    }

private:
    bool Read(const SCtx& ctx)
    {
        if (SReadHelperBase::operator()()) {
            return true;
        }

        BOOST_ERROR_CTX("Failed to read: " << g_RW_ResultToString(m_Result), ctx);
        return false;
    }
};

template <class TApi>
struct SReadHelper<TApi, TLocationNotFound> : SReadHelperBase
{
    SReadHelper(IExtReader& s) : SReadHelperBase(s) {}

    bool operator()(const SCtx& ctx)
    {
        BOOST_CHECK_THROW_CTX(SReadHelperBase::operator()(),
                CNetStorageException, ctx);
        return false;
    }
};

template <class TApi>
struct SReadHelper<TApi, TLocationRelocated> : SReadHelper<TApi, TLocationNotFound>
{
    SReadHelper(IExtReader& s) : SReadHelper<TApi, TLocationNotFound>(s) {}
};

typedef CApi<SIosApiImpl> TIosApi;

template <>
struct SReadHelper<TIosApi, TLocationNotFound> : SReadHelperBase
{
    SReadHelper(IExtReader& s) : SReadHelperBase(s) {}

    bool operator()(const SCtx& ctx)
    {
        if (SReadHelperBase::operator()()) {
            BOOST_ERROR_CTX("iostream::read() was expected to fail", ctx);
        }

        return false;
    }
};

template <>
struct SReadHelper<TIosApi, TLocationRelocated> : SReadHelper<TIosApi, TLocationNotFound>
{
    SReadHelper(IExtReader& s) : SReadHelper<TIosApi, TLocationNotFound>(s) {}
};

// A helper to read from expected and write to object
template<class TApi>
struct SWriteHelper
{
    SWriteHelper(CNetStorageObject& o)
        : m_Dest(o),
          m_Result(eRW_Success),
          m_Empty(true)
    {}

    void Close(const SCtx& ctx)
    {
        BOOST_CHECK_CTX(m_Dest.Flush() == eRW_Success, ctx);
        BOOST_CHECK_NO_THROW_CTX(m_Dest.Close(), ctx);
    }

    template <class TReader>
    bool operator()(const TReader& reader, const SCtx& ctx)
    {
        CRandom& random = GetRandom();
        const char* data = reader.buf;
        size_t total = reader.length;

        while (total || m_Empty) {
            m_Empty = false;
            size_t length = 0;

            // Write random length data
            size_t to_write = total ? 1 + random.GetRandIndexSize_t(total) : 0;
            m_Result = m_Dest.Write(data, to_write, &length);

            if (m_Result == eRW_Success) {
                data += length;
                total -= length;
            } else {
                BOOST_ERROR_CTX("Failed to write to object: " <<
                        g_RW_ResultToString(m_Result), ctx);
                return false;
            }
        }

        return true;
    }

    operator bool() { return m_Result == eRW_Success; }

private:
    typename TApi::CWriter m_Dest;
    ERW_Result m_Result;
    bool m_Empty;
};


// Random attributes set
struct CAttributes
{
    typedef map<string, string, PNocase> TAttrs;
    TAttrs attrs;
    typedef vector<pair<const string*, const string*> > TData;
    TData data;

    CAttributes()
    {
        default_random_engine& generator(GetRandom::GetGenerator());

        // Attribute name/value length range
        uniform_int_distribution<size_t> name_range(5, 64);
        uniform_int_distribution<size_t> value_range(5, 900);
        auto name_length = bind(name_range, generator);
        auto value_length = bind(value_range, generator);

        const char name_chars[] =
            "0123456789_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

        uniform_int_distribution<size_t> name_indexes(0, sizeof(name_chars) - 2);
        uniform_int_distribution<short> value_chars(
                numeric_limits<char>::min(),
                numeric_limits<char>::max());
        auto name_char = [&]()->char { return name_chars[name_indexes(generator)]; };
        auto value_char = [&]() { return static_cast<char>(value_chars(generator)); };

        // Number of attributes
        uniform_int_distribution<size_t> attr_number(3, 10);
        size_t size = attr_number(generator);

        // The probability of an attribute value to be { empty, not_empty }:
        discrete_distribution<int> attr_set{ 1, 9 };

        string name;
        string value;

        name.reserve(name_range.max());
        value.reserve(value_range.max());

        while (size-- > 0) {
            generate_n(back_inserter(name), name_length(), name_char);

            if (attr_set(generator)) {
                generate_n(back_inserter(value), value_length(), value_char);
                attrs.insert(make_pair(name, value));
            } else {
                attrs.insert(make_pair(name, kEmptyStr));
            }

            name.clear();
            value.clear();
        }

        data.reserve(attrs.size());
        ITERATE(TAttrs, i, attrs) {
            data.push_back(make_pair(&i->first, &i->second));
        }
    }

    void Shuffle()
    {
        shuffle(data.begin(), data.end(), GetRandom::GetGenerator());
    }

    void Read(const SCtx& ctx, CNetStorageObject& object)
    {
        Shuffle();

        try {
            ITERATE(TData, i, data) {
                BOOST_CHECK_CTX(object.GetAttribute(*i->first) == *i->second, ctx);
            }
        }
        catch (...) {
            // Restore test context
            BOOST_TEST_CHECKPOINT(ctx.desc);
            throw;
        }

        CNetStorageObject::TAttributeList read_attrs(object.GetAttributeList());
        TAttrs attrs_copy(attrs);
        int failed = 0;

        ITERATE(CNetStorageObject::TAttributeList, i, read_attrs) {
            if (!attrs_copy.erase(*i)) {
                ++failed;
            }
        }

        if (failed) {
            BOOST_ERROR_CTX("" << failed << " attribute(s) missing", ctx);
        } else if (size_t size = attrs_copy.size()) {
            BOOST_ERROR_CTX("Got " << size << " less attributes than expected", ctx);
        }
    }

    void Write(const SCtx& ctx, CNetStorageObject& object)
    {
        Shuffle();

        try {
            locale loc;
            auto uc = [&](char& c) { c = toupper(c, loc); };

            NON_CONST_ITERATE(TData, i, data) {
                const string& name(*i->first);

                // Testing case-insensitity of attribute names
                string uc_name(name); // Attribute name in upper case
                for_each(uc_name.begin(), uc_name.end(), uc);

                object.SetAttribute(name, name);
                BOOST_CHECK_CTX(object.GetAttribute(uc_name) == name, ctx);
                object.SetAttribute(uc_name, *i->second);
            }
        }
        catch (...) {
            // Restore test context
            BOOST_TEST_CHECKPOINT(ctx.desc);
            throw;
        }
    }
};


typedef pair<string, TNetStorageFlags> TKey;


NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
            "CNetStorage Unit Test");
}


// Templated interfaces

namespace NType
{

struct TServerless
{
    static string GetInitString();
    static string GetNotFound();
    static bool CheckLocInfo();
};

struct TServer
{
    static string GetInitString();
    static string GetNotFound();
    static bool CheckLocInfo();
};

struct TNetCache
{
    static string GetInitString();
    static string GetNotFound();
    static bool CheckLocInfo();
};

}


namespace NStorage
{

template <class TType, class TClass, class TBackend> struct SFactory;
template <class TType, class TClass, class TBackend> struct SStorage;

namespace NTests
{

template <class TResult, class TStorage, class TKey>
TKey Relocate(TStorage&, const TKey&, TNetStorageFlags, SCtx&);

template <class TResult, class TStorage, class TKey>
void Exists(TStorage&, const TKey&, SCtx&);

template <class TResult, class TStorage, class TKey>
void Exists(TStorage&, const string& old_loc, const TKey&, SCtx&);

template <class TResult, class TStorage, class TKey>
void Remove(TStorage&, const TKey&, SCtx&);

}

template <class TType, class TClass, class TBackend> struct STestCase;

template <class TType, class TClass, class TBackend>
struct SFixture : STestCase<TType, TClass, TBackend>
{
};

}


namespace NObject
{

namespace NTests
{

template <class TResult, class TExpected = CRndData, class TIo = SBufApiImpl>
void Read(CNetStorageObject& object, TExpected& expected, SCtx& ctx);

template <class TSource = CRndData, class TIo = SBufApiImpl>
void Write(CNetStorageObject& object, TSource& source, SCtx ctx);

template <class TResult>
void GetSize(CNetStorageObject& object, Uint8 expected, SCtx ctx);

template <class TResult>
void SetExpiration(CNetStorageObject& object, const CTimeout& ttl, SCtx ctx);

template <class TResult, class TType, class TBackend>
void GetInfo(CNetStorageObject& object, Uint8 size, SCtx ctx);

}

template <class TType, class TObject, class TBackend> struct STestCase;

template <class TType, class TObject, class TBackend>
struct SFixture : STestCase<TType, TObject, TBackend>
{
};

}


namespace NSource
{

typedef CEmpData TEmp;
typedef CStrData TStr;
typedef CRndData TRnd;
typedef CNstData TNst;

}


namespace NIo
{

typedef SStrApiImpl TStr;
typedef SBufApiImpl TBuf;
typedef SIrwApiImpl TIrw;
typedef SIosApiImpl TIos;

template <class TType, class TSource, class TIo, class TBackend> struct STestCase;

template <class TType, class TSource, class TIo, class TBackend>
struct SFixture : STestCase<TType, TSource, TIo, TBackend>
{
};

}


namespace NBackend
{

struct TNC
{
    static const TNetStorageFlags flags = fNST_NetCache;
    static const ENetStorageObjectLocation location = eNFL_NetCache;
};

struct TFT
{
    static const TNetStorageFlags flags = fNST_FileTrack;
    static const ENetStorageObjectLocation location = eNFL_FileTrack;
};

template <class TBackend> struct TRelocate;

}


namespace NResult
{

// NB: It does not matter whether TLocationNetCache or TLocationFileTrack is used here
struct TSuccess { typedef TLocationNetCache TLocation; };
struct TFail    { typedef TLocationNotFound TLocation; };

typedef TFail TNotFound;
typedef TFail TCancel;
typedef TSuccess TFound;

template <class TResult>
struct TExists;

template <class TResult>
struct TRemove;

template <class TResult, class TType, class TBackend>
struct TSetExpiration;

template <class TType, class TBackend>
struct TExpired;

}


// Specialized implementations

namespace NType
{

string TServerless::GetInitString()
{
    return
        string("mode=direct").
        append("&nc=").append(TNetCache_ServiceName::GetDefault()).
        append("&domain=").append(TNetStorage_AppDomain::GetDefault()).
        append("&client=" APP_NAME).
        append("&ft_site=").append(TFileTrack_Site::GetDefault()).
        append("&ft_token=").append(TFileTrack_Token::GetDefault());
}

string TServerless::GetNotFound()
{
    return "fSkaPShlfjEubG02APJ8hozs-Wc8e2gLStUfew";
}

bool TServerless::CheckLocInfo()
{
    return true;
}

string TServer::GetInitString()
{
    return
        string("nst=").append(TNetStorage_ServiceName::GetDefault()).
        append("&domain=").append(TNetStorage_AppDomain::GetDefault()).
        append("&client=" APP_NAME).
        append("&err_mode=ignore");
}

string TServer::GetNotFound()
{
    return "DiggFUIicQdBCUsbfiJSBC68OfKUhMqb8aLWq_OCz7TCDN4jWKlNTDTS2saRsjl-J0ZYeica";
}

bool TServer::CheckLocInfo()
{
    return true;
}

string TNetCache::GetInitString()
{
    return
        string("nc=").append(TNetCache_ServiceName::GetDefault()).
        append("&domain=").append(TNetStorage_AppDomain::GetDefault()).
        append("&client=" APP_NAME).
        append("&default_storage=nc").
        append("&err_mode=ignore");
}

string TNetCache::GetNotFound()
{
    return "o-c8TRwX3VuFSZnf5BuKLelYjHcUK5xKpizKXbIX8EmNDNle";
}

bool TNetCache::CheckLocInfo()
{
    return false;
}

}


namespace NStorage
{

template <class TType, class TClass, class TBackend>
struct SFactory
{
    static TClass Create()
    {
        return TClass(TType::GetInitString(), TBackend::flags);
    }
};

void SetRegistry(CMemoryRegistry& registry)
{
    registry.Set("netstorage_api", "filetrack", "filetrack_api");
    registry.Set("netstorage_api", "netcache", "netcache_api");
    registry.Set("netcache_api", "service_name", TNetCache_ServiceName::GetDefault());
    registry.Set("netcache_api", "cache_name", TNetStorage_AppDomain::GetDefault());
    registry.Set("filetrack_api", "site", TFileTrack_Site::GetDefault());
    registry.Set("filetrack_api", "token", TFileTrack_Token::GetDefault());
    registry.Set("netstorage_api", "relocate_chunk", to_string(GetRandom::DataSize / 10));
}

template <class TBackend>
struct SFactory<NType::TServerless, CDirectNetStorage, TBackend>
{
    static CDirectNetStorage Create()
    {
        CMemoryRegistry registry;
        SetRegistry(registry);
        return CDirectNetStorage(
                registry,
                TNetStorage_ServiceName::GetDefault(),
                nullptr);
    }
};

template <class TBackend>
struct SFactory<NType::TServerless, CDirectNetStorageByKey, TBackend>
{
    static CDirectNetStorageByKey Create()
    {
        CMemoryRegistry registry;
        SetRegistry(registry);
        return CDirectNetStorageByKey(
                registry,
                TNetStorage_ServiceName::GetDefault(),
                nullptr,
                TNetStorage_AppDomain::GetDefault());
    }
};

template <class TType, class TClass, class TBackend>
struct SStorage
{
    TClass storage;
    SStorage()
        : storage(SFactory<TType, TClass, TBackend>::Create())
    {
    }
};


namespace NGenericApi
{

// TODO: Remove decltype after switching to C++14
template <class TNetStorage>
auto Open(TNetStorage& storage, TKey& key) -> decltype(storage.Open(key.first, key.second))
{
    return storage.Open(key.first, key.second);
}

template <class TNetStorage>
auto Open(TNetStorage& storage, const string& locator) -> decltype(storage.Open(locator))
{
    return storage.Open(locator);
}

TKey Relocate(CCombinedNetStorageByKey& storage, const TKey& key, TNetStorageFlags flags,
                    TNetStorageProgressCb cb)
{
    storage.Relocate(key.first, flags, key.second, cb);
    return TKey(key.first, flags);
}

string Relocate(CCombinedNetStorage& storage, const string& locator, TNetStorageFlags flags,
                    TNetStorageProgressCb cb)
{
    return storage.Relocate(locator, flags, cb);
}

bool Exists(CCombinedNetStorageByKey& storage, const TKey& key)
{
    return storage.Exists(key.first, key.second);
}

bool Exists(CCombinedNetStorage& storage, const string& locator)
{
    return storage.Exists(locator);
}

bool Exists(CDirectNetStorageByKey& storage, const string& old_loc,
        const TKey& key)
{
    return storage.Exists(old_loc, key.first, key.second);
}

bool Exists(CDirectNetStorage& storage, const string& old_loc,
        const string& new_loc)
{
    return storage.Exists(old_loc, new_loc);
}

template <class TNetStorage>
ENetStorageRemoveResult Remove(TNetStorage& storage, const TKey& key)
{
    return storage.Remove(key.first, key.second);
}

template <class TNetStorage>
ENetStorageRemoveResult Remove(TNetStorage& storage, const string& locator)
{
    return storage.Remove(locator);
}

}


namespace NTests
{

template <class TResult>
struct SRelocateCb
{
    SCtx& ctx;
    size_t calls = 0;
    Int8 current = 0;
    Int8 total = 0;

    SRelocateCb(SCtx& c) : ctx(c) {}

    void operator()(CJsonNode progress)
    {
        BOOST_CHECK_NO_THROW_CTX(current = progress.GetInteger("BytesRelocated"), ctx(__LINE__));
        BOOST_CHECK_NO_THROW_CTX(progress.GetString("Message"), ctx(__LINE__));

        if (current <= total) {
            BOOST_ERROR_CTX("Got relocated less than before", ctx(__LINE__));
        }

        if (current > static_cast<Int8>(GetRandom::DataSize)) {
            BOOST_ERROR_CTX("Got relocated more than expected", ctx(__LINE__));
        }

        ++calls;
        total = current;
    }

    void Check(function<void()> action)
    {
        BOOST_CHECK_NO_THROW_CTX(action(), ctx(__LINE__));

        if (!calls) {
            BOOST_ERROR_CTX("Progress callback not called", ctx(__LINE__));
        }
    }
};

template <>
struct SRelocateCb<NResult::TFail> : SRelocateCb<NResult::TSuccess>
{
    SRelocateCb(SCtx& c) : SRelocateCb<NResult::TSuccess>(c) {}

    void Check(function<void()> action)
    {
        BOOST_CHECK_THROW_CTX(action(), CNetStorageException, ctx(__LINE__));
    }
};

template <class TResult, class TStorage, class TKey>
TKey Relocate(TStorage& storage, const TKey& key, TNetStorageFlags flags, SCtx& ctx)
{
    SRelocateCb<TResult> impl(ctx);

    TKey result;
    auto action = [&]() { result = NGenericApi::Relocate(storage, key, flags, ref(impl)); };
 
    impl.Check(action);

    return result;
}

template <class TResult, class TStorage, class TKey>
void Exists(TStorage& storage, const TKey& key, SCtx& ctx)
{
    bool expected = NResult::TExists<TResult>::result;
    bool actual = NGenericApi::Exists(storage, key);
    BOOST_CHECK_CTX(actual == expected, ctx(__LINE__));
}

template <class TResult, class TStorage, class TKey>
void Exists(TStorage& storage, const string& old_loc, const TKey& key, SCtx& ctx)
{
    bool expected = NResult::TExists<TResult>::result;
    bool actual = NGenericApi::Exists(storage, old_loc, key);
    BOOST_CHECK_CTX(actual == expected, ctx(__LINE__));
}

template <class TResult, class TStorage, class TKey>
void Remove(TStorage& storage, const TKey& key, SCtx& ctx)
{
    ENetStorageRemoveResult expected = NResult::TRemove<TResult>::result;
    ENetStorageRemoveResult actual = NGenericApi::Remove(storage, key);
    BOOST_CHECK_CTX(actual == expected, ctx(__LINE__));
}

}


template <class TType, class TBackend>
struct STestCase<TType, CCombinedNetStorage, TBackend> :
        SStorage<TType, CCombinedNetStorage, TBackend>
{
    typedef SStorage<TType, CCombinedNetStorage, TBackend> TStorage;

    template <class TResult>
    void GetInfo(CNetStorageObject& object, Uint8 size, SCtx ctx)
    {
        NObject::NTests::GetInfo<TResult, TType, TBackend>(object, size, ctx);
    }

    void Test()
    {
        using namespace NStorage::NGenericApi;
        using namespace NTests;
        using namespace NObject::NTests;
        using namespace NResult;

        SCtx ctx;
        auto flags = TBackend::flags;
        auto relocate_flags = NBackend::TRelocate<TBackend>::TResult::flags;

        CCombinedNetStorage& netstorage(TStorage::storage);
        CRndData source(netstorage, flags);
        string locator(TType::GetNotFound());

        ctx("Non-existing object");
        CNetStorageObject object(Open(netstorage, locator));

        Read<TFail>(object, source, ctx);
        Relocate<TFail>(netstorage, locator, relocate_flags, ctx);
        Exists<TNotFound>(netstorage, locator, ctx);
        Remove<TNotFound>(netstorage, locator, ctx);

        ctx("Written object, no backend hint");
        object = netstorage.Create();
        Write(object, source, ctx);
        locator = object.GetLoc();

        Read<TSuccess>(object, source, ctx);
        Exists<TFound>(netstorage, locator, ctx);
        // Testing location for CXX-8221
        GetInfo<TFound>(object, source.Size(), ctx);
        Remove<TFound>(netstorage, locator, ctx);

        ctx("Written object");
        object = netstorage.Create(flags);
        Write(object, source, ctx);
        locator = object.GetLoc();

        Read<TSuccess>(object, source, ctx);
        Exists<TFound>(netstorage, locator, ctx);
        // Testing location for CXX-8221
        GetInfo<TFound>(object, source.Size(), ctx);
        Remove<TFound>(netstorage, locator, ctx);
    }
};

template <class TType, class TBackend>
struct STestCase<TType, CCombinedNetStorageByKey, TBackend> :
        SStorage<TType, CCombinedNetStorageByKey, TBackend>
{
    typedef SStorage<TType, CCombinedNetStorageByKey, TBackend> TStorage;

    template <class TResult>
    void GetInfo(CNetStorageObject& object, Uint8 size, SCtx ctx)
    {
        NObject::NTests::GetInfo<TResult, TType, TBackend>(object, size, ctx);
    }

    void Test()
    {
        using namespace NGenericApi;
        using namespace NTests;
        using namespace NObject::NTests;
        using namespace NResult;

        SCtx ctx;
        auto flags = TBackend::flags;
        auto relocate_flags = NBackend::TRelocate<TBackend>::TResult::flags;

        CCombinedNetStorageByKey& netstorage(TStorage::storage);
        CRndData source(netstorage, flags);

        TKey key(GetRandom::GetUniqueKey(), flags);
        TKey new_key(key.first, 0);

        ctx("Non-existing object");
        CNetStorageObject object(Open(netstorage, key));

        Read<TFail>(object, source, ctx);
        Relocate<TFail>(netstorage, key, relocate_flags, ctx);
        Exists<TNotFound>(netstorage, key, ctx);
        Remove<TNotFound>(netstorage, key, ctx);

        ctx("Written object");
        object = Open(netstorage, key);
        Write(object, source, ctx);

        ctx("Written object, no backend hint");
        CNetStorageObject new_object(Open(netstorage, new_key));

        Read<TSuccess>(new_object, source, ctx);
        Exists<TFound>(netstorage, new_key, ctx);
        // Testing location for CXX-8221
        GetInfo<TFound>(new_object, source.Size(), ctx);

        ctx("Written object");
        Read<TSuccess>(object, source, ctx);
        Exists<TFound>(netstorage, key, ctx);
        // Testing location for CXX-8221
        GetInfo<TFound>(object, source.Size(), ctx);
        Remove<TFound>(netstorage, key, ctx);
    }
};

template <class TType, class TBackend>
struct STestCase<TType, CDirectNetStorage, TBackend> :
        SStorage<TType, CDirectNetStorage, TBackend>
{
    typedef SStorage<TType, CDirectNetStorage, TBackend> TStorage;

    void TestSpecialExists(CDirectNetStorage& netstorage)
    {
        // Tests for Exists() overloads with optimising logic inside

        // All following objects do not exist.
        // Therefore, Exists() returns true only if internal logic has decided
        // to omit backend checks.

        using namespace NTests;
        using namespace NResult;

        SCtx ctx;

        // Empty loc
        string em_loc;

        // Not found
        string nf_loc("fSkaPShlfjEubG02APJ8hozs-Wc8e2gLStUfew");

        // NetCache
        string nc_loc("FTwqQHB4TkR2aU1LWO9c49b866n-5v_-AbluEAhUDxQAFyMpTR44Z14KB1VGNRB0VQ");

        // NetCache & Movable
        string nm_loc("2_L0jq62kIqop5OFhiGCLQ7dM-dS0tXK1x_QYLYksWS-Z51Z826GF-B6uSX4Ra4E6w");

        // FileTrack
        string ft_loc("gpiBxMecncDBjZbP7wuPZ2dqBrkhHn9mBiuAX4p76VuKLw");

        // FileTrack & Movable
        string fm_loc("MCojdmUuP3JjPzR9Tbkt1cMBovBm0SupCiENYAdEZGQHEA");

        Exists<TNotFound>(netstorage, em_loc, nf_loc, ctx("Empty vs Not found"));
        Exists<TNotFound>(netstorage, em_loc, nc_loc, ctx("Empty vs NetCache"));
        Exists<TNotFound>(netstorage, em_loc, nm_loc, ctx("Empty vs NetCache&Movable"));
        Exists<TNotFound>(netstorage, em_loc, ft_loc, ctx("Empty vs FileTrack"));
        Exists<TNotFound>(netstorage, em_loc, fm_loc, ctx("Empty vs FileTrack&Movable"));

        Exists<TNotFound>(netstorage, nf_loc, nf_loc, ctx("Not found vs Not found"));
        Exists<TNotFound>(netstorage, nf_loc, nc_loc, ctx("Not found vs NetCache"));
        Exists<TNotFound>(netstorage, nf_loc, nm_loc, ctx("Not found vs NetCache&Movable"));
        Exists<TNotFound>(netstorage, nf_loc, ft_loc, ctx("Not found vs FileTrack"));
        Exists<TNotFound>(netstorage, nf_loc, fm_loc, ctx("Not found vs FileTrack&Movable"));

        Exists<TFound>   (netstorage, nc_loc, nf_loc, ctx("NetCache vs Not found"));
        Exists<TFound>   (netstorage, nc_loc, nc_loc, ctx("NetCache vs NetCache"));
        Exists<TFound>   (netstorage, nc_loc, nm_loc, ctx("NetCache vs NetCache&Movable"));
        Exists<TNotFound>(netstorage, nc_loc, ft_loc, ctx("NetCache vs FileTrack"));
        Exists<TFound>   (netstorage, nc_loc, fm_loc, ctx("NetCache vs FileTrack&Movable"));

        Exists<TFound>   (netstorage, nm_loc, nf_loc, ctx("NetCache&Movable vs Not found"));
        Exists<TFound>   (netstorage, nm_loc, nc_loc, ctx("NetCache&Movable vs NetCache"));
        Exists<TFound>   (netstorage, nm_loc, nm_loc, ctx("NetCache&Movable vs NetCache&Movable"));
        Exists<TNotFound>(netstorage, nm_loc, ft_loc, ctx("NetCache&Movable vs FileTrack"));
        Exists<TFound>   (netstorage, nm_loc, fm_loc, ctx("NetCache&Movable vs FileTrack&Movable"));

        Exists<TFound>   (netstorage, ft_loc, nf_loc, ctx("FileTrack vs Not found"));
        Exists<TNotFound>(netstorage, ft_loc, nc_loc, ctx("FileTrack vs NetCache"));
        Exists<TFound>   (netstorage, ft_loc, nm_loc, ctx("FileTrack vs NetCache&Movable"));
        Exists<TFound>   (netstorage, ft_loc, ft_loc, ctx("FileTrack vs FileTrack"));
        Exists<TFound>   (netstorage, ft_loc, fm_loc, ctx("FileTrack vs FileTrack&Movable"));

        Exists<TFound>   (netstorage, fm_loc, nf_loc, ctx("FileTrack&Movable vs Not found"));
        Exists<TNotFound>(netstorage, fm_loc, nc_loc, ctx("FileTrack&Movable vs NetCache"));
        Exists<TFound>   (netstorage, fm_loc, nm_loc, ctx("FileTrack&Movable vs NetCache&Movable"));
        Exists<TFound>   (netstorage, fm_loc, ft_loc, ctx("FileTrack&Movable vs FileTrack"));
        Exists<TFound>   (netstorage, fm_loc, fm_loc, ctx("FileTrack&Movable vs FileTrack&Movable"));
    }

    void Test()
    {
        using namespace NGenericApi;
        using namespace NTests;
        using namespace NObject::NTests;
        using namespace NResult;

        SCtx ctx;
        auto flags = TBackend::flags;

        CDirectNetStorage& netstorage(TStorage::storage);
        CRndData source(netstorage, flags);
        string locator(TType::GetNotFound());

        ctx("Non-existing object");
        CDirectNetStorageObject object(Open(netstorage, locator));

        Read<TFail>(object, source, ctx);

        ctx("Written object");
        object = netstorage.Create(TNetStorage_ServiceName::GetDefault(), flags);
        Write(object, source, ctx);
        locator = object.GetLoc();

        Read<TSuccess>(object, source, ctx);

        ctx("Existing object");
        object = Open(netstorage, locator);

        Read<TSuccess>(object, source, ctx);
        Remove<TFound>(netstorage, locator, ctx);

        TestSpecialExists(netstorage); // Testing CXX-8219

        ctx("Config reporting");
        CJsonNode config = netstorage.ReportConfig();
        BOOST_CHECK_CTX(config.HasKey("filetrack"), ctx(__LINE__));
        BOOST_CHECK_CTX(config.HasKey("netcache"), ctx(__LINE__));
    }
};

template <class TType, class TBackend>
struct STestCase<TType, CDirectNetStorageByKey, TBackend> :
        SStorage<TType, CDirectNetStorageByKey, TBackend>
{
    typedef SStorage<TType, CDirectNetStorageByKey, TBackend> TStorage;

    void TestSpecialExists(CDirectNetStorageByKey& netstorage)
    {
        // Tests for Exists() overloads with optimising logic inside

        // All following objects do not exist.
        // Therefore, Exists() returns true only if internal logic has decided
        // to omit backend checks.

        using namespace NTests;
        using namespace NResult;

        SCtx ctx;

        // Empty loc
        string em_loc;

        // Not found
        string nf_loc("fSkaPShlfjEubG02APJ8hozs-Wc8e2gLStUfew");
        auto nf_key = make_pair(GetRandom::GetUniqueKey(), 0);

        // NetCache
        string nc_loc("FTwqQHB4TkR2aU1LWO9c49b866n-5v_-AbluEAhUDxQAFyMpTR44Z14KB1VGNRB0VQ");
        auto nc_key = make_pair(GetRandom::GetUniqueKey(), fNST_NetCache);

        // NetCache & Movable
        string nm_loc("2_L0jq62kIqop5OFhiGCLQ7dM-dS0tXK1x_QYLYksWS-Z51Z826GF-B6uSX4Ra4E6w");
        auto nm_key = make_pair(GetRandom::GetUniqueKey(), fNST_NetCache | fNST_Movable);

        // FileTrack
        string ft_loc("gpiBxMecncDBjZbP7wuPZ2dqBrkhHn9mBiuAX4p76VuKLw");
        auto ft_key = make_pair(GetRandom::GetUniqueKey(), fNST_FileTrack);

        // FileTrack & Movable
        string fm_loc("MCojdmUuP3JjPzR9Tbkt1cMBovBm0SupCiENYAdEZGQHEA");
        auto fm_key = make_pair(GetRandom::GetUniqueKey(), fNST_FileTrack | fNST_Movable);

        Exists<TNotFound>(netstorage, em_loc, nf_key, ctx("Empty vs Not found"));
        Exists<TNotFound>(netstorage, em_loc, nc_key, ctx("Empty vs NetCache"));
        Exists<TNotFound>(netstorage, em_loc, nm_key, ctx("Empty vs NetCache&Movable"));
        Exists<TNotFound>(netstorage, em_loc, ft_key, ctx("Empty vs FileTrack"));
        Exists<TNotFound>(netstorage, em_loc, fm_key, ctx("Empty vs FileTrack&Movable"));

        Exists<TNotFound>(netstorage, nf_loc, nf_key, ctx("Not found vs Not found"));
        Exists<TNotFound>(netstorage, nf_loc, nc_key, ctx("Not found vs NetCache"));
        Exists<TNotFound>(netstorage, nf_loc, nm_key, ctx("Not found vs NetCache&Movable"));
        Exists<TNotFound>(netstorage, nf_loc, ft_key, ctx("Not found vs FileTrack"));
        Exists<TNotFound>(netstorage, nf_loc, fm_key, ctx("Not found vs FileTrack&Movable"));

        Exists<TFound>   (netstorage, nc_loc, nf_key, ctx("NetCache vs Not found"));
        Exists<TFound>   (netstorage, nc_loc, nc_key, ctx("NetCache vs NetCache"));
        Exists<TFound>   (netstorage, nc_loc, nm_key, ctx("NetCache vs NetCache&Movable"));
        Exists<TNotFound>(netstorage, nc_loc, ft_key, ctx("NetCache vs FileTrack"));
        Exists<TFound>   (netstorage, nc_loc, fm_key, ctx("NetCache vs FileTrack&Movable"));

        Exists<TFound>   (netstorage, nm_loc, nf_key, ctx("NetCache&Movable vs Not found"));
        Exists<TFound>   (netstorage, nm_loc, nc_key, ctx("NetCache&Movable vs NetCache"));
        Exists<TFound>   (netstorage, nm_loc, nm_key, ctx("NetCache&Movable vs NetCache&Movable"));
        Exists<TNotFound>(netstorage, nm_loc, ft_key, ctx("NetCache&Movable vs FileTrack"));
        Exists<TFound>   (netstorage, nm_loc, fm_key, ctx("NetCache&Movable vs FileTrack&Movable"));

        Exists<TFound>   (netstorage, ft_loc, nf_key, ctx("FileTrack vs Not found"));
        Exists<TNotFound>(netstorage, ft_loc, nc_key, ctx("FileTrack vs NetCache"));
        Exists<TFound>   (netstorage, ft_loc, nm_key, ctx("FileTrack vs NetCache&Movable"));
        Exists<TFound>   (netstorage, ft_loc, ft_key, ctx("FileTrack vs FileTrack"));
        Exists<TFound>   (netstorage, ft_loc, fm_key, ctx("FileTrack vs FileTrack&Movable"));

        Exists<TFound>   (netstorage, fm_loc, nf_key, ctx("FileTrack&Movable vs Not found"));
        Exists<TNotFound>(netstorage, fm_loc, nc_key, ctx("FileTrack&Movable vs NetCache"));
        Exists<TFound>   (netstorage, fm_loc, nm_key, ctx("FileTrack&Movable vs NetCache&Movable"));
        Exists<TFound>   (netstorage, fm_loc, ft_key, ctx("FileTrack&Movable vs FileTrack"));
        Exists<TFound>   (netstorage, fm_loc, fm_key, ctx("FileTrack&Movable vs FileTrack&Movable"));
    }

    void Test()
    {
        using namespace NGenericApi;
        using namespace NTests;
        using namespace NObject::NTests;
        using namespace NResult;

        SCtx ctx;
        auto flags = TBackend::flags;

        CDirectNetStorageByKey& netstorage(TStorage::storage);
        CRndData source(netstorage, flags);

        TKey key(GetRandom::GetUniqueKey(), flags);
        TKey new_key(key.first, 0);

        ctx("Non-existing object");
        CDirectNetStorageObject object(Open(netstorage, key));

        Read<TFail>(object, source, ctx);

        ctx("Written object");
        object = Open(netstorage, key);
        Write(object, source, ctx);

        Read<TSuccess>(object, source, ctx);

        ctx("Written object, no backend hint");
        CDirectNetStorageObject new_object(Open(netstorage, new_key));

        Read<TSuccess>(new_object, source, ctx);
        Remove<TFound>(netstorage, key, ctx);

        TestSpecialExists(netstorage); // Testing CXX-8219
    }
};

template <class TBackend>
struct STestCase<NType::TNetCache, CCombinedNetStorage, TBackend> :
        SStorage<NType::TNetCache, CCombinedNetStorage, TBackend>
{
    typedef SStorage<NType::TNetCache, CCombinedNetStorage, TBackend> TStorage;

    void Test()
    {
        using namespace NGenericApi;
        using namespace NTests;
        using namespace NObject::NTests;
        using namespace NResult;

        SCtx ctx;
        auto flags = TBackend::flags;

        CCombinedNetStorage& netstorage(TStorage::storage);
        CRndData source(netstorage, flags);
        string locator(NType::TNetCache::GetNotFound());

        ctx("Non-existing object");
        CNetStorageObject object(Open(netstorage, locator));

        Read<TFail>(object, source, ctx);
        Exists<TNotFound>(netstorage, locator, ctx);
        Remove<TNotFound>(netstorage, locator, ctx);

        ctx("Written object");
        object = netstorage.Create();
        Write(object, source, ctx);
        locator = object.GetLoc();

        Read<TSuccess>(object, source, ctx);
        Exists<TFound>(netstorage, locator, ctx);
        Remove<TFound>(netstorage, locator, ctx);

        ctx("Removed object");
        Read<TFail>(object, source, ctx);
        Exists<TNotFound>(netstorage, locator, ctx);
        Remove<TNotFound>(netstorage, locator, ctx);
    }
};

}


namespace NObject
{

namespace NTests
{

template <class TResult, class TExpected, class TIo>
void Read(CNetStorageObject& object, TExpected& expected, SCtx& ctx)
{
    SReadHelper<TIo, NResult::TSuccess::TLocation> expected_reader(expected);
    typename TIo::CReader object_reader(object);
    SReadHelper<TIo, typename TResult::TLocation> actual_reader(object_reader);

    for (;;) {
        if (!expected_reader(ctx(__LINE__))) break;
        if (!actual_reader(ctx(__LINE__))) break;

        size_t length = min(expected_reader.length, actual_reader.length);

        if (memcmp(expected_reader.buf, actual_reader.buf, length)) {
            BOOST_ERROR_CTX("Read() returned corrupted data", ctx(__LINE__));
            break;
        }

        expected_reader -= length;
        actual_reader -= length;

        if (expected_reader.Empty()) {
            if (actual_reader.Empty()) {
                break;
            }

            if (actual_reader.HasData()) {
                BOOST_ERROR_CTX("Got more data than expected", ctx(__LINE__));
                break;
            }
        }

        if (actual_reader.Empty()) {
            if (expected_reader.HasData()) {
                BOOST_ERROR_CTX("Got less data than expected", ctx(__LINE__));
                break;
            }
        }
    }

    expected_reader.Close(ctx(__LINE__));
    actual_reader.Close(ctx(__LINE__));
}

template <class TSource, class TIo>
void Write(CNetStorageObject& object, TSource& source, SCtx ctx)
{
    SReadHelper<TIo, NResult::TSuccess::TLocation> source_reader(source);
    SWriteHelper<TIo> dest_writer(object);

    while (source_reader(ctx(__LINE__)) &&
            dest_writer(source_reader, ctx(__LINE__)) &&
            !source_reader.Empty()) {
        source_reader.length = 0;
    }

    source_reader.Close(ctx(__LINE__));
    dest_writer.Close(ctx(__LINE__));
}

template <class TResult>
void GetSize(CNetStorageObject& object, Uint8 expected, SCtx ctx)
{
    CGetSize(ctx(__LINE__), object, expected).Check<typename TResult::TLocation>();
}

template <class TResult>
void SetExpiration(CNetStorageObject& object, double ttl, SCtx ctx)
{
    BOOST_CHECK_THROW_CTX(object.SetExpiration(CTimeout(ttl)), CNetStorageException, ctx(__LINE__));
}

template <>
void SetExpiration<NResult::TSuccess>(CNetStorageObject& object, double ttl, SCtx ctx)
{
    BOOST_CHECK_NO_THROW_CTX(object.SetExpiration(CTimeout(ttl)), ctx(__LINE__));
}

template <class TResult, class TType, class TBackend>
void GetInfo(CNetStorageObject& object, Uint8 size, SCtx ctx)
{
    typedef typename conditional<
            is_same<TResult, NResult::TNotFound>::value,
            NResult::TNotFound::TLocation,
            integral_constant<ENetStorageObjectLocation, TBackend::location>
        >::type TLocation;

    CGetInfo<TLocation>(ctx(__LINE__), object, size).Check(TType::CheckLocInfo());
}

template <class TResult>
struct SObjectRelocateCb : NStorage::NTests::SRelocateCb<TResult>
{
    SObjectRelocateCb(CDirectNetStorageObject&, SCtx& c) : NStorage::NTests::SRelocateCb<TResult>(c) {}
};

template <>
struct SObjectRelocateCb<NResult::TCancel> : NStorage::NTests::SRelocateCb<NResult::TCancel>
{
    typedef NStorage::NTests::SRelocateCb<NResult::TCancel> TBase;

    CDirectNetStorageObject& object;

    SObjectRelocateCb(CDirectNetStorageObject& o, SCtx& c) : TBase(c), object(o) {}

    void operator()(CJsonNode progress)
    {
        TBase::operator()(progress);

        if (calls > 1) {
            object.CancelRelocate();
        }
    }
};

template <class TResult>
string Relocate(CDirectNetStorageObject& object, TNetStorageFlags flags, SCtx& ctx)
{
    SObjectRelocateCb<TResult> impl(object, ctx);

    string result;
    auto action = [&]() { result = object.Relocate(flags, ref(impl)); };
 
    impl.Check(action);

    return result;
}

template <class TResult, class TBackend>
void GetUserInfo(CDirectNetStorageObject& object, SCtx ctx)
{
    BOOST_CHECK_THROW_CTX(object.GetUserInfo(), CNetStorageException, ctx(__LINE__));
}

template <>
void GetUserInfo<NResult::TSuccess, NBackend::TNC>(CDirectNetStorageObject& object, SCtx ctx)
{
    auto user_info = object.GetUserInfo();
    BOOST_CHECK_CTX(user_info.first.empty(), ctx(__LINE__));
    BOOST_CHECK_CTX(user_info.second.empty(), ctx(__LINE__));
}

template <>
void GetUserInfo<NResult::TSuccess, NBackend::TFT>(CDirectNetStorageObject& object, SCtx ctx)
{
    auto user_info = object.GetUserInfo();
    BOOST_CHECK_CTX(!user_info.first.empty(), ctx(__LINE__));
    BOOST_CHECK_CTX(!user_info.second.empty(), ctx(__LINE__));
}

template <class TResult, class TBackend>
void FileTrack_Path(CDirectNetStorageObject& object, SCtx ctx)
{
    BOOST_CHECK_THROW_CTX(object.FileTrack_Path(), CNetStorageException, ctx(__LINE__));
}

template <>
void FileTrack_Path<NResult::TSuccess, NBackend::TFT>(CDirectNetStorageObject& object, SCtx ctx)
{
    string path;
    BOOST_CHECK_NO_THROW_CTX(path = object.FileTrack_Path(), ctx(__LINE__));

    if (path.empty()) {
        BOOST_ERROR_CTX("FileTrack_Path() returned empty path", ctx(__LINE__));
    }
}

template <class TResult>
void Locator(CDirectNetStorageObject& object, const string& locator, SCtx ctx)
{
    BOOST_CHECK_CTX(object.Locator().GetLocator() == locator, ctx(__LINE__));
}

template <>
void Locator<NResult::TFail>(CDirectNetStorageObject& object, const string& locator, SCtx ctx)
{
    BOOST_CHECK_CTX(object.Locator().GetLocator() != locator, ctx(__LINE__));
}

template <class TResult>
void Remove(CDirectNetStorageObject& object, SCtx ctx)
{
    BOOST_CHECK_CTX(object.Remove() == eNSTRR_Removed, ctx(__LINE__));
}

template <>
void Remove<NResult::TNotFound>(CDirectNetStorageObject& object, SCtx ctx)
{
    BOOST_CHECK_CTX(object.Remove() == eNSTRR_NotFound, ctx(__LINE__));
}

template <class TResult, class TType, class TBackend>
void ReadAttributes(CNetStorageObject&, CAttributes&, SCtx)
{
}

template <>
void ReadAttributes<NResult::TSuccess, NType::TServer, NBackend::TNC>(CNetStorageObject& object,
        CAttributes& attributes, SCtx ctx)
{
    BOOST_CHECK_NO_THROW_CTX(attributes.Read(ctx, object), ctx);
}

template <>
void ReadAttributes<NResult::TFail, NType::TServer, NBackend::TNC>(CNetStorageObject& object,
        CAttributes& attributes, SCtx ctx)
{
    BOOST_CHECK_THROW_CTX(attributes.Read(ctx, object), CNetStorageException, ctx);
}

template <class TType, class TBackend>
void WriteAttributes(CNetStorageObject&, CAttributes&, SCtx)
{
}

template <>
void WriteAttributes<NType::TServer, NBackend::TNC>(CNetStorageObject& object,
        CAttributes& attributes, SCtx ctx)
{
    BOOST_CHECK_NO_THROW_CTX(attributes.Write(ctx, object), ctx);
}

}


template <class TType, class TBackend>
struct STestCase<TType, CNetStorageObject, TBackend> :
        NStorage::SStorage<TType, CCombinedNetStorage, TBackend>
{
    typedef NStorage::SStorage<TType, CCombinedNetStorage, TBackend> TStorage;

    template <class TResult>
    void GetInfo(CNetStorageObject& object, Uint8 size, SCtx ctx)
    {
        NTests::GetInfo<TResult, TType, TBackend>(object, size, ctx);
    }

    template <class TResult, class TStorage, class TKey>
    void Remove(TStorage& netstorage, const TKey& key, SCtx& ctx)
    {
        NStorage::NTests::Remove<TResult, TStorage, TKey>(netstorage, key, ctx);
    }

    template <class TResult>
    void ReadAttributes(CNetStorageObject& object, CAttributes& attributes, SCtx ctx)
    {
        NTests::ReadAttributes<TResult, TType, TBackend>(object, attributes, ctx);
    }

    void WriteAttributes(CNetStorageObject& object, CAttributes& attributes, SCtx ctx)
    {
        NTests::WriteAttributes<TType, TBackend>(object, attributes, ctx);
    }

    void Test()
    {
        using namespace NStorage::NGenericApi;
        using namespace NStorage::NTests;
        using namespace NTests;
        using namespace NResult;

        SCtx ctx;
        auto flags = TBackend::flags;

        CCombinedNetStorage& netstorage(TStorage::storage);
        string locator(TType::GetNotFound());

        ctx("Non-existing object");
        CAttributes attributes;
        CNetStorageObject object(Open(netstorage, locator));

        ReadAttributes<TFail>(object, attributes, ctx);
        GetSize<TFail>(object, 0, ctx);
        GetInfo<TNotFound>(object, 0, ctx);

        ctx("Written object, empty");
        CEmpData empty_source(netstorage, flags);
        object = netstorage.Create(flags);
        Write(object, empty_source, ctx);
        WriteAttributes(object, attributes, ctx);

        Read<TSuccess>(object, empty_source, ctx);
        ReadAttributes<TSuccess>(object, attributes, ctx);
        GetSize<TSuccess>(object, 0, ctx);
        GetInfo<TFound>(object, 0, ctx);

        typedef typename TSetExpiration<TSuccess, TType, TBackend>::TResult
                TSetExpirationResult;

        ctx("Written object, half size");
        CRndData half_source(netstorage, flags, 0.5);
        object = netstorage.Create(flags);
        Write(object, half_source, ctx);
        SetExpiration<TSetExpirationResult>(object, 10.0, ctx);

        Read<TSuccess>(object, half_source, ctx);
        GetSize<TSuccess>(object, half_source.Size(), ctx);
        GetInfo<TFound>(object, half_source.Size(), ctx);

        ctx("Written object");
        CRndData source(netstorage, flags);
        Write(object, source, ctx);

        Read<TSuccess>(object, source, ctx);
        GetSize<TSuccess>(object, source.Size(), ctx);
        GetInfo<TFound>(object, source.Size(), ctx);

        ctx("Written object, quarter size");
        CRndData quarter_source(netstorage, flags, 0.25);
        Write(object, quarter_source, ctx);
        locator = object.GetLoc();

        Read<TSuccess>(object, quarter_source, ctx);
        GetSize<TSuccess>(object, quarter_source.Size(), ctx);
        GetInfo<TFound>(object, quarter_source.Size(), ctx);
        Remove<TFound>(netstorage, locator, ctx);

        ctx("Removed object");
        Read<TFail>(object, quarter_source, ctx);
        ReadAttributes<TFail>(object, attributes, ctx);
        GetSize<TFail>(object, quarter_source.Size(), ctx);
        GetInfo<TNotFound>(object, quarter_source.Size(), ctx);

        ctx("Written object");
        Write(object, source, ctx);

        SetExpiration<TSetExpirationResult>(object, 1.0, ctx);
        this_thread::sleep_for(chrono::seconds(3));

        ctx("Expired object");
        typedef typename TExpired<TType, TBackend>::TResult TExpiredResult;
        GetSize<TExpiredResult>(object, source.Size(), ctx);
        GetInfo<TSuccess>(object, source.Size(), ctx);
        Read<TExpiredResult>(object, source, ctx);
    }
};

template <class TBackend>
struct STestCase<NType::TServerless, CDirectNetStorageObject, TBackend> :
        NStorage::SStorage<NType::TServerless, CDirectNetStorage, TBackend>
{
    typedef NStorage::SStorage<NType::TServerless, CDirectNetStorage, TBackend> TStorage;

    template <class TResult>
    void GetUserInfo(CDirectNetStorageObject& object, SCtx ctx)
    {
        NTests::GetUserInfo<TResult, TBackend>(object, ctx);
    }

    template <class TResult>
    void Remove(CDirectNetStorageObject& object, SCtx ctx)
    {
        NTests::Remove<TResult>(object, ctx);
    }

    void Test()
    {
        using namespace NStorage::NTests;
        using namespace NTests;
        using namespace NResult;

        SCtx ctx;
        auto flags = TBackend::flags;
        auto relocate_flags = NBackend::TRelocate<TBackend>::TResult::flags;

        CDirectNetStorage& netstorage(TStorage::storage);
        CRndData source(netstorage, flags);

        ctx("Creating test object");
        CDirectNetStorageObject object(netstorage.Create(TNetStorage_ServiceName::GetDefault(), flags));
        Write(object, source, ctx);
        Read<TSuccess>(object, source, ctx);
        GetUserInfo<TSuccess>(object, ctx);
        FileTrack_Path<TSuccess, TBackend>(object, ctx);

        ctx("Relocating");
        string new_locator = Relocate<TSuccess>(object, relocate_flags, ctx);

        ctx("Original object");
        Read<TFail>(object, source, ctx);
        GetUserInfo<TFail>(object, ctx);
        FileTrack_Path<TFail, TBackend>(object, ctx);
        Locator<TFail>(object, new_locator, ctx);
        Remove<TNotFound>(object, ctx);

        ctx("Relocated object");
        object = netstorage.Open(new_locator);
        Read<TSuccess>(object, source, ctx);

        ctx("Relocating with cancel");
        Relocate<TCancel>(object, flags, ctx);

        ctx("Relocated object after cancel");
        Read<TSuccess>(object, source, ctx);
        Locator<TSuccess>(object, new_locator, ctx);
        Remove<TSuccess>(object, ctx);
    }
};

}


namespace NIo
{

template <class TType, class TSource, class TIo, class TBackend>
struct STestCase : NStorage::SStorage<TType, CCombinedNetStorage, TBackend>
{
    typedef NStorage::SStorage<TType, CCombinedNetStorage, TBackend> TStorage;

    template <class TResult>
    void Read(CNetStorageObject& object, TSource& source, SCtx& ctx)
    {
        NObject::NTests::Read<TResult, TSource, TIo>(object, source, ctx);
    }

    void Write(CNetStorageObject& object, TSource& source, SCtx ctx)
    {
        NObject::NTests::Write<TSource, TIo>(object, source, ctx);
    }

    void Test()
    {
        using namespace NStorage::NTests;
        using namespace NObject::NTests;
        using namespace NResult;

        SCtx ctx;
        auto flags = TBackend::flags;

        CCombinedNetStorage& netstorage(TStorage::storage);

        ctx("IO test");
        CNetStorageObject object(netstorage.Create(flags));
        TSource source(netstorage, flags);
        Write(object, source, ctx);
        Read<TSuccess>(object, source, ctx);
        Remove<TSuccess>(netstorage, object.GetLoc(), ctx);
    }
};

// All *IoEmpIos* tests are no-op
// (due to ignore of empty writes by instances created with GetRWStream()).
template <class TType, class TBackend>
struct STestCase<TType, NSource::TEmp, NIo::TIos, TBackend>
{
    void Test() {}
};

}


namespace NBackend
{

template <> struct TRelocate<TNC> { typedef TFT TResult; };
template <> struct TRelocate<TFT> { typedef TNC TResult; };

}


namespace NResult
{

template <class TResult>
struct TExists
{
    static const bool result = false;
};

template <>
struct TExists<TFound>
{
    static const bool result = true;
};

template <class TResult>
struct TRemove
{
    static const ENetStorageRemoveResult result = eNSTRR_NotFound;
};

template <>
struct TRemove<TFound>
{
    static const ENetStorageRemoveResult result = eNSTRR_Removed;
};

template <class TR, class TType, class TBackend>
struct TSetExpiration
{
    typedef TR TResult;
};

// SetExpiration is not supported by FileTrack
template <>
struct TSetExpiration<TSuccess, NType::TServerless, NBackend::TFT>
{
    typedef TFail TResult;
};

// NetCache has too much time to wait for object expiration and
// FileTrack does not support expiration setting.
// So, TSuccess is here for all cases except ones via NetStorage server.
template <class TType, class TBackend>
struct TExpired
{
    typedef TSuccess TResult;
};

template <class TBackend>
struct TExpired<NType::TServer, TBackend>
{
    typedef TFail TResult;
};

}


// Workaround as Boost > 1.57 seems not like more than one BOOST_PP_COMMA()
#define COMMON_TEST_CASE(NAME, FIXTURE) \
    typedef FIXTURE T ## NAME ## Fixture; \
    BOOST_FIXTURE_TEST_CASE(NAME, T ## NAME ## Fixture) \

#define STORAGE_TEST_CASE(TYPE, STORAGE, BACKEND) \
    COMMON_TEST_CASE(TYPE ## STORAGE ## BACKEND, \
        NStorage::SFixture< \
            NType::T ## TYPE BOOST_PP_COMMA() \
            C ## STORAGE BOOST_PP_COMMA() \
            NBackend::T ## BACKEND>) \
    { Test(); }

#define OBJECT_TEST_CASE(TYPE, OBJECT, BACKEND) \
    COMMON_TEST_CASE(TYPE ## OBJECT ## BACKEND, \
        NObject::SFixture< \
            NType::T ## TYPE BOOST_PP_COMMA() \
            C ## OBJECT BOOST_PP_COMMA() \
            NBackend::T ## BACKEND>) \
    { Test(); }

#define IO_TEST_CASE(TYPE, BACKEND, SOURCE, IO) \
    COMMON_TEST_CASE(TYPE ## Io ## SOURCE ## IO ## BACKEND, \
        NIo::SFixture< \
            NType::T ## TYPE BOOST_PP_COMMA() \
            NSource::T ## SOURCE BOOST_PP_COMMA() \
            NIo::T ## IO BOOST_PP_COMMA() \
            NBackend::T ## BACKEND>) \
    { Test(); }


#define CALL_MACRO(m, p) m p

#define CALL_STORAGE_TEST_CASE(r, p) CALL_MACRO(STORAGE_TEST_CASE, p)
#define CALL_OBJECT_TEST_CASE(r, p)  CALL_MACRO(OBJECT_TEST_CASE,  p)
#define CALL_IO_TEST_CASE(r, p)      CALL_MACRO(IO_TEST_CASE,      p)

#define TEST_CASES(TYPES, BACKENDS, OBJECTS, STORAGES, SOURCES, IOS) \
    BOOST_PP_LIST_FOR_EACH_PRODUCT(CALL_STORAGE_TEST_CASE, 3, (TYPES, STORAGES, BACKENDS)) \
    BOOST_PP_LIST_FOR_EACH_PRODUCT(CALL_OBJECT_TEST_CASE,  3, (TYPES, OBJECTS, BACKENDS)) \
    BOOST_PP_LIST_FOR_EACH_PRODUCT(CALL_IO_TEST_CASE,      4, (TYPES, BACKENDS, SOURCES, IOS))


#define SOURCES (Emp, Str, Rnd, Nst)
#define IOS     (Str, Buf, Irw, Ios)

#define TEST_SUITE(TYPE, BACKENDS, OBJECTS, STORAGES) \
BOOST_AUTO_TEST_SUITE(TYPE) \
    TEST_CASES( \
        BOOST_PP_TUPLE_TO_LIST((TYPE)), \
        BOOST_PP_TUPLE_TO_LIST(BACKENDS), \
        BOOST_PP_TUPLE_TO_LIST(OBJECTS), \
        BOOST_PP_TUPLE_TO_LIST(STORAGES), \
        BOOST_PP_TUPLE_TO_LIST(SOURCES), \
        BOOST_PP_TUPLE_TO_LIST(IOS)) \
BOOST_AUTO_TEST_SUITE_END()


TEST_SUITE(
        Serverless,
        (NC, FT),
        (NetStorageObject, DirectNetStorageObject),
        (CombinedNetStorage, CombinedNetStorageByKey, DirectNetStorage, DirectNetStorageByKey))

TEST_SUITE(
        Server,
        (NC, FT),
        (NetStorageObject),
        (CombinedNetStorage, CombinedNetStorageByKey))

TEST_SUITE(
        NetCache,
        (NC),
        (NetStorageObject),
        (CombinedNetStorage))
