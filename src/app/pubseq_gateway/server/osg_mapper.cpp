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
* Author:  Eugene Vasilchenko, Aaron Ucko
*
* File Description:
*   PSG to OSG connection service mapper.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "osg_mapper.hpp"
//#include "osgtime.hpp"

#include <connect/ncbi_socket.hpp>
#include <dbapi/driver/public.hpp>

#include <cmath>

//#include <internal/cppcore/os_gateway/error_codes.hpp>

BEGIN_NCBI_NAMESPACE;

//#define NCBI_USE_ERRCODE_X OSG_Lib_ServiceMapper
//NCBI_DEFINE_ERR_SUBCODE_X(14);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);

static double s_DefaultPositiveWeight;
static double s_DefaultNegativeWeight;
static double s_DefaultNormalizationInterval;
static double s_DefaultDecayRate;
static double s_DefaultInitialPenalty;
static bool   s_DefaultsInitialized;


COSGServiceMapper::COSGServiceMapper(const IRegistry* registry)
{
    m_AllServerRatingsTimer.Start();
    m_Random.Randomize();
    Configure(registry);
}


void COSGServiceMapper::InitDefaults(IRWRegistry& reg)
{
    CNcbiApplication* app = CNcbiApplication::Instance();
    _ASSERT(app);
    string section = app->GetAppName();

    reg.Set(section, "positive_feedback_weight", "0.01",
            IRegistry::fNoOverride);
    reg.Set(section, "negative_feedback_weight", "0.5",
            IRegistry::fNoOverride);
    reg.Set(section, "penalty_normalization_interval", "10.0",
            IRegistry::fNoOverride);
    reg.Set(section, "penalty_half_life", "3600.0", IRegistry::fNoOverride);
    reg.Set(section, "initial_penalty", "0.15", IRegistry::fNoOverride);
    
    s_DefaultPositiveWeight
        = reg.GetDouble(section, "positive_feedback_weight", 0);
    s_DefaultNegativeWeight
        = reg.GetDouble(section, "negative_feedback_weight", 0);
    s_DefaultNormalizationInterval
        = reg.GetDouble(section, "penalty_normalization_interval", 0);
    s_DefaultDecayRate
        = -M_LN2 / reg.GetDouble(section, "penalty_half_life", 0);
    s_DefaultInitialPenalty = reg.GetDouble(section, "initial_penalty", 0);

    s_DefaultsInitialized = true;
}


void COSGServiceMapper::Configure(const IRegistry* registry)
{
    ConfigureFromRegistry(registry);

    CNcbiApplication* app = CNcbiApplication::Instance();
    _ASSERT(app);
    string section = app->GetAppName();

    if (registry == NULL) {
        registry = &app->GetConfig();
    }

    _ASSERT(s_DefaultsInitialized);

    m_PositiveFeedbackWeight
        = registry->GetDouble(section, "positive_feedback_weight",
                              s_DefaultPositiveWeight);
    m_NegativeFeedbackWeight
        = registry->GetDouble(section, "negative_feedback_weight",
                              s_DefaultNegativeWeight);
    m_PenaltyNormalizationInterval
        = registry->GetDouble(section, "penalty_normalization_interval",
                              s_DefaultNormalizationInterval);
    m_PenaltyDecayRate
        = -M_LN2 / registry->GetDouble(section, "penalty_half_life",
                                       s_DefaultDecayRate);
    m_InitialPenalty
        = registry->GetDouble(section, "initial_penalty",
                              s_DefaultInitialPenalty);

    list<string> entries;
    string msn_section = section + "/main_service_name";
    registry->EnumerateEntries(msn_section, &entries);
    for (const auto &it : entries) {
        m_MainServiceNameMap[it] = registry->Get(msn_section, it);
    }
}


inline
static string s_EndpointKeyName(TOSGEndpointKey k)
{
    return CSocketAPI::HostPortToString(g_OSG_GetHost(k), g_OSG_GetPort(k));
}


TSvrRef COSGServiceMapper::x_GetServer(const string& service,
                                       const TTried* tried)
{
    TServerRatings ratings;
    double         min_penalty = 1.0;
    size_t         full_count = 0, num_ratings = 0;

    _TRACE("Finding a server for " << service);
    
    x_NormalizePenalties();

    {{
        CFastReadGuard LOCK(m_AllServerRatingsLock);
        TAllServerRatings::const_iterator raw_ratings
            = m_AllServerRatings.find(service);
        if (raw_ratings == m_AllServerRatings.end()
            ||  raw_ratings->second.empty()) {
            LOCK.Release();
            list<string> serv_list;
            GetServersList(service, &serv_list);
        } else {
            double min_untried_penalty = 1.0;

            ITERATE (TServerRatings, it, raw_ratings->second) {
                bool tried_it
                    = (tried != NULL
                       &&  (find(tried->begin(), tried->end(), it->first)
                            != tried->end()));
                if (tried_it  ||  it->second.excluded
                    ||  it->second.penalty > 0.0) {
                    _ASSERT(it->second.ref.NotEmpty());
                    ratings.insert(*it);
                    ++num_ratings;
                }
                if ( !it->second.excluded ) {
                    if (it->second.penalty < min_penalty) {
                        min_penalty = it->second.penalty;
                    }
                    if ( !tried_it
                        &&  it->second.penalty < min_untried_penalty) {
                        min_untried_penalty = it->second.penalty;
                    }
                }
                ++full_count;
            }

            if (tried != NULL  &&  !tried->empty()) {
                if (min_untried_penalty < 1.0) {
                    min_penalty = min_untried_penalty;
                    ITERATE (TTried, it, *tried) {
                        TServerRatings::iterator rit = ratings.find(*it);
                        if (rit == ratings.end()) {
                            string msg
                                = FORMAT(s_EndpointKeyName(*it)
                                         << " unknown, but listed as tried.\n"
                                         << CStackTrace());
                            ERR_POST(Warning << "OSG: " << msg);
                        } else {
                            _TRACE("Skipping " << s_EndpointKeyName(*it)
                                   << " (already tried for this request)");
                            rit->second.excluded = true;
                        }
                    }
                } else {
                    _TRACE("Re-allowing previously tried backends. "
                           "(Out of alternatives.)");
                }
            }
        }
    }}

    vector<TSvrRef> to_exclude;
    if ( !ratings.empty() ) {
        CFastMutexGuard LOCK(m_RandomMutex);
        CRandom::TValue max_random = CRandom::GetMax();
        to_exclude.reserve(num_ratings);
        ITERATE (TServerRatings, it, ratings) {
            if (it->second.excluded
                ||  (m_Random.GetRand() < it->second.penalty * max_random)) {
                to_exclude.push_back(it->second.ref);
                _TRACE("Temporarily excluding "
                       << s_EndpointKeyName(it->first) << " (penalty: "
                       << it->second.penalty << ')');
            } else {
                _TRACE("Considering " << s_EndpointKeyName(it->first)
                       << " (penalty: " << it->second.penalty << ')');
            }
        }
        if (to_exclude.size() == full_count) {
            // Apparently excluded everything; rescale and try again.
            // (Not done right away, as this technique can introduce skew
            // if somehow starting with an incomplete list.)
            to_exclude.clear();
            to_exclude.reserve(full_count);
            if (min_penalty < 1.0) {
                // Give the numerator a slight boost to keep roundoff
                // error from yielding a (slim!) possibility of still
                // excluding everything.
                double scale_factor = (max_random + 1.0) / (1.0 - min_penalty);
                ITERATE (TServerRatings, it, ratings) {
                    double score = (1.0 - it->second.penalty) * scale_factor;
                    if (it->second.excluded  ||  m_Random.GetRand() > score) {
                        to_exclude.push_back(it->second.ref);
                    } else {
                        _TRACE("Reconsidering "
                               << s_EndpointKeyName(it->first) << " (score: "
                               << NStr::UInt8ToString(score,
                                                      NStr::fWithCommas)
                               << ')');
                    }
                }
            }
        }
    }
    
    CFastMutexGuard LOCK(m_DBLBExclusionsMutex);
    TParent::CleanExcluded(service);
    ITERATE (vector<TSvrRef>, it, to_exclude) {
        TParent::Exclude(service, *it);
    }
    TSvrRef result = TParent::GetServer(service);
    TParent::CleanExcluded(service);
    ITERATE (TServerRatings, it, ratings) {
        if (it->second.excluded) {
            TParent::Exclude(service, it->second.ref);
        }
    }
    _TRACE("Returning " << CSocketAPI::ntoa(result->GetHost())
           << ", expiring "
           << CTime(result->GetExpireTime()).ToLocalTime().AsString());
    return result;
}


void COSGServiceMapper::Exclude(const string& service, const TSvrRef& server)
{
    {{
        CFastWriteGuard LOCK(m_AllServerRatingsLock);
        bool was_new = false;
        SServerRating& rating
            = x_SetRating(m_AllServerRatings[service], server->GetHost(),
                          server->GetPort(), &was_new, server->GetName());
        rating.excluded = true;
        if (was_new) {
            string msg
                = "Excluding previously undiscovered " + service + " on "
                + CSocketAPI::ntoa(server->GetHost()) + '.';
            ERR_POST(Warning << "OSG: " << msg);
        }
    }}
    {{
        CFastMutexGuard LOCK(m_DBLBExclusionsMutex);
        TParent::Exclude(service, server);
    }}
    auto msn = m_MainServiceNameMap.find(service);
    if (msn != m_MainServiceNameMap.end()) {
        Exclude(msn->second, server);
    }
}


void COSGServiceMapper::CleanExcluded(const string& service)
{
    {{
        CFastWriteGuard LOCK(m_AllServerRatingsLock);
        TServerRatings& ratings = m_AllServerRatings[service];
        NON_CONST_ITERATE (TServerRatings, it, ratings) {
            it->second.excluded = false;
        }
    }}
    {{
        CFastMutexGuard LOCK(m_DBLBExclusionsMutex);
        TParent::CleanExcluded(service);
    }}
    auto msn = m_MainServiceNameMap.find(service);
    if (msn != m_MainServiceNameMap.end()) {
        CleanExcluded(msn->second);
    }
}


void COSGServiceMapper::GetServersList(const string& service,
                                       list<string>* serv_list) const
{
    unique_ptr<set<string>> main_set;
    TServerRatings* main_ratings = nullptr;
    TParent::GetServersList(service, serv_list);
    auto msn = m_MainServiceNameMap.find(service);
    if (msn != m_MainServiceNameMap.end()) {
        list<string> main_list;
        TParent::GetServersList(msn->second, &main_list);
        main_set.reset(new set<string>(main_list.begin(), main_list.end()));
    }

    CFastWriteGuard LOCK(m_AllServerRatingsLock);
    TServerRatings& ratings = m_AllServerRatings[service];
    if (main_set.get() != nullptr) {
        main_ratings = &m_AllServerRatings[msn->second];
    }
    ITERATE (list<string>, it, *serv_list) {
        Uint4 host;
        Uint2 port;
        bool  was_new = false;
        CSocketAPI::StringToHostPort(*it, &host, &port);
        auto& rating = x_SetRating(ratings, host, port, &was_new);
        if (was_new) {
            _TRACE("Discovered " << service << " on " << *it);
            if (main_ratings != nullptr) {
                auto& main_rating = x_SetRating(*main_ratings, host, port,
                                                &was_new);
                if (main_set->find(*it) == main_set->end()) {
                    main_rating.penalty = 1.0;
                    main_rating.excluded = true;
                } else {
                    _TRACE("Discovered " << msn->second << " on " << *it);
                    main_rating = rating;
                }
            }
        }
    }
}


void COSGServiceMapper::GetServerOptions(const string& service,
                                         TOptions* options)
{
    set<TOSGEndpointKey> main_set;
    TServerRatings* main_ratings = nullptr;
    TParent::GetServerOptions(service, options);
    auto msn = m_MainServiceNameMap.find(service);
    if (msn != m_MainServiceNameMap.end()) {
        TOptions main_options;
        TParent::GetServerOptions(msn->second, &main_options);
        for (const auto &it : main_options) {
            main_set.insert(g_OSG_MakeEndpointKey(it->GetHost(),
                                                  it->GetPort()));
        }
    }

    CFastWriteGuard LOCK(m_AllServerRatingsLock);
    TServerRatings& ratings = m_AllServerRatings[service];
    if (msn != m_MainServiceNameMap.end()) {
        main_ratings = &m_AllServerRatings[msn->second];
    }
    for (const auto& it : *options) {
        auto host = it->GetHost();
        auto port = it->GetPort();
        bool was_new = false;
        auto& rating = x_SetRating(ratings, host, port, &was_new);
        if (was_new) {
            _TRACE("Discovered " << service << " on "
                   << CSocketAPI::HostPortToString(host, port));
            if (main_ratings != nullptr) {
                auto& main_rating = x_SetRating(*main_ratings, host, port,
                                                &was_new);
                auto key = g_OSG_MakeEndpointKey(host, port);
                if (main_set.find(key) == main_set.end()) {
                    main_rating.penalty = 1.0;
                    main_rating.excluded = true;
                } else {
                    _TRACE("Discovered " << msn->second << " on "
                           << CSocketAPI::HostPortToString(host, port));
                    main_rating = rating;
                }
            }
        }
    }
}


void COSGServiceMapper::AcceptFeedback(const CDB_Connection* connection,
                                       EFeedback feedback)
{
    // Must parse service name out of pool name. :-/
    const string& pool = connection->PoolName();
    SIZE_TYPE at_pos = pool.find('@');
    _ASSERT(at_pos != NPOS);
    string service(pool, at_pos + 1, pool.find(':') - at_pos - 1);
    AcceptFeedback(service, connection->Host(), connection->Port(), feedback);
}
 
void COSGServiceMapper::AcceptFeedback(const string& service,
                                       unsigned int host, unsigned short port,
                                       EFeedback feedback)
{
    x_NormalizePenalties();
    CFastWriteGuard LOCK(m_AllServerRatingsLock);
    bool was_new = false;
    SServerRating& rating = x_SetRating(m_AllServerRatings[service],
                                        host, port, &was_new);
    double&        penalty = rating.penalty;
    _DEBUG_ARG(double old_penalty = penalty);
    if (was_new) {
        string msg = ("Accepting feedback for previously undiscovered "
                      + service + " on " + rating.ref->GetName() + '.');
        ERR_POST(Warning << "OSG: " << msg);
    }
    if (feedback == ePositiveFeedback) {
        penalty *= (1.0 - m_PositiveFeedbackWeight);
    } else {
        penalty = penalty * (1.0 - m_NegativeFeedbackWeight)
            + m_NegativeFeedbackWeight;
    }
    _TRACE(((feedback == ePositiveFeedback)
            ? "Reducing" : "Increasing")
           << " penalty for " << service << " on "
           << CSocketAPI::HostPortToString(host, port)
           << " from " << old_penalty << " to " << penalty);
    LOCK.Release();
    auto msn = m_MainServiceNameMap.find(service);
    if (msn != m_MainServiceNameMap.end()) {
        AcceptFeedback(msn->second, host, port, feedback);
    }
}


void COSGServiceMapper::x_NormalizePenalties(void)
{
    {{
        CFastReadGuard LOCK(m_AllServerRatingsLock);
        if (m_AllServerRatingsTimer.Elapsed()
            < m_PenaltyNormalizationInterval) {
            return;
        }
    }}

    CFastWriteGuard LOCK(m_AllServerRatingsLock);
    // Recheck, as another thread could have just taken care of it.
    double elapsed = m_AllServerRatingsTimer.Elapsed();
    if (elapsed < m_PenaltyNormalizationInterval) {
        return;
    }

    double decay = exp(m_PenaltyDecayRate * elapsed);
    _TRACE("Decaying penalties by " << decay << " after " << elapsed
           << " s");
    NON_CONST_ITERATE (TAllServerRatings, it, m_AllServerRatings) {
        NON_CONST_ITERATE (TServerRatings, it2, it->second) { 
            it2->second.penalty
                = (it2->second.penalty - m_InitialPenalty) * decay
                + m_InitialPenalty;
        }
    }
    m_AllServerRatingsTimer.Restart();
}


COSGServiceMapper::SServerRating& COSGServiceMapper::x_SetRating
(TServerRatings& ratings, Uint4 host, Uint2 port, bool* was_new,
 CTempString name) const
{
    TOSGEndpointKey          key = g_OSG_MakeEndpointKey(host, port);
    TServerRatings::iterator it  = ratings.lower_bound(key);

    if (it == ratings.end()  ||  it->first != key) {
        if (was_new != NULL) {
            *was_new = true;
        }
        TServerRatings::value_type node(key, SServerRating());
        string name_str
            = (name.empty() ? CSocketAPI::HostPortToString(host, port)
               : string(name));
        node.second.ref.Reset(new CDBServer(name_str, host, port, kMax_Auto));
        node.second.penalty  = m_InitialPenalty;
        node.second.excluded = false;
        it = ratings.insert(it, node);
    }

    return it->second;
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
