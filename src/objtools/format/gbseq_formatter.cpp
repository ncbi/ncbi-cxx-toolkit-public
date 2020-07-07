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
*   GBseq formatting        
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrxml.hpp>

#include <objects/gbseq/GBSet.hpp>
#include <objects/gbseq/GBSeq.hpp>
#include <objects/gbseq/GBReference.hpp>
#include <objects/gbseq/GBKeyword.hpp>
#include <objects/gbseq/GBSeqid.hpp>
#include <objects/gbseq/GBFeature.hpp>
#include <objects/gbseq/GBInterval.hpp>
#include <objects/gbseq/GBQualifier.hpp>
#include <objects/gbseq/GBXref.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/impl/synonyms.hpp>

#include <objtools/format/text_ostream.hpp>
#include <objtools/format/gbseq_formatter.hpp>
#include <objtools/format/genbank_formatter.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/version_item.hpp>
#include <objtools/format/items/dbsource_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/items/primary_item.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/items/contig_item.hpp>
#include <objtools/format/items/genome_project_item.hpp>
#include <objtools/format/items/gap_item.hpp>
#include <objtools/format/items/wgs_item.hpp>
#include <objtools/format/items/tsa_item.hpp>
#include <objmgr/util/objutil.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// static functions

static void s_GBSeqStringCleanup(string& str, bool location = false)
{
    list<string> l;
    NStr::Split(str, " \n\r\t\b", l, NStr::fSplit_Tokenize);
    str = NStr::Join(l, " ");
    if ( location ) {
        str = NStr::Replace(str, ", ", ",");
    }
    NStr::TruncateSpacesInPlace(str);
}

/////////////////////////////////////////////////////////////////////////////
// Public

void CGBSeqFormatter::Reset()
{
    m_DidFeatStart = false;
    m_DidJourStart = false;
    m_DidKeysStart = false;
    m_DidRefsStart = false;
    m_DidWgsStart  = false;
    m_DidSequenceStart = false;
    m_NeedFeatEnd = false;
    m_NeedJourEnd = false;
    m_NeedRefsEnd = false;
    m_NeedWgsEnd  = false;
    m_NeedComment = false;
    m_NeedPrimary = false;
    m_NeedDbsource = false;
    m_NeedXrefs = false;
    m_OtherSeqIDs.clear();
    m_SecondaryAccns.clear();
    m_Comments.clear();
    m_Primary.clear();
    m_Dbsource.clear();
    m_Xrefs.clear();
}

// constructor
CGBSeqFormatter::CGBSeqFormatter(bool isInsd)
    : m_IsInsd(isInsd)
{
    Reset();
}

// detructor
CGBSeqFormatter::~CGBSeqFormatter(void) 
{
}


static string s_CombineStrings (const string& spaces, const string& tag, const string& value)

{
    return spaces + "<" + tag + ">" + NStr::XmlEncode(value) + "</" + tag + ">" + "\n";
}


static string s_CombineStrings (const string& spaces, const string& tag, int value)

{
    return spaces + "<" + tag + ">" + NStr::NumericToString(value) + "</" + tag + ">" + "\n";
}

static string s_AddAttribute (const string& spaces, const string& tag, const string& attribute,
                              const string& value)

{
    return spaces + "<" + tag + " " + attribute + "=\"" + value + "\"/>" + "\n";
}


static string s_OpenTag (const string& spaces, const string& tag)

{
    return spaces + "<" + tag + ">" + "\n";
}

static string s_OpenTagNoNewline (const string& spaces, const string& tag)

{
    return spaces + "<" + tag + ">";
}


static string s_CloseTag (const string& spaces, const string& tag)

{
    return spaces + "</" + tag + ">" + "\n";
}

void CGBSeqFormatter::Start(IFlatTextOStream& text_os)
{
    // x_WriteFileHeader(text_os);
        
    // x_StartWriteGBSet(text_os);
    text_os.Flush();
}


void CGBSeqFormatter::StartSection(const CStartSectionItem&, IFlatTextOStream& text_os)
{
    // ID-5736 : Reset internal variables before starting a new report
    Reset();
    m_GBSeq.Reset(new CGBSeq);
    // _ASSERT(m_GBSeq);
    string str;
    str.append( s_OpenTag("  ", "GBSeq"));

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str);
}


void CGBSeqFormatter::EndSection(const CEndSectionItem&, IFlatTextOStream& text_os)
{
    // x_WriteGBSeq(text_os);

    string str;

    if (m_NeedRefsEnd) {
        str.append( s_CloseTag("    ", "GBSeq_references"));
        m_NeedRefsEnd = false;
        m_DidRefsStart = false;
    }

    if (m_NeedComment) {
        m_NeedComment = false;

        string comm = NStr::Join( m_Comments, "; " );
        str.append( s_CombineStrings("    ", "GBSeq_comment", comm));
    }

    if (m_NeedPrimary) {
        m_NeedPrimary = false;
        str.append( s_CombineStrings("    ", "GBSeq_primary", m_Primary));
    }

    if (m_NeedFeatEnd) {
        str.append( s_CloseTag("    ", "GBSeq_feature-table"));
        m_NeedFeatEnd = false;
        m_DidFeatStart = false;
    }

    if (m_NeedWgsEnd) {
        str.append( s_CloseTag("    ", "GBSeq_alt-seq"));
        m_NeedWgsEnd = false;
        m_DidWgsStart = false;
    }

    // ID-4629 : Sequence is always the last element in the section, except for
    // possibly sequence xrefs, so only 1 boolean variable is sufficient to
    // control when to close the tag.
    // Also sequence closing tag is placed without a newline, hence no spaces are
    // needed.
    if (m_DidSequenceStart) {
        str.append( s_CloseTag("", "GBSeq_sequence"));
        m_DidSequenceStart = false;
    }

    if (m_NeedXrefs) {
        m_NeedXrefs = false;

        str.append( s_OpenTag("    ", "GBSeq_xrefs"));

        bool firstOfPair = true;

        FOR_EACH_STRING_IN_LIST (xr, m_Xrefs) {
            if (firstOfPair) {
                firstOfPair = false;
                str.append( s_OpenTag("      ", "GBXref"));
                str.append( s_CombineStrings("        ", "GBXref_dbname", *xr));
            } else {
                firstOfPair = true;
                str.append( s_CombineStrings("        ", "GBXref_id", *xr));
                str.append( s_CloseTag("      ", "GBXref"));
            }
        }

        str.append( s_CloseTag("    ", "GBSeq_xrefs"));

    }

    str.append( s_CloseTag("  ", "GBSeq"));

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, NULL, IFlatTextOStream::eAddNewline_No);

    text_os.Flush();

    m_GBSeq.Reset();
    // _ASSERT(!m_GBSeq);
}


void CGBSeqFormatter::End(IFlatTextOStream& text_os)
{
    // x_EndWriteGBSet(text_os);
    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// Locus
//


CGBSeq::TStrandedness s_GBSeqStrandedness(
    CSeq_inst::TStrand strand,
    CMolInfo::TBiomol eBiomol // moltype needed to determine defaults if unset
    )
{
    switch ( strand ) {
    case CSeq_inst::eStrand_ss:
        return "single";  // eStrandedness_single_stranded
    case CSeq_inst::eStrand_ds:
        return "double";  // eStrandedness_double_stranded
    case CSeq_inst::eStrand_mixed:
        return "mixed";  // eStrandedness_mixed_stranded
    case CSeq_inst::eStrand_other:
    case CSeq_inst::eStrand_not_set:
    default:
        break;
    }

    // not set, so try to use eBiomol to figure it out
    switch( eBiomol ) {
    case CMolInfo::eBiomol_genomic:
        return "double"; // DNA defaults to double-stranded
    case CMolInfo::eBiomol_peptide:
        // peptides default to single-stranded
        return "single";
    default: {
        // we're not sure about the enum type, so we check if
        // it's text name gives us something to work with

        const CEnumeratedTypeValues * pBiomolEnumInfo = 
            CMolInfo::GetTypeInfo_enum_EBiomol();
        if( pBiomolEnumInfo ) {
            CEnumeratedTypeValues::TValueToName::const_iterator find_iter =
                pBiomolEnumInfo->ValueToName().find(eBiomol);
            if( find_iter != pBiomolEnumInfo->ValueToName().end() ) {
                const string *psBiomolName = find_iter->second;

                // RNA types default to single-strand
                if( NStr::Find(*psBiomolName, "RNA") != NPOS ) {
                    return "single";
                }
            }
        }

        break;
    }
    }

    return kEmptyStr;  // eStrandedness_not_set;
    
}


CGBSeq::TMoltype s_GBSeqMoltype(CMolInfo::TBiomol biomol)
{
    // ID-4736 : there are 4 special cases of RNA molecules that are rendered
    // with full Biomol string on the LOCUS line: mRNA, rRNA, tRNA and cRNA.
    // All other RNA types are shown as just RNA, except for genomic_mRNA, which
    // is shown as DNA. The remaining Biomol types are shown as DNA.
    switch ( biomol ) {
    case CMolInfo::eBiomol_unknown:
        return kEmptyStr;  // eMoltype_nucleic_acid
    case CMolInfo::eBiomol_genomic_mRNA:
        return "DNA";  // eMoltype_dna
    case CMolInfo::eBiomol_mRNA:
        return "mRNA";  // eMoltype_mrna
    case CMolInfo::eBiomol_rRNA:
        return "rRNA";  // eMoltype_rrna
    case CMolInfo::eBiomol_tRNA:
        return "tRNA";  // eMoltype_trna
    case CMolInfo::eBiomol_cRNA:
        return "cRNA";  // eMoltype_crna
    case CMolInfo::eBiomol_peptide:
        return "AA";  // eMoltype_peptide
    default:
    {
        // For the remaining cases, if the biomol string contains "RNA",
        // return "RNA", otherwise return "DNA".
        string biomol_str =
            CMolInfo::GetTypeInfo_enum_EBiomol()->FindName(biomol,true);
        if (biomol_str.find("RNA") != NPOS)
            return "RNA";
        else
            return "DNA";
        break;

    }
    }
    return kEmptyStr;  // eMoltype_nucleic_acid
}


CGBSeq::TTopology s_GBSeqTopology(CSeq_inst::TTopology topology)
{
    if ( topology == CSeq_inst::eTopology_circular ) {
        return "circular";  // eTopology_circular
    }
    return "linear";  // eTopology_linear
}


string s_GetDate(const CBioseq_Handle& bsh, CSeqdesc::E_Choice choice)
{
    _ASSERT(choice == CSeqdesc::e_Update_date  ||
            choice == CSeqdesc::e_Create_date);
    CSeqdesc_CI desc(bsh, choice);
    if ( desc ) {
        string result;
        if ( desc->IsUpdate_date() ) {
            DateToString(desc->GetUpdate_date(), result);
        } else {
            DateToString(desc->GetCreate_date(), result);
        }
        return result;
    }

    return "01-JAN-1900";
}

void CGBSeqFormatter::FormatLocus
(const CLocusItem& locus, 
 IFlatTextOStream& text_os)
{
    CBioseqContext& ctx = *locus.GetContext();

    string str;

    str.append( s_CombineStrings("    ", "GBSeq_locus", locus.GetName()));

    str.append( s_CombineStrings("    ", "GBSeq_length", locus.GetLength()));

   CGBSeq::TStrandedness sStrandedness =
        s_GBSeqStrandedness(locus.GetStrand(), locus.GetBiomol());
    if( ! sStrandedness.empty() ) {
        str.append( s_CombineStrings("    ", "GBSeq_strandedness", sStrandedness));
    }

    CGBSeq::TMoltype sMolType = s_GBSeqMoltype(locus.GetBiomol());
    if( ! sMolType.empty() ) {
        str.append( s_CombineStrings("    ", "GBSeq_moltype", sMolType));
    } else if (ctx.IsProt()) {
        str.append( s_CombineStrings("    ", "GBSeq_moltype", "AA"));
    }
 
    str.append( s_CombineStrings("    ", "GBSeq_topology", s_GBSeqTopology(locus.GetTopology())));

    str.append( s_CombineStrings("    ", "GBSeq_division", locus.GetDivision()));

    str.append( s_CombineStrings("    ", "GBSeq_update-date", s_GetDate(ctx.GetHandle(), CSeqdesc::e_Update_date)));

    str.append( s_CombineStrings("    ", "GBSeq_create-date", s_GetDate(ctx.GetHandle(), CSeqdesc::e_Create_date)));

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, locus.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// Definition

void CGBSeqFormatter::FormatDefline
(const CDeflineItem& defline,
 IFlatTextOStream& text_os)
{
    string str;

    string def = defline.GetDefline();
    if ( NStr::EndsWith(def, '.') ) {
        def.resize(def.length() - 1);
    }

    str.append( s_CombineStrings("    ", "GBSeq_definition", def));

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, defline.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// Accession

void CGBSeqFormatter::FormatAccession
(const CAccessionItem& acc, 
 IFlatTextOStream& text_os)
{
    CBioseqContext& ctx = *acc.GetContext();

    string str;

    str.append( s_CombineStrings("    ", "GBSeq_primary-accession", acc.GetAccession()));

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, acc.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();

    bool hasOthers = false;
    string others;
    ITERATE (CBioseq::TId, it, ctx.GetBioseqIds()) {
        others.append( s_CombineStrings("      ", "GBSeqid", CGBSeqid((*it)->AsFastaString())));
        hasOthers = true;
    }
    if (hasOthers) {
        m_OtherSeqIDs = others;
    }

    bool hasExtras = false;
    string extras;
    ITERATE (CAccessionItem::TExtra_accessions, it, acc.GetExtraAccessions()) {
        extras.append( s_CombineStrings("      ", "GBSecondary-accn", CGBSecondary_accn(*it)));
        hasExtras = true;
    }
    if (hasExtras) {
        m_SecondaryAccns = extras;
    }

}


///////////////////////////////////////////////////////////////////////////
//
// Version

void CGBSeqFormatter::FormatVersion
(const CVersionItem& version,
 IFlatTextOStream& text_os)
{
    string str;

    str.append( s_CombineStrings("    ", "GBSeq_accession-version", version.GetAccession()));

    if (! m_OtherSeqIDs.empty()) {
        str.append( s_OpenTag("    ", "GBSeq_other-seqids"));
        str.append( m_OtherSeqIDs);
        str.append( s_CloseTag("    ", "GBSeq_other-seqids"));
    }

    if (! m_SecondaryAccns.empty()) {
        str.append( s_OpenTag("    ", "GBSeq_secondary-accessions"));
        str.append( m_SecondaryAccns);
        str.append( s_CloseTag("    ", "GBSeq_secondary-accessions"));
    }

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, version.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// DBLink

void CGBSeqFormatter::FormatGenomeProject
(const CGenomeProjectItem& gp,
 IFlatTextOStream& text_os)
{
    string str;

    CGenomeProjectItem::TDBLinkLineVec dblinklines = gp.GetDBLinkLines();
    if (dblinklines.size() == 0) return;

    ITERATE( CGenomeProjectItem::TDBLinkLineVec, gp_it, dblinklines ) {
        string line = *gp_it;
        string first;
        string second;
        list<string> ids;
        NStr::SplitInTwo( line, ":", first, second );
        first = NStr::TruncateSpaces(first);
        NStr::Split(second, ",", ids, NStr::fSplit_Tokenize);
        FOR_EACH_STRING_IN_LIST (s_itr, ids) {
            string id = *s_itr;
            id = NStr::TruncateSpaces(id);
            m_Xrefs.push_back(first);
            m_Xrefs.push_back(id);
            m_NeedXrefs = true;
            if (NStr::EqualNocase(first, "BioProject")) {
                str.append( s_CombineStrings("    ", "GBSeq_project", id));
            }
        }
    }

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, gp.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// Segment

void CGBSeqFormatter::FormatSegment
(const CSegmentItem& seg,
 IFlatTextOStream& text_os)
{
    string str = "    <GBSeq_segment>" + NStr::NumericToString(seg.GetNum()) + " of " + NStr::NumericToString(seg.GetCount()) + "</GBSeq_segment>\n";

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, seg.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// Source

void CGBSeqFormatter::FormatSource
(const CSourceItem& source,
 IFlatTextOStream& text_os)
{
    string str;

    string source_line = source.GetOrganelle() + source.GetTaxname();
    if ( !source.GetCommon().empty() ) {
        source_line.append( (source.IsUsingAnamorph() ? " (anamorph: " : " (") + source.GetCommon() + ")");
    }
    str.append( s_CombineStrings("    ", "GBSeq_source", source_line));

    str.append( s_CombineStrings("    ", "GBSeq_organism", source.GetTaxname()));

    const string & sTaxonomy = source.GetLineage();
    string staxon = sTaxonomy;
    if( NStr::EndsWith(staxon, ".") ) {
        staxon.resize( staxon.length() - 1);
    }
    str.append( s_CombineStrings("    ", "GBSeq_taxonomy", staxon));

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, source.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// String Cache

void CGBSeqFormatter::FormatCache
(const CCacheItem& csh,
 IFlatTextOStream& text_os)
{
    if ( csh.Skip() ) {
        return;
    }

    vector<string>* rcx = csh.GetCache();
    if (rcx) {
        for (auto& str : *rcx) {
            text_os.AddLine(str);
        }
    }
}


///////////////////////////////////////////////////////////////////////////
//
// Keywords

void CGBSeqFormatter::FormatKeywords
(const CKeywordsItem& keys,
 IFlatTextOStream& text_os)
{
    string str;

    ITERATE (CKeywordsItem::TKeywords, it, keys.GetKeywords()) {
        if (! m_DidKeysStart) {
            str.append( s_OpenTag("    ", "GBSeq_keywords"));
            m_DidKeysStart = true;
        }
        str.append( s_CombineStrings("      ", "GBKeyword", CGBKeyword(*it)));
    }
    if (m_DidKeysStart) {
        str.append( s_CloseTag("    ", "GBSeq_keywords"));
        m_DidKeysStart = false;
    }

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, keys.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// REFERENCE

void CGBSeqFormatter::FormatReference
(const CReferenceItem& ref,
 IFlatTextOStream& text_os)
{
    string str;

    if (! m_DidRefsStart) {
        str.append( s_OpenTag("    ", "GBSeq_references"));
        m_DidRefsStart = true;
        m_NeedRefsEnd = true;
    }

    str.append( s_OpenTag("      ", "GBReference"));

    CBioseqContext& ctx = *ref.GetContext();

    str.append( s_CombineStrings("        ", "GBReference_reference", ref.GetSerial()));

    string refstr;
    const CSeq_loc* loc = &ref.GetLoc();
    const char* pchDelim = "";
    for ( CSeq_loc_CI it(*loc);  it;  ++it ) {
        CSeq_loc_CI::TRange range = it.GetRange();
        if ( range.IsWhole() ) {
            range.SetTo(sequence::GetLength(it.GetSeq_id(), &ctx.GetScope()) - 1);
        }
        refstr.append( pchDelim + NStr::NumericToString(range.GetFrom() + 1) + ".." + NStr::NumericToString(range.GetTo() + 1));
        pchDelim = "; ";
    }
    str.append( s_CombineStrings("        ", "GBReference_position", refstr));

    list<string> authors;
    if (ref.IsSetAuthors()) {
        CReferenceItem::GetAuthNames(ref.GetAuthors(), authors);
        bool hasAuthors = false;
        ITERATE (list<string>, it, authors) {
            if (! hasAuthors) {
                str.append( s_OpenTag("        ", "GBReference_authors"));
                hasAuthors = true;
            }
            str.append( s_CombineStrings("          ", "GBAuthor", *it));
        }
        if (hasAuthors) {
            str.append( s_CloseTag("        ", "GBReference_authors"));
        }
    }
    if ( !ref.GetConsortium().empty() ) {
        str.append( s_CombineStrings("        ", "GBReference_consortium", ref.GetConsortium()));
    }
    if ( !ref.GetTitle().empty() ) {
        if ( NStr::EndsWith(ref.GetTitle(), '.') ) {
            string title = ref.GetTitle();
            title.resize(title.length() - 1);
            str.append( s_CombineStrings("        ", "GBReference_title", title));
        } else {
            str.append( s_CombineStrings("        ", "GBReference_title", ref.GetTitle()));
        }
    }
    string journal;
    CGenbankFormatter genbank_formatter;
    x_FormatRefJournal(ref, journal, ctx);
    NON_CONST_ITERATE (string, it, journal) {
        if ( (*it == '\n')  ||  (*it == '\t')  ||  (*it == '\r') ) {
            *it = ' ';
        }
    }
    if ( !journal.empty() ) {
        str.append( s_CombineStrings("        ", "GBReference_journal", journal));
    }
    string doi = ref.GetDOI();
    if ( ! doi.empty() ) {
        str.append( s_OpenTag("        ", "GBReference_xref"));
        str.append( s_OpenTag("          ", "GBXref"));
        str.append( s_CombineStrings("            ", "GBXref_dbname", "doi"));
        str.append( s_CombineStrings("            ", "GBXref_id", doi));
        str.append( s_CloseTag("          ", "GBXref"));
        str.append( s_CloseTag("        ", "GBReference_xref"));
    }
    if ( ref.GetPMID() != ZERO_ENTREZ_ID ) {
        str.append( s_CombineStrings("        ", "GBReference_pubmed", ENTREZ_ID_TO(int, ref.GetPMID())));
    }
    if ( !ref.GetRemark().empty() ) {
        str.append( s_CombineStrings("        ", "GBReference_remark", ref.GetRemark()));
    }

    str.append( s_CloseTag("      ", "GBReference"));

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, ref.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();
}

///////////////////////////////////////////////////////////////////////////
//
// COMMENT


void CGBSeqFormatter::FormatComment
(const CCommentItem& comment,
 IFlatTextOStream& text_os)
{
    string comm = NStr::Join( comment.GetCommentList(), "; " );
    s_GBSeqStringCleanup(comm);

    m_Comments.push_back(comm);
    m_NeedComment = true;
}

///////////////////////////////////////////////////////////////////////////
//
// PRIMARY


void CGBSeqFormatter::FormatPrimary
(const CPrimaryItem& primary,
 IFlatTextOStream& text_os)
{
    m_Primary = primary.GetString();
    NStr::ReplaceInPlace(m_Primary, "\n", "~");
    m_NeedPrimary = true;
}

///////////////////////////////////////////////////////////////////////////
//
// DBSOURCE


void CGBSeqFormatter::FormatDBSource
(const CDBSourceItem& dbs,
 IFlatTextOStream& text_os)
{
    if (! dbs.GetDBSource().empty()) {
        ITERATE (list<string>, it, dbs.GetDBSource()) {
            string db_src = *it;
            m_Dbsource.push_back(db_src);
            m_NeedDbsource = true;
        }
    }
}

///////////////////////////////////////////////////////////////////////////
//
// FEATURES

void CGBSeqFormatter::FormatFeature
(const CFeatureItemBase& f,
 IFlatTextOStream& text_os)
{
    string str;

    if (m_NeedRefsEnd) {
        str.append( s_CloseTag("    ", "GBSeq_references"));
        m_NeedRefsEnd = false;
        m_DidRefsStart = false;
    }

    if (m_NeedComment) {
        m_NeedComment = false;

        string comm = NStr::Join( m_Comments, "; " );
        str.append( s_CombineStrings("    ", "GBSeq_comment", comm));
    }

    if (m_NeedPrimary) {
        m_NeedPrimary = false;
        str.append( s_CombineStrings("    ", "GBSeq_primary", m_Primary));
    }

    if (m_NeedDbsource) {
        m_NeedDbsource = false;

        string dbsrc = NStr::Join( m_Dbsource, "; " );
        str.append( s_CombineStrings("    ", "GBSeq_source-db", dbsrc));
    }

    if (! m_DidFeatStart) {
        str.append( s_OpenTag("    ", "GBSeq_feature-table"));
        m_DidFeatStart = true;
        m_NeedFeatEnd = true;
    }

    str.append( s_OpenTag("      ", "GBFeature"));

    CConstRef<CFlatFeature> feat = f.Format();

    str.append( s_CombineStrings("        ", "GBFeature_key", feat->GetKey()));

    string location = feat->GetLoc().GetString();
    s_GBSeqStringCleanup(location, true);
    str.append( s_CombineStrings("        ", "GBFeature_location", location));

    str.append( s_OpenTag("        ", "GBFeature_intervals"));

    const CSeq_loc& loc = f.GetLoc();
    CScope& scope = f.GetContext()->GetScope();
    for (CSeq_loc_CI it(loc); it; ++it) {
        str.append( s_OpenTag("          ", "GBInterval"));

        CSeq_loc_CI::TRange range = it.GetRange();
        if ( range.GetLength() == 1 ) {  // point
            str.append( s_CombineStrings("            ", "GBInterval_point", range.GetFrom() + 1));
        } else {
            TSeqPos from, to;
            if ( range.IsWhole() ) {
                from = 1;
                to = sequence::GetLength(it.GetEmbeddingSeq_loc(), &scope);
            } else {
                from = range.GetFrom() + 1;
                to = range.GetTo() + 1;
            }
            if ( it.GetStrand() == eNa_strand_minus ) {
                swap(from, to);
            }
            str.append( s_CombineStrings("            ", "GBInterval_from", from));
            str.append( s_CombineStrings("            ", "GBInterval_to", to));
            if ( it.GetStrand() == eNa_strand_minus )
                str.append( s_AddAttribute("            ", "GBInterval_iscomp", "value", "true"));
        }

        CConstRef<CSeq_id> best(&it.GetSeq_id());
        if ( best->IsGi() ) {
            CConstRef<CSynonymsSet> syns = scope.GetSynonyms(*best);
            vector< CRef<CSeq_id> > ids;
            ITERATE (CSynonymsSet, id_iter, *syns) {
                CConstRef<CSeq_id> id =
                    syns->GetSeq_id_Handle(id_iter).GetSeqId();
                CRef<CSeq_id> sip(const_cast<CSeq_id*>(id.GetPointerOrNull()));
                ids.push_back(sip);
            }
            best.Reset(FindBestChoice(ids, CSeq_id::Score));
        }
        str.append( s_CombineStrings("            ", "GBInterval_accession", best->GetSeqIdString(true)));

        str.append( s_CloseTag("          ", "GBInterval"));
    }

    str.append( s_CloseTag("        ", "GBFeature_intervals"));

    if ( NStr::Find(location, "join") != NPOS ) {
        str.append( s_CombineStrings("        ", "GBFeature_operator", "join"));
    } else if ( NStr::Find(location, "order") != NPOS ) {
        str.append( s_CombineStrings("        ", "GBFeature_operator", "order"));
    }

    if ( loc.IsPartialStart(eExtreme_Biological) ) {
        str.append( "        <GBFeature_partial5 value=\"true\"/>\n");
    }
    if ( loc.IsPartialStop(eExtreme_Biological) ) {
        str.append( "        <GBFeature_partial3 value=\"true\"/>\n");
    }

    if ( !feat->GetQuals().empty() ) {
        str.append( s_OpenTag("        ", "GBFeature_quals"));

        const CFlatFeature::TQuals& quals = feat->GetQuals();
        ITERATE (CFlatFeature::TQuals, it, quals) {
            str.append( s_OpenTag("          ", "GBQualifier"));
            str.append( s_CombineStrings("            ", "GBQualifier_name", (*it)->GetName()));
            if ((*it)->GetStyle() != CFormatQual::eEmpty) {
               str.append( s_CombineStrings("            ", "GBQualifier_value", (*it)->GetValue()));
            }
            str.append( s_CloseTag("          ", "GBQualifier"));
        }

        str.append( s_CloseTag("        ", "GBFeature_quals"));
    }

    str.append( s_CloseTag("      ", "GBFeature"));

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, f.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// SEQUENCE

void CGBSeqFormatter::FormatSequence
(const CSequenceItem& seq,
 IFlatTextOStream& text_os)
{
    string str;

    if (m_NeedRefsEnd) {
        str.append( s_CloseTag("    ", "GBSeq_references"));
        m_NeedRefsEnd = false;
        m_DidRefsStart = false;
    }

    if (m_NeedComment) {
        m_NeedComment = false;

        string comm = NStr::Join( m_Comments, "; " );
        str.append( s_CombineStrings("    ", "GBSeq_comment", comm));
    }

    if (m_NeedPrimary) {
        m_NeedPrimary = false;
        str.append( s_CombineStrings("    ", "GBSeq_primary", m_Primary));
    }

    if (m_NeedFeatEnd) {
        str.append( s_CloseTag("    ", "GBSeq_feature-table"));
        m_NeedFeatEnd = false;
        m_DidFeatStart = false;
    }

    string data;

    TSeqPos from = seq.GetFrom();
    TSeqPos to = seq.GetTo();

    TSeqPos vec_pos = from-1;
    TSeqPos total = from <= to? to - from + 1 : 0;
    CSeqVector_CI vec_ci(seq.GetSequence(), vec_pos,
                         CSeqVector_CI::eCaseConversion_lower);
    vec_ci.GetSeqData(data, total);

    if (seq.IsFirst()) {
        str.append( s_OpenTagNoNewline("    ", "GBSeq_sequence"));
        m_DidSequenceStart = true;
    }

    str.append(data);

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, seq.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// CONTIG

void CGBSeqFormatter::FormatContig
(const CContigItem& contig,
 IFlatTextOStream& text_os)
{
    string str;

    if (m_NeedRefsEnd) {
        str.append( s_CloseTag("    ", "GBSeq_references"));
        m_NeedRefsEnd = false;
        m_DidRefsStart = false;
    }

    if (m_NeedComment) {
        m_NeedComment = false;

        string comm = NStr::Join( m_Comments, "; " );
        str.append( s_CombineStrings("    ", "GBSeq_comment", comm));
    }

    if (m_NeedPrimary) {
        m_NeedPrimary = false;
        str.append( s_CombineStrings("    ", "GBSeq_primary", m_Primary));
    }

    if (m_NeedFeatEnd) {
        str.append( s_CloseTag("    ", "GBSeq_feature-table"));
        m_NeedFeatEnd = false;
        m_DidFeatStart = false;
    }

    // ID-4736 : pass a flag (argument 4) to CFlatSeqLoc to prescribe adding a
    // join(...) wrapper even to a whole location.
    string assembly = 
        CFlatSeqLoc(contig.GetLoc(), *contig.GetContext(), 
                    CFlatSeqLoc::eType_assembly, false, true).GetString();
    s_GBSeqStringCleanup(assembly, true);

    str.append( s_CombineStrings("    ", "GBSeq_contig", assembly));

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, contig.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// GAPS

void CGBSeqFormatter::FormatGap(const CGapItem& gap, IFlatTextOStream& text_os)
{
    string str;

    // Close the preceding sections and open the feature section,
    // if not yet done.

    if (m_NeedRefsEnd) {
        str.append( s_CloseTag("    ", "GBSeq_references"));
        m_NeedRefsEnd = false;
        m_DidRefsStart = false;
    }

    if (m_NeedComment) {
        m_NeedComment = false;

        string comm = NStr::Join( m_Comments, "; " );
        str.append( s_CombineStrings("    ", "GBSeq_comment", comm));
    }

    if (m_NeedPrimary) {
        m_NeedPrimary = false;
        str.append( s_CombineStrings("    ", "GBSeq_primary", m_Primary));
    }

    if (m_NeedDbsource) {
        m_NeedDbsource = false;

        string dbsrc = NStr::Join( m_Dbsource, "; " );
        str.append( s_CombineStrings("    ", "GBSeq_source-db", dbsrc));
    }

    if (! m_DidFeatStart) {
        str.append( s_OpenTag("    ", "GBSeq_feature-table"));
        m_DidFeatStart = true;
        m_NeedFeatEnd = true;
    }

    str.append( s_OpenTag("      ", "GBFeature"));

    str.append( s_CombineStrings("        ", "GBFeature_key", gap.GetFeatureName()));


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
    str.append( s_CombineStrings("        ", "GBFeature_location", loc));

    str.append( s_OpenTag("        ", "GBFeature_intervals"));
    str.append( s_OpenTag("          ", "GBInterval"));
    str.append( s_CombineStrings("            ", "GBInterval_from", gapStart));
    str.append( s_CombineStrings("            ", "GBInterval_to", gapEnd));
    if (gap.GetContext() && !gap.GetContext()->GetAccession().empty()) {
        str.append( s_CombineStrings("            ", "GBInterval_accession",
                                     gap.GetContext()->GetAccession()));
    }
    str.append( s_CloseTag("          ", "GBInterval"));

    str.append( s_CloseTag("        ", "GBFeature_intervals"));

    str.append( s_OpenTag("        ", "GBFeature_quals"));
    // size zero gaps indicate non-consecutive residues
    if( isGapOfLengthZero ) {
        str.append( s_OpenTag("          ", "GBQualifier"));
        str.append ( s_CombineStrings("            ", "GBQualifier_name", "note"));
        str.append ( s_CombineStrings("            ", "GBQualifier_value",
                                      "Non-consecutive residues"));
        str.append( s_CloseTag("          ", "GBQualifier"));
    }

    // format mandatory /estimated_length qualifier
    string estimated_length;
    if (gap.HasEstimatedLength()) {
        estimated_length = NStr::UIntToString(gap.GetEstimatedLength());
    } else {
        estimated_length = "unknown";
    }
    str.append( s_OpenTag("          ", "GBQualifier"));
    str.append ( s_CombineStrings("            ", "GBQualifier_name", "estimated_length"));
    str.append ( s_CombineStrings("            ", "GBQualifier_value", estimated_length));
    str.append( s_CloseTag("          ", "GBQualifier"));

    // format /gap_type
    if( gap.HasType() ) {
        str.append( s_OpenTag("          ", "GBQualifier"));
        str.append ( s_CombineStrings("            ", "GBQualifier_name", "gap_type"));
        str.append ( s_CombineStrings("            ", "GBQualifier_value", gap.GetType()));
        str.append( s_CloseTag("          ", "GBQualifier"));
    }

    // format /linkage_evidence
    if( gap.HasEvidence() ) {
        ITERATE( CGapItem::TEvidence, evidence_iter, gap.GetEvidence() ) {
            str.append( s_OpenTag("          ", "GBQualifier"));
            str.append ( s_CombineStrings("            ", "GBQualifier_name", "linkage_evidence"));
            str.append ( s_CombineStrings("            ", "GBQualifier_value", *evidence_iter));
            str.append( s_CloseTag("          ", "GBQualifier"));
        }
    }

    str.append( s_CloseTag("        ", "GBFeature_quals"));
    str.append( s_CloseTag("      ", "GBFeature"));

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, gap.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();

}

void CGBSeqFormatter::FormatWGS(const CWGSItem& wgs, IFlatTextOStream& text_os)
{
    string name;

    switch ( wgs.GetType() ) {
    case CWGSItem::eWGS_Projects:
        name = "WGS"; break;
    case CWGSItem::eWGS_ScaffoldList:
        name = "WGS_SCAFLD"; break;
    case CWGSItem::eWGS_ContigList:
        name = "WGS_CONTIG"; break;
    default: return;
    }

    x_FormatAltSeq(wgs, name, text_os);
}

void CGBSeqFormatter::FormatTSA(const CTSAItem& tsa, IFlatTextOStream& text_os)
{
    string name;

    switch ( tsa.GetType() ) {
    case CTSAItem::eTSA_Projects:
        name = "TSA"; break;
    case CTSAItem::eTLS_Projects:
        name = "TLS"; break;
    default: return;
    }
 
    x_FormatAltSeq(tsa, name, text_os);
}

template <typename T> void 
CGBSeqFormatter::x_FormatAltSeq(const T& item, const string& name,
                                IFlatTextOStream& text_os)
{
    string str;

    // Close the preceding sections and open the feature section,
    // if not yet done.

    if (m_NeedRefsEnd) {
        str.append( s_CloseTag("    ", "GBSeq_references"));
        m_NeedRefsEnd = false;
        m_DidRefsStart = false;
    }

    if (m_NeedComment) {
        m_NeedComment = false;

        string comm = NStr::Join( m_Comments, "; " );
        str.append( s_CombineStrings("    ", "GBSeq_comment", comm));
    }

    if (m_NeedPrimary) {
        m_NeedPrimary = false;
        str.append( s_CombineStrings("    ", "GBSeq_primary", m_Primary));
    }

    if (m_NeedDbsource) {
        m_NeedDbsource = false;

        string dbsrc = NStr::Join( m_Dbsource, "; " );
        str.append( s_CombineStrings("    ", "GBSeq_source-db", dbsrc));
    }

    if (m_NeedFeatEnd) {
        str.append( s_CloseTag("    ", "GBSeq_feature-table"));
        m_NeedFeatEnd = false;
        m_DidFeatStart = false;
    }

    if (!m_DidWgsStart) {
        str.append( s_OpenTag("    ", "GBSeq_alt-seq"));
        m_DidWgsStart = true;
        m_NeedWgsEnd = true;
    }

    str.append( s_OpenTag("      ", "GBAltSeqData"));
    str.append( s_CombineStrings("        ", "GBAltSeqData_name", name));
    str.append( s_OpenTag("        ", "GBAltSeqData_items"));
    str.append( s_OpenTag("          ", "GBAltSeqItem"));

    // Get first and last id (sanitized for html, if necessary)
    list<string> l;
    string first_id = item.GetFirstID();
    string last_id = item.GetLastID();

    str.append( s_CombineStrings("          ", "GBAltSeqItem_first-accn", first_id));
    if (first_id != last_id)
        str.append( s_CombineStrings("          ", "GBAltSeqItem_last-accn", last_id));

    str.append( s_CloseTag("          ", "GBAltSeqItem"));
    str.append( s_CloseTag("        ", "GBAltSeqData_items"));
    str.append( s_CloseTag("      ", "GBAltSeqData"));

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, item.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();
}

//=========================================================================//
//                                Private                                  //
//=========================================================================//


void CGBSeqFormatter::x_WriteFileHeader(IFlatTextOStream& text_os)
{
    m_Out.reset(CObjectOStream::Open(eSerial_Xml, m_StrStream));
}

void CGBSeqFormatter::x_WriteGBSeq(IFlatTextOStream& text_os)
{
    m_Out->WriteObject(ConstObjectInfo(*m_GBSeq));
    x_StrOStreamToTextOStream(text_os);
}


void CGBSeqFormatter::x_StrOStreamToTextOStream(IFlatTextOStream& text_os)
{
    list<string> l;

    // flush ObjectOutputStream to underlying strstream
    m_Out->Flush();
    // read text from strstream
    NStr::Split((string)CNcbiOstrstreamToString(m_StrStream), "\n", l, NStr::fSplit_Tokenize);
    // convert GBseq to INSDSeq
    if ( m_IsInsd ) {
        for (string& str : l) {
            NStr::ReplaceInPlace(str, "<GB", "<INSD");
            NStr::ReplaceInPlace(str,"</GB", "</INSD");
        }
    }
    // add text to TextOStream
    text_os.AddParagraph(l);
    // reset strstream
    m_StrStream.seekp(0);
#ifdef NCBI_SHUN_OSTRSTREAM
    m_StrStream.str(kEmptyStr);
#endif
}


END_SCOPE(objects)
END_NCBI_SCOPE
