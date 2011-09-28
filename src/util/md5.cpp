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
 * Author:  Aaron Ucko (C++ interface); original author unknown
 *
 * File Description:
 *   CMD5 - class for computing Message Digest version 5 checksums.
 *
 */

#include <ncbi_pch.hpp>
#include <util/md5.hpp>
#include <util/util_exception.hpp>


BEGIN_NCBI_SCOPE


// Note: this code is harmless on little-endian machines.
inline
static void s_ByteReverse(unsigned char* buf, size_t longs)
{
    Uint4 t;
    do {
        t = (Uint4) ((unsigned) buf[3] << 8 | buf[2]) << 16 |
            ((unsigned) buf[1] << 8 | buf[0]);
        *(reinterpret_cast<Uint4*>(buf)) = t;
        buf += 4;
    } while (--longs);
}


// Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
// initialization constants.
CMD5::CMD5(void)
    : m_Bits(0), m_Finalized(false)
{
    m_Buf[0] = 0x67452301;
    m_Buf[1] = 0xefcdab89;
    m_Buf[2] = 0x98badcfe;
    m_Buf[3] = 0x10325476;
}


// Update state to reflect the concatenation of another buffer full of bytes.
void CMD5::Update(const char* buf, size_t length)
{
    if ( m_Finalized ) {
        NCBI_THROW(CUtilException, eWrongCommand,
                   "attempt to update a finalized MD5 instance");
    }

    // Number of leftover bytes in m_In
    unsigned int tmp = (unsigned int)((m_Bits >> 3) % sizeof(m_In));
    
    // Update bit count
    m_Bits += length << 3;

    // Handle any leading odd-sized chunks
    if ( tmp ) {
        unsigned char* p = m_In + tmp;

        tmp = kBlockSize - tmp;
        if (length < tmp) {
            memcpy(p, buf, length);
            return;
        }
        memcpy(p, buf, tmp);
#ifdef WORDS_BIGENDIAN
        s_ByteReverse(m_In, 16);
#endif
        Transform();
        buf    += tmp;
        length -= tmp;
    }

    // Process remaining data in kBlockSize-byte chunks
    while (length >= kBlockSize) {
        memcpy(m_In, buf, kBlockSize);
#ifdef WORDS_BIGENDIAN
        s_ByteReverse(m_In, 16);
#endif
        Transform();
        buf    += kBlockSize;
        length -= kBlockSize;
    }

    // Handle any remaining bytes of data
    memcpy(m_In, buf, length);
}


// Final wrapup - pad to kBlockSize-byte boundary with the bit pattern
// 1 0* (64-bit count of bits processed, MSB-first).
void CMD5::Finalize(unsigned char digest[16])
{
    if ( m_Finalized ) {
        memcpy(digest, m_Buf, 16);
        return;
    }

    // Compute number of bytes mod kBlockSize
    int count = (int)((m_Bits >> 3) % kBlockSize);

    // Set the first char of padding to 0x80.  This is safe since there is
    // always at least one byte free.
    unsigned char *p = m_In + count;
    *p++ = 0x80;

    // Bytes of padding needed to make kBlockSize bytes
    count = kBlockSize - 1 - count;

    // Pad out to 56 mod kBlockSize
    if (count < 8) {
        // Two lots of padding:  Pad the first block to kBlockSize bytes
        memset(p, 0, count);
#ifdef WORDS_BIGENDIAN
        s_ByteReverse(m_In, 16);
#endif
        Transform();

        // Now fill the next block with 56 bytes
        memset(m_In, 0, kBlockSize - 8);
    } else {
        // Pad block to 56 bytes
        memset(p, 0, count - 8);
#ifdef WORDS_BIGENDIAN
        s_ByteReverse(m_In, 14);
#endif
    }

    // Append length in bits and transform
    reinterpret_cast<Uint4*>(m_In)[14] = static_cast<Uint4>(m_Bits);
    reinterpret_cast<Uint4*>(m_In)[15] = static_cast<Uint4>(m_Bits >> 32);

    Transform();
#ifdef WORDS_BIGENDIAN
    s_ByteReverse(reinterpret_cast<unsigned char*>(m_Buf), 4);
#endif
    memcpy(digest, m_Buf, 16);
    memset(m_In, 0, kBlockSize); // may be sensitive
    m_Finalized = true;
}


string CMD5::GetHexSum(unsigned char digest[16])
{
    CNcbiOstrstream oss;
    for (size_t i = 0; i < 16; ++i) {
        oss << hex << setw(2) << setfill('0') << (int)digest[i];
    }
    return CNcbiOstrstreamToString(oss);
}


// The four core functions - F1 is optimized somewhat

// #define F1(x, y, z) (x & y | ~x & z)
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) ((z & x) | (~z & y))
#define F3(x, y, z) (x ^ (y ^ z))
#define F4(x, y, z) (y ^ (x | ~z))

// This is the central step in the MD5 algorithm.
#define MD5STEP(f, w, x, y, z, data, s) \
        ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

// The core of the MD5 algorithm, this alters an existing MD5 hash to
// reflect the addition of 16 longwords of new data.  MD5Update blocks
// the data and converts bytes into longwords for this routine.
void CMD5::Transform(void)
{
    Uint4  a, b, c, d;
    Uint4* inw = reinterpret_cast<Uint4*>(m_In);

    a = m_Buf[0];
    b = m_Buf[1];
    c = m_Buf[2];
    d = m_Buf[3];

    MD5STEP(F1, a, b, c, d, inw[0]  + 0xd76aa478,  7);
    MD5STEP(F1, d, a, b, c, inw[1]  + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, inw[2]  + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, inw[3]  + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, inw[4]  + 0xf57c0faf,  7);
    MD5STEP(F1, d, a, b, c, inw[5]  + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, inw[6]  + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, inw[7]  + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, inw[8]  + 0x698098d8,  7);
    MD5STEP(F1, d, a, b, c, inw[9]  + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, inw[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, inw[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, inw[12] + 0x6b901122,  7);
    MD5STEP(F1, d, a, b, c, inw[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, inw[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, inw[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, inw[1]  + 0xf61e2562,  5);
    MD5STEP(F2, d, a, b, c, inw[6]  + 0xc040b340,  9);
    MD5STEP(F2, c, d, a, b, inw[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, inw[0]  + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, inw[5]  + 0xd62f105d,  5);
    MD5STEP(F2, d, a, b, c, inw[10] + 0x02441453,  9);
    MD5STEP(F2, c, d, a, b, inw[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, inw[4]  + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, inw[9]  + 0x21e1cde6,  5);
    MD5STEP(F2, d, a, b, c, inw[14] + 0xc33707d6,  9);
    MD5STEP(F2, c, d, a, b, inw[3]  + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, inw[8]  + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, inw[13] + 0xa9e3e905,  5);
    MD5STEP(F2, d, a, b, c, inw[2]  + 0xfcefa3f8,  9);
    MD5STEP(F2, c, d, a, b, inw[7]  + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, inw[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, inw[5]  + 0xfffa3942,  4);
    MD5STEP(F3, d, a, b, c, inw[8]  + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, inw[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, inw[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, inw[1]  + 0xa4beea44,  4);
    MD5STEP(F3, d, a, b, c, inw[4]  + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, inw[7]  + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, inw[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, inw[13] + 0x289b7ec6,  4);
    MD5STEP(F3, d, a, b, c, inw[0]  + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, inw[3]  + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, inw[6]  + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, inw[9]  + 0xd9d4d039,  4);
    MD5STEP(F3, d, a, b, c, inw[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, inw[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, inw[2]  + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, inw[0]  + 0xf4292244,  6);
    MD5STEP(F4, d, a, b, c, inw[7]  + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, inw[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, inw[5]  + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, inw[12] + 0x655b59c3,  6);
    MD5STEP(F4, d, a, b, c, inw[3]  + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, inw[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, inw[1]  + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, inw[8]  + 0x6fa87e4f,  6);
    MD5STEP(F4, d, a, b, c, inw[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, inw[6]  + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, inw[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, inw[4]  + 0xf7537e82,  6);
    MD5STEP(F4, d, a, b, c, inw[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, inw[2]  + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, inw[9]  + 0xeb86d391, 21);

    m_Buf[0] += a;
    m_Buf[1] += b;
    m_Buf[2] += c;
    m_Buf[3] += d;
}


END_NCBI_SCOPE
