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
#include <objtools/readers/source_mod_parser.hpp>

#include <corelib/ncbiutil.hpp>
#include <util/static_map.hpp>
#include <serial/enumvalues.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

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


string CSourceModParser::ParseTitle(const CTempString& title)
{
    SMod   mod;
    string stripped_title;
    size_t pos = 0;

    m_Mods.clear();

    while (pos < title.size()) {
        size_t lb_pos = title.find('[', pos), eq_pos = title.find('=', lb_pos),
               end_pos = CTempString::npos;
        if (eq_pos != CTempString::npos) {
            mod.key = NStr::TruncateSpaces
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
            mod.value = NStr::TruncateSpaces
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
            CTempString text = NStr::TruncateSpaces
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

            {{
                CAutoInitRef<CGene_ref> gene;
                x_ApplyMods(gene);
                if (&gene.Get(LeaveAsIs<CGene_ref>) != NULL) {
                    CRef<CSeq_feat> feat(new CSeq_feat);
                    feat->SetData().SetGene(*gene);
                    feat->SetLocation().SetWhole(*id);
                    ftable->SetData().SetFtable().push_back(feat);
                }
            }}

            {{
                CAutoInitRef<CProt_ref> prot;
                x_ApplyMods(prot);
                if (&prot.Get(LeaveAsIs<CProt_ref>) != NULL) {
                    CRef<CSeq_feat> feat(new CSeq_feat);
                    feat->SetData().SetProt(*prot);
                    feat->SetLocation().SetWhole(*id);
                    ftable->SetData().SetFtable().push_back(feat);
                }
            }}

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
        CAutoInitRef<CBioSource> bsrc;
        x_ApplyMods(bsrc, organism);
        if (&bsrc.Get(LeaveAsIs<CBioSource>) != NULL) {
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetSource(*bsrc);
            seq.SetDescr().Set().push_back(desc);
        }
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


void CSourceModParser::ApplyMods(CBioseq& seq)
{
    const SMod* mod;

    if ((mod = FindMod("topology", "top")) != NULL) {
        if (NStr::EqualNocase(mod->value, "linear")) {
            seq.SetInst().SetTopology(CSeq_inst::eTopology_linear);
        } else if (NStr::EqualNocase(mod->value, "circular")) {
            seq.SetInst().SetTopology(CSeq_inst::eTopology_circular);
        } else {
            x_HandleBadModValue(*mod);
        }
    }

    // mol[ecule], strand
}


void CSourceModParser::x_ApplyMods(CAutoInitRef<CBioSource>& bsrc,
                                   CTempString organism)
{
    const SMod* mod;

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

    if ((mod = FindMod("location")) != NULL) {
        if (NStr::EqualNocase(mod->value, "mitochondrial")) {
            bsrc->SetGenome(CBioSource::eGenome_mitochondrion);
        } else if (NStr::EqualNocase(mod->value, "provirus")) {
            bsrc->SetGenome(CBioSource::eGenome_proviral);
        } else {
            try {
                bsrc->SetGenome(CBioSource::GetTypeInfo_enum_EGenome()
                                ->FindValue(mod->value));
            } catch (CSerialException&) {
                x_HandleBadModValue(*mod);
            }
        }
    }

    if ((mod = FindMod("origin")) != NULL) {
        try {
            bsrc->SetOrigin(CBioSource::GetTypeInfo_enum_EOrigin()
                            ->FindValue(mod->value));
        } catch (CSerialException&) {
            x_HandleBadModValue(*mod);
        }
    }

    struct SSubtypeAlias {
        const char *name;
        int         value;
    };

    {{
        const CEnumeratedTypeValues* etv = COrgMod::GetTypeInfo_enum_ESubtype();
        ITERATE (CEnumeratedTypeValues::TValues, it, etv->GetValues()) {
            if ((mod = FindMod(it->first)) != NULL) {
                CRef<COrgMod> org_mod(new COrgMod);
                org_mod->SetSubtype(it->second);
                org_mod->SetSubname(mod->value);
                bsrc->SetOrg().SetOrgname().SetMod().push_back(org_mod);
            }
        }
    }}

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

    {{
        const CEnumeratedTypeValues* etv
            = CSubSource::GetTypeInfo_enum_ESubtype();
        ITERATE (CEnumeratedTypeValues::TValues, it, etv->GetValues()) {
            if ((mod = FindMod(it->first)) != NULL) {
                CRef<CSubSource> subsource(new CSubSource);
                subsource->SetSubtype(it->second);
                subsource->SetName(mod->value);
                bsrc->SetSubtype().push_back(subsource);
            }
        }
    }}

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

    // db_xref, div[ision], lineage, [m|p]gcode, note[s], focus
    // delegate to CFeatureTableReader in any cases?
}


typedef pair<const char*, CMolInfo::TBiomol> TBiomolMapEntry;
static const TBiomolMapEntry sc_BiomolArray[] = {
    TBiomolMapEntry("?",               CMolInfo::eBiomol_unknown),
    TBiomolMapEntry("cRNA",            CMolInfo::eBiomol_cRNA),
    TBiomolMapEntry("genomic",         CMolInfo::eBiomol_genomic),
    TBiomolMapEntry("genomic-mRNA",    CMolInfo::eBiomol_genomic_mRNA),
    TBiomolMapEntry("mRNA",            CMolInfo::eBiomol_mRNA),
    TBiomolMapEntry("non-coding RNA",  CMolInfo::eBiomol_ncRNA),
    TBiomolMapEntry("other-genetic",   CMolInfo::eBiomol_other_genetic),
    TBiomolMapEntry("peptide",         CMolInfo::eBiomol_peptide),
    TBiomolMapEntry("precursor RNA",   CMolInfo::eBiomol_pre_RNA),
    TBiomolMapEntry("rRNA",            CMolInfo::eBiomol_rRNA),
    TBiomolMapEntry("scRNA",           CMolInfo::eBiomol_scRNA),
    TBiomolMapEntry("snoRNA",          CMolInfo::eBiomol_snoRNA),
    TBiomolMapEntry("snRNA",           CMolInfo::eBiomol_snRNA),
    TBiomolMapEntry("transcribed-RNA", CMolInfo::eBiomol_transcribed_RNA),
    TBiomolMapEntry("transfer-messenger RNA", CMolInfo::eBiomol_tmRNA),
    TBiomolMapEntry("tRNA",            CMolInfo::eBiomol_tRNA)
};
typedef CStaticArrayMap<const char*, CMolInfo::TBiomol,
                        CSourceModParser::PKeyCompare>  TBiomolMap;
DEFINE_STATIC_ARRAY_MAP(TBiomolMap, sc_BiomolMap, sc_BiomolArray);

void CSourceModParser::x_ApplyMods(CAutoInitRef<CMolInfo>& mi)
{
    const SMod* mod;

    if ((mod = FindMod("moltype", "mol-type")) != NULL) {
        TBiomolMap::const_iterator it = sc_BiomolMap.find(mod->value.c_str());
        if (it == sc_BiomolMap.end()) {
            x_HandleBadModValue(*mod);
        } else {
            mi->SetBiomol(it->second);
        }
    }

    // tech, complete[d]ness
}


void CSourceModParser::x_ApplyMods(CAutoInitRef<CGene_ref>& gene)
{
    const SMod* mod;

    if ((mod = FindMod("gene")) != NULL) {
        gene->SetLocus(mod->value);
    }

    // allele, gene_syn[onym], locus_tag
}


void CSourceModParser::x_ApplyMods(CAutoInitRef<CProt_ref>& prot)
{
    const SMod* mod;

    if ((mod = FindMod("protein", "prot")) != NULL) {
        prot->SetName().push_back(mod->value);
    }

    // prot_desc, EC_number, activity/function
}


void CSourceModParser::x_ApplyMods(CAutoInitRef<CGB_block>& gbb)
{
    const SMod* mod;

    if ((mod = FindMod("secondary-accession", "secondary-accessions")) != NULL)
    {
        list<CTempString> ranges;
        NStr::Split(mod->value, ",", ranges);
        ITERATE (list<CTempString>, it, ranges) {
            string s = NStr::TruncateSpaces(*it);
            try {
                SSeqIdRange range(s);
                ITERATE (SSeqIdRange, it2, range) {
                    gbb->SetExtra_accessions().push_back(*it2);
                }
            } catch (CSeqIdException& e) {
                gbb->SetExtra_accessions().push_back(s);
            }
        }
    }

    // keyword[s]
}


void CSourceModParser::x_ApplyMods(CAutoInitRef<CSeq_hist>& hist)
{
    const SMod* mod;

    if ((mod = FindMod("secondary-accession", "secondary-accessions")) != NULL)
    {
        list<CTempString> ranges;
        NStr::Split(mod->value, ",", ranges);
        ITERATE (list<CTempString>, it, ranges) {
            string s = NStr::TruncateSpaces(*it);
            try {
                SSeqIdRange range(s);
                ITERATE (SSeqIdRange, it2, range) {
                    hist->SetReplaces().SetIds().push_back(it2.GetID());
                }
            } catch (CSeqIdException& e) {
                NStr::ReplaceInPlace(s, "ref_seq|", "ref|", 0, 1);
                hist->SetReplaces().SetIds()
                    .push_back(CRef<CSeq_id>(new CSeq_id(s)));
            }
        }
    }
}


// void CSourceModParser::x_ApplyMods(CAutoInitRef<CSubmit_block>& sb) { }
//   hup


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
    const SMod* mod;

    if ((mod = FindMod("primary", "primary_accessions")) != NULL) {
        CUser_object::TData data;
        list<CTempString> accns;
        NStr::Split(mod->value, ",", accns);
        ITERATE (list<CTempString>, it, accns) {
            CRef<CUser_field> field(new CUser_field), subfield(new CUser_field);
            field->SetLabel().SetId(0);
            subfield->SetLabel().SetStr("accession");
            subfield->SetData().SetStr(*it);
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
    const SMod* mod;

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
    s_ApplyPubMods(sd, FindAllMods("PubMed"));
    s_ApplyPubMods(sd, FindAllMods("PMID"));
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
                                                        CTempString alt_key)
{
    SMod mod;

    for (int tries = 0;  tries < 2;  ++tries) {
        mod.key = tries ? alt_key : key;
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

void CSourceModParser::x_HandleBadModValue(const SMod& mod)
{
    // Ignore per the C Toolkit; other possibilities might be to display
    // a warning or to mark the modifier as unused after all.
}


END_SCOPE(objects)
END_NCBI_SCOPE
