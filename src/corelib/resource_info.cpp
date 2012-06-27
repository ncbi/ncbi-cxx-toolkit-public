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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   NCBI C++ secure resources API
 *
 */


#include <ncbi_pch.hpp>

#include <ncbiconf.h>
#include <corelib/ncbi_bswap.hpp>
#include <corelib/resource_info.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  Utility finctions
//

// Forward declaration of the local MD5
void CalcMD5(const char* data, size_t len, unsigned char* digest);

inline char Hex(unsigned char c)
{
    if (c < 10) return c + '0';
    return c - 10 + 'A';
}


string StringToHex(const string& s)
{
    string ret;
    ret.reserve(s.size()*2);
    ITERATE(string, c, s) {
        ret += Hex((unsigned char)(*c) >> 4);
        ret += Hex((unsigned char)(*c) & 0x0f);
    }
    return ret;
}


string HexToString(const string& hex)
{
    string ret;
    _ASSERT(hex.size() % 2 == 0);
    ret.reserve(hex.size()/2);
    ITERATE(string, h, hex) {
        char c1 = NStr::HexChar(*h);
        h++;
        char c2 = NStr::HexChar(*h);
        if (c1 < 0  ||  c2 < 0) {
            ERR_POST("Invalid character in the encrypted string: " << hex);
            return kEmptyStr;
        }
        ret += ((c1 << 4) + c2);
    }
    return ret;
}


// Get encoded and hex-formatted string
inline string EncodeString(const string& s, const string& pwd)
{
    return StringToHex(BlockTEA_Encode(pwd, s));
}


/////////////////////////////////////////////////////////////////////////////
//
//  CNcbiResourceInfoFile
//


static const char* kDefaultResourceInfoPath = "/etc/ncbi/.info";
// Separator used in the encrypted file between name and value
static const char* kResourceValueSeparator = " ";
// Separator used in the encrypted file between main and extra
static const char* kResourceExtraSeparator = "&";
// Separators used in the source file
static const char* kParserSeparators = " \t";


string CNcbiResourceInfoFile::GetDefaultFileName(void)
{
    return kDefaultResourceInfoPath;
}


CNcbiResourceInfoFile::CNcbiResourceInfoFile(const string& filename)
    : m_FileName(filename)
{
    CNcbiIfstream in(m_FileName.c_str());
    if ( !in.good() ) {
        return;
    }

    string line;
    while ( !in.eof() ) {
        getline(in, line);
        line = NStr::TruncateSpaces(line);
        // Skip empty lines
        if ( line.empty() ) continue;
        string name, value;
        NStr::SplitInTwo(line, kResourceValueSeparator, name, value);
        m_Cache[name].encoded = value;
    }
}


void CNcbiResourceInfoFile::SaveFile(const string& new_name)
{
    string fname = new_name.empty() ? m_FileName : new_name;

    CNcbiOfstream out(fname.c_str());
    if ( !out.good() ) {
        NCBI_THROW(CNcbiResourceInfoException, eFileSave,
            "Failed to save encrypted file.");
        return;
    }

    ITERATE(TCache, it, m_Cache) {
        // Data may be modified, re-encode using saved password
        string enc = it->second.info ?
            it->second.info->x_GetEncoded() : it->second.encoded;
        out << it->first << kResourceValueSeparator << enc << endl;
    }

    // If new_name was not empty, remember it on success
    m_FileName = fname;
}


const CNcbiResourceInfo&
CNcbiResourceInfoFile::GetResourceInfo(const string& res_name,
                                       const string& pwd) const
{
    TCache::iterator it = m_Cache.find(EncodeString(res_name, pwd));
    if (it == m_Cache.end()) {
        return CNcbiResourceInfo::GetEmptyResInfo();
    }
    if ( !it->second.info ) {
        it->second.info.Reset(new CNcbiResourceInfo(res_name,
            x_GetDataPassword(pwd, res_name), it->second.encoded));
    }
    return *it->second.info;
}


CNcbiResourceInfo&
CNcbiResourceInfoFile::GetResourceInfo_NC(const string& res_name,
                                          const string& pwd)
{
    SResInfoCache& res_info = m_Cache[EncodeString(res_name, pwd)];
    if ( !res_info.info ) {
        res_info.info.Reset(new CNcbiResourceInfo(res_name,
            x_GetDataPassword(pwd, res_name), res_info.encoded));
    }
    return *res_info.info;
}


void CNcbiResourceInfoFile::DeleteResourceInfo(const string& res_name,
                                               const string& pwd)
{
    TCache::iterator it = m_Cache.find(EncodeString(res_name, pwd));
    if (it != m_Cache.end()) {
        m_Cache.erase(it);
    }
}


CNcbiResourceInfo&
CNcbiResourceInfoFile::AddResourceInfo(const string& plain_text)
{
    string data = NStr::TruncateSpaces(plain_text);
    // Ignore empty lines
    if ( data.empty() ) {
        NCBI_THROW(CNcbiResourceInfoException, eParser,
            "Empty source string.");
        // return CNcbiResourceInfo::GetEmptyResInfo();
    }
    list<string> split;
    list<string>::iterator it;
    string pwd, res_name, res_value, extra;

    // Get password for encoding
    NStr::Split(data, kParserSeparators, split);
    it = split.begin();
    if ( it == split.end() ) {
        // No password
        NCBI_THROW(CNcbiResourceInfoException, eParser,
            "Missing password.");
        // return CNcbiResourceInfo::GetEmptyResInfo();
    }
    pwd = NStr::URLDecode(*it);
    it++;
    if ( it == split.end() ) {
        // No resource name
        NCBI_THROW(CNcbiResourceInfoException, eParser,
            "Missing resource name.");
        // return CNcbiResourceInfo::GetEmptyResInfo();
    }
    res_name = NStr::URLDecode(*it);
    it++;
    if ( it == split.end() ) {
        // No main value
        NCBI_THROW(CNcbiResourceInfoException, eParser,
            "Missing main resource value.");
        return CNcbiResourceInfo::GetEmptyResInfo();
    }
    res_value = NStr::URLDecode(*it);
    it++;

    CNcbiResourceInfo& info = GetResourceInfo_NC(res_name, pwd);
    info.SetValue(res_value);
    if ( it != split.end() ) {
        info.GetExtraValues_NC().Parse(*it);
        it++;
    }

    if (it != split.end()) {
        // Too many fields
        NCBI_THROW(CNcbiResourceInfoException, eParser,
            "Unrecognized data found after extra values: " + *it + "...");
    }
    return info;
}


void CNcbiResourceInfoFile::ParsePlainTextFile(const string& filename)
{
    CNcbiIfstream in(filename.c_str());
    while ( in.good()  &&  !in.eof() ) {
        string line;
        getline(in, line);
        if ( line.empty() ) continue;
        AddResourceInfo(line);
    }
}


string CNcbiResourceInfoFile::x_GetDataPassword(const string& name_pwd,
                                                const string& res_name) const
{
    // The user's password is combined with the resource name.
    // This will produce different encoded data for different
    // resources even if they use the same password and have the
    // same value.
    // Name and password are swapped (name is used as encryption key)
    // so that the result is not equal to the encoded resource name.
    return BlockTEA_Encode(res_name, name_pwd);
}


/////////////////////////////////////////////////////////////////////////////
//
//  CNcbiResourceInfo
//

CNcbiResourceInfo::CNcbiResourceInfo(void)
{
    m_Extra.SetEncoder(new CStringEncoder_Url());
    m_Extra.SetDecoder(new CStringDecoder_Url());
}


CNcbiResourceInfo::CNcbiResourceInfo(const string& res_name,
                                     const string& pwd,
                                     const string& enc)
{
    _ASSERT(!res_name.empty());
    m_Extra.SetEncoder(new CStringEncoder_Url());
    m_Extra.SetDecoder(new CStringDecoder_Url());

    // Decode values only if enc is not empty.
    // If it's not set, we are creating a new resource info
    // and values will be set later.
    if ( !enc.empty() ) {
        string dec = BlockTEA_Decode(pwd, HexToString(enc));
        if ( dec.empty() ) {
            // Error decoding data
            NCBI_THROW(CNcbiResourceInfoException, eDecrypt,
                "Error decrypting resource info value.");
            return;
        }
        string val, extra;
        NStr::SplitInTwo(dec, kResourceExtraSeparator, val, extra);
        // Main value is URL-encoded, extra is not (but its members are).
        m_Value = NStr::URLDecode(val);
        m_Extra.Parse(extra);
    }
    m_Name = res_name;
    m_Password = pwd;
}


CNcbiResourceInfo& CNcbiResourceInfo::GetEmptyResInfo(void)
{
    static CSafeStaticPtr<CNcbiResourceInfo> sEmptyResInfo;
    return *sEmptyResInfo;
}


string CNcbiResourceInfo::x_GetEncoded(void) const
{
    if ( x_IsEmpty() ) {
        return kEmptyStr;
    }
    string str = NStr::URLEncode(m_Value) +
        kResourceExtraSeparator +
        m_Extra.Merge();
    return StringToHex(BlockTEA_Encode(m_Password, str));
}


/////////////////////////////////////////////////////////////////////////////
//
// XXTEA implementation
//

namespace { // Hide the implementation

const Uint4 kBlockTEA_Delta = 0x9e3779b9;
// Use 128-bit key
const int kBlockTEA_KeySize = 4;
// Block size is a multiple of key size. The longer the better (hides
// the source length).
const int kBlockTEA_BlockSize = kBlockTEA_KeySize*sizeof(Int4)*4;

typedef Int4 TBlockTEA_Key[kBlockTEA_KeySize];

#define TEA_MX ((z >> 5)^(y << 2)) + (((y >> 3)^(z << 4))^(sum^y)) + (key[(p & 3)^e]^z);

// Corrected Block TEA encryption
void BlockTEA_Encode_In_Place(Int4* data, Int4 n, const TBlockTEA_Key key)
{
    if (n <= 1) return;
    Uint4 z = data[n - 1];
    Uint4 y = data[0];
    Uint4 sum = 0;
    Uint4 e;
    Int4 p;
    Int4 q = 6 + 52/n;
    while (q-- > 0) {
        sum += kBlockTEA_Delta;
        e = (sum >> 2) & 3;
        for (p = 0; p < n - 1; p++) {
            y = data[p + 1];
            z = data[p] += TEA_MX;
        }
        y = data[0];
        z = data[n - 1] += TEA_MX;
    }
}


// Corrected Block TEA decryption
void BlockTEA_Decode_In_Place(Int4* data, Int4 n, const TBlockTEA_Key key)
{
    if (n <= 1) return;
    Uint4 z = data[n - 1];
    Uint4 y = data[0];
    Uint4 e;
    Int4 p;
    Int4 q = 6 + 52/n;
    Uint4 sum = q*kBlockTEA_Delta;
    while (sum != 0) {
        e = (sum >> 2) & 3;
        for (p = n - 1; p > 0; p--) {
            z = data[p - 1];
            y = data[p] -= TEA_MX;
        }
        z = data[n - 1];
        y = data[0] -= TEA_MX;
        sum -= kBlockTEA_Delta;
    }
}


// Read an integer from a little-endian ordered buffer
inline Int4 GetInt4LE(const char* ptr)
{
#ifdef WORDS_BIGENDIAN
    return CByteSwap::GetInt4((const unsigned char*)ptr);
#else
    return *(const Int4*)(ptr);
#endif
}

// Put the integer to the buffer in little-endian order
inline void PutInt4LE(Int4 i, char* ptr)
{
#ifdef WORDS_BIGENDIAN
    CByteSwap::PutInt4((unsigned char*)ptr, i);
#else
    *(Int4*)(ptr) = i;
#endif
}


// Convert string to array of integers assuming bytes in the string
// have little-endian order. 'dst' must have enough space to store
// the result. Length is given in bytes (chars).
void StringToInt4Array(const char* src, Int4* dst, size_t len)
{
    len /= sizeof(Int4);
    const char* p = src;
    for (size_t i = 0; i < len; i++, p += sizeof(Int4)) {
        dst[i] = GetInt4LE(p);
    }
}


// Convert array of integers to string with little-endian order of bytes.
// Length is given in integers.
string Int4ArrayToString(const Int4* src, size_t len)
{
    string ret;
    ret.reserve(len*sizeof(Int4));
    char buf[4];
    for (size_t i = 0; i < len; i++) {
        PutInt4LE(src[i], buf);
        ret += string(buf, 4);
    }
    return ret;
}


void GenerateKey(const string& pwd, TBlockTEA_Key& key)
{
    const unsigned char kBlockTEA_Salt[] = {
        0x2A, 0x0C, 0x84, 0x24, 0x5B, 0x0D, 0x85, 0x26,
        0x72, 0x40, 0xBC, 0x38, 0xD3, 0x43, 0x63, 0x9E,
        0x8E, 0x56, 0xF9, 0xD7, 0x00
    };
    string hash = pwd + (char*)kBlockTEA_Salt;
    int len = (int)hash.size();
    // Allocate memory for both digest (16) and salt (20+1)
    char digest[37];
    memcpy(digest + 16, kBlockTEA_Salt, 21);
    {{
        CalcMD5(hash.c_str(), hash.size(), (unsigned char*)digest);
    }}
    for (int i = 0; i < len; i++) {
        CalcMD5(digest, 36, (unsigned char*)digest);
    }
    StringToInt4Array(digest, key, kBlockTEA_KeySize*sizeof(Int4));
}

} // namespace


string BlockTEA_Encode(const string& password,
                       const string& src)
{
    // Prepare the key
    TBlockTEA_Key key;
    GenerateKey(password, key);

    // Prepare the source:
    // Add padding so that the src length is a multiple of key size.
    // Padding is always present even if the string size is already good -
    // this is necessary to remove it correctly after decoding.
    size_t padlen = kBlockTEA_BlockSize - (src.size() % kBlockTEA_BlockSize);
    string padded = string(padlen, char(padlen)) + src;
    _ASSERT(padded.size() % sizeof(Int4) == 0);
    // Convert source string to array of integers
    size_t buflen = padded.size() / sizeof(Int4);
    Int4* buf = new Int4[buflen];
    StringToInt4Array(padded.c_str(), buf, padded.size());

    // Encode data
    BlockTEA_Encode_In_Place(buf, (Int4)buflen, key);

    // Convert encoded buffer back to string
    string ret = Int4ArrayToString(buf, buflen);
    delete[] buf;
    return ret;
}


string BlockTEA_Decode(const string& password,
                       const string& src)
{
    if ( src.empty() ) {
        return kEmptyStr;
    }

    // Prepare the key
    TBlockTEA_Key key;
    GenerateKey(password, key);

    _ASSERT(src.size() % kBlockTEA_BlockSize == 0);
    // Convert source string to array of integers
    size_t buflen = src.size() / sizeof(Int4);
    Int4* buf = new Int4[buflen];
    StringToInt4Array(src.c_str(), buf, src.size());

    // Decode data
    BlockTEA_Decode_In_Place(buf, (Int4)buflen, key);

    // Convert decoded buffer back to string
    string ret = Int4ArrayToString(buf, buflen);
    delete[] buf;

    // Make sure padding is correct
    size_t padlen = (size_t)ret[0];
    if (padlen >= ret.size()) {
        return kEmptyStr;
    }
    for (size_t i = 0; i < padlen; i++) {
        if ((size_t)ret[i] != padlen) {
            return kEmptyStr;
        }
    }
    // Remove padding and return the result
    return ret.substr((size_t)ret[0], ret.size());
}


/////////////////////////////////////////////////////////////////////////////
//
//  Local MD5 implementation
//


namespace {
    union SUnionLenbuf {
        char lenbuf[8];
        Uint8 len8;
    };

    inline Uint4 leftrotate(Uint4 x, Uint4 c)
    {
        return (x << c) | (x >> (32 - c));
    }
}


void CalcMD5(const char* data, size_t len, unsigned char* digest)
{
    const Uint4 r[64] = {
        7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
        5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
        4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
        6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
    };

    const Uint4 k[64] = {
        3614090360U, 3905402710U, 606105819U,  3250441966U,
        4118548399U, 1200080426U, 2821735955U, 4249261313U,
        1770035416U, 2336552879U, 4294925233U, 2304563134U,
        1804603682U, 4254626195U, 2792965006U, 1236535329U,
        4129170786U, 3225465664U, 643717713U,  3921069994U,
        3593408605U, 38016083U,   3634488961U, 3889429448U,
        568446438U,  3275163606U, 4107603335U, 1163531501U,
        2850285829U, 4243563512U, 1735328473U, 2368359562U,
        4294588738U, 2272392833U, 1839030562U, 4259657740U,
        2763975236U, 1272893353U, 4139469664U, 3200236656U,
        681279174U,  3936430074U, 3572445317U, 76029189U,
        3654602809U, 3873151461U, 530742520U,  3299628645U,
        4096336452U, 1126891415U, 2878612391U, 4237533241U,
        1700485571U, 2399980690U, 4293915773U, 2240044497U,
        1873313359U, 4264355552U, 2734768916U, 1309151649U,
        4149444226U, 3174756917U, 718787259U,  3951481745U
    };

    // Initialize variables:
    Uint4 h0 = 0x67452301;
    Uint4 h1 = 0xEFCDAB89;
    Uint4 h2 = 0x98BADCFE;
    Uint4 h3 = 0x10325476;

    // Pre-processing:
    // append "1" bit to message
    // append "0" bits until message length in bits == 448 (mod 512)
    // append bit (not byte) length of unpadded message as 64-bit
    // little-endian integer to message
    Uint4 padlen = 64 - Uint4(len % 64);
    if (padlen < 9) padlen += 64;
    string buf(data, len);
    buf += char(0x80);
    buf.append(string(padlen - 9, 0));
    Uint8 len8 = len*8;
    SUnionLenbuf lb;
#ifdef WORDS_BIGENDIAN
    CByteSwap::PutInt8((unsigned char*)lb.lenbuf, len8);
#else
    lb.len8 = len8;
#endif
    buf.append(lb.lenbuf, 8);

    const char* buf_start = buf.c_str();
    const char* buf_end = buf_start + len + padlen;
    // Process the message in successive 512-bit chunks
    for (const char* p = buf_start; p < buf_end; p += 64) {
        // Break chunk into sixteen 32-bit little-endian words w[i]
        Uint4 w[16];
        for (int i = 0; i < 16; i++) {
            w[i] = (Uint4)GetInt4LE(p + i*4);
        }

        // Initialize hash value for this chunk:
        Uint4 a = h0;
        Uint4 b = h1;
        Uint4 c = h2;
        Uint4 d = h3;

        Uint4 f, g;

        // Main loop:
        for (unsigned int i = 0; i < 64; i++) {
            if (i < 16) {
                f = (b & c) | ((~b) & d);
                g = i;
            }
            else if (i < 32) {
                f = (d & b) | ((~d) & c);
                g = (5*i + 1) % 16;
            }
            else if (i < 48) {
                f = b ^ c ^ d;
                g = (3*i + 5) % 16;
            }
            else {
                f = c ^ (b | (~d));
                g = (7*i) % 16;
            }
     
            Uint4 temp = d;
            d = c;
            c = b;
            b += leftrotate((a + f + k[i] + w[g]), r[i]);
            a = temp;
        }

        // Add this chunk's hash to result so far:
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
    }

    PutInt4LE(h0, (char*)digest);
    PutInt4LE(h1, (char*)(digest + 4));
    PutInt4LE(h2, (char*)(digest + 8));
    PutInt4LE(h3, (char*)(digest + 12));
}

END_NCBI_SCOPE
