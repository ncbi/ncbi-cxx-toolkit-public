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
#include <serial/objostr.hpp>
#include <cstring>

//////////////////////////////////////////////////////////////////////////
namespace {

USING_SCOPE(ncbi);
USING_SCOPE(objects);
USING_SCOPE(FSM);

static bool xCompareAsnBlobs(const CCompiledFSM* compiled_fsm, CRef<CSerialObject> obj)
{
    if (compiled_fsm->m_rules_asn1.data() == nullptr || compiled_fsm->m_rules_asn1.size() == 0 || obj.Empty())
        return false;

    std::ostringstream ostring;
    std::unique_ptr<CObjectOStream> ostream(CObjectOStream::Open(eSerial_AsnBinary, ostring));
    *ostream << *obj;


    std::string_view view;
#if __cplusplus > 201703L
    view = ostring.view();
#else
    std::string serialized = ostring.str();
    view = serialized;
#endif

    if (view.size() != compiled_fsm->m_rules_asn1.size())
        return false;

    auto cmp = std::memcmp(view.data(), compiled_fsm->m_rules_asn1.data(), compiled_fsm->m_rules_asn1.size());

    return cmp == 0;
}

class CStaticRules
{
protected:
    string_view m_suffix;
    const CCompiledFSM* m_compiled_fsm = nullptr;
    bool m_Initialized = false;
    string  m_FileName;
    CRef<CSuspect_rule_set> m_Rules;
    std::mutex mutex;
public:
    CStaticRules(string_view suffix, const CCompiledFSM* fsm)
    : m_suffix{suffix}, m_compiled_fsm{fsm} {}

    CConstRef<CSuspect_rule_set> InitializeRules(const string& name)
    {
        std::lock_guard<std::mutex> g{mutex};

        if (m_Initialized && name == m_FileName) {
            return m_Rules;
        }

        auto s_Rules = Ref(new CSuspect_rule_set);

        if (!name.empty()) {
            s_Rules = Ref(new CSuspect_rule_set());
            LOG_POST("Reading from " + name + " for " + m_suffix);

            unique_ptr<CObjectIStream> istr (CObjectIStream::Open(eSerial_AsnText, name));
            *istr >> *s_Rules;
            //if (m_compiled_fsm && m_compiled_fsm->m_rules_asn1.data())
            if (xCompareAsnBlobs(m_compiled_fsm, s_Rules)) {
                LOG_POST("Content of " + name + " is the same as built-in.");
                s_Rules = Ref(new CSuspect_rule_set);
            } else {
                s_Rules->SetPrecompiledData(nullptr);
                m_Rules = s_Rules;
            }
        }
        if (!s_Rules->IsSet()) {
            //LOG_POST("Falling back on built-in data for " + m_suffix);
            unique_ptr<CObjectIStream> istr (CObjectIStream::CreateFromBuffer(eSerial_AsnBinary, (const char*)m_compiled_fsm->m_rules_asn1.data(), m_compiled_fsm->m_rules_asn1.size()));
            //auto types = istr->GuessDataType({CSuspect_rule_set::GetTypeInfo()});
            //_ASSERT(types.size() == 1 && *types.begin() == CSuspect_rule_set::GetTypeInfo());
            *istr >> *s_Rules;
            //s_Rules->SetPrecompiledData(nullptr);
            s_Rules->SetPrecompiledData(m_compiled_fsm);
            m_Rules = s_Rules;
        }

        m_FileName = name;
        m_Initialized = true;
        return m_Rules;
    }
};

}

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);
USING_SCOPE(FSM);

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
