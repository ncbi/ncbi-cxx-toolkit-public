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
* Authors:  Justin Foley
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>

#include <objtools/logging/message.hpp>
#include <objtools/logging/listener.hpp>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <objtools/readers/mod_reader.hpp>
#include <objtools/readers/mod_error.hpp>
#include "mod_to_enum.hpp"
#include "descr_mod_apply.hpp"
//#include <util/compile_time.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//MAKE_CONST_MAP(s_TechStringToEnum, NStr::eCase, const char*, CMolInfo::TTech,
static const unordered_map<string,CMolInfo::TTech> s_TechStringToEnum =
{   { "?",                  CMolInfo::eTech_unknown },
    { "barcode",            CMolInfo::eTech_barcode },
    { "both",               CMolInfo::eTech_both },
    { "compositewgshtgs",   CMolInfo::eTech_composite_wgs_htgs },
    { "concepttrans",       CMolInfo::eTech_concept_trans },
    { "concepttransa",      CMolInfo::eTech_concept_trans_a },
    { "derived",            CMolInfo::eTech_derived },
    { "est",                CMolInfo::eTech_est },
    { "flicdna",            CMolInfo::eTech_fli_cdna },
    { "geneticmap",         CMolInfo::eTech_genemap },
    { "htc",                CMolInfo::eTech_htc },
    { "htgs0",              CMolInfo::eTech_htgs_0 },
    { "htgs1",              CMolInfo::eTech_htgs_1 },
    { "htgs2",              CMolInfo::eTech_htgs_2 },
    { "htgs3",              CMolInfo::eTech_htgs_3 },
    { "physicalmap",        CMolInfo::eTech_physmap },
    { "seqpept",            CMolInfo::eTech_seq_pept },
    { "seqpepthomol",       CMolInfo::eTech_seq_pept_homol },
    { "seqpeptoverlap",     CMolInfo::eTech_seq_pept_overlap },
    { "standard",           CMolInfo::eTech_standard },
    { "sts",                CMolInfo::eTech_sts },
    { "survey",             CMolInfo::eTech_survey },
    { "targeted",           CMolInfo::eTech_targeted },
    { "tsa",                CMolInfo::eTech_tsa },
    { "wgs",                CMolInfo::eTech_wgs }
};
//);


//MAKE_CONST_MAP(s_CompletenessStringToEnum, NStr::eCase, const char*, CMolInfo::TCompleteness,

static const unordered_map<string,CMolInfo::TCompleteness> s_CompletenessStringToEnum =
{   { "complete",  CMolInfo::eCompleteness_complete  },
    { "hasleft",   CMolInfo::eCompleteness_has_left  },
    { "hasright",  CMolInfo::eCompleteness_has_right  },
    { "noends",    CMolInfo::eCompleteness_no_ends  },
    { "noleft",    CMolInfo::eCompleteness_no_left  },
    { "noright",   CMolInfo::eCompleteness_no_right  },
    { "partial",   CMolInfo::eCompleteness_partial  }
};
//);


static const auto s_OrgModStringToEnum = g_InitModNameOrgSubtypeMap();

static const auto s_SubSourceStringToEnum = g_InitModNameSubSrcSubtypeMap();


class CDescrCache
{
public:

    struct SDescrContainer_Base {
        virtual ~SDescrContainer_Base(void) = default;
        virtual bool IsSet(void) const = 0;
        virtual CSeq_descr& SetDescr(void) = 0;
    };

    using TDescrContainer = SDescrContainer_Base;
    using TSubtype = CBioSource::TSubtype;
    using TOrgMods = COrgName::TMod;
    using TPCRReactionSet = CPCRReactionSet;

    CDescrCache(CBioseq& bioseq);

    CUser_object& SetDBLink(void);
    CUser_object& SetTpaAssembly(void);
    CUser_object& SetGenomeProjects(void);
    CUser_object& SetFileTrack(void);

    CGB_block& SetGBblock(void);
    CMolInfo& SetMolInfo(void);
    CBioSource& SetBioSource(void);

    string& SetComment(void);
    CPubdesc& SetPubdesc(void);

    TSubtype& SetSubtype(void);
    TOrgMods& SetOrgMods(void);
    CPCRReactionSet& SetPCR_primers(void);

private:
    enum EChoice : size_t {
        eDBLink = 1,
        eTpa = 2,
        eGenomeProjects = 3,
        eMolInfo = 4,
        eGBblock = 5,
        eBioSource = 6,
        eFileTrack = 7,
    };

    void x_SetUserType(const string& type, CUser_object& user_object);

    CSeqdesc& x_SetDescriptor(const EChoice eChoice,
                              function<bool(const CSeqdesc&)> f_verify,
                              function<CRef<CSeqdesc>(void)> f_create);

    CSeqdesc& x_SetDescriptor(const EChoice eChoice,
                              function<bool(const CSeqdesc&)> f_verify,
                              function<CRef<CSeqdesc>(void)> f_create,
                              TDescrContainer* pDescrContainer);

    TSubtype* m_pSubtype = nullptr;
    TOrgMods* m_pOrgMods = nullptr;
    CPCRReactionSet* m_pPCRReactionSet = nullptr;
    bool m_FirstComment = true;
    bool m_FirstPubdesc = true;
    bool m_HasSetTaxid  = false;
    using TMap = unordered_map<EChoice, CRef<CSeqdesc>, hash<underlying_type<EChoice>::type>>;
    TMap m_Cache;

    TDescrContainer* m_pPrimaryContainer;
    unique_ptr<TDescrContainer> m_pNucProtSetContainer;
    unique_ptr<TDescrContainer> m_pBioseqContainer;
};


CDescrModApply::CDescrModApply(CBioseq& bioseq,
        FReportError fReportError,
        TSkippedMods& skipped_mods) :
m_pDescrCache(new CDescrCache(bioseq)),
m_fReportError(fReportError),
m_SkippedMods(skipped_mods)
{}


CDescrModApply::~CDescrModApply() = default;


bool CDescrModApply::Apply(const TModEntry& mod_entry)
{
    if (x_TryBioSourceMod(mod_entry, m_PreserveTaxId)) {
        return true;
    }

    {
        using TMemFuncPtr = void (CDescrModApply::*)(const TModEntry&);


        static const unordered_map<string,TMemFuncPtr>
            s_MethodMap = {{"sra", &CDescrModApply::x_SetDBLink},
                           {"bioproject", &CDescrModApply::x_SetDBLink},
                           {"biosample", &CDescrModApply::x_SetDBLink},
                           {"mol-type", &CDescrModApply::x_SetMolInfoType},
                           {"completeness", &CDescrModApply::x_SetMolInfoCompleteness},
                           {"tech", &CDescrModApply::x_SetMolInfoTech},
                           {"primary-accession", &CDescrModApply::x_SetTpaAssembly},
                           {"secondary-accession", &CDescrModApply::x_SetGBblockIds},
                           {"keyword", &CDescrModApply::x_SetGBblockKeywords},
                           {"project", &CDescrModApply::x_SetGenomeProjects},
                           {"comment", &CDescrModApply::x_SetComment},
                           {"pmid", &CDescrModApply::x_SetPMID},
                           {"ft-map", &CDescrModApply::x_SetFileTrack},
                           {"ft-mod", &CDescrModApply::x_SetFileTrack}
                          };
        const auto& mod_name = x_GetModName(mod_entry);
        auto it = s_MethodMap.find(mod_name);
        if (it != s_MethodMap.end()) {
            auto mem_func_ptr = it->second;
            (this->*mem_func_ptr)(mod_entry);
            return true;
        }
    }
    return false;
}


bool CDescrModApply::x_TryBioSourceMod(const TModEntry& mod_entry, bool& preserve_taxid)
{
    const auto& name = x_GetModName(mod_entry);
    if (name == "location") {
        const auto& value = x_GetModValue(mod_entry);
        static const auto s_GenomeStringToEnum = g_InitModNameGenomeMap();
        auto it = s_GenomeStringToEnum.find(g_GetNormalizedModVal(value));
        if (it == s_GenomeStringToEnum.end()) {
            x_ReportInvalidValue(mod_entry.second.front());
            return true;
        }
        m_pDescrCache->SetBioSource().SetGenome(it->second);
        return true;
    }

    if (name == "origin") {
        const auto& value = x_GetModValue(mod_entry);
        static const auto s_OriginStringToEnum = g_InitModNameOriginMap();
        auto it = s_OriginStringToEnum.find(g_GetNormalizedModVal(value));
        if (it == s_OriginStringToEnum.end()) {
            x_ReportInvalidValue(mod_entry.second.front());
            return true;
        }
        m_pDescrCache->SetBioSource().SetOrigin(it->second);
        return true;
    }

    if (name == "focus") {
        const auto& value = x_GetModValue(mod_entry);
        if (NStr::EqualNocase(value, "true")) {
            m_pDescrCache->SetBioSource().SetIs_focus();
        }
        else
        if (NStr::EqualNocase(value, "false")) {
            x_ReportInvalidValue(mod_entry.second.front());
        }
        return true;
    }


    { // check to see if this is a subsource mod
         auto it = s_SubSourceStringToEnum.find(name);
         if (it != s_SubSourceStringToEnum.end()) {
            x_SetSubtype(mod_entry);
            return true;
         }
    }

    if (x_TryPCRPrimerMod(mod_entry)) {
        return true;
    }

    if (x_TryOrgRefMod(mod_entry, preserve_taxid)) {
        return true;
    }
    return false;
}


void CDescrModApply::x_SetSubtype(const TModEntry& mod_entry)
{
    const auto& mod_name = x_GetModName(mod_entry);
    const auto subtype = s_SubSourceStringToEnum.at(mod_name);
    if (subtype == CSubSource::eSubtype_plasmid_name) {
        m_pDescrCache->SetBioSource().SetGenome(CBioSource::eGenome_plasmid);
    }
    const auto needs_no_text = CSubSource::NeedsNoText(subtype);
    CBioSource::TSubtype subsources;
    for (const auto& mod : mod_entry.second) {
        const auto& value = mod.GetValue();
        if (needs_no_text &&
            !NStr::EqualNocase(value, "true")) {
            x_ReportInvalidValue(mod);
            return;
        }
        auto pSubSource = Ref(new CSubSource(subtype,value));
        if (mod.IsSetAttrib()) {
            pSubSource->SetAttrib(mod.GetAttrib());
        }
        m_pDescrCache->SetSubtype().push_back(move(pSubSource));
    }
}


static void s_SetPrimerNames(const string& primer_names, CPCRPrimerSet& primer_set)
{
    const auto set_size = primer_set.Get().size();
    vector<string> names;
    NStr::Split(primer_names, ":", names, NStr::fSplit_Tokenize);
    const auto num_names = names.size();

    auto it = primer_set.Set().begin();
    for (size_t i=0; i<num_names; ++i) {
        if (NStr::IsBlank(names[i])) {
            continue;
        }
        if (i<set_size) {
            (*it)->SetName().Set(names[i]);
            ++it;
        }
        else {
            auto pPrimer = Ref(new CPCRPrimer());
            pPrimer->SetName().Set(names[i]);
            primer_set.Set().push_back(move(pPrimer));
        }
    }
}


static void s_SetPrimerSeqs(const string& primer_seqs, CPCRPrimerSet& primer_set)
{
    const auto set_size = primer_set.Get().size();
    vector<string> seqs;
    NStr::Split(primer_seqs, ":", seqs, NStr::fSplit_Tokenize);
    const auto num_seqs = seqs.size();

    auto it = primer_set.Set().begin();
    for (size_t i=0; i<num_seqs; ++i) {
        if (NStr::IsBlank(seqs[i])) {
            continue;
        }
        if (i<set_size) {
            (*it)->SetSeq().Set(seqs[i]);
            ++it;
        }
        else {
            auto pPrimer = Ref(new CPCRPrimer());
            pPrimer->SetSeq().Set(seqs[i]);
            primer_set.Set().push_back(move(pPrimer));
        }
    }
}


static void s_AppendPrimerNames(const string& mod, vector<string>& reaction_names)
{
    vector<string> names;
    NStr::Split(mod, ",", names, NStr::fSplit_Tokenize);
    reaction_names.insert(reaction_names.end(), names.begin(), names.end());
}


static void s_AppendPrimerSeqs(const string& mod, vector<string>& reaction_seqs)
{
    vector<string> seqs;
    NStr::Split(mod, ",", seqs, NStr::fSplit_Tokenize);
    if (seqs.size() > 1) {
        if (seqs.front().front() == '(') {
            seqs.front().erase(0,1);
        }
        if (seqs.back().back() == ')') {
            seqs.back().erase(seqs.back().size()-1,1);
        }
    }

    for (auto& seq : seqs) {
        reaction_seqs.push_back(NStr::ToLower(seq));
    }
}


bool CDescrModApply::x_TryPCRPrimerMod(const TModEntry& mod_entry)
{
    const auto& mod_name = x_GetModName(mod_entry);

    // Refactor to eliminate duplicated code
    if (mod_name == "fwd-primer-name") {
        vector<string> names;
        for (const auto& mod : mod_entry.second)
        {
            s_AppendPrimerNames(mod.GetValue(), names);
        }

        auto& pcr_reaction_set = m_pDescrCache->SetPCR_primers();
        auto it = pcr_reaction_set.Set().begin();
        for (const auto& reaction_names : names) {
            if (it == pcr_reaction_set.Set().end()) {
                auto pPCRReaction = Ref(new CPCRReaction());
                s_SetPrimerNames(reaction_names, pPCRReaction->SetForward());
                pcr_reaction_set.Set().push_back(move(pPCRReaction));
            }
            else {
                s_SetPrimerNames(reaction_names, (*it++)->SetForward());
            }
        }
        return true;
    }


    if (mod_name == "fwd-primer-seq") {
        vector<string> seqs;
        for (const auto& mod : mod_entry.second)
        {
            s_AppendPrimerSeqs(mod.GetValue(), seqs);
        }
        auto& pcr_reaction_set = m_pDescrCache->SetPCR_primers();
        auto it = pcr_reaction_set.Set().begin();
        for (const auto& reaction_seqs : seqs) {
            if (it == pcr_reaction_set.Set().end()) {
                auto pPCRReaction = Ref(new CPCRReaction());
                s_SetPrimerSeqs(reaction_seqs, pPCRReaction->SetForward());
                pcr_reaction_set.Set().push_back(move(pPCRReaction));
            }
            else {
                s_SetPrimerSeqs(reaction_seqs, (*it++)->SetForward());
            }
        }
        return true;
    }


    if(mod_name == "rev-primer-name")
    {
        vector<string> names;
        for (const auto& mod : mod_entry.second) {
            s_AppendPrimerNames(mod.GetValue(), names);
        }
        if (!names.empty()) {
            auto& pcr_reaction_set = m_pDescrCache->SetPCR_primers();
            const size_t num_reactions = pcr_reaction_set.Get().size();
            const size_t num_names = names.size();
            if (num_names <= num_reactions) {
                auto it = pcr_reaction_set.Set().rbegin();
                for(int i=num_names-1; i>=0; --i) { // don't use auto here. i stops when at -1.
                    s_SetPrimerNames(names[i], (*it++)->SetReverse());
                }
            }
            else {

                auto it = pcr_reaction_set.Set().begin();
                for (size_t i=0; i<num_reactions; ++i) {
                     s_SetPrimerNames(names[i], (*it++)->SetReverse());
                }

                for (auto i=num_reactions; i<num_names; ++i) {
                    auto pPCRReaction = Ref(new CPCRReaction());
                    s_SetPrimerNames(names[i], pPCRReaction->SetReverse());
                    pcr_reaction_set.Set().push_back(move(pPCRReaction));
                }
            }
        }
        return true;
    }


    if(mod_name == "rev-primer-seq")
    {
        vector<string> seqs;
        for (const auto& mod : mod_entry.second) {
            s_AppendPrimerSeqs(mod.GetValue(), seqs);
        }
        if (!seqs.empty()) {
            auto& pcr_reaction_set = m_pDescrCache->SetPCR_primers();
            const size_t num_reactions = pcr_reaction_set.Get().size();
            const size_t num_seqs = seqs.size();
            if (num_seqs <= num_reactions) {
                auto it = pcr_reaction_set.Set().rbegin();
                for(int i=num_seqs-1; i>=0; --i) { // don't use auto here. i stops at -1.
                    s_SetPrimerSeqs(seqs[i], (*it++)->SetReverse());
                }
            }
            else {
                auto it = pcr_reaction_set.Set().begin();
                for (size_t i=0; i<num_reactions; ++i) {
                     s_SetPrimerSeqs(seqs[i], (*it++)->SetReverse());
                }

                for (auto i=num_reactions; i<num_seqs; ++i) {
                    auto pPCRReaction = Ref(new CPCRReaction());
                    s_SetPrimerSeqs(seqs[i], pPCRReaction->SetReverse());
                    pcr_reaction_set.Set().push_back(move(pPCRReaction));
                }
            }
        }
        return true;
    }

    return false;
}


bool CDescrModApply::x_TryOrgRefMod(const TModEntry& mod_entry, bool& preserve_taxid)
{
    const auto& name = x_GetModName(mod_entry);
    if (name == "taxname") {
        const auto& value = x_GetModValue(mod_entry);
        m_pDescrCache->SetBioSource().SetOrg().SetTaxname(value);
        if (!preserve_taxid &&
             m_pDescrCache->SetBioSource().GetOrg().GetTaxId() != ZERO_TAX_ID) {
            // clear taxid if it does not occur in this modifier set
            m_pDescrCache->SetBioSource().SetOrg().SetTaxId(ZERO_TAX_ID);
        }
        return true;
    }

    if (name == "taxid") {
        const auto& value = x_GetModValue(mod_entry);
        TTaxId taxid;
        try {
            taxid = NStr::StringToNumeric<TTaxId>(value);
        }
        catch (...) {
            x_ReportInvalidValue(mod_entry.second.front(), "Integer value expected.");
            return true;
        }
        m_pDescrCache->SetBioSource().SetOrg().SetTaxId(taxid);
        preserve_taxid = true;
        return true;
    }


    if (name == "common") {
        const auto& value = x_GetModValue(mod_entry);
        m_pDescrCache->SetBioSource().SetOrg().SetCommon(value);
        return true;
    }

    if (name == "dbxref") {
        x_SetDBxref(mod_entry);
        return true;
    }

    if (x_TryOrgNameMod(mod_entry)) {
        return true;
    }
    return false;
}


void CDescrModApply::x_SetDBxref(const TModEntry& mod_entry)
{
   vector<CRef<CDbtag>> dbtags;
   for (const auto& value_attrib : mod_entry.second) {
       const auto& value = value_attrib.GetValue();

       auto colon_pos = value.find(":");
       string database;
       string tag;
       if (colon_pos < (value.length()-1)) {
           database = value.substr(0, colon_pos);
           tag = value.substr(colon_pos+1);
       }
       else {
            database = "?";
            tag = value;
       }
       auto pDbtag = Ref(new CDbtag());
       pDbtag->SetDb(database);
       pDbtag->SetTag().SetStr(tag);
       dbtags.push_back(move(pDbtag));
   }

   m_pDescrCache->SetBioSource().SetOrg().SetDb() = dbtags;
}


bool CDescrModApply::x_TryOrgNameMod(const TModEntry& mod_entry)
{
    const auto& name = x_GetModName(mod_entry);
    if (name == "lineage") {
        const auto& value = x_GetModValue(mod_entry);
        m_pDescrCache->SetBioSource().SetOrg().SetOrgname().SetLineage(value);
        return true;
    }

    if (name == "division") {
        const auto& value = x_GetModValue(mod_entry);
        m_pDescrCache->SetBioSource().SetOrg().SetOrgname().SetDiv(value);
        return true;
    }

    // check for gcode, mgcode, pgcode
    using TSetCodeMemFn = void (COrgName::*)(int);
    using TFunction = function<void(COrgName&, int)>;
    static const
        unordered_map<string, TFunction>
                s_GetCodeSetterMethods =
                {{"gcode",  TFunction(static_cast<TSetCodeMemFn>(&COrgName::SetGcode))},
                 {"mgcode",  TFunction(static_cast<TSetCodeMemFn>(&COrgName::SetMgcode))},
                 {"pgcode",  TFunction(static_cast<TSetCodeMemFn>(&COrgName::SetPgcode))}};

    auto it = s_GetCodeSetterMethods.find(name);
    if (it != s_GetCodeSetterMethods.end()) {
        const auto& value = x_GetModValue(mod_entry);
        int code;
        try {
            code = NStr::StringToInt(value);
        }
        catch (...) {
            x_ReportInvalidValue(mod_entry.second.front(), "Integer value expected.");
            return true;
        }
        it->second(m_pDescrCache->SetBioSource().SetOrg().SetOrgname(), code);
        return true;
    }

    { //  check for orgmod
        auto it = s_OrgModStringToEnum.find(name);
        if (it != s_OrgModStringToEnum.end()) {
            x_SetOrgMod(mod_entry);
            return true;
        }
    }
    return false;
}


void CDescrModApply::x_SetOrgMod(const TModEntry& mod_entry)
{
    const auto& subtype = s_OrgModStringToEnum.at(x_GetModName(mod_entry));
    for (const auto& mod : mod_entry.second) {
        const auto& subname = mod.GetValue();
        auto pOrgMod = Ref(new COrgMod(subtype,subname));
        if (mod.IsSetAttrib()) {
            pOrgMod->SetAttrib(mod.GetAttrib());
        }
        m_pDescrCache->SetOrgMods().push_back(move(pOrgMod));
    }
}


void CDescrModApply::x_SetDBLink(const TModEntry& mod_entry)
{
    const auto& name = x_GetModName(mod_entry);
    static const unordered_map<string, string> s_NameToLabel =
    {{"sra", "Sequence Read Archive"},
     {"biosample", "BioSample"},
     {"bioproject", "BioProject"}};

    const auto& label = s_NameToLabel.at(name);

    x_SetDBLinkField(label, mod_entry, *m_pDescrCache);
}


void CDescrModApply::x_SetDBLinkField(const string& label,
        const TModEntry& mod_entry,
        CDescrCache& descr_cache)
{
    list<CTempString> value_list;
    for (const auto& mod : mod_entry.second) {
        list<CTempString> value_sublist;
        const auto& vals = mod.GetValue();
        NStr::Split(vals, ",; \t", value_sublist, NStr::fSplit_Tokenize);
        value_list.splice(value_list.end(), value_sublist);
    }

    if (value_list.empty()) {
        return;
    }
    x_SetDBLinkFieldVals(label, value_list, descr_cache.SetDBLink());
}


void CDescrModApply::x_SetDBLinkFieldVals(const string& label,
        const list<CTempString>& vals,
        CUser_object& dblink)
{
    if (vals.empty()) {
        return;
    }

    CRef<CUser_field> pField;
    if (dblink.IsSetData()) {
        for (auto pUserField : dblink.SetData()) {
            if (pUserField &&
                pUserField->IsSetLabel() &&
                pUserField->GetLabel().IsStr() &&
                NStr::EqualNocase(pUserField->GetLabel().GetStr(), label)) {
                pField = pUserField;
                break;
            }
        }
    }

    if (!pField) {
        pField = Ref(new CUser_field());
        pField->SetLabel().SetStr() = label;
        dblink.SetData().push_back(pField);
    }

    pField->SetData().SetStrs().assign(vals.begin(), vals.end());
}


void CDescrModApply::x_SetMolInfoType(const TModEntry& mod_entry)
{
    string value = x_GetModValue(mod_entry);
    auto it = g_BiomolStringToEnum.find(g_GetNormalizedModVal(value));
    if (it != g_BiomolStringToEnum.end()) {
        m_pDescrCache->SetMolInfo().SetBiomol(it->second);
        return;
    }
    x_ReportInvalidValue(mod_entry.second.front());
}


void CDescrModApply::x_SetMolInfoTech(const TModEntry& mod_entry)
{
    string value = x_GetModValue(mod_entry);
    auto it = s_TechStringToEnum.find(g_GetNormalizedModVal(value));
    if (it != s_TechStringToEnum.end()) {
        m_pDescrCache->SetMolInfo().SetTech(it->second);
        return;
    }
    x_ReportInvalidValue(mod_entry.second.front());
}


void CDescrModApply::x_SetMolInfoCompleteness(const TModEntry& mod_entry)
{
    string value = x_GetModValue(mod_entry);
    auto it = s_CompletenessStringToEnum.find(g_GetNormalizedModVal(value));
    if (it != s_CompletenessStringToEnum.end()) {
        m_pDescrCache->SetMolInfo().SetCompleteness(it->second);
        return;
    }
    x_ReportInvalidValue(mod_entry.second.front());
}


void CDescrModApply::x_SetFileTrack(const TModEntry& mod_entry)
{
    list<string> vals;
    for (const auto& mod : mod_entry.second) {
        vals.push_back(mod.GetValue());
    }

    string label = (mod_entry.first == "ft-map") ?
                    "Map-FileTrackURL" :
                    "BaseModification-FileTrackURL";

    for (auto val : vals) {
        auto& user = m_pDescrCache->SetFileTrack();
        auto pField = Ref(new CUser_field());
        pField->SetLabel().SetStr(label);
        pField->SetNum(1);
        pField->SetData().SetStr(val);
        user.SetData().push_back(pField);
    }
}


void CDescrModApply::x_SetTpaAssembly(const TModEntry& mod_entry)
{
    list<CStringUTF8> accession_list;
    for (const auto& mod : mod_entry.second) {
        list<CTempString> value_sublist;
        const auto& vals = mod.GetValue();
        NStr::Split(vals, ",; \t", value_sublist, NStr::fSplit_Tokenize);

        list<CStringUTF8> accession_sublist;
        try {
            transform(value_sublist.begin(), value_sublist.end(), back_inserter(accession_sublist),
                    [](const CTempString& val) { return CUtf8::AsUTF8(val, eEncoding_UTF8); });
        }
        catch (...) {
            x_ReportInvalidValue(mod);
            continue;
        }
        accession_list.splice(accession_list.end(), accession_sublist);
    }

    if (accession_list.empty()) {
        return;
    }

    auto make_user_field = [](const CStringUTF8& accession) {
        auto pField = Ref(new CUser_field());
        pField->SetLabel().SetId(0);
        auto pSubfield = Ref(new CUser_field());
        pSubfield->SetLabel().SetStr("accession");
        pSubfield->SetData().SetStr(accession);
        pField->SetData().SetFields().push_back(move(pSubfield));
        return pField;
    };

    auto& user = m_pDescrCache->SetTpaAssembly();
    user.SetData().resize(accession_list.size());
    transform(accession_list.begin(), accession_list.end(),
            user.SetData().begin(), make_user_field);
}


void CDescrModApply::x_SetGBblockIds(const TModEntry& mod_entry)
{
    list<string> id_list;
    for (const auto& mod : mod_entry.second) {
        list<CTempString> value_sublist;
        const auto& vals = mod.GetValue();
        NStr::Split(vals, ",; \t", value_sublist, NStr::fSplit_Tokenize);
        for (const auto& val : value_sublist) {
            string value = NStr::TruncateSpaces_Unsafe(val);
            try {
                SSeqIdRange idrange(value);
                id_list.insert(id_list.end(),idrange.begin(), idrange.end());
            }
            catch(...)
            {
                id_list.push_back(value);
            }
        }
    }
    auto& gb_block = m_pDescrCache->SetGBblock();
    gb_block.SetExtra_accessions().assign(id_list.begin(), id_list.end());
}


void CDescrModApply::x_SetGBblockKeywords(const TModEntry& mod_entry)
{
    list<CTempString> value_list;
    for (const auto& mod : mod_entry.second) {
        list<CTempString> value_sublist;
        const auto& vals = mod.GetValue();
        NStr::Split(vals, ",; \t", value_sublist, NStr::fSplit_Tokenize);
        value_list.splice(value_list.end(), value_sublist);
    }
    if (value_list.empty()) {
        return;
    }
    m_pDescrCache->SetGBblock().SetKeywords().assign(value_list.begin(), value_list.end());
}




void CDescrModApply::x_SetGenomeProjects(const TModEntry& mod_entry)
{
    list<int> id_list;
    for (const auto& mod : mod_entry.second) {
        list<CTempString> value_sublist;
        const auto& vals = mod.GetValue();
        NStr::Split(vals, ",; \t", value_sublist, NStr::fSplit_Tokenize);
        list<int> id_sublist;
        try {
            transform(value_sublist.begin(), value_sublist.end(), back_inserter(id_sublist),
                    [](const CTempString& val) { return NStr::StringToUInt(val); });
        }
        catch (...) {
            x_ReportInvalidValue(mod);
            continue;
        }
        id_list.splice(id_list.end(), id_sublist);
    }
    if (id_list.empty()) {
        return;
    }

    auto make_user_field = [](const int& id) {
        auto pField = Ref(new CUser_field());
        auto pSubfield = Ref(new CUser_field());
        pField->SetLabel().SetId(0);
        pSubfield->SetLabel().SetStr("ProjectID");
        pSubfield->SetData().SetInt(id);
        pField->SetData().SetFields().push_back(pSubfield);
        pSubfield.Reset(new CUser_field());
        pSubfield->SetLabel().SetStr("ParentID");
        pSubfield->SetData().SetInt(0);
        pField->SetData().SetFields().push_back(pSubfield);
        return pField;
    };

    auto& user = m_pDescrCache->SetGenomeProjects();
    user.SetData().resize(id_list.size());
    transform(id_list.begin(), id_list.end(),
            user.SetData().begin(), make_user_field);
}


void CDescrModApply::x_SetComment(const TModEntry& mod_entry)
{
    for (const auto& mod : mod_entry.second) {
        m_pDescrCache->SetComment() = mod.GetValue();
    }

}


void CDescrModApply::x_SetPMID(const TModEntry& mod_entry)
{
    for (const auto& mod : mod_entry.second)
    {
        const auto& value = mod.GetValue();
        TEntrezId pmid;
        try {
            pmid = NStr::StringToNumeric<TEntrezId>(value);
        }
        catch(...) {
            x_ReportInvalidValue(mod_entry.second.front(), "Expected integer value.");
            continue;
        }
        auto pPub = Ref(new CPub());
        pPub->SetPmid().Set(pmid);
        m_pDescrCache->SetPubdesc()
                      .SetPub()
                      .Set()
                      .push_back(move(pPub));
    }
}


const string& CDescrModApply::x_GetModName(const TModEntry& mod_entry)
{
    return CModHandler::GetCanonicalName(mod_entry);
}


const string& CDescrModApply::x_GetModValue(const TModEntry& mod_entry)
{
    return CModHandler::AssertReturnSingleValue(mod_entry);
}


void CDescrModApply::x_ReportInvalidValue(const CModData& mod_data,
                                    const string& add_msg)
{
    const auto& mod_name = mod_data.GetName();
    const auto& mod_value = mod_data.GetValue();
    string msg = "Invalid value: " + mod_name + "=" + mod_value + ".";
    if (!NStr::IsBlank(add_msg)) {
        msg += " " + add_msg;
    }

    if (m_fReportError) {
        m_fReportError(mod_data, msg, eDiag_Error, eModSubcode_InvalidValue);
        m_SkippedMods.push_back(mod_data);
        return;
    }

    NCBI_THROW(CModReaderException, eInvalidValue, msg);
}


static bool s_IsUserType(const CUser_object& user_object, const string& type)
{
    return (user_object.IsSetType() &&
            user_object.GetType().IsStr() &&
            user_object.GetType().GetStr() == type);
}


template<class TObject>
class CDescrContainer : public CDescrCache::TDescrContainer
{
public:
    CDescrContainer(TObject& object) : m_Object(object) {}

    bool IsSet(void) const {
        return m_Object.IsSetDescr();
    }

    CSeq_descr& SetDescr(void) {
        return m_Object.SetDescr();
    }

private:
    TObject& m_Object;
};

//}

CDescrCache::CDescrCache(CBioseq& bioseq)
    : m_pBioseqContainer(new CDescrContainer<CBioseq>(bioseq))
{
    auto pParentSet = bioseq.GetParentSet();

    if (pParentSet &&
        pParentSet->IsSetClass() &&
        pParentSet->GetClass() == CBioseq_set::eClass_nuc_prot)
    {
        auto& bioseq_set = const_cast<CBioseq_set&>(*pParentSet);
        m_pNucProtSetContainer.reset(new CDescrContainer<CBioseq_set>(bioseq_set));
        m_pPrimaryContainer = m_pNucProtSetContainer.get();
    }
    else {
        m_pPrimaryContainer = m_pBioseqContainer.get();
    }

    assert(m_pPrimaryContainer);
}



void CDescrCache::x_SetUserType(const string& type,
                                     CUser_object& user_object)
{
    user_object.SetType().SetStr(type);
}


static bool s_EmptyAfterRemovingPMID(CRef<CSeqdesc>& pDesc)
{
    if (!pDesc ||
        !pDesc->IsPub()) {
        return false;
    }

    auto& pub_desc = pDesc->SetPub();
    pub_desc.SetPub().Set().remove_if([](const CRef<CPub>& pPub) { return (pPub && pPub->IsPmid()); });
    return pub_desc.GetPub().Get().empty();
}


CPubdesc& CDescrCache::SetPubdesc()
{
    assert(m_pPrimaryContainer);

    if (m_FirstPubdesc) {
        if (m_pPrimaryContainer->IsSet()) {  // Probably need to change this
            m_pPrimaryContainer->SetDescr().Set().remove_if(s_EmptyAfterRemovingPMID);
        }
        m_FirstPubdesc = false;
    }

    auto pDesc = Ref(new CSeqdesc());
    m_pPrimaryContainer->SetDescr().Set().push_back(pDesc);
    return pDesc->SetPub();
}


string& CDescrCache::SetComment()
{
    assert(m_pPrimaryContainer);

    if (m_FirstComment) {
        if (m_pPrimaryContainer->IsSet()) {
            m_pPrimaryContainer->SetDescr().Set().remove_if([](const CRef<CSeqdesc>& pDesc) { return (pDesc && pDesc->IsComment()); });
        }
        m_FirstComment = false;
    }

    auto pDesc = Ref(new CSeqdesc());
    m_pPrimaryContainer->SetDescr().Set().push_back(pDesc);
    return pDesc->SetComment();
}


CUser_object& CDescrCache::SetDBLink()
{
    return x_SetDescriptor(eDBLink,
        [](const CSeqdesc& desc) {
            return (desc.IsUser() && desc.GetUser().IsDBLink());
        },
        []() {
            auto pDesc = Ref(new CSeqdesc());
            pDesc->SetUser().SetObjectType(CUser_object::eObjectType_DBLink);
            return pDesc;
        }).SetUser();
}



CUser_object& CDescrCache::SetFileTrack()
{
    return x_SetDescriptor(eFileTrack,
        [](const CSeqdesc& desc) {
            return (desc.IsUser() && s_IsUserType(desc.GetUser(), "FileTrack"));
        },
        [this]() {
            auto pDesc = Ref(new CSeqdesc());
            x_SetUserType("FileTrack", pDesc->SetUser());
            return pDesc;
        }
    ).SetUser();
}


CUser_object& CDescrCache::SetTpaAssembly()
{
    return x_SetDescriptor(eTpa,
        [](const CSeqdesc& desc) {
            return (desc.IsUser() && s_IsUserType(desc.GetUser(), "TpaAssembly"));
        },
        [this]() {
            auto pDesc = Ref(new CSeqdesc());
            x_SetUserType("TpaAssembly", pDesc->SetUser());
            return pDesc;
        }
    ).SetUser();
}


CUser_object& CDescrCache::SetGenomeProjects()
{
        return x_SetDescriptor(eGenomeProjects,
        [](const CSeqdesc& desc) {
            return (desc.IsUser() && s_IsUserType(desc.GetUser(), "GenomeProjectsDB"));
        },
        [this]() {
            auto pDesc = Ref(new CSeqdesc());
            x_SetUserType("GenomeProjectsDB", pDesc->SetUser());
            return pDesc;
        }
    ).SetUser();
}


CGB_block& CDescrCache::SetGBblock()
{
    return x_SetDescriptor(eGBblock,
        [](const CSeqdesc& desc) {
            return desc.IsGenbank();
        },
        []() {
            auto pDesc = Ref(new CSeqdesc());
            pDesc->SetGenbank();
            return pDesc;
        }
    ).SetGenbank();
}


CMolInfo& CDescrCache::SetMolInfo()
{   // MolInfo is a Bioseq descriptor
    return x_SetDescriptor(eMolInfo,
        [](const CSeqdesc& desc) {
            return desc.IsMolinfo();
        },
        []() {
            auto pDesc = Ref(new CSeqdesc());
            pDesc->SetMolinfo();
            return pDesc;
        },
        m_pBioseqContainer.get()
    ).SetMolinfo();
}


CBioSource& CDescrCache::SetBioSource()
{
    return x_SetDescriptor(eBioSource,
            [](const CSeqdesc& desc) {
                return desc.IsSource();
            },
            []() {
                auto pDesc = Ref(new CSeqdesc());
                pDesc->SetSource();
                return pDesc;
            }
        ).SetSource();
}


CDescrCache::TSubtype& CDescrCache::SetSubtype()
{
    if (!m_pSubtype) {
        m_pSubtype = &(SetBioSource().SetSubtype());
        m_pSubtype->clear();
    }

    return *m_pSubtype;
}


CDescrCache::TOrgMods& CDescrCache::SetOrgMods()
{
    if (!m_pOrgMods) {
        m_pOrgMods = &(SetBioSource().SetOrg().SetOrgname().SetMod());
        m_pOrgMods->clear();
    }

    return *m_pOrgMods;
}



CPCRReactionSet& CDescrCache::SetPCR_primers()
{
    if (!m_pPCRReactionSet) {
        m_pPCRReactionSet = &(SetBioSource().SetPcr_primers());
        m_pPCRReactionSet->Set().clear();
    }
    return *m_pPCRReactionSet;
}


CSeqdesc& CDescrCache::x_SetDescriptor(const EChoice eChoice,
                                       function<bool(const CSeqdesc&)> f_verify,
                                       function<CRef<CSeqdesc>(void)> f_create)
{
    return x_SetDescriptor(eChoice, f_verify, f_create, m_pPrimaryContainer);
}


CSeqdesc& CDescrCache::x_SetDescriptor(const EChoice eChoice,
                                       function<bool(const CSeqdesc&)> f_verify,
                                       function<CRef<CSeqdesc>(void)> f_create,
                                       TDescrContainer* pDescrContainer)
{
    auto it = m_Cache.find(eChoice);
    if (it != m_Cache.end()) {
        return *(it->second);
    }


    // Search for descriptor on Bioseq
    if (pDescrContainer->IsSet()) {
        for (auto& pDesc : pDescrContainer->SetDescr().Set()) {
            if (pDesc.NotEmpty() && f_verify(*pDesc)) {
                m_Cache.insert(make_pair(eChoice, pDesc));
                return *pDesc;
            }
        }
    }

    auto pDesc = f_create();
    m_Cache.insert(make_pair(eChoice, pDesc));
    pDescrContainer->SetDescr().Set().push_back(pDesc);
    return *pDesc;
}


END_SCOPE(objects)
END_NCBI_SCOPE

