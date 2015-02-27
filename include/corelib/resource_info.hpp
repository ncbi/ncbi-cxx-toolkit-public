#ifndef CORELIB___RESOURCE_INFO__HPP
#define CORELIB___RESOURCE_INFO__HPP

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

/// @file resource_info.hpp
///
///   Defines NCBI C++ secure resources API.


#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_safe_static.hpp>


BEGIN_NCBI_SCOPE


/// Class for storing encrypted resource information.
class NCBI_XNCBI_EXPORT CNcbiResourceInfo : public CObject
{
public:
    /// Get resource name.
    const string& GetName(void) const { return m_Name; }

    /// Get main value associated with the requested resource.
    const string& GetValue(void) const { return m_Value; }

    /// Set new main value for the resource.
    void SetValue(const string& new_value) { m_Value = new_value; }

    /// Types to access extra values
    typedef multimap<string, string>      TExtraValuesMap;
    typedef CStringPairs<TExtraValuesMap> TExtraValues;

    /// Get read-only set of extra values associated with the resource.
    const TExtraValues& GetExtraValues(void) const { return m_Extra; }

    /// Get non-const set of extra values associated with the resource.
    TExtraValues& GetExtraValues_NC(void) { return m_Extra; }

    /// Check if the object is initialized
    operator bool(void) const { return !x_IsEmpty(); }
    
private:
    // Prohibit copy operations
    CNcbiResourceInfo(const CNcbiResourceInfo&);
    CNcbiResourceInfo& operator=(const CNcbiResourceInfo&);

    friend class CNcbiResourceInfoFile;
    friend class CSafeStatic_Allocator<CNcbiResourceInfo>;

    /// Create a new empty resource info.
    CNcbiResourceInfo(void);

    // Initialize resource info. Decode the data, store the
    // password for firther encoding.
    CNcbiResourceInfo(const string& res_name,
                      const string& pwd,
                      const string& enc);

    static CNcbiResourceInfo& GetEmptyResInfo(void);

    // Check if the object has not been initialized
    bool x_IsEmpty(void) const { return m_Name.empty(); }

    // Get encoded value string. If not initialized, return an empty string.
    string x_GetEncoded(void) const;

    string         m_Name;      // plaintext resource name
    mutable string m_Password;
    mutable string m_Value;
    TExtraValues   m_Extra;
};


/// Class for handling resource info files
class NCBI_XNCBI_EXPORT CNcbiResourceInfoFile
{
public:
    /// Get default resource info file location (/etc/ncbi/.info).
    static string GetDefaultFileName(void);

    /// Load resource-info file. If the file does not exist or can not
    /// be read, an empty object is created.
    CNcbiResourceInfoFile(const string& filename);

    /// Save data to file. If new_name is empty, overwrite the original
    /// file.
    void SaveFile(const string& new_name = kEmptyStr);

    /// Get read-only resource info for the given name. Returns empty
    /// object if the resource does not exist.
    const CNcbiResourceInfo& GetResourceInfo(const string& res_name,
                                             const string& pwd) const;
    /// Get (or create) non-const resource info for the given name
    CNcbiResourceInfo& GetResourceInfo_NC(const string& res_name,
                                          const string& pwd);

    /// Delete resource associated with the name
    void DeleteResourceInfo(const string& res_name,
                            const string& pwd);

    /// Create new resource info from the string and return reference to
    /// the new CNcbiResourceInfo object or an empty object if the string
    /// format is invalid.
    /// String format is (tabs or spaces can be used as separators):
    /// <password> <resource name> <main value> <extra values>
    /// where password, resource name and main value are URL-encoded
    /// and extra values are formatted like a CGI query string (individual
    /// values are URL-encoded). The password is used to encode all other
    /// information.
    /// Example:
    /// my%20pass%21 db%22name dbpa%24%24word ex1=val%201&ex2=val_2
    CNcbiResourceInfo& AddResourceInfo(const string& plain_text);

    /// Parse file containing source plaintext data, add (replace) all
    /// resources to the current file.
    void ParsePlainTextFile(const string& filename);

private:
    // Generate password for resource data using it's name and
    // user provided password.
    string x_GetDataPassword(const string& name_pwd,
                             const string& res_name) const;

    // Structure to cache decoded resource info
    struct SResInfoCache {
        string                  encoded;  // Original encoded values
        CRef<CNcbiResourceInfo> info;     // Decoded data
    };

    typedef map<string, SResInfoCache> TCache;

    string         m_FileName;
    mutable TCache m_Cache;
};


/// "NCBI keys file" -- contains a set of encryption keys, one key per line.
/// These files are supposed to be readable only by selected accounts that
/// require the use of keys (through CNcbiEncrypt API).
///
/// By default, the key file is ".ncbi_keys" in the user's home directory.
/// This can be changed via env variable $NCBI_KEY_FILES. There can be more
/// than one key file, e.g. $NCBI_KEY_FILES="/foo/.bar:/bar/.foo"
///
class NCBI_XNCBI_EXPORT CNcbiEncrypt
{
public:
    /// Encrypt a string using key from the 1st line of the 1st NCBI keys file.
    /// @note The key must be generated using GenerateKey() method.
    /// @param original_string
    ///   The string to encrypt
    /// @return
    ///   Encrypted string
    static string Encrypt(const string& original_string);

    /// Decrypt a string using the matching key found in the NCBI keys files.
    /// @note It looks up for the matching key (using key checksum stored
    ///       in the "encrypted_string") in all NCBI keys files.
    ///
    /// @note The key must be generated using GenerateKey() method.
    /// @param encrypted_string
    ///   The string to decrypt. The string may contain domain suffix,
    ///   separated by slash ("<encrypted data>/<domain>"). In this case
    ///   the function will try to find the decryption key for the specified
    ///   domain rather than use the usual set of keys.
    /// @return
    ///   Decrypted string
    /// @sa DecryptForDomain
    static string Decrypt(const string& encrypted_string);

    /// Re-read key file locations and domain paths, reload encryption keys.
    static void Reload(void);

    /// Generate an encryption key from the password, encrypt the string
    /// using the key.
    /// @param original_string
    ///   The string to encrypt
    /// @param password
    ///   The password used to generate the encryption key.
    /// @return
    ///   Encrypted string
    static string Encrypt(const string& original_string,
                          const string& password);

    /// Generate a decryption key from the password, decrypt the string
    /// using the key.
    /// @param encrypted_string
    ///   The string to decrypt
    /// @param password
    ///   The password used to generate the decryption key.
    /// @return
    ///   Decrypted string
    static string Decrypt(const string& encrypted_string,
                          const string& password);

    /// Encrypt data using domain key. Search NCBI_KEY_PATHS for a
    /// keys file for the specified domain, load the first key from
    /// the file and use it for encryption. If no domain file is found
    /// in the paths, throw exception.
    /// @param original_string
    ///   The string to encrypt
    /// @param domain
    ///   Keys domain. The corresponding keys file name is ".ncbi_keys.<domain>".
    /// @return
    ///   Data encrypted using the first key from the first available domain file.
    ///   The encrypted data include domain suffix which can be used for automatic
    ///   decryption.
    static string EncryptForDomain(const string& original_string,
                                   const string& domain);

    /// Decrypt data using domain key. Search NCBI_KEY_PATHS for a
    /// keys file for the specified domain, find a key for the
    /// checksum (stored in the encrypted string), use it for
    /// decryption. If no domain file is found, throw exception.
    /// @param encrypted_string
    ///   The string to decrypt. If the string includes domain suffix, it is
    ///   checked as well as the domain passed as the second argument.
    /// @param domain
    ///   Keys domain. The corresponding keys file name is ".ncbi_keys.<domain>".
    /// @return
    ///   Decrypted string.
    static string DecryptForDomain(const string& encrypted_string,
                                   const string& domain);

    /// Generate an encryption/decryption key from the seed string.
    /// @param seed
    ///   String to be used as a seed for the key. This can be a random
    ///   set of characters, a password provided by user input, or
    ///   anything else.
    /// @return
    ///   Hexadecimal string representation of the key.
    static string GenerateKey(const string& seed);

    /// Get encryption API version.
    static const char* GetVersion(void);

private:
    // Encryption key info
    struct SEncryptionKeyInfo
    {
        SEncryptionKeyInfo(void)
            : m_Severity(eDiag_Trace), m_Line(0), m_Version(0)
        {}

        SEncryptionKeyInfo(const string& key,
                           EDiagSev      sev,
                           const string& file,
                           size_t        line,
                           char          ver)
            : m_Key(key),
              m_Severity(sev),
              m_File(file),
              m_Line(line),
              m_Version(ver)
        {}

        string   m_Key;
        EDiagSev m_Severity;
        string   m_File;
        size_t   m_Line;
        char     m_Version; // API version
    };
    
    // Key storage
    typedef map<string, SEncryptionKeyInfo> TKeyMap;

    // Load (if not yet loaded) and return key map.
    static void sx_InitKeyMap(void);
    // Calculate checksum for a binary (not printable hexadecimal) key.
    static string x_GetBinKeyChecksum(const string& key);
    // Load keys from the file, put them in the map. Return the first key
    // found in the file (or empty string if there are no valid keys).
    static string x_LoadKeys(const string& filename, TKeyMap* keys);
    // Load keys from the domain (search NCBI_KEY_PATHS for the domain
    // file). Return the first key.
    static string x_GetDomainKeys(const string& domain, TKeyMap* keys);
    // Encrypt data using the provided binary key.
    static string x_Encrypt(const string& data, const string& key);
    // Find a matching key and decrypt data.
    static string x_Decrypt(const string& data, const TKeyMap& keys);

    static CSafeStatic<CNcbiEncrypt::TKeyMap> s_KeyMap;
    static CSafeStatic<string> s_DefaultKey;
    static volatile bool s_KeysInitialized;
};


/// Exception thrown by resource info classes
class NCBI_XNCBI_EXPORT CNcbiResourceInfoException :
    EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eFileSave,  //< Failed to save file
        eParser,    //< Error while parsing data
        eDecrypt    //< Error decrypting value
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch ( GetErrCode() ) {
        case eFileSave: return "eFileSave";
        case eParser:   return "eParser";
        case eDecrypt:  return "eDecrypt";
        default:         return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNcbiResourceInfoException, CException);
};


/// Exception thrown by encryption classes
class NCBI_XNCBI_EXPORT CNcbiEncryptException :
    EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eMissingKey,   //< No keys found
        eBadPassword,  //< Bad password string (empty password).
        eBadFormat,    //< Invalid encrypted string format
        eBadDomain,    //< Bad keys domain provided.
        eBadVersion    //< Bad API version in the data.
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch ( GetErrCode() ) {
        case eMissingKey:   return "eMissingKey";
        case eBadPassword:  return "eBadPassword";
        case eBadFormat:    return "eBadFormat";
        case eBadDomain:    return "eBadDomain";
        case eBadVersion:   return "eBadVersion";
        default:         return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNcbiEncryptException, CException);
};


END_NCBI_SCOPE


#endif  /* CORELIB___RESOURCE_INFO__HPP */
