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
* Author:  Mati Shomrat, NCBI
*
* File Description:
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqblock/PIR_block.hpp>
#include <objects/seqblock/PRF_block.hpp>
#include <objects/seqblock/PRF_ExtraSrc.hpp>
#include <objects/seqblock/PDB_block.hpp>
#include <objects/seqblock/PDB_replace.hpp>
#include <objects/seqblock/SP_block.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/dbsource_item.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


CDBSourceItem::CDBSourceItem(CBioseqContext& ctx) :
    CFlatItem(&ctx)
{
    x_GatherInfo(ctx);
}


void CDBSourceItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const

{
    formatter.FormatDBSource(*this, text_os);
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


void CDBSourceItem::x_GatherInfo(CBioseqContext& ctx)
{
    const CBioseq::TId& ids = ctx.GetBioseqIds();
    CConstRef<CSeq_id> id = FindBestChoice(ids, s_ScoreForDBSource);

    if ( !id ) {
        m_DBSource.push_back("UNKNOWN");
        return;
    }
    switch ( id->Which() ) {
    case CSeq_id::e_Pir:
        m_DBSource.push_back(x_FormatDBSourceID(*id));
        x_AddPIRBlock(ctx);
        break;

    case CSeq_id::e_Swissprot:
        m_DBSource.push_back(x_FormatDBSourceID(*id));
        x_AddSPBlock(ctx);
        break;

    case CSeq_id::e_Prf:
        m_DBSource.push_back(x_FormatDBSourceID(*id));
        x_AddPRFBlock(ctx);
        break;

    case CSeq_id::e_Pdb:
        m_DBSource.push_back(x_FormatDBSourceID(*id));
        x_AddPDBBlock(ctx);
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
        CScope& scope = ctx.GetScope();
        const CSeq_feat* feat = GetCDSForProduct(ctx.GetHandle());
        if ( feat == 0 ) {
            // may also be protein product of mature peptide feature
            feat = GetPROTForProduct(ctx.GetHandle());
        }
        if ( feat != 0 ) {
            const CSeq_loc& loc = feat->GetLocation();
            CBioseq_Handle nuc = scope.GetBioseqHandle(loc);
            if ( nuc ) {
                for ( CSeq_loc_CI li(loc); li; ++li ) {
                    CBioseq_Handle bsh = scope.GetBioseqHandle(li.GetSeq_id());
                    if ( bsh ) {
                        sources.insert(bsh);
                    }
                }

            }
        }
        ITERATE (set<CBioseq_Handle>, it, sources) {
            CConstRef<CSeq_id> id2 = FindBestChoice(it->GetBioseqCore()->GetId(),
                s_ScoreForDBSource);
            if ( id2 != 0 ) {
                string str = x_FormatDBSourceID(*id2);
                if ( !str.empty() ) {
                    m_DBSource.push_back(str);
                }
            }
        }
        if ( sources.empty() ) {
            m_DBSource.push_back(x_FormatDBSourceID(*id));
        }
        break;
    }
    default:
        m_DBSource.push_back("UNKNOWN");
    }
}

void CDBSourceItem::x_AddPIRBlock(CBioseqContext& ctx)
{
    CSeqdesc_CI dsc(ctx.GetHandle(), CSeqdesc::e_Pir);
    if ( !dsc ) {
        return;
    }

    x_SetObject(*dsc);

    const CPIR_block& pir = dsc->GetPir();
    if (pir.CanGetHost()) {
        m_DBSource.push_back("host: " + pir.GetHost());
    }
    if (pir.CanGetSource()) {
        m_DBSource.push_back("source: " + pir.GetSource());
    }
    if (pir.CanGetSummary()) {
        m_DBSource.push_back("summary: " + pir.GetSummary());
    }
    if (pir.CanGetGenetic()) {
        m_DBSource.push_back("genetic: " + pir.GetGenetic());
    }
    if (pir.CanGetIncludes()) {
        m_DBSource.push_back("includes: " + pir.GetIncludes());
    }
    if (pir.CanGetPlacement()) {
        m_DBSource.push_back("placement: " + pir.GetPlacement());
    }
    if (pir.CanGetSuperfamily()) {
        m_DBSource.push_back("superfamily: " + pir.GetSuperfamily());
    }
    if (pir.CanGetCross_reference()) {
        m_DBSource.push_back("xref: " + pir.GetCross_reference());
    }
    if (pir.CanGetDate()) {
        m_DBSource.push_back("PIR dates: " + pir.GetDate());
    }
    if (pir.GetHad_punct()) {
        m_DBSource.push_back("punctuation in sequence");
    }
    if (pir.CanGetSeqref()) {
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


void CDBSourceItem::x_AddSPBlock(CBioseqContext& ctx)
{
    CSeqdesc_CI dsc(ctx.GetHandle(), CSeqdesc::e_Sp);
    if ( !dsc ) {
        return;
    }
    x_SetObject(*dsc);

    const CSP_block& sp = dsc->GetSp();
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
    if (sp.CanGetExtra_acc()  &&  !sp.GetExtra_acc().empty() ) {
        m_DBSource.push_back("extra_accessions:"
                             + NStr::Join(sp.GetExtra_acc(), ","));
    }
    if (sp.GetImeth()) {
        m_DBSource.push_back("seq starts with Met");
    }
    if (sp.CanGetPlasnm()  &&  !sp.GetPlasnm().empty() ) {
        m_DBSource.push_back("plasmid:" + NStr::Join(sp.GetPlasnm(), ","));
    }
    if (sp.CanGetCreated()) {
        string s("created: ");
        sp.GetCreated().GetDate(&s, "%3N %D %Y");
        m_DBSource.push_back(s + '.');
    }
    if (sp.CanGetSequpd()) {
        string s("sequence updated: ");
        sp.GetSequpd().GetDate(&s, "%3N %D %Y");
        m_DBSource.push_back(s + '.');
    }
    if (sp.CanGetAnnotupd()) {
        string s("annotation updated: ");
        sp.GetAnnotupd().GetDate(&s, "%3N %D %Y");
        m_DBSource.push_back(s + '.');
    }
    if (sp.CanGetSeqref()  &&  !sp.GetSeqref().empty() ) {
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
            if ( s ) {
                string acc = (*it)->GetSeqIdString(true);
                xrefs.push_back(s + acc);
            }
        }
        if ( !xrefs.empty() ) {
            m_DBSource.push_back("xrefs: " + NStr::Join(xrefs, ", "));
        }
    }
    if (sp.CanGetDbref()  &&  !sp.GetDbref().empty() ) {
        list<string> xrefs;
        ITERATE (CSP_block::TDbref, it, sp.GetDbref()) {
            const CObject_id& tag = (*it)->GetTag();
            string id = (tag.IsStr() ? tag.GetStr()
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


void CDBSourceItem::x_AddPRFBlock(CBioseqContext& ctx)
{
    CSeqdesc_CI dsc(ctx.GetHandle(), CSeqdesc::e_Prf);
    if ( !dsc ) {
        return;
    }

    x_SetObject(*dsc);

    const CPRF_block& prf = dsc->GetPrf();
    if (prf.CanGetExtra_src()) {
        const CPRF_ExtraSrc& es = prf.GetExtra_src();
        if (es.CanGetHost()) {
            m_DBSource.push_back("host: " + es.GetHost());
        }
        if (es.CanGetPart()) {
            m_DBSource.push_back("part: " + es.GetPart());
        }
        if (es.CanGetState()) {
            m_DBSource.push_back("state: " + es.GetState());
        }
        if (es.CanGetStrain()) {
            m_DBSource.push_back("strain: " + es.GetStrain());
        }
        if (es.CanGetTaxon()) {
            m_DBSource.push_back("taxonomy: " + es.GetTaxon());
        }
    }
    NON_CONST_ITERATE (list<string>, it, m_DBSource) {
        *it += (&*it == &m_DBSource.back() ? '.' : ';');
    }
}


void CDBSourceItem::x_AddPDBBlock(CBioseqContext& ctx)
{
    CSeqdesc_CI dsc(ctx.GetHandle(), CSeqdesc::e_Pdb);
    if ( !dsc ) {
        return;
    }

    x_SetObject(*dsc);

    const CPDB_block& pdb = dsc->GetPdb();
    {{
        string s("deposition: ");
        DateToString(pdb.GetDeposition(), s);
        m_DBSource.push_back(s);
    }}
    m_DBSource.push_back("class: " + pdb.GetClass());
    if (!pdb.GetSource().empty() ) {
        m_DBSource.push_back("source: " + NStr::Join(pdb.GetSource(), ", "));
    }
    if (pdb.CanGetExp_method()) {
        m_DBSource.push_back("Exp. method: " + pdb.GetExp_method());
    }
    if (pdb.CanGetReplace()) {
        const CPDB_replace& rep = pdb.GetReplace();
        if ( !rep.GetIds().empty() ) {
            m_DBSource.push_back
                ("ids replaced: " + NStr::Join(pdb.GetSource(), ", "));
        }
        string s("replacement date: ");
        DateToString(rep.GetDate(), s);
        m_DBSource.push_back(s);
    }
    NON_CONST_ITERATE (list<string>, it, m_DBSource) {
        *it += (&*it == &m_DBSource.back() ? '.' : ';');
    }
}


string CDBSourceItem::x_FormatDBSourceID(const CSeq_id& id) {
    switch ( id.Which() ) {
    case CSeq_id::e_Local:
        {{
            const CObject_id& oi = id.GetLocal();
            return (oi.IsStr() ? oi.GetStr() : NStr::IntToString(oi.GetId()));
        }}
    case CSeq_id::e_Gi:
        {{
            return "gi: " + NStr::IntToString(id.GetGi());
        }}
    case CSeq_id::e_Pdb:
        {{
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
            if (pdb.CanGetRel()) {
                s += sep + "release ";
                DateToString(pdb.GetRel(), s);
                sep = ",";
            }
            return s;
        }}
    default:
        {{
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
            if ( tsid->CanGetName() ) {
                s += "locus " + tsid->GetName();
                sep = " ";
            } else {
                comma.erase();
            }
            if (tsid->CanGetAccession()) {
                string acc = tsid->GetAccession();
                if (tsid->CanGetVersion()) {
                    acc += '.' + NStr::IntToString(tsid->GetVersion());
                }
                s += comma + sep + "accession " + acc;
                sep = " ";
            }
            if (tsid->CanGetRelease()) {
                s += sep + "release " + tsid->GetRelease();
            }
            if (id.IsSwissprot()) {
                s += ';';
            }
            return s;
        }}
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2004/04/22 15:55:04  shomrat
* Changes in context
*
* Revision 1.4  2004/03/25 20:36:31  shomrat
* Use handles
*
* Revision 1.3  2004/03/16 19:07:25  vasilche
* Use CConstRef<CSeq_id> to store returned CConstRef<CSeq_id>.
*
* Revision 1.2  2003/12/18 17:43:32  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:19:33  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
