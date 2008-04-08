    AutoArray<char> dst_buf_arr(kBufLen);
    AutoArray<char> cmp_buf_arr(kBufLen);
    char* dst_buf = dst_buf_arr.get();
    char* cmp_buf = cmp_buf_arr.get();

    size_t dst_len, out_len;
    bool result;

    assert(dst_buf);
    assert(cmp_buf);

    // The fAllowTransparentRead should be the same for all compressors.
    
    _VERIFY((unsigned int)CZipCompression::fAllowTransparentRead == 
            (unsigned int)CBZip2Compression::fAllowTransparentRead);
#if defined(HAVE_LIBLZO)
    _VERIFY((unsigned int)CZipCompression::fAllowTransparentRead == 
            (unsigned int)CLZOCompression::fAllowTransparentRead);
#endif

    //------------------------------------------------------------------------
    // Version info
    //------------------------------------------------------------------------

    CVersionInfo version = TCompression().GetVersion();
    _TRACE("Compression library name and version: " << version.Print());

    // zlib v1.2.2 and earlier have a bug in decoding. In some cases
    // decompressor can produce output data on invalid compressed data.
    // So, we do not run such tests if zlib version < 1.2.3.
    bool allow_transparent_read_test = 
            version.GetName() != "zlib"  || 
            version.IsUpCompatible(CVersionInfo(1,2,3));

    _TRACE("Transparent read tests are " << 
           (allow_transparent_read_test ? "" : "not ") << "allowed.\n");

    //------------------------------------------------------------------------
    // Compress/decomress buffer
    //------------------------------------------------------------------------
    {{
        _TRACE("Compress/decompress buffer test (default level)...");
        INIT_BUFFERS;

        // Compress data
        TCompression c(CCompression::eLevel_Medium);
        result = c.CompressBuffer(src_buf, kDataLen, dst_buf, kBufLen,
                                  &out_len);
        PrintResult(eCompress, c.GetErrorCode(), kDataLen, kBufLen, out_len);
        assert(result);

        // Decompress data
        dst_len = out_len;
        result = c.DecompressBuffer(dst_buf, dst_len, cmp_buf, kBufLen,
                                    &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), dst_len, kBufLen,out_len);
        assert(result);
        assert(out_len == kDataLen);

        // Compare original and decompressed data
        assert(memcmp(src_buf, cmp_buf, out_len) == 0);
        OK;
    }}

    //------------------------------------------------------------------------
    // Compress/decomress buffer with CRC32 checksum (LZO only)
    //------------------------------------------------------------------------

#if defined(HAVE_LIBLZO)
    if (version.GetName() == "lzo")
    {{
        _TRACE("Compress/decompress buffer test (LZO, CRC32, default level)...");
        INIT_BUFFERS;

        // Compress data
        TCompression c(CCompression::eLevel_Best);
        c.SetFlags(c.GetFlags() | CLZOCompression::fChecksum);
        result = c.CompressBuffer(src_buf, kDataLen, dst_buf, kBufLen,
                                  &out_len);
        PrintResult(eCompress, c.GetErrorCode(), kDataLen, kBufLen, out_len);
        assert(result);

        // Decompress data
        dst_len = out_len;
        result = c.DecompressBuffer(dst_buf, dst_len, cmp_buf, kBufLen,
                                    &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), dst_len, kBufLen,out_len);
        assert(result);
        assert(out_len == kDataLen);

        // Compare original and decompressed data
        assert(memcmp(src_buf, cmp_buf, out_len) == 0);
        OK;
    }}
#endif

    //------------------------------------------------------------------------
    // Overflow test
    //------------------------------------------------------------------------
    {{
        _TRACE("Output buffer overflow test...");

        TCompression c;
        dst_len = 5;
        result = c.CompressBuffer(src_buf, kDataLen,
                                  dst_buf, dst_len, &out_len);
        PrintResult(eCompress, c.GetErrorCode(), kDataLen, dst_len, out_len);
        assert(!result);
        // The lzo decoder produce nothing in the buffer overflow case,
        // bzip2 and zlib can produce some data.
        assert(out_len == 0  ||  out_len == dst_len);
        OK;
    }}


    //------------------------------------------------------------------------
    // Decompress buffer: transparent read
    //------------------------------------------------------------------------
    if (allow_transparent_read_test)
    {{
        _TRACE("Decompress buffer (transparent read)...");
        INIT_BUFFERS;

        TCompression c(CCompression::eLevel_Medium);
        c.SetFlags(CZipCompression::fAllowTransparentRead);

        // "Decompress" source buffer, which is uncompressed
        result = c.DecompressBuffer(src_buf, kDataLen, dst_buf, kBufLen,
                                    &dst_len);
        PrintResult(eDecompress, c.GetErrorCode(), kDataLen, kBufLen, dst_len);
        assert(result);
        // Should have the same buffer as output
        assert(dst_len == kDataLen);
        assert(memcmp(src_buf, dst_buf, dst_len) == 0);

        // Overflow test
        dst_len = 5;
        result = c.DecompressBuffer(src_buf, kDataLen, dst_buf, dst_len,
                                    &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), kDataLen, dst_len,out_len);
        assert(!result);
        assert(dst_len == out_len);

        OK;
    }}

    //------------------------------------------------------------------------
    // File compression/decompression test
    //------------------------------------------------------------------------
    {{
        _TRACE("File compress/decompress test...");
        size_t n;

        // First test -- write 1k blocks and read in bulk

        INIT_BUFFERS;
        {{
            {{
                TCompressionFile zf;
                // Compressing data and write it to the file
                assert(zf.Open(kFileName, TCompressionFile::eMode_Write)); 
                size_t i;
                for (i = 0; i < kDataLen/1024; i++) {
                    n = zf.Write(src_buf + i*1024, 1024);
                    assert(n == 1024);
                }
                n = zf.Write(src_buf + i*1024, kDataLen % 1024);
                assert(n == kDataLen % 1024);

                assert(zf.Close()); 
                assert(CFile(kFileName).GetLength() > 0);
            }}
            {{
                TCompressionFile zf;
#if defined(HAVE_LIBLZO)
                // The blocksize and flags stored in the header should be
                // used for decompression instead of the values passed
                // as parameters.
                // Add bogus flag to check CRC32 checksum
                // (compressed data doesn't have it).
                zf.SetFlags(zf.GetFlags() | CLZOCompression::fChecksum);
#endif
                // Decompress data from file
                assert(zf.Open(kFileName, TCompressionFile::eMode_Read)); 
                assert(zf.Read(cmp_buf, kDataLen) == (int)kDataLen);
                assert(zf.Close()); 

                // Compare original and decompressed data
                assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);
            }}
        }}

        // Second test -- write in bulk and read by 100 bytes

        INIT_BUFFERS;
        {{
            {{
                // Write data with compression into the file
                TCompressionFile zf(kFileName, TCompressionFile::eMode_Write,
                                    CCompression::eLevel_Best);
                n = zf.Write(src_buf, kDataLen);
                assert(n == kDataLen);
            }}
            {{
                // Read data from compressed file
                TCompressionFile zf(kFileName, TCompressionFile::eMode_Read);
                size_t nread = 0;
                do {
                    n = zf.Read(cmp_buf + nread, 100);
                    nread += n;
                } while ( n != 0 );
                assert(nread == kDataLen);
            }}

            // Compare original and decompressed data
            assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);
        }}
        CFile(kFileName).Remove();
        OK;
    }}

    //------------------------------------------------------------------------
    // File decompression: transparent read test
    //------------------------------------------------------------------------
    if (allow_transparent_read_test)
    {{
        _TRACE("Decompress file (transparent read)...");
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
        size_t n = zf.Write(src_buf, kDataLen);
        assert(n == kDataLen);
        assert(zf.Close()); 
        assert(CFile(kFileName).GetLength() > 0);
        
        // Decompress data from file
        assert(zf.Open(kFileName, TCompressionFile::eMode_Read)); 
        assert(zf.Read(cmp_buf, kDataLen) == (int)kDataLen);
        assert(zf.Close()); 

        // Compare original and decompressed data
        assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);

        //
        // Test to read uncompressed data
        //

        // Create file with uncompressed data
        CNcbiOfstream os(kFileName, IOS_BASE::out | IOS_BASE::binary);
        assert(os.good());
        os.write(src_buf, kDataLen);
        os.close();
        assert(CFile(kFileName).GetLength() == (int)kDataLen);

        // Transparent read from this file
        assert(zf.Open(kFileName, TCompressionFile::eMode_Read)); 
        assert(zf.Read(cmp_buf, kDataLen) == (int)kDataLen);
        assert(zf.Close()); 

        // Compare original and "decompressed" data
        assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);

        CFile(kFileName).Remove();
        OK;
    }}

    //------------------------------------------------------------------------
    // Compression input stream test
    //------------------------------------------------------------------------
    {{
        _TRACE("Compression input stream test...");
        INIT_BUFFERS;

        // Compression input stream test 
        CNcbiIstrstream is_str(src_buf, kDataLen);
        CCompressionIStream ics_zip(is_str, new TStreamCompressor(),
                                    CCompressionStream::fOwnProcessor);
        
        // Read compressed data from stream
        ics_zip.read(dst_buf, kReadMax);
        dst_len = ics_zip.gcount();
        // We should have all packed data here, because compressor
        // finalization for input streams accomplishes automaticaly.
        PrintResult(eCompress, kUnknownErr, kDataLen, kUnknown, dst_len);
        assert(ics_zip.GetProcessedSize() == kDataLen);
        assert(ics_zip.GetOutputSize() == dst_len);

        // Decompress data
        TCompression c;
#if defined(HAVE_LIBLZO)
        if (version.GetName() == "lzo") {
            // For LZO we should use fStreamFormat flag for DecompressBuffer()
            // method to decompress data, compressed using streams.
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat);
        }
#endif
        result = c.DecompressBuffer(dst_buf, dst_len,
                                    cmp_buf, kBufLen, &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), dst_len, kBufLen, out_len);
        assert(result);

        // Compare original and uncompressed data
        assert(out_len == kDataLen);
        assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);
        OK;
    }}

    //------------------------------------------------------------------------
    // Decompression input stream test
    //------------------------------------------------------------------------
    {{
        _TRACE("Decompression input stream test...");
        INIT_BUFFERS;

        // Compress data and create test stream
        TCompression c;
#if defined(HAVE_LIBLZO)
        if (version.GetName() == "lzo") {
            // For LZO we should use fStreamFormat flag for CompressBuffer()
            // method, because we will decompress it using decompression
            // stream.
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat);
        }
#endif
        result = c.CompressBuffer(src_buf, kDataLen,
                                  dst_buf, kBufLen, &dst_len);
        PrintResult(eCompress, c.GetErrorCode(), kDataLen, kBufLen, dst_len);
        assert(result);
        CNcbiIstrstream is_str(dst_buf, dst_len);

        // Read decompressed data from stream
        CCompressionIStream ids_zip(is_str, new TStreamDecompressor(),
                                    CCompressionStream::fOwnReader);
        ids_zip.read(cmp_buf, kReadMax);
        out_len = ids_zip.gcount();
        // We should have all packed data here, because compressor
        // finalization for input streams accompishes automaticaly.
        PrintResult(eDecompress, kUnknownErr, dst_len, kBufLen, out_len);
        assert(ids_zip.GetProcessedSize() == dst_len);
        assert(ids_zip.GetOutputSize() == kDataLen);

        // Compare original and uncompressed data
        assert(out_len == kDataLen);
        assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);
        OK;
    }}

    //------------------------------------------------------------------------
    // Compression output stream test
    //------------------------------------------------------------------------
    {{
        _TRACE("Compression output stream test...");
        INIT_BUFFERS;

        // Write data to compressing stream
        CNcbiOstrstream os_str;
        {{
            CCompressionOStream os_zip(os_str, new TStreamCompressor(),
                                       CCompressionStream::fOwnWriter);
            os_zip.write(src_buf, kDataLen);
            // Finalize compression stream in the destructor.
            // The output stream os_str will receive all data only after
            // finalization.
            // You can also call os_zip.Finalize() directly at any time.
        }}
        // Get compressed size
        const char* str = os_str.str();
        size_t os_str_len = os_str.pcount();
        PrintResult(eCompress, kUnknownErr, kDataLen, kBufLen, os_str_len);

        // Try to decompress data
        TCompression c;
#if defined(HAVE_LIBLZO)
        if (version.GetName() == "lzo") {
            // For LZO we should use fStreamFormat flag for DecompressBuffer()
            // method to decompress data compressed inside compression stream.
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat);
        }
#endif
        result = c.DecompressBuffer(str, os_str_len, cmp_buf, kBufLen,
                                    &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), os_str_len, kBufLen,
                    out_len);
        assert(result);

        // Compare original and decompressed data
        assert(out_len == kDataLen);
        assert(memcmp(src_buf, cmp_buf, out_len) == 0);
        os_str.rdbuf()->freeze(0);
        OK;
    }}

    //------------------------------------------------------------------------
    // Decompression output stream test
    //------------------------------------------------------------------------
    {{
        _TRACE("Decompression output stream test...");
        INIT_BUFFERS;

        // Compress the data
        TCompression c;
#if defined(HAVE_LIBLZO)
        if (version.GetName() == "lzo") {
            // For LZO we should use fStreamFormat flag for CompressBuffer()
            // method for following decompress of data using compression
            // stream.
            c.SetFlags(c.GetFlags() | CLZOCompression::fStreamFormat | 
                       CLZOCompression::fChecksum);
        }
#endif
        result = c.CompressBuffer(src_buf, kDataLen, dst_buf, kBufLen,
                                  &out_len);
        PrintResult(eCompress, c.GetErrorCode(), kDataLen, kBufLen, out_len);
        assert(result);

        // Write compressed data to decompressing stream
        CNcbiOstrstream os_str;
        CCompressionOStream os_zip(os_str, new TStreamDecompressor(),
                                   CCompressionStream::fOwnProcessor);

#if defined(HAVE_LIBLZO)
        // The blocksize and flags stored in the header should be used
        // instead of the value passed in the parameters for decompressor.
        c.SetFlags(c.GetFlags() | CLZOCompression::fChecksum);
#endif

        os_zip.write(dst_buf, out_len);
        // Finalize a compression stream via direct call Finalize().
        os_zip.Finalize();

        // Get decompressed size
        const char*  str = os_str.str();
        size_t os_str_len = os_str.pcount();
        PrintResult(eDecompress, kUnknownErr, out_len, kBufLen, os_str_len);
        assert(os_zip.GetProcessedSize() == out_len);
        assert(os_zip.GetOutputSize() == kDataLen);

        // Compare original and uncompressed data
        assert(os_str_len == kDataLen);
        assert(memcmp(src_buf, str, os_str_len) == 0);
        os_str.rdbuf()->freeze(0);
        OK;
    }}

    //------------------------------------------------------------------------
    // Decompression input stream test: transparent read
    //------------------------------------------------------------------------
    if (allow_transparent_read_test)
    {{
        _TRACE("Decompression input stream test (transparent read)...");
        INIT_BUFFERS;

        // Create test input stream with uncompressed data
        CNcbiIstrstream is_str(src_buf, kDataLen);

        // Create decompressor and set flags to allow transparent read
        TStreamDecompressor 
            decompressor(CZipCompression::fAllowTransparentRead);

        // Read uncompressed data from stream
        CCompressionIStream ids_zip(is_str, &decompressor);

        ids_zip.read(dst_buf, kReadMax);
        out_len = ids_zip.gcount();
        PrintResult(eDecompress, kUnknownErr, kDataLen, kBufLen, out_len);
        assert(ids_zip.GetProcessedSize() == out_len);
        assert(ids_zip.GetOutputSize() == out_len);

        // Compare original and uncompressed data
        assert(out_len == kDataLen);
        assert(memcmp(src_buf, dst_buf, kDataLen) == 0);
        OK;
    }}

    //------------------------------------------------------------------------
    // I/O stream tests
    //------------------------------------------------------------------------

    {{
        _TRACE("I/O stream tests...");
        {{
            INIT_BUFFERS;

            CNcbiStrstream stm(dst_buf, (int)kBufLen,
                               IOS_BASE::in|IOS_BASE::out|IOS_BASE::binary);
            CCompressionIOStream zip(stm, new TStreamDecompressor(),
                                          new TStreamCompressor(),
                                          CCompressionStream::fOwnProcessor);
            assert((bool)zip  &&  zip.good()  &&  stm.good());
            zip.write(src_buf, kDataLen);
            assert((bool)zip  &&  zip.good()  &&  stm.good());
            zip.Finalize(CCompressionStream::eWrite);
            assert(!zip.eof()  &&  zip.good());
            assert(!stm.eof()  &&  stm.good());
            assert(zip.GetProcessedSize(CCompressionStream::eWrite)
                   == kDataLen);
            assert(zip.GetProcessedSize(CCompressionStream::eRead) == 0);
            assert(zip.GetOutputSize(CCompressionStream::eWrite) > 0);
            assert(zip.GetOutputSize(CCompressionStream::eRead) == 0);

            // Read data
            zip.read(cmp_buf, kDataLen);
            out_len = zip.gcount();
            assert(!stm.eof()  &&  stm.good());
            assert(!zip.eof()  &&  zip.good());
            assert(out_len == kDataLen);
            assert(zip.GetProcessedSize(CCompressionStream::eWrite)
                   == kDataLen);
            assert(zip.GetProcessedSize(CCompressionStream::eRead) > 0);
            assert(zip.GetOutputSize(CCompressionStream::eWrite) > 0);
            assert(zip.GetOutputSize(CCompressionStream::eRead) == kDataLen);

            // Check on EOF
            char c; 
            zip >> c;
            assert(zip.eof());
            assert(!zip);

            // Compare buffers
            assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);
        }}
        
        // Second test
        {{
            const int kCount[2] = {1, 1000};
            for (int k = 0; k < 2; k++) {
                int n = kCount[k];

                INIT_BUFFERS;
                CNcbiStrstream stm(dst_buf, (int)kBufLen);
                CCompressionIOStream zip(stm,
                                         new TStreamDecompressor(),
                                         new TStreamCompressor(),
                                         CCompressionStream::fOwnProcessor);
                int v;
                for (int i = 0; i < n; i++) {
                    v = i * 2;
                    zip << v << endl;
                }
                zip.Finalize(CCompressionStream::eWrite);
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
    {{
        _TRACE("Advanced I/O stream test...");
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
                for (int i = 0; i < n; i++) {
                    v = i * 2;
                    ocs_zip << v << endl;
                }
            }}
            const char* str = os_str.str();
            size_t os_str_len = os_str.pcount();
            PrintResult(eCompress, kUnknownErr, kUnknown, kUnknown,os_str_len);

            // Decompress data from input stream
            CNcbiIstrstream is_str(str, os_str_len);
            CCompressionIStream ids_zip(is_str, &decompressor);
            for (int i = 0; i < n; i++) {
                ids_zip >> v;
                assert(!ids_zip.eof());
                assert( i*2 == v);
            }

            PrintResult(eDecompress, kUnknownErr, os_str_len, kUnknown,
                        kUnknown);
            // Check EOF
            ids_zip >> v;
            assert(ids_zip.eof());
            os_str.rdbuf()->freeze(0);
        }
        OK;
    }}
    //------------------------------------------------------------------------
