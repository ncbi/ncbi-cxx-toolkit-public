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


template <typename T>
inline
T* LeaveAsIs(void)
{
    return static_cast<T*>(NULL);
}

class CAutoInitAddBioSource: public CAutoInitRef<CBioSource> 
{         
public:
    CAutoInitAddBioSource(CSeq_descr& descr): 
      m_descr(descr), m_need_to_add(true)
      {
          NON_CONST_ITERATE(CSeq_descr::Tdata, desc_it, m_descr.Set())
          {
              if ((**desc_it).IsSource())
              {
                  CBioSource& biosource = (**desc_it).SetSource();
                  Set(&biosource);
                  m_need_to_add = false;
                  break;
              }
          }

      };
      ~CAutoInitAddBioSource()
      {
          if (m_need_to_add && &Get(LeaveAsIs<CBioSource>) != NULL) 
          {
              CRef<CSeqdesc> desc(new CSeqdesc);
              desc->SetSource(Get()); 
              m_descr.Set().push_back(desc);
          }
      }        

private:
    bool         m_need_to_add;
    CSeq_descr&  m_descr;
};


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
    for( ; (pos < title.size()) && (iModsFoundSoFar < iMaxModsToParse); 
        ++iModsFoundSoFar )
    {
        size_t lb_pos = title.find('[', pos), eq_pos = title.find('=', lb_pos),
               end_pos = CTempString::npos;
        if (eq_pos != CTempString::npos) {
            mod.key = NStr::TruncateSpaces_Unsafe
                (title.substr(lb_pos + 1, eq_pos - lb_pos - 1));
            if (eq_pos + 3 < title.size()  &&  title[eq_pos + 1] == '"') {
                end_pos = title.find('"', ++eq_pos + 1);
            } else {
                end_pos = title.find(']', eq_pos + 1);
            }
        }
        if (end_pos == CTempString::npos) {
            stripped_title += title.substr(pos);
            break;
        } else {
            mod.value = NStr::TruncateSpaces_Unsafe
                (title.substr(eq_pos + 1, end_pos - eq_pos - 1));
            if (title[end_pos] == '"') {
                end_pos = title.find(']', end_pos + 1);
                if (end_pos == CTempString::npos) {
                    break;
                }
            }
            mod.pos = lb_pos;
            mod.used = false;
            m_Mods.insert(mod);
            CTempString text = NStr::TruncateSpaces_Unsafe
                (title.substr(pos, lb_pos - pos));
            if ( !stripped_title.empty()  &&  !text.empty() ) {
                stripped_title += ' ';
            }
            stripped_title += text;
            pos = end_pos + 1;
        }
    }

    return stripped_title;
}


void CSourceModParser::ApplyAllMods(CBioseq& seq, CTempString organism)
{
    ApplyMods(seq);
    // Although the logic below reuses some existing objects if
    // present, it always creates new features and descriptors.

    {{
        CRef<CSeq_id> id = FindBestChoice(seq.GetId(), CSeq_id::BestRank);
        if (id) {
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
                if (&gene.Get(LeaveAsIs<CGene_ref>) != NULL) {
                    CRef<CSeq_feat> feat(new CSeq_feat);
                    feat->SetData().SetGene(*gene);
                    feat->SetLocation().SetWhole(*id);
                    ftable->SetData().SetFtable().push_back(feat);
                }
            }

            // only add Prot_ref if amino acid (or at least not nucleic acid)
            // (Yes, the FIELD_CHAIN_OF_2_IS_SET is necessary because IsAa()
            // can throw an exception if mol isn't set)
            if( ! FIELD_CHAIN_OF_2_IS_SET(seq, Inst, Mol) || seq.IsAa() ) {
                CAutoInitRef<CProt_ref> prot;
                x_ApplyMods(prot);
                if ( &prot.Get(LeaveAsIs<CProt_ref>) != NULL ) {
                    CRef<CSeq_feat> feat(new CSeq_feat);
                    feat->SetData().SetProt(*prot);
                    feat->SetLocation().SetWhole(*id);
                    ftable->SetData().SetFtable().push_back(feat);
                }
            }

            if ( !had_ftable  &&  &ftable.Get(LeaveAsIs<CSeq_annot>) != NULL ) {
                seq.SetAnnot().push_back(CRef<CSeq_annot>(&*ftable));
            }
        }
    }}

    if (seq.GetInst().IsSetHist()) {
        ApplyMods(seq.SetInst().SetHist());
    } else {
        CAutoInitRef<CSeq_hist> hist;
        x_ApplyMods(hist);
        if (&hist.Get(LeaveAsIs<CSeq_hist>) != NULL) {
            seq.SetInst().SetHist(*hist);
        }
    }

    {{
        CAutoInitAddBioSource bsrc(seq.SetDescr());
        x_ApplyMods(bsrc, organism);
    }}

    {{
        CAutoInitRef<CMolInfo> mi;
        x_ApplyMods(mi);
        if (&mi.Get(LeaveAsIs<CMolInfo>) != NULL) {
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetMolinfo(*mi);
            seq.SetDescr().Set().push_back(desc);
        }
    }}

    {{
        CAutoInitRef<CGB_block> gbb;
        x_ApplyMods(gbb);
        if (&gbb.Get(LeaveAsIs<CGB_block>) != NULL) {
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetGenbank(*gbb);
            seq.SetDescr().Set().push_back(desc);
        }
    }}

    {{
        CAutoInitRef<CUser_object> tpa;
        x_ApplyTPAMods(tpa);
        if (&tpa.Get(LeaveAsIs<CUser_object>) != NULL) {
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetUser(*tpa);
            seq.SetDescr().Set().push_back(desc);
        }
    }}

    {{
        CAutoInitRef<CUser_object> gpdb;
        x_ApplyGenomeProjectsDBMods(gpdb);
        if (&gpdb.Get(LeaveAsIs<CUser_object>) != NULL) {
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetUser(*gpdb);
            seq.SetDescr().Set().push_back(desc);
        }
    }}

    {{
        CSeq_descr pubs;
        ApplyPubMods(pubs);
        if ( !pubs.Get().empty() ) {
            CSeq_descr::Tdata& sds = seq.SetDescr().Set();
            sds.splice(sds.end(), pubs.Set());
        }
    }}
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
    if ((mod = FindMod("topology", "top")) != NULL) {
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
        if ((mod = FindMod("molecule", "mol")) != NULL) {
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
            if ((mod = FindMod("moltype", "mol-type")) != NULL) {
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
    if ((mod = FindMod("strand")) != NULL) {
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
    if ((mod = FindMod("comment")) != NULL) {
        CRef<CSeqdesc> desc(new CSeqdesc);
        desc->SetComment( mod->value );
        seq.SetDescr().Set().push_back(desc);
    }
}

void CSourceModParser::x_ApplyMods(CAutoInitRef<CBioSource>& bsrc,
                                   CTempString organism)
{
    const SMod* mod = NULL;

    // org[anism]
    if (((mod = FindMod("organism", "org")) != NULL)  &&  organism.empty()) {
        organism = mod->value;
    }
    if ( !organism.empty()
        &&  ( !bsrc->GetOrg().IsSetTaxname()
             ||  !NStr::EqualNocase(bsrc->GetOrg().GetTaxname(), organism))) {
        bsrc->ResetOrg();
        bsrc->ResetSubtype();
        bsrc->SetOrg().SetTaxname(organism);
    }

    // location
    if ((mod = FindMod("location")) != NULL) {
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
    if ((mod = FindMod("origin")) != NULL) {
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

    struct SSubtypeAlias {
        const char *name;
        int         value;
    };

    // handle orgmods
    {{
        // create lookup set for finding discouraged tags
        static const char * kDeprecatedSubtypes_arr[] = {
            "dosage", "old-lineage", "old-name"
        };
        const int kDeprecatedSubtypes_arr_len = sizeof(kDeprecatedSubtypes_arr) / sizeof(kDeprecatedSubtypes_arr[0]);
        const set<const char*, CSourceModParser::PKeyCompare> kDeprecatedSubtypes( 
            kDeprecatedSubtypes_arr, kDeprecatedSubtypes_arr + kDeprecatedSubtypes_arr_len);

        const CEnumeratedTypeValues* etv = COrgMod::GetTypeInfo_enum_ESubtype();
        ITERATE (CEnumeratedTypeValues::TValues, it, etv->GetValues()) {
            if( kDeprecatedSubtypes.find(it->first.c_str()) != kDeprecatedSubtypes.end() ) {
                // skip this bad tag
            } else if ((mod = FindMod(it->first)) != NULL) {
                CRef<COrgMod> org_mod(new COrgMod);
                org_mod->SetSubtype(it->second);
                org_mod->SetSubname(mod->value);
                bsrc->SetOrg().SetOrgname().SetMod().push_back(org_mod);
            }
        }
    }}

    // handle orgmod aliases
    {{
        static const SSubtypeAlias kOrgmodAliases[] = {
            { "subspecies",    COrgMod::eSubtype_sub_species },
            { "host",          COrgMod::eSubtype_nat_host    },
            { "specific-host", COrgMod::eSubtype_nat_host    },
            { NULL,            0                             }
        };
        for (const SSubtypeAlias* it = &kOrgmodAliases[0];
             it->name != NULL;  ++it) {
            if ((mod = FindMod(it->name)) != NULL) {
                CRef<COrgMod> org_mod(new COrgMod);
                org_mod->SetSubtype(it->value);
                org_mod->SetSubname(mod->value);
                bsrc->SetOrg().SetOrgname().SetMod().push_back(org_mod);
            }
        }
    }}

    // handle subsources
    {{
        const CEnumeratedTypeValues* etv
            = CSubSource::GetTypeInfo_enum_ESubtype();
        ITERATE (CEnumeratedTypeValues::TValues, it, etv->GetValues()) {
            // certain tags have to be handled differently
            switch( it->second ) {
                case CSubSource::eSubtype_fwd_primer_seq:
                case CSubSource::eSubtype_rev_primer_seq:
                case CSubSource::eSubtype_fwd_primer_name:
                case CSubSource::eSubtype_rev_primer_name:
                    // skip; we'll handle these below
                    break;
                case CSubSource::eSubtype_transposon_name:
                case CSubSource::eSubtype_plastid_name:
                case CSubSource::eSubtype_insertion_seq_name:
                    // skip these deprecated tags
                    break;
                default:
                    if ((mod = FindMod(it->first)) != NULL) {
                        CRef<CSubSource> subsource(new CSubSource);
                        subsource->SetSubtype(it->second);

                        if( CSubSource::NeedsNoText(it->second) ) {
                            subsource->SetName(kEmptyStr);
                        } else {
                            subsource->SetName(mod->value);
                        }

                        bsrc->SetSubtype().push_back(subsource);
                    }
                    break;
            }
        }
    }}

    // handle PCR Primers
    {{
        CAutoInitRef<CPCRReaction> pcr_reaction;
        
        CAutoInitRef<CPCRPrimer> fwd_primer;
        CAutoInitRef<CPCRPrimer> rev_primer;

        bool used_fwd = false;
        bool used_rev = false;

        if ((mod = FindMod("fwd-primer-name")) != NULL) {
            fwd_primer->SetName().Set( mod->value );
            used_fwd = true;
        }
        if ((mod = FindMod("fwd-primer-seq")) != NULL) {
            fwd_primer->SetSeq().Set( mod->value );
            NStr::ToLower( fwd_primer->SetSeq().Set() );
            used_fwd = true;
        }
        if ((mod = FindMod("rev-primer-name")) != NULL) {
            rev_primer->SetName().Set( mod->value );
            used_rev = true;
        }
        if ((mod = FindMod("rev-primer-seq")) != NULL) {
            rev_primer->SetSeq().Set( mod->value );
            NStr::ToLower( rev_primer->SetSeq().Set() );
            used_rev = true;
        }

        if( used_fwd ) {
            pcr_reaction->SetForward().Set().push_back( CRef<CPCRPrimer>(&*fwd_primer) );
        }
        if( used_rev ) {
            pcr_reaction->SetReverse().Set().push_back( CRef<CPCRPrimer>(&*rev_primer) );
        }
        if( used_fwd || used_rev ) {
            bsrc->SetPcr_primers().Set().push_back( CRef<CPCRReaction>(&*pcr_reaction) );
        }
    }}

    // handle subsource aliases
    {{
        static const SSubtypeAlias kSubsourceAliases[] = {
            { "sub-clone",          CSubSource::eSubtype_subclone },
            { "lat-long",           CSubSource::eSubtype_lat_lon  },
            { "latitude-longitude", CSubSource::eSubtype_lat_lon  },
            { NULL,                 0                          }
        };
        for (const SSubtypeAlias* it = &kSubsourceAliases[0];
             it->name != NULL;  ++it) {
            if ((mod = FindMod(it->name)) != NULL) {
                CRef<CSubSource> subsource(new CSubSource);
                subsource->SetSubtype(it->value);
                subsource->SetName(mod->value);
                bsrc->SetSubtype().push_back(subsource);
            }
        }
    }}

    // db_xref
    TModsRange db_xref_mods_range = FindAllMods( "db_xref" );
    for( TModsCI db_xref_iter = db_xref_mods_range.first; 
            db_xref_iter != db_xref_mods_range.second; 
            ++db_xref_iter ) {
        CRef< CDbtag > new_db( new CDbtag );

        const string &db_xref_str = db_xref_iter->value;
        int colon_location = db_xref_str.find( ":" );
        if( colon_location == string::npos ) {
            // no colon: it's just tag, and db is unknown
            colon_location = -1; // we imagine the colon to be just before the start of the string
            new_db->SetDb( "?" );
        } else {
            // there's a colon, so db and tag are both known
            new_db->SetDb( db_xref_str.substr( 0, colon_location ) );
        }
        
        CRef<CObject_id> object_id( new CObject_id );
        object_id->SetStr( db_xref_str.substr( colon_location + 1 ) );
        new_db->SetTag( *object_id );

        bsrc->SetOrg().SetDb().push_back( new_db );
    }

    // div[ision]
    if ((mod = FindMod("division", "div")) != NULL) {
        bsrc->SetOrg().SetOrgname().SetDiv( mod->value );
    }
    
    // lineage
    if ((mod = FindMod("lineage")) != NULL) {
        bsrc->SetOrg().SetOrgname().SetLineage( mod->value );
    }
    
    // gcode
    if ((mod = FindMod("gcode")) != NULL) {
        bsrc->SetOrg().SetOrgname().SetGcode( NStr::StringToInt(mod->value, NStr::fConvErr_NoThrow) );
    }

    // mgcode
    if ((mod = FindMod("mgcode")) != NULL) {
        bsrc->SetOrg().SetOrgname().SetMgcode( NStr::StringToInt(mod->value, NStr::fConvErr_NoThrow) );
    }

    // pgcode
    if ((mod = FindMod("pgcode")) != NULL) {
        bsrc->SetOrg().SetOrgname().SetPgcode( NStr::StringToInt(mod->value, NStr::fConvErr_NoThrow) );
    }

    // note[s]
    if ((mod = FindMod("note", "notes")) != NULL) {
        CRef< CSubSource > new_subsource( new CSubSource );
        new_subsource->SetSubtype( CSubSource::eSubtype_other );
        new_subsource->SetName( mod->value );
        bsrc->SetSubtype().push_back( new_subsource );
    }

    // focus
    if ((mod = FindMod("focus")) != NULL) {
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
    if ((mod = FindMod("moltype", "mol-type")) != NULL) {
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
    if ((mod = FindMod("tech")) != NULL) {
        TTechMap::const_iterator it = sc_TechMap.find(mod->value.c_str());
        if (it == sc_TechMap.end()) {
            x_HandleBadModValue(*mod);
        } else {
            mi->SetTech(it->second);
        }
    }

    // complete[d]ness
    if ((mod = FindMod("completeness", "completedness")) != NULL) {
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
    if ((mod = FindMod("gene")) != NULL) {
        gene->SetLocus(mod->value);
    }

    // allele
    if ((mod = FindMod("allele")) != NULL) {
        gene->SetAllele( mod->value );
    }

    // gene_syn[onym]
    if ((mod = FindMod("gene_syn", "gene_synonym")) != NULL) {
        gene->SetSyn().push_back( mod->value );
    }
    
    // locus_tag
    if ((mod = FindMod("locus_tag")) != NULL) {
        gene->SetLocus_tag( mod->value );
    }
}


void CSourceModParser::x_ApplyMods(CAutoInitRef<CProt_ref>& prot)
{
    const SMod* mod = NULL;

    // prot[ein]
    if ((mod = FindMod("protein", "prot")) != NULL) {
        prot->SetName().push_back(mod->value);
    }

    // prot_desc
    if ((mod = FindMod("prot_desc")) != NULL) {
        prot->SetDesc( mod->value );
    }
    
    // EC_number 
    if ((mod = FindMod("EC_number")) != NULL) {
        prot->SetEc().push_back( mod->value );
    }

    // activity/function
    if ((mod = FindMod("activity", "function")) != NULL) {
        prot->SetActivity().push_back( mod->value );
    }
}


void CSourceModParser::x_ApplyMods(CAutoInitRef<CGB_block>& gbb)
{
    const SMod* mod = NULL;

    // secondary-accession[s]
    if ((mod = FindMod("secondary-accession", "secondary-accessions")) != NULL)
    {
        list<CTempString> ranges;
        NStr::Split(mod->value, ",", ranges);
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
    if ((mod = FindMod("keyword", "keywords")) != NULL) {
        list<string> keywordList;
        NStr::Split( mod->value, ",;", keywordList );
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
    if ((mod = FindMod("secondary-accession", "secondary-accessions")) != NULL)
    {
        list<CTempString> ranges;
        NStr::Split(mod->value, ",", ranges);
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
    if ((mod = FindMod("primary", "primary-accessions")) != NULL) {
        CUser_object::TData data;
        list<CTempString> accns;
        NStr::Split(mod->value, ",", accns);
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


void
CSourceModParser::x_ApplyGenomeProjectsDBMods(CAutoInitRef<CUser_object>& gpdb)
{
    const SMod* mod = NULL;

    // project[s]
    if ((mod = FindMod("project", "projects")) != NULL) {
        CUser_object::TData data;
        list<CTempString> ids;
        NStr::Split(mod->value, ",;", ids);
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
void s_ApplyPubMods(CSeq_descr& sd, CSourceModParser::TModsRange range)
{
    for (CSourceModParser::TModsCI it = range.first;
         it != range.second;  ++it) {
        int pmid = NStr::StringToInt(it->value, NStr::fConvErr_NoThrow);
        CRef<CSeqdesc> desc(new CSeqdesc);
        CRef<CPub> pub(new CPub);
        pub->SetPmid().Set(pmid);
        desc->SetPub().SetPub().Set().push_back(pub);
        sd.Set().push_back(desc);
    }
}


void CSourceModParser::ApplyPubMods(CSeq_descr& sd)
{
    // find PubMed IDs
    s_ApplyPubMods(sd, FindAllMods("PubMed"));
    s_ApplyPubMods(sd, FindAllMods("PMID"));
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

CSourceModParser::TMods CSourceModParser::GetMods(TWhichMods which) const
{
    if (which == fAllMods) {
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


const CSourceModParser::SMod* CSourceModParser::FindMod(const CTempString& key,
                                                        CTempString alt_key )
{
    // check against m_pModFilter, if any
    if( m_pModFilter ) {
        if( ! (*m_pModFilter)(key) || ! (*m_pModFilter)(alt_key) ) {
            return NULL;
        }
    }

    SMod mod;

    for (int tries = 0;  tries < 2;  ++tries) {
        mod.key = ( tries == 0 ? key : alt_key );
        mod.pos = 0;
        if ( !mod.key.empty() ) {
            TModsCI it = m_Mods.lower_bound(mod);
            if (it != m_Mods.end()  &&  CompareKeys(it->key, mod.key) == 0) {
                const_cast<SMod&>(*it).used = true;
                return &*it;
            }
        }
    }

    return NULL;
}


CSourceModParser::TModsRange
CSourceModParser::FindAllMods(const CTempString& key)
{
    SMod mod;
    mod.key = key;
    mod.pos = 0;

    TModsRange r;
    r.first = m_Mods.lower_bound(mod);
    for (r.second = r.first;
         r.second != m_Mods.end()  &&  CompareKeys(r.second->key, key) == 0;
         ++r.second) {
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
    default:
        _TROUBLE;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
