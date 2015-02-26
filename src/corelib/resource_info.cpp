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
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/resource_info.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  Utility finctions
//


// Forward declarations
void CalcMD5(const char* data, size_t len, unsigned char* digest);

string x_BlockTEA_Encode(const string& str_key,
                         const string& src,
                         size_t block_size);
string x_BlockTEA_Decode(const string& str_key,
                         const string& src,
                         size_t block_size);


namespace {

inline char Hex(unsigned char c)
{
    if (c < 10) return c + '0';
    return c - 10 + 'A';
}


// Convert binary data to a printable hexadecimal string.
string BinToHex(const string& data)
{
    string ret;
    ret.reserve(data.size()*2);
    ITERATE(string, c, data) {
        ret += Hex((unsigned char)(*c) >> 4);
        ret += Hex((unsigned char)(*c) & 0x0f);
    }
    return ret;
}


// Convert hexadecimal string to binary data. Throw CNcbiEncryptException
// if string format is not valid.
string HexToBin(const string& hex)
{
    string ret;
    _ASSERT(hex.size() % 2 == 0);
    ret.reserve(hex.size()/2);
    ITERATE(string, h, hex) {
        char c1 = NStr::HexChar(*h);
        h++;
        char c2 = NStr::HexChar(*h);
        if (c1 < 0  ||  c2 < 0) {
            NCBI_THROW(CNcbiEncryptException, eBadFormat,
                "Invalid hexadecimal string format: " + hex);
            return kEmptyStr;
        }
        ret += ((c1 << 4) + c2);
    }
    return ret;
}


// Use 128-bit key
const int kBlockTEA_KeySize = 4;

// Block size is a multiple of key size. The longer the better (hides
// the source length).
// For historical reasons block sizes for NCBI resources and encryption
// API are different.
const int kResInfo_BlockSize = kBlockTEA_KeySize*sizeof(Int4)*4;
const int kEncrypt_BlockSize = kBlockTEA_KeySize*sizeof(Int4);


// Helper function converting a seed string to a 128 bit binary key.
string GenerateBinaryKey(const string& seed)
{
    const unsigned char kBlockTEA_Salt[] = {
        0x2A, 0x0C, 0x84, 0x24, 0x5B, 0x0D, 0x85, 0x26,
        0x72, 0x40, 0xBC, 0x38, 0xD3, 0x43, 0x63, 0x9E,
        0x8E, 0x56, 0xF9, 0xD7, 0x00
    };
    string hash = seed + (char*)kBlockTEA_Salt;
    int len = (int)hash.size();
    // Allocate memory for both digest (16) and salt (20+1)
    char digest[37];
    memcpy(digest + 16, kBlockTEA_Salt, 21);
    {{
        CalcMD5(hash.c_str(), hash.size(), (unsigned char*)digest);
    }}
    // On every step calculate new digest from the last one + salt
    for (int i = 0; i < len; i++) {
        CalcMD5(digest, 36, (unsigned char*)digest);
    }
    return string(digest, kBlockTEA_KeySize*sizeof(Int4)); // 16 bytes
}


string BlockTEA_Encode(const string& password,
                       const string& src,
                       size_t        block_size)
{
    return x_BlockTEA_Encode(GenerateBinaryKey(password), src, block_size);
}


string BlockTEA_Decode(const string& password,
                       const string& src,
                       size_t        block_size)
{
    return x_BlockTEA_Decode(GenerateBinaryKey(password), src, block_size);
}


// Get encoded and hex-formatted string, to be used for resource-info
// only!
inline string EncodeString(const string& s, const string& pwd)
{
    return BinToHex(BlockTEA_Encode(pwd, s, kResInfo_BlockSize));
}

} // namespace


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
    return BlockTEA_Encode(res_name, name_pwd, kResInfo_BlockSize);
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
        string dec = BlockTEA_Decode(pwd, HexToBin(enc), kResInfo_BlockSize);
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
    static CSafeStatic<CNcbiResourceInfo> sEmptyResInfo;
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
    return BinToHex(BlockTEA_Encode(m_Password, str, kResInfo_BlockSize));
}


/////////////////////////////////////////////////////////////////////////////
//
//  CNcbiEncrypt
//

// The default keys file if present in the user's home directory.
const char* kDefaultKeysFile = ".ncbi_keys";
const char* kDefaultKeysPath = "/opt/ncbi/config";
const char* kKeysDomainPrefix = ".ncbi_keys.";
const char* kNcbiEncryptVersion = "1";

// Key files to cache in memory.
NCBI_PARAM_DECL(string, NCBI_KEY, FILES);
NCBI_PARAM_DEF_EX(string, NCBI_KEY, FILES, kEmptyStr, eParam_NoThread,
    NCBI_KEY_FILES);
typedef NCBI_PARAM_TYPE(NCBI_KEY, FILES) TKeyFiles;

// Paths to search for domain keys, colon separated. Special value '$$' can be
// added to include the default path.
NCBI_PARAM_DECL(string, NCBI_KEY, PATHS);
NCBI_PARAM_DEF_EX(string, NCBI_KEY, PATHS, "$$", eParam_NoThread,
    NCBI_KEY_PATHS);
typedef NCBI_PARAM_TYPE(NCBI_KEY, PATHS) TKeyPaths;


CSafeStatic<CNcbiEncrypt::TKeyMap> CNcbiEncrypt::s_KeyMap;
CSafeStatic<string> CNcbiEncrypt::s_DefaultKey;
volatile bool CNcbiEncrypt::s_KeysInitialized = false;

DEFINE_STATIC_MUTEX(s_EncryptMutex);


const char* CNcbiEncrypt::GetVersion(void)
{
    return kNcbiEncryptVersion;
}


string CNcbiEncrypt::Encrypt(const string& original_string)
{
    sx_InitKeyMap();
    const string& key = s_DefaultKey.Get();
    if ( key.empty() ) {
        NCBI_THROW(CNcbiEncryptException, eMissingKey,
            "No encryption keys found.");
    }
    return x_Encrypt(original_string, key);
}


string CNcbiEncrypt::Decrypt(const string& encrypted_string)
{
    sx_InitKeyMap();
    const TKeyMap& keys = s_KeyMap.Get();
    if ( keys.empty() ) {
        NCBI_THROW(CNcbiEncryptException, eMissingKey,
            "No decryption keys found.");
    }

    size_t domain_pos = encrypted_string.find('/');
    if (domain_pos != NPOS) {
        return DecryptForDomain(encrypted_string.substr(0, domain_pos),
            encrypted_string.substr(domain_pos + 1));
    }
    return x_Decrypt(encrypted_string, keys);
}


string CNcbiEncrypt::Encrypt(const string& original_string,
                             const string& password)
{
    if ( password.empty() ) {
        NCBI_THROW(CNcbiEncryptException, eBadPassword,
            "Encryption password can not be empty.");
    }
    return x_Encrypt(original_string, GenerateBinaryKey(password));
}


string CNcbiEncrypt::Decrypt(const string& encrypted_string,
                             const string& password)
{
    if ( password.empty() ) {
        NCBI_THROW(CNcbiEncryptException, eBadPassword,
            "Encryption password can not be empty.");
    }
    TKeyMap keys;
    string key = GenerateBinaryKey(password);
    char md5[16];
    CalcMD5(key.c_str(), key.size(), (unsigned char*)md5);
    keys[string(md5, 16)] =
        SEncryptionKeyInfo(key, eDiag_Trace, kEmptyStr, 0, *kNcbiEncryptVersion);
    return x_Decrypt(encrypted_string, keys);
}


string CNcbiEncrypt::EncryptForDomain(const string& original_string,
                                      const string& domain)
{
    string key = x_GetDomainKeys(domain, NULL);
    if ( key.empty() ) {
        NCBI_THROW(CNcbiEncryptException, eBadDomain,
            "No encryption keys found for domain " + domain);
    }

    return x_Encrypt(original_string, key) + "/" + domain;
}


string CNcbiEncrypt::DecryptForDomain(const string& encrypted_string,
                                      const string& domain)
{
    TKeyMap keys;
    x_GetDomainKeys(domain, &keys);

    size_t domain_pos = encrypted_string.find('/');
    if (domain_pos != NPOS) {
        // If the data include domain, check it as well.
        string domain2 = encrypted_string.substr(domain_pos + 1);
        if (domain2 != domain) {
            x_GetDomainKeys(domain2, &keys);
        }
    }

    if ( keys.empty() ) {
        NCBI_THROW(CNcbiEncryptException, eBadDomain,
            "No decryption keys found for domain " + domain);
    }

    return x_Decrypt(encrypted_string.substr(0, domain_pos), keys);
}


string CNcbiEncrypt::GenerateKey(const string& seed)
{
    // This method is just a helper for storing keys as hexadecimal strings.
    string key = GenerateBinaryKey(seed);
    string checksum = x_GetBinKeyChecksum(key);
    return kNcbiEncryptVersion + checksum + ":" + BinToHex(key);
}


void CNcbiEncrypt::Reload(void)
{
    CMutexGuard guard(s_EncryptMutex);
    s_KeysInitialized = false;
    TKeyFiles::ResetDefault();
    TKeyPaths::ResetDefault();
    s_KeyMap.Get().clear();
    s_DefaultKey.Get().clear();
    sx_InitKeyMap();
}


void CNcbiEncrypt::sx_InitKeyMap(void)
{
    if ( !s_KeysInitialized ) {
        CMutexGuard guard(s_EncryptMutex);
        if ( !s_KeysInitialized ) {
            TKeyMap& keys = s_KeyMap.Get();
            // Load keys from all available files.
            string files = TKeyFiles::GetDefault();
            if ( files.empty() ) {
                files = CDirEntry::MakePath(CDir::GetHome(), kDefaultKeysFile);
            }
            list<string> file_list;
            NStr::Split(files, ";", file_list);
            ITERATE(list<string>, it, file_list) {
                string fname = *it;
                size_t home_pos = fname.find("$HOME");
                if (home_pos == 0  &&  fname.size() > 5  &&  CDirEntry::IsPathSeparator(fname[5])) {
                    fname = CDir::ConcatPath(CDir::GetHome(), fname.substr(6));
                }
                string first_key = x_LoadKeys(fname, &keys);
                // Set the first available key as the default one.
                if ( s_DefaultKey->empty() ) {
                    *s_DefaultKey = first_key;
                }
            }
            s_KeysInitialized = true;
        }
    }
}


string CNcbiEncrypt::x_GetBinKeyChecksum(const string& key)
{
    char md5[16];
    CalcMD5(key.c_str(), key.size(), (unsigned char*)md5);
    return BinToHex(string(md5, 16));
}


string CNcbiEncrypt::x_LoadKeys(const string& filename, TKeyMap* keys)
{
    string first_key;
    CNcbiIfstream in(filename.c_str());
    if ( !in.good() ) return first_key;
    size_t line = 0;
    while ( in.good() && !in.eof() ) {
        line++;
        string s;
        getline(in, s);
        NStr::TruncateSpacesInPlace(s);
        // Ignore empty lines and comments.
        if (s.empty() || s[0] == '#') continue;
        
        char version = s[0];
        if (version != '1') {
            NCBI_THROW(CNcbiEncryptException, eBadVersion,
                "Invalid or unsupported API version in encryption key.");
        }

        string checksum, key;
        if ( !NStr::SplitInTwo(s, ":", checksum, key)  ||
            checksum.size() != 33 ) {
            ERR_POST(Warning <<
                "Invalid encryption key format in " << filename << ", line " << line);
            continue;
        }
        checksum = HexToBin(checksum.substr(1));

        EDiagSev sev = eDiag_Trace;
        size_t sevpos = key.find('/');
        if (sevpos != NPOS) {
            string sevname = key.substr(sevpos + 1);
            NStr::TruncateSpacesInPlace(sevname);
            if ( !CNcbiDiag::StrToSeverityLevel(sevname.c_str(), sev) ) {
                ERR_POST(Warning <<
                    "Invalid key severity in " << filename << ", line " << line);
                continue;
            }
            key.resize(sevpos);
            NStr::TruncateSpacesInPlace(key);
        }
        if (key.size() != 32) {
            ERR_POST(Warning <<
                "Invalid key format in " << filename << ", line " << line);
            continue;
        }

        key = HexToBin(key);
        char md5[16];
        CalcMD5(key.c_str(), key.size(), (unsigned char*)md5);
        if (string(md5, 16) != checksum) {
            ERR_POST(Warning << "Invalid key checksum in " << filename << ", line " << line);
            continue;
        }
        // Set the first available key as the default one.
        if ( first_key.empty() ) {
            first_key = key;
        }
        if ( !keys ) {
            // Found the first key, no need to parse the rest of the file.
            break;
        }
        (*keys)[checksum] = SEncryptionKeyInfo(key, sev, filename, line, version);
    }
    return first_key;
}


string CNcbiEncrypt::x_GetDomainKeys(const string& domain, TKeyMap* keys)
{
    string first_key;
    list<string> paths;
    NStr::Split(TKeyPaths::GetDefault(), ":", paths);
    ITERATE(list<string>, it, paths) {
        string path = *it;
        if (path == "$$") {
            path = kDefaultKeysPath;
        }
        string fname = CDirEntry::MakePath(*it, kKeysDomainPrefix + domain);
        string res = x_LoadKeys(fname, keys);
        if ( first_key.empty() ) {
            first_key = res;
        }
        if (!first_key.empty()  &&  !keys) {
            // Found the first key, no need to check other files.
            break;
        }
    }
    if ( keys ) {
        // Reset severity of the first key to Trace so that it's not reported.
        string first_checksum = x_GetBinKeyChecksum(first_key);
        (*keys)[first_checksum].m_Severity = eDiag_Trace;
    }
    return first_key;
}


string CNcbiEncrypt::x_Encrypt(const string& data, const string& key)
{
    _ASSERT(!key.empty());
    string checksum = x_GetBinKeyChecksum(key);
    return kNcbiEncryptVersion +
        checksum + ":" +
        BinToHex(x_BlockTEA_Encode(key, data, kEncrypt_BlockSize));
}


string CNcbiEncrypt::x_Decrypt(const string& data, const TKeyMap& keys)
{
    if (data.empty()) {
        NCBI_THROW(CNcbiEncryptException, eBadFormat,
            "Trying to decrypt an empty string.");
    }

    // API version
    char version = data[0];
    if (version != '1') {
        NCBI_THROW(CNcbiEncryptException, eBadVersion,
            "Invalid or unsupported API version in the encrypted data.");
    }

    // Parse the encrypted string, find key checksum.
    // The checksum is in the 16 bytes following version (32 hex chars),
    // separated with a colon.
    if (data.size() < 34  ||  data[33] != ':') {
        NCBI_THROW(CNcbiEncryptException, eBadFormat,
            "Invalid encrypted string format - missing key checksum.");
    }
    string checksum = HexToBin(data.substr(1, 32));
    TKeyMap::const_iterator key_it = keys.find(checksum);
    if (key_it == keys.end()) {
        NCBI_THROW(CNcbiEncryptException, eMissingKey,
            "No decryption key found for the checksum.");
    }
    string key = key_it->second.m_Key;
    EDiagSev sev = key_it->second.m_Severity;
    // TRACE severity indicates there's no need to log key usage.
    if (key != s_DefaultKey.Get()  &&  sev != eDiag_Trace) {
        ERR_POST_ONCE(Severity(key_it->second.m_Severity) <<
            "Decryption key accessed: checksum=" << x_GetBinKeyChecksum(key) <<
            ", location=" << key_it->second.m_File << ":" << key_it->second.m_Line);
    }

    // Domain must be parsed by the caller.
    _ASSERT(data.find('/') == NPOS);
    _ASSERT(!key.empty());
    return x_BlockTEA_Decode(key, HexToBin(data.substr(34)), kEncrypt_BlockSize);
}


/////////////////////////////////////////////////////////////////////////////
//
// XXTEA implementation
//

namespace { // Hide the implementation

const Uint4 kBlockTEA_Delta = 0x9e3779b9;

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
void StringToInt4Array(const string& src, Int4* dst)
{
    size_t len = src.size()/sizeof(Int4);
    const char* p = src.c_str();
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

} // namespace


// The string key must contain a 128-bit value.
string x_BlockTEA_Encode(const string& str_key,
                         const string& src,
                         size_t        block_size)
{
    if ( src.empty() ) {
        return kEmptyStr;
    }

    // Prepare the key
    _ASSERT(str_key.size() == kBlockTEA_KeySize*sizeof(Int4));
    TBlockTEA_Key key;
    StringToInt4Array(str_key, key);

    // Prepare the source:
    // Add padding so that the src length is a multiple of key size.
    // Padding is always present even if the string size is already good -
    // this is necessary to remove it correctly after decoding.
    size_t padlen = block_size - (src.size() % block_size);
    string padded = string(padlen, char(padlen)) + src;
    _ASSERT(padded.size() % sizeof(Int4) == 0);
    // Convert source string to array of integers
    size_t buflen = padded.size() / sizeof(Int4);
    Int4* buf = new Int4[buflen];
    StringToInt4Array(padded, buf);

    // Encode data
    BlockTEA_Encode_In_Place(buf, (Int4)buflen, key);

    // Convert encoded buffer back to string
    string ret = Int4ArrayToString(buf, buflen);
    delete[] buf;
    return ret;
}


// The string key must contain a 128-bit value.
string x_BlockTEA_Decode(const string& str_key,
                         const string& src,
                         size_t        block_size)
{
    if ( src.empty() ) {
        return kEmptyStr;
    }

    // Prepare the key
    _ASSERT(str_key.size() == kBlockTEA_KeySize*sizeof(Int4));
    TBlockTEA_Key key;
    StringToInt4Array(str_key, key);

    _ASSERT(src.size() % block_size == 0);
    // Convert source string to array of integers
    size_t buflen = src.size() / sizeof(Int4);
    Int4* buf = new Int4[buflen];
    StringToInt4Array(src, buf);

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
