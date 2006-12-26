    AutoArray<char> dst_buf_arr(kBufLen);
    AutoArray<char> cmp_buf_arr(kBufLen);
    char* dst_buf = dst_buf_arr.get();
    char* cmp_buf = cmp_buf_arr.get();

    size_t dst_len, out_len;
    bool result;

    assert(dst_buf);
    assert(cmp_buf);


    //------------------------------------------------------------------------
    // Compress/decomress buffer
    //------------------------------------------------------------------------
    {{
        LOG_POST("Testing default level compression...");
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
    // Overflow test
    //------------------------------------------------------------------------
    {{
        LOG_POST("Output buffer overflow test...");

        TCompression c;
        dst_len = 100;
        result = c.CompressBuffer(src_buf, kDataLen,
                                  dst_buf, dst_len, &out_len);
        PrintResult(eCompress, c.GetErrorCode(), kDataLen, dst_len, out_len);
        assert(!result);
        assert(out_len == dst_len);
        OK;
    }}

    //------------------------------------------------------------------------
    // Decompress buffer: transparent read
    //------------------------------------------------------------------------
    {{
        LOG_POST("Decompress buffer (transparent read)...");
        INIT_BUFFERS;

        _VERIFY(CZipCompression::fAllowTransparentRead == 
                CBZip2Compression::fAllowTransparentRead);

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
        dst_len = 100;
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
        LOG_POST("File compress/decompress test...");
        size_t n;

        // First test

        INIT_BUFFERS;
        {{
            TCompressionFile zf;

            // Compressing data and write it to the file
            assert(zf.Open(kFileName, TCompressionFile::eMode_Write)); 
            for (size_t i=0; i < kDataLen/1024; i++) {
                n = zf.Write(src_buf + i*1024, 1024);
                assert(n == 1024);
            }
            assert(zf.Close()); 
            assert(CFile(kFileName).GetLength() > 0);
            
            // Decompress data from file
            assert(zf.Open(kFileName, TCompressionFile::eMode_Read)); 
            assert(zf.Read(cmp_buf, kDataLen) == (int)kDataLen);
            assert(zf.Close()); 

            // Compare original and decompressed data
            assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);
        }}

        // Second test

        INIT_BUFFERS;
        {{
            {{
                // Write data with compression into the file
                TCompressionFile zf(kFileName, TCompressionFile::eMode_Write,
                                    CCompression::eLevel_Best);
                n = zf.Write(src_buf, kDataLen);
                assert(n == (int)kDataLen);
            }}
            {{
                // Read data from compressed file
                TCompressionFile zf(kFileName, TCompressionFile::eMode_Read);
                size_t nread = 0;
                do {
                    n = zf.Read(cmp_buf + nread, 100);
                    nread += n;
                } while ( n != 0 );
                assert(nread == (int)kDataLen);
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
    {{
        LOG_POST("Decompress file - transparent read...");
        INIT_BUFFERS;

        TCompressionFile zf;
        // Set flag to allow transparent read on decompression
        zf.SetFlags(CZipCompression::fAllowTransparentRead |
                    CZipCompression::fCheckFileHeader);

        //
        // Test for usual compression/decompression
        //

        // Compress data to file
        assert(zf.Open(kFileName, TCompressionFile::eMode_Write)); 
        size_t n = zf.Write(src_buf, kDataLen);
        assert(n == (int)kDataLen);
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
        assert(CFile(kFileName).GetLength() == kDataLen);

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
        LOG_POST("Testing compression input stream...");
        INIT_BUFFERS;

        // Compression input stream test 
        CNcbiIstrstream is_str(src_buf, kDataLen);
        CCompressionIStream ics_zip(is_str, new TStreamCompressor(),
                                    CCompressionStream::fOwnProcessor);
        
        // Read compressed data from stream
        ics_zip.read(dst_buf, kDataLen);
        dst_len = ics_zip.gcount();
        // We should have all packed data here, because compressor
        // finalization for input streams accompishes automaticaly.
        PrintResult(eCompress, kUnknownErr, kDataLen, kUnknown, dst_len);
        assert(ics_zip.GetProcessedSize() == kDataLen);
        assert(ics_zip.GetOutputSize() == dst_len);

        // Decompress data
        TCompression c;
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
        LOG_POST("Testing decompression input stream...");
        INIT_BUFFERS;

        // Compress data and create test stream
        TCompression c;
        result = c.CompressBuffer(src_buf, kDataLen,
                                  dst_buf, kBufLen, &dst_len);
        PrintResult(eCompress, c.GetErrorCode(), kDataLen, kBufLen, dst_len);
        assert(result);
        CNcbiIstrstream is_str(dst_buf, dst_len);

        // Read decompressed data from stream
        CCompressionIStream ids_zip(is_str, new TStreamDecompressor(),
                                    CCompressionStream::fOwnReader);
        ids_zip.read(cmp_buf, kDataLen);
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
        LOG_POST("Testing compression output stream...");
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
        LOG_POST("Testing decompression output stream...");
        INIT_BUFFERS;

        // Compress the data
        TCompression c;
        result = c.CompressBuffer(src_buf, kDataLen, dst_buf, kBufLen,
                                  &out_len);
        PrintResult(eCompress, c.GetErrorCode(), kDataLen, kBufLen, out_len);
        assert(result);

        // Write compressed data to decompressing stream
        CNcbiOstrstream os_str;

        CCompressionOStream os_zip(os_str, new TStreamDecompressor(),
                                   CCompressionStream::fOwnProcessor);

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
    {{
        LOG_POST("Testing decompression input stream (transparent read)...");
        INIT_BUFFERS;

        // Create test input stream with uncompressed data
        CNcbiIstrstream is_str(src_buf, kDataLen);

        // Create decompressor and set flags to allow transparent read
        TStreamDecompressor 
            decompressor(CZipCompression::fAllowTransparentRead);

        // Read uncompressed data from stream
        CCompressionIStream ids_zip(is_str, &decompressor);

        ids_zip.read(dst_buf, kDataLen);
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
    // IO stream tests
    //------------------------------------------------------------------------
    {{
        LOG_POST("Testing IO stream...");
        {{
            INIT_BUFFERS;

            CNcbiStrstream stm(dst_buf, (int)kBufLen);
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

            // Read as much as possible
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
        {{
            INIT_BUFFERS;

            CNcbiStrstream stm(dst_buf, (int)kBufLen);
            CCompressionIOStream zip(stm, new TStreamDecompressor(),
                                          new TStreamCompressor(),
                                          CCompressionStream::fOwnProcessor);

            int v;
            for (int i = 0; i < 1000; i++) {
                 v = i * 2;
                 zip << v << endl;
            }
            zip.Finalize(CCompressionStream::eWrite);
            zip.clear();

            for (int i = 0; i < 1000; i++) {
                 zip >> v;
                 assert(!zip.eof());
                 assert(v == i * 2);
            }
            zip.Finalize();
            zip >> v;
            assert(zip.eof());
        }}
        OK;
    }}

    //------------------------------------------------------------------------
    // Advanced I/O stream test:
    //      - read/write;
    //      - reusing compression/decompression stream processors.
    //------------------------------------------------------------------------
    {{
        LOG_POST("Advanced I/O stream test...");
        INIT_BUFFERS;

        TStreamCompressor   compressor;
        TStreamDecompressor decompressor;
        int v;

        for (int count = 0; count<5; count++) {
            // Compress data to output stream
            CNcbiOstrstream os_str;
            {{
                CCompressionOStream ocs_zip(os_str, &compressor);
                for (int i = 0; i < 1000; i++) {
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
            for (int i = 0; i < 1000; i++) {
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
