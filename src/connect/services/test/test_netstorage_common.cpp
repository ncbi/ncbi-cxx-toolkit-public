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

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


// Hierarchy for GetInfo() tests

class IGetInfo : protected CNetStorageObjectInfo
{
public:
    IGetInfo(CNetStorageObject object)
        : CNetStorageObjectInfo(object.GetInfo())
    {}

    virtual ~IGetInfo() {}

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

private:
    virtual void CheckGetLocation() = 0;
    virtual void CheckGetObjectLocInfo() = 0;
    virtual void CheckGetCreationTime() = 0;
    virtual void CheckGetSize() = 0;
    virtual void CheckGetStorageSpecificInfo() = 0;
    virtual void CheckGetNFSPathname() = 0;
    virtual void CheckToJSON() = 0;
};

class CGetInfoBase : public IGetInfo
{
public:
    CGetInfoBase(CNetStorageObject object, int location, Uint8 size)
        : IGetInfo(object), m_Location(location), m_Size(size)
    {
    }

private:
    void CheckGetLocation()
    {
        BOOST_CHECK_EQUAL(GetLocation(), m_Location);
    }

    void CheckGetObjectLocInfo()
    {
        if (CJsonNode node = GetObjectLocInfo()) {
            m_ObjectLocInfoRepr = node.Repr();
            BOOST_CHECK(m_ObjectLocInfoRepr.size());
        } else {
            BOOST_ERROR("!GetObjectLocInfo()");
        }
    }

    void CheckGetCreationTime()
    {
        BOOST_CHECK(GetCreationTime() != CTime());
    }

    void CheckGetSize()
    {
        BOOST_CHECK_EQUAL(GetSize(), m_Size);
    }

    void CheckGetStorageSpecificInfo()
    {
        if (CJsonNode node = GetStorageSpecificInfo()) {
            BOOST_CHECK(node.Repr().size());
        } else {
            BOOST_FAIL("!GetStorageSpecificInfo()");
        }
    }

    void CheckGetNFSPathname()
    {
        BOOST_CHECK_THROW(GetNFSPathname(), CNetStorageException);
    }

    void CheckToJSON()
    {
        if (CJsonNode node = ToJSON()) {
            if (m_ObjectLocInfoRepr.size()) {
                string repr = node.Repr();
                BOOST_CHECK(NPOS != NStr::Find(repr, m_ObjectLocInfoRepr));
            }
        } else {
            BOOST_ERROR("!ToJSON()");
        }
    }

    int m_Location;
    Uint8 m_Size;
    string m_ObjectLocInfoRepr;
};

class CGetInfoNotFound : public CGetInfoBase
{
public:
    CGetInfoNotFound(CNetStorageObject object, Uint8 = 0U)
        : CGetInfoBase(object, eNFL_NotFound, 0U)
    {}

private:
    void CheckGetCreationTime()
    {
        BOOST_CHECK(GetCreationTime() == CTime());
    }

    void CheckGetStorageSpecificInfo()
    {
        BOOST_CHECK(!GetStorageSpecificInfo());
    }
};

class CGetInfoNetCache : public CGetInfoBase
{
public:
    CGetInfoNetCache(CNetStorageObject object, Uint8 size)
        : CGetInfoBase(object, eNFL_NetCache, size)
    {
        BOOST_CHECK(object.GetSize() == size);
    }
};

class CGetInfoFileTrack : public CGetInfoBase
{
public:
    CGetInfoFileTrack(CNetStorageObject object, Uint8 size)
        : CGetInfoBase(object, eNFL_FileTrack, size)
    {
        BOOST_CHECK(object.GetSize() == size);
    }

private:
    void CheckGetNFSPathname()
    {
        BOOST_CHECK(GetNFSPathname().size());
    }
};


// An extended IReader interface
class IExtReader : public IReader
{
public:
    virtual void Open() {}
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
    virtual void Open() {}
    virtual void Close() = 0;
};

// An extended IExtReader interface for expected data
struct IExpected : public IExtReader
{
    virtual size_t Size() const = 0;
};

// A reader that is used to read from a string
template <class TReader>
class CStrReader : public TReader
{
public:
    typedef CStrReader<TReader> TStrReader; // Self

    CStrReader() {}
    CStrReader(const string& str) : m_Str(str) {}

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0)
    {
        size_t to_read = 0;

        if (buf) {
            to_read = count < m_Remaining ? count : m_Remaining;
            memcpy(buf, m_Ptr, to_read);
            m_Ptr += to_read;
            m_Remaining -= to_read;
        }

        if (bytes_read) *bytes_read = to_read;
        return m_Remaining ? eRW_Success : eRW_Eof;
    }

    void Open()
    {
        m_Ptr = m_Str.data();
        m_Remaining = m_Str.size();
    }

protected:
    string m_Str;
    const char* m_Ptr;
    size_t m_Remaining;
};


// String data to test APIs
class CStrData : public CStrReader<IExpected>
{
public:
    CStrData()
        : TStrReader("The quick brown fox jumps over the lazy dog")
    {}

    void Close()
    {
        if (m_Remaining) {
            BOOST_ERROR("Got less data than expected");
        }
    }

    size_t Size() const { return m_Str.size(); }
};

// TODO:
// Implement CBigData (derived from IExtReader)
// to test API of big data volumes


// NetStorage reader/writer base class
template <class TBase>
class CNetStorageRW : public TBase
{
public:
    typedef CNetStorageRW<TBase> TNetStorageRW; // Self

    CNetStorageRW(CNetStorageObject o) : m_Object(o) {}
    void Close() { m_Object.Close(); }

protected:
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
        void Open() { m_Object.Read(&m_Str); TStrReader::Open(); }
    };

    class CWriter : public CNetStorageRW<IExtWriter>
    {
    public:
        CWriter(CNetStorageObject o) : TNetStorageRW(o) {}
        ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0)
        {
            if (buf) {
                m_Output.append(static_cast<const char*>(buf), count);
            }

            if (bytes_written) {
                *bytes_written = count;
            }
            return eRW_Success;
        }
        ERW_Result Flush(void)
        {
            m_Object.Write(m_Output);
            m_Output.clear();
            return eRW_Success;
        }
        void Open() { m_Output.clear(); }
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
            size_t read = m_Object.Read(buf, count);
            if (bytes_read) *bytes_read = read;
            return eRW_Success;
        }
    };

    class CWriter : public CNetStorageRW<IExtWriter>
    {
    public:
        CWriter(CNetStorageObject o) : TNetStorageRW(o) {}
        ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0)
        {
            m_Object.Write(buf, count);
            if (bytes_written) *bytes_written = count;
            return eRW_Success;
        }
        ERW_Result Flush(void) { return eRW_Success; }
    };
};


// TODO:
// Implement TApi classes to test following CNetStorage methods:
// 1) GetReader()/GetWriter()
// 2) GetRWStream();


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
    {
        source.Open();
    }

    void Close()
    {
        source.Close();
    }

    bool operator()()
    {
        if (length) return true;
        m_Result = source.Read(buf, sizeof(buf), &length);

        switch (m_Result) {
        case eRW_Success:
        case eRW_Eof:
            return true;
        default:
            BOOST_ERROR("Failed to read: " << g_RW_ResultToString(m_Result));
            return false;
        }
    }

    operator bool() { return m_Result == eRW_Success; }

private:
    ERW_Result m_Result;
};

// A helper to read from expected and write to object
struct SWriteHelper
{
    SWriteHelper(IExtWriter& dest)
        : m_Dest(dest),
          m_Result(eRW_Success)
    {
        m_Dest.Open();
    }
    void Close()
    {
        BOOST_CHECK(m_Dest.Flush() == eRW_Success);
        m_Dest.Close();
    }

    template <class TReader>
    bool operator()(TReader& reader)
    {
        while (reader.length) {
            size_t length = 0;
            m_Result = m_Dest.Write(reader.buf, reader.length, &length);

            switch (m_Result) {
            case eRW_Success:
                reader.length -= length;
                break;
            case eRW_Eof:
                if (reader.length -= length) {
                    BOOST_ERROR("Wrote less data than expected");
                    return false;
                }
                break;
            default:
                BOOST_ERROR("Failed to write to object: " <<
                        g_RW_ResultToString(m_Result));
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


template <class TGetInfo, class TApi>
static void s_ReadAndCompare(IExpected& expected, CNetStorageObject object)
{
    SReadHelper expected_reader(expected);
    TApi api(object);
    SReadHelper actual_reader(api.GetReader());

    do {
        if (!expected_reader()) break;
        if (!actual_reader()) break;

        size_t length = min(expected_reader.length, actual_reader.length);

        if (memcmp(expected_reader.buf, actual_reader.buf, length)) {
            BOOST_ERROR("Read() returned corrupted data");
            break;
        }

        expected_reader.length -= length;
        actual_reader.length -= length;
    } while (expected_reader && actual_reader);

    expected_reader.Close();
    actual_reader.Close();

    TGetInfo(object, expected.Size()).Check();
}

// This to help preprocessor
template <class TApi>
static void s_ReadAndCompareNotFound(IExpected& expected,
        CNetStorageObject object)
{
    s_ReadAndCompare<CGetInfoNotFound, TApi>(expected, object);
}

template <class TGetInfo, class TApi>
static string s_WriteAndRead(IExpected& source, CNetStorageObject object)
{
    SReadHelper source_reader(source);
    TApi api(object);
    SWriteHelper dest_writer(api.GetWriter());

    do {
        if (!source_reader()) break;
        if (!dest_writer(source_reader)) break;
    } while (source_reader && dest_writer);

    source_reader.Close();
    dest_writer.Close();

    s_ReadAndCompare<TGetInfo, TApi>(source, object);

    return object.GetLoc();
}

// Tests Exists() and Remove() methods
template <class TNetStorage, class TApi>
static void s_ExistsAndRemoveTests(const string& id, TNetStorage& netstorage,
        IExpected& expected)
{
    BOOST_CHECK(netstorage.Exists(id));
    CNetStorageObject object(netstorage.Open(id));
    CGetInfoFileTrack(object, expected.Size()).Check();
    netstorage.Remove(id);
    BOOST_CHECK(!netstorage.Exists(id));
    CNetStorageObject no_object(netstorage.Open(id));
    BOOST_CHECK_THROW(s_ReadAndCompareNotFound<TApi>(expected, no_object),
            CNetStorageException);
}

template <class TExpected, class TApi>
void s_TestNetStorage(CNetStorage netstorage)
{
    TExpected data;
    string not_found(
            "VHwxYl4jgV6Y8pM4TzhGbhfoROgxMl10Rz9GNlwqch9HHwkoJw0WbEsxAGoT0Cb0EeOG");
    CGetInfoNotFound(netstorage.Open(not_found)).Check();

    // Create a NetStorage object that should to go to NetCache.
    string object_loc = s_WriteAndRead<CGetInfoNetCache, TApi>(data,
            netstorage.Create(fNST_Fast | fNST_Movable));

    // Now read the whole object using the buffered version
    // of Read(). Verify that the contents of each buffer
    // match the original data.
    s_ReadAndCompare<CGetInfoNetCache, TApi>(data, netstorage.Open(object_loc));

    // Generate a "non-movable" object ID by calling Relocate()
    // with the same storage preferences (so the object should not
    // be actually relocated).
    string fast_storage_object_loc = netstorage.Relocate(object_loc, fNST_Fast);

    // Relocate the object to a persistent storage.
    string persistent_loc = netstorage.Relocate(object_loc, fNST_Persistent);

    // Verify that the object has disappeared from the "fast" storage.
    CNetStorageObject fast_storage_object =
            netstorage.Open(fast_storage_object_loc);
    CGetInfoNotFound(fast_storage_object).Check();

    // Make sure the relocated object does not exists in the
    // original storage anymore.
    BOOST_CHECK_THROW(s_ReadAndCompareNotFound<TApi>(data, fast_storage_object),
            CNetStorageException);

    // However, the object must still be accessible
    // either using the original ID:
    s_ReadAndCompare<CGetInfoFileTrack, TApi>(data, netstorage.Open(object_loc));
    // or using the newly generated persistent storage ID:
    s_ReadAndCompare<CGetInfoFileTrack, TApi>(data, netstorage.Open(persistent_loc));

    s_ExistsAndRemoveTests<CNetStorage, TApi>(object_loc, netstorage, data);
}

void g_TestNetStorage(CNetStorage netstorage)
{
    s_TestNetStorage<CStrData, CApi<SStrApiImpl> >(netstorage);
    s_TestNetStorage<CStrData, CApi<SBufApiImpl> >(netstorage);
}

template <class TExpected, class TApi>
void s_TestNetStorageByKey(CNetStorageByKey netstorage)
{
    TExpected data;
    CRandom random_gen;

    random_gen.Randomize();

    string unique_key = NStr::NumericToString(
            random_gen.GetRand() * random_gen.GetRand());
    CGetInfoNotFound(netstorage.Open(unique_key,
            fNST_Fast | fNST_Movable)).Check();

    // Write and read test data using a user-defined key.
    s_WriteAndRead<CGetInfoNetCache, TApi>(data, netstorage.Open(unique_key,
            fNST_Fast | fNST_Movable));

    // Make sure the object is readable with a different combination of flags.
    s_ReadAndCompare<CGetInfoNetCache, TApi>(data, netstorage.Open(unique_key,
            fNST_Persistent | fNST_Movable));

    // Relocate the object to FileTrack and make sure
    // it can be read from there.
    netstorage.Relocate(unique_key, fNST_Persistent, fNST_Movable);

    s_ReadAndCompare<CGetInfoFileTrack, TApi>(data,
            netstorage.Open(unique_key, fNST_Persistent));

    s_ExistsAndRemoveTests<CNetStorageByKey, TApi>(unique_key, netstorage, data);
}

void g_TestNetStorageByKey(CNetStorageByKey netstorage)
{
    s_TestNetStorageByKey<CStrData, CApi<SStrApiImpl> >(netstorage);
    s_TestNetStorageByKey<CStrData, CApi<SBufApiImpl> >(netstorage);
}

// TODO:
// Add tests for GetAttribute()/SetAttribute()

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
            "CNetStorage Unit Test");
}
