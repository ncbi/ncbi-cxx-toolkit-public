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
#include <misc/discrepancy/discrepancy.hpp>
#include <util/multipattern_search.hpp>
#include <util/impl/generated_fsm.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);
USING_SCOPE(FSM);

//////////////////////////////////////////////////////////////////////////

namespace {

class CStaticRules
{
protected:
    string_view m_suffix;
    const CCompiledFSM* m_compiled_fsm = nullptr;
    bool s_Initialized = false;
    string  s_FileName;
    CRef<CSuspect_rule_set> s_Rules;
    std::mutex mutex;
public:
    CStaticRules(string_view suffix, const CCompiledFSM* fsm)
    : m_suffix{suffix}, m_compiled_fsm{fsm} {}

    CConstRef<CSuspect_rule_set> InitializeRules(const string& name)
    {
        std::lock_guard<std::mutex> g{mutex};

        if (s_Initialized && name == s_FileName) {
            return s_Rules;
        }
        s_Rules.Reset(new CSuspect_rule_set());
        s_FileName = name;

        if (!name.empty()) {
            LOG_POST("Reading from " + name + " for " + m_suffix);

            unique_ptr<CObjectIStream> istr (CObjectIStream::Open(eSerial_AsnText, name));
            *istr >> *s_Rules;
            s_Rules->SetPrecompiledData(nullptr);
        }
        if (!s_Rules->IsSet()) {
            //LOG_POST("Falling back on built-in data for " + m_suffix);
            unique_ptr<CObjectIStream> istr (CObjectIStream::CreateFromBuffer(eSerial_AsnBinary, (const char*)m_compiled_fsm->m_rules_asn1.data(), m_compiled_fsm->m_rules_asn1.size()));
            //auto types = istr->GuessDataType({CSuspect_rule_set::GetTypeInfo()});
            //_ASSERT(types.size() == 1 && *types.begin() == CSuspect_rule_set::GetTypeInfo());
            *istr >> *s_Rules;
            s_Rules->SetPrecompiledData(m_compiled_fsm);
        }

        s_Initialized = true;
        return s_Rules;
    }
};

}

BEGIN_SCOPE(NDiscrepancy)

CConstRef<CSuspect_rule_set> GetOrganelleProductRules(const string& name)
{
    #include "organelle_products.inc"
    static constexpr TLocalFSM s_FSM{s_compact, s_hits_init_1, s_hits_init_2, s_states, s_rules};

    static CStaticRules rules("organelle products", &s_FSM);

    return rules.InitializeRules(name);
}


CConstRef<CSuspect_rule_set> GetProductRules(const string& name)
{
    #include "product_rules.inc"
    static constexpr TLocalFSM s_FSM{s_compact, s_hits_init_1, s_hits_init_2, s_states, s_rules};

    static CStaticRules rules("suspect product rules", &s_FSM);

    return rules.InitializeRules(name);
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
