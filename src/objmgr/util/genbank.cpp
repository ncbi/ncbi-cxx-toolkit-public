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
* Author:  Aaron Ucko
*
* File Description:
*   Code to write Genbank/Genpept flat-file records.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/09/25 20:12:06  ucko
* More cleanups from Denis.
* Put utility code in the objects namespace.
* Moved utility code to {src,include}/objects/util (to become libxobjutil).
* Moved static members of CGenbankWriter to above their first use.
*
* ---------------------------------------------------------------------------
* old log:
* Revision 1.4  2001/09/24 14:37:55  ucko
* Comment out names of unused args to WriteXxx.
*
* Revision 1.3  2001/09/21 22:39:07  ucko
* Fix MSVC build.
*
* Revision 1.2  2001/09/05 14:44:59  ucko
* Use NStr::IntToString instead of Stringify.
*
* Revision 1.1  2001/09/04 16:20:53  ucko
* Dramatically fleshed out id1_fetch
*
* ===========================================================================
*/

#include <objects/util/genbank.hpp>

#include <algorithm>

#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>
#include <serial/serialimpl.hpp>
#include <serial/stltypes.hpp>

#include <objects/biblio/Affil.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Id_pat.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Heterogen.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/PIR_block.hpp>
#include <objects/seqblock/PRF_block.hpp>
#include <objects/seqblock/SP_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Rsite_ref.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Txinit.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/util/asciiseqdata.hpp>


#ifdef NCBI_COMPILER_WORKSHOP // workaround for compiler bug
#define BREAK(it) while (it) { ++(it); }  break
#else
#define BREAK(it) break
#endif


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


typedef CConstRef<CSeq_descr> TDescRef;

class CDescList {
public:
    typedef list<TDescRef> TData;
    
    CDescList& Add(const TDescRef& ref)
        { m_Data.push_front(ref);  return *this; }
    DECLARE_INTERNAL_TYPE_INFO();

private:
    TData m_Data;
};


BEGIN_CLASS_INFO(CDescList)
{
    ADD_MEMBER(m_Data, STL_list, (STL_CConstRef, (CLASS, (CSeq_descr))));
}
END_CLASS_INFO


static string s_Pad(const string& s, SIZE_TYPE width)
{
    if (s.size() >= width) {
        return s;
    } else {
        return s + string(width - s.size(), ' ');
    }
}


const unsigned int CGenbankWriter::sm_KeywordWidth = 12;
const unsigned int CGenbankWriter::sm_LineWidth = 80;
const unsigned int CGenbankWriter::sm_DataWidth
= CGenbankWriter::sm_LineWidth - CGenbankWriter::sm_KeywordWidth;
const unsigned int CGenbankWriter::sm_FeatureNameIndent = 5;
const unsigned int CGenbankWriter::sm_FeatureNameWidth = 16;


bool CGenbankWriter::Write(CSeq_entry& entry)
{
    return Write(entry, CDescList());
}


bool CGenbankWriter::Write(const CSeq_entry& entry, const CDescList& descs)
{
    typedef bool (CGenbankWriter::*TFun)(const CBioseq&, const CDescList&);
    static const TFun funlist[]
        = { &CGenbankWriter::WriteLocus,
            &CGenbankWriter::WriteDefinition,
            &CGenbankWriter::WriteAccession,
            &CGenbankWriter::WriteVersion,
            &CGenbankWriter::WriteID,
            &CGenbankWriter::WriteKeywords,
            &CGenbankWriter::WriteSegment,
            &CGenbankWriter::WriteSource,
            &CGenbankWriter::WriteReference,
            &CGenbankWriter::WriteComment,
            &CGenbankWriter::WriteFeatures,
            &CGenbankWriter::WriteSequence,
            NULL};

    CDescList new_descs(descs);

    if (entry.IsSet()) {
        const CBioseq_set& s = entry.GetSet();
        if (s.IsSetDescr()) {
            new_descs.Add(TDescRef(&s.GetDescr()));
        }
        iterate(CBioseq_set::TSeq_set, it, s.GetSeq_set()) {
            if (!Write(**it, new_descs)) {
                return false;
            }
        }
        return true;
    } else {
        const CBioseq& seq = entry.GetSeq();
        if (seq.GetInst().GetRepr() == CSeq_inst::eRepr_seg) {
            return true; // don't bother with dummy entry for whole sequence
        }

        {{
            bool type_okay = false;
            switch (seq.GetInst().GetMol()) {
            case CSeq_inst::eMol_dna:
            case CSeq_inst::eMol_rna:
            case CSeq_inst::eMol_na:
                if (m_Format == eFormat_Genbank)
                    type_okay = true;
                break;
            case CSeq_inst::eMol_aa:
                if (m_Format == eFormat_Genpept)
                    type_okay = true;
                break;
            default:
                break; // stays false
            }
            if (!type_okay) {
                return true; // succeeded, albeit with no output
            }
        }}

        if (seq.IsSetDescr()) {
            new_descs.Add(TDescRef(&seq.GetDescr()));
        }
        for (unsigned int i = 0;  funlist[i];  ++i) {
            if (!(this->*funlist[i])(seq, new_descs)) {
                return false;
            }
        }
        m_Stream << "//" << NcbiEndl;
        return true;
    }
}


void CGenbankWriter::SetParameters(void)
{
    switch (m_Version) {
    case eVersion_pre127:
        m_LocusNameWidth = 9;
        m_SequenceLenWidth = 7;
        m_MoleculeTypeWidth = 5;
        m_TopologyWidth = 9;
        m_DivisionWidth = 9;
        m_GenpeptBlanks = 20; // 33-52
        break;
    case eVersion_127:
        m_LocusNameWidth = 16;
        m_SequenceLenWidth = 11;
        m_MoleculeTypeWidth = 7;
        m_TopologyWidth = 8;
        m_DivisionWidth = 3;
        m_GenpeptBlanks = 21; // 44-64
        break;
    }
    switch (m_Format) {
    case eFormat_Genbank:
        m_LengthUnit = "bases";
        break;
    case eFormat_Genpept:
        m_LengthUnit = "residues";
        break;
    }
}


static string s_FormatDate(const CDate& date) {
    if (date.IsStr()) {
        return date.GetStr();
    }

    // otherwise standard
    const CDate_std& std = date.GetStd();

    static const char kMonthNames[13][4]
        = {"000", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG",
           "SEP", "OCT", "NOV", "DEC" };
    CNcbiOstrstream oss;

    oss << setfill('0') << setw(2) << std.GetDay() << '-'
        << kMonthNames[std.GetMonth()] << '-' << setw(4) << std.GetYear();

    string result(oss.str(), oss.pcount());
    oss.freeze(0);
    return result;
}


static string s_StripDot(const string& s) {
    // removes a trailing period if present
    if (s.empty()  ||  s[s.size() - 1] != '.')
        return s;
    else
        return s.substr(0, s.size() - 1);
}


bool CGenbankWriter::WriteLocus(const CBioseq& seq, const CDescList& descs)
{
    const CSeq_inst& inst = seq.GetInst();

    {{
        string name;
        for (CTypeConstIterator<CTextseq_id> it = ConstBegin(seq);
             it;  ++it) {
            if (it->IsSetName()) {
                name = it->GetName();
            } else {
                name = it->GetAccession();
            }
            BREAK(it);
        }
        m_Stream << s_Pad("LOCUS", sm_KeywordWidth)
                 << s_Pad(name, m_LocusNameWidth) << ' '
                 << setw(m_SequenceLenWidth) << inst.GetLength() << ' ';
    }}
    switch (m_Format) {
    case eFormat_Genbank:
        m_Stream << "bp ";
        {{
            CSeq_inst::TStrand strand = CSeq_inst::eStrand_not_set;
            if (inst.IsSetStrand()) {
                strand = inst.GetStrand();
            }
#ifdef SUPPLY_DEFAULT_STRAND_TYPE
            if (strand == CSeq_inst::eStrand_not_set) {
                if (inst.GetMol() == CSeq_inst::eMol_dna) {
                    strand = CSeq_inst::eStrand_ds;
                } else {
                    strand = CSeq_inst::eStrand_ss;
                }
            }
#endif
            switch (strand) {
            case CSeq_inst::eStrand_ss:     m_Stream << "ss-";  break;
            case CSeq_inst::eStrand_ds:     m_Stream << "ds-";  break;
            case CSeq_inst::eStrand_mixed:  m_Stream << "ms-";  break;
            default:                        m_Stream << "   ";  break;
            }
        }}                

        {{
            const char* moltype = "";
            switch (inst.GetMol()) {
            case CSeq_inst::eMol_dna:
                moltype = "DNA";
                break;
            case CSeq_inst::eMol_rna:
                for (CTypeConstIterator<CMolInfo> it=ConstBegin(seq);
                     it; ++it) {
                    switch (it->GetBiomol()) {
                    case CMolInfo::eBiomol_tRNA:
                        moltype = "tRNA";
                        break;
                    case CMolInfo::eBiomol_rRNA:
                        moltype = "rRNA";
                        break;
                    case CMolInfo::eBiomol_mRNA:
                    case CMolInfo::eBiomol_genomic_mRNA: // ?
                        moltype = "mRNA";
                        break;
                    case CMolInfo::eBiomol_snRNA:
                        moltype = "snRNA";
                        break;
                    case CMolInfo::eBiomol_scRNA:
                        moltype = "scRNA";
                        break;
                    case CMolInfo::eBiomol_snoRNA:
                        if (m_Version >= eVersion_127) {
                            moltype = "snoRNA";
                            break;
                        } // otherwise fall through
                    default: // Why do we never yield "uRNA"?
                        moltype = "RNA";
                        break;
                    }
                    break;
                }
                break;
            default: // stays blank
                break;
            }
            m_Stream << s_Pad(moltype, m_MoleculeTypeWidth) << ' ';
        }}

        {{
            const char* topology = "";
            switch (inst.GetTopology()) {
            case CSeq_inst::eTopology_circular:
                topology = "circular";
                break;
            case CSeq_inst::eTopology_linear:
                if (m_Version >= eVersion_127) {
                    topology = "linear";
                    break;
                } // otherwise fall through
            default: // stays blank; what about tandem?
                break;
            }
            m_Stream << s_Pad(topology, m_TopologyWidth) << ' ';
        }}
        break;

    case eFormat_Genpept:
        m_Stream << s_Pad("aa", m_GenpeptBlanks + 2);
        break;
    }

    {{
        string division;
        CTypesConstIterator it;
        Type<COrgName>::AddTo(it);
        Type<CGB_block>::AddTo(it);
        // Deal with translating EMBL divisions?

        for (it = ConstBegin(descs);  it;  ++it) {
            if (Type<COrgName>::Match(it)) {
                const COrgName* info = Type<COrgName>::Get(it);
                if (info->IsSetDiv()) {
                    division = info->GetDiv();
                    continue; // in hopes of finding a GB_block
                }
            } else if (Type<CGB_block>::Match(it)) {
                const CGB_block* info = Type<CGB_block>::Get(it);
                if (info->IsSetDiv()) {
                    division = info->GetDiv();
                    BREAK(it);
                }
            }
        }
        m_Stream << s_Pad(division, m_DivisionWidth) << ' ';
    }}

    {{
        const CDate* date = NULL;
        for (CTypeConstIterator<CSeqdesc> it = ConstBegin(descs);  it;  ++it) {
            if (it->IsUpdate_date()) {
                date = &it->GetUpdate_date();
                BREAK(it);
            }
        }
        if (date) {
            m_Stream << s_FormatDate(*date);
        }
    }}
    m_Stream << NcbiEndl; // Finally, the end of the LOCUS line!

    return true;
}


bool CGenbankWriter::WriteDefinition(const CBioseq& /* seq */,
                                     const CDescList& descs)
{
    string definition;
    for (CTypeConstIterator<CSeqdesc> it = ConstBegin(descs);  it;  ++it) {
        if (it->IsTitle()) {
            definition = it->GetTitle();
            break;
        }
    }
                
    if (definition.empty()  ||  definition[definition.size()-1] != '.') {
        definition += '.';
    }
    Wrap("DEFINITION", definition);
    return true;
}


bool CGenbankWriter::WriteAccession(const CBioseq& seq,
                                    const CDescList& /* descs */)
{
    string accessions;
    for (CTypeConstIterator<CTextseq_id> it = ConstBegin(seq);  it;  ++it) {
        accessions = it->GetAccession();
        BREAK(it);
    }

    CTypesConstIterator it;
    Type<CEMBL_block>::AddTo(it);
    Type<CSP_block>::AddTo(it);
    Type<CGB_block>::AddTo(it);
    for (it = ConstBegin(seq);  it;  ++it) {
        if (Type<CEMBL_block>::Match(it)) {
            const CEMBL_block::TExtra_acc& accs
                = Type<CEMBL_block>::Get(it)->GetExtra_acc();
            iterate (CEMBL_block::TExtra_acc, acc, accs) {
                accessions += ' ' + *acc;
            }
        } else if (Type<CSP_block>::Match(it)) {
            const CSP_block::TExtra_acc& accs
                = Type<CSP_block>::Get(it)->GetExtra_acc();
            iterate (CSP_block::TExtra_acc, acc, accs) {
                accessions += ' ' + *acc;
            }
        } else if (Type<CGB_block>::Match(it)) {
            const CGB_block::TExtra_accessions& accs
                = Type<CGB_block>::Get(it)->GetExtra_accessions();
            iterate (CGB_block::TExtra_accessions, acc, accs) {
                accessions += ' ' + *acc;
            }
        }
    }
    Wrap("ACCESSION", accessions);
    return true;
}


bool CGenbankWriter::WriteVersion(const CBioseq& seq,
                                  const CDescList& /* descs */)
{
    string accession;
    int version;
    for (CTypeConstIterator<CTextseq_id> it = ConstBegin(seq);  it;  ++it) {
        accession = it->GetAccession();
        version = it->GetVersion();
        BREAK(it);
    }

    int gi = 0;
    for (CTypeConstIterator<CSeq_id> it = ConstBegin(seq);  it;  ++it) {
        if (it->IsGi()) {
            gi = it->GetGi();
            BREAK(it);
        }
    }
    m_Stream << s_Pad("VERSION", sm_KeywordWidth) << accession
             << '.' << version << "  GI:" << gi << NcbiEndl;
    return true;
}


bool CGenbankWriter::WriteID(const CBioseq& seq, const CDescList& /* descs */)
{
    if (m_Format == eFormat_Genbank) {
        return true;
    }

    for (CTypeConstIterator<CSeq_id> it = ConstBegin(seq);  it;  ++it) {
        if (it->IsGi()) {
            m_Stream << s_Pad("PID", sm_KeywordWidth) << 'g' << it->GetGi()
                     << NcbiEndl;
            BREAK(it);
        }
    }
    return true;
}


bool CGenbankWriter::WriteKeywords(const CBioseq& /* seq */,
                                   const CDescList& descs)
{
    vector<string> keywords;
    CTypesConstIterator it;
    Type<CEMBL_block>::AddTo(it);
    Type<CSP_block>::AddTo(it);
    Type<CPIR_block>::AddTo(it);
    Type<CGB_block>::AddTo(it);
    Type<CPRF_block>::AddTo(it);
    for (it = ConstBegin(descs);  it;  ++it) {
        if (Type<CEMBL_block>::Match(it)) {
            const CEMBL_block::TKeywords& kw
                = Type<CEMBL_block>::Get(it)->GetKeywords();
            iterate (CEMBL_block::TKeywords, k, kw) {
                keywords.push_back(*k);
            }
        } else if (Type<CSP_block>::Match(it)) {
            const CSP_block::TKeywords& kw
                = Type<CSP_block>::Get(it)->GetKeywords();
            iterate (CSP_block::TKeywords, k, kw) {
                keywords.push_back(*k);
            }
        } else if (Type<CPIR_block>::Match(it)) {
            const CPIR_block::TKeywords& kw
                = Type<CPIR_block>::Get(it)->GetKeywords();
            iterate (CPIR_block::TKeywords, k, kw) {
                keywords.push_back(*k);
            }
        } else if (Type<CGB_block>::Match(it)) {
            const CGB_block::TKeywords& kw
                = Type<CGB_block>::Get(it)->GetKeywords();
            iterate (CGB_block::TKeywords, k, kw) {
                keywords.push_back(*k);
            }
        } else if (Type<CPRF_block>::Match(it)) {
            const CPRF_block::TKeywords& kw
                = Type<CPRF_block>::Get(it)->GetKeywords();
            iterate (CPRF_block::TKeywords, k, kw) {
                keywords.push_back(*k);
            }
        }
    }

    // Wrap manually, because splitting within a keyword is bad.
    SIZE_TYPE space_left = sm_DataWidth;
    bool at_start = true;
    for (unsigned int n = 0;  n < keywords.size();  ++n) {
        if (n == 0) {
            m_Stream << s_Pad("KEYWORDS", sm_KeywordWidth);
        } else if (keywords[n].size() >= space_left  &&  !at_start) {
            // Don't let long keywords cause infinite loops.
            m_Stream << NcbiEndl << s_Pad("", sm_KeywordWidth);
            space_left = sm_DataWidth;
            at_start = true;
        } else {
            m_Stream << ' ';
        }
        m_Stream << keywords[n];
        if (n == keywords.size() - 1) {
            m_Stream << '.' << NcbiEndl;
        } else {
            m_Stream << ';';
            space_left -= keywords[n].size() + 2; // 2 for "; "
            at_start = false;
        }
    }
    return true;
}


bool CGenbankWriter::WriteSegment(const CBioseq& seq,
                                  const CDescList& /* descs */)
{
    if (!seq.GetParentEntry()->GetParentEntry()->IsSet()) {
        return true; // nothing to do
    }
    const CSeq_entry* parent_entry = seq.GetParentEntry()->GetParentEntry();
    const CBioseq_set& parent = parent_entry->GetSet();
    if (parent.GetClass() != CBioseq_set::eClass_parts
        ||  (parent_entry->GetParentEntry()->GetSet().GetClass()
             != CBioseq_set::eClass_segset)) {
        // Got enough levels here?
        return true; // nothing to do
    }

    unsigned int n = 1;
    iterate (CBioseq_set::TSeq_set, it, parent.GetSeq_set()) {
        if ((*it)->IsSeq()  &&  &(*it)->GetSeq() == &seq) {
            m_Stream << s_Pad("SEGMENT", sm_KeywordWidth) << n
                     << " of " << parent.GetSeq_set().size() << NcbiEndl;
            break;
        }
        n++;
    }

    return true;
}


bool CGenbankWriter::WriteSource(const CBioseq& /* seq */,
                                 const CDescList& descs)
{
    string source, taxname, lineage;
    for (CTypeConstIterator<CGB_block> it = ConstBegin(descs);  it;  ++it) {
        if (it->IsSetSource()  &&  source.empty()) {
            source = it->GetSource();
        }
        if (it->IsSetTaxonomy()  &&  lineage.empty()) {
            lineage = it->GetTaxonomy();
        }                        
    }

    for (CTypeConstIterator<COrg_ref> it = ConstBegin(descs); it;  ++it) {
        if (it->IsSetCommon()  &&  source.empty()) {
            source = it->GetCommon();
        }
        if (it->IsSetTaxname()  &&  taxname.empty()) {
            taxname = it->GetTaxname();
        }
        if (it->IsSetOrgname()  &&  it->GetOrgname().IsSetLineage()
            &&  lineage.empty()) {
            lineage = it->GetOrgname().GetLineage();
        }
    }

    if (taxname.empty()) {
        taxname = "Unknown";
    }

    if (lineage.empty()) {
        lineage = "Unclassified.";
    }
    
    if (source.empty()) {
        source = taxname;
    }
    
    if (source[source.size()-1] != '.') {
        source += '.';
    }
    if (lineage[lineage.size()-1] != '.') {
        lineage += '.';
    }

    Wrap("SOURCE", source);
    m_Stream << s_Pad("  ORGANISM", sm_KeywordWidth) << taxname << NcbiEndl;
    Wrap("", lineage);

    return true;
}


static int s_FirstLocation(const CSeq_loc& loc)
{
    // XXX - should honor fuzz.
    switch (loc.Which()) {
    case CSeq_loc::e_Int:
        return loc.GetInt().GetFrom();
    case CSeq_loc::e_Packed_int:
        return loc.GetPacked_int().Get().front()->GetFrom();
    case CSeq_loc::e_Pnt:
        return loc.GetPnt().GetPoint();
    case CSeq_loc::e_Packed_pnt:
        return loc.GetPacked_pnt().GetPoints().front();
    case CSeq_loc::e_Mix:
        return s_FirstLocation(*loc.GetMix().Get().front());
    case CSeq_loc::e_Equiv:
        return s_FirstLocation(*loc.GetEquiv().Get().front());
    case CSeq_loc::e_Bond:
        if (loc.GetBond().IsSetB()) {
            return min(loc.GetBond().GetA().GetPoint(),
                       loc.GetBond().GetB().GetPoint());
        } else {
            return loc.GetBond().GetA().GetPoint();
        }
    default:
        return -1;
    }
}


static int s_LastLocation(const CSeq_loc& loc)
{
    // XXX - should honor fuzz.
    switch (loc.Which()) {
    case CSeq_loc::e_Int:
        return loc.GetInt().GetTo();
    case CSeq_loc::e_Packed_int:
        return loc.GetPacked_int().Get().back()->GetTo();
    case CSeq_loc::e_Pnt:
        return loc.GetPnt().GetPoint();
    case CSeq_loc::e_Packed_pnt:
        return loc.GetPacked_pnt().GetPoints().back();
    case CSeq_loc::e_Mix:
        return s_LastLocation(*loc.GetMix().Get().back());
    case CSeq_loc::e_Equiv:
        return s_LastLocation(*loc.GetEquiv().Get().back());
    case CSeq_loc::e_Bond:
        if (loc.GetBond().IsSetB()) {
            return max(loc.GetBond().GetA().GetPoint(),
                       loc.GetBond().GetB().GetPoint());
        } else {
            return loc.GetBond().GetA().GetPoint();
        }
    default:
        return -1;
    }
}


static string s_FormatTitle(const CTitle& title) {
    string result;
    iterate (CTitle::Tdata, it, title.Get()) {
        switch ((*it)->Which()) {
        case CTitle::C_E::e_Name:    result = (*it)->GetName();     break;
        case CTitle::C_E::e_Trans:   result = (*it)->GetTrans();    break;
        case CTitle::C_E::e_Jta:     result = (*it)->GetJta();      break;
        case CTitle::C_E::e_Iso_jta: result = (*it)->GetIso_jta();  break;
        case CTitle::C_E::e_Ml_jta:  result = (*it)->GetMl_jta();   break;
        case CTitle::C_E::e_Abr:     result = (*it)->GetAbr();      break;
        default: // tsub, coden, issn, isbn
            break; // not useful
        }
        if (!result.empty()) {
            break;
        }
    }
    return result;
}


// works for CImprint and CCit_gen
template <class T>
string FormatImprint(const T& imprint) {
    CNcbiOstrstream oss;
    if (imprint.IsSetVolume()) {
        oss << ' ' << imprint.GetVolume();
        if (imprint.IsSetIssue()) {
            oss << " (" << imprint.GetIssue() << ')';
        }
    }
    if (imprint.IsSetPages()) {
        oss << ", " << imprint.GetPages();
    }
    if (imprint.GetDate().IsStd()) {
        oss << " (" << imprint.GetDate().GetStd().GetYear() << ')';
    }

    string result(oss.str(), oss.pcount());
    oss.freeze(0);
    return result;
}


static string s_FormatAffil(const CAffil& affil)
{
    if (affil.IsStr()) {
        return affil.GetStr();
    } else {
        string result;
        const CAffil::C_Std& std = affil.GetStd();
        if (std.IsSetDiv()) {
            result = std.GetDiv();
        }
        if (std.IsSetAffil()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetAffil();
        }
        if (std.IsSetStreet()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetStreet();
        }
        if (std.IsSetCity()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetCity();
        }
        if (std.IsSetSub()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetSub();
        }
        if (std.IsSetPostal_code()) {
            if (!result.empty()) {
                result += " ";
            }
            result += std.GetPostal_code();
        }
        if (std.IsSetCountry()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetCountry();
        }
        if (std.IsSetFax()) {
            if (!result.empty()) {
                result += "; ";
            }
            result += "Fax: " + std.GetFax();
        }
        if (std.IsSetPhone()) {
            if (!result.empty()) {
                result += "; ";
            }
            result += "Phone: " + std.GetPhone();
        }
        if (std.IsSetEmail()) {
            if (!result.empty()) {
                result += "; ";
            }
            result += "E-mail: " + std.GetEmail();
        }
        return result;
    }
}


string FormatArticleSource(const CCit_art& art)
{
    switch (art.GetFrom().Which()) {
    case CCit_art::C_From::e_Journal:
    {
        const CCit_jour& jour = art.GetFrom().GetJournal();
        return s_FormatTitle(jour.GetTitle()) + FormatImprint(jour.GetImp());
    }
    case CCit_art::C_From::e_Book:
    {
        const CCit_book& book = art.GetFrom().GetBook();
        return s_FormatTitle(book.GetTitle()) + FormatImprint(book.GetImp());
    }
    case CCit_art::C_From::e_Proc:
    {
        const CCit_book& book = art.GetFrom().GetProc().GetBook();
        return s_FormatTitle(book.GetTitle()) + FormatImprint(book.GetImp());
    }
    default:
        return string();
    }
}


struct SReference {
    const CPub_equiv* pub;
    string            location;
    string            remark;
    unsigned int      serial_number;

    SReference(const CPubdesc& desc, const CSeq_loc& loc,
               const string& length_unit);
};


SReference::SReference(const CPubdesc& desc, const CSeq_loc& loc,
                       const string& length_unit)
{
    pub = &desc.GetPub();

    switch (desc.GetReftype()) {
    case CPubdesc::eReftype_sites:
        location = "sites";
        break;
    default:
        switch (loc.Which()) {
        case CSeq_loc::e_Mix:
        {
            location = length_unit + ' ';
            iterate (CSeq_loc_mix::Tdata, it, loc.GetMix().Get()) {
                if (it != loc.GetMix().Get().begin()) {
                    location += "; ";
                }
                location += (NStr::IntToString(s_FirstLocation(**it) + 1)
                             + " to "
                             + NStr::IntToString(s_LastLocation(**it) + 1));
            }
            break;
        }
        default:
            location = (length_unit + ' '
                        + NStr::IntToString(s_FirstLocation(loc) + 1)
                        + " to " + NStr::IntToString(s_LastLocation(loc) + 1));
            break;
        }
        break;
    }

    if (desc.IsSetComment()) {
        remark = desc.GetComment();
    } else {
        remark = kEmptyStr;
    }

    serial_number = kMax_UInt;
    for (CTypeConstIterator<CCit_gen> it = ConstBegin(*pub);  it;  ++it) {
        if (it->IsSetSerial_number()) {
            serial_number = it->GetSerial_number();
            BREAK(it);
        }
    }
}


bool operator<(const SReference& ref1, const SReference& ref2) {
    return ref1.serial_number < ref2.serial_number;
}


bool CGenbankWriter::WriteReference(const CBioseq& seq, const CDescList& descs)
{
    vector<SReference> v;
    {{
        CSeq_loc everywhere;
        CRef<CSeq_id> id = seq.GetId().front();
        // Try to substitute a GI id, since the object manager seems to care
        // which ID I use. :-/
        iterate (CBioseq::TId, it, seq.GetId()) {
            if ((*it)->IsGi()) {
                id = *it;
                break;
            }
        }
        // everywhere.SetWhole(id);
        {{
            CSeq_interval& si = everywhere.SetInt();
            si.SetFrom(0);
            si.SetTo(seq.GetInst().GetLength() - 1);
            // si.SetId() = *id; // Forbidden.  (Why???)
            si.SetId().SetGi(id->GetGi());
        }}

        for (CTypeConstIterator<CPubdesc> pub = ConstBegin(descs);
             pub;  ++pub) {
            v.push_back(SReference(*pub, everywhere, m_LengthUnit));
        }

        // get references from features
        for (CFeat_CI pub = m_Scope.BeginFeat(everywhere, CSeqFeatData::e_Pub);
             pub;  ++pub) {
            v.push_back(SReference(pub->GetData().GetPub(), pub->GetLocation(),
                                   m_LengthUnit));
        }
    }}

    sort(v.begin(), v.end());

    unsigned int serial_number = 0;
    iterate (vector<SReference>, ref, v) {
        if (ref->serial_number < kMax_UInt) {
            serial_number = ref->serial_number;
        } else {
            ++serial_number;
        }
        m_Stream << s_Pad("REFERENCE", sm_KeywordWidth) << serial_number
                 << "  (" << ref->location << ')' << NcbiEndl;
        
        {{
            list<string> author_list;
            for (CTypeConstIterator<CAuth_list> it = ConstBegin(*ref->pub);
                 it;  ++it) {
                switch (it->GetNames().Which()) {
                case CAuth_list::C_Names::e_Ml:
                    author_list = it->GetNames().GetMl();
                    break;
                case CAuth_list::C_Names::e_Str:
                    author_list = it->GetNames().GetStr();
                    break;
                case CAuth_list::C_Names::e_Std:
                    for (CTypeConstIterator<CName_std> it2
                             = ConstBegin(it->GetNames());
                         it2;  ++it2) {
                        string author = it2->GetLast();
                        if (it2->IsSetInitials()) {
                            author += ',' + it2->GetInitials();
                        } else if (it2->IsSetFirst()) {
                            author += ',';
                            author += it2->GetFirst()[0];
                            author += '.';
                            if (it2->IsSetMiddle()) {
                                author += it2->GetMiddle()[0];
                                author += '.';
                            }
                        }
                        author_list.push_back(author);
                    }
                    break;
                default:
                    // skip, for lack of better information
                    break;
                }
            }
            
            string authors;
            SIZE_TYPE count = author_list.size();
            iterate (list<string>, it, author_list) {
                authors += *it;
                switch (--count) {
                case 0:
                    break; // last entry
                case 1:
                    authors += " and "; // next-to-last
                    break;
                default:
                    authors += ", ";
                    break;
                }
            }
            Wrap("  AUTHORS", authors);
        }}
        
        {{
            int pubmed = 0;
            int medline = 0;
            bool seen_main = false;
            iterate (CPub_equiv::Tdata, it, ref->pub->Get()) {
                switch ((*it)->Which()) {
                case CPub::e_Gen:
                {
                    const CCit_gen& gen = (*it)->GetGen();
                    if (gen.IsSetMuid()  &&  !medline) {
                        medline = gen.GetMuid();
                    }
                    if (gen.IsSetPmid()  &&  !pubmed) {
                        pubmed = gen.GetPmid().Get();
                    }
                    if (seen_main) {
                        break;
                    }
                    if (gen.IsSetTitle()) {
                        seen_main = true;
                        Wrap("  TITLE", s_StripDot(gen.GetTitle()));
                        if (gen.IsSetJournal()) {
                            Wrap("  JOURNAL",
                                 s_FormatTitle(gen.GetJournal())
                                 + FormatImprint(gen));
                        } else if (gen.IsSetCit()
                                   &&  (gen.GetCit() == "unpublished"
                                        ||  gen.GetCit() == "Unpublished")) {
                            m_Stream << s_Pad("  JOURNAL", sm_KeywordWidth)
                                     << "Unpublished";
                            if (gen.IsSetDate() && gen.GetDate().IsStd()) {
                                m_Stream << " ("
                                         << gen.GetDate().GetStd().GetYear()
                                         << ')';
                            }
                            m_Stream << NcbiEndl;
                        }
                    }
                    break;
                }

                case CPub::e_Sub:
                {
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    const CCit_sub& sub = (*it)->GetSub();
                    Wrap("  TITLE", "Direct Submission");
                    string details = "Submitted";
                    if (sub.IsSetDate()) {
                        details += " (" + s_FormatDate(sub.GetDate()) + ')';
                    } else if (sub.IsSetImp()) {
                        details += " (" + s_FormatDate(sub.GetImp().GetDate())
                            + ')';
                    } if (sub.GetAuthors().IsSetAffil()) {
                        details += ' '
                            + s_FormatAffil(sub.GetAuthors().GetAffil());
                    }
                    Wrap("  JOURNAL", details);
                    break;
                }

                case CPub::e_Medline:
                {
                    const CMedline_entry& entry = (*it)->GetMedline();
                    if (entry.IsSetUid()  &&  !medline) {
                        medline = entry.GetUid();
                    }
                    if (entry.IsSetPmid()  &&  !pubmed) {
                        pubmed = entry.GetPmid().Get();
                    }
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    const CCit_art& art = entry.GetCit();
                    Wrap("  TITLE", s_StripDot(s_FormatTitle(art.GetTitle())));
                    Wrap("  JOURNAL", FormatArticleSource(art));
                    break;
                }

                case CPub::e_Muid:
                    if (!medline) {
                        medline = (*it)->GetMuid();
                    }
                    break;

                case CPub::e_Article:
                {
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    const CCit_art& art = (*it)->GetArticle();
                    Wrap("  TITLE", s_StripDot(s_FormatTitle(art.GetTitle())));
                    Wrap("  JOURNAL", FormatArticleSource(art));
                    break;
                }

                case CPub::e_Journal:
                {
                    const CCit_jour& jour = (*it)->GetJournal();
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    Wrap("  JOURNAL",
                         s_FormatTitle(jour.GetTitle())
                         + FormatImprint(jour.GetImp()));
                    break;
                }

                case CPub::e_Book:
                {
                    const CCit_book& book = (*it)->GetBook();
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    Wrap("  JOURNAL",
                         s_FormatTitle(book.GetTitle())
                         + FormatImprint(book.GetImp()));
                    break;
                }

                case CPub::e_Proc:
                {
                    const CCit_book& book = (*it)->GetProc().GetBook();
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    Wrap("  JOURNAL",
                         s_FormatTitle(book.GetTitle())
                         + FormatImprint(book.GetImp()));
                    break;
                }

                case CPub::e_Patent:
                {
                    const CCit_pat& pat = (*it)->GetPatent();
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    Wrap("  TITLE", s_StripDot(pat.GetTitle()));
                    string journal = "Patent: " + pat.GetCountry() + ' ';
                    if (pat.IsSetNumber()) {
                        journal += pat.GetNumber() + '-';
                    }
                    journal += pat.GetDoc_type();
                    for (CTypeConstIterator<CPatent_seq_id> psid
                             = ConstBegin(seq);
                         psid;  ++psid) {
                        journal += ' ' + NStr::IntToString(psid->GetSeqid());
                    }
                    if (pat.IsSetDate_issue()) {
                        journal += ' ' + s_FormatDate(pat.GetDate_issue());
                    }
                    Wrap("  JOURNAL", journal + ';');
                    break;
                }

                case CPub::e_Pat_id:
                {
                    const CId_pat& id = (*it)->GetPat_id();
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    string doc_type("patent");
                    if (id.IsSetDoc_type()) {
                        doc_type = id.GetDoc_type();
                    }
                    string number;
                    if (id.GetId().IsNumber()) {
                        number = id.GetId().GetNumber();
                    } else {
                        number = "application " + id.GetId().GetApp_number();
                    }
                    Wrap("  JOURNAL",
                         id.GetCountry() + ' ' + doc_type + ' ' + number);
                    break;
                }
                
                case CPub::e_Man:
                {
                    const CCit_let& let = (*it)->GetMan();
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    if (let.IsSetMan_id()) {
                        Wrap("  TITLE", s_StripDot(let.GetMan_id()));
                    }
                    const CCit_book& book = let.GetCit();
                    Wrap("  JOURNAL",
                         s_FormatTitle(book.GetTitle())
                         + FormatImprint(book.GetImp()));
                    break;
                }
                    
                case CPub::e_Pmid:
                    if (!pubmed) {
                        pubmed = (*it)->GetPmid().Get();
                    }
                    break;

                default:
                    break;
                }
            }

            if (medline) {
                m_Stream << s_Pad("  MEDLINE", sm_KeywordWidth) << medline
                         << NcbiEndl;
            }
            if (pubmed) {
                m_Stream << s_Pad("   PUBMED", sm_KeywordWidth) << pubmed
                         << NcbiEndl;
            }
        }}
        Wrap("  REMARK", ref->remark);
    }
    return true;
}


bool CGenbankWriter::WriteComment(const CBioseq& /* seq */,
                                  const CDescList& descs)
{
    string comments;
    for (CTypeConstIterator<CSeqdesc> it = ConstBegin(descs);  it;  ++it) {
        if (it->IsComment()) {
            if (!comments.empty()) {
                comments += "~~"; // blank line between comments
            }
            comments += it->GetComment();
            if (comments[comments.size() - 1] != '.') {
                comments += '.';
            }
        }
    }
    Wrap("COMMENT", comments);
    return true;
}


static string s_FormatDbtag(const CDbtag& dbtag)
{
    string result(dbtag.GetDb());
    result += ':';
    switch (dbtag.GetTag().Which()) {
    case CObject_id::e_Str:
        result += dbtag.GetTag().GetStr();
        break;
    case CObject_id::e_Id:
        result += NStr::IntToString(dbtag.GetTag().GetId());
        break;
    default:
        break;
    }
    return result;
}


static string s_FeatureName(const CSeqFeatData& data)
{
    switch (data.Which()) {
    case CSeqFeatData::e_Gene:
        return "gene";
    case CSeqFeatData::e_Org:
        return "source";
    case CSeqFeatData::e_Cdregion:
        return "CDS";
    case CSeqFeatData::e_Prot:
        return "Protein"; // XXX -- only Genpept?
    case CSeqFeatData::e_Rna:
        switch (data.GetRna().GetType()) {
        case CRNA_ref::eType_premsg:  return "precursor_rna";
        case CRNA_ref::eType_mRNA:    return "mRNA";
        case CRNA_ref::eType_tRNA:    return "tRNA";
        case CRNA_ref::eType_rRNA:    return "rRNA";
        case CRNA_ref::eType_snRNA:   return "snRNA";
        case CRNA_ref::eType_scRNA:   return "scRNA";
        default:                      return "misc_RNA";
        }
    case CSeqFeatData::e_Imp:
        return data.GetImp().GetKey();
    case CSeqFeatData::e_Site:
        switch (data.GetSite()) {
        case CSeqFeatData::eSite_binding:
        case CSeqFeatData::eSite_metal_binding:
        case CSeqFeatData::eSite_lipid_binding:
            return "misc_binding";
        case CSeqFeatData::eSite_np_binding:
            return "protein_bind";
        case CSeqFeatData::eSite_dna_binding:
            return "primer_bind"; // ?
        case CSeqFeatData::eSite_signal_peptide:
            return "sig_peptide";
        case CSeqFeatData::eSite_transit_peptide:
            return "transit_peptide";
        default:
            break;
        }
        break;
    case CSeqFeatData::e_Txinit:
        return "promoter";
    case CSeqFeatData::e_Het:
        return "misc_binding";
    case CSeqFeatData::e_Biosrc:
        return "source";
    default:
        break;
    }
    return "misc_feature";
}


static const CSeq_id& s_GetID(const CSeq_loc& loc)
{
    static CSeq_id unknown;
    switch (loc.Which()) {
    case CSeq_loc::e_Whole:
        return loc.GetWhole();
    case CSeq_loc::e_Int:
        return loc.GetInt().GetId();
    case CSeq_loc::e_Packed_int:
        return loc.GetPacked_int().Get().front()->GetId();
    case CSeq_loc::e_Pnt:
        return loc.GetPnt().GetId();
    case CSeq_loc::e_Packed_pnt:
        return loc.GetPacked_pnt().GetId();
    case CSeq_loc::e_Mix:
        return s_GetID(*loc.GetMix().Get().front());
    case CSeq_loc::e_Equiv:
        return s_GetID(*loc.GetEquiv().Get().front());
    case CSeq_loc::e_Bond:
        return loc.GetBond().GetA().GetId();
    default:
        return unknown;
    }
}


static bool s_StartsBefore(CConstRef<CSeq_feat> f1, CConstRef<CSeq_feat> f2)
{
    const CSeq_loc& loc1 = f1->GetLocation();
    const CSeq_loc& loc2 = f2->GetLocation();
    if (SerialEquals<CSeq_id>(s_GetID(loc1), s_GetID(loc2))) {
        return s_FirstLocation(loc1) < s_FirstLocation(loc2);
    } else if (f1->IsSetProduct()) {        
        if (SerialEquals<CSeq_id>(s_GetID(f1->GetProduct()), s_GetID(loc2))) {
            return s_FirstLocation(f1->GetProduct()) < s_FirstLocation(loc2);
        } else if (f2->IsSetProduct()
                   &&  SerialEquals<CSeq_id>(s_GetID(f1->GetProduct()),
                                             s_GetID(f2->GetProduct()))) {
            return s_FirstLocation(f1->GetProduct())
                < s_FirstLocation(f2->GetProduct());
        }
    }
    if (f2->IsSetProduct()
        &&  SerialEquals<CSeq_id>(s_GetID(loc1), s_GetID(f2->GetProduct()))) {
        return s_FirstLocation(loc1) < s_FirstLocation(f2->GetProduct());
    }
    return false;
}


static bool s_QuotedQualifierP(const string& name)
{
    static const char* const unquoted[] = { // keep in order!
        "anticodon", "codon", "codon_start", "cons_splice", "direction",
        "evidence", "label", "mod_base", "number", "rpt_type", "rpt_unit",
        "transl_except", "transl_table", "usedin" };
    static const size_t num_unquoted
        = sizeof(unquoted) / sizeof(*unquoted);

    return ! binary_search(unquoted, unquoted + num_unquoted, name);
}


// Amino acid abbreviations for feature table.  Differ from Iupac3aa
// in that unknown is Xaa or OTHER instead of Xxx and termination is TERM
// rather than Ter.

static const char* s_RawToAbbrev(unsigned char aa)
{ // takes Ncbistdaa or Ncbi8aa
    static const char* const kAbbrev[] = {
        "---", "Ala", "Asx", "Cys", "Asp", "Glu", "Phe", "Gly", "His", "Ile",
        "Lys", "Leu", "Met", "Asn", "Pro", "Gln", "Arg", "Ser", "Thr", "Val",
        "Trp", "Xaa", "Tyr", "Glx", "Sec", "TERM" };
    if (aa < sizeof(kAbbrev)/sizeof(*kAbbrev)) {
        return kAbbrev[aa];
    } else {
        return "OTHER";
    }
}


static const char* s_ASCIIToAbbrev(char aa)
{
    if (aa == '*') {
        return "TERM";
    } else if (aa == '-') {
        return "---";
    } else if (aa >= 'A'  &&  aa <= 'I') {
        return s_RawToAbbrev(aa - 'A' + 1);
    } else if (aa >= 'K'  &&  aa <= 'N') {
        return s_RawToAbbrev(aa - 'K' + 10);
    } else if (aa >= 'P'  &&  aa <= 'T') {
        return s_RawToAbbrev(aa - 'P' + 14);
    } else if (aa == 'U') {
        return "Sec";
    } else if (aa >= 'V'  &&  aa <= 'Z') {
        return s_RawToAbbrev(aa - 'V' + 19);
    } else if (islower(aa)) {
        return s_ASCIIToAbbrev(toupper(aa));
    } else {
        return "OTHER";
    }
}


bool CGenbankWriter::WriteFeatures(const CBioseq& seq, const CDescList& descs)
{
    CSeq_loc everywhere;
    everywhere.SetWhole(seq.GetId().front());
    // Try to substitute a GI id, since the object manager seems to care
    // which ID I use. :-/
    iterate (CBioseq::TId, id, seq.GetId()) {
        if ((*id)->IsGi()) {
            everywhere.SetWhole(*id);
            break;
        }
    }

    m_Stream << s_Pad("FEATURES", sm_FeatureNameIndent + sm_FeatureNameWidth)
             << "Location/Qualifiers" << NcbiEndl;

    {{
        WriteFeatureLocation("source", everywhere, seq);
        bool found_source = false;
        for (CTypeConstIterator<CBioSource> it = ConstBegin(descs);
             it;  ++it) {
            WriteSourceQualifiers(*it);
            found_source = true;
            BREAK(it);
        }
        if (!found_source) {
            WriteFeatureQualifier("/organism=\"unknown\"");
        }
    }}

    typedef vector< CConstRef<CSeq_feat> > TFeatVect;
    TFeatVect v;
    for (CFeat_CI feat
             = m_Scope.BeginFeat(everywhere, CSeqFeatData::e_not_set);
         feat;  ++feat) {
        if ((feat->GetData().Which() == CSeqFeatData::e_Prot
             &&  m_Format != eFormat_Genpept)
            ||  feat->GetData().Which() == CSeqFeatData::e_Pub) {
            continue;
        }
        v.push_back(CConstRef<CSeq_feat>(&*feat));
    }
    sort(v.begin(), v.end(), s_StartsBefore);

    iterate (TFeatVect, feat, v) {
        string name = s_FeatureName((*feat)->GetData());
        if ((*feat)->IsSetProduct()) {
            CNcbiOstrstream loc_stream, prod_stream;
            FormatFeatureLocation((*feat)->GetLocation(), seq, loc_stream);
            FormatFeatureLocation((*feat)->GetProduct(),  seq, prod_stream);
            string location(loc_stream.str(),  loc_stream.pcount());
            string product (prod_stream.str(), prod_stream.pcount());
            loc_stream.freeze(0);
            prod_stream.freeze(0);
            if (location.find(':') != NPOS  &&  product.find(':') == NPOS) {
                WriteFeatureLocation(name, (*feat)->GetProduct(), seq);
                WriteFeatureQualifier("coded_by", location, true);
            } else {
                // What if both contain colons (because both are segmented)?
                WriteFeatureLocation(name, location);
            }
        } else {
            WriteFeatureLocation(name, (*feat)->GetLocation(), seq);
        }

        switch ((*feat)->GetData().Which()) {
        case CSeqFeatData::e_Gene: // gene
        {
            const CGene_ref& gene = (*feat)->GetData().GetGene();
            if (gene.IsSetAllele()) {
                WriteFeatureQualifier("allele", gene.GetAllele(), true);
            }
            if (gene.IsSetDesc()) {
                WriteFeatureQualifier("function", gene.GetAllele(), true);
            }
            if (gene.IsSetMaploc()) {
                WriteFeatureQualifier("map", gene.GetAllele(), true);
            }
            if (gene.GetPseudo()) {
                WriteFeatureQualifier("/pseudo");
            }
            if (gene.IsSetDb()) {
                iterate (CGene_ref::TDb, it, gene.GetDb()) {
                    WriteFeatureQualifier("db_xref", s_FormatDbtag(**it),
                                          true);
                }
            }
            if (gene.IsSetSyn()) {
                iterate (CGene_ref::TSyn, it, gene.GetSyn()) {
                    WriteFeatureQualifier("standard_name", *it, true);
                }
            }
            break;
        }

        case CSeqFeatData::e_Org:
            WriteSourceQualifiers((*feat)->GetData().GetOrg());
            break;

        case CSeqFeatData::e_Cdregion: // CDS
        {
            if (m_Format != eFormat_Genbank) {
                continue;
            }
            const CCdregion& region = (*feat)->GetData().GetCdregion();
            if (region.IsSetFrame()) {
                WriteFeatureQualifier("codon_start",
                                      NStr::IntToString(region.GetFrame()),
                                      false);
            }
            if (region.IsSetCode()) {
                iterate (CGenetic_code::Tdata, it, region.GetCode().Get()) {
                    if ((*it)->IsId()) {
                        // XXX -- deal with other types
                        WriteFeatureQualifier("transl_table",
                                              NStr::IntToString
                                              ((*it)->GetId()),
                                              false);
                    }
                }
            }
            if (region.IsSetCode_break()) {
                iterate (CCdregion::TCode_break, it, region.GetCode_break()) {
                    const char* abbrev;
                    switch ((*it)->GetAa().Which()) {
                    case CCode_break::C_Aa::e_Ncbieaa:
                        abbrev = s_ASCIIToAbbrev((*it)->GetAa().GetNcbieaa());
                        break;
                    case CCode_break::C_Aa::e_Ncbi8aa:
                        abbrev = s_RawToAbbrev((*it)->GetAa().GetNcbi8aa());
                        break;
                    case CCode_break::C_Aa::e_Ncbistdaa:
                        abbrev = s_RawToAbbrev((*it)->GetAa().GetNcbistdaa());
                        break;
                    default:
                        abbrev = "";
                        break;
                    }
                    CNcbiOstrstream oss;
                    oss << "/transl_except=(pos:";
                    FormatFeatureLocation((*it)->GetLoc(), seq, oss);
                    oss << ",aa:" << abbrev << ')';
                    WriteFeatureQualifier(string(oss.str(), oss.pcount()));
                    oss.freeze(0);
                }
            }
            break;
        }

        case CSeqFeatData::e_Prot: // Protein
            if (m_Format == eFormat_Genpept) {
                WriteProteinQualifiers((*feat)->GetData().GetProt());
            }
            break;

        case CSeqFeatData::e_Rna: // *RNA
        {
            const CRNA_ref& rna = (*feat)->GetData().GetRna();
            if (rna.IsSetExt()  &&  rna.GetExt().IsName()) {
                WriteFeatureQualifier("note", rna.GetExt().GetName(),
                                      true);
            }

            switch (rna.GetType()) {
            case CRNA_ref::eType_tRNA:
            {
                if (!rna.IsSetExt()  ||  !rna.GetExt().IsTRNA()
                    ||  !rna.GetExt().GetTRNA().IsSetAa()
                    ||  !rna.GetExt().GetTRNA().IsSetAnticodon()) {
                    break;
                }
                const CTrna_ext& trna = rna.GetExt().GetTRNA();
                const char* abbrev;
                switch (trna.GetAa().Which()) {
                case CTrna_ext::C_Aa::e_Iupacaa:
                    abbrev = s_ASCIIToAbbrev(trna.GetAa().GetIupacaa());
                    break;
                case CTrna_ext::C_Aa::e_Ncbieaa:
                    abbrev = s_ASCIIToAbbrev(trna.GetAa().GetNcbieaa());
                    break;
                case CTrna_ext::C_Aa::e_Ncbi8aa:
                    abbrev = s_RawToAbbrev(trna.GetAa().GetNcbi8aa());
                    break;
                case CTrna_ext::C_Aa::e_Ncbistdaa:
                    abbrev = s_RawToAbbrev(trna.GetAa().GetNcbistdaa());
                    break;
                default:
                    abbrev = "";
                    break;
                }
                CNcbiOstrstream oss;
                oss << "/anticodon=(pos:";
                FormatFeatureLocation(trna.GetAnticodon(), seq, oss);
                oss << ",aa:" << abbrev << ')';
                WriteFeatureQualifier(string(oss.str(), oss.pcount()));
                oss.freeze(0);
                break;
            }
            case CRNA_ref::eType_snoRNA:
                WriteFeatureQualifier("/note=\"snoRNA\"");
                break;
            default:
                break;
            }
            if (rna.IsSetPseudo()  &&  rna.GetPseudo()) {
                WriteFeatureQualifier("/pseudo");
            }
            break;
        }

        case CSeqFeatData::e_Pub:
            for (CTypeConstIterator<CCit_gen> it
                     = ConstBegin((*feat)->GetData().GetPub());
                 it;  ++it) {
                if (it->IsSetSerial_number()) {
                    WriteFeatureQualifier("/citation=["
                                          + (NStr::IntToString
                                             (it->GetSerial_number()))
                                          + ']');
                }
                // XXX - try to find citation if no serial number
            }
            break;

        case CSeqFeatData::e_Seq:
        {
            CNcbiOstrstream oss;
            oss << "origin: ";
            FormatFeatureLocation((*feat)->GetData().GetSeq(), seq, oss);
            WriteFeatureQualifier("note", string(oss.str(), oss.pcount()),
                                  true);
            oss.freeze(0);
            break;
        }
        
        case CSeqFeatData::e_Imp:
            if ((*feat)->GetData().GetImp().IsSetDescr()) {
                WriteFeatureQualifier("note",
                                      (*feat)->GetData().GetImp().GetDescr(),
                                      true);
            }
            break;

        case CSeqFeatData::e_Region:
            // XXX -- some of these (LTR) should be feature names
            WriteFeatureQualifier("note", (*feat)->GetData().GetRegion(),
                                  true);
            break;

        case CSeqFeatData::e_Bond:
        {
            string type = CSeqFeatData::GetTypeInfo_enum_EBond()
                ->FindName((*feat)->GetData().GetBond(), true);
            WriteFeatureQualifier("note", type + " bond", true);
            break;
        }

        case CSeqFeatData::e_Site:
            switch ((*feat)->GetData().GetSite()) {
            case CSeqFeatData::eSite_binding:
            case CSeqFeatData::eSite_np_binding:
            case CSeqFeatData::eSite_dna_binding:
            case CSeqFeatData::eSite_signal_peptide:
            case CSeqFeatData::eSite_transit_peptide:
                break;
            case CSeqFeatData::eSite_metal_binding:
                WriteFeatureQualifier("/bound_moiety=\"metal\"");
                break;
            case CSeqFeatData::eSite_lipid_binding:
                WriteFeatureQualifier("/bound_moiety=\"lipid\"");
                break;
            default:
            {
                string type = CSeqFeatData::GetTypeInfo_enum_ESite()
                    ->FindName((*feat)->GetData().GetSite(), true);
                WriteFeatureQualifier("note", type + " site", true);
                break;
            }
            }
            break;

        case CSeqFeatData::e_Rsite:
        {
            const CRsite_ref& rsite = (*feat)->GetData().GetRsite();
            switch (rsite.Which()) {
            case CRsite_ref::e_Str:
                WriteFeatureQualifier("note",
                                      "Restriction site: " + rsite.GetStr(),
                                      true);
                break;
            case CRsite_ref::e_Db:
                WriteFeatureQualifier("/note=\"Restriction site\"");
                WriteFeatureQualifier("db_xref", s_FormatDbtag(rsite.GetDb()),
                                      true);
                break;
            default:
                WriteFeatureQualifier("/note=\"Restriction site\"");
            }
            break;
        }

        case CSeqFeatData::e_Txinit: // promoter
        {
            const CTxinit& txinit = (*feat)->GetData().GetTxinit();
            WriteFeatureQualifier("function", txinit.GetName(), true);
            // XXX - should turn rest of data into notes
            break;
        }

        case CSeqFeatData::e_Num:
            // XXX - turn into notes?
            break;

        case CSeqFeatData::e_Psec_str:
        {
            string type = CSeqFeatData::GetTypeInfo_enum_EPsec_str()
                ->FindName((*feat)->GetData().GetPsec_str(), true);
            WriteFeatureQualifier("note", "secondary structure: " + type,
                                  true);
            break;
        }

        case CSeqFeatData::e_Non_std_residue:
            WriteFeatureQualifier("note",
                                  "non-standard residue: "
                                  + (*feat)->GetData().GetNon_std_residue(),
                                  true);
            break;

        case CSeqFeatData::e_Het:
            WriteFeatureQualifier("bound_moiety",
                                  (*feat)->GetData().GetHet().Get(), true);
            break;

        case CSeqFeatData::e_Biosrc:
            WriteSourceQualifiers((*feat)->GetData().GetBiosrc());
            break;

        default:
            break;
        }

        // Standard qualifiers.
        if ((*feat)->GetData().Which() != CSeqFeatData::e_Biosrc) {
            try {
                // handle "CAnnot_CI::CAnnot_CI() -- unsupported location type"
                for (CFeat_CI gene = m_Scope.BeginFeat((*feat)->GetLocation(),
                                                       CSeqFeatData::e_Gene);
                     gene;  ++gene) {
                    if (gene->GetData().GetGene().IsSetLocus()) {
                        WriteFeatureQualifier("gene",
                                              gene->GetData().GetGene()
                                              .GetLocus(),
                                              true);
                    }
                }
            } catch (exception& e) {
                ERR_POST(Warning << e.what());
            }
        }

        if ((*feat)->IsSetPartial()  &&  (*feat)->GetPartial()) {
            bool fuzzy = false;
            for (CTypeConstIterator<CInt_fuzz> it
                     = ConstBegin((*feat)->GetLocation());
                 it;  ++it) {
                fuzzy = true;
                BREAK(it);
            }
            if (!fuzzy) {
                WriteFeatureQualifier("/partial");
            }
        }

        if ((*feat)->IsSetComment()) {
            WriteFeatureQualifier("note", (*feat)->GetComment(), true);
        }

        if ((*feat)->IsSetProduct()  &&  m_Format != eFormat_Genpept) {
            try {
                // handle "CAnnot_CI::CAnnot_CI() -- unsupported location type"
                for (CFeat_CI prot = m_Scope.BeginFeat((*feat)->GetProduct(),
                                                       CSeqFeatData::e_Prot);
                     prot;  ++prot) {
                    WriteProteinQualifiers(prot->GetData().GetProt());
                }
            } catch (exception& e) {
                ERR_POST(Warning << e.what());
            } 
            {{
                const CBioseq& prot_seq
                    = m_Scope.GetBioseq(m_Scope.GetBioseqHandle
                                        (s_GetID((*feat)->GetProduct())));
                const CTextseq_id* tsid;
                iterate(CBioseq::TId, it, prot_seq.GetId()) {
                    if ((tsid = (*it)->GetTextseq_Id()) != NULL) {
                        WriteFeatureQualifier("protein_id",
                                              tsid->GetAccession() + '.'
                                              + (NStr::IntToString
                                                 (tsid->GetVersion())),
                                              true);
                    } else if ((*it)->IsGi()) {
                        WriteFeatureQualifier("db_xref",
                                              "GI:"
                                              + (NStr::IntToString
                                                 ((*it)->GetGi())),
                                              true);
                    }
                }
#ifdef USE_SEQ_VECTOR
                {{
                    TASCIISeqData asd
                        = m_Scope.GetSequence(m_Scope.GetBioseqHandle
                                              (*prot_seq.GetId().front()));
                    string data;
                    data.resize(asd.size());
                    for (SIZE_TYPE n = 0; n < asd.size(); n++) {
                        data[n] = asd[n];
                    }
                    WriteFeatureQualifier("translation", data, true);
                }}
#else
                WriteFeatureQualifier("translation",
                                      ToASCII(prot_seq.GetInst()), true);
#endif                    
            }}
        }

        if ((*feat)->IsSetQual()) {
            iterate (CSeq_feat::TQual, it, (*feat)->GetQual()) {
                WriteFeatureQualifier((*it)->GetQual(), (*it)->GetVal(),
                                      s_QuotedQualifierP((*it)->GetQual()));
            }
        }

        if ((*feat)->IsSetTitle()) {
            WriteFeatureQualifier("label", (*feat)->GetTitle(), false);
        }

        if ((*feat)->IsSetCit()) {
            for (CTypeConstIterator<CCit_gen> it
                     = ConstBegin((*feat)->GetCit());
                 it;  ++it) {
                if (it->IsSetSerial_number()) {
                    WriteFeatureQualifier("/citation=["
                                          + (NStr::IntToString
                                             (it->GetSerial_number()))
                                          + ']');
                }
                // otherwise, track down...could be in descs OR
                // elsewhere in the feature table. :-/
            }
        }

        if ((*feat)->IsSetExp_ev()) {
            switch ((*feat)->GetExp_ev()) {
            case CSeq_feat::eExp_ev_experimental:
                WriteFeatureQualifier("/evidence=experimental");
                break;
            case CSeq_feat::eExp_ev_not_experimental:
                WriteFeatureQualifier("/evidence=not_experimental");
                break;
            }
        }

        if ((*feat)->IsSetDbxref()) {
            iterate (CSeq_feat::TDbxref, it, (*feat)->GetDbxref()) {
                WriteFeatureQualifier("db_xref", s_FormatDbtag(**it), true);
            }
        }

        if ((*feat)->IsSetPseudo()  &&  (*feat)->GetPseudo()) {
            WriteFeatureQualifier("/pseudo");
        }

        if ((*feat)->IsSetExcept_text()) {
            WriteFeatureQualifier("exception", (*feat)->GetExcept_text(),
                                  true);
        }
    }

    return true;
}


void CGenbankWriter::WriteProteinQualifiers(const CProt_ref& prot)
{
    if (prot.IsSetName()) {
        iterate (CProt_ref::TName, it, prot.GetName()) {
            WriteFeatureQualifier("product", *it, true);
        }
    }
    
    if (prot.IsSetDesc()) {
        WriteFeatureQualifier("function", prot.GetDesc(), true);
    }
    
    if (prot.IsSetEc()) {
        iterate (CProt_ref::TEc, it, prot.GetEc()) {
            WriteFeatureQualifier("EC_number", *it, true);
        }
    }

    if (prot.IsSetActivity()) {
        iterate (CProt_ref::TActivity, it, prot.GetActivity()) {
            WriteFeatureQualifier("function", *it, true);
        }
    }

    if (prot.IsSetDb()) {
        iterate (CProt_ref::TDb, it, prot.GetDb()) {
            WriteFeatureQualifier("db_xref", s_FormatDbtag(**it), true);
        }
    }
}


void CGenbankWriter::WriteSourceQualifiers(const COrg_ref& org)
{
    if (org.IsSetTaxname()) {
        WriteFeatureQualifier("organism", org.GetTaxname(), true);
    } else if (org.IsSetCommon()) {
        WriteFeatureQualifier("organism", org.GetCommon(), true);
    }
    

    if (org.IsSetMod()) {
        iterate (COrg_ref::TMod, it, org.GetMod()) {
            WriteFeatureQualifier("note", *it, true);
        }
    }

    if (org.IsSetDb()) {
        iterate (COrg_ref::TDb, it, org.GetDb()) {
            WriteFeatureQualifier("db_xref", s_FormatDbtag(**it), true);
        }
    }

    if (org.IsSetOrgname() && org.GetOrgname().IsSetMod()) {
        iterate (COrgName::TMod, it, org.GetOrgname().GetMod()) {
            switch ((*it)->GetSubtype()) {
            case COrgMod::eSubtype_strain:
                WriteFeatureQualifier("strain", (*it)->GetSubname(), true);
                break;
            case COrgMod::eSubtype_substrain:
                WriteFeatureQualifier("sub_strain", (*it)->GetSubname(), true);
                break;
            case COrgMod::eSubtype_variety:
                WriteFeatureQualifier("variety", (*it)->GetSubname(), true);
                break;
            case COrgMod::eSubtype_serotype:
                WriteFeatureQualifier("serotype", (*it)->GetSubname(), true);
                break;
            case COrgMod::eSubtype_cultivar:
                WriteFeatureQualifier("cultivar", (*it)->GetSubname(), true);
                break;
            case COrgMod::eSubtype_isolate:
                WriteFeatureQualifier("isolate", (*it)->GetSubname(), true);
                break;
            case COrgMod::eSubtype_nat_host:
                WriteFeatureQualifier("specific_host", (*it)->GetSubname(),
                                      true);
                break;
            case COrgMod::eSubtype_sub_species:
                WriteFeatureQualifier("sub_species", (*it)->GetSubname(),
                                      true);
                break;
            case COrgMod::eSubtype_specimen_voucher:
                WriteFeatureQualifier("specimen_voucher", (*it)->GetSubname(),
                                      true);
                break;
            case COrgMod::eSubtype_other:
                WriteFeatureQualifier("note", (*it)->GetSubname(), true);
                break;
            default:
                string type = COrgMod::GetTypeInfo_enum_ESubtype()
                    ->FindName((*it)->GetSubtype(), true);
                WriteFeatureQualifier("note",
                                      type + ": " + (*it)->GetSubname(), true);
            }
        }
    }
}


void CGenbankWriter::WriteSourceQualifiers(const CBioSource& source)
{
    WriteSourceQualifiers(source.GetOrg());

    if (source.IsSetGenome()) {
        switch (source.GetGenome()) {
        case CBioSource::eGenome_chloroplast:
            WriteFeatureQualifier("/organelle=\"plastid:chloroplast\"");
            break;
        case CBioSource::eGenome_chromoplast:
            WriteFeatureQualifier("/organelle=\"plastid:chromoplast\"");
            break;
        case CBioSource::eGenome_kinetoplast:
            WriteFeatureQualifier("/organelle=\"mitochondrion:kinetoplast\"");
            break;
        case CBioSource::eGenome_mitochondrion:
            WriteFeatureQualifier("/organelle=\"mitochondrion\"");
            break;
        case CBioSource::eGenome_plastid:
            WriteFeatureQualifier("/organelle=\"plastid\"");
            break;
        case CBioSource::eGenome_macronuclear:
            WriteFeatureQualifier("/macronuclear");
            break;
        case CBioSource::eGenome_cyanelle:
            WriteFeatureQualifier("/organelle=\"plastid:cyanelle\"");
            break;
        case CBioSource::eGenome_proviral:
            WriteFeatureQualifier("/proviral");
            break;
        case CBioSource::eGenome_virion:
            WriteFeatureQualifier("/virion");
            break;
        case CBioSource::eGenome_nucleomorph:
            WriteFeatureQualifier("/organelle=\"nucleomorph\"");
            break;
        case CBioSource::eGenome_apicoplast:
            WriteFeatureQualifier("/organelle=\"plastid:apicoplast\"");
            break;
        case CBioSource::eGenome_leucoplast:
            WriteFeatureQualifier("/organelle=\"plastid:leucoplast\"");
            break;
        case CBioSource::eGenome_proplastid:
            WriteFeatureQualifier("/organelle=\"plastid:proplastid\"");
            break;
        case CBioSource::eGenome_plasmid:
        case CBioSource::eGenome_transposon:
            break;
            // tag requires a value; hope for corresponding subsource.
        default:
            string type = CBioSource::GetTypeInfo_enum_EGenome()
                ->FindName(source.GetGenome(), true);
            WriteFeatureQualifier("note", "genome: " + type, true);
            break;
        }
    }
    
    if (source.IsSetSubtype()) {
        iterate (CBioSource::TSubtype, it, source.GetSubtype()) {
            switch ((*it)->GetSubtype()) {
            case CSubSource::eSubtype_chromosome:
                WriteFeatureQualifier("chromosome", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_map:
                WriteFeatureQualifier("map", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_clone:
                WriteFeatureQualifier("clone", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_subclone:
                WriteFeatureQualifier("sub_clone", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_haplotype:
                WriteFeatureQualifier("haplotype", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_sex:
                WriteFeatureQualifier("sex", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_cell_line:
                WriteFeatureQualifier("cell_line", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_cell_type:
                WriteFeatureQualifier("cell_type", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_tissue_type:
                WriteFeatureQualifier("tissue_type", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_clone_lib:
                WriteFeatureQualifier("clone_lib", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_dev_stage:
                WriteFeatureQualifier("dev_stage", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_frequency:
                WriteFeatureQualifier("frequency", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_germline:
                WriteFeatureQualifier("/germline");
                break;
            case CSubSource::eSubtype_rearranged:
                WriteFeatureQualifier("/rearranged");
                break;
            case CSubSource::eSubtype_lab_host:
                WriteFeatureQualifier("lab_host", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_pop_variant:
                WriteFeatureQualifier("pop_variant", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_tissue_lib:
                WriteFeatureQualifier("tissue_lib", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_plasmid_name:
                WriteFeatureQualifier("plasmid", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_transposon_name:
                WriteFeatureQualifier("transposon", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_insertion_seq_name:
                WriteFeatureQualifier("insertion_seq", (*it)->GetName(), true);
                break;
            case CSubSource::eSubtype_country:
                WriteFeatureQualifier("country", (*it)->GetName(), true);
                break;
                // ...
            case COrgMod::eSubtype_other:
                WriteFeatureQualifier("note", (*it)->GetName(), true);
                break;
            default:
                string type = CSubSource::GetTypeInfo_enum_ESubtype()
                    ->FindName((*it)->GetSubtype(), true);
                WriteFeatureQualifier("note", type + ": " + (*it)->GetName(),
                                      true);
            }
        }
    }

    if (source.IsSetIs_focus()) {
        WriteFeatureQualifier("/focus");
    }
}


bool CGenbankWriter::WriteSequence(const CBioseq& seq,
                                   const CDescList& /* descs */)
{
#ifdef USE_SEQ_VECTOR
    TASCIISeqData asd
        = m_Scope.GetSequence(m_Scope.GetBioseqHandle(*seq.GetId().front()));
#else
    TASCIISeqData asd = ToASCII(seq.GetInst());
#endif
    if (m_Format == eFormat_Genbank) {
        size_t a = 0, c = 0, g = 0, t = 0, other = 0;
        for (size_t pos = 0;  pos < asd.size();  ++pos) {
            switch (asd[pos]) {
            case 'A': ++a;     break;
            case 'C': ++c;     break;
            case 'G': ++g;     break;
            case 'T': ++t;     break;
            default:  ++other; break;
            }
        }
        m_Stream << s_Pad("BASE COUNT", sm_KeywordWidth) << ' '
                 << setw(6) << a << " a " << setw(6) << c << " c "
                 << setw(6) << g << " g " << setw(6) << t << " t";
        if (other) {
            m_Stream << ' ' << setw(6) << other << " other";
            if (other > 1) {
                m_Stream << 's';
            }
        }
        m_Stream << NcbiEndl;
    }
    m_Stream << s_Pad("ORIGIN", sm_KeywordWidth);
    for (size_t n = 0;  n < asd.size();  ++n) {
        if (n % 60 == 0) {
            m_Stream << NcbiEndl << setw(9) << n + 1 << ' ';
        } else if (n % 10 == 0) {
            m_Stream << ' ';
        }
        m_Stream << static_cast<char>(tolower(asd[n]));
    }
    m_Stream << NcbiEndl;
    return true;
}


void CGenbankWriter::Wrap(const string& keyword, const string& contents,
                          unsigned int indent)
{
    unsigned int data_width = sm_LineWidth - indent;

    if (contents.empty()) {
        return;
    }

    m_Stream << s_Pad(keyword, indent);
    const string newline = s_Pad("\n", indent + 1);

    SIZE_TYPE pos = 0, tilde_pos;
    while ((tilde_pos = contents.find('~', pos)) != NPOS
           ||  contents.size() - pos > data_width) {
        if (tilde_pos <= pos + data_width - 1) {
            m_Stream << contents.substr(pos, tilde_pos - pos) << newline;
            pos = tilde_pos + 1;
        } else {
            SIZE_TYPE split_pos = contents.rfind(' ', pos + data_width - 1);
            if (split_pos <= pos  ||  split_pos == NPOS) {
                split_pos = contents.rfind(',', pos + data_width - 1) + 1;
            }
            if (split_pos <= pos  ||  split_pos == NPOS + 1) {
                split_pos = pos + data_width - 1;
            }
            while (split_pos > 0  &&  contents[split_pos-1] == ' ') {
                --split_pos;
            }
            m_Stream << contents.substr(pos, split_pos - pos) << newline;
            while (split_pos < contents.size() - 1
                   &&  contents[split_pos] == ' ') {
                ++split_pos;
            }
            pos = split_pos;
        }
    }
    m_Stream << contents.substr(pos) << NcbiEndl;
}


void CGenbankWriter::FormatIDPrefix(const CSeq_id& id,
                                    const CBioseq& default_seq,
                                    CNcbiOstream& dest)
{
    iterate (CBioseq::TId, it, default_seq.GetId()) {
        if ((*it)->Match(id)) {
            return;
        }
    }

    const CTextseq_id* tsid = id.GetTextseq_Id();
    if (tsid == NULL) {
        const CBioseq& seq = m_Scope.GetBioseq(m_Scope.GetBioseqHandle(id));
        for (CTypeConstIterator<CTextseq_id> it = ConstBegin(seq); it;  ++it) {
            tsid = &*it;
            BREAK(it);
        }
    }
    if (tsid) {
        dest << tsid->GetAccession() << '.' << tsid->GetVersion() << ':';
    } else {
        dest << "EXTERN:";
    }
}


static void s_FormatFuzzyInt(const CInt_fuzz& fuzz, int n, CNcbiOstream& dest)
{
    switch (fuzz.Which()) {
    case CInt_fuzz::e_P_m:
    {
        int delta = fuzz.GetP_m();
        dest << '(' << (n - delta) << '.' << (n + delta) << ')';
        break;
    }

    case CInt_fuzz::e_Range:
    {
        const CInt_fuzz::C_Range& range = fuzz.GetRange();
        dest << '(' << range.GetMin() << '.' << range.GetMax() << ')';
        break;
    }

    case CInt_fuzz::e_Pct:
    {
        int delta = n * fuzz.GetPct() / 1000;
        dest << '(' << (n - delta) << '.' << (n + delta) << ')';
        break;
    }

    case CInt_fuzz::e_Lim:
        switch (fuzz.GetLim()) {
        case CInt_fuzz::eLim_gt:
            dest << '>' << n;
            break;
        case CInt_fuzz::eLim_lt:
            dest << '<' << n;
            break;
        case CInt_fuzz::eLim_tr:
            dest << n << '^' << (n + 1);
            break;
        case CInt_fuzz::eLim_tl:
            dest << (n - 1) << '^' << n;
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }
}


template <class T>
const char* StrandPrefix(const T& location) {
    if (location.IsSetStrand()  &&  location.GetStrand() == eNa_strand_minus) {
        return "complement(";
    } else {
        return "";
    }
}


template <class T>
const char* StrandSuffix(const T& location) {
    if (location.IsSetStrand()  &&  location.GetStrand() == eNa_strand_minus) {
        return ")";
    } else {
        return "";
    }
}


void CGenbankWriter::FormatFeatureInterval(const CSeq_interval& interval,
                                           const CBioseq& default_seq,
                                           CNcbiOstream& dest)
{
    dest << StrandPrefix(interval);
    FormatIDPrefix(interval.GetId(), default_seq, dest);
    if (interval.IsSetFuzz_from()) {
        s_FormatFuzzyInt(interval.GetFuzz_from(), interval.GetFrom() + 1,
                         dest);
    } else {
        dest << (interval.GetFrom() + 1);
    }
    dest << "..";
    if (interval.IsSetFuzz_to()) {
        s_FormatFuzzyInt(interval.GetFuzz_to(), interval.GetTo() + 1, dest);
    } else {
        dest << (interval.GetTo() + 1);
    }
    dest << StrandSuffix(interval);
}


void CGenbankWriter::FormatFeatureLocation(const CSeq_loc& location,
                                           const CBioseq& default_seq,
                                           CNcbiOstream& dest)
{
    // Add 1 to all numbers because genbank indexes from 1 but the raw
    // data indexes from 0
    switch (location.Which()) {
    case CSeq_loc::e_Whole:
    {
        const CSeq_id& id = location.GetWhole();
        FormatIDPrefix(id, default_seq, dest);
        dest << "1.." << (m_Scope.GetBioseq(m_Scope.GetBioseqHandle(id))
                          .GetInst().GetLength());
        break;
    }

    case CSeq_loc::e_Int:
        FormatFeatureInterval(location.GetInt(), default_seq, dest);
        break;

    case CSeq_loc::e_Packed_int:
    {
        const CPacked_seqint& pi = location.GetPacked_int();
        dest << "join("; // should this be order?
        bool first = true;
        iterate (CPacked_seqint::Tdata, it, pi.Get()) {
            if (!first) {
                dest << ',';
            }
            FormatFeatureInterval(**it, default_seq, dest);
            first = false;
        }
        dest << ')'; // closes join(...);
        break;
    }

    case CSeq_loc::e_Pnt:
    {
        const CSeq_point& pnt = location.GetPnt();
        dest << StrandPrefix(pnt);
        FormatIDPrefix(pnt.GetId(), default_seq, dest);
        if (pnt.IsSetFuzz()) {
            s_FormatFuzzyInt(pnt.GetFuzz(), pnt.GetPoint() + 1, dest);
        } else {
            dest << pnt.GetPoint();
        }
        dest << StrandSuffix(pnt);
        break;
    }

    case CSeq_loc::e_Packed_pnt:
    {
        const CPacked_seqpnt& pp = location.GetPacked_pnt();
        dest << StrandPrefix(pp) << "join("; // should this be order?
        bool first = true;
        iterate (CPacked_seqpnt::TPoints, it, pp.GetPoints()) {
            if (!first) {
                dest << ',';
            }
            if (pp.IsSetFuzz()) {
                s_FormatFuzzyInt(pp.GetFuzz(), *it + 1, dest);
            } else {
                dest << *it;
            }
            first = false;
        }
        dest << ')' /* closes join(...); */ << StrandSuffix(pp);
        break;
    }

    case CSeq_loc::e_Mix:
    {
        const CSeq_loc_mix& mix = location.GetMix();
        bool first = true;
        dest << "join(";
        iterate (CSeq_loc_mix::Tdata, it, mix.Get()) {
            if (!first) {
                dest << ',';
            }
            FormatFeatureLocation(**it, default_seq, dest);
            first = false;
        }
        dest << ')'; // closes join(...);
        break;
    }

    case CSeq_loc::e_Equiv: // just take the first one
        FormatFeatureLocation(**location.GetEquiv().Get().begin(), default_seq,
                              dest);
        break;

    case CSeq_loc::e_Bond:
    {
        if (location.GetBond().IsSetB()) {
            dest << "order(";
        }
        const CSeq_point& a = location.GetBond().GetA();
        dest << StrandPrefix(a);
        FormatIDPrefix(a.GetId(), default_seq, dest);
        if (a.IsSetFuzz()) {
            s_FormatFuzzyInt(a.GetFuzz(), a.GetPoint() + 1, dest);
        } else {
            dest << a.GetPoint();
        }
        dest << StrandSuffix(a);
        if (location.GetBond().IsSetB()) {
            const CSeq_point& b = location.GetBond().GetB();
            dest << ',' << StrandPrefix(b);
            FormatIDPrefix(b.GetId(), default_seq, dest);
            if (b.IsSetFuzz()) {
                s_FormatFuzzyInt(b.GetFuzz(), b.GetPoint() + 1, dest);
            } else {
                dest << b.GetPoint();
            }
            dest << StrandSuffix(b) << ')'; // closes order(...);
        }
    }

    case CSeq_loc::e_Feat:
    {
        const CSeq_feat& feat = FindFeature(location.GetFeat(), default_seq);
        FormatFeatureLocation(feat.GetLocation(), default_seq, dest);
        break;
    }

    default:
        break;
    }
}


const CSeq_feat& CGenbankWriter::FindFeature(const CFeat_id& id,
                                             const CBioseq& default_seq)
{
    static CSeq_feat dummy;
    const CSeq_entry& tse = m_Scope.GetTSE(m_Scope.GetBioseqHandle
                                           (*default_seq.GetId().front()));
    CTypeConstIterator<CSeq_feat> it = ConstBegin(tse);
    // declared outside loop to avoid WorkShop bug. :-/
    for (;  it;  ++it) {
        const CFeat_id& id2 = it->GetId();
        if (id.Which() != id2.Which()) {
            continue;
        }
        switch (id.Which()) {
        case CFeat_id::e_Gibb:
            if (id.GetGibb() == id2.GetGibb()) {
                return *it;
            }
            break;

        case CFeat_id::e_Giim:
        { 
            const CGiimport_id& giim  = id.GetGiim();
            const CGiimport_id& giim2 = id2.GetGiim();
            if (giim.GetId() == giim2.GetId()
                &&  (!giim.IsSetDb()
                     ||  (giim2.IsSetDb()  &&  giim.GetDb() == giim2.GetDb()))
                &&  (!giim.IsSetRelease()
                     ||  (giim2.IsSetRelease()
                          &&  giim.GetRelease() == giim2.GetRelease()))) {
                return *it;
            }
            break;        
        }

        case CFeat_id::e_Local:
        {
            const CObject_id& local  = id.GetLocal();
            const CObject_id& local2 = id2.GetLocal();
            if (local.Which() != local2.Which()) {
                break; // out of switch; continue the for loop
            }
            switch (local.Which()) {
            case CObject_id::e_Id:
                if (local.GetId() == local2.GetId()) {
                    return *it;
                }
                break;

            case CObject_id::e_Str:
                if (local.GetStr() == local2.GetStr()) {
                    return *it;
                }
                break;

            default:
                break;
            }
            break;
        }

        case CFeat_id::e_General:
        {
            if (id.GetGeneral().GetDb() != id2.GetGeneral().GetDb()) {
                break;
            }
            const CObject_id& tag  = id.GetGeneral().GetTag();
            const CObject_id& tag2 = id2.GetGeneral().GetTag();
            if (tag.Which() != tag2.Which()) {
                break; // out of switch; continue the for loop
            }
            switch (tag.Which()) {
            case CObject_id::e_Id:
                if (tag.GetId() == tag2.GetId()) {
                    return *it;
                }
                break;

            case CObject_id::e_Str:
                if (tag.GetStr() == tag2.GetStr()) {
                    return *it;
                }
                break;

            default:
                break;
            }
            break;
        }

        default:
            break;
        }
    }
    return dummy; // didn't find anything :-(
}


void CGenbankWriter::WriteFeatureLocation(const string& name,
                                          const string& location)
{
    static const string indent(sm_FeatureNameIndent, ' ');
    Wrap(indent + name, location, sm_FeatureNameIndent + sm_FeatureNameWidth);
}


void CGenbankWriter::WriteFeatureLocation(const string& name,
                                          const CSeq_loc& location,
                                          const CBioseq& default_seq)
{
    CNcbiOstrstream oss;
    FormatFeatureLocation(location, default_seq, oss);
    WriteFeatureLocation(name, string(oss.str(), oss.pcount()));
    oss.freeze(0);
}


void CGenbankWriter::WriteFeatureQualifier(const string& qual)
{
    Wrap("", qual, sm_FeatureNameIndent + sm_FeatureNameWidth);
}


void CGenbankWriter::WriteFeatureQualifier(const string& name,
                                           const string& value, bool quote)
{
    if (quote) {
        // surround with double-quotes; double all internal double-quotes.
        string text = '/' + name + "=\"";
        SIZE_TYPE pos = 0, quote_pos;
        while ((quote_pos = value.find('\"', pos)) != NPOS) {
            text += value.substr(pos, quote_pos - pos) + '\"';
            pos = quote_pos;
        }
        WriteFeatureQualifier(text + value.substr(pos) + '\"');
    } else {
        WriteFeatureQualifier('/' + name + '=' + value);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
