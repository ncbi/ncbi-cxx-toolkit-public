/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Authors: Sema Kachalo
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/macro/Suspect_rule_set.hpp>

#include <serial/objistrasn.hpp>
#include <mutex>
#include <serial/serial.hpp>
#include <util/compile_time.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

//////////////////////////////////////////////////////////////////////////

#define _FSM_RULES static const unsigned char s_Defaultorganelleproducts[]
#define _FSM_EMIT(size) static constexpr bool s_Defaultorganelleproducts_emit[]
#define _FSM_HITS static map<size_t, vector<size_t>> s_Defaultorganelleproducts_hits
#define _FSM_STATES static size_t s_Defaultorganelleproducts_states[]
#include "organelle_products.inc"
#undef _FSM_EMIT
#undef _FSM_HITS
#undef _FSM_STATES
#undef _FSM_RULES

static CConstRef<CSuspect_rule_set> s_InitializeOrganelleProductRules(const string& name)
{
    static bool s_OrganelleProductRulesInitialized = false;
    static CRef<CSuspect_rule_set> s_OrganelleProductRules;
    static std::mutex mutex;

    std::lock_guard<std::mutex> g{mutex};

    if (s_OrganelleProductRulesInitialized) {
        return s_OrganelleProductRules;
    }
    s_OrganelleProductRules.Reset(new CSuspect_rule_set());
    //string file = name.empty() ? g_FindDataFile("organelle_products.prt") : name;

    if (!name.empty()) {
        LOG_POST("Reading from " + name + " for organelle products");

        unique_ptr<CObjectIStream> istr (CObjectIStream::Open(eSerial_AsnText, name));
        *istr >> *s_OrganelleProductRules;
        s_OrganelleProductRules->SetPrecompiledData(nullptr, nullptr, nullptr);
    }
    if (!s_OrganelleProductRules->IsSet()) {
        //LOG_POST("Falling back on built-in data for organelle products");
        unique_ptr<CObjectIStream> istr (CObjectIStream::CreateFromBuffer(eSerial_AsnBinary, (const char*)s_Defaultorganelleproducts, sizeof(s_Defaultorganelleproducts)));
        auto types = istr->GuessDataType({CSuspect_rule_set::GetTypeInfo()});
        _ASSERT(types.size() == 1 && *types.begin() == CSuspect_rule_set::GetTypeInfo());
        *istr >> *s_OrganelleProductRules;

        s_OrganelleProductRules->SetPrecompiledData(s_Defaultorganelleproducts_emit, &s_Defaultorganelleproducts_hits, s_Defaultorganelleproducts_states);
    }

    s_OrganelleProductRulesInitialized = true;

    return s_OrganelleProductRules;
}


#define _FSM_RULES static const unsigned char s_Defaultproductrules[]
#define _FSM_EMIT(size) static constexpr bool s_Defaultproductrules_emit[]
#define _FSM_HITS static map<size_t, vector<size_t>> s_Defaultproductrules_hits
#define _FSM_STATES static size_t s_Defaultproductrules_states[]
#include "product_rules.inc"
#undef _FSM_EMIT
#undef _FSM_HITS
#undef _FSM_STATES
#undef _FSM_RULES

static CConstRef<CSuspect_rule_set> s_InitializeProductRules(const string& name)
{
    static bool s_ProductRulesInitialized = false;
    static string  s_ProductRulesFileName;
    static CRef<CSuspect_rule_set> s_ProductRules;
    static std::mutex mutex;

    std::lock_guard<std::mutex> g{mutex};

    if (s_ProductRulesInitialized && name == s_ProductRulesFileName) {
        return s_ProductRules;
    }
    s_ProductRules.Reset(new CSuspect_rule_set());
    s_ProductRulesFileName = name;
    //string file = name.empty() ? g_FindDataFile("product_rules.prt") : name;

    if (!name.empty()) {
        LOG_POST("Reading from " + name + " for suspect product rules");

        unique_ptr<CObjectIStream> istr (CObjectIStream::Open(eSerial_AsnText, name));
        *istr >> *s_ProductRules;
        s_ProductRules->SetPrecompiledData(nullptr, nullptr, nullptr);
    }
    if (!s_ProductRules->IsSet()) {
        //LOG_POST("Falling back on built-in data for suspect product rules");
        unique_ptr<CObjectIStream> istr (CObjectIStream::CreateFromBuffer(eSerial_AsnBinary, (const char*)s_Defaultproductrules, sizeof(s_Defaultproductrules)));
        auto types = istr->GuessDataType({CSuspect_rule_set::GetTypeInfo()});
        _ASSERT(types.size() == 1 && *types.begin() == CSuspect_rule_set::GetTypeInfo());
        *istr >> *s_ProductRules;
        s_ProductRules->SetPrecompiledData(s_Defaultproductrules_emit, &s_Defaultproductrules_hits, s_Defaultproductrules_states);
    }

    s_ProductRulesInitialized = true;
    return s_ProductRules;
}


CConstRef<CSuspect_rule_set> GetOrganelleProductRules(const string& name)
{
    return s_InitializeOrganelleProductRules(name);
}


CConstRef<CSuspect_rule_set> GetProductRules(const string& name)
{
    return s_InitializeProductRules(name);
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
