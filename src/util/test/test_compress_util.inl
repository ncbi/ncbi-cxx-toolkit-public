//============================================================================
//
// Common constants and auxiliary functions for ST and MT tests
//
//============================================================================


// Error values for PrintResult()
const int           kUnknownErr   = kMax_Int;
const unsigned int  kUnknown      = kMax_UInt;

// Use stream format for LZO if buffer size exceed specified value
const size_t        LZO_kMaxBlockSize = kMax_UInt; 


//
// CCompressionFile and all derived classes limited to 'long' type range
// for reading/writing. Use these wrappers to process bigger data.
//

template<typename T> size_t ReadCompressionFile(T& file, void* buf, size_t len)
{
    char* ptr = (char*)buf;
    while (len) {
        long n = file.Read(ptr, len);
        if (n <= 0) {
            return ptr - (char*)buf;
        }
        len -= (size_t)n;
        ptr += n;
    }
    return ptr - (char*)buf;
}

template<typename T> size_t WriteCompressionFile(T& file, const void* buf, size_t len)
{
    char* ptr = (char*)buf;
    while (len) {
        long n = file.Write(ptr, len);
        if (n <= 0) {
            return ptr - (char*)buf;
        }
        len -= (size_t)n;
        ptr += n;
    }
    return ptr - (char*)buf;
}



//////////////////////////////////////////////////////////////////////////////
//
// Auxiliary methods for CTest
//

CNcbiIos* CTest::x_CreateIStream(const string& filename, const string& src, size_t buf_len)
{
    CNcbiIos* stm = nullptr;

    if ( m_AllowIstrstream ) {
        stm = new CNcbiIstrstream(src);
    } else {
        // Create file with uncompressed data,
        // or reuse m_SrcFile if we have it already (big data test)
        string fname;
        if ( !m_SrcFile.empty() && 
             (src.size() == buf_len) /* this is possible for uncompressed data only */) {
            fname = m_SrcFile;
        } else {
            fname = filename;
            x_CreateFile(fname, src.data(), src.size());
        }
        stm = new CNcbiIfstream(_T_XCSTRING(fname), ios::in | ios::binary);
    }
    assert(stm->good());
    return stm;
}


void CTest::x_CreateFile(const string& filename, const char* buf, size_t len)
{
    CFileIO f;
    f.Open(filename, CFileIO::eCreate, CFileIO::eReadWrite);
    size_t n = f.Write(buf, len);
    assert(n == len);
}
