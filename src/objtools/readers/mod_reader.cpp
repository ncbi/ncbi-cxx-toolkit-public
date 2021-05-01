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
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objtools/logging/message.hpp>
#include <objtools/logging/listener.hpp>
#include <objtools/readers/mod_reader.hpp>
#include <objtools/readers/mod_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cassert>
//#include <util/compile_time.hpp>

#include "mod_to_enum.hpp"
#include "descr_mod_apply.hpp"
#include "feature_mod_apply.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//MAKE_CONST_MAP(s_ModNameMap, NStr::eCase, const char*, const char*,

static const unordered_map<string, string> s_ModNameMap =
{{"top","topology"},
 {"mol","molecule"},
 {"moltype", "mol-type"},
 {"fwd-pcr-primer-name", "fwd-primer-name"},
 {"fwd-pcr-primer-names", "fwd-primer-name"},
 {"fwd-primer-names", "fwd-primer-name"},
 {"fwd-pcr-primer-seq","fwd-primer-seq"},
 {"fwd-pcr-primer-seqs","fwd-primer-seq"},
 {"fwd-primer-seqs","fwd-primer-seq"},
 {"rev-pcr-primer-name", "rev-primer-name"},
 {"rev-pcr-primer-names", "rev-primer-name"},
 {"rev-primer-names", "rev-primer-name"},
 {"rev-pcr-primer-seq", "rev-primer-seq"},
 {"rev-pcr-primer-seqs", "rev-primer-seq"},
 {"rev-primer-seqs", "rev-primer-seq"},
 {"org", "taxname"},
 {"organism", "taxname"},
 {"div", "division"},
 {"notes", "note"},
 {"completedness", "completeness"},
 {"gene-syn", "gene-synonym"},
 {"genesyn", "gene-synonym"},
 {"genesynonym", "gene-synonym"},
 {"prot", "protein"},
 {"prot-desc", "protein-desc"},
 {"function", "activity"},
 {"secondary", "secondary-accession"},
 {"secondary-accessions", "secondary-accession"},
 {"keywords", "keyword"},
 {"primary", "primary-accession"},
 {"primary-accessions", "primary-accession"},
 {"projects", "project"},
 {"db-xref", "dbxref"},
 {"pubmed", "pmid"},
 {"ft-url-mod", "ft-mod"},
 {"ft-url", "ft-map"}
 };
//);


const CModHandler::TNameSet CModHandler::sm_DeprecatedModifiers
{
    "dosage",
    "transposon-name",
    "plastid-name",
    "insertion-seq-name",
    "old-lineage",
    "old-name",
    "gene",
    "gene-synonym",
    "allele",
    "locus-tag"
};


const CModHandler::TNameSet CModHandler::sm_MultipleValuesForbidden =
{
    "topology", // Seq-inst
    "molecule",
    "strand",
    "gene",  // Gene-ref
    "allele",
    "locus-tag",
    "protein-desc",// Protein-ref
    "mol-type", // MolInfo descriptor
    "tech",
    "completeness",
    "location", // Biosource descriptor
    "origin",
    "focus",
    "taxname", // Biosource - Org-ref
    "common",
    "lineage", // Biosource - Org-ref - OrgName
    "division",
    "gcode",
    "mgcode",
    "pgcode"
};


//MAKE_CONST_MAP(s_StrandStringToEnum, NStr::eCase, const char*, CSeq_inst::EStrand,
static const unordered_map<string, CSeq_inst::EStrand> s_StrandStringToEnum =
{{"single", CSeq_inst::eStrand_ss},
 {"double", CSeq_inst::eStrand_ds},
 {"mixed", CSeq_inst::eStrand_mixed},
 {"other", CSeq_inst::eStrand_other}
 };
//);


//MAKE_CONST_MAP(s_MolStringToEnum, NStr::eCase, const char*, CSeq_inst::EMol,
static const unordered_map<string, CSeq_inst::EMol> s_MolStringToEnum =
{{"dna", CSeq_inst::eMol_dna},
 {"rna", CSeq_inst::eMol_rna},
 {"aa", CSeq_inst::eMol_aa},
 {"na", CSeq_inst::eMol_na},
 {"other", CSeq_inst::eMol_other}
 };
 //);


//MAKE_CONST_MAP(s_TopologyStringToEnum, NStr::eCase, const char*, CSeq_inst::ETopology,
static const unordered_map<string, CSeq_inst::ETopology> s_TopologyStringToEnum =
{{"linear", CSeq_inst::eTopology_linear},
 {"circular", CSeq_inst::eTopology_circular},
 {"tandem", CSeq_inst::eTopology_tandem},
 {"other", CSeq_inst::eTopology_other}
 };
 //);

/*
MAKE_CONST_MAP(s_BiomolEnumToMolEnum, NStr::eNocase, CMolInfo::TBiomol, CSeq_inst::EMol,
{{ CMolInfo::eBiomol_genomic, CSeq_inst::eMol_dna},
 { CMolInfo::eBiomol_pre_RNA,  CSeq_inst::eMol_rna},
 { CMolInfo::eBiomol_mRNA,  CSeq_inst::eMol_rna },
 { CMolInfo::eBiomol_rRNA, CSeq_inst::eMol_rna},
 { CMolInfo::eBiomol_tRNA, CSeq_inst::eMol_rna},
 { CMolInfo::eBiomol_snRNA, CSeq_inst::eMol_rna},
 { CMolInfo::eBiomol_scRNA, CSeq_inst::eMol_rna},
 { CMolInfo::eBiomol_genomic_mRNA, CSeq_inst::eMol_rna },
 { CMolInfo::eBiomol_cRNA, CSeq_inst::eMol_rna },
 { CMolInfo::eBiomol_snoRNA, CSeq_inst::eMol_rna},
 { CMolInfo::eBiomol_transcribed_RNA, CSeq_inst::eMol_rna},
 { CMolInfo::eBiomol_ncRNA, CSeq_inst::eMol_rna},
 { CMolInfo::eBiomol_tmRNA, CSeq_inst::eMol_rna},
 { CMolInfo::eBiomol_peptide, CSeq_inst::eMol_aa},
 { CMolInfo::eBiomol_other_genetic, CSeq_inst::eMol_other},
 { CMolInfo::eBiomol_other, CSeq_inst::eMol_other}
});
*/


CModHandler::CModHandler(){}


void CModHandler::SetExcludedMods(const vector<string>& excluded_mods)
{
    m_ExcludedModifiers.clear();
    transform(excluded_mods.begin(), excluded_mods.end(),
            inserter(m_ExcludedModifiers, m_ExcludedModifiers.end()),
            [](const string& mod_name) { return GetCanonicalName(mod_name); });
}


void CModHandler::SetMods(const TMods& mods)
{
    m_Mods = mods;
}


void CModHandler::AddMods(const TModList& mods,
                          EHandleExisting handle_existing,
                          TModList& rejected_mods,
                          FReportError fPostMessage)
{
    rejected_mods.clear();

    unordered_set<string> current_set;
    TMods accepted_mods;
    TMods conflicting_mods;

    for (const auto& mod : mods) {
        const auto& canonical_name = GetCanonicalName(mod.GetName());
        const auto allow_multiple_values = x_MultipleValuesAllowed(canonical_name);
        // Don't want to check for errors if we're not going to keep the modifier
        if (handle_existing == ePreserve ||
            (handle_existing == eAppendPreserve &&
             !allow_multiple_values)) {
            if (m_Mods.find(canonical_name) != m_Mods.end()) {
                continue;
            }
        }

        if (m_ExcludedModifiers.find(canonical_name) !=
            m_ExcludedModifiers.end()) {
            string message = "The following modifier is unsupported in this context and will be ignored: " + mod.GetName() + ".";
            if (fPostMessage) {
                fPostMessage(mod, message, eDiag_Warning, eModSubcode_Excluded);
            }
            rejected_mods.push_back(mod);
            continue;
        }

        if (x_IsDeprecated(canonical_name)) {
            string message = "Use of the following modifier in a sequence file is discouraged and the information will be ignored: " + mod.GetName() + ".";
            if (fPostMessage) {
                fPostMessage(mod, message, eDiag_Warning, eModSubcode_Deprecated);
            }
            rejected_mods.push_back(mod);
            continue;
        }

        const auto first_occurrence = current_set.insert(canonical_name).second;

       // Put this in its own method
       if (!first_occurrence) {
            string msg;
            EDiagSev sev;
            EModSubcode subcode;

            auto it = accepted_mods.find(canonical_name);
            if (it != accepted_mods.end() &&
                NStr::EqualNocase(it->second.front().GetValue(),
                       mod.GetValue())) {
                msg = "Duplicated modifier value detected, ignoring duplicate, no action required: "
                    + mod.GetName() + "=" + mod.GetValue() + ".";
                sev = eDiag_Warning;
                subcode = eModSubcode_Duplicate;
            }
            else
            if (!allow_multiple_values) {
                msg = "Conflicting modifiers detected. Provide one modifier with one value for: " + mod.GetName() + ".";
                sev = eDiag_Error;
                subcode = eModSubcode_ConflictingValues;

                if (it != accepted_mods.end()) {
                    conflicting_mods[canonical_name] = it->second;
                    accepted_mods.erase(it);
                }
                conflicting_mods[canonical_name].push_back(mod);
            }
            else
            {
                accepted_mods[canonical_name].push_back(mod);
                continue;
            }

            CModData reportMod =
                (subcode == eModSubcode_Duplicate) ?
                mod :
                CModData( mod.GetName(), kEmptyStr);

            if (fPostMessage) {
                fPostMessage(reportMod, msg, sev, subcode);
                continue;
            }
            NCBI_THROW(CModReaderException, eMultipleValuesForbidden, msg);
       }

        accepted_mods[canonical_name].push_back(mod);
    }

    for (auto& conflicts : conflicting_mods) {
        rejected_mods.splice(rejected_mods.end(), conflicts.second);
    }

    x_SaveMods(move(accepted_mods), handle_existing, m_Mods);
}


void CModHandler::x_SaveMods(TMods&& mods, EHandleExisting handle_existing, TMods& dest)
{
    if (handle_existing == eReplace) {
        for (auto& mod_entry : mods) {
            const auto& canonical_name = mod_entry.first;
            dest[canonical_name] = mod_entry.second;
        }
    }
    else
    if (handle_existing == ePreserve) {
        dest.insert(make_move_iterator(mods.begin()),
                    make_move_iterator(mods.end()));
    }
    else
    if (handle_existing == eAppendReplace) {
        for (auto& mod_entry : mods) {
            const auto& canonical_name = mod_entry.first;
            auto& dest_mod_list = dest[canonical_name];
            if (x_MultipleValuesAllowed(canonical_name)){
                dest_mod_list.splice(
                        dest_mod_list.end(),
                        move(mod_entry.second));
            }
            else {
                dest_mod_list = move(mod_entry.second);
            }
        }
    }
    else
    if (handle_existing == eAppendPreserve) {
        for (auto& mod_entry : mods) {
            const auto& canonical_name = mod_entry.first;
            auto& dest_mod_list = dest[canonical_name];
            if (dest_mod_list.empty()) {
                dest_mod_list = move(mod_entry.second);
            }
            else
            if (x_MultipleValuesAllowed(canonical_name)){
                dest_mod_list.splice(
                        dest_mod_list.end(),
                        move(mod_entry.second));
            }
        }
    }
}


bool CModHandler::x_MultipleValuesAllowed(const string& canonical_name)
{
    return (sm_MultipleValuesForbidden.find(canonical_name) ==
            sm_MultipleValuesForbidden.end());
}


const CModHandler::TMods& CModHandler::GetMods(void) const
{
    return m_Mods;
}


void CModHandler::Clear(void)
{
    m_Mods.clear();
}


const string& CModHandler::GetCanonicalName(const TModEntry& mod_entry)
{
    return mod_entry.first;
}


const string& CModHandler::AssertReturnSingleValue(const TModEntry& mod_entry)
{
    assert(mod_entry.second.size() == 1);
    return mod_entry.second.front().GetValue();
}

string CModHandler::GetCanonicalName(const string& name)
{
    const auto normalized_name = x_GetNormalizedString(name);
    const auto it = s_ModNameMap.find(normalized_name);
    if (it != s_ModNameMap.end()) {
        return it->second;
    }

    return normalized_name;
}


bool CModHandler::x_IsDeprecated(const string& canonical_name)
{
    return (sm_DeprecatedModifiers.find(canonical_name) !=
            sm_DeprecatedModifiers.end());
}


static string s_GetNormalizedString(const string& unnormalized)
{
    string normalized = unnormalized;
    NStr::ToLower(normalized);
    NStr::TruncateSpacesInPlace(normalized);
    auto new_end = unique(normalized.begin(),
                          normalized.end(),
                          [](char a, char b) {
                              return ((a=='-' || a=='_' || a==' ') &&
                                      (b=='-' || b=='_' || b==' ')); });

    normalized.erase(new_end, normalized.end());
    for (char& c : normalized) {
        if (c == '_' || c == ' ') {
            c = '-';
        }
    }
    return normalized;
}

string CModHandler::x_GetNormalizedString(const string& name)
{
    return s_GetNormalizedString(name);
}


void CModAdder::Apply(const CModHandler& mod_handler,
                      CBioseq& bioseq,
                      TSkippedMods& skipped_mods,
                      FReportError fPostMessage)
{
    Apply(mod_handler, bioseq, skipped_mods, false, fPostMessage);
}


void CModAdder::Apply(const CModHandler& mod_handler,
                      CBioseq& bioseq,
                      TSkippedMods& skipped_mods,
                      bool logInfo,
                      FReportError fPostMessage)
{
    skipped_mods.clear();

    CDescrModApply descr_mod_apply(bioseq,
                                   fPostMessage,
                                   skipped_mods);

    CFeatModApply feat_mod_apply(bioseq,
                                 fPostMessage,
                                 skipped_mods);

    list<string> applied_mods;
    for (const auto& mod_entry : mod_handler.GetMods()) {
        try {
            bool applied = false;
            if (descr_mod_apply.Apply(mod_entry)) {
                const string& mod_name = x_GetModName(mod_entry);
                if (mod_name == "secondary-accession"){
                    x_SetHist(mod_entry, bioseq.SetInst());
                }
                else if (mod_name == "mol-type") {
                    // mol-type appears before molecule in the default-ordered
                    // map keys. Therefore, if both mol-type and molecule are
                    // specified, molecule will take precedence over (or, more precisly, overwrite)
                    // the information extracted from mol-type when setting Seq-inst::mol
                    x_SetMoleculeFromMolType(mod_entry, bioseq.SetInst());
                }
                applied = true;
            }
            else
            if (x_TrySeqInstMod(mod_entry, bioseq.SetInst(), skipped_mods, fPostMessage) ||
                feat_mod_apply.Apply(mod_entry)) {
                applied = true;
            }

            if (applied) {
                if (logInfo) {
                    applied_mods.push_back(x_GetModName(mod_entry));
                }
                continue;
            }

            // Report unrecognised modifier
            if (fPostMessage) {
                skipped_mods.insert(skipped_mods.end(),
                    mod_entry.second.begin(),
                    mod_entry.second.end());

                for (const auto& modData : mod_entry.second) {
                    string msg = "Unrecognized modifier: " + modData.GetName() + ".";
                    fPostMessage(modData, msg, eDiag_Warning, eModSubcode_Unrecognized);
                }
                continue;
            }
            string canonicalName = x_GetModName(mod_entry);
            string msg = "Unrecognized modifier: " + canonicalName + ".";
            NCBI_THROW(CModReaderException, eUnknownModifier, msg);
        }
        catch(const CModReaderException& e) {
            skipped_mods.insert(skipped_mods.end(),
                    mod_entry.second.begin(),
                    mod_entry.second.end());
            if (fPostMessage) {
                string canonicalName = x_GetModName(mod_entry);
                fPostMessage(CModData( canonicalName, kEmptyStr), e.GetMsg(), eDiag_Error, eModSubcode_Undefined);
            }
            else {
                throw; // rethrow e
            }
        }
    }

    if (!applied_mods.empty()) {
        string msg = "Applied mods: ";
        for (const auto& applied_mod : applied_mods) {
            msg += " " + applied_mod;
        }
        fPostMessage(CModData("",""), msg, eDiag_Info, eModSubcode_Applied);
    }
}


void CModAdder::x_ReportInvalidValue(const CModData& mod_data,
                                    TSkippedMods& skipped_mods,
                                    FReportError fPostMessage)
{
    const auto& mod_name = mod_data.GetName();
    const auto& mod_value = mod_data.GetValue();
    string msg = "Invalid value: " + mod_name + "=" + mod_value + ".";

    if (fPostMessage) {
        fPostMessage(mod_data, msg, eDiag_Error, eModSubcode_InvalidValue);
        skipped_mods.push_back(mod_data);
        return;
    }

    NCBI_THROW(CModReaderException, eInvalidValue, msg);
}



const string& CModAdder::x_GetModName(const TModEntry& mod_entry)
{
    return CModHandler::GetCanonicalName(mod_entry);
}


const string& CModAdder::x_GetModValue(const TModEntry& mod_entry)
{
    return CModHandler::AssertReturnSingleValue(mod_entry);
}


bool CModAdder::x_TrySeqInstMod(
        const TModEntry& mod_entry,
        CSeq_inst& seq_inst,
        TSkippedMods& skipped_mods,
        FReportError fPostMessage)
{
    const auto& mod_name = x_GetModName(mod_entry);

    if (mod_name == "strand") {
        x_SetStrand(mod_entry, seq_inst, skipped_mods, fPostMessage);
        return true;
    }

    if (mod_name == "molecule") {
        x_SetMolecule(mod_entry, seq_inst, skipped_mods, fPostMessage);
        return true;
    }

    if (mod_name == "topology") {
        x_SetTopology(mod_entry, seq_inst, skipped_mods, fPostMessage);
        return true;
    }

//   Note that we do not check for the 'secondary-accession' modifier here.
//   secondary-accession also modifies the GB_block descriptor
//   The check for secondary-accession and any resulting call
//   to x_SetHist is performed before x_TrySeqInstMod
//   is invoked.

    return false;
}



void CModAdder::x_SetStrand(const TModEntry& mod_entry,
                            CSeq_inst& seq_inst,
                            TSkippedMods& skipped_mods,
                            FReportError fPostMessage)
{
    string value = x_GetModValue(mod_entry);
    const auto it = s_StrandStringToEnum.find(g_GetNormalizedModVal(value));
    if (it == s_StrandStringToEnum.end()) {
        x_ReportInvalidValue(mod_entry.second.front(), skipped_mods, fPostMessage);
        return;
    }
    seq_inst.SetStrand(it->second);
}


void CModAdder::x_SetMolecule(const TModEntry& mod_entry,
                              CSeq_inst& seq_inst,
                              TSkippedMods& skipped_mods,
                              FReportError fPostMessage)
{
    string value = x_GetModValue(mod_entry);
    const auto it = s_MolStringToEnum.find(g_GetNormalizedModVal(value));
    if (it == s_MolStringToEnum.end()) {
        x_ReportInvalidValue(mod_entry.second.front(), skipped_mods, fPostMessage);
        return;
    }
    seq_inst.SetMol(it->second);
}


void CModAdder::x_SetMoleculeFromMolType(const TModEntry& mod_entry, CSeq_inst& seq_inst)
{
    string value = x_GetModValue(mod_entry);
    auto it = g_BiomolStringToEnum.find(g_GetNormalizedModVal(value));
    if (it == g_BiomolStringToEnum.end()) {
        // No need to report an error here.
        // The error is reported in x_SetMolInfoType
        return;
    }
    CSeq_inst::EMol mol = g_BiomolEnumToMolEnum.at(it->second);
    seq_inst.SetMol(mol);
}


void CModAdder::x_SetTopology(const TModEntry& mod_entry,
                              CSeq_inst& seq_inst,
                              TSkippedMods& skipped_mods,
                              FReportError fPostMessage)
{
    string value = x_GetModValue(mod_entry);
    const auto it = s_TopologyStringToEnum.find(g_GetNormalizedModVal(value));
    if (it == s_TopologyStringToEnum.end()) {
        x_ReportInvalidValue(mod_entry.second.front(), skipped_mods, fPostMessage);
        return;
    }
    seq_inst.SetTopology(it->second);
}


void CModAdder::x_SetHist(const TModEntry& mod_entry, CSeq_inst& seq_inst)
{
    list<string> id_list;
    for (const auto& mod : mod_entry.second) {
        const auto& vals = mod.GetValue();
        list<CTempString> value_sublist;
        NStr::Split(vals, ",; \t", value_sublist, NStr::fSplit_Tokenize);
        for (const auto& val : value_sublist) {
            string value = NStr::TruncateSpaces_Unsafe(val);
            try {
                SSeqIdRange idrange(value);
                id_list.insert(id_list.end(), idrange.begin(), idrange.end());
            }
            catch (...)
            {
                id_list.push_back(value);
            }
        }
    }

    if (id_list.empty()) {
        return;
    }

    list<CRef<CSeq_id>> secondary_ids;
    // try catch statement
    transform(id_list.begin(), id_list.end(), back_inserter(secondary_ids),
            [](const string& id_string) { return Ref(new CSeq_id(id_string)); });

    seq_inst.SetHist().SetReplaces().SetIds() = move(secondary_ids);
}


CDefaultModErrorReporter::CDefaultModErrorReporter(
        const string& seqId,
        int lineNum,
        IObjtoolsListener* pMessageListener)
    : m_SeqId(seqId),
      m_LineNum(lineNum),
      m_pMessageListener(pMessageListener)
    {}


void CDefaultModErrorReporter::operator()(
    const CModData& mod,
    const string& msg,
    EDiagSev sev,
    EModSubcode subcode)
{
    if (!m_pMessageListener) {
        if (sev == eDiag_Info) {
            return;
        }
        if (sev == eDiag_Warning) {
            ERR_POST(Warning << msg);
            return;
        }
        NCBI_THROW2(CObjReaderParseException, eFormat, msg, 0);
    }


    if (!m_pMessageListener->SevEnabled(sev)) {
        return;
    }

    AutoPtr<CLineErrorEx> pErr(
        CLineErrorEx::Create(
            ILineError::eProblem_GeneralParsingError,
            sev,
            EReaderCode::eReader_Mods,
            subcode,
            m_SeqId,
            m_LineNum,
            msg,
            "",
            mod.GetName(),
            mod.GetValue()));

    if (!m_pMessageListener->PutMessage(*pErr)) {
        NCBI_THROW2(CObjReaderParseException, eFormat, msg, 0);
    }
}


void CTitleParser::Apply(const CTempString& title, TModList& mods, string& remainder)
{
    mods.clear();
    remainder.clear();
    size_t start_pos = 0;
    while(start_pos < title.size()) {
        size_t lb_pos, end_pos, eq_pos;
        lb_pos = start_pos;
        if (x_FindBrackets(title, lb_pos, end_pos, eq_pos)) {
            if (eq_pos < end_pos) {
                if ((lb_pos > start_pos) ) {
                    auto left_remainder = NStr::TruncateSpaces_Unsafe(title.substr(start_pos, lb_pos-start_pos));
                    if (!left_remainder.empty()) {
                        if (!remainder.empty()) {
                            remainder.append(" ");
                        }
                        remainder.append(left_remainder);
                    }
                }
                auto name = NStr::TruncateSpaces_Unsafe(title.substr(lb_pos+1, eq_pos-(lb_pos+1)));
                auto value = NStr::TruncateSpaces_Unsafe(title.substr(eq_pos+1, end_pos-(eq_pos+1)));
                mods.emplace_back(name, value);
            }
            start_pos = end_pos+1;
        }
        else {
            auto right_remainder = NStr::TruncateSpaces_Unsafe(title.substr(start_pos));
            if (!right_remainder.empty()) {
                if (!remainder.empty()) {
                    remainder.append(" ");
                }
                remainder.append(right_remainder);
            }
            return;
        }
    }
}


bool CTitleParser::HasMods(const CTempString& title)
{
    size_t start_pos = 0;
    while (start_pos < title.size()) {
        size_t lb_pos, end_pos, eq_pos;
        lb_pos = start_pos;
        if (x_FindBrackets(title, lb_pos, end_pos, eq_pos)) {
            if (eq_pos < end_pos) {
                return true;
            }
            start_pos = end_pos+1;
        }
        else {
            return false;
        }
    }
    return false;
}


bool CTitleParser::x_FindBrackets(const CTempString& line, size_t& start, size_t& stop, size_t& eq_pos)
{ // Copied from CSourceModParser
    size_t i = start;

    eq_pos = CTempString::npos;
    const char* s = line.data() + start;

    int num_unmatched_left_brackets = 0;
    while (i < line.size())
    {
        switch (*s)
        {
        case '[':
            num_unmatched_left_brackets++;
            if (num_unmatched_left_brackets == 1)
            {
                start = i;
            }
            break;
        case '=':
            if (num_unmatched_left_brackets > 0 && eq_pos == CTempString::npos) {
                eq_pos = i;
            }
            break;
        case ']':
            if (num_unmatched_left_brackets == 1)
            {
                stop = i;
                return (eq_pos<stop);
            }
            else
            if (num_unmatched_left_brackets == 0) {
                return false;
            }
            else
            {
                num_unmatched_left_brackets--;
            }
        }
        i++; s++;
    }
    return false;
};


END_SCOPE(objects)
END_NCBI_SCOPE



