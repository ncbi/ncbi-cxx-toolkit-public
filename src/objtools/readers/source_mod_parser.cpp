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

#include <objects/general/User_object.hpp>

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

    STATIC_SMOD(biosample);
    STATIC_SMOD(bioproject);
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


template<class _T>
class CAutoInitDesc : protected CAutoAddDesc
{
public:
    CAutoInitDesc(CSeq_descr& descr, CSeqdesc::E_Choice which);
    CAutoInitDesc(CBioseq& bioseq, CSeqdesc::E_Choice which);
    CAutoInitDesc(CBioseq_set& bioset, CSeqdesc::E_Choice which);
    CAutoInitDesc(_T& obj);
    _T* operator->();
    _T& operator*();
protected:
    _T* m_ptr;
    void _getfromdesc();
    mutable CRef<CBioseq> m_bioseq;
    mutable CRef<CBioseq_set> m_bioset;
};

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

CSafeStaticRef<CSeq_descr> fake_descr;

template<class _T>
inline
CAutoInitDesc<_T>::CAutoInitDesc(CSeq_descr& descr, CSeqdesc::E_Choice which) :
  CAutoAddDesc(descr, which), 
  m_ptr(0)
{
}

template<class _T>
inline
CAutoInitDesc<_T>::CAutoInitDesc(CBioseq& bioseq, CSeqdesc::E_Choice which) :
  CAutoAddDesc(*fake_descr, which), 
   m_ptr(0),
   m_bioseq(&bioseq)
{
    m_descr.Reset();
}

template<class _T>
inline
CAutoInitDesc<_T>::CAutoInitDesc(CBioseq_set& bioset, CSeqdesc::E_Choice which) :
  CAutoAddDesc(*fake_descr, which), 
  m_ptr(0),
  m_bioset(&bioset)

{
    m_descr.Reset();
}

template<class _T>
inline
CAutoInitDesc<_T>::CAutoInitDesc(_T& obj):
   CAutoAddDesc(*fake_descr, CSeqdesc::e_not_set), m_ptr(&obj)
{
    m_descr.Reset();
}


template<class _T>
inline
_T& CAutoInitDesc<_T>::operator*()
{
    return * operator->();
}

template<class _T>
inline
_T* CAutoInitDesc<_T>::operator->()
{
    if (m_ptr == 0 &&
        m_which != CSeqdesc::e_not_set)
    {
      if (m_descr.Empty())
      {
        if (!m_bioseq.Empty())
          m_descr = &m_bioseq->SetDescr();
        else
        if (!m_bioset.Empty())
          m_descr = &m_bioset->SetDescr();
      }
      _getfromdesc();
    }

    return m_ptr;
}

template<>
void CAutoInitDesc<CBioSource>::_getfromdesc()
{
    m_ptr = &Set().SetSource();
}

template<>
void CAutoInitDesc<CMolInfo>::_getfromdesc()
{
    m_ptr = &Set().SetMolinfo();
}

template<>
void CAutoInitDesc<CGB_block>::_getfromdesc()
{
    m_ptr = &Set().SetGenbank();
}


string CSourceModParser::ParseTitle(const CTempString& title, 
    CConstRef<CSeq_id> seqid,
    size_t iMaxModsToParse )
{
    SMod   mod;
    string stripped_title;
    size_t pos = 0;

    m_Mods.clear();

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
                mod.used = false;
                m_Mods.emplace(mod);
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

void CSourceModParser::ApplyAllMods(CBioseq& seq, CTempString organism, CConstRef<CSeq_loc> location)
{
    ApplyMods(seq);
    // Although the logic below reuses some existing objects if
    // present, it always creates new features and descriptors.

    {{
        CRef<CSeq_id> best_id = FindBestChoice(seq.GetId(), CSeq_id::BestRank);
        if (location.Empty() && !best_id.Empty())
        {
            CRef<CSeq_loc> loc(new CSeq_loc);
            loc->SetWhole(*best_id);
            location = loc;
        }

        if (location) 
        {
            CAutoInitRef<CSeq_annot> ftable;
            bool                     had_ftable = false;

            if (seq.IsSetAnnot()) {
                NON_CONST_ITERATE (CBioseq::TAnnot, it, seq.SetAnnot()) {
                    if ((*it)->GetData().IsFtable()) {
                        ftable.Set(*it);
                        had_ftable = true;
                        break;
                    }
                }
            }

            // CGene_ref only on nucleotide seqs
            if( ! FIELD_CHAIN_OF_2_IS_SET(seq, Inst, Mol) || seq.IsNa() ) {
                CAutoInitRef<CGene_ref> gene;
                x_ApplyMods(gene);
                if (gene.IsInitialized()) {
                    CRef<CSeq_feat> feat(new CSeq_feat);
                    feat->SetData().SetGene(*gene);
                    feat->SetLocation().Assign(*location);
                    ftable->SetData().SetFtable().push_back(feat);
                }
            }

            // only add Prot_ref if amino acid (or at least not nucleic acid)
            // (Yes, the FIELD_CHAIN_OF_2_IS_SET is necessary because IsAa()
            // can throw an exception if mol isn't set)
            if( ! FIELD_CHAIN_OF_2_IS_SET(seq, Inst, Mol) || seq.IsAa() ) {
                CAutoInitRef<CProt_ref> prot;
                x_ApplyMods(prot);
                if ( prot.IsInitialized() ) {
                    CRef<CSeq_feat> feat(new CSeq_feat);
                    feat->SetData().SetProt(*prot);
                    feat->SetLocation().Assign(*location);
                    ftable->SetData().SetFtable().push_back(feat);
                }
            }

            if ( !had_ftable  &&  ftable.IsInitialized() ) {
                seq.SetAnnot().push_back(CRef<CSeq_annot>(&*ftable));
            }
        }
    }}

    if (seq.GetInst().IsSetHist()) {
        ApplyMods(seq.SetInst().SetHist());
    } else {
        CAutoInitRef<CSeq_hist> hist;
        x_ApplyMods(hist);
        if (hist.IsInitialized()) {
            seq.SetInst().SetHist(*hist);
        }
    }

    {{
        //CSeq_descr* descr = 0;
        if (
          seq.GetParentSet() && seq.GetParentSet()->IsSetClass() &&
          seq.GetParentSet()->GetClass() == CBioseq_set::eClass_nuc_prot)
        {
            CBioseq_set& bioset = *(CBioseq_set*)(seq.GetParentSet().GetPointerOrNull());
            //descr = &bioset.SetDescr();
            CAutoInitDesc<CBioSource> bsrc(bioset, CSeqdesc::e_Source);
            x_ApplyMods(bsrc, organism);
        }
        else
        {
          //descr = &seq.SetDescr();
            CAutoInitDesc<CBioSource> bsrc(seq, CSeqdesc::e_Source);
            x_ApplyMods(bsrc, organism);
        }
        //CAutoInitDesc<CBioSource> bsrc(*descr, CSeqdesc::e_Source);
        //x_ApplyMods(bsrc, organism);
    }}

    {{
        CAutoInitDesc<CMolInfo> mi(seq, CSeqdesc::e_Molinfo);
        x_ApplyMods(mi);
    }}

    {{
        CAutoInitDesc<CGB_block> gbb(seq, CSeqdesc::e_Genbank);
        x_ApplyMods(gbb);
    }}

    {{
        CAutoInitRef<CUser_object> tpa;
        x_ApplyTPAMods(tpa);
        if (tpa.IsInitialized()) {
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetUser(*tpa);
            seq.SetDescr().Set().push_back(desc);
        }
    }}

    x_ApplyDBLinkMods(seq);

    {{
        CAutoInitRef<CUser_object> gpdb;
        x_ApplyGenomeProjectsDBMods(gpdb);
        if (gpdb.IsInitialized()) {
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetUser(*gpdb);
            seq.SetDescr().Set().push_back(desc);
        }
    }}

    {{
        ApplyPubMods(seq);
    }}

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

void CSourceModParser::ApplyMods(CBioseq& seq)
{
    const SMod* mod = NULL;

    // top[ology]
    if ((mod = FindMod(s_Mod_topology, s_Mod_top)) != NULL) {
        if (NStr::EqualNocase(mod->value, "linear")) {
            seq.SetInst().SetTopology(CSeq_inst::eTopology_linear);
        } else if (NStr::EqualNocase(mod->value, "circular")) {
            seq.SetInst().SetTopology(CSeq_inst::eTopology_circular);
        } else {
            x_HandleBadModValue(*mod);
        }
    }

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

    // comment
    if ((mod = FindMod(s_Mod_comment)) != NULL) {
        CRef<CSeqdesc> desc(new CSeqdesc);
        desc->SetComment( mod->value );
        seq.SetDescr().Set().push_back(desc);
    }
}


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


void CSourceModParser::x_ApplyMods(CAutoInitDesc<CBioSource>& bsrc,
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
            reset_taxid = true;
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
                bsrc->SetGenome(CBioSource::GetTypeInfo_enum_EGenome()
                                ->FindValue(mod->value));
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

    // handle orgmods
    for(const auto & smod_orgsubtype : kSModOrgSubtypeMap.Get()) {
        const SMod & smod = smod_orgsubtype.first;
        const COrgMod::ESubtype e_subtype = smod_orgsubtype.second;
        if ((mod = FindMod(smod.key)) != NULL) {
            CRef<COrgMod> org_mod(new COrgMod);
            org_mod->SetSubtype(e_subtype);
            org_mod->SetSubname(mod->value);
            bsrc->SetOrg().SetOrgname().SetMod().push_back(org_mod);
            reset_taxid = true;
        }
    }

    // handle subsources
    for( const auto & smod_subsrcsubtype : kSModSubSrcSubtypeMap.Get() ) {
        const SMod & smod = smod_subsrcsubtype.first;
        const CSubSource::ESubtype e_subtype = smod_subsrcsubtype.second;
        if ((mod = FindMod(smod.key)) != NULL) {
            auto& subtype = bsrc->SetSubtype();
            CRef<CSubSource> subsource(new CSubSource);
            subsource->SetSubtype(e_subtype);

            if( CSubSource::NeedsNoText(e_subtype) ) {
                subsource->SetName(kEmptyStr);
            } else {
                subsource->SetName(mod->value);
            }

            if (!CSubSource::IsMultipleValuesAllowed(e_subtype))
            {
                // since only one of this e_subtype is allowed, we erase any
                // that are already in the subtype list.
                // (Unfortunately, we cannot just use bsrc->RemoveSubSource
                // because it will ResetSubtype if subtype ends up empty)
                subtype.erase(
                    remove_if(subtype.begin(), subtype.end(),
                              equal_subtype(e_subtype)),
                    subtype.end());
            }

            subtype.push_back(subsource);
        }
    }

    // handle PCR Primers
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


    // db_xref
    TModsRange db_xref_mods_range = FindAllMods( s_Mod_db_xref, s_Mod_dbxref );
    for( TModsCI db_xref_iter = db_xref_mods_range.first; 
            db_xref_iter != db_xref_mods_range.second; 
            ++db_xref_iter ) {
        CRef< CDbtag > new_db( new CDbtag );

        const CTempString db_xref_str = db_xref_iter->value;
        CRef<CObject_id> object_id(new CObject_id);

        size_t colon_location = db_xref_str.find(":");
        if (colon_location == string::npos) {
            // no colon: it's just tag, and db is unknown
            new_db->SetDb() = "?";
            db_xref_str.Copy(object_id->SetStr(), 0, CTempString::npos);
        } else {
            // there's a colon, so db and tag are both known
            db_xref_str.Copy(new_db->SetDb(), 0, colon_location);
            db_xref_str.Copy(object_id->SetStr(), colon_location + 1, CTempString::npos);
        }
        
        new_db->SetTag( *object_id );

        bsrc->SetOrg().SetDb().push_back( new_db );
    }

    // div[ision]
    if ((mod = FindMod(s_Mod_division, s_Mod_div)) != NULL) {
        bsrc->SetOrg().SetOrgname().SetDiv( mod->value );
    }
    
    // lineage
    if ((mod = FindMod(s_Mod_lineage)) != NULL) {
        bsrc->SetOrg().SetOrgname().SetLineage( mod->value );
    }
    
    // gcode
    if ((mod = FindMod(s_Mod_gcode)) != NULL) {
        bsrc->SetOrg().SetOrgname().SetGcode( NStr::StringToInt(mod->value, NStr::fConvErr_NoThrow) );
    }

    // mgcode
    if ((mod = FindMod(s_Mod_mgcode)) != NULL) {
        bsrc->SetOrg().SetOrgname().SetMgcode( NStr::StringToInt(mod->value, NStr::fConvErr_NoThrow) );
    }

    // pgcode
    if ((mod = FindMod(s_Mod_pgcode)) != NULL) {
        bsrc->SetOrg().SetOrgname().SetPgcode( NStr::StringToInt(mod->value, NStr::fConvErr_NoThrow) );
    }

    // note[s]
    TModsRange mods[2];
    mods[0] = FindAllMods(s_Mod_note);
    mods[1] = FindAllMods(s_Mod_notes);
    for (size_t i = 0; i < 2; i++)
    {
        for (TModsCI it = mods[i].first; it != mods[i].second; it++)
        {
            CRef< CSubSource > new_subsource(new CSubSource);
            new_subsource->SetSubtype(CSubSource::eSubtype_other);
            new_subsource->SetName(it->value);
            bsrc->SetSubtype().push_back(new_subsource);
        }
    }

    // focus
    if ((mod = FindMod(s_Mod_focus)) != NULL) {
        if( NStr::EqualNocase( mod->value, "TRUE" ) ) {
            bsrc->SetIs_focus();
        }
    }


    if ((mod = FindMod(s_Mod_taxid)) != NULL) {
        bsrc->SetOrg().SetTaxId( NStr::StringToNumeric<TTaxId>(mod->value, NStr::fConvErr_NoThrow) );
    }
    else 
    if (reset_taxid && bsrc->IsSetOrgname() && bsrc->GetOrg().GetTaxId() != ZERO_TAX_ID) {
       bsrc->SetOrg().SetTaxId(ZERO_TAX_ID);
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

void CSourceModParser::x_ApplyMods(CAutoInitDesc<CMolInfo>& mi)
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


void CSourceModParser::x_ApplyMods(CAutoInitDesc<CGB_block>& gbb)
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


static CRef<CSeqdesc> s_SetDBLinkDesc(CBioseq& bioseq) 
{
    CConstRef<CBioseq_set> pParentSet = bioseq.GetParentSet();
    CSeq_descr& descriptors = (pParentSet &&
                               pParentSet->GetClass() == CBioseq_set::eClass_nuc_prot) ?

        (const_cast<CBioseq_set&>(*pParentSet)).SetDescr() :
        bioseq.SetDescr();


    for (auto pDesc : descriptors.Set()) {
        if (pDesc->IsUser() && pDesc->GetUser().IsDBLink()) {
            return pDesc;
        }
    }

    auto pDBLinkDesc = Ref(new CSeqdesc());
    pDBLinkDesc->SetUser().SetObjectType(CUser_object::eObjectType_DBLink);
    descriptors.Set().push_back(pDBLinkDesc);
    return pDBLinkDesc;
}


static void s_SetDBLinkFieldVals(const string& label, 
                                const list<CTempString>& vals,
                                CSeqdesc& dblink_desc)
{
    if (vals.empty()) {
        return;
    }

    auto& user_obj = dblink_desc.SetUser();
    CRef<CUser_field> pField;
    if (user_obj.IsSetData()) {
        for (auto pUserField : user_obj.SetData()) {
            if (pUserField->IsSetLabel() &&
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
        user_obj.SetData().push_back(pField);
    }

    pField->SetData().SetStrs().clear(); // RW-518 - clear any preexisting entries
    for (const auto& val : vals) {
        pField->SetData().SetStrs().push_back(val);
    }
    pField->SetNum(pField->GetData().GetStrs().size());
}


static void s_SetDBLinkField(const string& label,
                             const string& vals,
                             CRef<CSeqdesc>& pDBLinkDesc,
                             CBioseq& bioseq) 
{
    list<CTempString> value_list;
    NStr::Split(vals, ",", value_list, NStr::fSplit_MergeDelimiters);
    for (auto& val : value_list) {
        val = NStr::TruncateSpaces_Unsafe(val);
    }
    value_list.remove_if([](const CTempString& val){ return val.empty(); });
    if (value_list.empty()) { // nothing to do
        return;
    }

    if (!pDBLinkDesc) {
        pDBLinkDesc =  s_SetDBLinkDesc(bioseq);
    }

    s_SetDBLinkFieldVals(label,
                         value_list,
                         *pDBLinkDesc);
}


void CSourceModParser::x_ApplyDBLinkMods(CBioseq& bioseq) 
{
    CRef<CSeqdesc> pDBLinkDesc;
    const SMod* mod = NULL;
    if ((mod = FindMod(s_Mod_SRA)) != NULL) {
        s_SetDBLinkField("Sequence Read Archive", mod->value, pDBLinkDesc, bioseq);
    }

    if ((mod = FindMod(s_Mod_bioproject)) != NULL) {
        s_SetDBLinkField("BioProject", mod->value, pDBLinkDesc, bioseq);
    }

    if ((mod = FindMod(s_Mod_biosample)) != NULL) {
        s_SetDBLinkField("BioSample", mod->value, pDBLinkDesc, bioseq);
    }
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
        TEntrezId pmid = NStr::StringToNumeric<TEntrezId>(it->value, NStr::fConvErr_NoThrow);
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
    if (which == fAllMods) {
        // if caller gave this they probably should prefer calling GetAllMods
        // to avoid the struct copy.
        return m_Mods;
    } else {
        TMods ret;

        ITERATE (TMods, it, m_Mods) {
            if (which == (it->used ? fUsedMods : fUnusedMods)) {
                ret.insert(ret.end(), *it);
            }
        }

        return ret;
    }
}

const CSourceModParser::SMod* CSourceModParser::FindMod(
    //const SMod & smod, const SMod & alt_smod)
    const CTempString& key, const CTempString& alt_key)
{
    // check against m_pModFilter, if any
    if( m_pModFilter ) {
        if( ! (*m_pModFilter)(key) || ! (*m_pModFilter)(alt_key) ) {
            return NULL;
        }
    }

    SMod mod;

    for (int tries = 0;  tries < 2;  ++tries) {
        const CTempString & modkey = ( tries == 0 ? key : alt_key );
        if( modkey.empty() ) {
            continue;
        }
        mod.key = modkey;

        TModsCI it = m_Mods.lower_bound(mod);
        if (it != m_Mods.end()  &&  EqualKeys(it->key, modkey)) {
            // set iterators are const since changing an object could affect
            // its order in the set.  However, in this case we know that
            // changing the `used` field won't affect the order so we know
            // that a const_cast to change it is safe to do.
            const_cast<SMod&>(*it).used = true;
            return &*it;
        }
    }

    return NULL;
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

    ITERATE (TMods, it, m_Mods) {
        if ((which & (it->used ? fUsedMods : fUnusedMods)) != 0) {
            *s += delim + '[' + it->key + '=' + it->value + ']';
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
    CAutoInitDesc<CBioSource> ref(bsrc);
    x_ApplyMods(ref, organism);
}


void CSourceModParser::ApplyMods(CMolInfo& mi)
{
    CAutoInitDesc<CMolInfo> ref(mi);
    x_ApplyMods(ref);
}


void CSourceModParser::ApplyMods(CGB_block& gbb)
{
    CAutoInitDesc<CGB_block> ref(gbb);
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

void CSourceModParser::AddMods(const CTempString& name, const CTempString& value)
{
    SMod newmod(NStr::TruncateSpaces_Unsafe(name));
    newmod.value = NStr::TruncateSpaces_Unsafe(value);
    newmod.used = false;

    m_Mods.insert(newmod);
}

END_SCOPE(objects)
END_NCBI_SCOPE
