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
#include <util/md5.hpp>
#include <util/resource_info.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  Utility finctions
//

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
    }
    it++;
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
    while ( !in.eof() ) {
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
    // return name_pwd + res_name;
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
    static CNcbiResourceInfo sEmptyResInfo;
    return sEmptyResInfo;
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
// Block size is a multiple of key size
const int kBlockTEA_BlockSize = kBlockTEA_KeySize*sizeof(Int4);

typedef Int4 TBlockTEA_Key[kBlockTEA_KeySize];

#define TEA_MX ((z >> 5)^(y << 2)) + ((y >> 3)^(z << 4))^(sum^y) + (key[(p & 3)^e]^z);

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
    Uint4 sum = 0;
    Uint4 e;
    Int4 p;
    Int4 q = 6 + 52/n;
    q = 6 + 52/n;
    sum = q*kBlockTEA_Delta;
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
    int len = hash.size();
    // Allocate memory for both digest (16) and salt (20+1)
    char digest[37];
    memcpy(digest + 16, kBlockTEA_Salt, 21);
    {{
        CMD5 md5;
        md5.Update(hash.c_str(), hash.size());
        md5.Finalize((unsigned char*)digest);
    }}
    for (int i = 0; i < len; i++) {
        CMD5 md5;
        md5.Update(digest, 36);
        md5.Finalize((unsigned char*)digest);
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
    BlockTEA_Encode_In_Place(buf, buflen, key);

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
    BlockTEA_Decode_In_Place(buf, buflen, key);

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


END_NCBI_SCOPE
