#ifndef NETCACHED__HPP
#define NETCACHED__HPP

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
 * Authors:  Pavel Ivanov
 *
 * File Description: Network cache daemon
 *
 */


#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE


struct SNCStateStat;


/// Policy for accepting passwords for reading and writing blobs
enum ENCBlobPassPolicy {
    eNCBlobPassAny,     ///< Both blobs with password and without are accepted
    eNCOnlyWithPass,    ///< Only blobs with password are accepted
    eNCOnlyWithoutPass  ///< Only blobs without password are accepted
};


struct SNCSpecificParams : public CObject
{
    bool  disable;
    bool  prolong_on_read;
    bool  srch_on_read;
    bool  fast_on_main;
    ENCBlobPassPolicy pass_policy;
    //Uint4 conn_timeout;
    //Uint4 cmd_timeout;
    Uint4 lifespan_ttl;
    Uint4 max_ttl;
    Uint4 blob_ttl;
    Uint4 ver_ttl;
    Uint2 ttl_unit;
    Uint1 quorum;

    SNCSpecificParams()
      : disable(false), prolong_on_read(false), srch_on_read(false), fast_on_main(false),
        pass_policy(eNCBlobPassAny), lifespan_ttl(0), max_ttl(0), blob_ttl(0), ver_ttl(0), ttl_unit(0), quorum(0)
    {
    }
    SNCSpecificParams(const SNCSpecificParams& o)
      : disable(o.disable), prolong_on_read(o.prolong_on_read),
        srch_on_read(o.srch_on_read), fast_on_main(o.fast_on_main),
        pass_policy(o.pass_policy),
        lifespan_ttl(o.lifespan_ttl), max_ttl(o.max_ttl), blob_ttl(o.blob_ttl), ver_ttl(o.ver_ttl), ttl_unit(o.ttl_unit),
        quorum(o.quorum)
    {
    }
    virtual ~SNCSpecificParams(void);
};

/////////////////////////////////////////////////////////////////////////////
// CNCBlobKeyLight
class CNCBlobKeyLight
{
public:
    CNCBlobKeyLight(void);
    CNCBlobKeyLight(const CNCBlobKeyLight& another);
    CNCBlobKeyLight& operator=(const CNCBlobKeyLight& another);
    CNCBlobKeyLight(const string& packed_key);
    CNCBlobKeyLight& operator=(const string& packed_key);
    CNCBlobKeyLight(const CTempString& packed_key);
    CNCBlobKeyLight& operator=(const CTempString& packed_key);

    CNCBlobKeyLight& Copy(const CNCBlobKeyLight& another);

    void Clear(void);
    bool IsValid(void) const;

    unsigned int KeyVersion(void) const;
    bool IsICacheKey(void) const;
    const string& PackedKey(void) const;
    const CTempString& RawKey(void) const;
    const CTempString& SubKey(void) const;
    const CTempString& Cache(void) const;
    string KeyForLogs(void) const;

protected:
    void PackBlobKey(const CTempString& cache_name,
                     const CTempString& blob_key,
                     const CTempString& blob_subkey);
    void UnpackBlobKey(void);
    void SetKeyVersion(unsigned int v);

private:
    string m_PackedKey;
    CTempString m_Cachename;
    CTempString m_RawKey;
    CTempString m_SubKey;
    unsigned int m_KeyVersion;
};

/////////////////////////////////////////////////////////////////////////////
// CNCBlobKey
class CNCBlobKey : public CNCBlobKeyLight,
                   public CNetCacheKey
{
public:
    CNCBlobKey(void);
    void Assign( const CTempString& cache_name,
                 const CTempString& blob_key,
                 const CTempString& blob_subkey);
    void Assign( const CTempString& blob_key);

private:
    CNCBlobKey(const CNCBlobKey& another);
    CNCBlobKey& operator=(const CNCBlobKey& another);
    CNCBlobKey(const string& packed_key);
    CNCBlobKey& operator=(const string& packed_key);
    CNCBlobKey(const CTempString& packed_key);
    CNCBlobKey& operator=(const CTempString& packed_key);
    CNCBlobKey& Copy(const CNCBlobKey& another);
};

/////////////////////////////////////////////////////////////////////////////
inline
CNCBlobKeyLight::CNCBlobKeyLight(void) {
    Clear();
}
inline
CNCBlobKeyLight::CNCBlobKeyLight(const CNCBlobKeyLight& another) {
    Copy(another);
}
inline
CNCBlobKeyLight& CNCBlobKeyLight::operator=(const CNCBlobKeyLight& another) {
    return Copy(another);
}
inline
CNCBlobKeyLight::CNCBlobKeyLight(const string& packed_key) {
    *this = CTempString(packed_key);
}
inline
CNCBlobKeyLight& CNCBlobKeyLight::operator=(const string& packed_key) {
    return *this = CTempString(packed_key);
}
inline
CNCBlobKeyLight::CNCBlobKeyLight(const CTempString& packed_key) {
    *this = packed_key;
}
inline
bool CNCBlobKeyLight::IsICacheKey(void) const {
    return !m_Cachename.empty();
}
inline
const string& CNCBlobKeyLight::PackedKey(void) const {
    return m_PackedKey;
}
inline
const CTempString& CNCBlobKeyLight::RawKey(void) const {
    return m_RawKey;
}
inline
const CTempString& CNCBlobKeyLight::SubKey(void) const {
    return m_SubKey;
}
inline
const CTempString& CNCBlobKeyLight::Cache(void) const {
    return m_Cachename;
}
inline
bool CNCBlobKeyLight::IsValid(void) const {
    return !RawKey().empty();
}
inline
void CNCBlobKeyLight::SetKeyVersion(unsigned int v) {
    m_KeyVersion = v;
}
inline
CNCBlobKey::CNCBlobKey(void) {
}
inline
void CNCBlobKey::Assign( const CTempString& blob_key) {
    Assign( CTempString(), blob_key, CTempString());
}

/////////////////////////////////////////////////////////////////////////////
/// Netcache server
class CNCServer
{
public:
    static const SNCSpecificParams* GetAppSetup(const TStringMap& client_params);
    static void WriteAppSetup(CSrvSocketTask& task, const SNCSpecificParams* app);
    static void WriteEnvInfo(CSrvSocketTask& task);

    /// Get inactivity timeout for each connection
    //static unsigned GetDefConnTimeout(void);
    static int GetDefBlobTTL(void);
    /// Get name of client that should be used for administrative commands
    static const string& GetAdminClient(void);

    /// Get total number of seconds the server is running
    static int GetUpTime(void);
    static void CachingCompleted(void);
    static bool IsInitiallySynced(void);
    static void InitialSyncComplete(void);
    static void InitialSyncRequired(void);
    static bool IsCachingComplete(void);
    static bool IsDebugMode(void);

    static void ReadCurState(SNCStateStat& state);

private:
    CNCServer(void);
};


class CNCHeartBeat : public CSrvTask
{
public:
    CNCHeartBeat(void);
    virtual ~CNCHeartBeat(void);

    static void CheckConfFile(void);
private:
    virtual void ExecuteSlice(TSrvThreadNum thr_num);
};


END_NCBI_SCOPE

#endif /* NETCACHED__HPP */
