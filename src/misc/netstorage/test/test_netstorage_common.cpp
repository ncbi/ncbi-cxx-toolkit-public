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

#include <connect/services/netstorage.hpp>

#include <corelib/test_boost.hpp>

#include <util/random_gen.hpp>

#include <boost/preprocessor.hpp>

#include "test_netstorage_common.hpp"

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


#define TEST_ATTRIBUTES

// These tests are not working well enough at the moment
#undef TEST_NON_EXISTENT
#undef TEST_RELOCATED
#undef TEST_REMOVED


// ENetStorageObjectLocation to type mapping
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

    void Check()
    {
        CheckGetLocation();
        CheckGetObjectLocInfo();
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

    virtual void CheckGetObjectLocInfo()
    {
        if (CJsonNode node = GetObjectLocInfo()) {
            m_ObjectLocInfoRepr = node.Repr();
            BOOST_CHECK_CTX(m_ObjectLocInfoRepr.size(), m_Ctx);
        } else {
            BOOST_ERROR_CTX("!GetObjectLocInfo()", m_Ctx);
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


// Convenience function to fill container with random char data
template <class TContainer>
void RandomFill(CRandom& random, TContainer& container, size_t length)
{
    const char kMin = ' ';
    const char kMax = '~';
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

// An extended IExtReader interface for expected data
struct IExpected : public IExtReader
{
    virtual size_t Size() = 0;
};

// A reader that is used to read from a string
template <class TReader>
class CStrReader : public TReader
{
public:
    typedef CStrReader TStrReader; // Self

    CStrReader() : m_Ptr(NULL) {}
    CStrReader(const string& str) : m_Str(str), m_Ptr(NULL) {}

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0)
    {
        if (!Init()) return eRW_Error;
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
    bool Init()
    {
        if (m_Ptr) return true;

        try {
            DoInit();
            m_Ptr = m_Str.data();
            m_Remaining = m_Str.size();
            return true;
        }
        catch (...) {
            return false;
        }
    }

    virtual void DoInit() {};
};


// String data to test APIs
class CStrData : public CStrReader<IExpected>
{
public:
    CStrData(CNetStorageObject)
        : TStrReader("The quick brown fox jumps over the lazy dog")
    {}

    size_t Size() { return m_Str.size(); }
};

// Large volume data to test APIs
class CBigData : public IExpected
{
public:
    struct SSource
    {
        vector<char> data;
        CRandom random;

        SSource()
        {
            random.Randomize();
            RandomFill(random, data, 20 * 1024 * 1024); // 20MB
        }
    };

    CBigData(CNetStorageObject)
        : m_Begin(0),
          m_Length(m_Source.data.size())
    {}

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0)
    {
        if (!m_Length) return eRW_Eof;

        size_t read = m_Length < count ? m_Length : count;
        if (read > 2) read = m_Source.random.GetRand(2 * read / 3, read);
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
class CNstData : public IExpected
{
public:
    CNstData(CNetStorageObject object)
        : m_Object(object)
    {
        // Create a NetStorage object to use as a source
        CBigData::SSource source;
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
    class CReader : public CNetStorageRW<CStrReader<IExtReader> >
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
        void Close() { Flush(); m_Object.Close(); }

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
            try {
                if (m_Object.Eof()) return eRW_Eof;
                size_t read = m_Object.Read(buf, count);
                return Finalize(read, bytes_read, eRW_Success);
            }
            catch (...) {
                return eRW_Error;
            }
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
struct SReadHelper
{
    IExtReader& source;
    char buf[65537]; // Prime
    size_t length;

    SReadHelper(IExtReader& s)
        : source(s),
          length(0),
          m_Result(eRW_Success)
    {}

    void Close()
    {
        source.Close();
    }

    bool operator()(const SCtx& ctx)
    {
        if (length) return true;
        m_Result = source.Read(buf, sizeof(buf), &length);

        switch (m_Result) {
        case eRW_Success:
            BOOST_REQUIRE_CTX(length, ctx);
            /* FALL THROUGH */
        case eRW_Eof:
            return true;
        default:
            BOOST_ERROR_CTX("Failed to read: " << g_RW_ResultToString(m_Result), ctx);
            return false;
        }
    }

    void operator-=(size_t l)
    {
        length -= l;
        memmove(buf, buf + l, length);
    }

    bool Eof() { return m_Result == eRW_Eof && !length; }

private:
    ERW_Result m_Result;
};

// A helper to read from expected and write to object
struct SWriteHelper
{
    SWriteHelper(IExtWriter& dest)
        : m_Dest(dest),
          m_Result(eRW_Success)
    {}

    void Close(const SCtx& ctx)
    {
        BOOST_CHECK_CTX(m_Dest.Flush() == eRW_Success, ctx);
        m_Dest.Close();
    }

    template <class TReader>
    bool operator()(TReader& reader, const SCtx& ctx)
    {
        while (reader.length) {
            size_t length = 0;
            m_Result = m_Dest.Write(reader.buf, reader.length, &length);

            if (m_Result == eRW_Success) {
                reader.length -= length;
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
};


// Random attributes set
struct SAttrApiBase
{
    typedef pair<string, string> TStrPair;
    typedef vector<TStrPair> TData;
    TData data;

    SAttrApiBase()
    {
        CRandom r;
        r.Randomize();

        // Attribute name/value length range
        const size_t kMinLength = 5;
        const size_t kMaxLength = 1000;

        // Number of attributes
        const size_t kMinSize = 3;
        const size_t kMaxSize = 100;
        size_t size = r.GetRand(kMinSize, kMaxSize);

        // The probability of an attribute to be set:
        // (kSetProbability - 1) / kSetProbability
        const size_t kSetProbability = 10;

        string name;
        string value;
        string not_set;

        name.reserve(kMaxLength);
        value.reserve(kMaxLength);
        data.reserve(size);

        while (size-- > 0) {
            RandomFill(r, name, r.GetRand(kMinLength, kMaxLength));

            if (r.GetRandIndex(kSetProbability)) {
                RandomFill(r, value, r.GetRand(kMinLength, kMaxLength));
                data.push_back(TStrPair(name, value));
            } else {
                data.push_back(TStrPair(name, not_set));
            }
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
                BOOST_CHECK_CTX(object.GetAttribute(i->first) == i->second, ctx);
            }
        }
        catch (...) {
            // Restore test context
            BOOST_TEST_CHECKPOINT(ctx.desc);
            throw;
        }
    }

    void Write(const SCtx& ctx, CNetStorageObject& object)
    {
        Shuffle();

        try {
            NON_CONST_ITERATE(TData, i, data) {
                object.SetAttribute(i->first, i->first);
                BOOST_CHECK_CTX(object.GetAttribute(i->first) == i->first, ctx);
                object.SetAttribute(i->first, i->second);
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

// Default implementation (attribute reading/writing always throws)
template <class TAttrTesting>
struct SAttrApi : SAttrApiBase
{
    template <class TLocation>
    void Read(TLocation, const SCtx& ctx, CNetStorageObject& object)
    {
#ifdef TEST_ATTRIBUTES
        BOOST_CHECK_THROW_CTX(SAttrApiBase::Read(ctx, object),
                CNetStorageException, ctx);
#endif
    }

    template <class TLocation>
    void Write(TLocation, const SCtx& ctx, CNetStorageObject& object)
    {
#ifdef TEST_ATTRIBUTES
        BOOST_CHECK_THROW_CTX(SAttrApiBase::Write(ctx, object),
                CNetStorageException, ctx);
#endif
    }
};

typedef boost::integral_constant<bool, true> TAttrTestingEnabled;

// Attribute testing is enabled
#ifdef TEST_ATTRIBUTES
template <>
struct SAttrApi<TAttrTestingEnabled> : SAttrApiBase
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
#endif


// Just a convenience wrapper
template <class TNetStorage, class TExpected, class TApi, class TAttrTesting>
struct SPolicy
{
    typedef TNetStorage NetStorage;
    typedef TExpected Expected;
    typedef TApi Api;
    typedef TAttrTesting AttrTesting;
};

// Test context
template <class TPolicy>
struct SFixture
{
    // Unwrapping the policy
    typedef typename TPolicy::NetStorage TNetStorage;
    typedef typename TPolicy::Expected TExpected;
    typedef typename TPolicy::Api TApi;
    typedef typename TPolicy::AttrTesting TAttrTesting;

    TNetStorage netstorage;
    auto_ptr<TExpected> data;
    SAttrApi<TAttrTesting> attr_tester;
    SCtx ctx;

    SFixture()
        : netstorage(g_GetNetStorage<TNetStorage>())
    {
    }

    // These two are used to set test context
    SFixture& Ctx(const string& d)
    {
        BOOST_TEST_CHECKPOINT(d);
        ctx.desc = d;
        return *this;
    }
    const SCtx& Line(long l) { ctx.line = l; return ctx; }

    template <class TLocation>
    void ReadAndCompare(const string&, CNetStorageObject object);

    string WriteAndRead(CNetStorageObject object);
    void ExistsAndRemoveTests(const string& id);

    // The parameter is only used to select the implementation
    void Test(CNetStorage&);
    void Test(CNetStorageByKey&);
};


template <class TPolicy>
template <class TLocation>
void SFixture<TPolicy>::ReadAndCompare(const string& ctx, CNetStorageObject object)
{
    Ctx(ctx);
    SReadHelper expected_reader(*data);
    TApi api(object);
    SReadHelper actual_reader(api.GetReader());

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

        if (expected_reader.Eof()) {
            if (!actual_reader.Eof()) {
                BOOST_ERROR_CTX("Got more data than expected", Line(__LINE__));
            }
            break;
        } else {
            if (actual_reader.Eof()) {
                BOOST_ERROR_CTX("Got less data than expected", Line(__LINE__));
                break;
            }
        }
    }

    expected_reader.Close();
    actual_reader.Close();

    attr_tester.Read(TLocation(), Line(__LINE__), object);

    CGetInfo<TLocation>(Line(__LINE__), object, data->Size()).Check();
}

template <class TPolicy>
string SFixture<TPolicy>::WriteAndRead(CNetStorageObject object)
{
    SReadHelper source_reader(*data);
    TApi api(object);
    SWriteHelper dest_writer(api.GetWriter());

    while (source_reader(Line(__LINE__)) &&
            dest_writer(source_reader, Line(__LINE__)) &&
            !source_reader.Eof());

    attr_tester.Write(TShouldThrow(), Line(__LINE__), object);

    source_reader.Close();
    dest_writer.Close(Line(__LINE__));

    attr_tester.Write(TLocationNetCache(), Line(__LINE__), object);

    // Sometimes this object is not immediately available for reading
    sleep(1);

    ReadAndCompare<TLocationNetCache>("Reading NetCache after writing", object);

    return object.GetLoc();
}

// TODO: Add tests that read from/write into two objects simultaneously

// Tests Exists() and Remove() methods
template <class TPolicy>
void SFixture<TPolicy>::ExistsAndRemoveTests(const string& id)
{
    BOOST_CHECK_CTX(netstorage.Exists(id),
            Ctx("Checking existent object").Line(__LINE__));
    CNetStorageObject object(netstorage.Open(id));
    CGetInfo<TLocationFileTrack>(Ctx("Reading existent object info").Line(__LINE__),
            object, data->Size()).Check();
    netstorage.Remove(id);
    BOOST_CHECK_CTX(!netstorage.Exists(id),
            Ctx("Checking non-existent object").Line(__LINE__));

#ifdef TEST_REMOVED
    CNetStorageObject no_object(netstorage.Open(id));
    BOOST_CHECK_THROW_CTX(
            ReadAndCompare<TLocationNotFound>("Trying to read removed object",
                no_object), CNetStorageException, ctx);
#endif
}

template <class TPolicy>
void SFixture<TPolicy>::Test(CNetStorage&)
{
    data.reset(new TExpected(netstorage.Create(fNST_Persistent)));

#ifdef TEST_NON_EXISTENT
    string not_found(
            "VHwxYl4jgV6Y8pM4TzhGbhfoROgxMl10Rz9GNlwqch9HHwkoJw0WbEsxAGoT0Cb0EeOG");
    CGetInfo<TLocationNotFound>(
            Ctx("Trying to read non-existent object info").Line(__LINE__),
            netstorage.Open(not_found)).Check();

    BOOST_CHECK_THROW_CTX(
            ReadAndCompare<TLocationNotFound>("Trying to read non-existent object",
                netstorage.Open(not_found)),
            CNetStorageException, Line(__LINE__));
#endif

    // Create a NetStorage object that should to go to NetCache.
    string object_loc = WriteAndRead(netstorage.Create(fNST_Fast | fNST_Movable));

    // Generate a "non-movable" object ID by calling Relocate()
    // with the same storage preferences (so the object should not
    // be actually relocated).
    string fast_storage_object_loc = netstorage.Relocate(object_loc, fNST_Fast);

    // Relocate the object to a persistent storage.
    string persistent_loc = netstorage.Relocate(object_loc, fNST_Persistent);

#ifdef TEST_RELOCATED
    // Verify that the object has disappeared from the "fast" storage.
    CNetStorageObject fast_storage_object =
            netstorage.Open(fast_storage_object_loc);
    CGetInfo<TLocationNotFound>(
            Ctx("Trying to read relocated object info").Line(__LINE__),
            fast_storage_object).Check();

    // Make sure the relocated object does not exists in the
    // original storage anymore.
    BOOST_CHECK_THROW_CTX(
            ReadAndCompare<TLocationNotFound>("Trying to read relocated object",
                fast_storage_object), CNetStorageException, ctx);
#endif

    // However, the object must still be accessible
    // either using the original ID:
    ReadAndCompare<TLocationFileTrack>("Reading using original ID",
            netstorage.Open(object_loc));
    // or using the newly generated persistent storage ID:
    ReadAndCompare<TLocationFileTrack>("Reading using newly generated ID",
            netstorage.Open(persistent_loc));

    ExistsAndRemoveTests(object_loc);
}

template <class TPolicy>
void SFixture<TPolicy>::Test(CNetStorageByKey&)
{
    CRandom random_gen;

    random_gen.Randomize();

    string unique_key = NStr::NumericToString(
            random_gen.GetRand() * random_gen.GetRand());
    data.reset(new TExpected(netstorage.Open(unique_key, fNST_Persistent)));

    unique_key = NStr::NumericToString(
            random_gen.GetRand() * random_gen.GetRand());

#ifdef TEST_NON_EXISTENT
    CGetInfo<TLocationNotFound>(
            Ctx("Trying to read non-existent object info").Line(__LINE__),
            netstorage.Open(unique_key,
            fNST_Fast | fNST_Movable)).Check();

    BOOST_CHECK_THROW_CTX(
            ReadAndCompare<TLocationNotFound>("Trying to read non-existent object",
                netstorage.Open(unique_key, fNST_Fast | fNST_Movable)),
            CNetStorageException, Line(__LINE__));
#endif

    // Write and read test data using a user-defined key.
    WriteAndRead(netstorage.Open(unique_key,
            fNST_Fast | fNST_Movable));

    // Make sure the object is readable with a different combination of flags.
    ReadAndCompare<TLocationNetCache>("Reading using different set of flags",
            netstorage.Open(unique_key, fNST_Persistent | fNST_Movable));

    // Relocate the object to FileTrack and make sure
    // it can be read from there.
    netstorage.Relocate(unique_key, fNST_Persistent, fNST_Movable);

    ReadAndCompare<TLocationFileTrack>("Reading relocated object",
            netstorage.Open(unique_key, fNST_Persistent));

    ExistsAndRemoveTests(unique_key);
}

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
            "CNetStorage Unit Test");
}

#define ST_LIST     (NetStorage, (NetStorageByKey, BOOST_PP_NIL))
#define SRC_LIST    (Str, (Big, (Nst, BOOST_PP_NIL)))
#define API_LIST    (Str, (Buf, (Irw, (Ios, BOOST_PP_NIL))))

#define TEST_CASE(ST, SRC, API) \
typedef SPolicy<C##ST, C##SRC##Data, CApi<S##API##ApiImpl>, TAttrTesting> \
    T##ST##SRC##API##Policy; \
BOOST_FIXTURE_TEST_CASE(Test##ST##SRC##API, SFixture<T##ST##SRC##API##Policy>) \
{ \
    Test(netstorage); \
}

#define DEFINE_TEST_CASE(r, p) TEST_CASE p

BOOST_PP_LIST_FOR_EACH_PRODUCT(DEFINE_TEST_CASE, 3, (ST_LIST, SRC_LIST, API_LIST));
