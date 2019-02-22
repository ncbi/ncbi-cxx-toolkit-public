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
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cassert>

#include "mod_info.hpp"
#include "descr_mod_apply.hpp"
#include "feature_mod_apply.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CModData::CModData(const string& name) : mName(name) {}


CModData::CModData(const string& name, const string& value) : mName(name), mValue(value) {}


void CModData::SetName(const string& name) 
{
    mValue = name;
}


void CModData::SetValue(const string& value) 
{
    mValue = value;
}


void CModData::SetAttrib(const string& attrib)
{
    mAttrib = attrib;
}


bool CModData::IsSetAttrib(void) const
{
    return !(mAttrib.empty());
}


const string& CModData::GetName(void) const
{
    return mName;
}


const string& CModData::GetValue(void) const
{
    return mValue;
}


const string& CModData::GetAttrib(void) const
{
    return mAttrib;
}


const CModHandler::TNameMap CModHandler::sm_NameMap = 
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
 {"pubmed", "pmid"}
};


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


CModHandler::CModHandler(FReportError fReportError) 
    : m_fReportError(fReportError) {}



// Replace CModValueAndAttrib class with a CModData
// or CModifier class, which also has a name attribute. 
// Then, AddMods will take a list<CModInfo> 
void CModHandler::AddMods(const TModList& mods,
                          EHandleExisting handle_existing,
                          TModList& rejected_mods)
{
    rejected_mods.clear();

    unordered_set<string> current_set;
    TMods accepted_mods;

    for (const auto& mod : mods) {
        const auto& canonical_name = x_GetCanonicalName(mod.GetName());
        if (x_IsDeprecated(canonical_name)) {
            rejected_mods.push_back(mod);
            string message = mod.GetName() + " is deprecated - ignoring";
            if (m_fReportError) {
                m_fReportError(message, eDiag_Warning, eModSubcode_Deprecated);
            }
            continue;
        }

        const auto allow_multiple_values = x_MultipleValuesAllowed(canonical_name);
        const auto first_occurrence = current_set.insert(canonical_name).second;

        if (!allow_multiple_values &&
            !first_occurrence) {
            rejected_mods.push_back(mod);

            string msg;
            EDiagSev sev;
            EModSubcode subcode;
            if (NStr::EqualNocase(accepted_mods[canonical_name].front().GetValue(),
                                  mod.GetValue())) {
                msg = mod.GetName() + "=" 
                    + mod.GetValue() + " duplicates previously encountered modifier.";
                sev = eDiag_Warning;
                subcode = eModSubcode_Duplicate;
            }
            else {
                msg = mod.GetName() + "=" 
                    + mod.GetValue() + " conflicts with previously encountered modifier.";
                sev = eDiag_Error;
                subcode = eModSubcode_ConflictingValues;
            }

            if (m_fReportError) {
                m_fReportError(msg, sev, subcode);
                continue;
            }   
            NCBI_THROW(CModReaderException, eMultipleValuesForbidden, msg);
        }
        accepted_mods[canonical_name].push_back(mod);
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

string CModHandler::x_GetCanonicalName(const string& name) const
{
    const auto& normalized_name = x_GetNormalizedString(name);
    const auto& it = sm_NameMap.find(normalized_name);
    if (it != sm_NameMap.end()) {
        return it->second;
    }

    return normalized_name;
}


bool CModHandler::x_IsDeprecated(const string& canonical_name)
{
    return (sm_DeprecatedModifiers.find(canonical_name) !=
            sm_DeprecatedModifiers.end());
}


string CModHandler::x_GetNormalizedString(const string& name) const
{
    string normalized_name = name;
    NStr::ToLower(normalized_name);
    NStr::TruncateSpacesInPlace(normalized_name);
    NStr::ReplaceInPlace(normalized_name, "_", "-");
    NStr::ReplaceInPlace(normalized_name, " ", "-");
    auto new_end = unique(normalized_name.begin(), 
                          normalized_name.end(),
                          [](char a, char b) { return ((a==b) && (a==' ')); });

    normalized_name.erase(new_end, normalized_name.end());
    NStr::ReplaceInPlace(normalized_name, " ", "-");

    return normalized_name;
}


void CModAdder::Apply(const CModHandler& mod_handler,
                      CBioseq& bioseq,
                      TSkippedMods& skipped_mods,
                      FReportError fReportError)
{
    skipped_mods.clear();

    CDescrModApply descr_mod_apply(bioseq,
                                   fReportError,
                                   skipped_mods);
                
    CFeatModApply feat_mod_apply(bioseq, 
                                 fReportError,
                                 skipped_mods);

    for (const auto& mod_entry : mod_handler.GetMods()) {
        try {
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
                continue;
            }

            if (x_TrySeqInstMod(mod_entry, bioseq.SetInst())) {
                continue;
            }

            if (feat_mod_apply.Apply(mod_entry)) {
                continue;
            }

            // Report unrecognised modifier
            string msg = "Unrecognized modifier: " + x_GetModName(mod_entry) + ".";
            if (fReportError) {
                skipped_mods.insert(skipped_mods.end(),
                    mod_entry.second.begin(),
                    mod_entry.second.end());
                fReportError(msg, eDiag_Warning, eModSubcode_Unrecognized);
                continue;
            }
            NCBI_THROW(CModReaderException, eUnknownModifier, msg);
        }
        catch(const CModReaderException& e) {
            skipped_mods.insert(skipped_mods.end(),
                    mod_entry.second.begin(),
                    mod_entry.second.end());
            if (fReportError) {
                fReportError(e.GetMsg(), eDiag_Error, eModSubcode_Undefined);
            } 
            else { 
                throw; // rethrow e
            }
        }
    }
}




void CModAdder::x_ThrowInvalidValue(const CModData& mod_data,
                                    const string& add_msg)
{
    const auto& mod_name = mod_data.GetName();
    const auto& mod_value = mod_data.GetValue();
    string msg = mod_name + " modifier has invalid value: \"" +   mod_value + "\".";
    if (!NStr::IsBlank(add_msg)) {
        msg += " " + add_msg;
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


bool CModAdder::x_TrySeqInstMod(const TModEntry& mod_entry, CSeq_inst& seq_inst)
{
    const auto& mod_name = x_GetModName(mod_entry);

    if (mod_name == "strand") {
        x_SetStrand(mod_entry, seq_inst);
        return true;
    }

    if (mod_name == "molecule") {
        x_SetMolecule(mod_entry, seq_inst);
        return true;
    }

    if (mod_name == "topology") {
        x_SetTopology(mod_entry, seq_inst);
        return true;
    }

//   Note that we do not check for the 'secondary-accession' modifier here.
//   secondary-accession also modifies the GB_block descriptor
//   The check for secondary-accession and any resulting call 
//   to x_SetHist is performed before x_TrySeqInstMod 
//   is invoked. 

    return false;
}


void CModAdder::x_SetStrand(const TModEntry& mod_entry, CSeq_inst& seq_inst)
{
    const auto& value = x_GetModValue(mod_entry);
    const auto it = s_StrandStringToEnum.find(value);
    if (it == s_StrandStringToEnum.end()) {
        x_ThrowInvalidValue(mod_entry.second.front());
    }
    seq_inst.SetStrand(it->second);
}


void CModAdder::x_SetMolecule(const TModEntry& mod_entry, CSeq_inst& seq_inst)
{
    const auto& value = x_GetModValue(mod_entry);
    const auto it = s_MolStringToEnum.find(value);
    if (it == s_MolStringToEnum.end()) {
        x_ThrowInvalidValue(mod_entry.second.front());
    }
    seq_inst.SetMol(it->second);
}


void CModAdder::x_SetMoleculeFromMolType(const TModEntry& mod_entry, CSeq_inst& seq_inst)
{
    const auto& value = x_GetModValue(mod_entry);
    auto it = s_BiomolStringToEnum.find(value);
    if (it == s_BiomolStringToEnum.end()) {
        // No need to report an error here.
        // The error is reported in x_SetMolInfoType
        return; 
    }
    auto mol =  s_BiomolEnumToMolEnum.at(it->second);
    seq_inst.SetMol(mol);
}


void CModAdder::x_SetTopology(const TModEntry& mod_entry, CSeq_inst& seq_inst)
{
    const auto& value = x_GetModValue(mod_entry);
    const auto it = s_TopologyStringToEnum.find(value);
    if (it == s_TopologyStringToEnum.end()) {
        x_ThrowInvalidValue(mod_entry.second.front());
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
    bool found = false;

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



