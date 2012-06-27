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
    friend class CSafeStaticPtr<CNcbiResourceInfo>;

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


/// Encrypt the string using XXTEA and the password.
NCBI_XNCBI_EXPORT string BlockTEA_Encode(const string& password,
                                         const string& src);
// Decrypt the string using XXTEA and the password. Return empty
/// string on error.
NCBI_XNCBI_EXPORT string BlockTEA_Decode(const string& password,
                                         const string& src);

END_NCBI_SCOPE


#endif  /* CORELIB___RESOURCE_INFO__HPP */
