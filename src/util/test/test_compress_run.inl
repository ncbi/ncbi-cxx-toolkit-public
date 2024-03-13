
//============================================================================
// NOTE:
// 
// Some tests using same code to tests eZip / eZipCloudflare.
// This is not technically correct, but should works for the test purposes.
//
//============================================================================

    // Check allowed compression methods

    switch(method) {
    case M::eBZip2:
    case M::eLZO:
    case M::eZip:
    case M::eZipCloudflare:
    case M::eZstd:
        break;
    default:
        _TROUBLE;
    }

    // Check flags

    // The fAllowTransparentRead / fAllowEmptyData should be the same for all compressors.
#if defined(HAVE_LIBBZ2)  &&  defined(HAVE_LIBZ)
    _VERIFY(static_cast<int>(CZipCompression::fAllowTransparentRead) == 
            static_cast<int>(CBZip2Compression::fAllowTransparentRead));
    _VERIFY(static_cast<int>(CZipCompression::fAllowEmptyData) == 
            static_cast<int>(CBZip2Compression::fAllowEmptyData));
#endif
#if defined(HAVE_LIBLZO)  &&  defined(HAVE_LIBZ)
    _VERIFY(static_cast<int>(CZipCompression::fAllowTransparentRead) == 
            static_cast<int>(CLZOCompression::fAllowTransparentRead));
    _VERIFY(static_cast<int>(CZipCompression::fAllowEmptyData) == 
            static_cast<int>(CLZOCompression::fAllowEmptyData));
#endif
#if defined(HAVE_LIBZSTD)  &&  defined(HAVE_LIBZ)
    _VERIFY(static_cast<int>(CZipCompression::fAllowTransparentRead) == 
            static_cast<int>(CZstdCompression::fAllowTransparentRead));
    _VERIFY(static_cast<int>(CZipCompression::fAllowEmptyData) == 
            static_cast<int>(CZstdCompression::fAllowEmptyData));
#endif

    // Regular/Cloudflare zlib flags should be the same.
#if defined(HAVE_LIBZCF)  &&  defined(HAVE_LIBZ)
    _VERIFY(static_cast<int>(CZipCompression::fAllowTransparentRead) == 
            static_cast<int>(CZipCloudflareCompression::fAllowTransparentRead));
    _VERIFY(static_cast<int>(CZipCompression::fAllowEmptyData) == 
            static_cast<int>(CZipCloudflareCompression::fAllowEmptyData));
    _VERIFY(static_cast<int>(CZipCompression::fCheckFileHeader) == 
            static_cast<int>(CZipCloudflareCompression::fCheckFileHeader));
    _VERIFY(static_cast<int>(CZipCompression::fWriteGZipFormat) == 
            static_cast<int>(CZipCloudflareCompression::fWriteGZipFormat));
    _VERIFY(static_cast<int>(CZipCompression::fAllowConcatenatedGZip) == 
            static_cast<int>(CZipCloudflareCompression::fAllowConcatenatedGZip));
    _VERIFY(static_cast<int>(CZipCompression::fGZip) == 
            static_cast<int>(CZipCloudflareCompression::fGZip));
    _VERIFY(static_cast<int>(CZipCompression::fRestoreFileAttr) == 
            static_cast<int>(CZipCloudflareCompression::fRestoreFileAttr));
#endif


//============================================================================
//
// Common code to test compression methods for ST and MT tests
//
//============================================================================

    // Allocate memory for buffers.Extra byte is necessary just in case... 
    // Some tests trying to read a bit more to get EOF.
    AutoArray<char> dst_buf_arr(buf_len + 1);
    AutoArray<char> cmp_buf_arr(buf_len + 1);
    char* dst_buf = dst_buf_arr.get();
    char* cmp_buf = cmp_buf_arr.get();

    assert(dst_buf);
    assert(cmp_buf);

    size_t dst_len, out_len;
    bool result;


    //------------------------------------------------------------------------
    // Version info
    //------------------------------------------------------------------------

    CVersionInfo version = TCompression().GetVersion();
    ERR_POST(Info << "Compression library name and version: " << version.Print());

    // zlib v1.2.2 and earlier have a bug in decoding. In some cases
    // decompressor can produce output data on invalid compressed data.
    // So, we do not run "transparent read" tests if zlib version < 1.2.3.

    bool allow_transparent_read_test = true;
    if ( (method == M::eZip  ||  method == M::eZipCloudflare)
         &&  !version.IsUpCompatible(CVersionInfo(1, 2, 3)) ) 
    {
        allow_transparent_read_test = false;
        ERR_POST(Info << "Transparent read tests are not allowed for this test and library version.");
    }

    //------------------------------------------------------------------------
    // Compress/decompress buffer
    //------------------------------------------------------------------------
    {{
        ERR_POST(Trace << "Compress/decompress buffer test (default level)...");
        INIT_BUFFERS;

        // Compress data
        TCompression c(CCompression::eLevel_Medium);
#if defined(HAVE_LIBLZO)
        if (method == M::eLZO  &&  buf_len > LZO_kMaxBlockSize) {
            // Automatically use stream format for LZO if buffer size is too big for a single block
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat);
        }
#endif
        result = c.CompressBuffer(src_buf, src_len, dst_buf, buf_len, &out_len);
        PrintResult(eCompress, c.GetErrorCode(), src_len, buf_len, out_len);
        assert(result);

        // Decompress data
        dst_len = out_len;
        result = c.DecompressBuffer(dst_buf, dst_len, cmp_buf, buf_len, &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), dst_len, buf_len, out_len);
        assert(result);
        assert(out_len == src_len);

        // Compare original and decompressed data
        assert(memcmp(src_buf, cmp_buf, out_len) == 0);
        OK;
    }}

#if defined(HAVE_LIBZ)  &&  defined(HAVE_LIBZCF)
    if (method == M::eZip  ||  method == M::eZipCloudflare)
    {{
        ERR_POST(Trace << "Compress/decompress buffer test (GZIP)...");
        INIT_BUFFERS;

        // Compress data
        TCompression c(CCompression::eLevel_Medium);
        c.SetFlags(CZipCompression::fGZip);
        result = c.CompressBuffer(src_buf, src_len, dst_buf, buf_len, &out_len);
        PrintResult(eCompress, c.GetErrorCode(), src_len, buf_len, out_len);
        assert(result);

        // Decompress data
        dst_len = out_len;
        result = c.DecompressBuffer(dst_buf, dst_len, cmp_buf, buf_len, &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), dst_len, buf_len, out_len);
        assert(result);
        assert(out_len == src_len);

        // Compare original and decompressed data
        assert(memcmp(src_buf, cmp_buf, out_len) == 0);
        OK;
    }}
#endif

    //------------------------------------------------------------------------
    // Compress/decompress buffer with CRC32 checksum (LZO only)
    //------------------------------------------------------------------------

#if defined(HAVE_LIBLZO)
    if (method == M::eLZO)
    {{
        ERR_POST(Trace << "Compress/decompress buffer test (LZO, CRC32, default level)...");
        INIT_BUFFERS;

        // Compress data
        TCompression c(CCompression::eLevel_Best);
        c.SetFlags(c.GetFlags() | CLZOCompression::fChecksum);
#if defined(HAVE_LIBLZO)
        if (buf_len > LZO_kMaxBlockSize) {
            // Automatically use stream format for LZO if buffer size is too big for a single block
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat);
        }
#endif
        result = c.CompressBuffer(src_buf, src_len, dst_buf, buf_len, &out_len);
        PrintResult(eCompress, c.GetErrorCode(), src_len, buf_len, out_len);
        assert(result);

        // Decompress data
        dst_len = out_len;
        result = c.DecompressBuffer(dst_buf, dst_len, cmp_buf, buf_len, &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), dst_len, buf_len, out_len);
        assert(result);
        assert(out_len == src_len);

        // Compare original and decompressed data
        assert(memcmp(src_buf, cmp_buf, out_len) == 0);
        OK;
    }}
#endif

    //------------------------------------------------------------------------
    // Overflow test
    //------------------------------------------------------------------------
    {{
        ERR_POST(Trace << "Output buffer overflow test...");

        TCompression c;
        dst_len = 5;
        result = c.CompressBuffer(src_buf, src_len, dst_buf, dst_len, &out_len);
        PrintResult(eCompress, c.GetErrorCode(), src_len, dst_len, out_len);
        assert(!result);
        // Some decoders (like lzo or zsd) produce nothing if destination buffer is too small,
        // some other can produce some data.
        assert(out_len == 0 || out_len == dst_len);
        OK;
    }}

    //------------------------------------------------------------------------
    // Compress/decompress buffer: empty input, default behavior.
    // Also see fAllowEmptyData flag test below.
    //------------------------------------------------------------------------
    {{
        ERR_POST(Trace << "Compress/decompress buffer (empty input)...");

        TCompression c;
        result = c.CompressBuffer(src_buf, 0, dst_buf, dst_len, &out_len);
        PrintResult(eCompress, c.GetErrorCode(), 0, dst_len, out_len);
        assert(result == false);
        assert(out_len == 0);
        result = c.DecompressBuffer(src_buf, 0, dst_buf, dst_len, &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), 0, dst_len, out_len);
        assert(result == false);
        assert(out_len == 0);
        OK;
    }}

    //------------------------------------------------------------------------
    // Decompress buffer: transparent read
    //------------------------------------------------------------------------

    if (allow_transparent_read_test)
    {{
        ERR_POST(Trace << "Decompress buffer (transparent read)...");
        INIT_BUFFERS;

        TCompression c(CCompression::eLevel_Medium);
        c.SetFlags(CZipCompression::fAllowTransparentRead);
#if defined(HAVE_LIBLZO)
        // Set stream format for LZO just to avoid a warning
        // on a decompression of very big data.
        if (buf_len > LZO_kMaxBlockSize) {
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat);
        }
#endif
        // "Decompress" source buffer, which is uncompressed
        result = c.DecompressBuffer(src_buf, src_len, dst_buf, buf_len, &dst_len);
        PrintResult(eDecompress, c.GetErrorCode(), src_len, buf_len, dst_len);
        assert(result);
        // Should have the same buffer as output
        assert(dst_len == src_len);
        assert(memcmp(src_buf, dst_buf, dst_len) == 0);

        // Overflow test
        dst_len = 5;
        result = c.DecompressBuffer(src_buf, src_len, dst_buf, dst_len, &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), src_len, dst_len, out_len);
        assert(!result);
        assert(dst_len == out_len);
        OK;
    }}

    //------------------------------------------------------------------------
    // File compression/decompression test
    //------------------------------------------------------------------------
    {{
        ERR_POST(Trace << "File compress/decompress test...");
        size_t n;

        // Empty input test

        INIT_BUFFERS;
        {{
            TCompressionFile zf;
            assert(zf.Open(kFileName, TCompressionFile::eMode_Write)); 
            assert(zf.Close()); 
            assert(CFile(kFileName).GetLength() == 0);
            CFile(kFileName).Remove();
        }}

        // First test -- write 1Kb blocks and read in bulk

        INIT_BUFFERS;
        {{
            {{
                TCompressionFile zf;
                // Compressing data and write it to the file
                assert(zf.Open(kFileName, TCompressionFile::eMode_Write)); 
                size_t i;
                for (i = 0; i < src_len / 1024; i++) {
                    n = zf.Write(src_buf + i*1024, 1024);
                    assert(n == 1024);
                }
                n = zf.Write(src_buf + i * 1024, src_len % 1024);
                assert(n == src_len % 1024);

                assert(zf.Close()); 
                assert(CFile(kFileName).GetLength() > 0);
            }}
            {{
                TCompressionFile zf;
#if defined(HAVE_LIBLZO)
                // The blocksize and flags stored in the header should be used
                // for decompression instead of the values passed as parameters.
                // Add bogus flag to check CRC32 checksum (compressed data doesn't have it).
                zf.SetFlags(zf.GetFlags() | CLZOCompression::fChecksum);
#endif
                // Decompress data from file
                assert(zf.Open(kFileName, TCompressionFile::eMode_Read)); 
                assert(ReadCompressionFile<TCompressionFile>(zf, cmp_buf, src_len) == src_len);
                assert(zf.Close()); 

                // Compare original and decompressed data
                assert(memcmp(src_buf, cmp_buf, src_len) == 0);
            }}
        }}

        // Second test -- write in bulk and read by 100 bytes

        INIT_BUFFERS;
        {{
            {{
                // Write data with compression into the file
                TCompressionFile zf(kFileName, TCompressionFile::eMode_Write, CCompression::eLevel_Best);
                n = WriteCompressionFile<TCompressionFile>(zf, src_buf, src_len);
                assert(n == src_len);
            }}
            {{
                // Read data from compressed file
                TCompressionFile zf(kFileName, TCompressionFile::eMode_Read);
                size_t nread = 0;
                do {
                    n = zf.Read(cmp_buf + nread, 100);
                    nread += n;
                } while ( n != 0 );
                assert(nread == src_len);
            }}
            // Compare original and decompressed data
            assert(memcmp(src_buf, cmp_buf, src_len) == 0);
        }}
        
        CFile(kFileName).Remove();
        OK;
    }}

    //------------------------------------------------------------------------
    // File decompression: transparent read test
    //------------------------------------------------------------------------

    if (allow_transparent_read_test)
    {{
        ERR_POST(Trace << "Decompress file (transparent read)...");
        INIT_BUFFERS;

        TCompressionFile zf;
        // Set flag to allow transparent read on decompression.
        // The fAllowTransparentRead should be the same for all compressors.
        zf.SetFlags(zf.GetFlags() | CZipCompression::fAllowTransparentRead);

        //
        // Test for usual compression/decompression
        //

        // Compress data to file
        assert(zf.Open(kFileName, TCompressionFile::eMode_Write)); 
        assert(WriteCompressionFile<TCompressionFile>(zf, src_buf, src_len) == src_len);
        assert(zf.Close()); 
        assert(CFile(kFileName).GetLength() > 0);
        
        // Decompress data from file
        assert(zf.Open(kFileName, TCompressionFile::eMode_Read)); 
        assert(ReadCompressionFile<TCompressionFile>(zf, cmp_buf, src_len) == src_len);
        assert(zf.Close()); 

        // Compare original and decompressed data
        assert(memcmp(src_buf, cmp_buf, src_len) == 0);

        //
        // Test to read uncompressed data
        //

        // Create file with uncompressed data,
        // or reuse m_SrcFile if we have it already (big data test)
        string srcfile;
        if ( !m_SrcFile.empty() ) {
            srcfile = m_SrcFile;
        } else {
            srcfile = kFileName;
            x_CreateFile(srcfile, src_buf, src_len);
        }
        // Transparent read from this file
        assert(zf.Open(srcfile, TCompressionFile::eMode_Read));
        assert(ReadCompressionFile<TCompressionFile>(zf, cmp_buf, src_len) == src_len);
        assert(zf.Close()); 

        // Compare original and "decompressed" data
        assert(memcmp(src_buf, cmp_buf, src_len) == 0);

        CFile(kFileName).Remove();
        OK;
    }}

    //------------------------------------------------------------------------
    // Compression input stream test
    //------------------------------------------------------------------------
    {{
        ERR_POST(Trace << "Compression input stream test...");
        INIT_BUFFERS;
        string src(src_buf, src_len);

        // Create stream with uncompressed data
        unique_ptr<CNcbiIos> stm(x_CreateIStream(kFileName, src, buf_len));
        CCompressionIStream ics_zip(*stm, new TStreamCompressor(),
                                    CCompressionStream::fOwnProcessor);
        assert(ics_zip.good());
        
        // Read compressed data from stream
        dst_len = ics_zip.Read(dst_buf, buf_len + 1);
        assert(ics_zip.eof());
        
        // We should have all packed data here, because compressor
        // finalization for input streams accomplishes automatically.
        PrintResult(eCompress, kUnknownErr, src_len, kUnknown, dst_len);
        assert(ics_zip.GetProcessedSize() == src_len);
        assert(ics_zip.GetOutputSize() == dst_len);

        // Decompress data back, and compare
        TCompression c;
#if defined(HAVE_LIBLZO)
        if (method == M::eLZO) {
            // For LZO we should use fStreamFormat flag for DecompressBuffer()
            // method to decompress data, compressed using streams.
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat);
        }
#endif
        result = c.DecompressBuffer(dst_buf, dst_len, cmp_buf, buf_len, &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), dst_len, buf_len, out_len);
        assert(result);

        // Compare original and uncompressed data
        assert(out_len == src_len);
        assert(memcmp(src_buf, cmp_buf, src_len) == 0);
        
        CFile(kFileName).Remove();
        OK;
    }}

    //------------------------------------------------------------------------
    // Decompression input stream test
    //------------------------------------------------------------------------
    {{
        ERR_POST(Trace << "Decompression input stream test...");
        INIT_BUFFERS;

        // Compress data and use it to create input stream
        TCompression c;
#if defined(HAVE_LIBLZO)
        if (method == M::eLZO) {
            // For LZO we should use fStreamFormat flag for CompressBuffer()
            // method, because we will decompress it using decompression
            // stream.
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat);
        }
#endif
        result = c.CompressBuffer(src_buf, src_len, dst_buf, buf_len, &dst_len);
        PrintResult(eCompress, c.GetErrorCode(), src_len, buf_len, dst_len);
        assert(result);
        assert(dst_len > 0);
        string dst(dst_buf, dst_len);

        unique_ptr<CNcbiIos> stm(x_CreateIStream(kFileName, dst, buf_len));
        CCompressionIStream ids_zip(*stm, new TStreamDecompressor(), CCompressionStream::fOwnReader);
        assert(ids_zip.good());

        // Read decompressed data from stream
        out_len = ids_zip.Read(cmp_buf, buf_len + 1);
        assert(ids_zip.eof());
        
        // We should have all packed data here, because compressor
        // finalization for input streams accomplishes automatically.
        PrintResult(eDecompress, kUnknownErr, dst_len, buf_len, out_len);
        assert(ids_zip.GetProcessedSize() == dst_len);
        assert(ids_zip.GetOutputSize() == src_len);

        // Compare original and uncompressed data
        assert(out_len == src_len);
        assert(memcmp(src_buf, cmp_buf, src_len) == 0);

        CFile(kFileName).Remove();
        OK;
    }}

    //------------------------------------------------------------------------
    // Compression output stream test
    //------------------------------------------------------------------------
    {{
        ERR_POST(Trace << "Compression output stream test...");
        INIT_BUFFERS;

        // Create output stream

        unique_ptr<CNcbiIos> stm;
        CNcbiOstrstream* os_str = nullptr; // need for CNcbiOstrstreamToString()
        
        if ( m_AllowOstrstream ) {
            os_str = new CNcbiOstrstream();
            stm.reset(os_str);
        } else {
            stm.reset(new CNcbiOfstream(kFileName, ios::out | ios::binary));
        }
        assert(stm->good());

        // Write data to compressing stream
        {{
            CCompressionOStream os_zip(*stm, new TStreamCompressor(),
                                       CCompressionStream::fOwnWriter);
            assert(os_zip.good());
            os_zip.Write(src_buf, src_len);
            assert(os_zip.good());
            // Finalize compression stream in the destructor.
            // The output stream 'stm' will receive all data only after
            // finalization. But we do not recommend to do this in real 
            // applications, because you cannot check stream status 
            // after finalization. It is better to call Finalize()
            // directly, after you finish writing to stream.
        }}
        
        // Get compressed data and size
        string str;
        size_t n;
        if ( m_AllowOstrstream ) {
            str = CNcbiOstrstreamToString(*os_str);
            n = str.size();
        } else {
            stm.reset();
            size_t filelen = (size_t)CFile(kFileName).GetLength();
            assert(filelen > 0);
            CFileIO f;
            f.Open(kFileName, CFileIO::eOpen, CFileIO::eRead);
            n = f.Read(dst_buf, filelen);
            assert(n == filelen);
            f.Close();
            CFile(kFileName).Remove();
        }
        PrintResult(eCompress, kUnknownErr, src_len, buf_len, n);
        assert(n > 0);

        // Try to decompress data
        TCompression c;
#if defined(HAVE_LIBLZO)
        if (method == M::eLZO) {
            // For LZO we should use fStreamFormat flag for DecompressBuffer()
            // method to decompress data compressed inside compression stream.
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat);
        }
#endif
        if ( m_AllowOstrstream ) {
            result = c.DecompressBuffer(str.data(), n, cmp_buf, buf_len, &out_len);
        } else {
            result = c.DecompressBuffer(dst_buf, n, cmp_buf, buf_len, &out_len);
        }
        PrintResult(eDecompress, c.GetErrorCode(), n, buf_len, out_len);
        assert(result);

        // Compare original and decompressed data
        assert(out_len == src_len);
        assert(memcmp(src_buf, cmp_buf, out_len) == 0);
        OK;
    }}

    //------------------------------------------------------------------------
    // Decompression output stream test
    //------------------------------------------------------------------------
    {{
        ERR_POST(Trace << "Decompression output stream test...");
        INIT_BUFFERS;

        // Compress the data
        TCompression c;
#if defined(HAVE_LIBLZO)
        if (method == M::eLZO) {
            // For LZO we should use fStreamFormat flag for CompressBuffer()
            // method for following decompress of data using compression
            // stream.
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat | CLZOCompression::fChecksum);
        }
#endif
        result = c.CompressBuffer(src_buf, src_len, dst_buf, buf_len, &out_len);
        PrintResult(eCompress, c.GetErrorCode(), src_len, buf_len, out_len);
        assert(result);

        // Create output stream

        unique_ptr<CNcbiIos> stm;
        CNcbiOstrstream* os_str = nullptr; // need for CNcbiOstrstreamToString()
        
        if ( m_AllowOstrstream ) {
            os_str = new CNcbiOstrstream();
            stm.reset(os_str);
        } else {
            stm.reset(new CNcbiOfstream(kFileName, ios::trunc | ios::out | ios::binary));
        }
        assert(stm->good());

        // Write compressed data to decompressing stream
        CCompressionOStream os_zip(*stm, new TStreamDecompressor(),
                                   CCompressionStream::fOwnProcessor);
        assert(os_zip.good());
#if defined(HAVE_LIBLZO)
        // The blocksize and flags stored in the header should be used
        // instead of the value passed in the parameters for decompressor.
        c.SetFlags(c.GetFlags() | CLZOCompression::fChecksum);
#endif
        os_zip.Write(dst_buf, out_len);
        assert(os_zip.good());
        // Finalize a compression stream via direct call Finalize().
        os_zip.Finalize();
        assert(os_zip.good());

        // Get decompressed data and size
        string str;
        size_t n;
        if ( m_AllowOstrstream ) {
            str = CNcbiOstrstreamToString(*os_str);
            n = str.size();
        } else {
            stm.reset();
            size_t filelen = (size_t)CFile(kFileName).GetLength();
            CFileIO f;
            f.Open(kFileName, CFileIO::eOpen, CFileIO::eRead);
            n = f.Read(cmp_buf, filelen);
            assert(n == filelen);
            f.Close();
            CFile(kFileName).Remove();
        }
        PrintResult(eCompress, kUnknownErr, out_len, buf_len, n);
        assert(n == src_len);
        assert(os_zip.GetProcessedSize() == out_len);
        assert(os_zip.GetOutputSize() == src_len);

        // Compare original and uncompressed data
        if ( m_AllowOstrstream ) {
            assert(memcmp(src_buf, str.data(), n) == 0);
        } else {
            assert(memcmp(src_buf, cmp_buf, n) == 0);
        }
        
        CFile(kFileName).Remove();
        OK;
    }}

    //------------------------------------------------------------------------
    // Decompression input stream test: transparent read
    //------------------------------------------------------------------------
    if (allow_transparent_read_test)
    {{
        ERR_POST(Trace << "Decompression input stream test (transparent read)...");
        INIT_BUFFERS;
        string src(src_buf, src_len);

        // Create test input stream with uncompressed data
        unique_ptr<CNcbiIos> stm;
        if ( m_AllowIstrstream ) {
            stm.reset(new CNcbiIstrstream(src));
        } else {
            stm.reset(new CNcbiIfstream(_T_XCSTRING(m_SrcFile), ios::in | ios::binary));
        }
        assert(stm->good());

        // Create decompressor and set flags to allow transparent read
        TStreamDecompressor decompressor(CZipCompression::fAllowTransparentRead);

        // Read uncompressed data from stream
        CCompressionIStream ids_zip(*stm, &decompressor);
        assert(ids_zip.good());
        out_len = ids_zip.Read(dst_buf, buf_len + 1);
        assert(ids_zip.eof());
        PrintResult(eDecompress, kUnknownErr, src_len, buf_len, out_len);
        assert(ids_zip.GetProcessedSize() == out_len);
        assert(ids_zip.GetOutputSize() == out_len);

        // Compare original and uncompressed data
        assert(out_len == src_len);
        assert(memcmp(src_buf, dst_buf, src_len) == 0);
        OK;
    }}

    //------------------------------------------------------------------------
    // I/O stream tests
    //------------------------------------------------------------------------
    {{
        ERR_POST(Trace << "I/O stream tests...");
        {{
            INIT_BUFFERS;
            
            int errcode = kUnknownErr;
            string errmsg;
            
            unique_ptr<CNcbiIos> stm;
            CNcbiFstream* fs = nullptr; // need for flushing between read/write
            
            if ( m_AllowStrstream ) {
                stm.reset(new CNcbiStrstream());
            } else {
                fs = new CNcbiFstream(kFileName, ios::trunc | ios::in | ios::out | ios::binary);
                stm.reset(fs);
            }

            assert(stm->good());
        
            // Compress on write, decompress on read.
            CCompressionIOStream zip(*stm, new TStreamDecompressor(),
                                           new TStreamCompressor(),
                                           CCompressionStream::fOwnProcessor);
            assert(zip.good()  &&  stm->good());
            size_t n = zip.Write(src_buf, src_len);
            zip.GetError(CCompressionStream::eWrite, errcode, errmsg);
            assert(n == src_len);
            assert(zip.good()  &&  stm->good());
            zip.Finalize(CCompressionStream::eWrite);
            dst_len = zip.GetOutputSize(CCompressionStream::eWrite);
            PrintResult(eCompress, errcode, src_len, buf_len, dst_len);
            assert(!zip.eof()  &&  zip.good());
            assert(!stm->eof() &&  stm->good());
            assert(zip.GetProcessedSize(CCompressionStream::eWrite) == src_len);
            assert(zip.GetProcessedSize(CCompressionStream::eRead) == 0);
            assert(zip.GetOutputSize(CCompressionStream::eWrite) > 0);
            assert(zip.GetOutputSize(CCompressionStream::eRead) == 0);

            // Read data
            if ( fs ) {
                // for file streams we need to flush and reset a get position
                fs->flush();
                fs->seekg(0, std::ios::beg);
            }
            out_len = zip.Read(cmp_buf, src_len);
            zip.GetError(CCompressionStream::eRead, errcode, errmsg);
            PrintResult(eDecompress, errcode, dst_len, buf_len, out_len);
            assert(out_len == src_len);
            assert(!stm->eof() &&  stm->good());
            assert(!zip.eof()  &&  zip.good());
            assert(out_len == src_len);
            assert(zip.GetProcessedSize(CCompressionStream::eWrite) == src_len);
            assert(zip.GetProcessedSize(CCompressionStream::eRead) > 0);
            assert(zip.GetOutputSize(CCompressionStream::eWrite) > 0);
            assert(zip.GetOutputSize(CCompressionStream::eRead) == src_len);

            // Check on EOF
            char c; 
            zip >> c;
            assert(zip.eof());
            assert(!zip);

            // Compare buffers
            assert(memcmp(src_buf, cmp_buf, src_len) == 0);
            
            if ( !m_AllowStrstream ) {
                CFile(kFileName).Remove();
            }
       }}
        
        // Second test
        // Don't run for "big data" test, it is designed for fixed memory size.
        
        if (m_AllowStrstream)
        {{
            const int kCount[2] = {1, 1000};
            for (int k = 0; k < 2; k++) {
                int n = kCount[k];

                INIT_BUFFERS;
                CNcbiStrstream stm;
                CCompressionIOStream zip(stm,
                                         new TStreamDecompressor(),
                                         new TStreamCompressor(),
                                         CCompressionStream::fOwnProcessor);
                assert(zip.good());
                int v;
                for (int i = 0; i < n; i++) {
                    v = i * 2;
                    zip << v << endl;
                }
                zip.Finalize(CCompressionStream::eWrite);
                assert(zip.good());
                zip.clear();

                for (int i = 0; i < n; i++) {
                    zip >> v;
                    assert(!zip.eof());
                    assert(v == i * 2);
                }
                zip.Finalize();
                zip >> v;
                assert(zip.eof());
            }
        }}
        OK;
    }}

    //------------------------------------------------------------------------
    // Advanced I/O stream test:
    //      - read/write;
    //      - reusing compression/decompression stream processors.
    //------------------------------------------------------------------------
    // Dont run for "big data" test, it is designed for fixed memory size.
        
    if (m_AllowIstrstream && m_AllowOstrstream)
    {{
        ERR_POST(Trace << "Advanced I/O stream test...");
        INIT_BUFFERS;

        TStreamCompressor   compressor;
        TStreamDecompressor decompressor;
        int v;

        for (int count = 0; count<5; count++) {
            // Compress data to output stream
            CNcbiOstrstream os_str;
            int n = 1000;
            {{
                CCompressionOStream ocs_zip(os_str, &compressor);
                assert(ocs_zip.good());
                for (int i = 0; i < n; i++) {
                    v = i * 2;
                    ocs_zip << v << endl;
                }
            }}
            string str = CNcbiOstrstreamToString(os_str);
            PrintResult(eCompress, kUnknownErr, kUnknown, kUnknown, str.size());

            // Decompress data from input stream
            CNcbiIstrstream is_str(str);
            CCompressionIStream ids_zip(is_str, &decompressor);
            assert(ids_zip.good());
            for (int i = 0; i < n; i++) {
                ids_zip >> v;
                assert(!ids_zip.eof());
                assert( i*2 == v);
            }

            PrintResult(eDecompress, kUnknownErr, str.size(), kUnknown, kUnknown);
            // Check EOF
            ids_zip >> v;
            assert(ids_zip.eof());
        }
        OK;
    }}

    //------------------------------------------------------------------------
    // Manipulators: char* / string
    //
    // Compress data from char* to stream. 
    // Decompress data from stream to string.
    //------------------------------------------------------------------------

    if (m_AllowIstrstream && m_AllowOstrstream)
    {{
        ERR_POST(Trace << "Manipulators: string test...");
        INIT_BUFFERS;
        TCompression c;
        CNcbiOstrstream os_str;

        // Compress data using manipulator.
        // The 'src_buf' is zero-terminated and have only printable characters.

#if defined(HAVE_LIBBZ2)
        if (method == M::eBZip2) {
            os_str << MCompress_BZip2 << src_buf;
        }
#endif
#if defined(HAVE_LIBLZO)
        if (method == M::eLZO) {
            os_str << MCompress_LZO << src_buf;
            // For LZO we should use fStreamFormat flag for DecompressBuffer()
            // method to decompress data compressed using streams/manipulators.
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat);
        }
#endif
#if defined(HAVE_LIBZ)
        if (method == M::eZip) {
            os_str << MCompress_Zip << src_buf;
        }
#endif
#if defined(HAVE_LIBZCF)
        if (method == M::eZipCloudflare) {
            os_str << MCompress_ZipCloudflare << src_buf;
        }
#endif
#if defined(HAVE_LIBZSTD)
        if (method == M::eZstd) {
            os_str << MCompress_Zstd << src_buf;
        }
#endif
        string str = CNcbiOstrstreamToString(os_str);
        PrintResult(eCompress, kUnknownErr, src_len, kUnknown, str.size());
        // Decompress data and compare with original
        result = c.DecompressBuffer(str.data(), str.size(), cmp_buf, buf_len, &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), str.size(), buf_len, out_len);
        assert(result);
        assert(out_len == src_len);
        assert(memcmp(src_buf, cmp_buf, out_len) == 0);

        // Decompress data using manipulator
        CNcbiIstrstream is_cmp(str);
        string str_cmp;
        INIT_BUFFERS;
#if defined(HAVE_LIBBZ2)
        if (method == M::eBZip2) {
            is_cmp >> MDecompress_BZip2 >> str_cmp;
        }
#endif
#if defined(HAVE_LIBLZO)
        if (method == M::eLZO) {
            is_cmp >> MDecompress_LZO >> str_cmp;
        }
#endif
#if defined(HAVE_LIBZ)
        if (method == M::eZip) {
            is_cmp >> MDecompress_Zip >> str_cmp;
        }
#endif
#if defined(HAVE_LIBZCF)
        if (method == M::eZipCloudflare) {
            is_cmp >> MDecompress_ZipCloudflare >> str_cmp;
        }
#endif
#if defined(HAVE_LIBZSTD)
        if (method == M::eZstd) {
            is_cmp >> MDecompress_Zstd >> str_cmp;
        }
#endif
        str = str_cmp.data();
        out_len = str_cmp.length();
        PrintResult(eDecompress, kUnknownErr, str.size(), kUnknown, out_len);
        // Compare original and decompressed data
        assert(out_len == src_len);
        assert(memcmp(src_buf, str.data(), out_len) == 0);

        // Done
        OK;
    }}

    //------------------------------------------------------------------------
    // Manipulators: strstream
    //
    // Compress data from istrstream to ostrstream. 
    // Decompress data from istrstream to ostrstream.
    //------------------------------------------------------------------------

    if (m_AllowIstrstream && m_AllowOstrstream)
    {{
        ERR_POST(Trace << "Manipulators: strstream test...");

        TCompression c;
#if defined(HAVE_LIBLZO)
        if (method == M::eLZO) {
            // For LZO we should use fStreamFormat flag for CompressBuffer()
            // method, because we will decompress it using decompression
            // stream.
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat);
        }
#endif
        // Manipulators and operator<<
        {{
            INIT_BUFFERS;
            string src(src_buf, src_len);
            CNcbiIstrstream is_str(src);
            CNcbiOstrstream os_str;

#if defined(HAVE_LIBBZ2)
            if (method == M::eBZip2) {
                os_str << MCompress_BZip2 << is_str;
            }
#endif
#if defined(HAVE_LIBLZO)
            if (method == M::eLZO) {
                os_str << MCompress_LZO << is_str;
            }
#endif
#if defined(HAVE_LIBZ)
            if (method == M::eZip) {
                os_str << MCompress_Zip << is_str;
            }
#endif
#if defined(HAVE_LIBZCF)
            if (method == M::eZipCloudflare) {
                os_str << MCompress_ZipCloudflare << is_str;
            }
#endif
#if defined(HAVE_LIBZSTD)
            if (method == M::eZstd) {
                os_str << MCompress_Zstd << is_str;
            }
#endif
            string str = CNcbiOstrstreamToString(os_str);
            size_t os_str_len = str.size();
            PrintResult(eCompress, kUnknownErr, src_len, kUnknown, os_str_len);
            // Decompress data and compare with original
            result = c.DecompressBuffer(str.data(), os_str_len, cmp_buf, buf_len, &out_len);
            PrintResult(eDecompress, c.GetErrorCode(), os_str_len, buf_len, out_len);
            assert(result);
            assert(out_len == src_len);
            assert(memcmp(src_buf, cmp_buf, out_len) == 0);

            // Decompress data using manipulator and << operator
            INIT_BUFFERS;
            CNcbiIstrstream is_cmp(str);
            CNcbiOstrstream os_cmp;
#if defined(HAVE_LIBBZ2)
            if (method == M::eBZip2) {
                os_cmp << MDecompress_BZip2 << is_cmp;
            }
#endif
#if defined(HAVE_LIBLZO)
            if (method == M::eLZO) {
                os_cmp << MDecompress_LZO << is_cmp;
            }
#endif
#if defined(HAVE_LIBZ)
            if (method == M::eZip) {
                os_cmp << MDecompress_Zip << is_cmp;
            }
#endif
#if defined(HAVE_LIBZCF)
            if (method == M::eZipCloudflare) {
                os_cmp << MDecompress_ZipCloudflare << is_cmp;
            }
#endif
#if defined(HAVE_LIBZSTD)
            if (method == M::eZstd) {
                os_cmp << MDecompress_Zstd << is_cmp;
            }
#endif
            str = CNcbiOstrstreamToString(os_cmp);
            out_len = str.size();
            PrintResult(eDecompress, kUnknownErr, os_str_len, kUnknown, out_len);
            // Compare original and decompressed data
            assert(out_len == src_len);
            assert(memcmp(src_buf, str.data(), out_len) == 0);
        }}

        // Manipulators and operator>>
        {{
            INIT_BUFFERS;
            string src(src_buf, src_len);
            CNcbiIstrstream is_str(src);
            CNcbiOstrstream os_str;
#if defined(HAVE_LIBBZ2)
            if (method == M::eBZip2) {
                is_str >> MCompress_BZip2 >> os_str;
            }
#endif
#if defined(HAVE_LIBLZO)
            if (method == M::eLZO) {
                is_str >> MCompress_LZO >> os_str;
            }
#endif
#if defined(HAVE_LIBZ)
            if (method == M::eZip) {
                is_str >> MCompress_Zip >> os_str;
            }
#endif
#if defined(HAVE_LIBZCF)
            if (method == M::eZipCloudflare) {
                is_str >> MCompress_ZipCloudflare >> os_str;
            }
#endif
#if defined(HAVE_LIBZSTD)
            if (method == M::eZstd) {
                is_str >> MCompress_Zstd >> os_str;
            }
#endif
            string str = CNcbiOstrstreamToString(os_str);
            size_t os_str_len = str.size();
            PrintResult(eCompress, kUnknownErr, src_len, kUnknown, os_str_len);
            // Decompress data and compare with original
            result = c.DecompressBuffer(str.data(), os_str_len, cmp_buf, buf_len, &out_len);
            PrintResult(eDecompress, c.GetErrorCode(), os_str_len, buf_len, out_len);
            assert(result);
            assert(out_len == src_len);
            assert(memcmp(src_buf, cmp_buf, out_len) == 0);

            // Decompress data using manipulator and << operator
            INIT_BUFFERS;
            CNcbiIstrstream is_cmp(str);
            CNcbiOstrstream os_cmp;
#if defined(HAVE_LIBBZ2)
            if (method == M::eBZip2) {
                is_cmp >> MDecompress_BZip2 >> os_cmp;
            }
#endif
#if defined(HAVE_LIBLZO)
            if (method == M::eLZO) {
                is_cmp >> MDecompress_LZO >> os_cmp;
            }
#endif
#if defined(HAVE_LIBZ)
            if (method == M::eZip) {
                is_cmp >> MDecompress_Zip >> os_cmp;
            }
#endif
#if defined(HAVE_LIBZCF)
            if (method == M::eZipCloudflare) {
                is_cmp >> MDecompress_ZipCloudflare >> os_cmp;
            }
#endif
#if defined(HAVE_LIBZSTD)
            if (method == M::eZstd) {
                is_cmp >> MDecompress_Zstd >> os_cmp;
            }
#endif
            str = CNcbiOstrstreamToString(os_cmp);
            out_len = str.size();
            PrintResult(eDecompress, kUnknownErr, os_str_len, kUnknown, out_len);
            // Compare original and decompressed data
            assert(out_len == src_len);
            assert(memcmp(src_buf, str.data(), out_len) == 0);
        }}

        OK;
    }}

    //------------------------------------------------------------------------
    // Manipulators: fstream
    //
    // Compress data from istrstream to fstream. 
    // Decompress data from fstream to ostrstream.
    //------------------------------------------------------------------------

    if (m_AllowIstrstream && m_AllowOstrstream)
    {{
        ERR_POST(Trace << "Manipulators: fstream test...");
        INIT_BUFFERS;

        TCompression c;

        // Preparation

#if defined(HAVE_LIBLZO)
        if (method == M::eLZO) {
            // For LZO we should use fStreamFormat flag for CompressBuffer()
            // method, because we will decompress it using decompression
            // stream.
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat);
        }
#endif
#if defined(HAVE_LIBZ)
        if (method == M::eZip || method == M::eZipCloudflare) {
            /// Set of flags for gzip file format support
            c.SetFlags(c.GetFlags() | CZipCompression::fGZip);
        }
#endif
        // Compress data into the file
        string src(src_buf, src_len);
        CNcbiIstrstream is_str(src);
        CNcbiOfstream os(kFileName, ios::out | ios::binary);
        assert(os.good());

#if defined(HAVE_LIBBZ2)
        if (method == M::eBZip2) {
            os << MCompress_BZip2 << is_str;
        }
#endif
#if defined(HAVE_LIBLZO)
        if (method == M::eLZO) {
            os << MCompress_LZO << is_str;
        }
#endif
#if defined(HAVE_LIBZ)
        if (method == M::eZip) {
            os << MCompress_GZipFile << is_str;
        }
#endif
#if defined(HAVE_LIBZCF)
        if (method == M::eZipCloudflare) {
            os << MCompress_GZipCloudflareFile << is_str;
        }
#endif
#if defined(HAVE_LIBZSTD)
        if (method == M::eZstd) {
            os << MCompress_Zstd << is_str;
        }
#endif
        os.close();
        dst_len = (size_t)CFile(kFileName).GetLength();
        assert(dst_len > 0);
        PrintResult(eCompress, kUnknownErr, src_len, kUnknown, dst_len);

        // Decompress file and compare result with original data
        CNcbiOstrstream os_cmp;
        CNcbiIfstream is(kFileName, ios::in | ios::binary);
        assert(is.good());

#if defined(HAVE_LIBBZ2)
        if (method == M::eBZip2) {
            is >> MDecompress_BZip2 >> os_cmp;
        }
#endif
#if defined(HAVE_LIBLZO)
        if (method == M::eLZO) {
            is >> MDecompress_LZO >> os_cmp;
        }
#endif
#if defined(HAVE_LIBZ)
        if (method == M::eZip) {
            is >> MDecompress_GZipFile >> os_cmp;
        }
#endif
#if defined(HAVE_LIBZCF)
        if (method == M::eZipCloudflare) {
            is >> MDecompress_GZipCloudflareFile >> os_cmp;
        }
#endif
#if defined(HAVE_LIBZSTD)
        if (method == M::eZstd) {
            is >> MDecompress_Zstd >> os_cmp;
        }
#endif
        string str = CNcbiOstrstreamToString(os_cmp);
        out_len = str.size();
        PrintResult(eDecompress, kUnknownErr, dst_len, kUnknown, out_len);
        // Compare original and decompressed data
        assert(out_len == src_len);
        assert(memcmp(src_buf, str.data(), out_len) == 0);
        is.close();

        // Done
        CFile(kFileName).Remove();
        OK;
    }}
