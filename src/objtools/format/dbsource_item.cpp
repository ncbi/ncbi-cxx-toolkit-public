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
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/dbsource_item.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


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


static int s_ScoreForDBSource(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    switch (id->Which()) {
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


static const CSeq_id_Handle s_FindBestChoiceForDbsource(const CSeq_id_Handle& idh, CScope& scope)
{
    return FindBestChoice(scope.GetIds(idh), s_ScoreForDBSource);
}


static void s_AddToUniqueIdList(const CSeq_id_Handle& idh, vector<CSeq_id_Handle>& unique_ids)
{
    ITERATE (vector<CSeq_id_Handle>, it, unique_ids) {
        if (idh == *it) {
            return;
        }
    }
    unique_ids.push_back(idh);
}


static bool s_HasLocalBioseq(const CSeq_loc& loc, const CSeq_entry_Handle& tse)
{
    CScope& scope = tse.GetScope();
    for (CSeq_loc_CI li(loc); li; ++li) {
        CBioseq_Handle local = 
            scope.GetBioseqHandleFromTSE(li.GetSeq_id(), tse);
        if (local) {
            return true;
        }
    }
    return false;
}


void CDBSourceItem::x_GatherInfo(CBioseqContext& ctx)
{
    const bool bHtml = ctx.Config().DoHTML();

    const CBioseq_Handle& seq = ctx.GetHandle();
    const CBioseq_Handle::TId& ids = seq.GetId();
    CSeq_id_Handle idh = FindBestChoice(ids, s_ScoreForDBSource);

    if (!idh) {
        m_DBSource.push_back("UNKNOWN");
        return;
    }

    switch (idh.Which()) {
    case CSeq_id::e_Pir:
        m_DBSource.push_back(x_FormatDBSourceID(idh));
        x_AddPIRBlock(ctx);
        break;

    case CSeq_id::e_Swissprot:
        m_DBSource.push_back(x_FormatDBSourceID(idh));
        x_AddSPBlock(ctx);
        break;

    case CSeq_id::e_Prf:
        m_DBSource.push_back(x_FormatDBSourceID(idh));
        x_AddPRFBlock(ctx);
        break;

    case CSeq_id::e_Pdb:
        m_DBSource.push_back(x_FormatDBSourceID(idh));
        x_AddPDBBlock(ctx);
        break;

    case CSeq_id::e_General:
        if (!NStr::StartsWith(idh.GetSeqId()->GetGeneral().GetDb(), "PID")) {
            m_DBSource.push_back("UNKNOWN");
            break;
        }
        // otherwise, fall through
    case CSeq_id::e_Gibbsq: case CSeq_id::e_Gibbmt: case CSeq_id::e_Giim:
    case CSeq_id::e_Genbank: case CSeq_id::e_Embl: case CSeq_id::e_Other:
    case CSeq_id::e_Gi: case CSeq_id::e_Ddbj:
    case CSeq_id::e_Tpg: case CSeq_id::e_Tpe: case CSeq_id::e_Tpd:
    {
        CScope& scope = ctx.GetScope();
        vector<CSeq_id_Handle> unique_ids;

        // find generating feature
        const CSeq_feat* feat = sequence::GetCDSForProduct(seq);
        if (feat == NULL) {
            // may also be protein product of mature peptide feature
            feat = sequence::GetPROTForProduct(seq);
        }

        if (feat != NULL) {
            const CSeq_loc& loc = feat->GetLocation();
            CSeq_entry_Handle topLevelEntry = seq.GetTopLevelEntry();
            if (s_HasLocalBioseq(loc, topLevelEntry)) {
                for (CSeq_loc_CI li(loc); li; ++li) {
                    s_AddToUniqueIdList(li.GetSeq_id_Handle(), unique_ids);
                }
            } /* else {
                const CSeq_id *cds_seq_id = loc.GetId();
                if( NULL != cds_seq_id && cds_seq_id->IsGi() ) {
                    CSeq_id_Base::TGi cds_gi = cds_seq_id->GetGi();
                    s_AddToUniqueIdList( CSeq_id_Handle::GetHandle(cds_gi), unique_ids);
                } 
            } */
        }

        string str;
        ITERATE (vector<CSeq_id_Handle>, it, unique_ids) {
            CSeq_id_Handle idh2 = s_FindBestChoiceForDbsource(*it, scope);
            if (idh2) {
                str.erase();
                str = x_FormatDBSourceID(idh2);
                if (!NStr::IsBlank(str)) {
                    m_DBSource.push_back(str);
                }
            } else {
                m_DBSource.push_back( x_FormatDBSourceID( *it ) );
            }
        }

        if( m_DBSource.empty() && feat != NULL ) {
            const CSeq_loc& loc = feat->GetLocation();
            const CSeq_id *cds_seq_id = loc.GetId();
            if( NULL != cds_seq_id && cds_seq_id->IsGi() ) {
                CSeq_id_Base::TGi cds_gi = cds_seq_id->GetGi();
                // s_AddToUniqueIdList( CSeq_id_Handle::GetHandle(cds_gi), unique_ids);
                m_DBSource.push_back( x_FormatDBSourceID( CSeq_id_Handle::GetHandle(cds_gi) ) );
            } 
        }

        if (m_DBSource.empty()) {
            m_DBSource.push_back(x_FormatDBSourceID(idh));
        }
        break;
    }
    default:
        m_DBSource.push_back("UNKNOWN");
    }

    // turn double-quotes to single-quotes in all m_DBSources, 
    // except inside HTML tags
    NON_CONST_ITERATE( list<string>, it, m_DBSource ) {
        if( bHtml ) {
            ConvertQuotesNotInHTMLTags( *it );
        } else {
            replace( it->begin(), it->end(), '\"', '\'' );
        }
    }
}

void CDBSourceItem::x_AddPIRBlock(CBioseqContext& ctx)
{
    // In this function, the newlines seem weird because the C toolkit 
    // outputs this way.  Hopefully in the future we can do something 
    // more consistent.


    CSeqdesc_CI dsc(ctx.GetHandle(), CSeqdesc::e_Pir);
    if ( !dsc ) {
        return;
    }

    x_SetObject(*dsc);

    bool containsHostLine = false; // another hack to try to match C's whitespace

    const CPIR_block& pir = dsc->GetPir();
    if (pir.CanGetHost()) {
        m_DBSource.push_back("host:" + pir.GetHost() + "\n");
        containsHostLine = true;
    }
    if (pir.CanGetSource()) {
        m_DBSource.push_back("source: " + pir.GetSource() + "\n");
    }
    if (pir.CanGetSummary()) {
        m_DBSource.push_back("summary: " + pir.GetSummary() + "\n");
    }
    if (pir.CanGetGenetic()) {
        m_DBSource.push_back("genetic: " + pir.GetGenetic() + "\n");
    }
    if (pir.CanGetIncludes()) {
        m_DBSource.push_back("includes: " + pir.GetIncludes() + "\n");
    }
    if (pir.CanGetPlacement()) {
        m_DBSource.push_back("placement: " + pir.GetPlacement() + "\n");
    }
    if (pir.CanGetSuperfamily()) {
        m_DBSource.push_back("superfamily: " + pir.GetSuperfamily() + "\n");
    }
    if (pir.CanGetCross_reference()) {
        m_DBSource.push_back("xref: " + pir.GetCross_reference() + "\n");
    }
    if (pir.CanGetDate()) {
        m_DBSource.push_back("PIR dates: " + pir.GetDate() + "\n");
    }
    if (pir.CanGetHad_punct() && pir.GetHad_punct() ) {
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
        if( &*it == &m_DBSource.front() ) {
            // first one has newline AFTER the semicolon
            *it += ";\n";
            // another hack to match C toolkit
            /* if( (it + 1) != m_DBSource.end() && ! NStr::StartsWith(*(it + 1), "host")  ) {
                *it += ";\n";
            } */
        } else if( &*it == &m_DBSource.back() ) {
            // last one ends in period
            *it += ".";
        } else {
            // The C version puts newlines before some of these for some reason
            *it += ";\n";
        }
        // *it += (&*it == &m_DBSource.back() ? "." : "\n;");
    }

    // hack to match C's whitespace
    if( ! containsHostLine ) {
        m_DBSource.front() += "\n";
    }
}

static void s_FormatDate(const CDate& date, string& str)
{
    CTime time = date.AsCTime();
    str += time.AsString(CTimeFormat("b d, Y"));
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
        m_DBSource.push_back("extra accessions:"
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
        //sp.GetCreated().GetDate(&s, "%3N %D %Y");
        s_FormatDate(sp.GetCreated(), s);
        m_DBSource.push_back(s + '.');
    }
    if (sp.CanGetSequpd()) {
        string s("sequence updated: ");
        //sp.GetSequpd().GetDate(&s, "%3N %D %Y");
        s_FormatDate(sp.GetSequpd(), s);
        m_DBSource.push_back(s + '.');
    }
    if (sp.CanGetAnnotupd()) {
        string s("annotation updated: ");
        //sp.GetAnnotupd().GetDate(&s, "%3N %D %Y");
        s_FormatDate(sp.GetAnnotupd(), s);
        m_DBSource.push_back(s + '.');
    }
    if (sp.CanGetSeqref()  &&  !sp.GetSeqref().empty() ) {
        list<string> xrefs;
        ITERATE (CSP_block::TSeqref, it, sp.GetSeqref()) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(**it);
            CSeq_id_Handle best = sequence::GetId(idh, ctx.GetScope(),
                                                  sequence::eGetId_Best);
            if ( !best ) {
                best = idh;
            }
            if (best) {
                string acc = best.GetSeqId()->GetSeqIdString(true);
                xrefs.push_back(acc);
            }
            /**
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
            **/
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
            string db = (*it)->GetDb();
            if ( db == "MIM") {
                if (ctx.Config().DoHTML()) {
                    xrefs.push_back
                        ("MIM <a href=\""
                         "https://www.ncbi.nlm.nih.gov/entrez/dispomim.cgi?id=" + id
                         + "\">" + id + "</a>");
                } else {
                    xrefs.push_back("MIM:" + id);
                }
            } else {
                // For exmaple, HGNC has HGNC as part of its identifier, so we may need to eliminate 
                // such redundancies (example accession: Q02094.1)
                if( id.substr(0, db.length() + 1) == (db + ":") ) {
                    xrefs.push_back(id); // in this case, id already has db at beginning
                } else {
                    xrefs.push_back(db + ':' + id); // no space(!)
                }
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
            m_DBSource.push_back("host:" + es.GetHost());
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
        s_FormatDate(pdb.GetDeposition(), s);
        m_DBSource.push_back(s);
    }}
    m_DBSource.push_back("class: " + pdb.GetClass());
    if (!pdb.GetSource().empty() ) {
        m_DBSource.push_back("source: " + x_FormatPDBSource(pdb));
    }
    if (pdb.CanGetExp_method()) {
        m_DBSource.push_back("Exp. method: " + pdb.GetExp_method());
    }
    if (pdb.CanGetReplace()) {
        const CPDB_replace& rep = pdb.GetReplace();
        if ( !rep.GetIds().empty() ) {
            m_DBSource.push_back
                ("ids replaced: " + x_FormatPDBSource(pdb));
        }
        string s("replacement date: ");
        DateToString(rep.GetDate(), s);
        m_DBSource.push_back(s);
    }
    NON_CONST_ITERATE (list<string>, it, m_DBSource) {
        *it += (&*it == &m_DBSource.back() ? '.' : ';');
    }
}


string CDBSourceItem::x_FormatDBSourceID(const CSeq_id_Handle& idh)
{
    const bool is_html = ( GetContext()->Config().DoHTML() );

    CConstRef<CSeq_id> id;
    if (idh) {
        id = idh.GetSeqId();
    }
    if (!id) {
        return kEmptyStr;
    }

    CSeq_id::E_Choice choice = id->Which();

    switch (choice) {
    case CSeq_id::e_Local:
        {{
            const CObject_id& oi = id->GetLocal();
            return (oi.IsStr() ? oi.GetStr() : NStr::IntToString(oi.GetId()));
        }}
    case CSeq_id::e_Gi:
        {{
            return "gi: " + NStr::NumericToString(id->GetGi());
        }}
    case CSeq_id::e_Pdb:
        {{
            const CPDB_seq_id& pdb = id->GetPdb();
            string s("pdb: "), sep;
            if ( !pdb.GetMol().Get().empty() ) {
                s += "molecule " + pdb.GetMol().Get();
                sep = ", ";
            }
            if (pdb.GetChain() > 0) {
                s += sep + "chain " + NStr::IntToString(pdb.GetChain());
                sep = ", ";
            }
            if (pdb.CanGetRel()) {
                s += sep + "release ";
                s_FormatDate(pdb.GetRel(), s);
                sep = ", ";
            }
            return s;
        }}
    default:
        {{
            const CTextseq_id* tsid = id->GetTextseq_Id();
            if (tsid == NULL) {
                return kEmptyStr;
            }
            string s, sep, comma;
            switch (choice) {
            case CSeq_id::e_Embl:       s = "embl ";        comma = ",";  break;
            case CSeq_id::e_Other:      s = "REFSEQ: ";                   break;
            case CSeq_id::e_Swissprot:  s = "UniProtKB: ";  comma = ",";  break;
            case CSeq_id::e_Pir:        s = "UniProtKB: ";                break;
            case CSeq_id::e_Prf:        s = "prf: ";                      break;
            default:                    break;
            }
            if (tsid->CanGetName()) {
                s += "locus " + tsid->GetName();
                sep = " ";
            } else {
                comma.erase();
            }
            if (tsid->CanGetAccession()) {
                string acc = tsid->GetAccession();
                if (tsid->CanGetVersion()  &&
                    choice != CSeq_id::e_Swissprot) {
                    acc += '.' + NStr::IntToString(tsid->GetVersion());
                }
                if( is_html ) {
                    const TIntId gi = GetContext()->GetScope().GetGi(idh);
                    s += comma + sep + "accession <a href=\"" + strLinkBaseNuc +
                        NStr::NumericToString(gi) + "\">" + acc + "</a>";
                } else {
                    s += comma + sep + "accession " + acc;
                }
                sep = " ";
            }
            /**
            if (tsid->CanGetRelease()) {
                s += sep + "release " + tsid->GetRelease();
            }
            **/
            if (id->IsSwissprot()) {
                s += ';';
            }
            return s;
        }}
    }

    return kEmptyStr;
}

string CDBSourceItem::x_FormatPDBSource(const CPDB_block& pdb)
{
    if( ! pdb.IsSetSource() || pdb.GetSource().empty() ) {
        return kEmptyStr;
    }

    const bool bIsHtml = ( GetContext() && GetContext()->Config().DoHTML() );

    string answer;
    const CPDB_block::TSource & source = pdb.GetSource();
    ITERATE( CPDB_block::TSource, source_iter, source ) {
        const string & a_source = *source_iter;
        if( ! answer.empty() ) {
            answer += ", ";
        }

        const static string kMmdbIdPrefix = "Mmdb_id:";
        string prefix;
        string url;
        string url_suffix;
        if( bIsHtml && x_ExtractLinkableSource(a_source, prefix, url, url_suffix) ) {
            answer += prefix;
            answer += " <a href=\"" + url + url_suffix + "\">";
            answer += url_suffix;
            answer += "</a>";
        } else {
            answer += a_source;
        }
    }

    return answer;
}

bool CDBSourceItem::x_ExtractLinkableSource(
    const string & a_source,
    string & out_prefix,
    string & out_url,
    string & out_url_suffix )
{
    const static struct {
        string m_prefix;
        string m_url;
        bool   m_must_be_all_digits;
    } potentialPrefixes[] = {
        { "Mmdb_id:", "https://www.ncbi.nlm.nih.gov/Structure/mmdb/mmdbsrv.cgi?uid=", true }
    };

    const static size_t numPotentialPrefixes = sizeof(potentialPrefixes)/sizeof(potentialPrefixes[0]);
    
    for( size_t idx = 0; idx < numPotentialPrefixes; ++idx ) {
        const string & prefix = potentialPrefixes[idx].m_prefix;
        const string & url = potentialPrefixes[idx].m_url;
        const bool must_be_all_digits = potentialPrefixes[idx].m_must_be_all_digits;

        if( a_source.length() <= prefix.length() ) {
            continue;
        }

        if( ! NStr::StartsWith(a_source, prefix, NStr::eNocase) ) {
            continue;
        }

        // first_non_space_pos points to first non-space character after the prefix.
        string::size_type first_non_space_pos = prefix.length();
        for( ; first_non_space_pos < a_source.length(); ++first_non_space_pos ) {
            if( ! isspace(a_source[first_non_space_pos]) ) {
                break;
            }
        }
        if( first_non_space_pos >= a_source.length() ) {
            continue;
        }

        // some require extra test to make sure they're all digits
        if( must_be_all_digits ) {
            bool non_digit_found = false;
            string::size_type test_pos = first_non_space_pos;
            for( ; test_pos < a_source.length(); ++test_pos ) {
                if( ! isdigit(a_source[test_pos]) ) {
                    non_digit_found = true;
                    break;
                }
            }
            if( non_digit_found ) {
                continue;
            }
        }

        // all tests passed, so prepare to give result to caller
        out_prefix = prefix;
        out_url = url;
        out_url_suffix = NStr::TruncateSpaces(a_source.substr(first_non_space_pos));
        return true;
    }

    // didn't find any matches
    return false;
}


END_SCOPE(objects)
END_NCBI_SCOPE
