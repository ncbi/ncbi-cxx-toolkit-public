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
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/items/contig_item.hpp>
#include <objtools/format/items/genome_project_item.hpp>
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


static void s_GBSeqQualCleanup(string& val)
{
    
    val = NStr::Replace(val, "\"", " ");
    s_GBSeqStringCleanup(val);
}


/////////////////////////////////////////////////////////////////////////////
// Public


// constructor
CGBSeqFormatter::CGBSeqFormatter(bool isInsd)
    : m_IsInsd(isInsd)
{
    m_DidFeatStart = false;
    m_DidJourStart = false;
    m_DidKeysStart = false;
    m_DidRefsStart = false;
    m_NeedFeatEnd = false;
    m_NeedJourEnd = false;
    m_NeedKeysEnd = false;
    m_NeedRefsEnd = false;
    m_NeedComment = false;
    m_NeedDbsource = false;
    m_NeedXrefs = false;
    m_OtherSeqIDs.clear();
    m_SecondaryAccns.clear();
    m_Comments.clear();
    m_Dbsource.clear();
    m_Xrefs.clear();
}

// detructor
CGBSeqFormatter::~CGBSeqFormatter(void) 
{
}


static string s_CombineStrings (string spaces, string tag, string value)

{
    string str = spaces + "<" + tag + ">" + NStr::XmlEncode(value) + "</" + tag + ">" + "\n";

    return str;
}


static string s_CombineStrings (string spaces, string tag, int value)

{
    string str = spaces + "<" + tag + ">" + NStr::NumericToString(value) + "</" + tag + ">" + "\n";

    return str;
}


static string s_OpenTag (string spaces, string tag)

{
    string str = spaces + "<" + tag + ">" + "\n";

    return str;
}


static string s_CloseTag (string spaces, string tag)

{
    string str = spaces + "</" + tag + ">" + "\n";

    return str;
}


void CGBSeqFormatter::Start(IFlatTextOStream& text_os)
{
    // x_WriteFileHeader(text_os);
        
    // x_StartWriteGBSet(text_os);

    CNcbiOstrstream tmp;

    // tmp << s_OpenTag("  ", "GBSeq");
    tmp << "  <GBSeq>";

    string str = (string)CNcbiOstrstreamToString(tmp);

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str);

    text_os.Flush();
}


void CGBSeqFormatter::StartSection(const CStartSectionItem&, IFlatTextOStream& text_os)
{
    m_GBSeq.Reset(new CGBSeq);
    // _ASSERT(m_GBSeq);
}


void CGBSeqFormatter::EndSection(const CEndSectionItem&, IFlatTextOStream& text_os)
{
    // x_WriteGBSeq(text_os);

    CNcbiOstrstream tmp;

    if (m_NeedRefsEnd) {
        tmp << s_CloseTag("    ", "GBSeq_references");
        m_NeedRefsEnd = false;
    }

    if (m_NeedComment) {
        m_NeedComment = false;

        string comm = NStr::Join( m_Comments, "; " );
        tmp << s_CombineStrings("    ", "GBSeq_comment", comm);
    }

    if (m_NeedFeatEnd) {
        tmp << s_CloseTag("    ", "GBSeq_feature-table");
        m_NeedFeatEnd = false;
    }

    if (m_NeedXrefs) {
        m_NeedXrefs = false;

        tmp << s_OpenTag("    ", "GBSeq_xrefs");

        bool firstOfPair = true;

        FOR_EACH_STRING_IN_LIST (str, m_Xrefs) {
            if (firstOfPair) {
                firstOfPair = false;
                tmp << s_OpenTag("      ", "GBXref");
                tmp << s_CombineStrings("        ", "GBXref_dbname", *str);
            } else {
                firstOfPair = true;
                tmp << s_CombineStrings("        ", "GBXref_id", *str);
                tmp << s_CloseTag("      ", "GBXref");
            }
        }

        tmp << s_CloseTag("    ", "GBSeq_xrefs");

    }

    string str = (string)CNcbiOstrstreamToString(tmp);

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

    CNcbiOstrstream tmp;

    // tmp << s_CloseTag("  ", "GBSeq");
    tmp << "  </GBSeq>";

    string str = (string)CNcbiOstrstreamToString(tmp);

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str);

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
    switch ( biomol ) {
    case CMolInfo::eBiomol_unknown:
        return kEmptyStr;  // eMoltype_nucleic_acid
    case CMolInfo::eBiomol_genomic:
    case CMolInfo::eBiomol_other_genetic:
    case CMolInfo::eBiomol_genomic_mRNA:
        return "DNA";  // eMoltype_dna
    case CMolInfo::eBiomol_pre_RNA:
    case CMolInfo::eBiomol_cRNA:
    case CMolInfo::eBiomol_transcribed_RNA:
        return "RNA";  // eMoltype_rna
    case CMolInfo::eBiomol_mRNA:
        return "mRNA";  // eMoltype_mrna
    case CMolInfo::eBiomol_rRNA:
        return "rRNA";  // eMoltype_rrna
    case CMolInfo::eBiomol_tRNA:
        return "tRNA";  // eMoltype_trna
    case CMolInfo::eBiomol_snRNA:
        return "uRNA";  // eMoltype_urna
    case CMolInfo::eBiomol_scRNA:
        return "snRNA";  // eMoltype_snrna
    case CMolInfo::eBiomol_peptide:
        return "AA";  // eMoltype_peptide
    case CMolInfo::eBiomol_snoRNA:
        return "snoRNA";  // eMoltype_snorna
    default:
        break;
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

    CNcbiOstrstream tmp;

    tmp << s_CombineStrings("    ", "GBSeq_locus", locus.GetName());

    tmp << s_CombineStrings("    ", "GBSeq_length", locus.GetLength());

   CGBSeq::TStrandedness sStrandedness =
        s_GBSeqStrandedness(locus.GetStrand(), locus.GetBiomol());
    if( ! sStrandedness.empty() ) {
        tmp << s_CombineStrings("    ", "GBSeq_strandedness", sStrandedness);
    }

    CGBSeq::TMoltype sMolType = s_GBSeqMoltype(locus.GetBiomol());
    if( ! sMolType.empty() ) {
        tmp << s_CombineStrings("    ", "GBSeq_moltype", sMolType);
    } else if (ctx.IsProt()) {
        tmp << s_CombineStrings("    ", "GBSeq_moltype", "AA");
    }
 
    tmp << s_CombineStrings("    ", "GBSeq_topology", s_GBSeqTopology(locus.GetTopology()));

    tmp << s_CombineStrings("    ", "GBSeq_division", locus.GetDivision());

    tmp << s_CombineStrings("    ", "GBSeq_update-date", s_GetDate(ctx.GetHandle(), CSeqdesc::e_Update_date));

    tmp << s_CombineStrings("    ", "GBSeq_create-date", s_GetDate(ctx.GetHandle(), CSeqdesc::e_Create_date));

    string str = (string)CNcbiOstrstreamToString(tmp);

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
    CNcbiOstrstream tmp;

    string def = defline.GetDefline();
    if ( NStr::EndsWith(def, '.') ) {
        def.resize(def.length() - 1);
    }

    tmp << s_CombineStrings("    ", "GBSeq_definition", def);

    string str = (string)CNcbiOstrstreamToString(tmp);

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

    CNcbiOstrstream tmp;

    tmp << s_CombineStrings("    ", "GBSeq_primary-accession", acc.GetAccession());

    string str = (string)CNcbiOstrstreamToString(tmp);

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, acc.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();

    bool hasOthers = false;
    CNcbiOstrstream others;
    ITERATE (CBioseq::TId, it, ctx.GetBioseqIds()) {
        others << s_CombineStrings("      ", "GBSeqid", CGBSeqid((*it)->AsFastaString()));
        hasOthers = true;
    }
    if (hasOthers) {
        m_OtherSeqIDs = (string)CNcbiOstrstreamToString(others);
    }

    bool hasExtras = false;
    CNcbiOstrstream extras;
    ITERATE (CAccessionItem::TExtra_accessions, it, acc.GetExtraAccessions()) {
        extras << s_CombineStrings("      ", "GBSecondary-accn", CGBSecondary_accn(*it));
        hasExtras = true;
    }
    if (hasExtras) {
        m_SecondaryAccns = (string)CNcbiOstrstreamToString(extras);
    }

}


///////////////////////////////////////////////////////////////////////////
//
// Version

void CGBSeqFormatter::FormatVersion
(const CVersionItem& version,
 IFlatTextOStream& text_os)
{
    CNcbiOstrstream tmp;

    tmp << s_CombineStrings("    ", "GBSeq_accession-version", version.GetAccession());

    if (! m_OtherSeqIDs.empty()) {
        tmp << s_OpenTag("    ", "GBSeq_other-seqids");
        tmp << m_OtherSeqIDs;
        tmp << s_CloseTag("    ", "GBSeq_other-seqids");
    }

    if (! m_SecondaryAccns.empty()) {
        tmp << s_OpenTag("    ", "GBSeq_secondary-accessions");
        tmp << m_SecondaryAccns;
        tmp << s_CloseTag("    ", "GBSeq_secondary-accessions");
    }

    string str = (string)CNcbiOstrstreamToString(tmp);

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
    CNcbiOstrstream tmp;

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
                tmp << s_CombineStrings("    ", "GBSeq_project", id);
            }
        }
    }

    string str = (string)CNcbiOstrstreamToString(tmp);

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
    CNcbiOstrstream tmp;

    tmp << "    <GBSeq_segment>" << seg.GetNum() << " of " << seg.GetCount() << "</GBSeq_segment>" << "\n";

    string str = (string)CNcbiOstrstreamToString(tmp);

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
    CNcbiOstrstream tmp;

    CNcbiOstrstream source_line;
    source_line << source.GetOrganelle() << source.GetTaxname();
    if ( !source.GetCommon().empty() ) {
        source_line << (source.IsUsingAnamorph() ? " (anamorph: " : " (") 
                    << source.GetCommon() << ")";
    }
    tmp << s_CombineStrings("    ", "GBSeq_source", CNcbiOstrstreamToString(source_line));

    tmp << s_CombineStrings("    ", "GBSeq_organism", source.GetTaxname());

    const string & sTaxonomy = source.GetLineage();
    string staxon = sTaxonomy;
    if( NStr::EndsWith(staxon, ".") ) {
        staxon.resize( staxon.length() - 1);
    }
    tmp << s_CombineStrings("    ", "GBSeq_taxonomy", staxon);

    string str = (string)CNcbiOstrstreamToString(tmp);

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, source.GetObject(), IFlatTextOStream::eAddNewline_No);

    text_os.Flush();
}


///////////////////////////////////////////////////////////////////////////
//
// Keywords

void CGBSeqFormatter::FormatKeywords
(const CKeywordsItem& keys,
 IFlatTextOStream& text_os)
{
    CNcbiOstrstream tmp;

    ITERATE (CKeywordsItem::TKeywords, it, keys.GetKeywords()) {
        if (! m_DidKeysStart) {
            tmp << s_OpenTag("    ", "GBSeq_keywords");
            m_DidKeysStart = true;
            m_NeedKeysEnd = true;
        }
        tmp << s_CombineStrings("      ", "GBKeyword", CGBKeyword(*it));
    }
    if (m_NeedKeysEnd) {
        tmp << s_CloseTag("    ", "GBSeq_keywords");
    }

    string str = (string)CNcbiOstrstreamToString(tmp);

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
    CNcbiOstrstream tmp;

    if (! m_DidRefsStart) {
        tmp << s_OpenTag("    ", "GBSeq_references");
        m_DidRefsStart = true;
        m_NeedRefsEnd = true;
    }

    tmp << s_OpenTag("      ", "GBReference");

    CBioseqContext& ctx = *ref.GetContext();

    tmp << s_CombineStrings("        ", "GBReference_reference", ref.GetSerial());

    CNcbiOstrstream refstr;
    const CSeq_loc* loc = &ref.GetLoc();
    const char* pchDelim = "";
    for ( CSeq_loc_CI it(*loc);  it;  ++it ) {
        CSeq_loc_CI::TRange range = it.GetRange();
        if ( range.IsWhole() ) {
            range.SetTo(sequence::GetLength(it.GetSeq_id(), &ctx.GetScope()) - 1);
        }
        refstr << pchDelim << range.GetFrom() + 1 << ".." << range.GetTo() + 1;
        pchDelim = "; ";
    }
    tmp << s_CombineStrings("        ", "GBReference_position", CNcbiOstrstreamToString(refstr));

    list<string> authors;
    if (ref.IsSetAuthors()) {
        CReferenceItem::GetAuthNames(ref.GetAuthors(), authors);
        bool hasAuthors = false;
        ITERATE (list<string>, it, authors) {
            if (! hasAuthors) {
                tmp << s_OpenTag("        ", "GBReference_authors");
                hasAuthors = true;
            }
            tmp << s_CombineStrings("          ", "GBAuthor", *it);
        }
        if (hasAuthors) {
            tmp << s_CloseTag("        ", "GBReference_authors");
        }
    }
    if ( !ref.GetConsortium().empty() ) {
        tmp << s_CombineStrings("        ", "GBReference_consortium", ref.GetConsortium());
    }
    if ( !ref.GetTitle().empty() ) {
        if ( NStr::EndsWith(ref.GetTitle(), '.') ) {
            string title = ref.GetTitle();
            title.resize(title.length() - 1);
            tmp << s_CombineStrings("        ", "GBReference_title", title);
        } else {
            tmp << s_CombineStrings("        ", "GBReference_title", ref.GetTitle());
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
        tmp << s_CombineStrings("        ", "GBReference_journal", journal);
    }
    string doi = ref.GetDOI();
    if ( ! doi.empty() ) {
        tmp << s_OpenTag("        ", "GBReference_xref");
        tmp << s_OpenTag("          ", "GBXref");
        tmp << s_CombineStrings("            ", "GBXref_dbname", "doi");
        tmp << s_CombineStrings("            ", "GBXref_id", doi);
        tmp << s_CloseTag("          ", "GBXref");
        tmp << s_CloseTag("        ", "GBReference_xref");
    }
    if ( ref.GetPMID() != 0 ) {
        tmp << s_CombineStrings("        ", "GBReference_pubmed", ref.GetPMID());
    }
    if ( !ref.GetRemark().empty() ) {
        tmp << s_CombineStrings("        ", "GBReference_remark", ref.GetRemark());
    }

    tmp << s_CloseTag("      ", "GBReference");

    string str = (string)CNcbiOstrstreamToString(tmp);

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
    CNcbiOstrstream tmp;

    if (m_NeedRefsEnd) {
        tmp << s_CloseTag("    ", "GBSeq_references");
        m_NeedRefsEnd = false;
    }

    if (m_NeedComment) {
        m_NeedComment = false;

        string comm = NStr::Join( m_Comments, "; " );
        tmp << s_CombineStrings("    ", "GBSeq_comment", comm);
    }

    if (m_NeedDbsource) {
        m_NeedDbsource = false;

        string dbsrc = NStr::Join( m_Dbsource, "; " );
        tmp << s_CombineStrings("    ", "GBSeq_source-db", dbsrc);
    }

    if (! m_DidFeatStart) {
        tmp << s_OpenTag("    ", "GBSeq_feature-table");
        m_DidFeatStart = true;
        m_NeedFeatEnd = true;
    }

    tmp << s_OpenTag("      ", "GBFeature");

    CConstRef<CFlatFeature> feat = f.Format();

    tmp << s_CombineStrings("        ", "GBFeature_key", feat->GetKey());

    string location = feat->GetLoc().GetString();
    s_GBSeqStringCleanup(location, true);
    tmp << s_CombineStrings("        ", "GBFeature_location", location);

    tmp << s_OpenTag("        ", "GBFeature_intervals");

    const CSeq_loc& loc = f.GetLoc();
    CScope& scope = f.GetContext()->GetScope();
    for (CSeq_loc_CI it(loc); it; ++it) {
        tmp << s_OpenTag("          ", "GBInterval");

        CSeq_loc_CI::TRange range = it.GetRange();
        if ( range.GetLength() == 1 ) {  // point
            tmp << s_CombineStrings("            ", "GBInterval_point", range.GetFrom() + 1);
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
            tmp << s_CombineStrings("            ", "GBInterval_from", from);
            tmp << s_CombineStrings("            ", "GBInterval_to", to);
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
        tmp << s_CombineStrings("            ", "GBInterval_accession", best->GetSeqIdString(true));

        tmp << s_CloseTag("          ", "GBInterval");
    }

    tmp << s_CloseTag("        ", "GBFeature_intervals");

    if ( NStr::Find(location, "join") != NPOS ) {
        tmp << s_CombineStrings("        ", "GBFeature_operator", "join");
    } else if ( NStr::Find(location, "order") != NPOS ) {
        tmp << s_CombineStrings("        ", "GBFeature_operator", "order");
    }

    if ( loc.IsPartialStart(eExtreme_Biological) ) {
        tmp << "        <GBFeature_partial5 value=\"true\"/>" << "\n";
    }
    if ( loc.IsPartialStop(eExtreme_Biological) ) {
        tmp << "        <GBFeature_partial3 value=\"true\"/>" << "\n";
    }

    if ( !feat->GetQuals().empty() ) {
        tmp << s_OpenTag("        ", "GBFeature_quals");

        const CFlatFeature::TQuals& quals = feat->GetQuals();
        ITERATE (CFlatFeature::TQuals, it, quals) {
            tmp << s_OpenTag("          ", "GBQualifier");
            tmp << s_CombineStrings("            ", "GBQualifier_name", (*it)->GetName());
            if ((*it)->GetStyle() != CFormatQual::eEmpty) {
               tmp << s_CombineStrings("            ", "GBQualifier_value", (*it)->GetValue());
            }
            tmp << s_CloseTag("          ", "GBQualifier");
        }

        tmp << s_CloseTag("        ", "GBFeature_quals");
    }

    tmp << s_CloseTag("      ", "GBFeature");

    string str = (string)CNcbiOstrstreamToString(tmp);

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
    CNcbiOstrstream tmp;

    if (m_NeedRefsEnd) {
        tmp << s_CloseTag("    ", "GBSeq_references");
        m_NeedRefsEnd = false;
    }

    if (m_NeedComment) {
        m_NeedComment = false;

        string comm = NStr::Join( m_Comments, "; " );
        tmp << s_CombineStrings("    ", "GBSeq_comment", comm);
    }

    if (m_NeedFeatEnd) {
        tmp << s_CloseTag("    ", "GBSeq_feature-table");
        m_NeedFeatEnd = false;
    }

    string data;

    CSeqVector_CI vec_ci(seq.GetSequence(), 0, 
        CSeqVector_CI::eCaseConversion_lower);
    vec_ci.GetSeqData(data, seq.GetSequence().size() );

    tmp << s_CombineStrings("    ", "GBSeq_sequence", data);

    string str = (string)CNcbiOstrstreamToString(tmp);

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
    CNcbiOstrstream tmp;

    if (m_NeedRefsEnd) {
        tmp << s_CloseTag("    ", "GBSeq_references");
        m_NeedRefsEnd = false;
    }

    if (m_NeedComment) {
        m_NeedComment = false;

        string comm = NStr::Join( m_Comments, "; " );
        tmp << s_CombineStrings("    ", "GBSeq_comment", comm);
    }

    if (m_NeedFeatEnd) {
        tmp << s_CloseTag("    ", "GBSeq_feature-table");
        m_NeedFeatEnd = false;
    }

    string assembly = CFlatSeqLoc(contig.GetLoc(), *contig.GetContext(), 
        CFlatSeqLoc::eType_assembly).GetString();
    s_GBSeqStringCleanup(assembly, true);

    tmp << s_CombineStrings("    ", "GBSeq_contig", assembly);

    string str = (string)CNcbiOstrstreamToString(tmp);

    if ( m_IsInsd ) {
        NStr::ReplaceInPlace(str, "<GB", "<INSD");
        NStr::ReplaceInPlace(str,"</GB", "</INSD");
    }

    text_os.AddLine(str, contig.GetObject(), IFlatTextOStream::eAddNewline_No);

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
