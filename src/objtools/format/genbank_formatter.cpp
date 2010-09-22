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
#include <corelib/ncbistd.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/general/Person_id.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/format/text_ostream.hpp>
#include <objtools/format/genbank_formatter.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/accession_item.hpp>
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
#include <objtools/format/items/primary_item.hpp>
#include <objtools/format/items/contig_item.hpp>
#include <objtools/format/items/genome_item.hpp>
#include <objtools/format/items/origin_item.hpp>
#include <objtools/format/items/gap_item.hpp>
#include <objtools/format/items/genome_project_item.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"

#include <algorithm>
#include <stdio.h>



BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CGenbankFormatter::CGenbankFormatter(void) :
    m_uFeatureCount( 0 )
{
    SetIndent(string(12, ' '));
    SetFeatIndent(string(21, ' '));
    SetBarcodeIndent(string(35, ' '));
}


///////////////////////////////////////////////////////////////////////////
//
// END SECTION

void CGenbankFormatter::EndSection
(const CEndSectionItem&,
 IFlatTextOStream& text_os)
{
    bool bHtml = GetContext().GetConfig().DoHTML();
    list<string> l;
    if ( bHtml ) {
        l.push_back( "//</pre>" );
    }
    else {
        l.push_back("//");
    }
    text_os.AddParagraph(l, NULL);
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
 IFlatTextOStream& text_os)
{
    static const string strands[]  = { "   ", "ss-", "ds-", "ms-" };

    const CBioseqContext& ctx = *locus.GetContext();

    list<string> l;
    CNcbiOstrstream locus_line;

    string units = "bp";
    if ( !ctx.IsProt() ) {
        if ( ctx.IsWGSMaster()  &&  ctx.IsRSWGSNuc() ) {
            units = "rc";
        }
    } else {
        units = "aa";
    }
    string topology = (locus.GetTopology() == CSeq_inst::eTopology_circular) ?
                "circular" : "linear  ";

    string mol = s_GenbankMol[locus.GetBiomol()];

    locus_line.setf(IOS_BASE::left, IOS_BASE::adjustfield);
    locus_line << setw(16) << locus.GetName() << ' ';
    locus_line.setf(IOS_BASE::right, IOS_BASE::adjustfield);
    locus_line
        << setw(11) << locus.GetLength()
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

    Wrap(l, GetWidth(), "LOCUS", CNcbiOstrstreamToString(locus_line));
    if ( GetContext().GetConfig().DoHTML() ) {
        string& strFirstLine = *l.begin();
        strFirstLine = "<pre>" + strFirstLine;
    }
    
    text_os.AddParagraph(l, locus.GetObject());
}


///////////////////////////////////////////////////////////////////////////
//
// Definition

void CGenbankFormatter::FormatDefline
(const CDeflineItem& defline,
 IFlatTextOStream& text_os)
{
    list<string> l;
    Wrap(l, "DEFINITION", defline.GetDefline());
    text_os.AddParagraph(l, defline.GetObject());
}


///////////////////////////////////////////////////////////////////////////
//
// Accession

void CGenbankFormatter::FormatAccession
(const CAccessionItem& acc, 
 IFlatTextOStream& text_os)
{
    string acc_line = x_FormatAccession(acc, ' ');
    if ( acc.IsSetRegion() ) {
        acc_line += " REGION: ";
        acc_line += CFlatSeqLoc(acc.GetRegion(), *acc.GetContext()).GetString();
    }
    list<string> l;
    if (NStr::IsBlank(acc_line)) {
        l.push_back("ACCESSION   ");
    } else {
        Wrap(l, "ACCESSION", acc_line);
    }
    text_os.AddParagraph(l, acc.GetObject());
}


///////////////////////////////////////////////////////////////////////////
//
// Version

void CGenbankFormatter::FormatVersion
(const CVersionItem& version,
 IFlatTextOStream& text_os)
{
    list<string> l;
    CNcbiOstrstream version_line;

    if ( version.GetAccession().empty() ) {
        l.push_back("VERSION");
    } else {
        version_line << version.GetAccession();
        if ( version.GetGi() > 0 ) {
            version_line << "  GI:" << version.GetGi();
        }
        Wrap(l, GetWidth(), "VERSION", CNcbiOstrstreamToString(version_line));
    }

    text_os.AddParagraph(l, version.GetObject());
}


///////////////////////////////////////////////////////////////////////////////
//
// Genome Project

void CGenbankFormatter::FormatGenomeProject(
    const CGenomeProjectItem& gp,
    IFlatTextOStream& text_os) {
    
    if (0 == gp.GetProjectNumber()) {
        return;
    }
    list<string> l;
    CNcbiOstrstream project_line;
    project_line << "Project: " << gp.GetProjectNumber();
    Wrap(l, GetWidth(), "DBLINK", CNcbiOstrstreamToString(project_line));
    text_os.AddParagraph(l, gp.GetObject());
}

///////////////////////////////////////////////////////////////////////////
//
// Keywords

void CGenbankFormatter::FormatKeywords
(const CKeywordsItem& keys,
 IFlatTextOStream& text_os)
{
    list<string> l;
    x_GetKeywords(keys, "KEYWORDS", l);
    text_os.AddParagraph(l, keys.GetObject());
}


///////////////////////////////////////////////////////////////////////////
//
// Segment

void CGenbankFormatter::FormatSegment
(const CSegmentItem& seg,
 IFlatTextOStream& text_os)
{
    list<string> l;
    CNcbiOstrstream segment_line;

    segment_line << seg.GetNum() << " of " << seg.GetCount();

    Wrap(l, "SEGMENT", CNcbiOstrstreamToString(segment_line));
    text_os.AddParagraph(l, seg.GetObject());
}


///////////////////////////////////////////////////////////////////////////
//
// Source

// SOURCE + ORGANISM

void CGenbankFormatter::FormatSource
(const CSourceItem& source,
 IFlatTextOStream& text_os)
{
    list<string> l;
    x_FormatSourceLine(l, source);
    x_FormatOrganismLine(l, source);
    text_os.AddParagraph(l, source.GetObject());    
}


void CGenbankFormatter::x_FormatSourceLine
(list<string>& l,
 const CSourceItem& source) const
{
    CNcbiOstrstream source_line;
    
    string prefix = source.IsUsingAnamorph() ? " (anamorph: " : " (";
    
    source_line << source.GetOrganelle() << source.GetTaxname();
    if ( !source.GetCommon().empty() ) {
        source_line << prefix << source.GetCommon() << ")";
    }
    
    Wrap(l, GetWidth(), "SOURCE", CNcbiOstrstreamToString(source_line));
}


static string s_GetHtmlTaxname(const CSourceItem& source)
{
    CNcbiOstrstream link;
    
    if (!NStr::EqualNocase(source.GetTaxname(), "Unknown")) {
        if (source.GetTaxid() != CSourceItem::kInvalidTaxid) {
            link << "<a href=" << "http://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?" << "id=" << source.GetTaxid() << ">";
        } else {
            string taxname = source.GetTaxname();
            replace(taxname.begin(), taxname.end(), ' ', '+');
            link << "<a href=" << "http://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?" << "name=" << taxname << ">";
        }
        link << source.GetTaxname() << "</a>";
    } else {
        return source.GetTaxname();
    }

    return CNcbiOstrstreamToString(link);
}


void CGenbankFormatter::x_FormatOrganismLine
(list<string>& l,
 const CSourceItem& source) const
{
    // taxname
    if (source.GetContext()->Config().DoHTML()) {
        Wrap(l, "ORGANISM", s_GetHtmlTaxname(source), eSubp);
    } else {
        Wrap(l, "ORGANISM", source.GetTaxname(), eSubp);
    }
    // lineage
    Wrap(l, kEmptyStr, source.GetLineage(), eSubp);
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
        "http://www.patentlens.net/patentlens/simple.cgi?patnum=" );
    const string strBaseUrlCambiaPatentLensTail(
        "#list" );

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
        strPatString += pat.GetNumber();
        strPatString += strBaseUrlCambiaPatentLensTail;
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
 IFlatTextOStream& text_os)
{
    CBioseqContext& ctx = *ref.GetContext();

    list<string> l;

    x_Reference(l, ref, ctx);
    x_Authors(l, ref, ctx);
    x_Consortium(l, ref, ctx);
    x_Title(l, ref, ctx);
    x_Journal(l, ref, ctx);
    if (ref.GetPMID() == 0) {  // suppress MEDLINE if has PUBMED
        x_Medline(l, ref, ctx);
    }
    x_Pubmed(l, ref, ctx);
    x_Remark(l, ref, ctx);

    text_os.AddParagraph(l, ref.GetObject());
}


// The REFERENCE line contains the number of the particular reference and
// (in parentheses) the range of bases in the sequence entry reported in
// this citation.
void CGenbankFormatter::x_Reference
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    CNcbiOstrstream ref_line;

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
    Wrap(l, GetWidth(), "REFERENCE", CNcbiOstrstreamToString(ref_line));
}


void CGenbankFormatter::x_Authors
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    string authors;
    if (ref.IsSetAuthors()) {
        CReferenceItem::FormatAuthors(ref.GetAuthors(), authors);
    }
    if ( authors.empty() ) {
        /* supress AUTHOR line */
        return;
    }
    if (!NStr::EndsWith(authors, '.')) {
        authors += '.';
    }
    Wrap(l, "AUTHORS", authors, eSubp);
}


void CGenbankFormatter::x_Consortium
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    if (!NStr::IsBlank(ref.GetConsortium())) {
        Wrap(l, "CONSRTM", ref.GetConsortium(), eSubp);
    }
}


void CGenbankFormatter::x_Title
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    if (!NStr::IsBlank(ref.GetTitle())) {
        Wrap(l, "TITLE", ref.GetTitle(),   eSubp);
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
        Wrap(l, "JOURNAL", journal, eSubp);
    }
}


void CGenbankFormatter::x_Medline
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    if ( ref.GetMUID() != 0 ) {
        Wrap(l, GetWidth(), "MEDLINE", NStr::IntToString(ref.GetMUID()), eSubp);
    }
}


void CGenbankFormatter::x_Pubmed
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    
    if ( ref.GetPMID() == 0 ) {
        return;
    }
    string strPubmed = NStr::IntToString( ref.GetPMID() );
    if ( ctx.Config().DoHTML() ) {
        string strRaw = strPubmed;
        strPubmed = "<a href=\"http://www.ncbi.nlm.nih.gov/pubmed/";
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
    if (!NStr::IsBlank(ref.GetRemark())) {
        Wrap(l, "REMARK", ref.GetRemark(), eSubp);
    }
    if ( ctx.Config().GetMode() == CFlatFileConfig::eMode_Entrez ) {
        if ( ref.IsSetPatent() ) {
            string strCambiaPatentLens = s_GetLinkCambiaPatentLens( ref, 
                ctx.Config().DoHTML() );
            if ( ! strCambiaPatentLens.empty() ) {
                Wrap(l, "REMARK", strCambiaPatentLens, eSubp);
            }  
        }      
    }
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

void CGenbankFormatter::FormatComment
(const CCommentItem& comment,
 IFlatTextOStream& text_os)
{
    list<string> l;

    if (!comment.IsFirst()) {
        Wrap(l, kEmptyStr, comment.GetComment(), eSubp);
    } else {
        Wrap(l, "COMMENT", comment.GetComment());
    }

    // s_OrphanFixup( l, 1 ); // removed this because it was actually causing issues
    text_os.AddParagraph(l, comment.GetObject());
}


///////////////////////////////////////////////////////////////////////////
//
// FEATURES

// Fetures Header

void CGenbankFormatter::FormatFeatHeader
(const CFeatHeaderItem& fh,
 IFlatTextOStream& text_os)
{
    list<string> l;

    Wrap(l, "FEATURES", "Location/Qualifiers", eFeatHead);

    text_os.AddParagraph(l, NULL);
}

//  ============================================================================
string s_GetLinkFeatureKey( 
    const CFeatureItemBase& item,
    unsigned int uItemNumber = 0 )
//  ============================================================================
{
    const string strLinkbaseNuc( "http://www.ncbi.nlm.nih.gov/nuccore/" );
    const string strLinkbaseProt( "http://www.ncbi.nlm.nih.gov/protein/" );

    CConstRef<CFlatFeature> feat = item.Format();
    string strRawKey = feat->GetKey();
    if ( strRawKey == "source" || strRawKey == "gap" ) {
        return strRawKey;
    }

    CBioseq_Handle bsh = item.GetContext()->GetHandle();

    // link base location:
    string strLinkbase;
    if ( bsh.IsAa() ) {    
        strLinkbase += strLinkbaseProt;
    }
    else {
        strLinkbase += strLinkbaseNuc;
    }

    // sequence id:
    string strId = "0";
    if ( bsh.CanGetId() ) {
        vector< CSeq_id_Handle > ids = bsh.GetId();
        ITERATE( vector< CSeq_id_Handle >, it, bsh.GetId() ) {
            if ( it->IsGi() ) {
                strId = NStr::IntToString( it->GetGi() );
                break;
            }
        }
    }

    // location on sequence:
    string strLocation = "";
    if ( strLocation .empty() ) {
        const CSeq_loc& loc = item.GetLoc();
        if ( loc.IsInt() && loc.GetInt().IsSetFrom() && loc.GetInt().IsSetTo() ) {
            strLocation = "?from=";
            strLocation += NStr::UIntToString( loc.GetInt().GetFrom() + 1 );
            strLocation += "&amp;to=";
            strLocation += NStr::UIntToString( loc.GetInt().GetTo() + 1 );
        } 
    }
    if ( strLocation.empty() ) {
        strLocation = "?itemid=";
        strLocation += NStr::UIntToString( uItemNumber );
    }

    // report style:
    string strReport( "&amp;report=gbwithparts" );
    if ( bsh.IsAa() ) {
        strReport = "&amp;report=gpwithparts";
    }

    // assembly of the actual string:
    string strLink = "<a href=\"";
    strLink += strLinkbase;
    strLink += strId;
    strLink += strLocation;
    strLink += strReport;
    strLink += "\">";
    strLink += strRawKey;
    strLink += "</a>";
    return strLink;
}

//  ============================================================================
void CGenbankFormatter::FormatFeature
(const CFeatureItemBase& f,
 IFlatTextOStream& text_os)
//  ============================================================================
{ 
    CConstRef<CFlatFeature> feat = f.Format();
    const vector<CRef<CFormatQual> > & quals = feat->GetQuals();
    list<string>        l, l_new;

    if ( feat->GetKey() != "source" ) {
        ++ m_uFeatureCount;
    }
    bool bHtml = f.GetContext()->Config().DoHTML();

    const string strDummy( "[FEATKEY]" );
    string strKey = bHtml ? strDummy : feat->GetKey();
    Wrap(l, strKey, feat->GetLoc().GetString(), eFeat );
    if ( bHtml ) {
        string strFeatKey = s_GetLinkFeatureKey( f, m_uFeatureCount );
        NON_CONST_ITERATE( list<string>, it, l ) {
            NStr::ReplaceInPlace( *it, strDummy, strFeatKey );
        }
    }

    ITERATE (vector<CRef<CFormatQual> >, it, quals ) {
        string qual = '/' + (*it)->GetName(), value = (*it)->GetValue();
        TrimSpacesAndJunkFromEnds( value, true );
        switch ((*it)->GetStyle()) {
        case CFormatQual::eEmpty:
            value = qual;
            qual.erase();
            break;
        case CFormatQual::eQuoted:
            qual += "=\"";  value += '"';
            break;
        case CFormatQual::eUnquoted:
            qual += '=';
            break;
        }
        // Call NStr::Wrap directly to avoid unwanted line breaks right
        // before the start of the value (in /translation, e.g.)
        NStr::Wrap(value, GetWidth(), l_new, SetWrapFlags(), GetFeatIndent(),
            GetFeatIndent() + qual);
        if ( l_new.size() > 1  &&  NStr::TruncateSpaces( l_new.back() ) == "\"" ) {
            l_new.pop_back();
            l_new.back() += "\"";
        }
        // Values of qualifiers coming down this path do not carry additional
        // internal format (at least, they aren't supposed to). So we strip extra
        // blanks from both the begin and the end of qualifier lines.
        // (May have to be amended once sizeable numbers of violators are found
        // in existing data).
        NON_CONST_ITERATE (list<string>, it, l_new) {
            NStr::TruncateSpacesInPlace( *it, NStr::eTrunc_End );
        }
        l.insert( l.end(), l_new.begin(), l_new.end() );
        l_new.clear();
    }
        
    text_os.AddParagraph(l, f.GetObject());
}


///////////////////////////////////////////////////////////////////////////
//
// BASE COUNT

void CGenbankFormatter::FormatBasecount
(const CBaseCountItem& bc,
 IFlatTextOStream& text_os)
{
    list<string> l;

    CNcbiOstrstream bc_line;

    bc_line.setf(IOS_BASE::right, IOS_BASE::adjustfield);
    bc_line
        << setw(7) << bc.GetA() << " a"
        << setw(7) << bc.GetC() << " c"
        << setw(7) << bc.GetG() << " g"
        << setw(7) << bc.GetT() << " t";
    if ( bc.GetOther() > 0 ) {
        bc_line << setw(7) << bc.GetOther() << " others";
    }
    Wrap(l, "BASE COUNT", CNcbiOstrstreamToString(bc_line));
    text_os.AddParagraph(l, bc.GetObject());
}


///////////////////////////////////////////////////////////////////////////
//
// SEQUENCE

static inline
char* s_FormatSeqPosBack(char* p, TSeqPos v, size_t l)
{
    do {
        *--p = '0'+v%10;
    } while ( (v /= 10) && --l );
    return p;
}

void CGenbankFormatter::FormatSequence
(const CSequenceItem& seq,
 IFlatTextOStream& text_os)
{
    list<string> l;

    const CSeqVector& vec = seq.GetSequence();
    TSeqPos from = seq.GetFrom();
    TSeqPos to = seq.GetTo();
    TSeqPos base_count = from;
    TSeqPos total = to - from + 1;
    TSeqPos vec_pos = from-1;
    TSeqPos vec_size = vec.size();
    TSeqPos vec_remaining = vec_pos < vec_size? vec_size - vec_pos: 0;
    if ( vec_remaining < total ) {
        total = vec_remaining;
    }
    
    // format of sequence position
    const size_t kSeqPosWidth = 9;
    
    // 60 bases in a line, a space between every 10 bases.
    const TSeqPos kChunkSize = 10;
    const TSeqPos kChunkCount = 6;
    const TSeqPos kFullLineSize = kChunkSize*kChunkCount;

    const size_t kLineBufferSize = 100;
    char line[kLineBufferSize];
    // prefill the line buffer with spaces
    fill(line, line+kLineBufferSize, ' ');

    CSeqVector_CI iter(vec, vec_pos, CSeqVector_CI::eCaseConversion_lower);
    while ( total >= kFullLineSize ) {
        char* linep = line + kSeqPosWidth;
        s_FormatSeqPosBack(linep, base_count, kSeqPosWidth);
        for ( TSeqPos i = 0; i < kChunkCount; ++i) {
            ++linep;
            for ( TSeqPos j = 0; j < kChunkSize; ++j, ++iter, ++linep) {
                *linep = *iter;
            }
        }
        total -= kFullLineSize;
        base_count += kFullLineSize;

        *linep = 0;
        l.push_back(line);
    }
    if ( total > 0 ) {
        char* linep = line + kSeqPosWidth;
        s_FormatSeqPosBack(linep, base_count, kSeqPosWidth);
        for ( TSeqPos i = 0; total > 0  &&  i < kChunkCount; ++i) {
            ++linep;
            for ( TSeqPos j = 0; total > 0  &&  j < kChunkSize; ++j, ++iter, --total, ++linep) {
                *linep = *iter;
            }
        }
        *linep = 0;
        l.push_back(line);
    }

    text_os.AddParagraph(l, seq.GetObject());
}


///////////////////////////////////////////////////////////////////////////
//
// DBSOURCE

void CGenbankFormatter::FormatDBSource
(const CDBSourceItem& dbs,
 IFlatTextOStream& text_os)
{
    list<string> l;

    if ( !dbs.GetDBSource().empty() ) {
        string tag = "DBSOURCE";
        ITERATE (list<string>, it, dbs.GetDBSource()) {
            Wrap(l, tag, *it);
            tag.erase();
        }
        if ( !l.empty() ) {
            text_os.AddParagraph(l, dbs.GetObject());
        }
    }        
}


///////////////////////////////////////////////////////////////////////////
//
// WGS

void CGenbankFormatter::FormatWGS
(const CWGSItem& wgs,
 IFlatTextOStream& text_os)
{
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

    list<string> l;
    if ( wgs.GetFirstID() == wgs.GetLastID() ) {
        Wrap(l, tag, wgs.GetFirstID());
    } else {
        Wrap(l, tag, wgs.GetFirstID() + "-" + wgs.GetLastID());
    }
    text_os.AddParagraph(l, wgs.GetObject());
}


///////////////////////////////////////////////////////////////////////////
//
// PRIMARY

void CGenbankFormatter::FormatPrimary
(const CPrimaryItem& primary,
 IFlatTextOStream& text_os)
{
    list<string> l;

    Wrap(l, "PRIMARY", primary.GetString());

    text_os.AddParagraph(l, primary.GetObject());
}


///////////////////////////////////////////////////////////////////////////
//
// GENOME

void CGenbankFormatter::FormatGenome
(const CGenomeItem& genome,
 IFlatTextOStream& text_os)
{
    // !!!
}


///////////////////////////////////////////////////////////////////////////
//
// CONTIG

void CGenbankFormatter::FormatContig
(const CContigItem& contig,
 IFlatTextOStream& text_os)
{
    list<string> l;
    string assembly = CFlatSeqLoc(contig.GetLoc(), *contig.GetContext(), 
        CFlatSeqLoc::eType_assembly).GetString();

    // must have our info inside "join" in all cases
    if (assembly.empty()) {
        assembly = "join()";
    }
    if( assembly.substr( 0, 5 ) != "join(" ) {
        assembly = "join(" + assembly + ")";  // example where needed: accession NG_005477.4
    }

    Wrap(l, "CONTIG", assembly);
    text_os.AddParagraph(l, contig.GetObject());
}


///////////////////////////////////////////////////////////////////////////
//
// ORIGIN

void CGenbankFormatter::FormatOrigin
(const COriginItem& origin,
 IFlatTextOStream& text_os)
{
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
        Wrap( l, "ORIGIN", strOrigin );
    }
    text_os.AddParagraph( l, origin.GetObject() );
}


///////////////////////////////////////////////////////////////////////////
//
// GAP

void CGenbankFormatter::FormatGap(const CGapItem& gap, IFlatTextOStream& text_os)
{
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

    Wrap(l, "gap", loc, eFeat);

    // size zero gaps indicate non-consecutive residues
    if( isGapOfLengthZero ) {
        NStr::Wrap("\"Non-consecutive residues\"", GetWidth(), l, SetWrapFlags(),
            GetFeatIndent(), GetFeatIndent() + "/note=");
    }

    // format mandtory /estimated_length qualifier
    string estimated_length;
    if (gap.HasEstimatedLength()) {
        estimated_length = NStr::UIntToString(gap.GetEstimatedLength());
    } else {
        estimated_length = "unknown";
    }
    NStr::Wrap(estimated_length, GetWidth(), l, SetWrapFlags(),
        GetFeatIndent(), GetFeatIndent() + "/estimated_length=");

    text_os.AddParagraph(l, gap.GetObject());
}

END_SCOPE(objects)
END_NCBI_SCOPE
