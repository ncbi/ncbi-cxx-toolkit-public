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
*          Mati Shomrat
*
* File Description:
*
*
*/
#include <ncbi_pch.hpp>
#include <sstream>
#include <corelib/ncbistd.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/error_codes.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objtools/format/genbank_formatter.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/ctrl_items.hpp>
#include <objtools/format/items/version_item.hpp>
#include <objtools/format/items/dbsource_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/wgs_item.hpp>
#include <objtools/format/items/tsa_item.hpp>
#include <objtools/format/items/primary_item.hpp>
#include <objtools/format/items/contig_item.hpp>
#include <objtools/format/items/genome_item.hpp>
#include <objtools/format/items/origin_item.hpp>
#include <objtools/format/items/gap_item.hpp>
#include <objtools/format/items/genome_project_item.hpp>
#include <objtools/format/items/html_anchor_item.hpp>
#include <objtools/format/context.hpp>
#include <objtools/format/ostream_text_ostream.hpp>
#include <objmgr/util/objutil.hpp>

#include <algorithm>
#include <stdio.h>


#define NCBI_USE_ERRCODE_X   Objtools_Fmt_Genbank

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CGenbankFormatter::CGenbankFormatter(void) :
    m_uFeatureCount( 0 ), m_bHavePrintedSourceFeatureJavascript(false),
    m_bSourceDescriptorDone(false)
{
    SetIndent(string(12, ' '));
    SetFeatIndent(string(21, ' '));
    SetBarcodeIndent(string(35, ' '));
}


///////////////////////////////////////////////////////////////////////////
//
// END SECTION

namespace {

    // forwards AddParagraph, etc. to the underlying IFlatTextOStream but also
    // keeps a copy for itself to give to the blockcallback when the dtor is called.
    template<class TFlatItemClass>
    class CWrapperForFlatTextOStream : public IFlatTextOStream {
    public:
        CWrapperForFlatTextOStream(
            CRef<CFlatFileConfig::CGenbankBlockCallback> block_callback,
            IFlatTextOStream& orig_text_os,
            CRef<CBioseqContext> ctx,
            const TFlatItemClass& item ) :
        m_block_callback(block_callback),
            m_orig_text_os(orig_text_os),
            m_ctx(ctx),
            m_item(item)
        {
            m_Flushed = false;
        }

        ~CWrapperForFlatTextOStream()
        {
            if ( !m_Flushed ) {
                try {
                    Flush();
                    ERR_POST_X(1, Warning << "Flatfile output left unflushed in "
                               << CStackTrace());
                } catch (CFlatException& ) {
                    ERR_POST_X(2, "Missed flatfile output halt request in "
                               << CStackTrace());
                }
            }
        }

        void Flush(void)
        {
            m_Flushed = true;
            CFlatFileConfig::CGenbankBlockCallback::EAction eAction =
                m_block_callback->notify(m_block_text_str, *m_ctx, m_item);
            switch(eAction) {
            case CFlatFileConfig::CGenbankBlockCallback::eAction_HaltFlatfileGeneration:
                NCBI_THROW(CFlatException, eHaltRequested,
                    "A CGenbankBlockCallback has requested that flatfile generation halt");
                break;
            case CFlatFileConfig::CGenbankBlockCallback::eAction_Skip:
                // don't show this block
                break;
            default:
                // normal case: just print the string we got back
                m_orig_text_os.AddLine(m_block_text_str, nullptr, eAddNewline_No);
                break;
            }
        }

        virtual void AddParagraph(const list< string > &text, const CSerialObject *obj)
        {
            size_t add_size = m_block_text_str.size();
            ITERATE(list<string>, line, text) {
                add_size += (line->size() + 1);
            }
            m_block_text_str.reserve(max(m_block_text_str.capacity(), add_size));
            ITERATE(list<string>, line, text) {
                AddLine(*line, obj, eAddNewline_Yes);
            }
        }

        virtual void AddLine( const CTempString &line, const CSerialObject *obj,
            EAddNewline add_newline )
        {
            m_block_text_str.reserve(max(m_block_text_str.capacity(),
                m_block_text_str.length() +
                line.size() +
                (add_newline == eAddNewline_Yes?1:0) ) );
            m_block_text_str.append(line.data(), line.size());
            if( add_newline == eAddNewline_Yes ) {
                m_block_text_str += '\n';
            }
        }

    private:

        CRef<CFlatFileConfig::CGenbankBlockCallback> m_block_callback;
        IFlatTextOStream& m_orig_text_os;
        CRef<CBioseqContext> m_ctx;
        const TFlatItemClass& m_item;

        // build the block text here
        string m_block_text_str;

        bool m_Flushed;
    };

    template<class TFlatItemClass>
    IFlatTextOStream &s_WrapOstreamIfCallbackExists(
        CRef<IFlatTextOStream> & p_text_os, // note: reference to CRef
        const TFlatItemClass& item,
        IFlatTextOStream& orig_text_os)
    {
        // check if there's a callback, because we need to wrap if so
        CRef<CFlatFileConfig::CGenbankBlockCallback> block_callback =
            item.GetContext()->Config().GetGenbankBlockCallback();
        if( block_callback ) {
            CRef<CBioseqContext> ctx( item.GetContext() );
            p_text_os.Reset( new CWrapperForFlatTextOStream<TFlatItemClass>(
                block_callback, orig_text_os, ctx, item) );
            return *p_text_os;
        } else {
            return orig_text_os;
        }
    }
}

static
void s_PrintLocAsJavascriptArray(
    CBioseqContext &ctx,
    CNcbiOstream& text_os,
    const CSeq_loc &loc )
{
    CBioseq_Handle &bioseq_handle = ctx.GetHandle();

    ostringstream result; // will hold complete printed location
    result << "[";

    // special case for when the location is just a point with "lim tr"
    // ( This imitates C.  Not sure why C does this. )
    if( loc.IsPnt() &&
        loc.GetPnt().IsSetFuzz() &&
        loc.GetPnt().GetFuzz().IsLim() &&
        loc.GetPnt().GetFuzz().GetLim() == CInt_fuzz::eLim_tr )
    {
        const TSeqPos point = loc.GetPnt().GetPoint();
        // Note the "+2"
        result << "[" << (point+1) << ", " << (point+2) << "]]";
        text_os << result.str();
        return;
    }

    bool is_first = true;
    CSeq_loc_CI loc_piece_iter( loc, CSeq_loc_CI::eEmpty_Skip, CSeq_loc_CI::eOrder_Biological );
    for( ; loc_piece_iter ; ++loc_piece_iter ) {

        CSeq_id_Handle seq_id_handle = loc_piece_iter.GetSeq_id_Handle();
        
#if 0   // ID-7625 : this condition is not necessary. It prevents locations from being properly
        // included when features are annotated on components of CON sequences. 
        if( seq_id_handle && bioseq_handle && ! bioseq_handle.IsSynonym(seq_id_handle) ) {
            continue;
        }
#endif

        if( ! is_first ) {
            result << ",";
        }

        TSeqPos from = loc_piece_iter.GetRange().GetFrom();
        TSeqPos to = loc_piece_iter.GetRange().GetTo();
        if( (to == kMax_UInt || to == (kMax_UInt-1)) && bioseq_handle.CanGetInst_Length() ) {
            to = (bioseq_handle.GetInst_Length() - 1);
        }

        // reverse from and to if minus strand
        if( loc_piece_iter.IsSetStrand() &&
            loc_piece_iter.GetStrand() == eNa_strand_minus )
        {
            swap( from, to );
        }

        result << "[" <<  (from + 1) << ", " << (to + 1) << "]";

        is_first = false;
    }
    result << "]";
    text_os << result.str();
}

static
string s_GetAccessionWithoutPeriod(
    const CBioseqContext &ctx )
{
    string accn = ctx.GetAccession();
    SIZE_TYPE period_pos = accn.find_first_of(".");
    if( period_pos != NPOS ) {
        accn.resize(period_pos);
    }

    return accn;
}

static string s_get_anchor_html(const string & sAnchorName, CBioseqContext *ctx )
{
    ostringstream result;

    result << "<a name=\"" << sAnchorName << "_"
        << ctx->GetAccession() << "\"></a>";

    return result.str();
}

void CGenbankFormatter::EndSection
(const CEndSectionItem& end_item,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, end_item, orig_text_os);

    // print the double-slashes
    const CFlatFileConfig& cfg = GetContext().GetConfig();
    const bool bHtml = cfg.DoHTML();
    list<string> l;

    if ( bHtml ) {
        l.push_back( "//</pre>" );
    }
    else {
        l.push_back("//");
    }
    text_os.AddParagraph(l);

    if( bHtml && cfg.IsModeEntrez() ) {
        text_os.AddLine(
            s_get_anchor_html("slash", end_item.GetContext()),
            nullptr, IFlatTextOStream::eAddNewline_No);
    }

    text_os.Flush();

    // New record, so reset
    m_FeatureKeyToLocMap.clear();
    m_bHavePrintedSourceFeatureJavascript = false;
    m_bSourceDescriptorDone = false;
}


///////////////////////////////////////////////////////////////////////////
//
// Locus
//
// NB: The old locus line format is no longer supported for GenBank.
// (DDBJ will still show the old line format)

// Locus line format as specified in the GenBank release notes:
//
// Positions  Contents
// ---------  --------
// 01-05      'LOCUS'
// 06-12      spaces
// 13-28      Locus name
// 29-29      space
// 30-40      Length of sequence, right-justified
// 41-41      space
// 42-43      bp
// 44-44      space
// 45-47      spaces, ss- (single-stranded), ds- (double-stranded), or
//            ms- (mixed-stranded)
// 48-53      NA, DNA, RNA, tRNA (transfer RNA), rRNA (ribosomal RNA),
//            mRNA (messenger RNA), uRNA (small nuclear RNA), snRNA,
//            snoRNA. Left justified.
// 54-55      space
// 56-63      'linear' followed by two spaces, or 'circular'
// 64-64      space
// 65-67      The division code (see Section 3.3 in GenBank release notes)
// 68-68      space
// 69-79      Date, in the form dd-MMM-yyyy (e.g., 15-MAR-1991)

void CGenbankFormatter::FormatLocus
(const CLocusItem& locus,
 IFlatTextOStream& orig_text_os)
{
    static const char* strands[]  = { "   ", "ss-", "ds-", "ms-" };

    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, locus, orig_text_os);

    CBioseqContext& ctx = *locus.GetContext();

    list<string> l;
    ostringstream locus_line;

    const char* units = "bp";
    if ( !ctx.IsProt() ) {
        if ( ( ctx.IsWGSMaster() && ! ctx.IsRSWGSNuc() ) ||
            ctx.IsTSAMaster() || ctx.IsTLSMaster() )
        {
            units = "rc";
        }
    } else {
        units = "aa";
    }
    const char* topology = (locus.GetTopology() == CSeq_inst::eTopology_circular) ?  "circular" : "linear  ";

    const string& mol = s_GenbankMol[locus.GetBiomol()];

    const CFlatFileConfig& cfg = GetContext().GetConfig();

    locus_line.setf(IOS_BASE::left, IOS_BASE::adjustfield);

    const string& locusname =  cfg.LongLocusNames() ? locus.GetFullName() : locus.GetName();
    size_t locuslength = locusname.length();

    locus_line << setw(16) << locusname;
    // long LOCUS names may impinge on the length (e.g. gi 1449456)
    // I would consider this behavior conceptually incorrect; we should either fix the data
    // or truncate the locus names to 16 chars.  This is done here as a temporary measure
    // to make the asn2gb and asn2flat diffs match.
    // Note: currently this still cannot handle very long LOCUS names (e.g. in gi 1449821)
    int spaceForLength = min( 12, (int)(12 - (locuslength - 16))  );
    locus_line.setf(IOS_BASE::right, IOS_BASE::adjustfield);
    locus_line
        << ' '
        << setw(spaceForLength-1) << locus.GetLength()
        << ' '
        << units
        << ' '
        << strands[locus.GetStrand()];
    locus_line.setf(IOS_BASE::left, IOS_BASE::adjustfield);
    locus_line
        << setw(6) << mol
        << "  "
        << topology
        << ' '
        << locus.GetDivision()
        << ' '
        << locus.GetDate();

    const bool is_html = GetContext().GetConfig().DoHTML() ;

    string locus_line_str = locus_line.str();
    if ( is_html ) {
        TryToSanitizeHtml( locus_line_str );
    }
    Wrap(l, GetWidth(), "LOCUS", locus_line_str );
    if ( is_html ) {
        x_LocusHtmlPrefix( *l.begin(), ctx );
    }

    text_os.AddParagraph(l, locus.GetObject());

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// Definition

void CGenbankFormatter::FormatDefline
(const CDeflineItem& defline,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, defline, orig_text_os);

    list<string> l;
    string defline_text = defline.GetDefline();
    if( GetContext().GetConfig().DoHTML() ) {
        TryToSanitizeHtml(defline_text);
    }
    Wrap(l, "DEFINITION", defline_text);

    text_os.AddParagraph(l, defline.GetObject());

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// Accession

void CGenbankFormatter::FormatAccession
(const CAccessionItem& acc,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, acc, orig_text_os);

    string acc_str = x_FormatAccession(acc, ' ');
    string link_base = (acc.IsNuc() ? strLinkBaseNuc : strLinkBaseProt);
    string acc_line;
    if( acc.GetContext()->Config().DoHTML() && ! acc.GetContext()->GetLocation().IsWhole() ) {
        acc_line = "<a href=\"";
        acc_line += link_base + acc_str + "\">" + acc_str + "</a>";
    } else {
        acc_line = acc_str;
    }
    if ( acc.IsSetRegion() ) {
        acc_line += " REGION: ";
        acc_line += CFlatSeqLoc(acc.GetRegion(), *acc.GetContext()).GetString();
    }
    list<string> l;
    if (NStr::IsBlank(acc_line)) {
        l.push_back("ACCESSION   ");
    } else {
        if( acc.GetContext()->Config().DoHTML() ) {
            TryToSanitizeHtml( acc_line );
        }
        Wrap(l, "ACCESSION", acc_line);
    }
    text_os.AddParagraph(l, acc.GetObject());

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// Version

void CGenbankFormatter::FormatVersion
(const CVersionItem& version,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, version, orig_text_os);

    list<string> l;
    ostringstream version_line;

    if ( version.GetAccession().empty() ) {
        l.push_back("VERSION");
    } else {
        version_line << version.GetAccession();
        if ( version.GetGi() > ZERO_GI ) {
            const CFlatFileConfig& cfg = GetContext().GetConfig();
            if (! (cfg.HideGI() || ( cfg.IsPolicyFtp() || cfg.IsPolicyGenomes() /* && ctx.IsRefSeq() */ ) )) {
                version_line << "  GI:" << version.GetGi();
            }
        }
        string version_line_str = version_line.str();
        if( version.GetContext()->Config().DoHTML() ) {
            TryToSanitizeHtml( version_line_str );
        }
        Wrap(l, GetWidth(), "VERSION", version_line_str );
    }

    text_os.AddParagraph(l, version.GetObject());

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////////
//
// Genome Project

void CGenbankFormatter::FormatGenomeProject(
    const CGenomeProjectItem& gp,
    IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, gp, orig_text_os);

    list<string> l;
    const char *prefix = "DBLINK";

    if ( ! gp.GetProjectNumbers().empty() ) {

        ostringstream project_line;
        project_line << "Project: ";

        const bool is_html = GetContext().GetConfig().DoHTML();
        ITERATE( vector<int>, proj_num_iter, gp.GetProjectNumbers() ) {
            // put ", " before all but first
            if( proj_num_iter != gp.GetProjectNumbers().begin() ) {
                project_line << ", ";
            }

            const int proj_num = *proj_num_iter;
            if( is_html ) {
                project_line << "<a href=\"" << strLinkBaseGenomePrj << proj_num << "\">" <<
                    proj_num << "</a>";
            } else {
                project_line << proj_num;
            }
        }

        string project_line_str = project_line.str();
        if( gp.GetContext()->Config().DoHTML() ) {
            TryToSanitizeHtml( project_line_str );
        }
        Wrap(l, GetWidth(), prefix, project_line_str );
        prefix = kEmptyCStr;
    }

    ITERATE( CGenomeProjectItem::TDBLinkLineVec, it, gp.GetDBLinkLines() ) {
        string line = *it;
        if( gp.GetContext()->Config().DoHTML() ) {
            TryToSanitizeHtml( line );
        }
        Wrap(l, GetWidth(), prefix, line );
        prefix = kEmptyCStr;
    }

    if( ! l.empty() ) {
        text_os.AddParagraph(l, gp.GetObject());
    }

    text_os.Flush();
}

///////////////////////////////////////////////////////////////////////////
//
// HTML Anchor

void CGenbankFormatter::FormatHtmlAnchor(
    const CHtmlAnchorItem& html_anchor,
    IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, html_anchor, orig_text_os);

    text_os.AddLine(s_get_anchor_html(html_anchor.GetLabelCore(), html_anchor.GetContext()),
        nullptr, IFlatTextOStream::eAddNewline_No);
}

///////////////////////////////////////////////////////////////////////////
//
// String Cache

void CGenbankFormatter::FormatCache
(const CCacheItem& csh,
 IFlatTextOStream& text_os)
{
    if ( csh.Skip() ) {
        return;
    }

    vector<string>* rcx = csh.GetCache();
    if (rcx) {
        int length = csh.GetLength();
        string suffix = NStr::NumericToString(length) + ")";
        string prefix = (csh.IsProt() ? "residues" : "bases");
        for (auto& str : *rcx) {
            if (NStr::StartsWith (str, "REFERENCE ") && NStr::EndsWith (str, ")")) {
                size_t pos = NStr::Find(str, " (");
                if (pos > 10 && NStr::Find(str, "sites") == NPOS) {
                    text_os.AddLine(str.substr(0, pos + 2) + prefix + " 1 to " + suffix);
                    continue;
                }
            }
            text_os.AddLine(str);
        }
    }
}


///////////////////////////////////////////////////////////////////////////
//
// Keywords

void CGenbankFormatter::FormatKeywords
(const CKeywordsItem& keys,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, keys, orig_text_os);

    list<string> l;
    x_GetKeywords(keys, "KEYWORDS", l);
    if( keys.GetContext()->Config().DoHTML() ) {
        TryToSanitizeHtmlList(l);
    }
    text_os.AddParagraph(l, keys.GetObject());

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// Segment

void CGenbankFormatter::FormatSegment
(const CSegmentItem& seg,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, seg, orig_text_os);

    list<string> l;
    ostringstream segment_line;

    segment_line << seg.GetNum() << " of " << seg.GetCount();

    Wrap(l, "SEGMENT", segment_line.str());
    text_os.AddParagraph(l, seg.GetObject());

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// Source

// SOURCE + ORGANISM

void CGenbankFormatter::FormatSource
(const CSourceItem& source,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, source, orig_text_os);

    list<string> l;
    x_FormatSourceLine(l, source);
    x_FormatOrganismLine(l, source);
    text_os.AddParagraph(l, source.GetObject());

    text_os.Flush();
}


void CGenbankFormatter::x_FormatSourceLine
(list<string>& l,
 const CSourceItem& source) const
{
    ostringstream source_line;

    string prefix = source.IsUsingAnamorph() ? " (anamorph: " : " (";

    source_line << source.GetOrganelle() << source.GetTaxname();
    if ( !source.GetCommon().empty() ) {
        source_line << prefix << source.GetCommon() << ")";
    }
    string line = source_line.str();

    if( source.GetContext()->Config().DoHTML() ) {
        TryToSanitizeHtml(line);
    }
    Wrap(l, GetWidth(), "SOURCE", line,
        ePara, source.GetContext()->Config().DoHTML() );
}


void CGenbankFormatter::x_FormatOrganismLine(list<string>& l, const CSourceItem& source) const
{
    // taxname
    string s;
    GetContext().GetConfig().GetHTMLFormatter().FormatTaxid(s, source.GetTaxid(), source.GetTaxname());
    Wrap(l, "ORGANISM", s, eSubp);
    // lineage
    if (source.GetContext()->Config().DoHTML()) {
        string lineage = source.GetLineage();
        TryToSanitizeHtml( lineage );
        Wrap(l, kEmptyStr, lineage, eSubp);
    } else {
        Wrap(l, kEmptyStr, source.GetLineage(), eSubp);
    }
}


///////////////////////////////////////////////////////////////////////////
//
// REFERENCE

// The REFERENCE field consists of five parts: the keyword REFERENCE, and
// the subkeywords AUTHORS, TITLE (optional), JOURNAL, MEDLINE (optional),
// PUBMED (optional), and REMARK (optional).

string s_GetLinkCambiaPatentLens( const CReferenceItem& ref, bool bHtml )
{
    const string strBaseUrlCambiaPatentLensHead(
        "https://www.lens.org/lens/search/patent/list?q=" );

    if ( ! ref.IsSetPatent() ) {
        return "";
    }
    const CCit_pat& pat = ref.GetPatent();

    if ( ! pat.CanGetCountry()  ||  pat.GetCountry() != "US"  ||
        ! pat.CanGetNumber() )
    {
        return "";
    }

    string strPatString;
    if ( bHtml ) {
        strPatString = "CAMBIA Patent Lens: US ";
        strPatString += "<a href=\"";
        strPatString += strBaseUrlCambiaPatentLensHead;
        strPatString += pat.GetCountry();
        strPatString += "%20";
        strPatString += pat.GetNumber();
        strPatString += "\">";
        strPatString += pat.GetNumber();
        strPatString += "</a>";
    }
    else {
        strPatString = string( "CAMBIA Patent Lens: US " );
        strPatString += pat.GetNumber();
    }
    return strPatString;
}

//  ============================================================================
string s_GetLinkFeature(
    const CReferenceItem& ref,
    bool bHtml )
//  ============================================================================
{
    string strFeatureLink;
    return strFeatureLink;
}

void CGenbankFormatter::FormatReference
(const CReferenceItem& ref,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, ref, orig_text_os);

    CBioseqContext& ctx = *ref.GetContext();

    list<string> l;

    x_Reference(l, ref, ctx);
    x_Authors(l, ref, ctx);
    x_Consortium(l, ref, ctx);
    x_Title(l, ref, ctx);
    x_Journal(l, ref, ctx);
    if (ref.GetPMID() == ZERO_ENTREZ_ID) {  // suppress MEDLINE if has PUBMED
        x_Medline(l, ref, ctx);
    }
    x_Pubmed(l, ref, ctx);
    x_Remark(l, ref, ctx);

    if( ctx.Config().DoHTML() ) {
        TryToSanitizeHtmlList(l);
    }

    text_os.AddParagraph(l, ref.GetObject());

    vector<string>* rc = ctx.GetRefCache();
    if (rc) {
        for (auto str : l) {
            rc->push_back(str);
        }
    }

    text_os.Flush();
}


// Find bare links in the text and replace them with clickable links.
// E.g.
// http://www.example.com
// becomes
// <a href="http://www.example.com">http://www.example.com</a>
void s_GenerateWeblinks( const string& strProtocol, string& strText )
{
    const string strDummyProt( "<!PROT!>" );

    size_t uLinkStart = NStr::FindNoCase( strText, strProtocol + "://" );
    while ( uLinkStart != NPOS ) {
        size_t uLinkStop = strText.find_first_of( " \t\n", uLinkStart );
        if( uLinkStop == NPOS ) {
            uLinkStop = strText.length();
        }

        // detect if this link is already embedded in an href HTML tag so we don't
        // "re-embed" it, producing bad HTML.
        if( uLinkStart > 0 && ( strText[uLinkStart-1] == '"' || strText[uLinkStart-1]  == '>' )  ) {
            uLinkStart = NStr::FindNoCase( strText, strProtocol + "://", uLinkStop );
            continue;
        }

        string strLink = strText.substr( uLinkStart, uLinkStop - uLinkStart );
        // remove junk
        string::size_type last_good_char = strLink.find_last_not_of("\".),<>'");
        if( last_good_char != NPOS ) {
            strLink.resize( last_good_char + 1 );
        }

        string strDummyLink = NStr::Replace( strLink, strProtocol, strDummyProt );
        string strReplace( "<a href=\"" );
        strReplace += strDummyLink;
        strReplace += "\">";
        strReplace += strDummyLink;
        strReplace += "</a>";

        NStr::ReplaceInPlace( strText, strLink, strReplace, uLinkStart, 1 );
        uLinkStart = NStr::FindNoCase( strText, strProtocol + "://", uLinkStart + strReplace.length() );
    }
    NStr::ReplaceInPlace( strText, strDummyProt, strProtocol );
}

// The REFERENCE line contains the number of the particular reference and
// (in parentheses) the range of bases in the sequence entry reported in
// this citation.
void CGenbankFormatter::x_Reference
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    ostringstream ref_line;

    int serial = ref.GetSerial();
    CPubdesc::TReftype reftype = ref.GetReftype();

    // print serial
    if (serial > 99) {
        ref_line << serial << ' ';
    } else if (reftype ==  CPubdesc::eReftype_no_target) {
        ref_line << serial;
    } else {
        ref_line.setf(IOS_BASE::left, IOS_BASE::adjustfield);
        ref_line << setw(3) << serial;
    }

    // print sites or range
    if ( reftype == CPubdesc::eReftype_sites  ||
         reftype == CPubdesc::eReftype_feats ) {
        ref_line << "(sites)";
    } else if ( reftype == CPubdesc::eReftype_no_target ) {
        // do nothing
    } else {
        x_FormatRefLocation(ref_line, ref.GetLoc(), " to ", "; ", ctx);
    }
    string ref_line_str = ref_line.str();
    if( ref.GetContext()->Config().DoHTML() ) {
        TryToSanitizeHtml( ref_line_str );
    }
    Wrap(l, GetWidth(), "REFERENCE", ref_line_str );
}


void CGenbankFormatter::x_Authors
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    string authors;
    if (ref.IsSetAuthors()) {
        CReferenceItem::FormatAuthors(ref.GetAuthors(), authors);
        if (NStr::EqualNocase(authors, "?")) {
            authors = ".";
        }
    }
    if( authors.empty() ) {
        if( NStr::IsBlank(ref.GetConsortium()) ) {
            if( ctx.Config().IsFormatGenbank() ) {
                Wrap(l, "AUTHORS", ".", eSubp);
            } else if( ctx.Config().IsFormatEMBL() ) {
                Wrap(l, "AUTHORS", ";", eSubp);
            }
        }
        return;
    }
    // chop off extra periods at the end (e.g. AAA16431)
    string::size_type last_periods = authors.find_last_not_of('.');
    if( last_periods != string::npos ) {
        last_periods += 2; // point to the first period that we should remove
        if( last_periods < authors.length() ) {
            authors.resize( last_periods );
        }
    }
    if (!NStr::EndsWith(authors, '.')) {
        authors += '.';
    }
    CleanAndCompress(authors, authors.c_str());
    if( ref.GetContext()->Config().DoHTML() ) {
        TryToSanitizeHtml( authors );
    }
    Wrap(l, "AUTHORS", authors, eSubp);
}


void CGenbankFormatter::x_Consortium
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    if (!NStr::IsBlank(ref.GetConsortium())) {
        string consortium = ref.GetConsortium();
        if( ref.GetContext()->Config().DoHTML() ) {
            TryToSanitizeHtml( consortium );
        }
        Wrap(l, "CONSRTM", consortium, eSubp);
    }
}


void CGenbankFormatter::x_Title
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    if (!NStr::IsBlank(ref.GetTitle())) {
        string title = ref.GetTitle();
        if( ref.GetContext()->Config().DoHTML() ) {
            TryToSanitizeHtml( title );
        }
        Wrap(l, "TITLE", title,   eSubp);
    }
}


void CGenbankFormatter::x_Journal
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    string journal;
    x_FormatRefJournal(ref, journal, ctx);

    if (!NStr::IsBlank(journal)) {
        if( ref.GetContext()->Config().DoHTML() ) {
            TryToSanitizeHtml( journal );
        }
        Wrap(l, "JOURNAL", journal, eSubp);
    }
}


void CGenbankFormatter::x_Medline
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    bool bHtml = ctx.Config().DoHTML();

    string strDummy( "[PUBMED-ID]" );
    if ( ref.GetMUID() != ZERO_ENTREZ_ID) {
        Wrap(l, GetWidth(), "MEDLINE", strDummy, eSubp);
    }
    string strPubmed( NStr::NumericToString( ref.GetMUID() ) );
    if ( bHtml ) {
        string strLink = "<a href=\"";
        strLink += strLinkBasePubmed;
        strLink += strPubmed;
        strLink += "\">";
        strLink += strPubmed;
        strLink += "</a>";
        strPubmed = strLink;
    }
    NON_CONST_ITERATE( list<string>, it, l ) {
        NStr::ReplaceInPlace( *it, strDummy, strPubmed );
    }
}


void CGenbankFormatter::x_Pubmed
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    if ( ref.GetPMID() == ZERO_ENTREZ_ID) {
        return;
    }
    string strPubmed = NStr::NumericToString( ref.GetPMID() );
    if ( ctx.Config().DoHTML() ) {
        string strRaw = strPubmed;
        strPubmed = "<a href=\"https://www.ncbi.nlm.nih.gov/pubmed/";
        strPubmed += strRaw;
        strPubmed += "\">";
        strPubmed += strRaw;
        strPubmed += "</a>";
    }

    Wrap(l, " PUBMED", strPubmed, eSubp);
}


void CGenbankFormatter::x_Remark
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    const bool is_html = ctx.Config().DoHTML();

    if (!NStr::IsBlank(ref.GetRemark())) {
        if( is_html ) {
            string remarks = ref.GetRemark();
            TryToSanitizeHtml(remarks);
            s_GenerateWeblinks( "http", remarks );
            s_GenerateWeblinks( "https", remarks );
            Wrap(l, "REMARK", remarks, eSubp);
        } else {
            Wrap(l, "REMARK", ref.GetRemark(), eSubp);
        }
    }
    if ( ctx.Config().GetMode() == CFlatFileConfig::eMode_Entrez ) {
        if ( ref.IsSetPatent() ) {
            string strCambiaPatentLens = s_GetLinkCambiaPatentLens( ref,
                ctx.Config().DoHTML() );
            if ( ! strCambiaPatentLens.empty() ) {
                if( is_html ) {
                    s_GenerateWeblinks( "http", strCambiaPatentLens );
                    s_GenerateWeblinks( "https", strCambiaPatentLens );
                }
                Wrap(l, "REMARK", strCambiaPatentLens, eSubp);
            }
        }
    }
}

// This will change first_line to prepend HTML-relevant stuff.
void
CGenbankFormatter::x_LocusHtmlPrefix( string &first_line, CBioseqContext& ctx )
{
    // things are easy when we're not in entrez mode
    if( ! ctx.Config().IsModeEntrez() ) {
        first_line = "<pre>" + first_line;
        return;
    }

    ostringstream result;

    // determine what sections we have.

    // see if we do have a comment
    bool has_comment = false;
    {{
        CSeqdesc_CI desc_ci1( ctx.GetHandle(), CSeqdesc::e_Comment );
        CSeqdesc_CI desc_ci2( ctx.GetHandle(), CSeqdesc::e_Region );
        CSeqdesc_CI desc_ci3( ctx.GetHandle(), CSeqdesc::e_Maploc );
        if( desc_ci1 || desc_ci2 || desc_ci3 ) {
            has_comment = true;
        } else {
            // certain kinds of user objects make COMMENTs appear
            CSeqdesc_CI user_iter( ctx.GetHandle(), CSeqdesc::e_User );
            for( ; user_iter; ++user_iter ) {
                const CSeqdesc & desc = *user_iter;
                if( desc.GetUser().IsSetType() && desc.GetUser().GetType().IsStr() ) {
                    const string &type_str = desc.GetUser().GetType().GetStr();
                    if( type_str == "RefGeneTracking" ||
                        type_str == "GenomeBuild" ||
                        type_str == "ENCODE" )
                    {
                        has_comment = true;
                    }
                }
            }
        }

        // replaces or replaced-by can trigger comments, too
        if( ! has_comment ) {
            CBioseq_Handle bioseq = ctx.GetHandle();
            if( bioseq && bioseq.IsSetInst_Hist() ) {
                const CSeq_hist& hist = bioseq.GetInst_Hist();

                if ( hist.CanGetReplaced_by() ) {
                    const CSeq_hist::TReplaced_by& r = hist.GetReplaced_by();
                    if ( r.CanGetDate()  &&  !r.GetIds().empty() )
                    {
                        has_comment = true;
                    }
                }

                if ( hist.IsSetReplaces()  &&  !ctx.Config().IsModeGBench() ) {
                    const CSeq_hist::TReplaces& r = hist.GetReplaces();
                    if ( r.CanGetDate()  &&  !r.GetIds().empty() )
                    {
                        has_comment = true;
                    }
                }
            }
        }
    }}

    const CFlatFileConfig& cfg = ctx.Config();

    // see if we do have a contig
    bool has_contig = false;
    {{
        // we split the if-statement into little local vars for ease of reading
        const bool is_wgs_master = ( ctx.IsWGSMaster() && ctx.GetTech() == CMolInfo::eTech_wgs );
        const bool is_tsa_master = ( ctx.IsTSAMaster() && ctx.GetTech() == CMolInfo::eTech_tsa &&
                                     (ctx.GetBiomol() == CMolInfo::eBiomol_mRNA || ctx.GetBiomol() == CMolInfo::eBiomol_transcribed_RNA) );
        const bool do_contig_style = ctx.DoContigStyle();
        const bool show_contig = ( (ctx.IsSegmented()  &&  ctx.HasParts())  ||
                                   (ctx.IsDelta()  &&  ! ctx.IsDeltaLitOnly()) );
        if( ! is_wgs_master && ! is_tsa_master && (do_contig_style || ( ( cfg.ShowContigAndSeq() || ( ( cfg.IsPolicyFtp() || cfg.IsPolicyGenomes() ) && ctx.IsRefSeq() && ctx.IsProt() ) ) && show_contig )) ) {
            has_contig = true;
        }
    }}

    // see if we do have a sequence
    bool has_sequence = false;
    {{
        if( ! ctx.DoContigStyle() || cfg.ShowContigAndSeq() || ( ( cfg.IsPolicyFtp() || cfg.IsPolicyGenomes() ) && ctx.IsRefSeq() && ctx.IsProt() ) ) {
            has_sequence = true;
        }
    }}

    // list of links that let us jump to sections
    const string& accn = ctx.GetAccession();
    result << "<div class=\"localnav\"><ul class=\"locals\">";
    if( has_comment ) {
        result << "<li><a href=\"#comment_" << accn << "\" title=\"Jump to the comment section of this record\">Comment</a></li>";
    }
    result << "<li><a href=\"#feature_" << accn << "\" title=\"Jump to the feature table of this record\">Features</a></li>";
    if( has_contig ) {
        result << "<li><a href=\"#contig_" << accn << "\" title=\"Jump to the contig section of this record\">Contig</a></li>";
    }
    if( has_sequence ) {
        result << "<li><a href=\"#sequence_" << accn << "\" title=\"Jump to the sequence of this record\">Sequence</a></li>";
    }
    result << "</ul>";

    // prev & next links
    if( ctx.GetPrevHandle() || ctx.GetNextHandle() ) {
        result << "<ul class=\"nextprevlinks\">";
        if( ctx.GetNextHandle() ) {
            // TODO: check for NULL
            const TGi gi = ctx.GetNextHandle().GetAccessSeq_id_Handle().GetGi();
            const string accn = sequence::GetId( ctx.GetNextHandle(), sequence::eGetId_Best).GetSeqId()->GetSeqIdString(true);
            result << "<li class=\"next\"><a href=\"#locus_" << gi << "\" title=\"Jump to " << accn << "\">Next</a></li>";
        }
        if( ctx.GetPrevHandle() ) {
            // TODO: check for NULL
            const TGi gi = ctx.GetPrevHandle().GetAccessSeq_id_Handle().GetGi();
            const string accn = sequence::GetId( ctx.GetPrevHandle(), sequence::eGetId_Best).GetSeqId()->GetSeqIdString(true);
            result << "<li class=\"prev\"><a href=\"#locus_" << gi << "\" title=\"Jump to " << accn << "\">Previous</a></li>";
        }
        result << "</ul>";
    }

    // wrapping up here
    result << "</div>" << '\n';
    result << "<pre class=\"genbank\">";

    result << first_line;
    first_line = result.str();
}

void
CGenbankFormatter::x_GetFeatureSpanAndScriptStart(
    IFlatTextOStream& text_os,
    const CTempString& strKey,
    const CSeq_loc &feat_loc,
    CBioseqContext& ctx )
{
    // determine the count for this type, and push back
    // the new location
    // ( Note the post-increment )
    const int feat_type_count = ( m_FeatureKeyToLocMap[strKey]++ );

    // The span
    ostringstream pre_feature_html;
    pre_feature_html << "<span id=\"feature_" << ctx.GetAccession()
        << "_" << strKey << "_" << feat_type_count << "\" class=\"feature\">";

    // The javascript
    pre_feature_html << "<script type=\"text/javascript\">";

    // special treatment for source features
    if( NStr::Equal(strKey, "source") && ! m_bHavePrintedSourceFeatureJavascript ) {
        pre_feature_html << "if "
            "(typeof(oData) == \"undefined\") oData = []; oData.push "
            "({gi:" << ctx.GetGI() << ",acc:\""
            << s_GetAccessionWithoutPeriod(ctx)
            << "\",features: {}});";
        m_bHavePrintedSourceFeatureJavascript = true;
    }

    pre_feature_html
        << "if (!oData[oData.length - 1].features[\"" << strKey
        << "\"]) oData[oData.length - 1].features[\"" << strKey
        << "\"] = [];"
        << "oData[oData.length - 1].features[\"" << strKey << "\"].push(";
    s_PrintLocAsJavascriptArray( ctx, pre_feature_html, feat_loc );
    pre_feature_html << ");</script>";

    string temp = pre_feature_html.str();
    text_os.AddLine(temp, nullptr, IFlatTextOStream::eAddNewline_No);
}

///////////////////////////////////////////////////////////////////////////
//
// COMMENT

void s_OrphanFixup( list< string >& wrapped, size_t uMaxSize = 0 )
{
    if ( ! uMaxSize ) {
        return;
    }
    list< string >::iterator it = wrapped.begin();
    ++it;
    while ( it != wrapped.end() ) {
        string strContent = NStr::TruncateSpaces( *it );
        if ( strContent.size() && strContent.size() <= uMaxSize ) {
            --it;
            *it += strContent;
            list< string >::iterator delete_me = ++it;
            ++it;
            wrapped.erase( delete_me );
        }
        else {
            ++it;
        }
    }
}

//void s_FixLineBrokenWeblinks( list<string>& l )
//{
//}

static void
s_FixListIfBadWrap( list<string> &l, list<string>::iterator l_old_last,
                   int indent )
{
    // point to the first added line
    list<string>::iterator l_first_new_line;
    if( l_old_last != l.end() ) {
        l_first_new_line = l_old_last;
        ++l_first_new_line;
    } else {
        l_first_new_line = l.begin();
    }

    // no lines were added
    if( l_first_new_line == l.end() ) {
        return;
    }

    // find the line after it
    list<string>::iterator l_second_new_line = l_first_new_line;
    ++l_second_new_line;

    // only 1 new line added
    if( l_second_new_line == l.end() ) {
        return;
    }

    // if the first added line is too short, there must've been a problem,
    // so we join the first two lines together
    if( (int)l_first_new_line->length() <= indent ) {
        NStr::TruncateSpacesInPlace( *l_first_new_line, NStr::eTrunc_End );
        *l_first_new_line += " " + NStr::TruncateSpaces( *l_second_new_line );
        l.erase( l_second_new_line );
    }
}

void CGenbankFormatter::FormatComment
(const CCommentItem& comment,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, comment, orig_text_os);

    list<string> strComment( comment.GetCommentList() );
    const int internalIndent = comment.GetCommentInternalIndent();

    bool is_first = comment.IsFirst();

    list<string> l;
    NON_CONST_ITERATE( list<string>, comment_it, strComment ) {
        bool bHtml = GetContext().GetConfig().DoHTML();
        if ( bHtml ) {
            s_GenerateWeblinks( "http", *comment_it );
            s_GenerateWeblinks( "https", *comment_it );
        }

        list<string>::iterator l_old_last = l.end();
        if( ! l.empty() ) {
            --l_old_last;
        }

        if( bHtml ) {
            TryToSanitizeHtml(*comment_it);
        }

        string& comm = *comment_it;
        if (is_first) {
            Wrap(l, "COMMENT", comm, ePara, bHtml, internalIndent);
        } else {
            Wrap(l, kEmptyStr, comm, eSubp, bHtml, internalIndent);
        }

        // Sometimes Wrap gets overzealous and wraps us right after the "::"
        // for structured comments (e.g. FJ888345.1)
        if( internalIndent > 0 ) {
            s_FixListIfBadWrap( l, l_old_last, GetIndent().length() + internalIndent );
        }

        is_first = false;
    }

    //    if ( bHtml ) {
    //        s_FixLineBrokenWeblinks( l );
    //    }

    list<string> x;
    ITERATE(list<string>, line, l) {
        const string& txt = *line;
        if (NStr::Find(txt, "COMMENT") != NPOS) {
            x.push_back(txt);
        } else if (NStr::Find(txt, "START##") != NPOS) {
            x.push_back("            \n" + txt);
        } else if (NStr::Find(txt, "this sequence version replaced") != NPOS ||
                   NStr::Find(txt, "this sequence was replaced by") != NPOS ||
                   NStr::Find(txt, "this project was updated") != NPOS) {
            x.push_back("            \n" + txt);
        } else {
            x.push_back(txt);
        }
    }

    text_os.AddParagraph(x, comment.GetObject());

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// FEATURES

// Fetures Header

void CGenbankFormatter::FormatFeatHeader
(const CFeatHeaderItem& fh,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, fh, orig_text_os);

    list<string> l;

    Wrap(l, "FEATURES", "Location/Qualifiers", eFeatHead);

    text_os.AddParagraph(l);

    text_os.Flush();
}

//  ============================================================================
bool s_GetFeatureKeyLinkLocation(
    const CMappedFeat& feat,
    TGi& iGi,
    unsigned int& iFrom,                    // one based
    unsigned int& iTo )                     // one based
//  ============================================================================
{
    iGi = ZERO_GI;
    iFrom = iTo = 0;

    const CSeq_loc& loc = feat.GetLocation();

    if (iGi == ZERO_GI) {
        ITERATE( CSeq_loc, loc_iter, loc ) {
            CSeq_id_Handle idh = loc_iter.GetSeq_id_Handle();
            if ( idh && idh.IsGi() ) {
                CBioseq_Handle bioseq_h = feat.GetScope().GetBioseqHandle( idh );
                if( bioseq_h ) {
                    iGi = idh.GetGi();
                }
            }
        }
    }

    iFrom = loc.GetStart( eExtreme_Positional ) + 1;
    iTo = loc.GetStop( eExtreme_Positional ) + 1;
    return true;
}

//  ============================================================================
bool s_GetLinkFeatureKey(
    const CFeatureItemBase& item,
    const CFlatFeature& feat,
    const string& strRawKey,
    string& strLink,
    unsigned int uItemNumber = 0 )
//  ============================================================================
{
    if ( strRawKey == "gap" || strRawKey == "assembly_gap" )
    {
        return false;
    }

    // ID-7962 : The first "source" item comes from a descriptor, not a feature, and
    // hyperlink for it is not created.
    if ( strRawKey == "source" && uItemNumber == 0) {
        return false;
    }

    TGi iGi = ZERO_GI;
    unsigned int iFrom = 0, iTo = 0;
    s_GetFeatureKeyLinkLocation( item.GetFeat(), iGi, iFrom, iTo );
    if(iGi == ZERO_GI) {
        iGi = item.GetContext()->GetGI();
    }
    if ( iFrom == 0 && iFrom == iTo ) {
        return false;
    }

    // assembly of the actual string:
    strLink.reserve(100); // euristical URL length
    item.GetContext()->Config().GetHTMLFormatter().FormatLocation(strLink, item.GetFeat().GetLocation(), GI_TO(TIntId, iGi), strRawKey);
    return true;
}

class CGenbankFormatterWrapDest : public NStr::IWrapDest
{
public:
    CGenbankFormatterWrapDest(IFlatTextOStream& s) : m_text_os(s) {};
    IFlatTextOStream& m_text_os;
    virtual void Append(const string& s)
    {
        Append(CTempString(s));
    }
    virtual void Append(const CTempString& s)
    {
        CTempString t = NStr::TruncateSpaces_Unsafe(s, NStr::eTrunc_End);
        if (t.find_first_not_of(" ") == NPOS  &&  s.size() == 22) {
            t = s;
            t.erase(21);
        }
        m_text_os.AddLine(t);
    }
};

void CGenbankFormatter::x_SmartWrapQuals(const CFeatureItemBase& feat, const CFlatFeature& ff, IFlatTextOStream& text_os)
{
    // reuse some variable in the loop
    string prefix1;
    string value;
    string sanitized;

    const vector<CRef<CFormatQual> > & quals = ff.GetQuals();
    bool bHtml = feat.GetContext()->Config().DoHTML();

    ITERATE(vector<CRef<CFormatQual> >, it, quals) {
        const string& qual = (*it)->GetName();
        const string& prefix = GetFeatIndent();

        prefix1 = prefix;

        switch ((*it)->GetTrim()) {
        case CFormatQual::eTrim_Normal:
            TrimSpacesAndJunkFromEnds(value, (*it)->GetValue(), true);
            break;
        case CFormatQual::eTrim_WhitespaceOnly:
            value = NStr::TruncateSpaces_Unsafe((*it)->GetValue());
            break;
        default:
            value = (*it)->GetValue();
        }

        if (bHtml) {
            TryToSanitizeHtml(sanitized, value);
        }

        switch ((*it)->GetStyle()) {
        case CFormatQual::eEmpty:
            prefix1 += '/';
            if (bHtml) {
                sanitized = qual;
            } else {
                value = qual;
            }
            break;
        case CFormatQual::eQuoted:
            if (bHtml) sanitized += '"'; else value += '"';
            prefix1 += '/';
            prefix1 += qual;
            prefix1 += "=\"";
            break;
        case CFormatQual::eUnquoted:
            prefix1 += '/';
            prefix1 += qual;
            prefix1 += '=';
            break;
        }
        // Call NStr::Wrap directly to avoid unwanted line breaks right
        // before the start of the value (in /translation, e.g.)
#if 1
        CGenbankFormatterWrapDest dest(text_os);
        NStr::Wrap((bHtml) ? sanitized : value, GetWidth(), dest, SetWrapFlags(), &prefix, &prefix1);
#else
        NStr::Wrap((bHtml) ? sanitized : value, GetWidth(), l_new, SetWrapFlags(), prefix, prefix1);

        if (l_new.size() > 1) {
            const string &last_line = l_new.back();

            list<string>::const_iterator end_iter = l_new.end();
            end_iter--;
            end_iter--;
            const string &second_to_last_line = *end_iter;

            if (NStr::TruncateSpaces(last_line) == "\"" && second_to_last_line.length() < GetWidth()) {
                l_new.pop_back();
                l_new.back() += "\"";
            }
        }
        // Values of qualifiers coming down this path do not carry additional
        // internal format (at least, they aren't supposed to). So we strip extra
        // blanks from both the begin and the end of qualifier lines.
        // (May have to be amended once sizeable numbers of violators are found
        // in existing data).
        NON_CONST_ITERATE(list<string>, it, l_new) {
            //NStr::TruncateSpacesInPlace(*it, NStr::eTrunc_End);
            text_os.AddLine(NStr::TruncateSpaces_Unsafe(*it, NStr::eTrunc_End));
        }
        //l.insert(l.end(), l_new.begin(), l_new.end());
        //l_new.clear();
#endif
    }
}

//  ============================================================================
void CGenbankFormatter::FormatFeature
(const CFeatureItemBase& f,
 IFlatTextOStream& orig_text_os)
//  ============================================================================
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream* text_os = nullptr;

    {
        // this works differently from the others because we have to check
        // the underlying type
        const CSourceFeatureItem *p_source_feature_item =
            dynamic_cast<const CSourceFeatureItem *>(&f);
        if (p_source_feature_item) {
            text_os = &s_WrapOstreamIfCallbackExists(p_text_os, *p_source_feature_item, orig_text_os);
        }
        else
        {
            const CFeatureItem *p_feature_item =
                dynamic_cast<const CFeatureItem *>(&f);
            if (! text_os && p_feature_item) {
                text_os = &s_WrapOstreamIfCallbackExists(p_text_os, *p_feature_item, orig_text_os);
            }
        }
        _ASSERT(text_os);
    }

    bool bHtml = f.GetContext()->Config().DoHTML();

    CConstRef<CFlatFeature> feat = f.Format();

    // ID-7962 : Do not increment feature count for the source descriptor - this would tell the
    // s_GetLinkFeatureKey function below to not create a hyperlink.
    if ( feat->GetKey() != "source" || m_uFeatureCount > 0 || m_bSourceDescriptorDone)
        ++m_uFeatureCount;
    else
        m_bSourceDescriptorDone = true;

    const string& strKey = feat->GetKey();
    string fkey = strKey;
    if (NStr::EqualNocase(fkey, "propeptide")) {
        if (f.GetContext()->IsProt()) {
        }
        else if (f.GetContext()->IsRefSeq()) {
        }
        else if (f.GetContext()->Config().IsModeEntrez() || f.GetContext()->Config().IsModeRelease()) {
            // fkey = "misc_feature";
        }
    }
    // write <span...> and <script...> in HTML mode
    if (bHtml && f.GetContext()->Config().IsModeEntrez() && f.GetContext()->Config().ShowSeqSpans()) {
        x_GetFeatureSpanAndScriptStart(*text_os, fkey, f.GetLoc(), *f.GetContext());
    }

    list<string>        l;
    Wrap(l, fkey, feat->GetLoc().GetString(), eFeat );

    // ID-5922 : in HTML mode, we need to add a link that can serve 2 purposes:
    // 1. Direct hyperlink to open the flatfile for an interval encompassing this feature, or
    // 2. To show the feature information and highlight its location on the sequence "in place"
    // on the already generated flat file view (which itself can be either a full sequence,
    // or a location on it).
    // In case 2, the absolute offsets included in the link are adjusted to the relative offsets
    // by the Javascript responsible for showing the highlights.
    string strFeatKey;
    if (s_GetLinkFeatureKey(f, *feat, fkey, strFeatKey, m_uFeatureCount))
    {
        // We will need to pad since the feature's key might be smaller than strDummy
        // negative padding means we need to remove spaces.
        // const int padding_needed = (int)strDummy.length() - (int)feat->GetKey().length();
        // strFeatKey += string( padding_needed, ' ' );
        NON_CONST_ITERATE(list<string>, it, l) {
            // string::size_type dummy_loc = (*it).find(strDummy);
            NStr::ReplaceInPlace( *it, fkey, strFeatKey );
        }
    }

    text_os->AddParagraph(l, f.GetObject());

    x_SmartWrapQuals(f, *feat, *text_os);

    if (bHtml && f.GetContext()->Config().IsModeEntrez() && f.GetContext()->Config().ShowSeqSpans()) {
        // close the <span...>, without an endline
        text_os->AddLine("</span>", nullptr, IFlatTextOStream::eAddNewline_No);
    }

    text_os->Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// BASE COUNT

void CGenbankFormatter::FormatBasecount
(const CBaseCountItem& bc,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, bc, orig_text_os);

    list<string> l;

    ostringstream bc_line;

    bc_line.setf(IOS_BASE::right, IOS_BASE::adjustfield);
    bc_line
        << setw(7) << bc.GetA() << " a"
        << setw(7) << bc.GetC() << " c"
        << setw(7) << bc.GetG() << " g"
        << setw(7) << bc.GetT() << " t";
    if ( bc.GetOther() > 0 ) {
        bc_line << setw(7) << bc.GetOther() << " others";
    }
    Wrap(l, "BASE COUNT", bc_line.str());
    text_os.AddParagraph(l, bc.GetObject());

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// SEQUENCE

// 60 bases in a line, a space between every 10 bases.
const static TSeqPos s_kChunkSize = 10;
const static TSeqPos s_kChunkCount = 6;
const static TSeqPos s_kFullLineSize = s_kChunkSize*s_kChunkCount;

static inline
char* s_FormatSeqPosBack(char* p, TSeqPos v, size_t l)
{
    do {
        *--p = '0'+v%10;
    } while ( (v /= 10) && --l );
    return p;
}

// span should look like <span class="ff_line" id="gi_259526172_61">
//                                                              ^   ^
// "p" should point to the base_count part:  -------------------|   ^
// and everything before that should be filled in.  This fills      ^
// the rest and returns a pointer to just after the closing tag: ---|
static inline
char *s_FormatSeqSpanTag( char *p, int base_count )
{
    char * const initial_p = p;
    // To be as fast as possible, we write our own "int to string" function.
    // We actually write the number backward and then reverse it.  Is there a way
    // to avoid the reversal?
    do {
        *p = '0'+base_count%10;
        ++p;
    } while ( (base_count /= 10) > 0 );
    reverse( initial_p, p );

    *p = '\"';
    ++p;
    *p = '>';
    ++p;

    return p;
}

static TSeqPos
s_CalcDistanceUntilNextSignificantGapOrEnd(
    const CSequenceItem& seq,
    CSeqVector_CI iter // yes, COPY not reference
    )
{
    // see if we started in the middle of a gap
    if( iter.IsInGap() && iter.GetGapSizeBackward() > 0 ) {
        return 0;
    }

    TSeqPos dist_to_gap_or_end = 0;
    while( iter ) {
        if( ! iter.IsInGap() ) {
            const TSeqPos seg_len = iter.GetBufferSize();
            dist_to_gap_or_end += seg_len;
            iter += seg_len;
        } else {
            // see if gap is tiny enough to disregard
            // (the criterion is that it fit entirely on the current line,
            // with a non-gap after it)
            TSeqPos space_left_on_line =
                s_kFullLineSize - ( iter.GetPos() % s_kFullLineSize );
            if( 0 == space_left_on_line ) {
                space_left_on_line = s_kFullLineSize;
            }

            TSeqPos gap_size = 0;
            while( iter && iter.IsInGap() && gap_size < space_left_on_line ) {
                gap_size += iter.SkipGap();
            }
            if( gap_size >= space_left_on_line ) {
                // gap is too big and should be printed separately
                break;
            } else {
                // gap is tiny enough to print as N's, so keep going
                dist_to_gap_or_end += gap_size;
            }
        }
    }

    return dist_to_gap_or_end;
}

static void
s_FormatRegularSequencePiece
(const CSequenceItem& seq,
 IFlatTextOStream& text_os,
 CSeqVector_CI &iter,
 TSeqPos &total,
 TSeqPos &base_count )
{
    const bool bHtml = seq.GetContext()->Config().DoHTML() && seq.GetContext()->Config().ShowSeqSpans();
    const string& accn = seq.GetContext()->GetAccession();

    // format of sequence position
    size_t kSeqPosWidth = 9;

    const size_t kLineBufferSize = 170;
    char line[kLineBufferSize];
    // prefill the line buffer with spaces
    fill(line, line+kLineBufferSize, ' ');

    // add the span stuff
    const static string kCloseSpan = "</span>";
    TSeqPos length_of_span_before_base_count = 0;
    if( bHtml ) {
        string kSpan = " <span class=\"ff_line\" id=\"";
        kSpan += accn;
        kSpan += '_';
        copy( kSpan.begin(), kSpan.end(), line + kSeqPosWidth );
        length_of_span_before_base_count = kSpan.length();
    }

    // if base-count is offset, we indent the initial line
    TSeqPos initial_indent = 0;
    if( (base_count % s_kFullLineSize) != 1 ) {
        initial_indent = (base_count % s_kFullLineSize);
        if( 0 == initial_indent ) {
            initial_indent = (s_kFullLineSize - 1);
        } else {
            --initial_indent;
        }
    }

    while ( total > 0 ) {
        if (base_count >= 1000000000) {
            if (kSeqPosWidth == 9) {
                // repeat above calculation as soon as it reaches the first line with a 10 digit position count
                kSeqPosWidth = 10;
                // prefill the line buffer with spaces
                fill(line, line+kLineBufferSize, ' ');

                // add the span stuff
                length_of_span_before_base_count = 0;
                if( bHtml ) {
                    string kSpan = " <span class=\"ff_line\" id=\"";
                    kSpan += accn;
                    kSpan += '_';
                    copy( kSpan.begin(), kSpan.end(), line + kSeqPosWidth );
                    length_of_span_before_base_count = (int) kSpan.length();
                }

                // if base-count is offset, we indent the initial line
                TSeqPos initial_indent = 0;
                if( (base_count % s_kFullLineSize) != 1 ) {
                    initial_indent = (base_count % s_kFullLineSize);
                    if( 0 == initial_indent ) {
                        initial_indent = (s_kFullLineSize - 1);
                    } else {
                        --initial_indent;
                    }
                }
           }
        }
        char* linep = line + kSeqPosWidth;

        // each seqpos is a bigger number than the last, so we
        // don't have to worry about clearing out the old one
        s_FormatSeqPosBack(linep, base_count, kSeqPosWidth);
        if( bHtml ) {
            linep += length_of_span_before_base_count;
            linep = s_FormatSeqSpanTag( linep, base_count );
            --linep; // to balance out the extra ++linep farther below
        }

        char * const linep_right_after_span_tag = (linep + 1);

        TSeqPos i = 0;
        TSeqPos j = 0;

        // partial beginning line occurs sometimes, so we have to
        // offset some start-points
        int bases_to_skip = 0;
        if( initial_indent != 0 ) {
            bases_to_skip = initial_indent;
            // additional space required every chunk
            int chunks_to_skip = (bases_to_skip / s_kChunkSize);
            linep += (bases_to_skip + chunks_to_skip);
            i = chunks_to_skip;
            j = (bases_to_skip % s_kChunkSize);
            // don't indent subsequent lines
            initial_indent = 0;
        }

        if( total >= (s_kFullLineSize - bases_to_skip) ) {
            for ( ; i < s_kChunkCount; ++i) {
                ++linep;
                for ( ; j < s_kChunkSize; ++j, ++iter, ++linep) {
                    unsigned char ch = *iter;
                    if (ch > 126) {
                        ch = '?';
                    }
                    *linep = ch;
                }
                *linep = ' ';
                j = 0;
            }

            total -= (s_kFullLineSize - bases_to_skip);
            base_count += (s_kFullLineSize - bases_to_skip);
        } else {
            base_count += total;
            for ( ; total > 0  &&  i < s_kChunkCount; ++i) {
                ++linep;
                for ( ; total > 0  &&  j < s_kChunkSize; ++j, ++iter, --total, ++linep) {
                    unsigned char ch = *iter;
                    if (ch > 126) {
                        ch = '?';
                    }
                    *linep = ch;
                }
                *linep = ' ';
                j = 0;
            }
        }
        i = 0;

        if( bHtml ) {
            // Need to space-pad out to full length (except for the *very* last line)
            const bool doneWithEntireSequence = ( ! iter );
            if( ! doneWithEntireSequence ) {
                char * const linep_at_close_span =
                    linep_right_after_span_tag + s_kFullLineSize + s_kChunkCount - 1;
                fill( linep, linep_at_close_span, ' ' );
                linep = linep_at_close_span;
            }

            // put on closing </span> tag
            copy( kCloseSpan.begin(), kCloseSpan.end(), linep );
            linep += kCloseSpan.length();
        }

        *linep = 0;
        // CTempString avoids the cost of scanning "line"
        CTempString tempStrLine(line, linep-line);
        text_os.AddLine( tempStrLine, seq.GetObject() );
    }
}

void CGenbankFormatter::FormatSequence
(const CSequenceItem& seq,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, seq, orig_text_os);

    const bool bGapsHiddenUntilClicked = (
        GetContext().GetConfig().DoHTML() &&
        GetContext().GetConfig().IsModeEntrez() &&
        !GetContext().GetConfig().ExpandGaps());

    const CSeqVector& vec = seq.GetSequence();
    TSeqPos from = seq.GetFrom();
    TSeqPos to = seq.GetTo();
    TSeqPos base_count = from;

    TSeqPos vec_pos = from-1;
    TSeqPos total = from <= to? to - from + 1 : 0;

    CSeqVector_CI iter(vec, vec_pos, CSeqVector_CI::eCaseConversion_lower);
    if( ! bGapsHiddenUntilClicked ) {
        // normal case: print entire sequence, including all the N's in any gap.
        try {
            s_FormatRegularSequencePiece( seq, text_os, iter, total, base_count );
        } catch (CSeqVectorException&) {
        }
    } else {
        // special case: instead of showing the N's in a gap right away, we have the
        // "Expand Ns" link that users can click to show the Ns
        while( iter && total > 0 ) {
            const TSeqPos distance_until_next_significant_gap =
                min( total, s_CalcDistanceUntilNextSignificantGapOrEnd(seq, iter) );

            if( 0 == distance_until_next_significant_gap ) {

                const bool gap_started_before_this_point = ( iter.GetGapSizeBackward() > 0 );

                TSeqPos gap_size = 0;
                // sum up gap length, skipping over all gaps until we reach real data
                while( iter && iter.IsInGap() ) {
                    gap_size += iter.SkipGap();
                }

                if( total >= gap_size ) {
                    total -= gap_size;
                } else {
                    total = 0;
                }
                base_count += gap_size;

                if( gap_started_before_this_point && ! seq.IsFirst() ) {
                    continue;
                }

                // build gap size text and "Expand Ns" link
                ostringstream gap_link;
                GetContext().GetConfig().GetHTMLFormatter().FormatGapLink(gap_link, gap_size,
                                                                          seq.GetContext()->GetAccession(),
                                                                          seq.GetContext()->IsProt());
                text_os.AddLine(gap_link.str());
            } else {
                // create a fake total so we stop before the next gap
                TSeqPos fake_total = distance_until_next_significant_gap;
                try {
                    s_FormatRegularSequencePiece( seq, text_os, iter, fake_total, base_count);
                } catch (CSeqVectorException&) {
                }
                const TSeqPos amount_to_subtract_from_total =
                    ( distance_until_next_significant_gap - fake_total );
                if( total >= amount_to_subtract_from_total ) {
                    total -= amount_to_subtract_from_total;
                } else {
                    total = 0;
                }
            }
        }
    }

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// DBSOURCE

void CGenbankFormatter::FormatDBSource
(const CDBSourceItem& dbs,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, dbs, orig_text_os);

    list<string> l;

    const bool bHtml = dbs.GetContext()->Config().DoHTML();

    if ( !dbs.GetDBSource().empty() ) {
        string tag = "DBSOURCE";
        ITERATE (list<string>, it, dbs.GetDBSource()) {
            string db_src = *it;
            if( bHtml ) {
                TryToSanitizeHtml( db_src );
            }
            Wrap(l, tag, db_src);
            tag.erase();
        }
        if ( !l.empty() ) {
            if( dbs.GetContext()->Config().DoHTML() ) {
                TryToSanitizeHtmlList(l);
            }
            text_os.AddParagraph(l, dbs.GetObject());
        }
    }

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// WGS

void CGenbankFormatter::FormatWGS
(const CWGSItem& wgs,
 IFlatTextOStream& orig_text_os)
{
    const bool bHtml = wgs.GetContext()->Config().DoHTML();

    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, wgs, orig_text_os);

    string tag;

    switch ( wgs.GetType() ) {
    case CWGSItem::eWGS_Projects:
        tag = "WGS";
        break;

    case CWGSItem::eWGS_ScaffoldList:
        tag = "WGS_SCAFLD";
        break;

    case CWGSItem::eWGS_ContigList:
        tag = "WGS_CONTIG";
        break;

    default:
        return;
    }

    // Get first and last id (sanitized for html, if necessary)
    list<string> l;
    string first_id = wgs.GetFirstID();
    if( bHtml ) {
        TryToSanitizeHtml( first_id );
    }
    string last_id;
    bool first_id_equals_second_id = false;
    if ( wgs.GetFirstID() == wgs.GetLastID() ) {
        last_id = first_id;
        first_id_equals_second_id = true;
    } else {
        last_id = wgs.GetLastID();
        if( bHtml ) {
            TryToSanitizeHtml( last_id );
        }
    }

    string wgs_line = ( first_id_equals_second_id ? first_id : first_id + "-" + last_id );

    // surround wgs_line with a link, if necessary
    if( bHtml ) {
        string link;
        if( first_id_equals_second_id ) {
            link = "https://www.ncbi.nlm.nih.gov/nuccore/" + first_id;
        } else {
            string url_arg;
            // ID-5288 : Allow for variable prefix length
            // First 2 digits are the major version of the project which must be appended
            // ID-6000 : For contigs, always use link to WGS browser; for scaffolds,
            // use Entrez link if ID-based and WGS browser link if VDB-based.
            const bool bIsWGSProject = CWGSItem::eWGS_Projects == wgs.GetType();
            SIZE_TYPE prefix_len = first_id.find_first_of("0123456789");
            const bool bIsWGSScafldWithS =
                ( CWGSItem::eWGS_ScaffoldList == wgs.GetType() &&
                  first_id.length() > 7 && first_id[prefix_len+2] == 'S' );

            if (bIsWGSProject || bIsWGSScafldWithS) {
                url_arg = first_id.substr(0,prefix_len+2);
                link = "https://www.ncbi.nlm.nih.gov/Traces/wgs/" +
                    url_arg + "?display=" + ( bIsWGSScafldWithS ? "scaffolds" : "contigs" );
            } else {
                link = "https://www.ncbi.nlm.nih.gov/nuccore?term=" + first_id + ":" + last_id + "[PACC]";
            }
        }
        wgs_line = "<a href=\"" + link + "\">" + wgs_line + "</a>";
    }

    Wrap( l, tag, wgs_line, ePara, bHtml );

    text_os.AddParagraph(l, wgs.GetObject());

    text_os.Flush();
}

///////////////////////////////////////////////////////////////////////////
//
// TSA

void CGenbankFormatter::FormatTSA
(const CTSAItem& tsa,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, tsa, orig_text_os);

    string tag;

    const bool bHtml = tsa.GetContext()->Config().DoHTML();

   if ( tsa.GetType() == CTSAItem::eTLS_Projects ) {

        list<string> l;
        string first_id = tsa.GetFirstID();
        if( bHtml ) {
            TryToSanitizeHtml( first_id );
        }
        string id_range;
        if ( tsa.GetFirstID() == tsa.GetLastID() ) {
            id_range = first_id;
        } else {
            string last_id = tsa.GetLastID();
            id_range = first_id + "-" + last_id;
        }

        if( bHtml ) {
            TryToSanitizeHtml( id_range );

            string tls_master = tsa.GetContext()->GetTLSMasterName();
            // ID-5288 : Allow for variable prefix length
            // First 2 digits are the major version of the project which must be appended
            SIZE_TYPE prefix_len = tls_master.find_first_of("0123456789");
            tls_master = tls_master.substr(0, prefix_len+2);
            TryToSanitizeHtml(tls_master);
            if( ! tls_master.empty() ) {
                id_range = "<a href=\"https://www.ncbi.nlm.nih.gov/Traces/wgs?val=" + tls_master + "#contigs\">" + id_range + "</a>";
            }
        }

        Wrap(l, "TLS", id_range, ePara, bHtml);

        text_os.AddParagraph(l, tsa.GetObject());

        text_os.Flush();

        return;
    }

    list<string> l;
    string first_id = tsa.GetFirstID();
    if( bHtml ) {
        TryToSanitizeHtml( first_id );
    }
    string id_range;
    if ( tsa.GetFirstID() == tsa.GetLastID() ) {
        id_range = first_id;
    } else {
        string last_id = tsa.GetLastID();
        id_range = first_id + "-" + last_id;
    }

    if( bHtml ) {
        TryToSanitizeHtml( id_range );

        string tsa_master = tsa.GetContext()->GetTSAMasterName();
        // ID-5288 : Allow for variable prefix length
        // First 2 digits are the major version of the project which must be appended
        SIZE_TYPE prefix_len = tsa_master.find_first_of("0123456789");
        tsa_master = tsa_master.substr(0, prefix_len+2);
        TryToSanitizeHtml(tsa_master);
        if( ! tsa_master.empty() ) {
            id_range = "<a href=\"https://www.ncbi.nlm.nih.gov/Traces/wgs?val=" + tsa_master + "\">" + id_range + "</a>";
        }
    }

    Wrap(l, "TSA", id_range, ePara, bHtml);

    text_os.AddParagraph(l, tsa.GetObject());

    text_os.Flush();
}



///////////////////////////////////////////////////////////////////////////
//
// PRIMARY

void CGenbankFormatter::FormatPrimary
(const CPrimaryItem& primary,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, primary, orig_text_os);

    list<string> l;

    string primary_str = primary.GetString();
    if( primary.GetContext()->Config().DoHTML() ) {
        TryToSanitizeHtml( primary_str );
    }
    Wrap(l, "PRIMARY", primary_str);

    text_os.AddParagraph(l, primary.GetObject());

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// GENOME

void CGenbankFormatter::FormatGenome
(const CGenomeItem& genome,
 IFlatTextOStream& orig_text_os)
{
    // !!!
}


///////////////////////////////////////////////////////////////////////////
//
// CONTIG

void CGenbankFormatter::FormatContig
(const CContigItem& contig,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, contig, orig_text_os);

    list<string> l;
    string assembly = CFlatSeqLoc(contig.GetLoc(), *contig.GetContext(),
        CFlatSeqLoc::eType_assembly).GetString();

    // must have our info inside "join" in all cases
    if (assembly.empty()) {
        assembly = "join()";
    }
    if( ! NStr::StartsWith( assembly, "join(" ) ) {
        assembly = "join(" + assembly + ")";  // example where needed: accession NG_005477.4
    }

    Wrap(l, "CONTIG", assembly);

    text_os.AddParagraph(l, contig.GetObject());

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// ORIGIN

void CGenbankFormatter::FormatOrigin
(const COriginItem& origin,
 IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, origin, orig_text_os);

    bool bHtml = this->GetContext().GetConfig().DoHTML();

    list<string> l;
    string strOrigin = origin.GetOrigin();
    if ( strOrigin == "." ) {
        strOrigin.erase();
    }

    if ( strOrigin.empty() ) {
        l.push_back( "ORIGIN      " );
    } else {
        if ( ! NStr::EndsWith( strOrigin, "." ) ) {
            strOrigin += ".";
        }
        if ( bHtml ) {
            TryToSanitizeHtml( strOrigin );
        }
        Wrap( l, "ORIGIN", strOrigin );
    }
    text_os.AddParagraph( l, origin.GetObject() );

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// GAP

void CGenbankFormatter::FormatGap(const CGapItem& gap, IFlatTextOStream& orig_text_os)
{
    CRef<IFlatTextOStream> p_text_os;
    IFlatTextOStream& text_os = s_WrapOstreamIfCallbackExists(p_text_os, gap, orig_text_os);

    // const bool bHtml = gap.GetContext()->Config().DoHTML();

    list<string> l;

    TSeqPos gapStart = gap.GetFrom();
    TSeqPos gapEnd   = gap.GetTo();

    const bool isGapOfLengthZero = ( gapStart > gapEnd );

    // size zero gaps require an adjustment to print right
    if( isGapOfLengthZero ) {
        gapStart--;
        gapEnd++;
    }

        // format location
    string loc = NStr::UIntToString(gapStart);
    loc += "..";
    loc += NStr::UIntToString(gapEnd);

    Wrap(l, gap.GetFeatureName(), loc, eFeat);

    // gaps don't use the span stuff, but I'm leaving this code here
    // (but commented out) in case that changes in the future.

    //if( bHtml && gap.GetContext()->Config().IsModeEntrez() ) {
    //    CRef<CSeq_loc> gapLoc( new CSeq_loc );
    //    gapLoc->SetInt().SetFrom(gapStart - 1);
    //    gapLoc->SetInt().SetTo(gapEnd - 1);
    //    *l.begin() = x_GetFeatureSpanAndScriptStart(gap.GetFeatureName().c_str(), *gapLoc, *gap.GetContext()) + *l.begin();
    //}

    // size zero gaps indicate non-consecutive residues
    if( isGapOfLengthZero ) {
        NStr::Wrap("\"Non-consecutive residues\"", GetWidth(), l, SetWrapFlags(),
            GetFeatIndent(), GetFeatIndent() + "/note=");
    }

    // format mandatory /estimated_length qualifier
    string estimated_length;
    if (gap.HasEstimatedLength()) {
        estimated_length = NStr::UIntToString(gap.GetEstimatedLength());
    } else {
        estimated_length = "unknown";
    }
    NStr::Wrap(estimated_length, GetWidth(), l, SetWrapFlags(),
        GetFeatIndent(), GetFeatIndent() + "/estimated_length=");

    // format /gap_type
    if( gap.HasType() ) {
        NStr::Wrap('"' + gap.GetType() + '"', GetWidth(), l, SetWrapFlags(),
            GetFeatIndent(), GetFeatIndent() + "/gap_type=");
    }

    // format /linkage_evidence
    if( gap.HasEvidence() ) {
        ITERATE( CGapItem::TEvidence, evidence_iter, gap.GetEvidence() ) {
            NStr::Wrap( '"' + *evidence_iter + '"', GetWidth(), l, SetWrapFlags(),
                GetFeatIndent(), GetFeatIndent() + "/linkage_evidence=");
        }
    }

    text_os.AddParagraph(l, gap.GetObject());

    text_os.Flush();

    // gaps don't use the span stuff, but I'm leaving this code here
    // (but commented out) in case that changes in the future.

    //if( bHtml && gap.GetContext()->Config().IsModeEntrez() ) {
    //    text_os.AddLine("</span>", 0,
    //        IFlatTextOStream::eAddNewline_No );
    //}
}

END_SCOPE(objects)
END_NCBI_SCOPE
