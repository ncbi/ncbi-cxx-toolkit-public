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
* Authors:  Aaron Ucko, Jonathan Kans, Vasuki Gobi, Michael Kornbluh
*
* File Description:
*   Parser for source modifiers, as found in (Sequin-targeted) FASTA files.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <sstream>

#include <objtools/readers/source_mod_parser.hpp>
#include <objtools/readers/message_listener.hpp>

#include <corelib/ncbiutil.hpp>
#include <util/static_map.hpp>
#include <serial/enumvalues.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
// #include <objects/submit/Submit_block.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

namespace
{
    class equal_subtype
    {
    public:
        equal_subtype(CSubSource::TSubtype st) : m_st(st){};
        bool operator()(const CRef<CSubSource>& st) const
        {
            return st->IsSetSubtype() && (st->GetSubtype() == m_st);
        }
    private:
        CSubSource::TSubtype m_st;
    };

#ifdef STATIC_SMOD
#  error "STATIC_SMOD already defined"
#endif

    // The macro makes sure that the var's name matches its key.
    // Due to kKeyCanonicalizationTable, it's okay to use '_' for '-'
    // because it will match both.


#define STATIC_SMOD(key_str) \
    const char   s_Mod_s_##key_str[] = #key_str; \
    const size_t s_Mod_n_##key_str = sizeof(#key_str)-1; \
    const CTempString s_Mod_##key_str(s_Mod_s_##key_str, s_Mod_n_##key_str)


    // For CBioseq
    STATIC_SMOD(topology);
    STATIC_SMOD(top);
    STATIC_SMOD(molecule);
    STATIC_SMOD(mol);
    STATIC_SMOD(moltype);
    STATIC_SMOD(mol_type);
    STATIC_SMOD(strand);
    STATIC_SMOD(comment);

    // For CBioSource
    STATIC_SMOD(organism);
    STATIC_SMOD(org);
    STATIC_SMOD(taxname);
    STATIC_SMOD(taxid);
    STATIC_SMOD(location);
    STATIC_SMOD(origin);
    STATIC_SMOD(sub_clone);
    STATIC_SMOD(lat_long);
    STATIC_SMOD(latitude_longitude);
    STATIC_SMOD(fwd_primer_seq);
    STATIC_SMOD(fwd_pcr_primer_seq);
    STATIC_SMOD(rev_primer_seq);
    STATIC_SMOD(rev_pcr_primer_seq);
    STATIC_SMOD(fwd_primer_name);
    STATIC_SMOD(fwd_pcr_primer_name);
    STATIC_SMOD(rev_primer_name);
    STATIC_SMOD(rev_pcr_primer_name);
    STATIC_SMOD(dbxref);
    STATIC_SMOD(db_xref);
    STATIC_SMOD(division);
    STATIC_SMOD(div);
    STATIC_SMOD(lineage);
    STATIC_SMOD(gcode);
    STATIC_SMOD(mgcode);
    STATIC_SMOD(pgcode);
    STATIC_SMOD(note);
    STATIC_SMOD(notes);
    STATIC_SMOD(focus);

    // For CMolInfo
    STATIC_SMOD(tech);
    STATIC_SMOD(completeness);
    STATIC_SMOD(completedness);

    // For CGene_ref
    STATIC_SMOD(gene);
    STATIC_SMOD(allele);
    STATIC_SMOD(gene_syn);
    STATIC_SMOD(gene_synonym);
    STATIC_SMOD(locus_tag);

    // For CProt_ref
    STATIC_SMOD(protein);
    STATIC_SMOD(prot);
    STATIC_SMOD(prot_desc);
    STATIC_SMOD(protein_desc);
    STATIC_SMOD(EC_number);
    STATIC_SMOD(activity);
    STATIC_SMOD(function);

    // For CGB_block
    STATIC_SMOD(secondary_accession);
    STATIC_SMOD(secondary_accessions);
    STATIC_SMOD(keyword);
    STATIC_SMOD(keywords);

    // For TPA Mods (CUser_object)
    STATIC_SMOD(primary);
    STATIC_SMOD(primary_accessions);
    // For SRA (Sequence Read Archive) CUser_object
    STATIC_SMOD(SRA);

    // For Genome Project DB Mods (CUser_object)
    STATIC_SMOD(project);
    STATIC_SMOD(projects);

    // For Pub Mods (CSeq_descr)
    STATIC_SMOD(PubMed);
    STATIC_SMOD(PMID);


#undef STATIC_SMOD

    typedef set<const char*, CSourceModParser::PKeyCompare> TSModNameSet;

    // Loads up a map of SMod to subtype
    template<typename TEnum,
             typename TSModEnumMap = map<CSourceModParser::SMod, TEnum>,
             typename TEnumNameToValMap = map<string, TEnum>>
    TSModEnumMap * s_InitSmodToEnumMap(
        const CEnumeratedTypeValues* etv,
        // names to skip
        const TSModNameSet & skip_enum_names,
        // extra values to add that aren't in the enum
        const TEnumNameToValMap & extra_enum_names_to_vals )
    {
        unique_ptr<TSModEnumMap> smod_enum_map(new TSModEnumMap);

        ITERATE (CEnumeratedTypeValues::TValues, it, etv->GetValues()) {
            const string & enum_name = it->first;
            const TEnum enum_val = static_cast<TEnum>(it->second);
            if( skip_enum_names.find(enum_name.c_str()) !=
                skip_enum_names.end() )
            {
                // skip this tag
                continue;
            }
            auto emplace_result =
                smod_enum_map->emplace(
                    CSourceModParser::SMod(enum_name), enum_val);
            // emplace must succeed
            if( ! emplace_result.second) {
                NCBI_USER_THROW_FMT(
                   "s_InitSmodToEnumMap " << enum_name);
            }
        }

        for(auto extra_smod_to_enum : extra_enum_names_to_vals) {
            auto emplace_result =
                smod_enum_map->emplace(
                    CSourceModParser::SMod(extra_smod_to_enum.first),
                    extra_smod_to_enum.second);
            // emplace must succeed
            if( ! emplace_result.second) {
                NCBI_USER_THROW_FMT(
                   "s_InitSmodToEnumMap " << extra_smod_to_enum.first);
            }
        }

        return smod_enum_map.release();
    }

    typedef map<CSourceModParser::SMod, COrgMod::ESubtype> TSModOrgSubtypeMap;

    TSModOrgSubtypeMap * s_InitSModOrgSubtypeMap(void)
    {
        const TSModNameSet kDeprecatedOrgSubtypes{
            "dosage", "old-lineage", "old-name"};
        const map<const char*, COrgMod::ESubtype> extra_smod_to_enum_names {
            { "subspecies",    COrgMod::eSubtype_sub_species },
            { "host",          COrgMod::eSubtype_nat_host    },
            { "specific-host", COrgMod::eSubtype_nat_host    },
        };

        return s_InitSmodToEnumMap<COrgMod::ESubtype>(
            COrgMod::GetTypeInfo_enum_ESubtype(),
            kDeprecatedOrgSubtypes,
            extra_smod_to_enum_names
        );
    }

    // The subtype SMods are loaded from the names of the enum
    // and they map to ESubtype enum values so we can't just use STATIC_SMOD
    CSafeStatic<TSModOrgSubtypeMap> kSModOrgSubtypeMap(s_InitSModOrgSubtypeMap,
                                                 nullptr);

    typedef map<CSourceModParser::SMod,
                CSubSource::ESubtype> TSModSubSrcSubtype;

    TSModSubSrcSubtype * s_InitSModSubSrcSubtypeMap(void)
    {
        // some are skipped because they're handled specially and some are
        // skipped because they're deprecated
        TSModNameSet skip_enum_names {
            // skip because handled specially elsewhere
            "fwd_primer_seq", "rev_primer_seq",
            "fwd_primer_name", "rev_primer_name",
            "fwd_PCR_primer_seq", "rev_PCR_primer_seq",
            "fwd_PCR_primer_name", "rev_PCR_primer_name",
            // skip because deprecated
            "transposon_name",
            "plastid_name",
            "insertion_seq_name",
        };
        const map<string, CSubSource::ESubtype> extra_smod_to_enum_names {
            { "sub-clone",          CSubSource::eSubtype_subclone },
            { "lat-long",           CSubSource::eSubtype_lat_lon  },
            { "latitude-longitude", CSubSource::eSubtype_lat_lon  },
        };
        return s_InitSmodToEnumMap<CSubSource::ESubtype>(
            CSubSource::GetTypeInfo_enum_ESubtype(),
            skip_enum_names,
            extra_smod_to_enum_names );
    }
                                                 
    CSafeStatic<TSModSubSrcSubtype> kSModSubSrcSubtypeMap(
        s_InitSModSubSrcSubtypeMap, nullptr);

    bool x_FindBrackets(const CTempString& str, size_t& start, size_t& stop, size_t& eq_pos)
    {
        size_t i = start;
        bool found = false;

        eq_pos = CTempString::npos;

        const char* s = str.data() + start;

        int nested_brackets = -1;
        while (i < str.size())
        {
            switch (*s)
            {
            case '[':
                nested_brackets++;
                if (nested_brackets == 0)
                {
                    start = i;
                }
                break;
            case '=':
                if (nested_brackets >= 0)
                if (eq_pos == CTempString::npos)
                    eq_pos = i;
                break;
            case ']':
                if (nested_brackets == 0)
                {
                    stop = i;
                    if (eq_pos == CTempString::npos)
                        eq_pos = i;
                    return true;
                }
                else
                if (nested_brackets < 0)
                    return false;
                else
                {
                    nested_brackets--;
                }
            }
            i++; s++;
        }
        return false;
    };

    void x_AppendIfNonEmpty(string& s, const CTempString& o)
    {
        if (!o.empty())
        {
            if (!s.empty())
                s.push_back(' ');
            s.append(o.data(), o.length());
        }
    }

};


static bool s_MolNotSet(const CBioseq& bioseq) {
    return (!bioseq.IsSetInst() || 
            !bioseq.GetInst().IsSetMol());
}

CSafeStatic<CSourceModParser::SMod> CSourceModParser::kEmptyMod;

// ASCII letters to lowercase, space and underscore to hyphen.
const unsigned char CSourceModParser::kKeyCanonicalizationTable[257] =
    "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
    "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
    "-!\"#$%&'()*+,-./0123456789:;<=>?"
    "@abcdefghijklmnopqrstuvwxyz[\\]^-"
    "`abcdefghijklmnopqrstuvwxyz{|}~\x7F"
    "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F"
    "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F"
    "\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF"
    "\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF"
    "\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF"
    "\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF"
    "\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF"
    "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF";


class CAutoAddDBLink
{
public:
    CAutoAddDBLink(CBioseq& seq, const CTempString& id)
      :m_bioseq(seq), m_id(id)
    {
    }
    bool IsInitialised() const
    {
        return !m_dblink.Empty();
    }

    CUser_field& Get()
    {
        if (m_dblink)
            return *m_dblink;

        for (auto& d : m_bioseq.SetDescr().Set())
        {
            if (d->IsUser() && d->GetUser().IsDBLink())
            {
                for (auto& u : d->SetUser().SetData())
                {
                    if (u->IsSetLabel() && u->GetLabel().IsStr() &&
                        NStr::EqualCase(u->GetLabel().GetStr(), m_id))
                    {
                        m_dblink = u;
                        return *m_dblink;
                    }
                }
            }
        }
        if (m_dblink.Empty())
        {
            m_user_obj.Reset(new CSeqdesc);
            m_user_obj->SetUser().SetType().SetStr() = "DBLink";
            m_dblink.Reset(new CUser_field);
            m_dblink->SetLabel().SetStr() = m_id;
            m_user_obj->SetUser().SetData().push_back(m_dblink);
            m_bioseq.SetDescr().Set().push_back(m_user_obj);
        }

        return *m_dblink;
    }
protected:
    CBioseq& m_bioseq;
    CTempString m_id;
    CRef<CUser_field> m_dblink;
    CRef<CSeqdesc> m_user_obj;
};


string CSourceModParser::ParseTitle(const CTempString& title, 
    CConstRef<CSeq_id> seqid,
    size_t iMaxModsToParse )
{
    SMod   mod;
    string stripped_title;
    size_t pos = 0;

    m_Mods.clear();
    m_ModNameMap.clear();

    mod.seqid = seqid;

    size_t iModsFoundSoFar = 0;    
    for (; (pos < title.size()) && (iModsFoundSoFar < iMaxModsToParse);
        ++iModsFoundSoFar )
    {
        size_t lb_pos, end_pos, eq_pos;
        lb_pos = pos;
        if (x_FindBrackets(title, lb_pos, end_pos, eq_pos))
        {            
            CTempString skipped = NStr::TruncateSpaces_Unsafe(title.substr(pos, lb_pos - pos));

            if (eq_pos < end_pos) {
                CTempString key = NStr::TruncateSpaces_Unsafe(title.substr(lb_pos+1, eq_pos - lb_pos - 1));
                CTempString value = NStr::TruncateSpaces_Unsafe(title.substr(eq_pos + 1, end_pos - eq_pos - 1));

                mod.key = key;
                mod.value = value;
                mod.pos = lb_pos;
                m_Mods.emplace(mod);

                m_ModNameMap.emplace(key, mod);
            }

            x_AppendIfNonEmpty(stripped_title, skipped);

            pos = end_pos + 1;
        }
        else
        { // rest of the title is unparsed
            CTempString rest = NStr::TruncateSpaces_Unsafe(title.substr(pos));
            x_AppendIfNonEmpty(stripped_title, rest);
            break;
        }
    }

    return stripped_title;
}


void CSourceModParser::x_AddFeatures(const CSeq_loc& location, CBioseq& bioseq) 
{
    CAutoInitRef<CSeq_annot> ftable;
    bool new_ftable = true;

    if (bioseq.IsSetAnnot()) {
        for (auto pAnnot : bioseq.SetAnnot()) {
            if (pAnnot->GetData().IsFtable()) {
                ftable.Set(&*pAnnot);
                new_ftable = false;
                break;
            }
        }
    }

    // CGene_ref is only on nucleotide sequences
    if (s_MolNotSet(bioseq) ||
        bioseq.IsNa()) {
        CAutoInitRef<CGene_ref> gene;
        x_ApplyMods(gene);
        if (gene.IsInitialized()) {
            CRef<CSeq_feat> feat(new CSeq_feat());
            feat->SetData().SetGene(*gene);
            feat->SetLocation().Assign(location);
            ftable->SetData().SetFtable().push_back(feat);
        }
    }

    // Only add Prot_ref if amino acid (or at least not nucleic acid)
    if (s_MolNotSet(bioseq) ||
        bioseq.IsAa()) {
        CAutoInitRef<CProt_ref> prot;
        x_ApplyMods(prot);
        if (prot.IsInitialized()) {
            CRef<CSeq_feat> feat(new CSeq_feat());
            feat->SetData().SetProt(*prot);
            feat->SetLocation().Assign(location);
            ftable->SetData().SetFtable().push_back(feat);
        }
    }

    if (ftable.IsInitialized() && new_ftable) {
        bioseq.SetAnnot().push_back(CRef<CSeq_annot>(&*ftable));
    }
}

static void s_AddDesc(CUser_object& user_obj, CSeq_descr& descr) 
{
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetUser(user_obj);
    descr.Set().push_back(desc);
}

void CSourceModParser::x_AddDescriptors(CBioseq& seq)
{
    {{
        const SMod* mod = nullptr;
        if ((mod = FindMod(s_Mod_comment)) != NULL) {
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetComment( mod->value );
            seq.SetDescr().Set().push_back(desc);
        }
    }}

    {{
        CAutoInitRef<CMolInfo> mol_info;
        x_ApplyMods(mol_info);
        if (mol_info.IsInitialized()) {
            CRef<CSeqdesc> desc(new CSeqdesc());
            desc->SetMolinfo(*mol_info);
            seq.SetDescr().Set().push_back(desc);
        }
    }}

    {{
        CAutoInitRef<CGB_block> gb_block;
        x_ApplyMods(gb_block);
        if (gb_block.IsInitialized()) {
            CRef<CSeqdesc> desc(new CSeqdesc());
            desc->SetGenbank(*gb_block);
            seq.SetDescr().Set().push_back(desc);
        }
    }}

    {{
        CAutoInitRef<CUser_object> tpa;
        x_ApplyTPAMods(tpa);
        if (tpa.IsInitialized()) {
            s_AddDesc(*tpa, seq.SetDescr());
        }
    }}

    {{
        CAutoAddDBLink sra(seq, "Sequence Read Archive");
        x_ApplySRAMods(sra);
    }}

    {{
        CAutoInitRef<CUser_object> gpdb;
        x_ApplyGenomeProjectsDBMods(gpdb);
        if (gpdb.IsInitialized()) {
            s_AddDesc(*gpdb, seq.SetDescr());
        }
    }}

    {{
        ApplyPubMods(seq);
    }}

}

void CSourceModParser::ApplyAllMods(CBioseq& seq, CTempString organism, CConstRef<CSeq_loc> location)
{
    x_ApplySeqInstMods(seq.SetInst());

    {{
        if (location.Empty()) {
            auto pBestId = FindBestChoice(seq.GetId(), CSeq_id::BestRank);
            if (pBestId) {
                CRef<CSeq_loc> loc(new CSeq_loc());
                loc->SetWhole(*pBestId);
                location = loc;
            }
        }

        if (location) 
        {
            x_AddFeatures(*location, seq);
        }
    }}

    {{
         CAutoInitRef<CBioSource> biosource;
         x_ApplyMods(biosource, organism);
         if (biosource.IsInitialized()) {
            // The following is very bad practice!
            CSeq_descr& descr = (seq.GetParentSet() && 
                                 seq.GetParentSet()->IsSetClass() &&
                                 seq.GetParentSet()->GetClass() == CBioseq_set::eClass_nuc_prot) ?
                                (const_cast<CBioseq_set*>(seq.GetParentSet().GetPointer()))->SetDescr() :
                                seq.SetDescr();
            CRef<CSeqdesc> desc(new CSeqdesc());
            desc->SetSource(*biosource);
            descr.Set().push_back(desc);
         }
    }}
    
    // Other descriptors
    x_AddDescriptors(seq);
/*
    {{
        const SMod* mod = nullptr;
        if ((mod = FindMod(s_Mod_comment)) != NULL) {
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetComment( mod->value );
            seq.SetDescr().Set().push_back(desc);
        }
    }}

    {{
        CAutoInitRef<CMolInfo> mol_info;
        x_ApplyMods(mol_info);
        if (mol_info.IsInitialized()) {
            CRef<CSeqdesc> desc(new CSeqdesc());
            desc->SetMolinfo(*mol_info);
            seq.SetDescr().Set().push_back(desc);
        }
    }}

    {{
        CAutoInitRef<CGB_block> gb_block;
        x_ApplyMods(gb_block);
        if (gb_block.IsInitialized()) {
            CRef<CSeqdesc> desc(new CSeqdesc());
            desc->SetGenbank(*gb_block);
            seq.SetDescr().Set().push_back(desc);
        }
    }}

    {{
        CAutoInitRef<CUser_object> tpa;
        x_ApplyTPAMods(tpa);
        if (tpa.IsInitialized()) {
            s_AddDesc(*tpa, seq.SetDescr());
        }
    }}

    {{
        CAutoAddDBLink sra(seq, "Sequence Read Archive");
        x_ApplySRAMods(sra);
    }}

    {{
        CAutoInitRef<CUser_object> gpdb;
        x_ApplyGenomeProjectsDBMods(gpdb);
        if (gpdb.IsInitialized()) {
            s_AddDesc(*gpdb, seq.SetDescr());
        }
    }}

    {{
        ApplyPubMods(seq);
    }}
*/

    TMods unusedMods = GetMods(fUnusedMods);
    for (TMods::const_iterator unused = unusedMods.begin(); unused != unusedMods.end(); ++unused) {
        x_HandleUnkModValue(*unused); 
    }
};

struct SMolTypeInfo {

    // is it shown to the user as a possibility or just silently accepted?
    enum EShown {
        eShown_Yes, // Yes, show to user in error messages, etc.
        eShown_No   // No, don't show the user, but silently accept it if the user gives it to us
    };

    SMolTypeInfo(
        EShown eShown, 
        CMolInfo::TBiomol eBiomol,
        CSeq_inst::EMol eMol ) :
        m_eBiomol(eBiomol), m_eMol(eMol), m_eShown(eShown)
    { }

    CMolInfo::TBiomol m_eBiomol;
    CSeq_inst::EMol   m_eMol;
    EShown m_eShown; 
};
typedef SStaticPair<const char*, SMolTypeInfo> TBiomolMapEntry;
static const TBiomolMapEntry sc_BiomolArray[] = {
    // careful with the sort: remember that the key is canonicalized first
    {"cRNA",                  SMolTypeInfo(SMolTypeInfo::eShown_Yes, CMolInfo::eBiomol_cRNA,            CSeq_inst::eMol_rna) },   
    {"DNA",                   SMolTypeInfo(SMolTypeInfo::eShown_No,  CMolInfo::eBiomol_genomic,         CSeq_inst::eMol_dna) },   
    {"Genomic",               SMolTypeInfo(SMolTypeInfo::eShown_No,  CMolInfo::eBiomol_genomic,         CSeq_inst::eMol_dna) },   
    {"Genomic DNA",           SMolTypeInfo(SMolTypeInfo::eShown_Yes, CMolInfo::eBiomol_genomic,         CSeq_inst::eMol_dna) },   
    {"Genomic RNA",           SMolTypeInfo(SMolTypeInfo::eShown_Yes, CMolInfo::eBiomol_genomic,         CSeq_inst::eMol_rna) },   
    {"mRNA",                  SMolTypeInfo(SMolTypeInfo::eShown_Yes, CMolInfo::eBiomol_mRNA,            CSeq_inst::eMol_rna) },   
    {"ncRNA",                 SMolTypeInfo(SMolTypeInfo::eShown_No,  CMolInfo::eBiomol_ncRNA,           CSeq_inst::eMol_rna) },
    {"non-coding RNA",        SMolTypeInfo(SMolTypeInfo::eShown_Yes, CMolInfo::eBiomol_ncRNA,           CSeq_inst::eMol_rna) },   
    {"Other-Genetic",         SMolTypeInfo(SMolTypeInfo::eShown_Yes, CMolInfo::eBiomol_other_genetic,   CSeq_inst::eMol_other) }, 
    {"Precursor RNA",         SMolTypeInfo(SMolTypeInfo::eShown_Yes, CMolInfo::eBiomol_pre_RNA,         CSeq_inst::eMol_rna) },   
    {"Ribosomal RNA",         SMolTypeInfo(SMolTypeInfo::eShown_Yes, CMolInfo::eBiomol_rRNA,            CSeq_inst::eMol_rna) },   
    {"rRNA",                  SMolTypeInfo(SMolTypeInfo::eShown_No,  CMolInfo::eBiomol_rRNA,            CSeq_inst::eMol_rna) },   
    {"Transcribed RNA",       SMolTypeInfo(SMolTypeInfo::eShown_Yes, CMolInfo::eBiomol_transcribed_RNA, CSeq_inst::eMol_rna) },   
    {"Transfer-messenger RNA", SMolTypeInfo(SMolTypeInfo::eShown_Yes, CMolInfo::eBiomol_tmRNA,           CSeq_inst::eMol_rna) },   
    {"Transfer RNA",          SMolTypeInfo(SMolTypeInfo::eShown_Yes, CMolInfo::eBiomol_tRNA,            CSeq_inst::eMol_rna) },   
    {"tRNA",                  SMolTypeInfo(SMolTypeInfo::eShown_No,  CMolInfo::eBiomol_tRNA,            CSeq_inst::eMol_rna) },   
};
typedef CStaticPairArrayMap<const char*, SMolTypeInfo,
                        CSourceModParser::PKeyCompare>  TBiomolMap;
DEFINE_STATIC_ARRAY_MAP(TBiomolMap, sc_BiomolMap, sc_BiomolArray);


void CSourceModParser::x_AddTopology(CSeq_inst& seq_inst) 
{
    const SMod* mod = nullptr;
    if ((mod = FindMod(s_Mod_topology, s_Mod_top)) != NULL) {
        if (NStr::EqualNocase(mod->value, "linear")) {
            seq_inst.SetTopology(CSeq_inst::eTopology_linear);
        } else if (NStr::EqualNocase(mod->value, "circular")) {
            seq_inst.SetTopology(CSeq_inst::eTopology_circular);
        } else {
            x_HandleBadModValue(*mod);
        }
    }
}


void CSourceModParser::x_AddMolType(CSeq_inst& seq_inst) 
{   

    if (seq_inst.IsSetMol() && 
        seq_inst.IsAa()) {
        return;
    }

    const SMod* mod = nullptr;
    // mol[ecule]
    if ((mod = FindMod(s_Mod_molecule, s_Mod_mol)) != NULL) {
        if (NStr::EqualNocase(mod->value, "dna")) {
            seq_inst.SetMol( CSeq_inst::eMol_dna );
            return;
        } else if (NStr::EqualNocase(mod->value, "rna")) {
            seq_inst.SetMol( CSeq_inst::eMol_rna );
            return;
        } else {
            x_HandleBadModValue(*mod);
        }
    }

    if ((mod = FindMod(s_Mod_moltype, s_Mod_mol_type)) != NULL) {
        TBiomolMap::const_iterator it = sc_BiomolMap.find(mod->value.c_str());
        if (it == sc_BiomolMap.end()) {
            x_HandleBadModValue(*mod);
        } else {
            // moltype sets biomol and inst.mol
            seq_inst.SetMol(it->second.m_eMol);
        }
    }
}

void CSourceModParser::x_AddStrand(CSeq_inst& seq_inst)
{
    const SMod* mod = nullptr;

    if ((mod = FindMod(s_Mod_strand)) != NULL) {
        if (NStr::EqualNocase(mod->value, "single")) {
            seq_inst.SetStrand( CSeq_inst::eStrand_ss );
        } else if (NStr::EqualNocase(mod->value, "double")) {
            seq_inst.SetStrand( CSeq_inst::eStrand_ds );
        } else if (NStr::EqualNocase(mod->value, "mixed")) {
            seq_inst.SetStrand( CSeq_inst::eStrand_mixed );
        } else {
            x_HandleBadModValue(*mod);
        }
    }
}


void CSourceModParser::x_ApplySeqInstMods(CSeq_inst& seq_inst)
{
    x_AddTopology(seq_inst);
    x_AddMolType(seq_inst);
    x_AddStrand(seq_inst);

    if (seq_inst.IsSetHist()) {
        ApplyMods(seq_inst.SetHist());
    } else {
        CAutoInitRef<CSeq_hist> hist;
        x_ApplyMods(hist);
        if (hist.IsInitialized()) {
            seq_inst.SetHist(*hist);
        }
    }
}


void CSourceModParser::ApplyMods(CBioseq& seq)
{
    const SMod* mod = NULL;
    x_AddTopology(seq.SetInst());
    x_AddMolType(seq.SetInst());
    x_AddStrand(seq.SetInst());

/*
    // molecule information is not set for proteins at this time
    // (Yes, the FIELD_CHAIN_OF_2_IS_SET is necessary because IsNa()
    // can throw an exception if mol isn't set)
    if( ! FIELD_CHAIN_OF_2_IS_SET(seq, Inst, Mol) || seq.IsNa() ) {
        bool bMolSetViaMolMod = false;

        // mol[ecule]
        if ((mod = FindMod(s_Mod_molecule, s_Mod_mol)) != NULL) {
            if (NStr::EqualNocase(mod->value, "dna")) {
                seq.SetInst().SetMol( CSeq_inst::eMol_dna );
                bMolSetViaMolMod = true;
            } else if (NStr::EqualNocase(mod->value, "rna")) {
                seq.SetInst().SetMol( CSeq_inst::eMol_rna );
                bMolSetViaMolMod = true;
            } else {
                x_HandleBadModValue(*mod);
            }
        }

        // if mol/molecule not set right, we can use moltype instead

        // mol[-]type
        if( ! bMolSetViaMolMod ) {
            if ((mod = FindMod(s_Mod_moltype, s_Mod_mol_type)) != NULL) {
                TBiomolMap::const_iterator it = sc_BiomolMap.find(mod->value.c_str());
                if (it == sc_BiomolMap.end()) {
                    x_HandleBadModValue(*mod);
                } else {
                    // moltype sets biomol and inst.mol
                    seq.SetInst().SetMol(it->second.m_eMol);
                }
            }
        }
    }

    // strand
    if ((mod = FindMod(s_Mod_strand)) != NULL) {
        if (NStr::EqualNocase(mod->value, "single")) {
            seq.SetInst().SetStrand( CSeq_inst::eStrand_ss );
        } else if (NStr::EqualNocase(mod->value, "double")) {
            seq.SetInst().SetStrand( CSeq_inst::eStrand_ds );
        } else if (NStr::EqualNocase(mod->value, "mixed")) {
            seq.SetInst().SetStrand( CSeq_inst::eStrand_mixed );
        } else {
            x_HandleBadModValue(*mod);
        }
    }
    */
    // comment

    if ((mod = FindMod(s_Mod_comment)) != NULL) {
        CRef<CSeqdesc> desc(new CSeqdesc);
        desc->SetComment( mod->value );
        seq.SetDescr().Set().push_back(desc);
    }
}


void CSourceModParser::x_ApplyOrgRefMods(CAutoInitRef<COrg_ref>& pOrgRef) 
{
    const SMod* pMod = nullptr;
    bool reset_taxid = false;
    // handle orgmods
    for(const auto & smod_orgsubtype : kSModOrgSubtypeMap.Get()) {
        const SMod & smod = smod_orgsubtype.first;
        const COrgMod::ESubtype e_subtype = smod_orgsubtype.second;
        if ((pMod = FindMod(smod.key)) != NULL) {
            CRef<COrgMod> org_mod(new COrgMod);
            org_mod->SetSubtype(e_subtype);
            org_mod->SetSubname(pMod->value);
            pOrgRef->SetOrgname().SetMod().push_back(org_mod);
            reset_taxid = true;
        }
    }

    x_AddDbXrefs(pOrgRef);

    // div[ison]
    if ((pMod = FindMod(s_Mod_division, s_Mod_div)) != NULL) {
        pOrgRef->SetOrgname().SetDiv( pMod->value );
    }

    // lineage
    if ((pMod = FindMod(s_Mod_lineage)) != NULL) {
        pOrgRef->SetOrgname().SetLineage( pMod->value );
    }
    
    // gcode
    if ((pMod = FindMod(s_Mod_gcode)) != NULL) {
        pOrgRef->SetOrgname().SetGcode( NStr::StringToInt(pMod->value, NStr::fConvErr_NoThrow) );
    }

    // mgcode
    if ((pMod = FindMod(s_Mod_mgcode)) != NULL) {
        pOrgRef->SetOrgname().SetMgcode( NStr::StringToInt(pMod->value, NStr::fConvErr_NoThrow) );
    }

    // pgcode
    if ((pMod = FindMod(s_Mod_pgcode)) != NULL) {
        pOrgRef->SetOrgname().SetPgcode( NStr::StringToInt(pMod->value, NStr::fConvErr_NoThrow) );
    }

    if ((pMod = FindMod(s_Mod_taxid)) != NULL) {
        pOrgRef->SetTaxId( NStr::StringToInt(pMod->value, NStr::fConvErr_NoThrow) );
    }
    else 
    if (reset_taxid && pOrgRef->IsSetOrgname() && pOrgRef->GetTaxId() != 0) {
        pOrgRef->SetTaxId(0);
    }
}


void CSourceModParser::x_AddDbXrefs(CAutoInitRef<COrg_ref>& pOrgRef) 
{
    for (const string& name : {"db_xref", "dbxref"}) {
        auto range = x_FindAllMods(name);
        for (auto it=range.first; it != range.second; ++it) {
            CRef<CDbtag> dbtag(new CDbtag());
            const CTempString value = it->value;
            size_t colon_location = value.find(":");
            if (colon_location == string::npos) {
                // no colon: it's just tag, and db is unknown
                dbtag->SetDb() = "?";
                dbtag->SetTag().SetStr(value);
            } else {
                // there's a colon, so db and tag are both known
                value.Copy(dbtag->SetDb(), 0, colon_location);
                value.Copy(dbtag->SetTag().SetStr(), colon_location+1, CTempString::npos);
            }
            pOrgRef->SetDb().push_back(move(dbtag));
        }
    }
}


void CSourceModParser::x_AddNotes(CAutoInitRef<CBioSource>& biosource) 
{
    for (const string& name : {"note", "notes"}) {
        auto range = x_FindAllMods(name);
        for (auto it=range.first; it!=range.second; ++it) {
            CRef<CSubSource> subsource(new CSubSource());
            subsource->SetSubtype(CSubSource::eSubtype_other);
            subsource->SetName(it->value);
            biosource->SetSubtype().push_back(move(subsource));
        }
    }
}

/*
void CSourceModParser::x_AddPCRPrimers(CAutoInitRef<CPCRReaction>& pcr_reaction)
{
    vector<string> names;
    NStr::Split(primer_info.first, ":", names, NStr::fSplit_Tokenize);
    vector<string> seqs;
    NStr::Split(primer_info.second, ":", seqs, NStr::fSplit_Tokenize);

    const auto num_names = names.size();
    const auto num_seqs = seqs.size();
    const auto num_primers = max(num_names, num_seqs);

    for(size_t i=0; i<num_primers; ++i) {
        auto primer = Ref(new CPCRPrimer());

        if (i<num_names && !NStr::IsBlank(names[i])) {
            primer->SetName().Set(names[i]); 
        }
        if (i<num_seqs && !NStr::IsBlank(seqs[i])) {
            primer->SetSeq().Set(seqs[i]);
        }
        primer_set.Set().push_back(primer);
    }
}

static void s_GetPrimerInfo(const CSourceModParser::SMod* pNamesMod,
                            const CSourceModParser::SMod* pSeqsMod,
                            vector<pair<string, string>>& reaction_info)
{
    reaction_info.clear();
    vector<string> names;
    if (pNamesMod) {
        NStr::Split(pNamesMod->value, ",", names, NStr::fSplit_Tokenize);
    }

    vector<string> seqs;
    if (pSeqsMod) {
        NStr::Split(pSeqsMod->value, ",", seqs, NStr::fSplit_Tokenize);
        if (seqs.size()>1) {
            if (seqs.front().front() == '(') {
                seqs.front().erase(0,1);
            }
            if (seqs.back().back() == ')') {
                seqs.back().erase(seqs.back().size()-1, 1);
            }
        }
    }

    const auto num_names = names.size();
    const auto num_seqs = seqs.size();
    const auto num_reactions = max(num_names, num_seqs);

    for (int i=0; i<num_reactions; ++i) {
        const string name = (i<num_names) ? names[i] : "";
        const string seq  = (i<num_seqs) ? seqs[i] : "";
        reaction_info.push_back(make_pair(name, seq));
    }
}
*/

static void s_AddPrimers(const pair<string, string>& primer_info, CPCRPrimerSet& primer_set)
{
    vector<string> names;
    NStr::Split(primer_info.first, ":", names, NStr::fSplit_Tokenize);
    vector<string> seqs;
    NStr::Split(primer_info.second, ":", seqs, NStr::fSplit_Tokenize);

    const auto num_names = names.size();
    const auto num_seqs = seqs.size();
    const auto num_primers = max(num_names, num_seqs);

    for(size_t i=0; i<num_primers; ++i) {
        auto primer = Ref(new CPCRPrimer());

        if (i<num_names && !NStr::IsBlank(names[i])) {
            primer->SetName().Set(names[i]); 
        }
        if (i<num_seqs && !NStr::IsBlank(seqs[i])) {
            primer->SetSeq().Set(seqs[i]);
        }
        primer_set.Set().push_back(primer);
    }
}


static void s_GetPrimerInfo(const CSourceModParser::SMod* pNamesMod,
                            const CSourceModParser::SMod* pSeqsMod,
                            vector<pair<string, string>>& reaction_info)
{
    reaction_info.clear();
    vector<string> names;
    if (pNamesMod) {
        NStr::Split(pNamesMod->value, ",", names, NStr::fSplit_Tokenize);
    }

    vector<string> seqs;
    if (pSeqsMod) {
        NStr::Split(pSeqsMod->value, ",", seqs, NStr::fSplit_Tokenize);
        if (seqs.size()>1) {
            if (seqs.front().front() == '(') {
                seqs.front().erase(0,1);
            }
            if (seqs.back().back() == ')') {
                seqs.back().erase(seqs.back().size()-1, 1);
            }
        }
    }

    const auto num_names = names.size();
    const auto num_seqs = seqs.size();
    const auto num_reactions = max(num_names, num_seqs);

    for (int i=0; i<num_reactions; ++i) {
        const string name = (i<num_names) ? names[i] : "";
        const string seq  = (i<num_seqs) ? seqs[i] : "";
        reaction_info.push_back(make_pair(name, seq));
    }
}

void CSourceModParser::x_AddPCRPrimers(CAutoInitRef<CPCRReactionSet>& pcr_reaction_set)
{
    using TNameSeqPair = pair<string, string>;

    const SMod* pNameMod = nullptr;
    const SMod* pSeqMod = nullptr;

    pNameMod = FindMod(s_Mod_fwd_primer_name, s_Mod_fwd_pcr_primer_name);
    pSeqMod = FindMod(s_Mod_fwd_primer_seq, s_Mod_fwd_pcr_primer_seq);
    vector<TNameSeqPair> fwd_primer_info; 
    s_GetPrimerInfo(pNameMod, pSeqMod, fwd_primer_info);


    pNameMod = FindMod(s_Mod_rev_primer_name, s_Mod_rev_pcr_primer_name);
    pSeqMod = FindMod(s_Mod_rev_primer_seq, s_Mod_rev_pcr_primer_seq);
    vector<TNameSeqPair> rev_primer_info; 
    s_GetPrimerInfo(pNameMod, pSeqMod, rev_primer_info);

    if (fwd_primer_info.empty() &&
        rev_primer_info.empty()) {
        return;
    }

    auto num_fwd_primer_info = fwd_primer_info.size();
    auto num_rev_primer_info = rev_primer_info.size();

    if (num_fwd_primer_info == num_rev_primer_info) {
    
        for (auto i=0; i<num_fwd_primer_info; ++i) {
            CRef<CPCRReaction> pcr_reaction(new CPCRReaction());
            s_AddPrimers(fwd_primer_info[i], pcr_reaction->SetForward());
            s_AddPrimers(rev_primer_info[i], pcr_reaction->SetReverse());
            pcr_reaction_set->Set().push_back(pcr_reaction);
        }
    } 
    else 
    if (num_fwd_primer_info > num_rev_primer_info) {
        auto diff = num_fwd_primer_info - num_rev_primer_info;
        for (int i=0; i<diff; ++i) {
            CRef<CPCRReaction> pcr_reaction(new CPCRReaction());
            s_AddPrimers(fwd_primer_info[i], pcr_reaction->SetForward());
            pcr_reaction_set->Set().push_back(pcr_reaction);
        }

        for (int i=diff; i<num_fwd_primer_info; ++i) {
            CRef<CPCRReaction> pcr_reaction(new CPCRReaction());
            s_AddPrimers(fwd_primer_info[i], pcr_reaction->SetForward());
            s_AddPrimers(rev_primer_info[i-diff], pcr_reaction->SetReverse());
            pcr_reaction_set->Set().push_back(pcr_reaction);
        }
    }
    else 
    if (num_fwd_primer_info < num_rev_primer_info) {
        for (int i=0; i<num_fwd_primer_info; ++i) {
            CRef<CPCRReaction> pcr_reaction(new CPCRReaction());
            s_AddPrimers(fwd_primer_info[i], pcr_reaction->SetForward());
            s_AddPrimers(rev_primer_info[i], pcr_reaction->SetReverse());
            pcr_reaction_set->Set().push_back(pcr_reaction);
        }

        for (int i=num_fwd_primer_info; i<num_rev_primer_info; ++i) {
            CRef<CPCRReaction> pcr_reaction(new CPCRReaction());
            s_AddPrimers(rev_primer_info[i], pcr_reaction->SetReverse());
            pcr_reaction_set->Set().push_back(pcr_reaction);
        }
    } 
}


void CSourceModParser::x_GetSubtype(CBioSource::TSubtype& subtype) 
{
    const SMod* mod;
    // handle subsources
    for( const auto & smod_subsrcsubtype : kSModSubSrcSubtypeMap.Get() ) {
        const SMod & smod = smod_subsrcsubtype.first;
        const CSubSource::ESubtype e_subtype = smod_subsrcsubtype.second;
        if ((mod = FindMod(smod.key)) != NULL) {
            CRef<CSubSource> subsource(new CSubSource);
            subsource->SetSubtype(e_subtype);

            if( CSubSource::NeedsNoText(e_subtype) ) {
                subsource->SetName(kEmptyStr);
            } else {
                subsource->SetName(mod->value);
            }
            subtype.push_back(subsource);
        }
    }
}


void CSourceModParser::x_ApplyMods(CAutoInitRef<CBioSource>& bsrc,
                                   CTempString organism)
{

    const SMod* mod = NULL;
    bool reset_taxid = false;

    // org[anism]
    if (organism.empty())
    {
        if ((mod = FindMod(s_Mod_organism, s_Mod_org)) != NULL) {
            organism = mod->value;
        }
        else
        if ((mod = FindMod(s_Mod_taxname)) != NULL) {
            organism = mod->value;
        }
    }


    bool biosource_set = false;
    if ( !organism.empty())
    {
        if (!(bsrc->GetOrg().IsSetTaxname() && NStr::EqualNocase(bsrc->GetOrg().GetTaxname(), organism)))
        {
            if (bsrc->GetOrg().IsSetTaxname())
            {
                bsrc->ResetOrg();
//                bsrc->ResetSubtype();
            }
            bsrc->SetOrg().SetTaxname(organism);
            bsrc->SetOrg().SetTaxId(0);
            biosource_set = true;
        }
    }

    // location
    if ((mod = FindMod(s_Mod_location)) != NULL) {
        if (NStr::EqualNocase(mod->value, "mitochondrial")) {
            bsrc->SetGenome(CBioSource::eGenome_mitochondrion);
        } else if (NStr::EqualNocase(mod->value, "provirus")) {
            bsrc->SetGenome(CBioSource::eGenome_proviral);
        } else if (NStr::EqualNocase(mod->value, "extrachromosomal")) {
            bsrc->SetGenome(CBioSource::eGenome_extrachrom);
        } else if (NStr::EqualNocase(mod->value, "insertion sequence")) {
            bsrc->SetGenome(CBioSource::eGenome_insertion_seq);
        } else {
            try {
                bsrc->SetGenome(CBioSource::GetTypeInfo_enum_EGenome()->FindValue(mod->value));
            } catch (CSerialException&) {
                x_HandleBadModValue(*mod);
            }
        }
    }

    // origin
    if ((mod = FindMod(s_Mod_origin)) != NULL) {
        try {
            // also check for special cases that don't match the enum name
            if( NStr::EqualNocase(mod->value, "natural mutant") ) {
                bsrc->SetOrigin( CBioSource::eOrigin_natmut );
            } else if( NStr::EqualNocase(mod->value, "mutant") ) {
                bsrc->SetOrigin( CBioSource::eOrigin_mut );
            } else {
                bsrc->SetOrigin(CBioSource::GetTypeInfo_enum_EOrigin()
                                ->FindValue(mod->value));
            }
        } catch (CSerialException&) {
            x_HandleBadModValue(*mod);
        }
    }


    CBioSource::TSubtype subtype;
    x_GetSubtype(subtype);
    if (!subtype.empty()) {
        auto& bsrc_subtype = bsrc->SetSubtype();
        bsrc_subtype.splice(bsrc_subtype.end(), subtype);
    }


    {{
        CAutoInitRef<CPCRReactionSet> pcr_reaction_set;
        x_AddPCRPrimers(pcr_reaction_set);
        if (pcr_reaction_set.IsInitialized()) {
            if (!bsrc->IsSetPcr_primers()) {
                bsrc->SetPcr_primers(*pcr_reaction_set);
            }
            else {
                bsrc->SetPcr_primers().Set().splice(
                        bsrc->SetPcr_primers().Set().end(),
                        pcr_reaction_set->Set());
            }
        }
    }}


    {{
        CAutoInitRef<COrg_ref> pOrgRef;
        if (biosource_set && bsrc->IsSetOrg()) {
            pOrgRef.Set(&bsrc->SetOrg());
        }
        x_ApplyOrgRefMods(pOrgRef);
        if (pOrgRef.IsInitialized() && !bsrc->IsSetOrg()) {
            bsrc->SetOrg(*pOrgRef);
        }
    }}

    // note[s]
    //
    x_AddNotes(bsrc);
    // focus
    if ((mod = FindMod(s_Mod_focus)) != NULL) {
        if( NStr::EqualNocase( mod->value, "TRUE" ) ) {
            bsrc->SetIs_focus();
        }
    }
}

typedef SStaticPair<const char*, CMolInfo::TTech> TTechMapEntry;
static const TTechMapEntry sc_TechArray[] = {
    { "?",                  CMolInfo::eTech_unknown },
    { "barcode",            CMolInfo::eTech_barcode },
    { "both",               CMolInfo::eTech_both },
    { "composite-wgs-htgs", CMolInfo::eTech_composite_wgs_htgs },
    { "concept-trans",      CMolInfo::eTech_concept_trans },
    { "concept-trans-a",    CMolInfo::eTech_concept_trans_a },
    { "derived",            CMolInfo::eTech_derived },
    { "EST",                CMolInfo::eTech_est },
    { "fli cDNA",           CMolInfo::eTech_fli_cdna },
    { "genetic map",        CMolInfo::eTech_genemap },
    { "htc",                CMolInfo::eTech_htc },
    { "htgs 0",             CMolInfo::eTech_htgs_0 },
    { "htgs 1",             CMolInfo::eTech_htgs_1 },
    { "htgs 2",             CMolInfo::eTech_htgs_2 },
    { "htgs 3",             CMolInfo::eTech_htgs_3 },
    { "physical map",       CMolInfo::eTech_physmap },
    { "seq-pept",           CMolInfo::eTech_seq_pept },
    { "seq-pept-homol",     CMolInfo::eTech_seq_pept_homol },
    { "seq-pept-overlap",   CMolInfo::eTech_seq_pept_overlap },
    { "standard",           CMolInfo::eTech_standard },
    { "STS",                CMolInfo::eTech_sts },
    { "survey",             CMolInfo::eTech_survey },
    { "targeted",           CMolInfo::eTech_targeted },
    { "tsa",                CMolInfo::eTech_tsa },
    { "wgs",                CMolInfo::eTech_wgs }
};
typedef CStaticPairArrayMap<const char*, CMolInfo::TTech,
CSourceModParser::PKeyCompare>  TTechMap;
DEFINE_STATIC_ARRAY_MAP(TTechMap, sc_TechMap, sc_TechArray);

typedef SStaticPair<const char*, CMolInfo::TCompleteness> TCompletenessMapEntry;
static const TCompletenessMapEntry sc_CompletenessArray[] = {
    { "complete",  CMolInfo::eCompleteness_complete  },
    { "has-left",  CMolInfo::eCompleteness_has_left  },
    { "has-right", CMolInfo::eCompleteness_has_right  },
    { "no-ends",   CMolInfo::eCompleteness_no_ends  },
    { "no-left",   CMolInfo::eCompleteness_no_left  },
    { "no-right",  CMolInfo::eCompleteness_no_right  },
    { "partial",   CMolInfo::eCompleteness_partial  }
};
typedef CStaticPairArrayMap<const char*, CMolInfo::TCompleteness,
CSourceModParser::PKeyCompare>  TCompletenessMap;
DEFINE_STATIC_ARRAY_MAP(TCompletenessMap, sc_CompletenessMap, sc_CompletenessArray);


void CSourceModParser::x_ApplyMods(CAutoInitRef<CMolInfo>& mi)
{
    const SMod* mod = NULL;
    // mol[-]type
    if ((mod = FindMod(s_Mod_moltype, s_Mod_mol_type)) != NULL) {
        TBiomolMap::const_iterator it = sc_BiomolMap.find(mod->value.c_str());
        if (it == sc_BiomolMap.end()) {
            // construct the possible bad values by hand
            x_HandleBadModValue(*mod);
        } else {
            // moltype sets biomol and inst.mol
            mi->SetBiomol(it->second.m_eBiomol);
        }
    }

    // tech
    if ((mod = FindMod(s_Mod_tech)) != NULL) {
        TTechMap::const_iterator it = sc_TechMap.find(mod->value.c_str());
        if (it == sc_TechMap.end()) {
            x_HandleBadModValue(*mod);
        } else {
            mi->SetTech(it->second);
        }
    }

    // complete[d]ness
    if ((mod = FindMod(s_Mod_completeness, s_Mod_completedness)) != NULL) {
        TTechMap::const_iterator it = sc_CompletenessMap.find(mod->value.c_str());
        if (it == sc_CompletenessMap.end()) {
            x_HandleBadModValue(*mod);
        } else {
            mi->SetCompleteness(it->second);
        }
    }
}


void CSourceModParser::x_ApplyMods(CAutoInitRef<CGene_ref>& gene)
{
    const SMod* mod = NULL;

    // gene
    if ((mod = FindMod(s_Mod_gene)) != NULL) {
        gene->SetLocus(mod->value);
    }

    // allele
    if ((mod = FindMod(s_Mod_allele)) != NULL) {
        gene->SetAllele( mod->value );
    }

    // gene_syn[onym]
    if ((mod = FindMod(s_Mod_gene_syn, s_Mod_gene_synonym)) != NULL) {
        gene->SetSyn().push_back( mod->value );
    }
    
    // locus_tag
    if ((mod = FindMod(s_Mod_locus_tag)) != NULL) {
        gene->SetLocus_tag( mod->value );
    }
}


void CSourceModParser::x_ApplyMods(CAutoInitRef<CProt_ref>& prot)
{
    const SMod* mod = NULL;

    // prot[ein]
    if ((mod = FindMod(s_Mod_protein, s_Mod_prot)) != NULL) {
        prot->SetName().push_back(mod->value);
    }

    // prot[ein]_desc
    if ((mod = FindMod(s_Mod_prot_desc, s_Mod_protein_desc)) != NULL) {
        prot->SetDesc( mod->value );
    }
    
    // EC_number 
    if ((mod = FindMod(s_Mod_EC_number)) != NULL) {
        prot->SetEc().push_back( mod->value );
    }

    // activity/function
    if ((mod = FindMod(s_Mod_activity, s_Mod_function)) != NULL) {
        prot->SetActivity().push_back( mod->value );
    }
}


void CSourceModParser::x_ApplyMods(CAutoInitRef<CGB_block>& gbb)
{
    const SMod* mod = NULL;

    // secondary-accession[s]
    if ((mod = FindMod(s_Mod_secondary_accession,
                       s_Mod_secondary_accessions)) != NULL)
    {
        list<CTempString> ranges;
        NStr::Split(mod->value, ",", ranges, NStr::fSplit_MergeDelimiters);
        ITERATE (list<CTempString>, it, ranges) {
            string s = NStr::TruncateSpaces_Unsafe(*it);
            try {
                SSeqIdRange range(s);
                ITERATE (SSeqIdRange, it2, range) {
                    gbb->SetExtra_accessions().push_back(*it2);
                }
            } catch (CSeqIdException&) {
                gbb->SetExtra_accessions().push_back(s);
            }
        }
    }

    // keyword[s]
    if ((mod = FindMod(s_Mod_keyword, s_Mod_keywords)) != NULL) {
        list<string> keywordList;
        NStr::Split(mod->value, ",;", keywordList, NStr::fSplit_MergeDelimiters);
        // trim every string and push it into the real keyword list
        NON_CONST_ITERATE( list<string>, keyword_iter, keywordList ) {
            NStr::TruncateSpacesInPlace( *keyword_iter );
            gbb->SetKeywords().push_back( *keyword_iter );
        }
    }
}


void CSourceModParser::x_ApplyMods(CAutoInitRef<CSeq_hist>& hist)
{
    const SMod* mod = NULL;

    // secondary-accession[s]
    if ((mod = FindMod(s_Mod_secondary_accession,
                       s_Mod_secondary_accessions)) != NULL)
    {
        list<CTempString> ranges;
        NStr::Split(mod->value, ",", ranges, NStr::fSplit_MergeDelimiters);
        ITERATE (list<CTempString>, it, ranges) {
            string s = NStr::TruncateSpaces_Unsafe(*it);
            try {
                SSeqIdRange range(s);
                ITERATE (SSeqIdRange, it2, range) {
                    hist->SetReplaces().SetIds().push_back(it2.GetID());
                }
            } catch (CSeqIdException&) {
                NStr::ReplaceInPlace(s, "ref_seq|", "ref|", 0, 1);
                hist->SetReplaces().SetIds()
                    .push_back(CRef<CSeq_id>(new CSeq_id(s)));
            }
        }
    }
}

// Note: It's untested.
//
// This code is currently unused, but I'm leaving it here in case
// at some point in the future someone decides that we do want it.
//
// We're not using this because it would introduce a whole new
// dependency just for a single keyword.
//
//void CSourceModParser::x_ApplyMods(CAutoInitRef<CSubmit_block>& sb) { 
//
//    // hup
//    if ((mod = FindMod("hup")) != NULL) {
//        sb->SetHup( false );
//        sb->ResetReldate();
//        if( ! mod->value.empty() ) {
//            if( NStr::EqualNocase( mod->value, "y" ) ) {
//                sb->SetHup( true );
//                // by default, release in a year
//                CDate releaseDate( CTime(CTime::eCurrent) );
//                _ASSERT(releaseDate.IsStd());
//                releaseDate.GetStd().SetYear( releaseDate.GetStd().GetYear() + 1 );
//                sb->SetReldate( releaseDate );
//            } else {
//                // parse string as "m/d/y" (or with "-" instead of "/" )
//                try {
//                    CTime hupTime( NStr::Replace( mod->value, "-", "/" ), "M/D/Y" );
//                    sb->SetReldate( CDate(hupTime) );
//                    sb->SetHup( true );
//                } catch( const CException & e) {
//                    // couldn't parse date
//                    x_HandleBadModValue(*mod);
//                }
//            }
//        }
//    }
//}


static
void s_PopulateUserObject(CUser_object& uo, const string& type,
                          CUser_object::TData& data)
{
    if (uo.GetType().Which() == CObject_id::e_not_set) {
        uo.SetType().SetStr(type);
    } else if ( !uo.GetType().IsStr()  ||  uo.GetType().GetStr() != type) {
        // warn first?
        return;
    }

    swap(uo.SetData(), data);
}


void CSourceModParser::x_ApplyTPAMods(CAutoInitRef<CUser_object>& tpa)
{
    const SMod* mod = NULL;

    // primary[-accessions]
    if ((mod = FindMod(s_Mod_primary, s_Mod_primary_accessions)) != NULL) {
        CUser_object::TData data;
        list<CTempString> accns;
        NStr::Split(mod->value, ",", accns, NStr::fSplit_MergeDelimiters);
        ITERATE (list<CTempString>, it, accns) {
            CRef<CUser_field> field(new CUser_field), subfield(new CUser_field);
            field->SetLabel().SetId(0);
            subfield->SetLabel().SetStr("accession");
            subfield->SetData().SetStr(CUtf8::AsUTF8(*it, eEncoding_UTF8));
            field->SetData().SetFields().push_back(subfield);
            data.push_back(field);
        }

        if ( !data.empty() ) {
            s_PopulateUserObject(*tpa, "TpaAssembly", data);
        }
    }
}

void CSourceModParser::x_ApplySRAMods(CAutoAddDBLink& sra_obj)
{
    auto range = FindAllMods(s_Mod_SRA);
    for (auto it = range.first; it != range.second ; it++)
    {
        const SMod& mod = *it;

        list<CTempString> sras;
        NStr::Split(mod.value, ",", sras, NStr::fSplit_MergeDelimiters);

        if (!sras.empty())
        {
            for (auto& sra : sras)
            {
                sra = NStr::TruncateSpaces_Unsafe(sra);
                if (!sra.empty())
                  sra_obj.Get().SetData().SetStrs().push_back(sra);
            }
        }
    }
    if (sra_obj.IsInitialised())
        sra_obj.Get().SetNum(int(sra_obj.Get().GetData().GetStrs().size()));
}



void
CSourceModParser::x_ApplyGenomeProjectsDBMods(CAutoInitRef<CUser_object>& gpdb)
{
    const SMod* mod = NULL;

    // project[s]
    if ((mod = FindMod(s_Mod_project, s_Mod_projects)) != NULL) {
        CUser_object::TData data;
        list<CTempString> ids;
        NStr::Split(mod->value, ",;", ids, NStr::fSplit_MergeDelimiters);
        ITERATE (list<CTempString>, it, ids) {
            unsigned int id = NStr::StringToUInt(*it, NStr::fConvErr_NoThrow);
            if (id > 0) {
                CRef<CUser_field> field(new CUser_field),
                               subfield(new CUser_field);
                field->SetLabel().SetId(0);
                subfield->SetLabel().SetStr("ProjectID");
                subfield->SetData().SetInt(id);
                field->SetData().SetFields().push_back(subfield);
                subfield.Reset(new CUser_field);
                subfield->SetLabel().SetStr("ParentID");
                subfield->SetData().SetInt(0);
                field->SetData().SetFields().push_back(subfield);
                data.push_back(field);                
            }
        }

        if ( !data.empty() ) {
            s_PopulateUserObject(*gpdb, "GenomeProjectsDB", data);
        }
    }
}


static
void s_ApplyPubMods(CBioseq& bioseq, const CSourceModParser::TModsRange& range)
{
    for (CSourceModParser::TModsCI it = range.first;
         it != range.second;  ++it) {
        TIntId pmid = NStr::StringToNumeric<TIntId>(it->value, NStr::fConvErr_NoThrow);
        CRef<CPub> pub(new CPub);
        pub->SetPmid().Set(pmid);
        CRef<CSeqdesc> pubdesc(new CSeqdesc);
        pubdesc->SetPub().SetPub().Set().push_back(pub);
        bioseq.SetDescr().Set().push_back(pubdesc);
    }
}


void CSourceModParser::ApplyPubMods(CBioseq& seq)
{
    // find PubMed IDs
    s_ApplyPubMods(seq, FindAllMods(s_Mod_PubMed));
    s_ApplyPubMods(seq, FindAllMods(s_Mod_PMID));
}

CSourceModParser::CBadModError::CBadModError( 
    const SMod & badMod, 
    const string & sAllowedValues )
    : runtime_error(x_CalculateErrorString(badMod, sAllowedValues)),
            m_BadMod(badMod), m_sAllowedValues(sAllowedValues) 
{ 
    // no further work required
}

string CSourceModParser::CBadModError::x_CalculateErrorString(
            const SMod & badMod, 
            const string & sAllowedValues )
{
    stringstream str_strm;
    str_strm << "Bad modifier value at seqid '" 
        << ( badMod.seqid ? badMod.seqid->AsFastaString() : "UNKNOWN")
        << "'. '" << badMod.key << "' cannot have value '" << badMod.value
        << "'.  Accepted values are [" << sAllowedValues << "]";
    return str_strm.str();
}

CSourceModParser::CUnkModError::CUnkModError(
    const SMod& unkMod )
    : runtime_error(x_CalculateErrorString(unkMod)), m_UnkMod(unkMod)
{
}

string CSourceModParser::CUnkModError::x_CalculateErrorString(
    const SMod& unkMod)
{
    stringstream str_strm;
    str_strm << "Bad modifier key at seqid '" 
        << ( unkMod.seqid ? unkMod.seqid->AsFastaString() : "UNKNOWN")
        << "'. '" << unkMod.key << "' is not a recognized modifier key";
    return str_strm.str();
}


CSourceModParser::TMods CSourceModParser::GetMods(TWhichMods which) const
{

    TMods result;
    if (which == fAllMods) {
        for (const auto& kv : m_ModNameMap) {
            result.insert(kv.second);
        }
        return result;
    }
    // else
    const bool used = (which == fUsedMods);

    for (const auto& kv : m_ModNameMap) {
        const auto& mod = kv.second;
        if (mod.used == used) {
            result.insert(mod);
        }
    }

    return result;
}

const CSourceModParser::SMod* CSourceModParser::FindMod(
    const CTempString& key, const CTempString& alt_key)
{

    // check against m_pModFilter, if any
    if( m_pModFilter ) {
        if( ! (*m_pModFilter)(key) || ! (*m_pModFilter)(alt_key) ) {
            return NULL;
        }
    }

    auto it = m_ModNameMap.find(key);
    if (it == m_ModNameMap.end()) {
        it = m_ModNameMap.find(alt_key);
    }

    if (it != m_ModNameMap.end()) {
        const_cast<SMod&>(it->second).used = true;
        return &(it->second);
    }
    return nullptr;
}

CSourceModParser::TModsRange
CSourceModParser::FindAllMods(const CTempString& key)
{
    SMod smod(key);
    return FindAllMods(smod);
}


CSourceModParser::TModsRange
CSourceModParser::FindAllMods(const CTempString& key, const CTempString& alt_key)
{
    SMod smod(key);
    SMod alt_smod(alt_key);
    return FindAllMods(smod, alt_smod);
}


CSourceModParser::TRange
CSourceModParser::x_FindAllMods(const CTempString& name) 
{
    auto range = m_ModNameMap.equal_range(name);
    if (range.first == range.second) {
        return TRange();
    }

    for (auto it = range.first; it != range.second; ++it) {
        const_cast<SMod&>(it->second).used = true;
    }

    return TRange(TIterator(range.first), TIterator(range.second));
}

CSourceModParser::TModsRange
CSourceModParser::FindAllMods(const SMod & smod, const SMod & alt_smod)
{
    TModsRange r;
    r.first = m_Mods.lower_bound(smod);
    if (r.first == m_Mods.end() || !EqualKeys(r.first->key, smod.key)) {
        r.first = m_Mods.lower_bound(alt_smod);
    }
    for (r.second = r.first;
         r.second != m_Mods.end()  &&  (EqualKeys(r.second->key, smod.key) || EqualKeys(r.second->key, alt_smod.key));
         ++r.second)
    {
        // set iterators are const since changing an object could affect
        // its order in the set.  However, in this case we know that
        // changing the `used` field won't affect the order so we know
        // that a const_cast to change it is safe to do.
        const_cast<SMod&>(*r.second).used = true;
    }
    return r;
}


void CSourceModParser::GetLabel(string* s, TWhichMods which) const
{
    // Possible (flag-conditional?) behavior changes:
    // - leave off spaces between modifiers
    // - sort by position rather than key
    _ASSERT(s != NULL);

    string delim = s->empty() ? kEmptyStr : " ";
/*
    ITERATE (TMods, it, m_Mods) {
        if ((which & (it->used ? fUsedMods : fUnusedMods)) != 0) {
            *s += delim + '[' + it->key + '=' + it->value + ']';
            delim = " ";
        }
    }

*/
    const bool used = (which == fUsedMods);
    for (const auto& kv : m_ModNameMap) {
        const auto& mod = kv.second;
        if (mod.used == used) {
            *s += delim + '[' + mod.key + '=' + mod.value + ']';
            delim = " ";
        }
    }
}

// static 
const set<string> & 
CSourceModParser::GetModAllowedValues(const string &mod)
{
    // since this has a lock, do NOT grab any other locks
    // inside here.
    static CMutex mutex;
    CMutexGuard guard(mutex);

    typedef map< string, set<string>, CSourceModParser::PKeyCompare> TMapModToValidValues;
    static TMapModToValidValues s_mapModToValidValues;

    // see if value is already calculated to try to save time
    TMapModToValidValues::const_iterator find_iter =
        s_mapModToValidValues.find(mod);
    if( find_iter != s_mapModToValidValues.end() ) {
        return find_iter->second;
    }

    // does canonical comparison, which goes a little beyond case-insensitivity
    PKeyEqual key_equal;

    // not cached, so we need to calculate it ourselves
    set<string> & set_valid_values = s_mapModToValidValues[mod];
    if( key_equal(mod, "topology") || key_equal(mod, "top") ) {
        set_valid_values.insert("linear");
        set_valid_values.insert("circular");
    } else if( key_equal(mod, "molecule") || key_equal(mod, "mol") ) {
        set_valid_values.insert("rna");
        set_valid_values.insert("dna");
    } else if( key_equal(mod, "moltype") || key_equal(mod, "mol-type") ) {
        // construct the possible bad values by hand
        ITERATE( TBiomolMap, map_iter, sc_BiomolMap ) {
            if( map_iter->second.m_eShown == SMolTypeInfo::eShown_Yes ) {
                set_valid_values.insert(map_iter->first);
            }
        }
    } else if( key_equal(mod, "strand") ) {
        set_valid_values.insert("single");
        set_valid_values.insert("double");
        set_valid_values.insert("mixed");
    } else if( key_equal(mod, "location") ) {
        set_valid_values.insert("mitochondrial");
        set_valid_values.insert("provirus");
        set_valid_values.insert("extrachromosomal");
        set_valid_values.insert("insertion sequence");
    } else if( key_equal(mod, "origin") ) {
        set_valid_values.insert("natural mutant");
        set_valid_values.insert("mutant");
        ITERATE( CEnumeratedTypeValues::TValues, enum_iter, CBioSource::GetTypeInfo_enum_EOrigin()->GetValues() ) {
            set_valid_values.insert( enum_iter->first );
        }
    } else if( key_equal(mod, "tech") ) {
        ITERATE(TTechMap, tech_it, sc_TechMap) {
            set_valid_values.insert(tech_it->first);
        }
    } else if( key_equal(mod, "completeness") || key_equal(mod, "completedness") ) {
        ITERATE( TCompletenessMap, comp_it, sc_CompletenessMap ) {
            set_valid_values.insert(comp_it->first);
        }
    } else {
        set_valid_values.insert("ERROR TRYING TO DETERMINE ALLOWED VALUES");
    }

    return set_valid_values;
}

// static 
const string & 
CSourceModParser::GetModAllowedValuesAsOneString(const string &mod)
{
    // do not grab any other locks while in here (except the lock in 
    // GetModAllowedValues)
    static CMutex mutex;
    CMutexGuard guard(mutex);

    typedef map<string, string> TMapModNameToStringOfAllAllowedValues;
    static TMapModNameToStringOfAllAllowedValues mapModNameToStringOfAllAllowedValues;

    // see if we've already cached the value
    TMapModNameToStringOfAllAllowedValues::const_iterator find_iter =
        mapModNameToStringOfAllAllowedValues.find(mod);
    if( find_iter != mapModNameToStringOfAllAllowedValues.end() ) {
        return find_iter->second;
    }

    // not loaded, so we need to calculate it
    string & sAllValuesAsOneString = 
        mapModNameToStringOfAllAllowedValues[mod];
    const set<string> & setAllowedValues = GetModAllowedValues(mod);
    ITERATE( set<string>, value_it, setAllowedValues ) {
        if( ! sAllValuesAsOneString.empty() ) {
            sAllValuesAsOneString += ", ";
        }
        sAllValuesAsOneString += "'" + *value_it + "'";
    }

    return sAllValuesAsOneString;
}

void CSourceModParser::x_HandleBadModValue(
    const SMod& mod)
{
    m_BadMods.insert(mod);

    if( eHandleBadMod_Ignore == m_HandleBadMod ) {
        return;
    }

    const string & sAllAllowedValues = GetModAllowedValuesAsOneString(mod.key);

    CBadModError badModError(mod, sAllAllowedValues);

    switch( m_HandleBadMod ) {
    case eHandleBadMod_Throw:
        throw badModError;
    case eHandleBadMod_PrintToCerr:
        cerr << badModError.what() << endl;
        break;
    case eHandleBadMod_ErrorListener: {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
                eDiag_Warning,
                m_LineNumber,
                badModError.what(),
                ILineError::eProblem_GeneralParsingError) );
        x_ProcessError(*pErr);
        break;
    }
    default:
        _TROUBLE;
    }
}

void CSourceModParser::x_HandleUnkModValue(
    const SMod& mod)
{
    if (m_HandleBadMod == eHandleBadMod_Ignore) {
        return;
    }
    if (m_pModFilter  &&  !m_pModFilter->operator()(mod.key)) {
        return;
    }
    CUnkModError unkModError(mod);

    switch( m_HandleBadMod ) {
    case eHandleBadMod_Throw:
        throw unkModError;
    case eHandleBadMod_PrintToCerr:
        cerr << unkModError.what() << endl;
        break;
    case eHandleBadMod_ErrorListener: {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
                eDiag_Warning,
                m_LineNumber,
                unkModError.what(),
                ILineError::eProblem_GeneralParsingError) );
        x_ProcessError(*pErr);
        break;
    }
    default:
        _TROUBLE;
    }
}

void CSourceModParser::x_ProcessError(
    CObjReaderLineException& err)
{
    if (!m_pErrorListener) {
        err.Throw();
    }
    if (!m_pErrorListener->PutError(err)) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
                eDiag_Critical,
                0,
                "Error allowance exceeded",
                ILineError::eProblem_GeneralParsingError) );
        pErr->Throw();
    }
}

void CSourceModParser::ApplyMods(CBioSource& bsrc, CTempString organism)
{
    CAutoInitRef<CBioSource> pBiosource;
    pBiosource.Set(&bsrc);
    x_ApplyMods(pBiosource, organism);
}


void CSourceModParser::ApplyMods(CMolInfo& mi)
{
    CAutoInitRef<CMolInfo> ref;
    ref.Set(&mi);
    x_ApplyMods(ref);
}


void CSourceModParser::ApplyMods(CGB_block& gbb)
{
    CAutoInitRef<CGB_block> ref;
    ref.Set(&gbb);
    x_ApplyMods(ref);
}


void CSourceModParser::SetAllUnused()
{
    NON_CONST_ITERATE(TMods, it, m_Mods)
    {
        // set iterators are const since changing an object could affect
        // its order in the set.  However, in this case we know that
        // changing the `used` field won't affect the order so we know
        // that a const_cast to change it is safe to do.
        const_cast<SMod&>(*it).used = false;
    }
}

void CSourceModParser::AddMods(const CTempString& name, const CTempString& value, TGroupId group_id)
{
    m_Mods.emplace(name, value, group_id);
    auto ret = m_ModNameMap.emplace(name, SMod(name,value,group_id));
    auto it = ret.first;
    m_ModGroupMap.emplace(it->second.group_id, ref(it->second));
}

void CSourceModParser::AddMod(const pair<string, string>& mod, TGroupId group_id) 
{
    AddMods(mod.first, mod.second, group_id);
}

void CSourceModParser::AddMod(const pair<CTempString, CTempString>& mod, TGroupId group_id) 
{
    AddMods(mod.first, mod.second, group_id);
}

void CSourceModParser::AddMods(const initializer_list<pair<string, string>>& mods, TGroupId group_id)
{
    for (auto mod : mods) {
        AddMod(mod, group_id);
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
