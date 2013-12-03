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
 * Authors: Anatoliy Kuznetsov, David McElhany, Vladimir Ivanov, Maxim Didenko
 *
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/rwstream.hpp>

#include <connect/services/netcache_api.hpp>

#include <util/compress/zlib.hpp>


using namespace ncbi;


class CSampleNetCacheClient : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);

private:
    void DemoPutRead(void);
    void DemoPartialRead(void);
    void DemoStream(void);
    void DemoCompression(void);
    void DemoIWriterIReader(void);

    string m_StreamStyle;
};


// Setup command line arguments and parameters
void CSampleNetCacheClient::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "NetCacheAPI Sample");

    arg_desc->AddOptionalKey(
        "service",
        "ServiceName",
        "Load-balanced service name",
        CArgDescriptions::eString);

    arg_desc->AddDefaultKey(
        "stream",
        "StreamApi",
        "The stream API to use",
        CArgDescriptions::eString,
        "CRWStream");
    arg_desc->SetConstraint(
        "stream",
        &(*new CArgAllow_Strings, "iostream", "CRWStream"),
        CArgDescriptions::eConstraint);

    SetupArgDescriptions(arg_desc.release());
}


int CSampleNetCacheClient::Run(void)
{
    m_StreamStyle = GetArgs()["stream"].AsString();

    DemoPutRead();
    DemoPartialRead();
    DemoStream();
    DemoCompression();
    DemoIWriterIReader();

    return 0;
}


// How to perform I/O using PutData() and ReadData()
//
// Also demonstrates:
//      configuration based on the application registry
//      overwriting data by reusing keys
//      controlling time-to-live
//      getting blob info

void CSampleNetCacheClient::DemoPutRead(void)
{
    NcbiCout << "\nPutData()/ReadData() example:" << NcbiEndl;

    // Configure based on the application registry.
    CNetCacheAPI nc_api(CNetCacheAPI::eAppRegistry);

    // Create a shared key to pass from writer to reader.
    string key;

    // Write a simple object to NetCache.
    {{
        string message("The quick brown fox jumps over a lazy dog.");
        key = nc_api.PutData(message.c_str(), message.size());
    }}

    // Overwrite the data by reusing the key, and specify a time-to-live.
    {{
        string message("second message");
        nc_api.PutData(key, message.c_str(), message.size(), nc_blob_ttl = 600);
    }}

    // By not setting a time-to-live (TTL), the previously set TTL value
    // will be replaced by the server default.
    {{
        string message("yet another message");
        nc_api.PutData(key, message.c_str(), message.size());
        NcbiCout
            << "Wrote: '" << message
            << "' to blob: " << key << NcbiEndl;

        // Use GetBlobInfo() to find out what TTL the server gave this blob.
        string blob_info;
        CNetServerMultilineCmdOutput result(nc_api.GetBlobInfo(key));
        while (result.ReadLine(blob_info)) {
            if (NStr::EqualNocase(blob_info, 0, 4, "TTL:")) {
                NcbiCout << blob_info << NcbiEndl;
                break;
            }
        }
    }}

    // Read back the data for the key.
    {{
        string message;
        nc_api.ReadData(key, message);
        NcbiCout << "Read: '" << message << "'" << NcbiEndl;
    }}
}


// How to read just part of a large blob using PartRead()
//
// Also demonstrates:
//      configuration based on the application registry

void CSampleNetCacheClient::DemoPartialRead(void)
{
    NcbiCout << "\nPartRead() example:" << NcbiEndl;

    // Configure based on the application registry.
    CNetCacheAPI nc_api(CNetCacheAPI::eAppRegistry);

    // Create a shared key to pass from writer to reader.
    string key;

    // Write a blob to NetCache, and pretend it's really huge.
    {{
        string message("Just pretend this is a really huge blob, ok?");
        key = nc_api.PutData(message.c_str(), message.size());
        NcbiCout
            << "Wrote: '" << message
            << "' to blob: " << key << NcbiEndl;
    }}

    // Read back the data a part at a time, in case it's too big to handle
    // all at once.
    {{
        // Get the full size of the blob as stored in NetCache.
        // This is the initial size remaining to be read from NetCache.
        size_t remaining(nc_api.GetBlobSize(key));

        // Read all parts of the blob.
        for (size_t offset = 0; remaining > 0; ) {

            // The last read will probably be less than our max read size.
            static const size_t kMaxReadSize = 10;
            size_t part_size = min(remaining, kMaxReadSize);

            // Read part of the blob.
            string part;
            nc_api.ReadPart(key, offset, part_size, part);
            NcbiCout << "Read: '" << part << "'" << NcbiEndl;

            // Prep for the next part.
            offset    += part_size;
            remaining -= part_size;
        }
    }}
}


// How to perform I/O using C++ iostream API -- CreateOStream() and GetIStream()
//
// Also demonstrates:
//      configuration using a user-specified service name
//      retrieving the same data multiple times

void CSampleNetCacheClient::DemoStream(void)
{
    NcbiCout << "\nCreateOStream()/GetIStream() example:" << NcbiEndl;

    auto_ptr<CNetCacheAPI> nc_api_p;
    {{
        // Configure based on the user-specified service, if given.
        if (GetArgs()["service"]) {
            nc_api_p.reset(new CNetCacheAPI(
                GetArgs()["service"].AsString(), "ncapi_sample"));
        }
        else {
            // Default to configuring based on the application registry.
            nc_api_p.reset(new CNetCacheAPI(GetConfig()));
        }
    }}

    // Create a shared key to pass from writer to reader.
    string key;

    // Write to NetCache stream.
    {{
        auto_ptr<CNcbiOstream> os(nc_api_p->CreateOStream(key,
                nc_blob_ttl = 600));
        *os << "line one\n";
        *os << "line two\n";
        *os << "line three" << NcbiEndl;
        NcbiCout << "Wrote three lines to blob: " << key << NcbiEndl;

        // Streams are not written to the NetCache server until the stream
        // is deleted.  So you could call os.reset() if you wanted to
        // access the blob in the same scope as the stream auto_ptr.
        //os.reset(); // not necessary in this example
    }}

    // Retrieve words from NetCache stream.
    // Note that this doesn't remove the data from NetCache.
    {{
        auto_ptr<CNcbiIstream> is(nc_api_p->GetIStream(key));
        while (!is->eof()) {
            string message;
            *is >> message; // get one word at a time, ignoring whitespace
            NcbiCout << "Read word: '" << message << "'" << NcbiEndl;
        }
    }}

    // Reading from NetCache doesn't delete the data.  If desired, you can
    // retrieve the same data again by reusing the same key.
    // In this example, get all the data at once from a NetCache stream
    // (preserving whitespace).
    {{
        auto_ptr<CNcbiIstream> is(nc_api_p->GetIStream(key));
        NcbiCout << "Read buffer: '" << is->rdbuf() << "'" << NcbiEndl;
    }}
}


// How to perform I/O using compression
//
// Also demonstrates:
//      configuration based on the application registry

void CSampleNetCacheClient::DemoCompression(void)
{
    NcbiCout << "\nCompression example:" << NcbiEndl;

    // Configure based on the application registry.
    CNetCacheAPI nc_api(CNetCacheAPI::eAppRegistry);

    // Create a shared key to pass from writer to reader.
    string key;

    // Write to NetCache with compression.
    {{
        string message("The quick brown fox jumps over a lazy dog.");
        auto_ptr<CNcbiOstream> os(nc_api.CreateOStream(key));
        CCompressionOStream os_zip(*os, new CZipStreamCompressor(),
                                   CCompressionStream::fOwnWriter);
        os_zip << message;
        NcbiCout
            << "Wrote: '" << message
            << "' to blob: " << key << NcbiEndl;
    }}

    // Read back from NetCache with decompression.
    // NOTE: The decompression stream type must match the compression type.
    {{
        auto_ptr<CNcbiIstream> is(nc_api.GetIStream(key));
        string message;
        CCompressionIStream is_zip(*is, new CZipStreamDecompressor(),
                                    CCompressionStream::fOwnReader);
        getline(is_zip, message);
        NcbiCout << "Read: '" << message << "'" << NcbiEndl;
    }}
}


// How to perform I/O using IWriter() and IReader()
//
// Also demonstrates:
//      configuration based on a provided registry
//      reusing keys

void CSampleNetCacheClient::DemoIWriterIReader(void)
{
    NcbiCout << "\nIWriter()/IReader() example:" << NcbiEndl;

    // Configure based on the the provided registry.  In this case it is the
    // application registry for simplicity, but it could be any registry.
    CNetCacheAPI nc_api(GetConfig());

    // Create a shared key to pass from writer to reader.
    string key;

    // Write some data to NetCache.  Typically this might be in a loop,
    // but it's just done once here to cleanly illustrate the API call.
    {{
        // Create a data writer.
        auto_ptr<IEmbeddedStreamWriter> writer(nc_api.PutData(&key,
                nc_blob_ttl = 600));

        if (m_StreamStyle == "iostream") {
            // Write some data via ostream API.

            CWStream os(&*writer);
            string text1("just chars."), text2("with a newline");
            os << text1;
            os << text2 << NcbiEndl;
            NcbiCout
                << "Wrote '" << text1
                << "' and '" << text2 << "' to blob: " << key << NcbiEndl;
        } else {
            // Write some data via IWriter API.

            char data[] = "abcdefghij1234567890!@#";
            writer->Write(data, strlen(data)); // don't store terminating zero
            NcbiCout
                << "Wrote: '" << data
                << "' to blob: " << key << NcbiEndl;
        }

        // Writers do not actually write to the NetCache server until the
        // writer is deleted or explicitly closed.
        //writer->Close(); // not necessary in this example
    }}

    // Read some data from NetCache.  This example just demonstrates how to
    // use the API - you can extrapolate to looping through the whole blob.
    {{
        // Create a data reader and get the blob size.
        size_t blob_size;
        auto_ptr<IReader> reader(nc_api.GetReader(key, &blob_size));

        // Don't intermix using different stream APIs.  Reading via
        // CRStream may buffer more input than you consume, possibly resulting
        // in corrupted data and/or an exception if you switch to istream.

        if (m_StreamStyle == "iostream") {
            // Read some data via istream API.

            CRStream is(&*reader);
            string text1, text2;
            is >> text1 >> text2;
            NcbiCout
                << "Read '" << text1
                << "' and '" << text2 << "' from blob:" << key << NcbiEndl;
        } else {
            // Read some data via IReader API.

            // Create a working buffer.  In this example, a zero-termination
            // character is added only for the convenience of printing easily.
            static const Uint8 kMyBufSize = 10;
            char my_buf[kMyBufSize + 1];

            // Don't overflow!
            Uint8 read_size = min<Uint8>(blob_size, kMyBufSize);

            // Note: It's possible that fewer bytes will be read than are
            // available, without an error.  Therefore, bytes_read must
            // always be used after the read instead of read_size.
            size_t bytes_read;
            ERW_Result rw_res = reader->Read(my_buf,
                    (size_t) read_size, &bytes_read);
            my_buf[bytes_read] = '\0';

            if (rw_res == eRW_Success) {
                NcbiCout << "Read: '" << my_buf << "'" << NcbiEndl;
            } else {
                NCBI_USER_THROW("Error while reading blob.");
            }
        }
    }}
}


int main(int argc, const char* argv[])
{
    return CSampleNetCacheClient().AppMain(argc, argv);
}
