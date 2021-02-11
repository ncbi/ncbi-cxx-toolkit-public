#ifndef PSGS_OSGMAPPER__HPP
#define PSGS_OSGMAPPER__HPP

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
 * Authors:  Eugene Vasilchenko, Aaron Ucko
 *
 */

/// @file osgservicemapper.hpp
/// PSG to OSG connection service mapper.


//#include "osgdbconnfactory.hpp"

#include <util/random_gen.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>


/** @addtogroup OS_Gateway
 *
 * @{
 */


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


typedef Uint8 TOSGEndpointKey;

inline
TOSGEndpointKey g_OSG_MakeEndpointKey(Uint4 host, Uint2 port)
{
    return (static_cast<TOSGEndpointKey>(host) << 16) | port;
}

inline
Uint4 g_OSG_GetHost(TOSGEndpointKey key)
{
    return key >> 16;
}

inline
Uint2 g_OSG_GetPort(TOSGEndpointKey key)
{
    return key & 0xFFFF;
}


class COSGServiceMapper : public CDBLB_ServiceMapper
{
public:
    COSGServiceMapper(const IRegistry* registry = NULL);

    static void InitDefaults(IRWRegistry& reg);
    
    void    Configure     (const IRegistry* registry = NULL);
    TSvrRef GetServer     (const string& service)
        { return x_GetServer(service, NULL); }
    void    Exclude       (const string& service, const TSvrRef& server);
    void    CleanExcluded (const string& service);
    void    GetServersList(const string& service, list<string>* serv_list)
        const;
    void    GetServerOptions(const string& service, TOptions* options);

    typedef vector<TOSGEndpointKey> TTried;
    enum EFeedback {
        eNegativeFeedback,
        ePositiveFeedback
    };
    TSvrRef GetServer(const string& service, const TTried& tried)
        { return x_GetServer(service, &tried); }
    void    AcceptFeedback(const CDB_Connection* connection,
                           EFeedback feedback);
    void    AcceptFeedback(const string& service,
                           unsigned int host, unsigned short port,
                           EFeedback feedback);

    string GetMainServiceName(const string& service);

    static IDBServiceMapper* Factory(const IRegistry* registry)
        { return new COSGServiceMapper(registry); }

private:
    typedef CDBLB_ServiceMapper TParent;

    struct SServerRating : public CObject {
        TSvrRef ref;
        double  penalty; // 0.0 = always accept, 1.0 = always reject
        bool    excluded;
    };
    typedef map<TOSGEndpointKey, SServerRating>  TServerRatings;
    typedef map<string,          TServerRatings> TAllServerRatings;

    TSvrRef x_GetServer(const string& service, const TTried* tried);
    void x_NormalizePenalties(void);
    SServerRating& x_SetRating(TServerRatings& ratings,
                               Uint4 host, Uint2 port, bool* was_new,
                               CTempString name = kEmptyStr) const;

    map<string, string> m_MainServiceNameMap;
    double m_PositiveFeedbackWeight;
    double m_NegativeFeedbackWeight;
    double m_PenaltyNormalizationInterval;
    double m_PenaltyDecayRate;  // precalculated from half-life
    double m_InitialPenalty;

    mutable TAllServerRatings m_AllServerRatings;
    mutable CFastRWLock       m_AllServerRatingsLock;
    CStopWatch m_AllServerRatingsTimer;
    CFastMutex m_DBLBExclusionsMutex;
    CRandom    m_Random;
    CFastMutex m_RandomMutex;
};


//////////////////////////////////////////////////////////////////////


inline
string COSGServiceMapper::GetMainServiceName(const string& service)
{
    auto it = m_MainServiceNameMap.find(service);
    return it == m_MainServiceNameMap.end() ? service : it->second;
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;


/* @} */

#endif  // PSGS_OSGMAPPER__HPP
