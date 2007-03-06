#ifndef UTIL___ICACHE__REF__HPP
#define UTIL___ICACHE__REF__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: ICache external reference using hash key(CRC32, MD5)
 *
 */

/// @file cache_ref.hpp
/// cache (ICache) external reference using hash key(CRC32, MD5)
///

#include <util/cache/icache.hpp>

BEGIN_NCBI_SCOPE


/// Hashed content cache. This class works with hash-content pair.
/// Hash code alone can potentially give false positives (collisions)
/// so this implementation compares both hash and content. 
/// Hash mismatch automatically screens-out content comparison 
/// (it is presumed that hash cannot give us false negatives).
///
/// <pre>
/// Comparison order and operation complexity:
/// 1. Hash comparison (B-tree search)
/// 2.    - if 1 successful 
///          -> memory comparison (content length dependent).
/// </pre>
///
///
/// Note for CGI developers:
/// Caching functions can throw exceptions! 
/// For many CGIs cache is optional please catch and supress exceptions 
/// in the calling application. Treat exception case as if cache found 
/// nothing.
/// 
class CCacheHashedContent 
{
public:
    CCacheHashedContent(ICache& cache);


    /// Store hashed content
    ///
    /// @param hash_str
    ///     Hash key (CRC32 or MD5 or something else).
    ///     Choose low-collision hash value (like MD5).
    ///
    /// @param hashed_content
    ///     The content string (hash source). 
    /// @param ref_value
    ///     Content reference value (NetCache key).
    ///
    /// @return TRUE if hashed reference can be returned
    ///
    void StoreHashedContent(const string& hash_str, 
                            const string& hashed_content, 
                            const string& ref_value);


    /// Get hashed content
    /// Method compares both hash value and hashed content.
    ///
    /// @param hash_str
    ///     Hash key (could be CRC32 or MD5 or something else).
    /// @param hashed_content
    ///     The content string (hash source). 
    /// @param ref_value
    ///     Output content reference value (NetCache key).
    ///
    /// @return TRUE if hashed reference can be returned
    ///
    bool GetHashedContent(const string& hash_str, 
                          const string& hashed_content, 
                          string*       ref_value);

    /// Remove hash content
    ///
    void RemoveHashedContent(const string& hash_str);


private:
    CCacheHashedContent(const CCacheHashedContent&);
    CCacheHashedContent& operator=(const CCacheHashedContent&);
protected:
    ICache&         m_ICache;
    const string    m_HashContentSubKey;
    const string    m_RefValueSubKey;
};


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


inline
CCacheHashedContent::CCacheHashedContent(ICache& cache)
: m_ICache(cache),
  m_HashContentSubKey("CONTENT"),
  m_RefValueSubKey("REF")
{}

inline
void CCacheHashedContent::RemoveHashedContent(const string& hash_str)
{
    m_ICache.Remove(hash_str);
}

inline
void CCacheHashedContent::StoreHashedContent(const string& hash_str, 
                                             const string& hashed_content, 
                                             const string& ref_value)
{
    // two cache subkey are used here:
    //  CONTENT for hashed content (used for cache search-comparison)
    //  REF for the reference itself (reference storage)
    //
    const void* data = hashed_content.c_str();
    m_ICache.Store(hash_str,
                   0,
                   m_HashContentSubKey,
                   data,
                   hashed_content.length());
    data = ref_value.c_str();
    m_ICache.Store(hash_str,
                   0,
                   m_RefValueSubKey,
                   data,
                   ref_value.length());

}

inline
bool CCacheHashedContent::GetHashedContent(const string& hash_str, 
                                           const string& hashed_content, 
                                           string*       ref_value)
{
    const size_t buf_size = 4 * 1024;
    char buf[buf_size];

    ICache::SBlobAccessDescr blob_access(buf, buf_size);

    // read-compare hashed content
    //
    m_ICache.GetBlobAccess(hash_str, 0, m_HashContentSubKey, &blob_access);
    if (!blob_access.blob_found) {
        return false;
    }
    if (blob_access.reader.get()) {
        // too large...
        // TODO: implement reader based comparison
        return false;
    } else {
        // BLOB is in memory - memcmp
        if (hashed_content.length() != blob_access.blob_size) {
            return false;
        }
        int cmp = memcmp(blob_access.buf, 
                         hashed_content.c_str(), 
                         blob_access.blob_size);
        if (cmp != 0) {
            return false;
        }
    }

    // read the reference
    //

    m_ICache.GetBlobAccess(hash_str, 0, m_RefValueSubKey, &blob_access);
    if (!blob_access.blob_found) {
        return false;
    }
    if (blob_access.reader.get()) {
        // TODO: implement reader operation
        return false;
    } else {
        if (ref_value) {
            ref_value->resize(0);
            ref_value->append(blob_access.buf, blob_access.buf_size);
        }
    }
    return true;
}


END_NCBI_SCOPE

#endif  /* UTIL___ICACHE__REF__HPP */
