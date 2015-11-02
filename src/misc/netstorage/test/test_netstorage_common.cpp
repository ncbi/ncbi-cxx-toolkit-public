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

#include <misc/netstorage/netstorage.hpp>

#include <corelib/test_boost.hpp>

#include <util/random_gen.hpp>

#include <boost/preprocessor.hpp>
#include <boost/type_traits/integral_constant.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


#define APP_NAME                "test_netstorage"

// Configuration parameters
NCBI_PARAM_DECL(string, netstorage, service_name);
NCBI_PARAM_DEF(string, netstorage, service_name, "ST_Test");
typedef NCBI_PARAM_TYPE(netstorage, service_name) TNetStorage_ServiceName;

NCBI_PARAM_DECL(string, netcache, service_name);
NCBI_PARAM_DEF(string, netcache, service_name, "NC_UnitTest");
typedef NCBI_PARAM_TYPE(netcache, service_name) TNetCache_ServiceName;

NCBI_PARAM_DECL(string, netstorage, app_domain);
NCBI_PARAM_DEF(string, netstorage, app_domain, "nst_test");
typedef NCBI_PARAM_TYPE(netstorage, app_domain) TNetStorage_AppDomain;

NCBI_PARAM_DECL(string, filetrack, site);
NCBI_PARAM_DEF(string, filetrack, site, "dev");
typedef NCBI_PARAM_TYPE(filetrack, site) TFileTrack_Site;

NCBI_PARAM_DECL(string, filetrack, api_key);
NCBI_PARAM_DEF(string, filetrack, api_key, \
        "bqyMqDEHsQ3foORxBO87FMNhXjv9LxuzF9Rbs4HLuiaf2pHOku7D9jDRxxyiCtp2");
typedef NCBI_PARAM_TYPE(filetrack, api_key) TFileTrack_Key;


// ENetStorageObjectLocation to type mapping
typedef boost::integral_constant<ENetStorageObjectLocation, eNFL_Unknown>
    TLocationRelocated;
typedef boost::integral_constant<ENetStorageObjectLocation, eNFL_NotFound>
    TLocationNotFound;
typedef boost::integral_constant<ENetStorageObjectLocation, eNFL_NetCache>
    TLocationNetCache;
typedef boost::integral_constant<ENetStorageObjectLocation, eNFL_FileTrack>
    TLocationFileTrack;


struct SCtx
{
    string desc;
    long line;

    SCtx() : line(0) {}
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
        CheckGetNFSPathname();
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

    virtual void CheckGetNFSPathname()
    {
        BOOST_CHECK_THROW_CTX(GetNFSPathname(), CNetStorageException, m_Ctx);
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
class CGetInfo;

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

template <>
class CGetInfo<TLocationNetCache> : public CGetInfoBase<TLocationNetCache>
{
public:
    CGetInfo(const SCtx& ctx, CNetStorageObject object, Uint8 size)
        : TGetInfoBase(ctx, object, size)
    {
        BOOST_CHECK_CTX(object.GetSize() == size, m_Ctx);
    }
};

template <>
class CGetInfo<TLocationFileTrack> : public CGetInfoBase<TLocationFileTrack>
{
public:
    CGetInfo(const SCtx& ctx, CNetStorageObject object, Uint8 size)
        : TGetInfoBase(ctx, object, size)
    {
        BOOST_CHECK_CTX(object.GetSize() == size, m_Ctx);
    }

private:
    void CheckGetNFSPathname()
    {
        BOOST_CHECK_CTX(GetNFSPathname().size(), m_Ctx);
    }
};


// Convenience class for random generator
struct CRandomSingleton
{
    CRandom r;
    CRandomSingleton() { r.Randomize(); }

    static CRandom& instance()
    {
        static CRandomSingleton rs;
        return rs.r;
    }
};


// Convenience function to fill container with random char data
template <class TContainer>
void RandomFill(TContainer& container, size_t length, bool printable = true)
{
    CRandom& random(CRandomSingleton::instance());
    const char kMin = printable ? '!' : numeric_limits<char>::min();
    const char kMax = printable ? '~' : numeric_limits<char>::max();
    container.clear();
    container.reserve(length);
    while (length-- > 0) {
        container.push_back(kMin + random.GetRandIndex(kMax - kMin + 1));
    }
}


// An extended IReader interface
class IExtReader : public IReader
{
public:
    virtual void Close() = 0;

private:
    // This is not needed
    ERW_Result PendingCount(size_t* count)
    {
        BOOST_ERROR("PendingCount() is not implemented");
        return eRW_NotImplemented;
    }
};

// An extended IWriter interface
struct IExtWriter : public IWriter
{
    virtual void Close() = 0;
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
    CEmpData()
        : CStrReader(string())
    {}

    size_t Size() { return 0; }
};

// String data to test APIs
class CStrData : public CStrReader
{
public:
    CStrData()
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
        vector<char> data;

        SSource()
        {
            const size_t kSize = 20 * 1024 * 1024; // 20MB
            data.reserve(kSize);
            RandomFill(data, kSize, false);
        }
    };

    CRndData()
        : m_Begin(0),
          m_Length(m_Source.data.size())
    {}

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0)
    {
        if (!m_Length) return eRW_Eof;

        size_t read = m_Length < count ? m_Length : count;
        if (buf) memcpy(buf, &m_Source.data[m_Begin], read);
        m_Begin += read;
        m_Length -= read;
        if (bytes_read) *bytes_read = read;
        return eRW_Success;
    }

    void Close()
    {
        m_Begin = 0;
        m_Length = m_Source.data.size();
    }

    size_t Size() { return m_Source.data.size(); }

private:
    SSource m_Source;
    size_t m_Begin;
    size_t m_Length;
};

// NetStorage data as a source to test APIs (simultaneous access to NetStorage)
class CNstData : public IExtReader
{
public:
    CNstData(CNetStorageObject object)
        : m_Object(object)
    {
        // Create a NetStorage object to use as a source
        CRndData::SSource source;
        m_Object.Write(&source.data[0], source.data.size());
        m_Object.Close();
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
    CNetStorageObject m_Object;
};

// NetStorage object guard to remove objects after tests
template <class TNetStorage>
class CNstObjectGuard
{
public:
    CNstObjectGuard(TNetStorage& netstorage, const string& key)
        : m_NetStorage(netstorage),
          m_Key(key)
    {}

    ~CNstObjectGuard() { try { m_NetStorage.Remove(m_Key); } catch (...) {} }

private:
    TNetStorage m_NetStorage;
    const string m_Key;
};

// NetStorage source data guard to remove underlying objects after tests
template <class TNetStorage>
class CNstDataGuard : public CNstData, private CNstObjectGuard<TNetStorage>
{
public:
    CNstDataGuard(CNetStorageObject& object, TNetStorage& netstorage,
            const string& key)
        : CNstData(object),
          CNstObjectGuard<TNetStorage>(netstorage, key)
    {}
};


// Convenience function to generate a unique key for CNetStorageByKey
string GetUniqueKey()
{
    return NStr::NumericToString(time(NULL)) + "t" +
        NStr::NumericToString(CRandomSingleton::instance().GetRandUint8());
}


// Template for source data creation

template <class TExpected, class TNetStorage>
TExpected* Create(TNetStorage&, TNetStorageFlags)
{
    return new TExpected();
}

template <>
CNstData* Create<CNstData, CNetStorage>(CNetStorage& netstorage,
        TNetStorageFlags flags)
{
    CNetStorageObject object(netstorage.Create(flags));
    const string key(object.GetLoc());
    return new CNstDataGuard<CNetStorage>(object, netstorage, key);
}

template <>
CNstData* Create<CNstData, CNetStorageByKey>(CNetStorageByKey& netstorage,
        TNetStorageFlags flags)
{
    const string key(GetUniqueKey());
    CNetStorageObject object(netstorage.Open(key, flags));
    return new CNstDataGuard<CNetStorageByKey>(object, netstorage, key);
}

template <>
CNstData* Create<CNstData, CCombinedNetStorage>(CCombinedNetStorage& netstorage,
        TNetStorageFlags flags)
{
    CNetStorageObject object(netstorage.Create(flags));
    const string key(object.GetLoc());
    return new CNstDataGuard<CCombinedNetStorage>(object, netstorage, key);
}

template <>
CNstData* Create<CNstData, CCombinedNetStorageByKey>(CCombinedNetStorageByKey& netstorage,
        TNetStorageFlags flags)
{
    const string key(GetUniqueKey());
    CNetStorageObject object(netstorage.Open(key, flags));
    return new CNstDataGuard<CCombinedNetStorageByKey>(object, netstorage, key);
}


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
    CApi(CNetStorageObject o) : m_Reader(o), m_Writer(o) {}
    IExtReader& GetReader() { return m_Reader; } 
    IExtWriter& GetWriter() { return m_Writer; }

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

    class CWriter : public CNetStorageRW<IExtWriter>
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

    class CWriter : public CNetStorageRW<IExtWriter>
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

    class CWriter : public CNetStorageRW<IExtWriter>
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
        auto_ptr<CNcbiIostream> m_Impl;
    };

    class CWriter : public CNetStorageRW<IExtWriter>
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
        auto_ptr<CNcbiIostream> m_Impl;
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

    bool operator()(const SCtx& ctx)
    {
        if (length) return true;

        // Read random length data
        CRandom& random(CRandomSingleton::instance());
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
        if (SReadHelperBase::operator()(ctx)) {
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
        BOOST_CHECK_THROW_CTX(SReadHelperBase::operator()(ctx),
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
        if (SReadHelperBase::operator()(ctx)) {
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
struct SWriteHelper
{
    SWriteHelper(IExtWriter& dest)
        : m_Dest(dest),
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
        CRandom& random(CRandomSingleton::instance());
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
    IExtWriter& m_Dest;
    ERW_Result m_Result;
    bool m_Empty;
};


// Random attributes set
struct SAttrApiBase
{
    typedef map<string, string> TAttrs;
    TAttrs attrs;
    typedef vector<pair<const string*, const string*> > TData;
    TData data;

    SAttrApiBase()
    {
        CRandom& r(CRandomSingleton::instance());

        // Attribute name/value length range
        const size_t kMinLength = 5;
        const size_t kNameMaxLength = 256;
        const size_t kValueMaxLength = 1024;

        // Number of attributes
        const size_t kMinSize = 3;
        const size_t kMaxSize = 10;
        size_t size = r.GetRand(kMinSize, kMaxSize);

        // The probability of an attribute to be set:
        // (kSetProbability - 1) / kSetProbability
        const size_t kSetProbability = 10;

        string name;
        string value;

        name.reserve(kNameMaxLength);
        value.reserve(kValueMaxLength);

        while (size-- > 0) {
            RandomFill(name, r.GetRand(kMinLength, kNameMaxLength));

            if (r.GetRandIndex(kSetProbability)) {
                RandomFill(value, r.GetRand(kMinLength, kValueMaxLength));
                attrs.insert(make_pair(name, value));
            } else {
                attrs.insert(make_pair(name, kEmptyStr));
            }
        }

        data.reserve(attrs.size());
        ITERATE(TAttrs, i, attrs) {
            data.push_back(make_pair(&i->first, &i->second));
        }
    }

    void Shuffle()
    {
        random_shuffle(data.begin(), data.end());
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
            NON_CONST_ITERATE(TData, i, data) {
                object.SetAttribute(*i->first, *i->first);
                BOOST_CHECK_CTX(object.GetAttribute(*i->first) == *i->first, ctx);
                object.SetAttribute(*i->first, *i->second);
            }
        }
        catch (...) {
            // Restore test context
            BOOST_TEST_CHECKPOINT(ctx.desc);
            throw;
        }
    }
};

typedef TLocationNotFound TShouldThrow;

template <class TNetStorage>
struct TAttrTesting : boost::true_type {};

template <>
struct TAttrTesting<CCombinedNetStorage> : boost::false_type {};

template <>
struct TAttrTesting<CCombinedNetStorageByKey> : boost::false_type {};


// Default implementation (attribute reading/writing always throws)
template <class TAttrTesting>
struct SAttrApi : SAttrApiBase
{
    template <class TLocation>
    void Read(TLocation, const SCtx& ctx, CNetStorageObject& object)
    {
        BOOST_CHECK_THROW_CTX(SAttrApiBase::Read(ctx, object),
                CNetStorageException, ctx);
    }

    template <class TLocation>
    void Write(TLocation, const SCtx& ctx, CNetStorageObject& object)
    {
        BOOST_CHECK_THROW_CTX(SAttrApiBase::Write(ctx, object),
                CNetStorageException, ctx);
    }
};

// Attribute testing is enabled
template <>
struct SAttrApi<boost::true_type> : SAttrApiBase
{
    template <class TLocation>
    void Read(TLocation, const SCtx& ctx, CNetStorageObject& object)
    {
        SAttrApiBase::Read(ctx, object);
    }

    template <class TLocation>
    void Write(TLocation, const SCtx& ctx, CNetStorageObject& object)
    {
        SAttrApiBase::Write(ctx, object);
    }

    void Read(TShouldThrow, const SCtx& ctx, CNetStorageObject& object)
    {
        BOOST_CHECK_THROW_CTX(SAttrApiBase::Read(ctx, object),
                CNetStorageException, ctx);
    }

    void Write(TShouldThrow, const SCtx& ctx, CNetStorageObject& object)
    {
        BOOST_CHECK_THROW_CTX(SAttrApiBase::Write(ctx, object),
                CNetStorageException, ctx);
    }
};


// Locations and flags used for tests

struct SLocBase
{
    typedef boost::true_type TAttrTesting;

    static const bool check_relocate = true;
    static const bool loc_info = true;

    static const char* init_string() { return ""; }

    static const char* not_found()
    {
        return
            "CiB5fBBOPGA-TABpLX2KezBRLG35vvgq5_umuK_f6Nmv16_17pra7qSJytGakfnGv4I";
    }
};

struct SNC2FT : SLocBase
{
    typedef TLocationFileTrack TSource;
    static const TNetStorageFlags source = fNST_Persistent;

    static const TNetStorageFlags non_existent = fNST_Fast | fNST_Movable;

    typedef TLocationNetCache TCreate;
    static const TNetStorageFlags create = fNST_Fast | fNST_Movable;

    static const TNetStorageFlags immovable = fNST_Fast;

    static const TNetStorageFlags readable = fNST_Persistent | fNST_Movable;

    typedef TLocationFileTrack TRelocate;
    static const TNetStorageFlags relocate = fNST_Persistent;
};

struct SFT2NC : SLocBase
{
    typedef TLocationNetCache TSource;
    static const TNetStorageFlags source = fNST_Fast;

    static const TNetStorageFlags non_existent = fNST_Persistent | fNST_Movable;

    typedef TLocationFileTrack TCreate;
    static const TNetStorageFlags create = fNST_Persistent | fNST_Movable;

    static const TNetStorageFlags immovable = fNST_Persistent;

    static const TNetStorageFlags readable = fNST_Fast | fNST_Movable;

    typedef TLocationNetCache TRelocate;
    static const TNetStorageFlags relocate = fNST_Fast;
};

struct SDirectNC
{
    typedef boost::integral_constant<ENetStorageObjectLocation, eNFL_NetCache>
        TSource;
    static const TNetStorageFlags source = 0;

    static const TNetStorageFlags non_existent = 0;

    typedef TSource TCreate;
    static const TNetStorageFlags create = 0;

    static const TNetStorageFlags immovable = 0;

    static const TNetStorageFlags readable = 0;

    typedef TCreate TRelocate;
    static const TNetStorageFlags relocate = 0;

    typedef boost::false_type TAttrTesting;

    static const bool check_relocate = false;
    static const bool loc_info = false;

    static const char* init_string() { return "&default_storage=nc"; }

    static const char* not_found()
    {
        return "o-c8TRwX3VuFSZnf5BuKLelYjHcUK5xKpizKXbIX8EmNDNle";
    }
};


template <class TNetStorage>
inline TNetStorage g_GetNetStorage(const char* mode)
{
    string nst_service(TNetStorage_ServiceName::GetDefault());
    string nc_service(TNetCache_ServiceName::GetDefault());
    string nst_app_domain(TNetStorage_AppDomain::GetDefault());
    string init_string(
            "nst="     + nst_service +
            "&nc="     + nc_service +
            "&domain=" + nst_app_domain +
            "&client="   APP_NAME +
            mode);
    return CNetStorage(init_string);
}

template <>
inline CNetStorageByKey g_GetNetStorage<CNetStorageByKey>(const char* mode)
{
    string nst_service(TNetStorage_ServiceName::GetDefault());
    string nc_service(TNetCache_ServiceName::GetDefault());
    string nst_app_domain(TNetStorage_AppDomain::GetDefault());
    string init_string(
            "nst="     + nst_service +
            "&nc="     + nc_service +
            "&domain=" + nst_app_domain +
            "&client="   APP_NAME +
            mode);
    return CNetStorageByKey(init_string);
}


template <>
inline CCombinedNetStorage g_GetNetStorage<CCombinedNetStorage>(const char*)
{
    string nc_service(TNetCache_ServiceName::GetDefault());
    string nst_app_domain(TNetStorage_AppDomain::GetDefault());
    string ft_site(TFileTrack_Site::GetDefault());
    string ft_key(TFileTrack_Key::GetDefault());
    string init_string(
            "mode=direct&nc="     + nc_service +
            "&domain=" + nst_app_domain +
            "&client="   APP_NAME +
            "&ft_site=" + ft_site +
            "&ft_key=" + ft_key);
    return CCombinedNetStorage(init_string);
}

template <>
inline CCombinedNetStorageByKey g_GetNetStorage<CCombinedNetStorageByKey>(const char*)
{
    string nc_service(TNetCache_ServiceName::GetDefault());
    string nst_app_domain(TNetStorage_AppDomain::GetDefault());
    string ft_site(TFileTrack_Site::GetDefault());
    string ft_key(TFileTrack_Key::GetDefault());
    string init_string(
            "mode=direct&nc="     + nc_service +
            "&domain=" + nst_app_domain +
            "&client="   APP_NAME +
            "&ft_site=" + ft_site +
            "&ft_key=" + ft_key);
    return CCombinedNetStorageByKey(init_string);
}


// Just a convenience wrapper
template <
    class TNetStorage,
    class TExpected,
    class TApi,
    class TLoc>
struct SPolicy
{
    typedef TNetStorage NetStorage;
    typedef TExpected Expected;
    typedef TApi Api;
    typedef TLoc Loc;
};

// Test context
template <class TPolicy>
struct SFixture
{
    // Unwrapping the policy
    typedef typename TPolicy::NetStorage TNetStorage;
    typedef typename TPolicy::Expected TExpected;
    typedef typename TPolicy::Api TApi;
    typedef typename TPolicy::Loc TLoc;

    TNetStorage netstorage;
    auto_ptr<TExpected> data;

    typedef boost::integral_constant<bool, (TLoc::TAttrTesting::value &&
            TAttrTesting<TNetStorage>::value)> TAttrTesting;
    SAttrApi<TAttrTesting> attr_tester;

    SCtx ctx;

    SFixture()
        : netstorage(g_GetNetStorage<TNetStorage>(TLoc::init_string()))
    {
    }

    // These two are used to set test context
    SFixture& Ctx(const string& d)
    {
        LOG_POST(Trace << d);
        BOOST_TEST_CHECKPOINT(d);
        ctx.desc = d;
        return *this;
    }
    const SCtx& Line(long l) { ctx.line = l; return ctx; }

    template <class TLocation>
    void ReadAndCompare(const string&, CNetStorageObject object);

    template <class TLocation>
    void ReadTwoAndCompare(const string&,
            CNetStorageObject object1, CNetStorageObject object2);

    template <class TLocation>
    string WriteTwoAndRead(CNetStorageObject object1, CNetStorageObject object2);

    template <class TLocation, class TId>
    void ExistsAndRemoveTests(const TId& id);

    // The parameter is only used to select the implementation
    void Test(CNetStorage&);
    void Test(CNetStorageByKey&);
};

// NB: It does not matter whether TLocationNetCache or TLocationFileTrack is used here
typedef TLocationNetCache TShouldWork;

template <class TPolicy>
template <class TLocation>
void SFixture<TPolicy>::ReadAndCompare(const string& ctx, CNetStorageObject object)
{
    Ctx(ctx);
    SReadHelper<TApi, TShouldWork> expected_reader(*data);
    TApi api(object);
    SReadHelper<TApi, TLocation> actual_reader(api.GetReader());

    attr_tester.Read(TLocation(), Line(__LINE__), object);

    for (;;) {
        if (!expected_reader(Line(__LINE__))) break;
        if (!actual_reader(Line(__LINE__))) break;

        size_t length = min(expected_reader.length, actual_reader.length);

        if (memcmp(expected_reader.buf, actual_reader.buf, length)) {
            BOOST_ERROR_CTX("Read() returned corrupted data", Line(__LINE__));
            break;
        }

        expected_reader -= length;
        actual_reader -= length;

        if (expected_reader.Empty()) {
            if (actual_reader.Empty()) {
                break;
            }

            if (actual_reader.HasData()) {
                BOOST_ERROR_CTX("Got more data than expected", Line(__LINE__));
                break;
            }
        }

        if (actual_reader.Empty()) {
            if (expected_reader.HasData()) {
                BOOST_ERROR_CTX("Got less data than expected", Line(__LINE__));
                break;
            }
        }
    }

    expected_reader.Close(Line(__LINE__));
    actual_reader.Close(Line(__LINE__));

    attr_tester.Read(TLocation(), Line(__LINE__), object);

    CGetInfo<TLocation>(Line(__LINE__), object, data->Size()).Check(TLoc::loc_info);
}

template <class TPolicy>
template <class TLocation>
void SFixture<TPolicy>::ReadTwoAndCompare(const string& ctx,
        CNetStorageObject object1, CNetStorageObject object2)
{
    Ctx(ctx);
    SReadHelper<TApi, TShouldWork> expected_reader(*data);
    TApi api1(object1);
    SReadHelper<TApi, TLocation> actual_reader1(api1.GetReader());
    TApi api2(object2);
    SReadHelper<TApi, TLocation> actual_reader2(api2.GetReader());

    attr_tester.Read(TLocation(), Line(__LINE__), object1);
    attr_tester.Read(TLocation(), Line(__LINE__), object2);

    for (;;) {
        if (!expected_reader(Line(__LINE__))) break;
        if (!actual_reader1(Line(__LINE__))) break;
        if (!actual_reader2(Line(__LINE__))) break;

        size_t length = min(expected_reader.length,
                min(actual_reader1.length, actual_reader2.length));

        if (memcmp(expected_reader.buf, actual_reader1.buf, length)) {
            BOOST_ERROR_CTX("Read() returned corrupted data", Line(__LINE__));
            break;
        }

        if (memcmp(expected_reader.buf, actual_reader2.buf, length)) {
            BOOST_ERROR_CTX("Read() returned corrupted data", Line(__LINE__));
            break;
        }

        expected_reader -= length;
        actual_reader1 -= length;
        actual_reader2 -= length;

        if (expected_reader.Empty()) {
            if (actual_reader1.Empty() && actual_reader2.Empty()) {
                break;
            }

            if (actual_reader1.HasData()) {
                BOOST_ERROR_CTX("Got more data than expected", Line(__LINE__));
                break;
            }

            if (actual_reader2.HasData()) {
                BOOST_ERROR_CTX("Got more data than expected", Line(__LINE__));
                break;
            }
        }

        if (actual_reader1.Empty() || actual_reader2.Empty()) {
            if (expected_reader.HasData()) {
                BOOST_ERROR_CTX("Got less data than expected", Line(__LINE__));
                break;
            }
        }
    }

    expected_reader.Close(Line(__LINE__));
    actual_reader1.Close(Line(__LINE__));
    actual_reader2.Close(Line(__LINE__));

    attr_tester.Read(TLocation(), Line(__LINE__), object1);
    attr_tester.Read(TLocation(), Line(__LINE__), object2);

    CGetInfo<TLocation>(Line(__LINE__), object1, data->Size()).Check(TLoc::loc_info);
    CGetInfo<TLocation>(Line(__LINE__), object2, data->Size()).Check(TLoc::loc_info);
}

template <class TPolicy>
template <class TLocation>
string SFixture<TPolicy>::WriteTwoAndRead(CNetStorageObject object1,
        CNetStorageObject object2)
{
    Ctx("Creating two objects").Line(__LINE__);
    SReadHelper<TApi, TShouldWork> source_reader(*data);
    TApi api1(object1);
    SWriteHelper dest_writer1(api1.GetWriter());
    TApi api2(object2);
    SWriteHelper dest_writer2(api2.GetWriter());

    while (source_reader(Line(__LINE__)) &&
            dest_writer1(source_reader, Line(__LINE__)) &&
            dest_writer2(source_reader, Line(__LINE__)) &&
            !source_reader.Empty()) {
        source_reader.length = 0;
    }

    attr_tester.Write(TShouldThrow(), Line(__LINE__), object1);
    attr_tester.Write(TShouldThrow(), Line(__LINE__), object2);

    source_reader.Close(Line(__LINE__));
    dest_writer1.Close(Line(__LINE__));
    dest_writer2.Close(Line(__LINE__));

    attr_tester.Write(TLocation(), Line(__LINE__), object1);
    attr_tester.Write(TLocation(), Line(__LINE__), object2);

    ReadTwoAndCompare<TLocation>("Reading after writing", object1, object2);

    return object1.GetLoc();
}


// Generic API is used by default by Relocate, Exists and Remove functions

template <class TNetStorage>
inline string Relocate(TNetStorage& netstorage, const string& object_loc,
        TNetStorageFlags flags)
{
    return netstorage.Relocate(object_loc, flags);
}

template <class TNetStorage>
inline bool Exists(TNetStorage& netstorage, const string& object_loc)
{
    return netstorage.Exists(object_loc);
}

template <class TNetStorage>
inline CNetStorageObject Open(TNetStorage& netstorage, const string& object_loc)
{
    return netstorage.Open(object_loc);
}

template <class TNetStorage>
inline void Remove(TNetStorage& netstorage, const string& object_loc)
{
    netstorage.Remove(object_loc);
}

template <class TNetStorageByKey>
inline string Relocate(TNetStorageByKey& netstorage, const string& unique_key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    return netstorage.Relocate(unique_key, flags, old_flags);
}

typedef pair<string, TNetStorageFlags> TKey;

template <class TNetStorageByKey>
inline bool Exists(TNetStorageByKey& netstorage, const TKey& key)
{
    return netstorage.Exists(key.first, key.second);
}

template <class TNetStorageByKey>
inline CNetStorageObject Open(TNetStorageByKey& netstorage, const TKey& key)
{
    return netstorage.Open(key.first, key.second);
}

template <class TNetStorageByKey>
inline void Remove(TNetStorageByKey& netstorage, const TKey& key)
{
    netstorage.Remove(key.first, key.second);
}


// Tests Exists() and Remove() methods
template <class TPolicy>
template <class TLocation, class TId>
void SFixture<TPolicy>::ExistsAndRemoveTests(const TId& id)
{
    Ctx("Checking existent object").Line(__LINE__);
    BOOST_CHECK_CTX(Exists(netstorage, id), ctx);
    CNetStorageObject object(Open(netstorage, id));
    CGetInfo<TLocation>(Ctx("Reading existent object info").Line(__LINE__),
            object, data->Size()).Check(TLoc::loc_info);
    LOG_POST(Trace << "Removing existent object");
    Remove(netstorage, id);

    Ctx("Checking removed object").Line(__LINE__);
    BOOST_CHECK_CTX(!Exists(netstorage, id), ctx);

    ReadAndCompare<TLocationNotFound>("Trying to read removed object",
        Open(netstorage, id));
}

template <class TPolicy>
void SFixture<TPolicy>::Test(CNetStorage&)
{
    data.reset(Create<TExpected>(netstorage, TLoc::source));

    ReadAndCompare<TLocationNotFound>("Trying to read non-existent object",
        netstorage.Open(TLoc::not_found()));

    CNetStorageObject second(netstorage.Create(TLoc::create));
    CNstObjectGuard<CNetStorage> guard(netstorage, second.GetLoc());

    // Write and read test data.
    string object_loc = WriteTwoAndRead<typename TLoc::TCreate>(
            netstorage.Create(TLoc::create), second);

    if (TLoc::check_relocate) {
        // Generate a "non-movable" object ID by calling Relocate()
        // with the same storage preferences (so the object should not
        // be actually relocated).
        Ctx("Creating immovable locator");
        string immovable_loc = Relocate(netstorage, object_loc, TLoc::immovable);

        // Relocate the object to a different storage.
        Ctx("Relocating object");
        string relocated_loc = Relocate(netstorage, object_loc, TLoc::relocate);

        // Verify that the object has disappeared from the original storage.
        ReadAndCompare<TLocationRelocated>("Trying to read relocated object",
            netstorage.Open(immovable_loc));

        // However, the object must still be accessible
        // either using the original ID:
        ReadAndCompare<typename TLoc::TRelocate>("Reading using original ID",
                netstorage.Open(object_loc));
        // or using the newly generated storage ID:
        ReadAndCompare<typename TLoc::TRelocate>("Reading using newly generated ID",
                netstorage.Open(relocated_loc));
    }

    ExistsAndRemoveTests<typename TLoc::TRelocate>(object_loc);
}

template <class TPolicy>
void SFixture<TPolicy>::Test(CNetStorageByKey&)
{
    string unique_key2 = GetUniqueKey();
    string unique_key3 = GetUniqueKey();
    CNstObjectGuard<CNetStorageByKey> guard(netstorage, unique_key3);

    data.reset(Create<TExpected>(netstorage, TLoc::source));

    ReadAndCompare<TLocationNotFound>("Trying to read non-existent object",
        netstorage.Open(unique_key2, TLoc::non_existent));

    // Write and read test data using a user-defined key.
    WriteTwoAndRead<typename TLoc::TCreate>(
            netstorage.Open(unique_key2, TLoc::create),
            netstorage.Open(unique_key3, TLoc::create));

    // Make sure the object is readable with a different combination of flags.
    ReadAndCompare<typename TLoc::TCreate>("Reading using different set of flags",
            netstorage.Open(unique_key2, TLoc::readable));

    // Relocate the object to a different storage and make sure
    // it can be read from there.
    Ctx("Relocating object");
    Relocate(netstorage, unique_key2, TLoc::relocate, TLoc::create);

    // Verify that the object has disappeared from the original storage.
    ReadAndCompare<TLocationRelocated>("Trying to read relocated object",
            netstorage.Open(unique_key2, TLoc::immovable));

    ReadAndCompare<typename TLoc::TRelocate>("Reading using original flags",
            netstorage.Open(unique_key2, TLoc::create));

    ReadAndCompare<typename TLoc::TRelocate>("Reading using new flags",
            netstorage.Open(unique_key2, TLoc::relocate));

    const int create = TLoc::create;
    ExistsAndRemoveTests<typename TLoc::TRelocate>(TKey(unique_key2, create));
}

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
            "CNetStorage Unit Test");
}

#define TEST_CASE(ST, SRC, API, LOC) \
typedef SPolicy<C##ST, C##SRC##Data, CApi<S##API##ApiImpl>, S##LOC> \
    T##ST##SRC##API##LOC##Policy; \
BOOST_FIXTURE_TEST_CASE(Test##ST##SRC##API##LOC, \
        SFixture<T##ST##SRC##API##LOC##Policy>) \
{ \
    Test(netstorage); \
}

#define DEFINE_TEST_CASE(r, p) TEST_CASE p

#define ST_LIST1    (NetStorageByKey, (CombinedNetStorage, (CombinedNetStorageByKey, BOOST_PP_NIL)))
#define ST_LIST2    (NetStorage, BOOST_PP_NIL)
#define SRC_LIST    (Emp, (Str, (Rnd, (Nst, BOOST_PP_NIL))))
#define API_LIST    (Str, (Buf, (Irw, (Ios, BOOST_PP_NIL))))
#define LOC_LIST1   (NC2FT, (FT2NC, BOOST_PP_NIL))
#define LOC_LIST2   (DirectNC, LOC_LIST1)

BOOST_PP_LIST_FOR_EACH_PRODUCT(DEFINE_TEST_CASE, 4, \
        (ST_LIST2, SRC_LIST, API_LIST, LOC_LIST2))

BOOST_PP_LIST_FOR_EACH_PRODUCT(DEFINE_TEST_CASE, 4, \
        (ST_LIST1, SRC_LIST, API_LIST, LOC_LIST1))
