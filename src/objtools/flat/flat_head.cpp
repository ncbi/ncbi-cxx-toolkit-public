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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   New (early 2003) flat-file generator -- representation of "header"
*   data, which translates into a format-dependent sequence of paragraphs.
*
* ===========================================================================
*/

#include <objects/flat/flat_head.hpp>

#include <corelib/ncbiutil.hpp>
#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/PDB_block.hpp>
#include <objects/seqblock/PDB_replace.hpp>
#include <objects/seqblock/PIR_block.hpp>
#include <objects/seqblock/PRF_ExtraSrc.hpp>
#include <objects/seqblock/PRF_block.hpp>
#include <objects/seqblock/SP_block.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/objmgr/feat_ci.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seqdesc_ci.hpp>
#include <objects/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


inline
void SFlatHead::x_AddDate(const CDate& date)
{
    if (m_UpdateDate.Empty()
        ||  date.Compare(*m_UpdateDate) == CDate::eCompare_after) {
        m_UpdateDate = &date;
    }
    if (m_CreateDate.Empty()
        ||  date.Compare(*m_CreateDate) == CDate::eCompare_before) {
        m_CreateDate = &date;
    }
}


SFlatHead::SFlatHead(SFlatContext& ctx)
    : m_Strandedness(CSeq_inst::eStrand_not_set),
      m_Topology    (CSeq_inst::eTopology_not_set),
      m_Context     (&ctx)
{
    CScope&                     scope = ctx.m_Handle.GetScope();
    CBioseq_Handle::TBioseqCore seq   = ctx.m_Handle.GetBioseqCore();
    ctx.m_PrimaryID = FindBestChoice(seq->GetId(), CSeq_id::Score);
    ctx.m_Accession = ctx.m_PrimaryID->GetSeqIdString(true);
    {{
        const CTextseq_id* tsid = m_Context->m_PrimaryID->GetTextseq_Id();
        if (tsid  &&  tsid->IsSetName()) {
            m_Locus = tsid->GetName();
        } else if (tsid  &&  tsid->IsSetAccession()) {
            m_Locus = tsid->GetAccession();
        } else {
            // complain?
            m_Locus = m_Context->m_PrimaryID->GetSeqIdString(false);
        }
    }}
    ITERATE (CBioseq::TId, it, seq->GetId()) {
        if (*it != ctx.m_PrimaryID) {
            m_OtherIDs.push_back(*it);
        }
        switch ((*it)->Which()) {
        case CSeq_id::e_Gi:
            ctx.m_GI = (*it)->GetGi();
            break;
        case CSeq_id::e_Other:
            ctx.m_IsRefSeq = true;
            break;
        case CSeq_id::e_Tpg: case CSeq_id::e_Tpe: case CSeq_id::e_Tpd:
            ctx.m_IsTPA = true;
            break;
        default:
            break;
        }
        CSeq_id::EAccessionInfo ai = (*it)->IdentifyAccession();
        if ((ai & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_wgs
            &&  NStr::EndsWith((*it)->GetTextseq_Id()->GetAccession(),
                               "000000")) {
            ctx.m_IsWGSMaster = true;
        } else if (ai == CSeq_id::eAcc_refseq_genome) {
            ctx.m_IsRefSeqGenome = true;
        }
    }

    {{
        const CSeq_inst& inst = seq->GetInst();
        m_Strandedness = inst.GetStrand();
        m_Topology     = inst.GetTopology();
    }}

    ctx.m_Length = sequence::GetLength(*ctx.m_Location, &scope);
    m_Definition = sequence::GetTitle(ctx.m_Handle);
    if ( !NStr::EndsWith(m_Definition, ".") ) {
        m_Definition += '.';
    }
    if (ctx.m_IsProt) { // populate m_SourceIDs
        x_AddDBSource();
    }

    IFlatFormatter::EDatabase db = ctx.m_Formatter->GetDatabase();
    for (CSeqdesc_CI it(ctx.m_Handle);  it;  ++it) {
        switch (it->Which()) {
            // bother translating old GIBB-* data?
        case CSeqdesc::e_Org:
        case CSeqdesc::e_Source:
            if (m_Division.empty()  &&  db != IFlatFormatter::eDB_EMBL) {
                // iterate to deal with hybrids
                for (CTypeConstIterator<COrgName> orgn(*it);  orgn;  ++orgn) {
                    if (orgn->IsSetDiv()) {
                        m_Division = orgn->GetDiv();
                        BREAK(orgn);
                    }
                }
            }
            break;

        case CSeqdesc::e_Genbank:
        {
            const CGB_block& gb = it->GetGenbank();
            if (gb.IsSetExtra_accessions()) {
                m_SecondaryIDs.insert(m_SecondaryIDs.end(),
                                      gb.GetExtra_accessions().begin(),
                                      gb.GetExtra_accessions().end());
            }
            if (gb.IsSetEntry_date()) {
                x_AddDate(gb.GetEntry_date());
            }
            if (db != IFlatFormatter::eDB_EMBL  &&  gb.IsSetDiv()) {
                m_Division = gb.GetDiv();
            }
            break;
        }

        case CSeqdesc::e_Sp:
        {
            const CSP_block& sp = it->GetSp();
            if (sp.IsSetExtra_acc()) {
                m_SecondaryIDs.insert(m_SecondaryIDs.end(),
                                      sp.GetExtra_acc().begin(),
                                      sp.GetExtra_acc().end());
            }
            if (sp.IsSetCreated()) {
                x_AddDate(sp.GetCreated());
            }
            if (sp.IsSetSequpd()) {
                x_AddDate(sp.GetSequpd());
            }
            if (sp.IsSetAnnotupd()) {
                x_AddDate(sp.GetAnnotupd());
            }
            break;
        }

        case CSeqdesc::e_Embl:
        {
            const CEMBL_block& embl = it->GetEmbl();
            if (db == IFlatFormatter::eDB_EMBL  &&  embl.IsSetDiv()) {
                m_Division = CEMBL_block::GetTypeInfo_enum_EDiv()
                    ->FindName(embl.GetDiv(), true);
            }
            x_AddDate(embl.GetCreation_date()); // mandatory field
            x_AddDate(embl.GetUpdate_date()); // mandatory field
            if (embl.IsSetExtra_acc()) {
                m_SecondaryIDs.insert(m_SecondaryIDs.end(),
                                      embl.GetExtra_acc().begin(),
                                      embl.GetExtra_acc().end());
            }
            break;
        }

        case CSeqdesc::e_Create_date:
            x_AddDate(it->GetCreate_date());
            break;

        case CSeqdesc::e_Update_date:
            x_AddDate(it->GetUpdate_date());
            break;

        case CSeqdesc::e_Pdb:
        {
            const CPDB_block& pdb = it->GetPdb();
            x_AddDate(pdb.GetDeposition()); // mandatory field
            // replacement history -> secondary IDs?
            break;
        }

        case CSeqdesc::e_Molinfo:
        {
            const CMolInfo& mi = it->GetMolinfo();
            if (mi.IsSetBiomol()) {
                ctx.m_Biomol = mi.GetBiomol();
            }
        }

        default:
            break;
        }
    }
}


const char* SFlatHead::GetMolString(void) const
{
    const IFlatFormatter& f = *m_Context->m_Formatter;
    if (f.GetDatabase() == IFlatFormatter::eDB_EMBL
        &&  f.GetMode() <= IFlatFormatter::eMode_Entrez) {
        switch (m_Context->m_Biomol) {
        case CMolInfo::eBiomol_genomic:
        case CMolInfo::eBiomol_other_genetic:
        case CMolInfo::eBiomol_genomic_mRNA:
            return "DNA";

        case CMolInfo::eBiomol_pre_RNA:
        case CMolInfo::eBiomol_mRNA:
        case CMolInfo::eBiomol_rRNA:
        case CMolInfo::eBiomol_tRNA:
        case CMolInfo::eBiomol_snRNA:
        case CMolInfo::eBiomol_scRNA:
        case CMolInfo::eBiomol_cRNA:
        case CMolInfo::eBiomol_snoRNA:
        case CMolInfo::eBiomol_transcribed_RNA:
            return "RNA";

        case CMolInfo::eBiomol_peptide:
            return "AA ";

        default:
            switch (m_Context->m_Mol) {
            case CSeq_inst::eMol_dna: return "DNA";
            case CSeq_inst::eMol_rna: return "RNA";
            case CSeq_inst::eMol_aa:  return "AA ";
            default:                  return "xxx";
            }
        }
    } else {
        switch (m_Context->m_Biomol) {
        case CMolInfo::eBiomol_genomic:          return "DNA";
        case CMolInfo::eBiomol_pre_RNA:          return "RNA";
        case CMolInfo::eBiomol_mRNA:             return "mRNA";
        case CMolInfo::eBiomol_rRNA:             return "rRNA";
        case CMolInfo::eBiomol_tRNA:             return "tRNA";
        case CMolInfo::eBiomol_snRNA:            return "uRNA";
        case CMolInfo::eBiomol_scRNA:            return "scRNA";
        case CMolInfo::eBiomol_peptide:          return " AA";
        case CMolInfo::eBiomol_other_genetic:    return "DNA";
        case CMolInfo::eBiomol_genomic_mRNA:     return "DNA";
        case CMolInfo::eBiomol_cRNA:             return "RNA";
        case CMolInfo::eBiomol_snoRNA:           return "snoRNA";
        case CMolInfo::eBiomol_transcribed_RNA:  return "RNA";
        default:
            switch (m_Context->m_Mol) {
            case CSeq_inst::eMol_dna: return "DNA";
            case CSeq_inst::eMol_rna: return "RNA";
            case CSeq_inst::eMol_aa:  return " AA";
            default:                  return "   ";
            }
        }
    }
}


inline
static int s_ScoreForDBSource(const CRef<CSeq_id>& x) {
    switch (x->Which()) {
    case CSeq_id::e_not_set:                        return kMax_Int;
    case CSeq_id::e_Gi:                             return 31;
    case CSeq_id::e_Giim:                           return 30;
    case CSeq_id::e_Local: case CSeq_id::e_General: return 20;
    case CSeq_id::e_Other:                          return 18;
    case CSeq_id::e_Gibbmt:                         return 16;
    case CSeq_id::e_Gibbsq: case CSeq_id::e_Patent: return 15;
    case CSeq_id::e_Pdb:                            return 12;
    default:                                        return 10;
    }
}


void SFlatHead::x_AddDBSource(void)
{
    CBioseq_Handle::TBioseqCore seq = m_Context->m_Handle.GetBioseqCore();
    const CSeq_id* id = FindBestChoice(seq->GetId(), s_ScoreForDBSource);

    if ( !id ) {
        m_DBSource.push_back("UNKNOWN");
        return;
    }
    switch (id->Which()) {
    case CSeq_id::e_Pir:
        m_DBSource.push_back(x_FormatDBSourceID(*id));
        x_AddPIRBlock();
        break;

    case CSeq_id::e_Swissprot:
        m_DBSource.push_back(x_FormatDBSourceID(*id));
        x_AddSPBlock();
        break;

    case CSeq_id::e_Prf:
        m_DBSource.push_back(x_FormatDBSourceID(*id));
        x_AddPRFBlock();
        break;

    case CSeq_id::e_Pdb:
        m_DBSource.push_back(x_FormatDBSourceID(*id));
        x_AddPDBBlock();
        break;

    case CSeq_id::e_General:
        if ( !NStr::StartsWith(id->GetGeneral().GetDb(), "PID") ) {
            m_DBSource.push_back("UNKNOWN");
            break;
        }
        // otherwise, fall through
    case CSeq_id::e_Gibbsq: case CSeq_id::e_Gibbmt: case CSeq_id::e_Giim:
    case CSeq_id::e_Genbank: case CSeq_id::e_Embl: case CSeq_id::e_Other:
    case CSeq_id::e_Gi: case CSeq_id::e_Ddbj:
    case CSeq_id::e_Tpg: case CSeq_id::e_Tpe: case CSeq_id::e_Tpd:
    {
        set<CBioseq_Handle> sources;
        CScope&             scope = m_Context->m_Handle.GetScope();
        for (CFeat_CI it(scope, *m_Context->m_Location,
                         CSeqFeatData::e_not_set,
                         CAnnot_CI::eOverlap_Intervals, CFeat_CI::eResolve_TSE,
                         CFeat_CI::e_Product);
             it;  ++it) {
            for (CTypeConstIterator<CSeq_id> id2(it->GetLocation());
                 id2;  ++id2) {
                sources.insert(scope.GetBioseqHandle(*id2));
            }
        }
        ITERATE (set<CBioseq_Handle>, it, sources) {
            m_DBSource.push_back(x_FormatDBSourceID
                                 (*FindBestChoice(it->GetBioseqCore()->GetId(),
                                                  s_ScoreForDBSource)));
        }
        if (sources.empty()) {
            m_DBSource.push_back(x_FormatDBSourceID(*id));
        }
        break;
    }
    default:
        m_DBSource.push_back("UNKNOWN");
    }
}


string SFlatHead::x_FormatDBSourceID(const CSeq_id& id) {
    switch (id.Which()) {
    case CSeq_id::e_Local:
    {
        const CObject_id& oi = id.GetLocal();
        return (oi.IsStr() ? oi.GetStr() : NStr::IntToString(oi.GetId()));
    }
    case CSeq_id::e_Gi:
        return "gi: " + NStr::IntToString(id.GetGi());
    case CSeq_id::e_Pdb:
    {
        const CPDB_seq_id& pdb = id.GetPdb();
        string s("pdb: "), sep;
        if ( !pdb.GetMol().Get().empty() ) {
            s += "molecule " + pdb.GetMol().Get();
            sep = ",";
        }
        if (pdb.GetChain() > 0) {
            s += sep + "chain " + NStr::IntToString(pdb.GetChain());
            sep = ",";
        }
        if (pdb.IsSetRel()) {
            s += sep + "release ";
            m_Context->m_Formatter->FormatDate(pdb.GetRel(), s);
            sep = ",";
        }
        return s;
    }
    default:
    {
        const CTextseq_id* tsid = id.GetTextseq_Id();
        if ( !tsid ) {
            return kEmptyStr;
        }
        string s, sep, comma;
        switch (id.Which()) {
        case CSeq_id::e_Embl:       s = "embl ";        comma = ",";  break;
        case CSeq_id::e_Other:      s = "REFSEQ: ";                   break;
        case CSeq_id::e_Swissprot:  s = "swissprot: ";  comma = ",";  break;
        case CSeq_id::e_Pir:        s = "pir: ";                      break;
        case CSeq_id::e_Prf:        s = "prf: ";                      break;
        default:                    break;
        }
        if (tsid->IsSetName()) {
            s += "locus " + tsid->GetName();
            sep = " ";
        } else {
            comma.erase();
        }
        if (tsid->IsSetAccession()) {
            string acc = tsid->GetAccession();
            if (tsid->IsSetVersion()) {
                acc += '.' + NStr::IntToString(tsid->GetVersion());
            }
            s += comma + sep + "accession "
                + m_Context->m_Formatter->GetAccnLink(acc);
            sep = " ";
        }
        if (tsid->IsSetRelease()) {
            s += sep + "release " + tsid->GetRelease();
        }
        if (id.IsSwissprot()) {
            s += ';';
        }
        return s;
    }
    }
}


void SFlatHead::x_AddPIRBlock(void)
{
    for (CSeqdesc_CI dsc(m_Context->m_Handle, CSeqdesc::e_Pir);  dsc;  ++dsc) {
        m_ProteinBlock = &*dsc;
        break;
    }
    if ( !m_ProteinBlock ) {
        return;
    }
    const CPIR_block& pir = m_ProteinBlock->GetPir();
    if (pir.IsSetHost()) {
        m_DBSource.push_back("host: " + pir.GetHost());
    }
    if (pir.IsSetSource()) {
        m_DBSource.push_back("source: " + pir.GetSource());
    }
    if (pir.IsSetSummary()) {
        m_DBSource.push_back("summary: " + pir.GetSummary());
    }
    if (pir.IsSetGenetic()) {
        m_DBSource.push_back("genetic: " + pir.GetGenetic());
    }
    if (pir.IsSetIncludes()) {
        m_DBSource.push_back("includes: " + pir.GetIncludes());
    }
    if (pir.IsSetPlacement()) {
        m_DBSource.push_back("placement: " + pir.GetPlacement());
    }
    if (pir.IsSetSuperfamily()) {
        m_DBSource.push_back("superfamily: " + pir.GetSuperfamily());
    }
    if (pir.IsSetCross_reference()) {
        m_DBSource.push_back("xref: " + pir.GetCross_reference());
    }
    if (pir.IsSetDate()) {
        m_DBSource.push_back("PIR dates: " + pir.GetDate());
    }
    if (pir.GetHad_punct()) {
        m_DBSource.push_back("punctuation in sequence");
    }
    if (pir.IsSetSeqref()) {
        list<string> xrefs;
        ITERATE (CPIR_block::TSeqref, it, pir.GetSeqref()) {
            const char* type = 0;
            switch ((*it)->Which()) {
            case CSeq_id::e_Genbank:    type = "genbank ";    break;
            case CSeq_id::e_Embl:       type = "embl ";       break;
            case CSeq_id::e_Pir:        type = "pir ";        break;
            case CSeq_id::e_Swissprot:  type = "swissprot ";  break;
            case CSeq_id::e_Gi:         type = "gi: ";        break;
            case CSeq_id::e_Ddbj:       type = "ddbj ";       break;
            case CSeq_id::e_Prf:        type = "prf ";        break;
            default:                    break;
            }
            if (type) {
                xrefs.push_back(type + (*it)->GetSeqIdString(true));
            }
        }
        if ( !xrefs.empty() ) {
            m_DBSource.push_back("xrefs: " + NStr::Join(xrefs, ", "));
        }
    }
    NON_CONST_ITERATE (list<string>, it, m_DBSource) {
        // The C version puts newlines before these for some reason
        *it += (&*it == &m_DBSource.back() ? '.' : ';');
    }
}


void SFlatHead::x_AddSPBlock(void)
{
    for (CSeqdesc_CI dsc(m_Context->m_Handle, CSeqdesc::e_Sp);  dsc;  ++dsc) {
        m_ProteinBlock = &*dsc;
        break;
    }
    if ( !m_ProteinBlock ) {
        return;
    }
    const CSP_block& sp = m_ProteinBlock->GetSp();
    switch (sp.GetClass()) {
    case CSP_block::eClass_standard:
        m_DBSource.push_back("class: standard.");
        break;
    case CSP_block::eClass_prelim:
        m_DBSource.push_back("class: preliminary.");
        break;
    default:
        break;
    }
    // laid out slightly differently from the C version, but I think that's
    // a bug in the latter (which runs some things together)
    if (sp.IsSetExtra_acc()  &&  !sp.GetExtra_acc().empty() ) {
        m_DBSource.push_back("extra_accessions:"
                             + NStr::Join(sp.GetExtra_acc(), ","));
    }
    if (sp.GetImeth()) {
        m_DBSource.push_back("seq starts with Met");
    }
    if (sp.IsSetPlasnm()  &&  !sp.GetPlasnm().empty() ) {
        m_DBSource.push_back("plasmid:" + NStr::Join(sp.GetPlasnm(), ","));
    }
    if (sp.IsSetCreated()) {
        string s("created: ");
        sp.GetCreated().GetDate(&s, "%3N %D %Y");
        m_DBSource.push_back(s + '.');
    }
    if (sp.IsSetSequpd()) {
        string s("sequence updated: ");
        sp.GetSequpd().GetDate(&s, "%3N %D %Y");
        m_DBSource.push_back(s + '.');
    }
    if (sp.IsSetAnnotupd()) {
        string s("annotation updated: ");
        sp.GetAnnotupd().GetDate(&s, "%3N %D %Y");
        m_DBSource.push_back(s + '.');
    }
    if (sp.IsSetSeqref()  &&  !sp.GetSeqref().empty() ) {
        list<string> xrefs;
        ITERATE (CSP_block::TSeqref, it, sp.GetSeqref()) {
            const char* s = 0;
            switch ((*it)->Which()) {
            case CSeq_id::e_Genbank:  s = "genbank accession ";          break;
            case CSeq_id::e_Embl:     s = "embl accession ";             break;
            case CSeq_id::e_Pir:      s = "pir locus ";                  break;
            case CSeq_id::e_Swissprot: s = "swissprot accession ";       break;
            case CSeq_id::e_Gi:       s = "gi: ";                        break;
            case CSeq_id::e_Ddbj:     s = "ddbj accession ";             break;
            case CSeq_id::e_Prf:      s = "prf accession ";              break;
            case CSeq_id::e_Pdb:      s = "pdb accession ";              break;
            case CSeq_id::e_Tpg:   s = "genbank third party accession "; break;
            case CSeq_id::e_Tpe:      s = "embl third party accession "; break;
            case CSeq_id::e_Tpd:      s = "ddbj third party accession "; break;
            default:                  break;
            }
            if (s) {
                string acc = (*it)->GetSeqIdString(true);
                xrefs.push_back(s + m_Context->m_Formatter->GetAccnLink(acc));
            }
        }
        if ( !xrefs.empty() ) {
            m_DBSource.push_back("xrefs: " + NStr::Join(xrefs, ", "));
        }
    }
    if (sp.IsSetDbref()  &&  !sp.GetDbref().empty() ) {
        list<string> xrefs;
        ITERATE (CSP_block::TDbref, it, sp.GetDbref()) {
            const CObject_id& tag = (*it)->GetTag();
            string            id  = (tag.IsStr() ? tag.GetStr()
                                     : NStr::IntToString(tag.GetId()));
            if ((*it)->GetDb() == "MIM") {
                xrefs.push_back
                    ("MIM <a href=\""
                     "http://www.ncbi.nlm.nih.gov/entrez/dispomim.cgi?id=" + id
                     + "\">" + id + "</a>");
            } else {
                xrefs.push_back((*it)->GetDb() + id); // no space(!)
            }
        }
        m_DBSource.push_back
            ("xrefs (non-sequence databases): " + NStr::Join(xrefs, ", "));
    }
}


void SFlatHead::x_AddPRFBlock(void)
{
    for (CSeqdesc_CI dsc(m_Context->m_Handle, CSeqdesc::e_Prf);  dsc;  ++dsc) {
        m_ProteinBlock = &*dsc;
        break;
    }
    if ( !m_ProteinBlock ) {
        return;
    }
    const CPRF_block& prf = m_ProteinBlock->GetPrf();
    if (prf.IsSetExtra_src()) {
        const CPRF_ExtraSrc& es = prf.GetExtra_src();
        if (es.IsSetHost()) {
            m_DBSource.push_back("host: " + es.GetHost());
        }
        if (es.IsSetPart()) {
            m_DBSource.push_back("part: " + es.GetPart());
        }
        if (es.IsSetState()) {
            m_DBSource.push_back("state: " + es.GetState());
        }
        if (es.IsSetStrain()) {
            m_DBSource.push_back("strain: " + es.GetStrain());
        }
        if (es.IsSetTaxon()) {
            m_DBSource.push_back("taxonomy: " + es.GetTaxon());
        }
    }
    NON_CONST_ITERATE (list<string>, it, m_DBSource) {
        *it += (&*it == &m_DBSource.back() ? '.' : ';');
    }
}


void SFlatHead::x_AddPDBBlock(void)
{
    for (CSeqdesc_CI dsc(m_Context->m_Handle, CSeqdesc::e_Pdb);  dsc;  ++dsc) {
        m_ProteinBlock = &*dsc;
        break;
    }
    if ( !m_ProteinBlock ) {
        return;
    }
    const CPDB_block& pdb = m_ProteinBlock->GetPdb();
    {{
        string s("deposition: ");
        m_Context->m_Formatter->FormatDate(pdb.GetDeposition(), s);
        m_DBSource.push_back(s);
    }}
    m_DBSource.push_back("class: " + pdb.GetClass());
    if (!pdb.GetSource().empty() ) {
        m_DBSource.push_back("source: " + NStr::Join(pdb.GetSource(), ", "));
    }
    if (pdb.IsSetExp_method()) {
        m_DBSource.push_back("Exp. method: " + pdb.GetExp_method());
    }
    if (pdb.IsSetReplace()) {
        const CPDB_replace& rep = pdb.GetReplace();
        if ( !rep.GetIds().empty() ) {
            m_DBSource.push_back
                ("ids replaced: " + NStr::Join(pdb.GetSource(), ", "));
        }
        string s("replacement date: ");
        m_Context->m_Formatter->FormatDate(rep.GetDate(), s);
        m_DBSource.push_back(s);
    }
    NON_CONST_ITERATE (list<string>, it, m_DBSource) {
        *it += (&*it == &m_DBSource.back() ? '.' : ';');
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
