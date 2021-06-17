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
 * Author:  Eugene Vasilchenko, Vladimir Ivanov
 *
 * File Description:  Checksum and hash calculation classes.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>
#include <util/checksum.hpp>

// Use builtin versions of CityHash and FarmHash libraries,
// that compiles as separate files to avoid name clashing.
#include "checksum/cityhash/city.h"
#include "checksum/farmhash/config.h" // need for farmhash.h
#include "checksum/farmhash/farmhash.h"
// And include MurmurHash directly
#include "checksum/murmurhash/MurmurHash2.cxx"
#include "checksum/murmurhash/MurmurHash3.cxx"


#define USE_CRC32C_INTEL // try to use Intel CRC32C instructions

#ifdef USE_CRC32C_INTEL
# undef USE_CRC32C_INTEL // we'll define it again where available
# if defined(NCBI_COMPILER_GCC)  ||  defined(NCBI_COMPILER_ICC) \
    ||  defined(NCBI_COMPILER_ANY_CLANG)
#  if defined(__x86_64__) || defined(__i386__)
#   ifdef HAVE_CPUID_H
#    include <cpuid.h>
#   endif
#   define USE_CRC32C_INTEL
#  endif
#  if defined(__x86_64__)
#   define HAVE_CRC32C_64
#  endif
# elif defined(NCBI_COMPILER_MSVC)
#  if defined(_M_X64) || defined(_M_IX86)
#   include <intrin.h>
#   define USE_CRC32C_INTEL
#  endif
#  if defined(_M_X64)
#   define HAVE_CRC32C_64
#  endif
# endif
#endif


BEGIN_NCBI_SCOPE


static const size_t kCRC32Size = 256;
typedef Uint4 TCRC32Table[kCRC32Size];

// Defines

#define TABLES_COUNT 8
#define NCBI_USE_PRECOMPILED_CRC32_TABLES 1

// sx_Start must begin with "/* O" (see ValidChecksumLine() in checksum.hpp)
static const char sx_Start[]     = "/* Original file checksum: ";
static const char sx_End[]       = " */";
static const char sx_LineCount[] = "lines: ";
static const char sx_CharCount[] = "chars: ";

// Forward declarations

#ifdef NCBI_USE_PRECOMPILED_CRC32_TABLES
    static inline void s_InitTableCRC32Forward()  {}
    static inline void s_InitTableCRC32Reverse()  {}
    static inline void s_InitTableCRC32CReverse() {}
#else
    static void s_InitTableCRC32Forward();
    static void s_InitTableCRC32Reverse();
    static void s_InitTableCRC32CReverse();
#endif //NCBI_USE_PRECOMPILED_CRC32_TABLES

#ifdef USE_CRC32C_INTEL
    static bool s_IsCRC32CIntelEnabled(void);
#endif



CChecksumBase::CChecksumBase(EMethodDef method)
    : m_Method(eNone)
{
    x_Reset(method);
}


CChecksumBase::~CChecksumBase()
{
    x_Free();
}


CChecksumBase::CChecksumBase(const CChecksumBase& other)
    : m_Method(other.m_Method),
      m_CharCount(other.m_CharCount)
{
    if ( m_Method == eMD5 ) {
        m_Value.md5 = new CMD5(*other.m_Value.md5);
    } else {
        m_Value.v64 = other.m_Value.v64;
    }
}


CChecksumBase& CChecksumBase::operator= (const CChecksumBase& other)
{
    if (&other == this){
        return *this;
    }
    x_Free();

    m_Method    = other.m_Method;
    m_CharCount = other.m_CharCount;

    if ( m_Method == eMD5 ) {
        m_Value.md5 = new CMD5(*other.m_Value.md5);
    } else {
        m_Value.v64 = other.m_Value.v64;
    }
    return *this;
}


string CChecksumBase::GetResultHex(void) const
{
    switch (m_Method ) {
    case eMD5:
        return m_Value.md5->GetHexSum();
    default:
        if (GetBits() == 64) {
            return NStr::NumericToString(GetResult64(), 0, 16);
        }
        if (GetBits() == 32) {
            return NStr::NumericToString(GetResult32(), 0, 16);
        }
        _ASSERT(0);
        return kEmptyStr;
    }
}


void CChecksumBase::x_Reset(EMethodDef method)
{
    x_Free();

    m_Method = method;
    m_Value.v64 = 0;
    m_CharCount = 0;

    switch ( method ) {
    case eCRC32:
    case eCRC32CKSUM:
        s_InitTableCRC32Forward();
        break;
    case eCRC32ZIP:
    case eCRC32INSD:
        m_Value.v32 = ~0;
        s_InitTableCRC32Reverse();
        break;
    case eCRC32C:
        m_Value.v32 = ~0;
#ifdef USE_CRC32C_INTEL
        if ( s_IsCRC32CIntelEnabled() ) {
            break;
        }
#endif
        s_InitTableCRC32CReverse();
        break;
    case eAdler32:
        m_Value.v32 = 1;
        break;
    case eMD5:
        m_Value.md5 = new CMD5;
        break;
    case eCityHash32:
    case eCityHash64:
    case eFarmHash32:
    case eFarmHash64:
    case eMurmurHash2_32:
    case eMurmurHash2_64:
    case eMurmurHash3_32:
        break;
    default:
        _ASSERT(0);
    }
}


CHash::CHash(EMethod method)
    : CChecksumBase((EMethodDef)method)
{
}


CHash::CHash(const CHash& other)
    : CChecksumBase(other)
{
}


CHash& CHash::operator= (const CHash& other)
{
    CChecksumBase::operator=(other);
    return *this;
}


/// @sa CHash::SetSeed()
Uint8 CChecksumBase::m_Seed = 0;


void CHash::SetSeed(Uint8 seed)
{
    m_Seed = seed;
}


void CHash::Calculate(const CTempString str, EMethod method, Uint4& hash)
{
    CHash h(method);
    h.Calculate(str);
    hash = h.GetResult32();
}


void CHash::Calculate(const CTempString str, EMethod method, Uint8& hash)
{
    CHash h(method);
    h.Calculate(str);
    hash = h.GetResult64();
}


void CHash::Calculate(const char* str, size_t len, EMethod method, Uint4& hash)
{
    CHash h(method);
    h.Calculate(str, len);
    hash = h.GetResult32();
}


void CHash::Calculate(const char* str, size_t len, EMethod method, Uint8& hash)
{
    CHash h(method);
    h.Calculate(str, len);
    hash = h.GetResult64();
}


CChecksum::CChecksum(EMethod method)
    : CChecksumBase((EMethodDef)method),
      m_LineCount(0)
{
}


CChecksum::CChecksum(const CChecksum& other)
    : CChecksumBase(other),
      m_LineCount(other.m_LineCount)
{
}


CChecksum& CChecksum::operator= (const CChecksum& other)
{
    CChecksumBase::operator=(other);
    m_LineCount = other.m_LineCount;
    return *this;
}


CNcbiOstream& CChecksum::WriteChecksum(CNcbiOstream& out) const
{
    if (!out.good()) {
        return out;
    }
    out << sx_Start
        << sx_LineCount << m_LineCount << ", "
        << sx_CharCount << m_CharCount << ", ";
    WriteChecksumData(out);
    return out << sx_End << '\n';
}


bool CChecksum::ValidChecksumLineLong(const char* line, size_t len) const
{
    CNcbiOstrstream buffer;
    WriteChecksum(buffer);
    string buffer_str = CNcbiOstrstreamToString(buffer);
    if ( buffer_str.size() != len + 1 ) { // account for '\n'
        return false;
    }
    return memcmp(line, buffer_str.data(), len) == 0;
}


CNcbiOstream& CChecksum::WriteHexSum(CNcbiOstream& out) const
{
    if ( GetMethod() == eMD5 ) {
        out << m_Value.md5->GetHexSum();
    } else {
        IOS_BASE::fmtflags flags = out.setf(IOS_BASE::hex, IOS_BASE::basefield);
        out << setprecision(8);
        out << GetChecksum();
        out.flags(flags);
    }
    return out;
}


CNcbiOstream& CChecksum::WriteChecksumData(CNcbiOstream& out) const
{
    switch ( GetMethod() ) {
    case eMD5:
        out << "MD5: ";
        break;
    case eAdler32:
        out << "Adler32: ";
        break;
    case eCRC32:
    case eCRC32ZIP:
    case eCRC32INSD:
    case eCRC32CKSUM:
    case eCRC32C:
        out << "CRC32: ";
        break;
    default:
        _ASSERT(0);
        return out;
    }
    WriteHexSum(out);
    return out;
}


void CChecksum::NextLine(void)
{
    char eol = '\n';
    x_Update(&eol, 1);
    ++m_LineCount;
}


void CChecksum::AddFile(const string& file_path)
{
    CFileIO f;
    try {
        f.Open(file_path, CFileIO::eOpen, CFileIO::eRead);
        CChecksum tmp(*this);
        size_t n;
        char buf[1024 * 8];
        while ((n = f.Read(buf, sizeof(buf))) > 0) {
            tmp.AddChars(buf, n);
        }
        f.Close();
        *this = tmp;
    }
    catch (CFileException& e) {
        f.Close();
        NCBI_RETHROW(e, CChecksumException, eFileIO, "Error add checksum for file: " + file_path);
        throw;
    }
}


void CChecksum::AddStream(CNcbiIstream& is)
{
    if ( is.eof() ) {
        return;
    }
    if ( !is.good() ) {
        NCBI_THROW(CChecksumException, eStreamIO, "Input stream is not good()");
        return;
    }
    CChecksum tmp(*this);

    while ( !is.eof() ) {
        char buf[1024 * 8];
        is.read(buf, sizeof(buf));
        size_t n = (size_t)is.gcount();
        if (n) {
            tmp.AddChars(buf, n);
        } else {
            if (is.fail()  &&  !is.eof()) {
                NCBI_THROW(CChecksumException, eStreamIO, "Error reading from input stream");
                return;
            }
        }
    }
    *this = tmp;
}


// @deprecated
CChecksum& ComputeFileChecksum_deprecated(const string& path, CChecksum& checksum)
{
    CNcbiIfstream is(path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if ( !is.is_open() ) {
        return checksum;
    }
    while ( !is.eof() ) {
        char buf[1024*8];
        is.read(buf, sizeof(buf));
        size_t count = (size_t)is.gcount();
        if ( count ) {
            checksum.AddChars(buf, count);
        }
    }
    is.close();
    return checksum;
}

// @deprecated
CChecksum ComputeFileChecksum(const string& path, CChecksum::EMethod method)
{
    CChecksum checksum(method);
    return ComputeFileChecksum_deprecated(path, checksum);
}

// @deprecated
CChecksum& ComputeFileChecksum(const string& path, CChecksum& checksum)
{
    return ComputeFileChecksum_deprecated(path, checksum);
}

// @deprecated
Uint4 ComputeFileCRC32(const string& path)
{
    CChecksum checksum(CChecksum::eCRC32);
    return ComputeFileChecksum_deprecated(path, checksum).GetChecksum();
}


void CChecksumBase::InitTables(void)
{
    s_InitTableCRC32Forward();
    s_InitTableCRC32Reverse();
    s_InitTableCRC32CReverse();
}


template<size_t kCRC32Tables>
static inline
void s_PrintTable(CNcbiOstream& out, const char* name,
                  const TCRC32Table (&table)[kCRC32Tables])
{
    const size_t kLineSize = 4;
    out << "static const TCRC32Table " << name << "["<<kCRC32Tables<<"] = {";
    for ( size_t k = 0; k < kCRC32Tables; ++k ) {
        if ( k ) {
            out << ',';
        }
        out << "\n  {";
        for ( size_t i = 0; i < kCRC32Size; ++i ) {
            if ( i != 0 ) {
                out << ',';
            }
            if ( i % kLineSize == 0 ) {
                out << "\n    ";
            } else {
                out << ' ';
            }
            out << "0x" << hex << setw(8) << setfill('0') << table[k][i];
        }
        out << "\n  }";
    }
    out << dec << "\n};\n" << endl;
}


#ifdef NCBI_USE_PRECOMPILED_CRC32_TABLES

# include "crc32tables.c"

#else

static TCRC32Table s_CRC32TableForward[TABLES_COUNT];
static TCRC32Table s_CRC32TableReverse[TABLES_COUNT];
static TCRC32Table s_CRC32CTableReverse[TABLES_COUNT];

/////////////////////////////////////////////////////////////////////////////
//  Implementation of CRC32 algorithm.
/////////////////////////////////////////////////////////////////////////////
//
//  This code assumes that an unsigned is at least 32 bits wide and
//  that the predefined type char occupies one 8-bit byte of storage.

//  The polynomial used is
//  x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x^1+x^0
#define CRC32_POLYNOMIAL    0x04c11db7
//  CRC32C (Castagnoli) polynomial is
//  x^32+x^28+x^27+x^26+x^25+x^23+x^22+x^20+x^19+x^18+x^14+x^13+x^11+x^10+x^9+x^8+x^6+x^0
#define CRC32C_POLYNOMIAL   0x1edc6f41

// CRC32 is linear meaning that for any texts t1 & t2:
//   CRC32[t1 XOR t2] = CRC32[t1] XOR CRC32[t2].
// This allows to speed up calculation of CRC32 tables by first
// calculating CRC32 for bytes with only one bit set,
// and then xoring all CRC32 of lowest bit and CRC32 of remaining bits
// to get CRC32 of whole number.
// First part is done by calling s_CalcByteCRC32Forward or
// s_CalcByteCRC32Reverse for each bit.
// Second pass is universal for any CRC32 and is performed by function
// s_FillMultiBitsCRC().


static inline
Uint4 s_CalcByteCRC32Forward(size_t byte, Uint4 polynomial)
{
    Uint4 byteCRC = byte << 24;
    for ( int j = 0;  j < 8;  ++j ) {
        if ( byteCRC & 0x80000000U )
            byteCRC = (byteCRC << 1) ^ polynomial;
        else
            byteCRC = (byteCRC << 1);
    }
    return byteCRC;
}


static inline
Uint4 s_CalcByteCRC32Reverse(size_t byte, Uint4 reversed_polynomial)
{
    Uint4 byteCRC = byte;
    for ( int j = 0;  j < 8;  ++j ) {
        if ( byteCRC & 1 )
            byteCRC = (byteCRC >> 1) ^ reversed_polynomial;
        else
            byteCRC = (byteCRC >> 1);
    }
    return byteCRC;
}


static inline
void s_FillMultiBitsCRC(Uint4* table, size_t size)
{
    // Preconditions:
    //  Entries at one-bit indexes (1<<k), are calculated.
    for ( size_t i = 1;  i < size;  ++i ) { // order is significant
        // Split bits of i into two parts:
        //  lobit contains lowest bit set, or zero if no bits are set,
        //  hibits contains all other bits.
        size_t hibits = i & (i-1);
        size_t lobit = i & ~(i-1);
        // Because of:
        //  1. i = lobit ^ hibits
        //  2. lobit <= i
        //  3. hibits <= i
        // we can calculate entry at i by xoring entries at lobit and hibits
        // There are 3 possible cases:
        //  A. i = 0
        //    In this case lobit = 0 and hibits = 0.
        //    As a result table[0] will become 0, which is correct for CRC.
        //  B. i = 1<<k
        //    In this case lobit = i, and hibits = 0.
        //    table[i] will become table[i] ^ table[0].
        //    Because table[0] is 0 (see case A above),
        //    table[i] will not change and will preserve precalculated value
        //    (see Preconditions above).
        //  C. all other i
        //    In this case lobit < i, and hibits < i
        //    It means the entries at lobit and hibits are calculated already
        //    because of the order of iteration by i.
        table[i] = table[lobit] ^ table[hibits];
    }
}


template<size_t kCRC32Tables>
static inline
void s_InitTableCRC32Forward(TCRC32Table (&table)[kCRC32Tables],
                             Uint4 polynomial)
{
    // check the last element to make sure we minimize chances of races 
    // in MT programs.
    if ( table[kCRC32Tables-1][kCRC32Size-1] ) {
        return;
    }
    // Initialize CRC32 for bytes with only one bit set
    for ( size_t i = 1;  i < kCRC32Size;  i <<= 1 ) {
        table[0][i] = s_CalcByteCRC32Forward(i, polynomial);
    }
    // Fill the rest of the main table
    s_FillMultiBitsCRC(table[0], kCRC32Size);
    // Fill secondary tables
    for ( size_t k = 1; k < kCRC32Tables; ++k ) {
        for ( size_t i = 0; i < kCRC32Size; ++i ) {
            Uint4 checksum = table[k-1][i];
            checksum = (checksum << 8) ^ table[0][checksum >> 24];
            table[k][i] = checksum;
        }
    }
}


template<size_t kCRC32Tables>
static inline
void s_InitTableCRC32Reverse(TCRC32Table (&table)[kCRC32Tables],
                             Uint4 polynomial)
{
    Uint4 reversed_polynomial = 0;
    for ( size_t i = 0; i < 32; ++i ) {
        reversed_polynomial = (reversed_polynomial << 1)|(polynomial & 1);
        polynomial >>= 1;
    }
    // check the last element to make sure we minimize chances of races 
    // in MT programs.
    if ( table[kCRC32Tables-1][kCRC32Size-1] ) {
        return;
    }
    // Initialize CRC32 for bytes with only one bit set
    for ( size_t i = 1;  i < kCRC32Size;  i <<= 1 ) {
        table[0][i] = s_CalcByteCRC32Reverse(i, reversed_polynomial);
    }
    // Fill the rest of the table
    s_FillMultiBitsCRC(table[0], kCRC32Size);
    // Fill secondary tables
    for ( size_t k = 1; k < kCRC32Tables; ++k ) {
        for ( size_t i = 0; i < kCRC32Size; ++i ) {
            Uint4 checksum = table[k-1][i];
            checksum = (checksum >> 8) ^ table[0][checksum & 0xff];
            table[k][i] = checksum;
        }
    }
}


void s_InitTableCRC32Forward(void)
{
    s_InitTableCRC32Forward(s_CRC32TableForward, CRC32_POLYNOMIAL);
}


void s_InitTableCRC32Reverse(void)
{
    s_InitTableCRC32Reverse(s_CRC32TableReverse, CRC32_POLYNOMIAL);
}


void s_InitTableCRC32CReverse(void)
{
    s_InitTableCRC32Reverse(s_CRC32CTableReverse, CRC32C_POLYNOMIAL);
}


#endif //NCBI_USE_PRECOMPILED_CRC32_TABLES


#define s_UpdateCRC32Forward_1(crc, str, table)         \
    do {                                                \
        Uint4 v = *(const Uint1*)(str) ^ ((crc) >> 24); \
        (crc) = ((crc) << 8) ^ (table)[0][v];           \
    } while(0)

#define s_UpdateCRC32Forward_2(crc, str, table)         \
    do {                                                \
        Uint4 v = *(const Uint2*)(str);                 \
        /* index bytes are in wrong order */            \
        (crc) = ((crc) << 16) ^                         \
            (table)[0][(((crc)>>16)^(v>>8)) & 0xff] ^   \
            (table)[1][(((crc)>>24)^(v   )) & 0xff];    \
    } while(0)

#define s_UpdateCRC32Forward_4(crc, str, table)         \
    do {                                                \
        Uint4 v = *(const Uint4*)(str);                 \
        /* index bytes are in wrong order */            \
        (crc) =                                         \
            (table)[0][(((crc)    )^(v>>24)) & 0xff] ^  \
            (table)[1][(((crc)>> 8)^(v>>16)) & 0xff] ^  \
            (table)[2][(((crc)>>16)^(v>> 8)) & 0xff] ^  \
            (table)[3][(((crc)>>24)^(v    )) & 0xff];   \
    } while(0)

#define s_UpdateCRC32Forward_8(crc, str, table)         \
    do {                                                \
        Uint4 v0 = ((const Uint4*)(str))[0];            \
        Uint4 v1 = ((const Uint4*)(str))[1];            \
        /* index bytes are in wrong order */            \
        (crc) =                                         \
            (table)[0][(            (v1>>24))       ] ^ \
            (table)[1][(            (v1>>16)) & 0xff] ^ \
            (table)[2][(            (v1>> 8)) & 0xff] ^ \
            (table)[3][(            (v1    )) & 0xff] ^ \
            (table)[4][(((crc)    )^(v0>>24)) & 0xff] ^ \
            (table)[5][(((crc)>> 8)^(v0>>16)) & 0xff] ^ \
            (table)[6][(((crc)>>16)^(v0>> 8)) & 0xff] ^ \
            (table)[7][(((crc)>>24)^(v0    )) & 0xff];  \
    } while(0)


#define s_UpdateCRC32Reverse_1(crc, str, table) \
    do {                                        \
        Uint4 v = *(const Uint1*)(str);         \
        v ^= (crc);                             \
        (crc) = ((crc) >> 8) ^                  \
            (table)[0][v & 0xff];               \
    } while(0)

#define s_UpdateCRC32Reverse_2(crc, str, table) \
    do {                                        \
        Uint4 v = *(const Uint2*)(str);         \
        v ^= (crc);                             \
        (crc) = ((crc) >> 16) ^                 \
            (table)[1][(v   ) & 0xff] ^         \
            (table)[0][(v>>8) & 0xff];          \
    } while(0)

#define s_UpdateCRC32Reverse_4(crc, str, table) \
    do {                                        \
        Uint4 v = *(const Uint4*)(str);         \
        v ^= (crc);                             \
        (crc) =                                 \
            (table)[3][(v    ) & 0xff] ^        \
            (table)[2][(v>> 8) & 0xff] ^        \
            (table)[1][(v>>16) & 0xff] ^        \
            (table)[0][(v>>24)       ];         \
    } while(0)

#define s_UpdateCRC32Reverse_8(crc, str, table) \
    do {                                        \
        Uint4 v0 = ((const Uint4*)(str))[0];    \
        Uint4 v1 = ((const Uint4*)(str))[1];    \
        v0 ^= (crc);                            \
        (crc) =                                 \
            (table)[7][(v0    ) & 0xff] ^       \
            (table)[6][(v0>> 8) & 0xff] ^       \
            (table)[5][(v0>>16) & 0xff] ^       \
            (table)[4][(v0>>24)       ] ^       \
            (table)[3][(v1    ) & 0xff] ^       \
            (table)[2][(v1>> 8) & 0xff] ^       \
            (table)[1][(v1>>16) & 0xff] ^       \
            (table)[0][(v1>>24)       ];        \
    } while(0)


template<size_t kCRC32Tables>
static inline
Uint4 s_UpdateCRC32Forward(Uint4 checksum, const char *str, size_t count,
                           const TCRC32Table (&table)[kCRC32Tables])
{
#if TABLES_COUNT >= 2    
    if ( (uintptr_t(str)&1) && count >= 1 ) {
        s_UpdateCRC32Forward_1(checksum, str, table);
        count -= 1;
        str += 1;
    }
# if TABLES_COUNT >= 4
    if ( (uintptr_t(str)&2) && count >= 2 ) {
        s_UpdateCRC32Forward_2(checksum, str, table);
        count -= 2;
        str += 2;
    }
#  if TABLES_COUNT >= 8
    while ( count >= 8 ) {
        s_UpdateCRC32Forward_8(checksum, str, table);
        count -= 8;
        str += 8;
    }
    if ( count >= 4 ) {
        s_UpdateCRC32Forward_4(checksum, str, table);
        count -= 4;
        str += 4;
    }
#  else // < 8
    while ( count >= 4 ) {
        s_UpdateCRC32Forward_4(checksum, str, table);
        count -= 4;
        str += 4;
    }
#  endif // done 4
    if ( count >= 2 ) {
        s_UpdateCRC32Forward_2(checksum, str, table);
        count -= 2;
        str += 2;
    }
# else // < 4
    while ( count >= 2 ) {
        s_UpdateCRC32Forward_2(checksum, str, table);
        count -= 2;
        str += 2;
    }
# endif // done 2
    if ( count ) {
        s_UpdateCRC32Forward_1(checksum, str, table);
    }
#else // < 2
    while ( count ) {
        s_UpdateCRC32Forward_1(checksum, str, table);
        count -= 1;
        str += 1;
    }
#endif // done 1
    return checksum;
}


template<size_t kCRC32Tables>
static inline
Uint4 s_UpdateCRC32Reverse(Uint4 checksum, const char *str, size_t count,
                           const TCRC32Table (&table)[kCRC32Tables])
{
#if TABLES_COUNT >= 2    
    if ( (uintptr_t(str)&1) && count >= 1 ) {
        s_UpdateCRC32Reverse_1(checksum, str, table);
        count -= 1;
        str += 1;
    }
# if TABLES_COUNT >= 4
    if ( (uintptr_t(str)&2) && count >= 2 ) {
        s_UpdateCRC32Reverse_2(checksum, str, table);
        count -= 2;
        str += 2;
    }
#  if TABLES_COUNT >= 8
    while ( count >= 8 ) {
        s_UpdateCRC32Reverse_8(checksum, str, table);
        count -= 8;
        str += 8;
    }
    if ( count >= 4 ) {
        s_UpdateCRC32Reverse_4(checksum, str, table);
        count -= 4;
        str += 4;
    }
#  else // < 8
    while ( count >= 4 ) {
        s_UpdateCRC32Reverse_4(checksum, str, table);
        count -= 4;
        str += 4;
    }
#  endif // done 4
    if ( count >= 2 ) {
        s_UpdateCRC32Reverse_2(checksum, str, table);
        count -= 2;
        str += 2;
    }
# else // < 4
    while ( count >= 2 ) {
        s_UpdateCRC32Reverse_2(checksum, str, table);
        count -= 2;
        str += 2;
    }
# endif // done 2
    if ( count ) {
        s_UpdateCRC32Reverse_1(checksum, str, table);
    }
#else // < 2
    while ( count ) {
        s_UpdateCRC32Reverse_1(checksum, str, table);
        count -= 1;
        str += 1;
    }
#endif // done 1
    return checksum;
}


#ifdef USE_CRC32C_INTEL

#if !defined(NCBI_COMPILER_MSVC) && !defined(bit_SSE4_2)
// our Darwin GCC doesn't have cpuid.h :(
// we have to reimplement cpuid functionality, luckily it's not too big
static inline
void call_cpuid(unsigned level,
                unsigned* a, unsigned* b, unsigned* c, unsigned* d)
{
#if defined(__i386__) && defined(__PIC__)
    // ebx may be the PIC register and some old GCC versions fail to take care
    __asm__("xchgl %%ebx, %k1;"
            "cpuid;"
            "xchgl %%ebx, %k1;"
            : "=a" (*a), "=&r" (*b), "=c" (*c), "=d" (*d)
            : "0" (level));
#elif defined(__x86_64__) && defined(__PIC__)
    // rbx may be the PIC register and some old GCC versions fail to take care
    __asm__("xchgq %%rbx, %q1;"
            "cpuid;"
            "xchgq %%rbx, %q1;"
            : "=a" (*a), "=&r" (*b), "=c" (*c), "=d" (*d)
            : "0" (level));
#else
    __asm__("cpuid"
            : "=a" (*a), "=b"  (*b), "=c" (*c), "=d" (*d)
            : "0" (level));
#endif
}
static inline
unsigned get_cpuid_max(unsigned extended)
{
    unsigned a, b, c, d;
#ifdef __i386__
    // on 32-bit processors we test special flag to check CPUID support
    const unsigned HAS_CPUID_FLAG = 0x00200000;
    __asm__(
        "pushfl;"
        "pushfl;"
        "popl  %0;"
        "movl  %0, %1;"
        "xorl  %2, %0;"
        "pushl  %0;"
        "popfl;"
        "pushfl;"
        "popl  %0;"
        "popfl;"
        : "=&r" (a), "=&r" (b)
        : "i" (HAS_CPUID_FLAG));
    if ( !((a ^ b) & HAS_CPUID_FLAG) )
        return 0;
#endif
    call_cpuid(extended, &a, &b, &c, &d);
    return a;
}
static inline
bool get_cpuid(unsigned level,
               unsigned *a, unsigned *b, unsigned *c, unsigned *d)
{
    if ( get_cpuid_max(level & 0x80000000U) < level) {
        return false;
    }
    call_cpuid (level, a, b, c, d);
    return true;
}
# define __get_cpuid get_cpuid
# define bit_SSE4_2 (1<<20)
#endif

bool s_IsCRC32CIntelEnabled(void)
{
    static volatile bool enabled, initialized;
    if ( !initialized ) {
#ifdef NCBI_COMPILER_MSVC
        int a[4];
        __cpuid(a, 0);
        if ( a[0] >= 1 ) {
            __cpuid(a, 1);
            enabled = (a[2] & (1<<20)) != 0;
        }
#else
        unsigned a, b, c, d;
        enabled = __get_cpuid(1, &a, &b, &c, &d) && (c & bit_SSE4_2);
#endif
        initialized = true;
    }
    return enabled;
}

static inline
Uint4 s_CRC32C(Uint4 checksum, const char* data)
{
#ifdef NCBI_COMPILER_MSVC
    return _mm_crc32_u8(checksum, *data);
#else
    // We cannot rely on _mm_crc32_u8() as it's available only when
    // -msse4_2 compiler option is specified.
    __asm__("crc32b %1, %0"
            : "+r" (checksum)
            : "m" (*data));
    return checksum;
#endif
}

static inline
Uint4 s_CRC32C(Uint4 checksum, const Uint2* data)
{
#ifdef NCBI_COMPILER_MSVC
    return _mm_crc32_u16(checksum, *data);
#else
    // We cannot rely on _mm_crc32_u16() as it's available only when
    // -msse4_2 compiler option is specified.
    __asm__("crc32w %1, %0"
            : "+r" (checksum)
            : "m" (*data));
    return checksum;
#endif
}

static inline
Uint4 s_CRC32C(Uint4 checksum, const Uint4* data)
{
#ifdef NCBI_COMPILER_MSVC
    return _mm_crc32_u32(checksum, *data);
#else
    // We cannot rely on _mm_crc32_u32() as it's available only when
    // -msse4_2 compiler option is specified.
    __asm__("crc32l %1, %0"
            : "+r" (checksum)
            : "m" (*data));
    return checksum;
#endif
}

#ifdef HAVE_CRC32C_64
static inline
Uint8 s_CRC32C(Uint8 checksum, const Uint8* data)
{
#ifdef NCBI_COMPILER_MSVC
    return _mm_crc32_u64(checksum, *data);
#else
    // We cannot rely on _mm_crc32_u64() as it's available only when
    // -msse4_2 compiler option is specified.
    __asm__("crc32q %1, %0"
            : "+r" (checksum)
            : "m" (*data));
#endif
    return checksum;
}
#endif // HAVE_CRC32C_64

static inline
Uint4 s_UpdateCRC32CIntel(Uint4 checksum, const char *str, size_t count)
{
    // Newer Intel CPUs with SSE 4.2 have instructions for CRC32C polynomial.
    // Since byte order is little-endian on Intel there is no need to bswap.

    // Align buffer
    if ( (uintptr_t(str)&1) && count >= 1 ) {
        checksum = s_CRC32C(checksum, str);
        count -= 1;
        str += 1;
    }
    if ( (uintptr_t(str)&2) && count >= 2 ) {
        checksum = s_CRC32C(checksum, (const Uint2*)str);
        count -= 2;
        str += 2;
    }
#ifdef HAVE_CRC32C_64
    // Main loop processes by 8 bytes
    if ( count >= 4 ) {
        if ( (uintptr_t(str)&4) ) {
            checksum = s_CRC32C(checksum, (const Uint4*)str);
            count -= 4;
            str += 4;
        }
        Uint8 crc = checksum;
        while ( count >= 8 ) {
            crc = s_CRC32C(crc, (const Uint8*)str);
            count -= 8;
            str += 8;
        }
        checksum = Uint4(crc);
        if ( count >= 4 ) {
            checksum = s_CRC32C(checksum, (const Uint4*)str);
            count -= 4;
            str += 4;
        }
    }
#else
    // Main loop processes by 4 bytes
    while ( count >= 4 ) {
        checksum = s_CRC32C(checksum, (const Uint4*)str);
        count -= 4;
        str += 4;
    }
#endif
    // Process remainder smaller than 8 bytes
    if ( count >= 2 ) {
        checksum = s_CRC32C(checksum, (const Uint2*)str);
        count -= 2;
        str += 2;
    }
    if ( count >= 1 ) {
        checksum = s_CRC32C(checksum, str);
        //last count and str updates aren't necessary
        //count -= 1;
        //str += 1;
    }
    return checksum;
}

#endif //USE_CRC32C_INTEL


static inline
Uint4 s_UpdateAdler32(Uint4 sum, const char* data, size_t len)
{
    const Uint4 MOD_ADLER = 65521;

#define ADJUST_ADLER(a) a = (a & 0xffff) + (a >> 16) * (0x10000-MOD_ADLER)
#define FINALIZE_ADLER(a) if (a >= MOD_ADLER) a -= MOD_ADLER

    Uint4 a = sum & 0xffff, b = sum >> 16;
    
    const size_t kMaxLen = 5548u;
    while (len) {
        if ( len >= kMaxLen ) {
            len -= kMaxLen;
            for ( size_t i = 0; i < kMaxLen/4; ++i ) {
                b += a += Uint1(data[0]);
                b += a += Uint1(data[1]);
                b += a += Uint1(data[2]);
                b += a += Uint1(data[3]);
                data += 4;
            }
        } else {
            for ( size_t i = len >> 2; i; --i ) {
                b += a += Uint1(data[0]);
                b += a += Uint1(data[1]);
                b += a += Uint1(data[2]);
                b += a += Uint1(data[3]);
                data += 4;
            }
            for ( len &= 3; len; --len ) {
                b += a += Uint1(data[0]);
                data += 1;
            }
        }
        ADJUST_ADLER(a);
        ADJUST_ADLER(b);
    }
    // It can be shown that a <= 0x1013a here, so a single subtract will do.
    FINALIZE_ADLER(a);
    // It can be shown that b can reach 0xffef1 here.
    ADJUST_ADLER(b);
    FINALIZE_ADLER(b);
    return (b << 16) | a;
}


void CChecksumBase::PrintTables(CNcbiOstream& out)
{
    InitTables();
    s_PrintTable(out, "s_CRC32TableForward", s_CRC32TableForward);
    s_PrintTable(out, "s_CRC32TableReverse", s_CRC32TableReverse);
    s_PrintTable(out, "s_CRC32CTableReverse", s_CRC32CTableReverse);
}


void CChecksumBase::x_Update(const char* str, size_t count)
{
    switch ( m_Method ) {
    case eCRC32:
    case eCRC32CKSUM:
        m_Value.v32 = s_UpdateCRC32Forward(m_Value.v32, str, count, s_CRC32TableForward);
        break;
    case eCRC32ZIP:
    case eCRC32INSD:
        m_Value.v32 = s_UpdateCRC32Reverse(m_Value.v32, str, count, s_CRC32TableReverse);
        break;
    case eCRC32C:
#ifdef USE_CRC32C_INTEL
        if ( s_IsCRC32CIntelEnabled() ) {
            m_Value.v32 = s_UpdateCRC32CIntel(m_Value.v32, str, count);
            break;
        }
#endif
        m_Value.v32 = s_UpdateCRC32Reverse(m_Value.v32, str, count, s_CRC32CTableReverse);
        break;
    case eAdler32:
        m_Value.v32 = s_UpdateAdler32(m_Value.v32, str, count);
        break;
    case eMD5:
        m_Value.md5->Update(str, count);
        break;
    case eCityHash32:
        _ASSERT(!m_CharCount);
        m_Value.v32 = CityHash32(str, count);
        break;
    case eCityHash64:
        _ASSERT(!m_CharCount);
        m_Value.v64 = CityHash64(str, count);
        break;
    case eFarmHash32:
        _ASSERT(!m_CharCount);
        m_Value.v32 = farmhash::Hash32(str, count);
        break;
    case eFarmHash64:
        _ASSERT(!m_CharCount);
        m_Value.v64 = farmhash::Hash64(str, count);
        break;
    case eMurmurHash2_32:
        {{
            _ASSERT(!m_CharCount);
            int n = count > kMax_Int ? kMax_Int : (int)count;
            m_Value.v32 = MurmurHash2(str, n, (uint32_t)m_Seed);
        }}
        break;
    case eMurmurHash2_64:
        {{
            _ASSERT(!m_CharCount);
            int n = count > kMax_Int ? kMax_Int : (int)count;
            m_Value.v64 = MurmurHash64A(str, n, m_Seed);
        }}
        break;
    case eMurmurHash3_32:
        {{
            _ASSERT(!m_CharCount);
            int n = count > kMax_Int ? kMax_Int : (int)count;
            MurmurHash3_x86_32(str, n, (uint32_t)m_Seed, &m_Value.v32);
        }}
        break;
    default:
        _ASSERT(0);
        break;
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// NHash
//

Uint4 NHash::CityHash32(const CTempString str)
{
    return ::CityHash32(str.data(), str.length());
}

Uint4 NHash::CityHash32(const char* str, size_t len)
{
    return ::CityHash32(str, len);
}

Uint8 NHash::CityHash64(const CTempString str)
{
    return ::CityHash64(str.data(), str.length());
}

Uint8 NHash::CityHash64(const char* str, size_t len)
{
    return ::CityHash64(str, len);
}

Uint4 NHash::FarmHash32(const CTempString str)
{
    return farmhash::Hash32(str.data(), str.length());
 
}

Uint4 NHash::FarmHash32(const char* str, size_t len)
{
    return farmhash::Hash32(str, len);
}

Uint8 NHash::FarmHash64(const CTempString str)
{
    return farmhash::Hash64(str.data(), str.length());
}

Uint8 NHash::FarmHash64(const char* str, size_t len)
{
    return farmhash::Hash64(str, len);
}

Uint4 NHash::MurmurHash2(const CTempString str, Uint4 seed)
{
    _ASSERT(str.length() <= kMax_Int);
    return ::MurmurHash2(str.data(), (int)str.length(), seed);
}

Uint4 NHash::MurmurHash2(const char* str, size_t len, Uint4 seed)
{
    _ASSERT(len <= kMax_Int);
    return ::MurmurHash2(str, (int)len, seed);
}

Uint8 NHash::MurmurHash64A(const CTempString str, Uint8 seed)
{
    _ASSERT(str.length() <= kMax_Int);
    return ::MurmurHash64A(str.data(), (int)str.length(), seed);
}

Uint8 NHash::MurmurHash64A(const char* str, size_t len, Uint8 seed)
{
    _ASSERT(len <= kMax_Int);
    return ::MurmurHash64A(str, (int)len, seed);
}

Uint4 NHash::MurmurHash3_x86_32(const CTempString str, Uint4 seed)
{
    _ASSERT(str.length() <= kMax_Int);
    Uint4 result;
    ::MurmurHash3_x86_32(str.data(), (int)str.length(), seed, &result);
    return result;
}

Uint4 NHash::MurmurHash3_x86_32(const char* str, size_t len, Uint4 seed)
{
    _ASSERT(len <= kMax_Int);
    Uint4 result;
    ::MurmurHash3_x86_32(str, (int)len, seed, &result);
    return result;
}



//////////////////////////////////////////////////////////////////////////////
//
// CChecksumException
//

const char* CChecksumException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eStreamIO:  return "eStreamError";
    case eFileIO:    return "eFileError";
    default:         return CException::GetErrCodeString();
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// CChecksumStreamWriter
//

CChecksumStreamWriter::CChecksumStreamWriter(CChecksum::EMethod method)
    : m_Checksum(method)
{
}


CChecksumStreamWriter::~CChecksumStreamWriter(void)
{
}


ERW_Result CChecksumStreamWriter::Write(const void* buf, size_t count,
                                        size_t* bytes_written)
{
    m_Checksum.AddChars((const char*)buf, count);
    if (bytes_written) {
        *bytes_written = count;
    }
    return eRW_Success;
}


ERW_Result CChecksumStreamWriter::Flush(void)
{
    return eRW_Success;
}


END_NCBI_SCOPE
