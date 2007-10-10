#ifndef UTIL_ICACHE_CF__HPP
#define UTIL_ICACHE_CF__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Anatoliy Kuznetsov
*
* File Description:
*   Util library ICache class factory assistance functions
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <util/cache/icache.hpp>
#include <util/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Util_Cache

BEGIN_NCBI_SCOPE

/// Utility class for ICache class factories
///
/// @internal
///
template<class TDriver> 
class CICacheCF : public CSimpleClassFactoryImpl<ICache, TDriver>
{
public:
    typedef 
      CSimpleClassFactoryImpl<ICache, TDriver> TParent;

public:
    CICacheCF(const string& driver_name, int patch_level = -1)
        : TParent(driver_name, patch_level)
    {}

    /// Utility function, configures common ICache parameters
    void ConfigureICache(ICache*                        icache, 
                         const TPluginManagerParamTree* params) const
    {
        if (!params) return;

        // Timestamp configuration
        {{
        static const string kCFParam_timestamp = "timestamp";
        
        const string& ts_flags_str = 
            this->GetParam(params, kCFParam_timestamp, false);

        if (!ts_flags_str.empty()) {
            ConfigureTimeStamp(icache, params, ts_flags_str);
        }
        
        }}


        static const string kCFParam_keep_versions = "keep_versions";

        const string& keep_versions_str = 
            this->GetParam(params, kCFParam_keep_versions, false);
        if (!keep_versions_str.empty()) {
            static const string kCFParam_keep_versions_all = "all";
            static const string kCFParam_keep_versions_drop_old = "drop_old";
            static const string kCFParam_keep_versions_drop_all = "drop_all";

            ICache::EKeepVersions kv_policy = ICache::eKeepAll;
            if (NStr::CompareNocase(keep_versions_str, 
                                    kCFParam_keep_versions_all)==0) {
                kv_policy = ICache::eKeepAll;
            } else 
            if (NStr::CompareNocase(keep_versions_str, 
                                    kCFParam_keep_versions_drop_old)==0) {
                kv_policy = ICache::eDropOlder;
            } else 
            if (NStr::CompareNocase(keep_versions_str, 
                                    kCFParam_keep_versions_drop_all)==0) {
                kv_policy = ICache::eDropAll;
            } else {
                LOG_POST_X(1, Warning 
                    << "ICache::ClassFactory: Unknown keep_versions" 
                       " policy parameter: "
                    << keep_versions_str);
            }

            icache->SetVersionRetention(kv_policy);
        }

    }

    void ConfigureTimeStamp(ICache*                        icache,
                            const TPluginManagerParamTree* params,
                            const string&                  options) const
    {
        static 
        const string kCFParam_timeout       = "timeout";
        static 
        const string kCFParam_max_timeout   = "max_timeout";

        static 
        const string kCFParam_timestamp_onread   = "onread";
        static 
        const string kCFParam_timestamp_subkey   = "subkey";
        static 
        const string kCFParam_timestamp_expire_not_used = "expire_not_used";
        static 
        const string kCFParam_timestamp_purge_on_startup   = "purge_on_startup";
        static 
        const string kCFParam_timestamp_check_expiration   = "check_expiration";

        list<string> opt;
        NStr::Split(options, " \t", opt);
        ICache::TTimeStampFlags ts_flag = 0;
        ITERATE(list<string>, it, opt) {
            const string& opt_value = *it;
            if (NStr::CompareNocase(opt_value, 
                                    kCFParam_timestamp_onread)==0) {
                ts_flag |= ICache::fTimeStampOnRead;
                continue;
            }
            if (NStr::CompareNocase(opt_value, 
                                    kCFParam_timestamp_subkey)==0) {
                ts_flag |= ICache::fTrackSubKey;
                continue;
            }
            if (NStr::CompareNocase(opt_value, 
                                    kCFParam_timestamp_expire_not_used)==0) {
                ts_flag |= ICache::fExpireLeastFrequentlyUsed;
                continue;
            }
            if (NStr::CompareNocase(opt_value, 
                                    kCFParam_timestamp_purge_on_startup)==0) {
                ts_flag |= ICache::fPurgeOnStartup;
                continue;
            }
            if (NStr::CompareNocase(opt_value, 
                                    kCFParam_timestamp_check_expiration)==0) {
                ts_flag |= ICache::fCheckExpirationAlways;
                continue;
            }
            LOG_POST_X(2, Warning 
                      << "ICache::ClassFactory: Unknown timeout policy parameter: "
                      << opt_value);
        } // ITERATE


        unsigned int timeout = (unsigned int)
            this->GetParamInt(params, kCFParam_timeout, false, 60 * 60);
        unsigned int max_timeout = (unsigned int)
            this->GetParamInt(params, kCFParam_max_timeout, false, 0);

        if (max_timeout && max_timeout < timeout)
            max_timeout = timeout;

        if (ts_flag) {
            icache->SetTimeStampPolicy(ts_flag, timeout, max_timeout);
        }

    }
};

END_NCBI_SCOPE

#undef NCBI_USE_ERRCODE_X


#endif  /* UTIL_EXCEPTION__HPP */
