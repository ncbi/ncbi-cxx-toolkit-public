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

#include <objects/gbseq/GBSet.hpp>
#include <objects/gbseq/GBSeq.hpp>
#include <objects/gbseq/GBReference.hpp>
#include <objects/gbseq/GBKeyword.hpp>
#include <objects/gbseq/GBSeqid.hpp>
#include <objects/gbseq/GBFeature.hpp>
#include <objects/gbseq/GBInterval.hpp>
#include <objects/gbseq/GBQualifier.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/scope_info.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/impl/synonyms.hpp>

#include <objtools/format/text_ostream.hpp>
#include <objtools/format/gbseq_formatter.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/version_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/items/contig_item.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// static functions

static void s_GBSeqStringCleanup(string& str, bool location = false)
{
    list<string> l;
    NStr::Split(str, " \n\r\t\b", l);
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
CGBSeqFormatter::CGBSeqFormatter(void)
{
}

// detructor
CGBSeqFormatter::~CGBSeqFormatter(void) 
{
}


void CGBSeqFormatter::Start(IFlatTextOStream& text_os)
{
    x_WriteFileHeader(text_os);
        
    x_StartWriteGBSet(text_os);
}


void CGBSeqFormatter::StartSection(const CStartSectionItem&, IFlatTextOStream&)
{
    m_GBSeq.Reset(new CGBSeq);
    _ASSERT(m_GBSeq);
}


void CGBSeqFormatter::EndSection(const CEndSectionItem&, IFlatTextOStream& text_os)
{
    x_WriteGBSeq(text_os);

    m_GBSeq.Reset();
    _ASSERT(!m_GBSeq);
}


void CGBSeqFormatter::End(IFlatTextOStream& text_os)
{
    x_EndWriteGBSet(text_os);
}


///////////////////////////////////////////////////////////////////////////
//
// Locus
//


CGBSeq::TStrandedness s_GBSeqStrandedness(CSeq_inst::TStrand strand)
{
    switch ( strand ) {
    case CSeq_inst::eStrand_ss:
        return CGBSeq::eStrandedness_single_stranded;
    case CSeq_inst::eStrand_ds:
        return CGBSeq::eStrandedness_double_stranded;
    case CSeq_inst::eStrand_mixed:
        return CGBSeq::eStrandedness_mixed_stranded;
    case CSeq_inst::eStrand_other:
    case CSeq_inst::eStrand_not_set:
    default:
        break;
    }

    return CGBSeq::eStrandedness_not_set;
}


CGBSeq::TMoltype s_GBSeqMoltype(CMolInfo::TBiomol biomol)
{
    switch ( biomol ) {
    case CMolInfo::eBiomol_unknown:
        return CGBSeq::eMoltype_nucleic_acid;
    case CMolInfo::eBiomol_genomic:
    case CMolInfo::eBiomol_other_genetic:
    case CMolInfo::eBiomol_genomic_mRNA:
        return CGBSeq::eMoltype_dna;
    case CMolInfo::eBiomol_pre_RNA:
    case CMolInfo::eBiomol_cRNA:
    case CMolInfo::eBiomol_transcribed_RNA:
        return CGBSeq::eMoltype_rna;
    case CMolInfo::eBiomol_mRNA:
        return CGBSeq::eMoltype_mrna;
    case CMolInfo::eBiomol_rRNA:
        return CGBSeq::eMoltype_rrna;
    case CMolInfo::eBiomol_tRNA:
        return CGBSeq::eMoltype_trna;
    case CMolInfo::eBiomol_snRNA:
        return CGBSeq::eMoltype_urna;
    case CMolInfo::eBiomol_scRNA:
        return CGBSeq::eMoltype_snrna;
    case CMolInfo::eBiomol_peptide:
        return CGBSeq::eMoltype_peptide;
    case CMolInfo::eBiomol_snoRNA:
        return CGBSeq::eMoltype_snorna;
    default:
        break;
    }
    return CGBSeq::eMoltype_nucleic_acid;
}


CGBSeq::TTopology s_GBSeqTopology(CSeq_inst::TTopology topology)
{
    if ( topology == CSeq_inst::eTopology_circular ) {
        return CGBSeq::eTopology_circular;
    }
    return CGBSeq::eTopology_linear;
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
 IFlatTextOStream&)
{
    _ASSERT(m_GBSeq);
    CBioseqContext& ctx = *locus.GetContext();

    m_GBSeq->SetLocus(locus.GetName());
    m_GBSeq->SetLength(locus.GetLength());
    m_GBSeq->SetStrandedness(s_GBSeqStrandedness(locus.GetStrand()));
    m_GBSeq->SetMoltype(s_GBSeqMoltype(locus.GetBiomol()));
    m_GBSeq->SetTopology(s_GBSeqTopology(locus.GetTopology()));
    m_GBSeq->SetDivision(locus.GetDivision());
    m_GBSeq->SetUpdate_date(s_GetDate(ctx.GetHandle(), CSeqdesc::e_Update_date));
    m_GBSeq->SetCreate_date(s_GetDate(ctx.GetHandle(), CSeqdesc::e_Create_date));
    ITERATE (CBioseq::TId, it, ctx.GetBioseqIds()) {
        m_GBSeq->SetOther_seqids().push_back(CGBSeqid((*it)->AsFastaString()));
    }
}


///////////////////////////////////////////////////////////////////////////
//
// Definition

void CGBSeqFormatter::FormatDefline
(const CDeflineItem& defline,
 IFlatTextOStream&)
{
    _ASSERT(m_GBSeq);
    m_GBSeq->SetDefinition(defline.GetDefline());
    if ( NStr::EndsWith(m_GBSeq->GetDefinition(), '.') ) {
        m_GBSeq->SetDefinition().resize(m_GBSeq->GetDefinition().length() - 1);
    }
}


///////////////////////////////////////////////////////////////////////////
//
// Accession

void CGBSeqFormatter::FormatAccession
(const CAccessionItem& acc, 
 IFlatTextOStream&)
{
    m_GBSeq->SetPrimary_accession(acc.GetAccession());
    ITERATE (CAccessionItem::TExtra_accessions, it, acc.GetExtraAccessions()) {
        m_GBSeq->SetSecondary_accessions().push_back(CGBSecondary_accn(*it));
    }
}


///////////////////////////////////////////////////////////////////////////
//
// Version

void CGBSeqFormatter::FormatVersion
(const CVersionItem& version,
 IFlatTextOStream&)
{
    m_GBSeq->SetAccession_version(version.GetAccession());
}


///////////////////////////////////////////////////////////////////////////
//
// Segment

void CGBSeqFormatter::FormatSegment
(const CSegmentItem& seg,
 IFlatTextOStream&)
{
    CNcbiOstrstream segment_line;

    segment_line << seg.GetNum() << " of " << seg.GetCount();

    m_GBSeq->SetSegment(CNcbiOstrstreamToString(segment_line));
}


///////////////////////////////////////////////////////////////////////////
//
// Source

void CGBSeqFormatter::FormatSource
(const CSourceItem& source,
 IFlatTextOStream&)
{
    _ASSERT(m_GBSeq);
    CNcbiOstrstream source_line;
    source_line << source.GetOrganelle() << source.GetTaxname();
    if ( !source.GetCommon().empty() ) {
        source_line << (source.IsUsingAnamorph() ? " (anamorph: " : " (") 
                    << source.GetCommon() << ")";
    }

    m_GBSeq->SetSource(CNcbiOstrstreamToString(source_line));
    m_GBSeq->SetOrganism(source.GetTaxname());
    m_GBSeq->SetTaxonomy(source.GetLineage());
}


///////////////////////////////////////////////////////////////////////////
//
// Keywords

void CGBSeqFormatter::FormatKeywords
(const CKeywordsItem& keys,
 IFlatTextOStream&)
{
    ITERATE (CKeywordsItem::TKeywords, it, keys.GetKeywords()) {
        m_GBSeq->SetKeywords().push_back(CGBKeyword(*it));
    }
}


///////////////////////////////////////////////////////////////////////////
//
// REFERENCE

void CGBSeqFormatter::FormatReference
(const CReferenceItem& ref,
 IFlatTextOStream&)
{
    _ASSERT(m_GBSeq);
    CBioseqContext& ctx = *ref.GetContext();

    CRef<CGBReference> gbref(new CGBReference);
    const CSeq_loc* loc = &ref.GetLoc();
    CNcbiOstrstream refstr;
    refstr << ref.GetSerial() << ' ';
    x_FormatRefLocation(refstr, *loc, " to ", "; ", ctx);
    gbref->SetReference(CNcbiOstrstreamToString(refstr));
    list<string> authors;
    if (ref.IsSetAuthors()) {
        CReferenceItem::GetAuthNames(ref.GetAuthors(), authors);
    }
    ITERATE (list<string>, it, authors) {
        CGBAuthor author(*it);
        gbref->SetAuthors().push_back(author);
    }
    if ( !ref.GetConsortium().empty() ) {
        gbref->SetConsortium(ref.GetConsortium());
    }
    if ( !ref.GetTitle().empty() ) {
        if ( NStr::EndsWith(ref.GetTitle(), '.') ) {
            string title = ref.GetTitle();
            title.resize(title.length() - 1);
            gbref->SetTitle(title);
        } else {
            gbref->SetTitle(ref.GetTitle());
        }
    }
    string journal;
    x_FormatRefJournal(ref, journal, ctx);
    NON_CONST_ITERATE (string, it, journal) {
        if ( (*it == '\n')  ||  (*it == '\t')  ||  (*it == '\r') ) {
            *it = ' ';
        }
    }
    if ( !journal.empty() ) {
        gbref->SetJournal(journal);
    }
    if ( ref.GetMUID() != 0 ) {
        gbref->SetMedline(ref.GetMUID());
    }
    if ( ref.GetPMID() != 0 ) {
        gbref->SetPubmed(ref.GetPMID());
    }
    if ( !ref.GetRemark().empty() ) {
        gbref->SetRemark(ref.GetRemark());
    }
    m_GBSeq->SetReferences().push_back(gbref);
}

///////////////////////////////////////////////////////////////////////////
//
// COMMENT


void CGBSeqFormatter::FormatComment
(const CCommentItem& comment,
 IFlatTextOStream&)
{
    string str = comment.GetComment();
    s_GBSeqStringCleanup(str);
    
    if ( !m_GBSeq->IsSetComment() ) {
        m_GBSeq->SetComment(str);
    } else {    
        m_GBSeq->SetComment() += "; ";
        m_GBSeq->SetComment() += str;
    }
}


///////////////////////////////////////////////////////////////////////////
//
// FEATURES

static void s_SetIntervals(CGBFeature::TIntervals& intervals,
                    const CSeq_loc& loc,
                    CScope& scope)
{
    for (CSeq_loc_CI it(loc); it; ++it) {
        CRef<CGBInterval> ival(new CGBInterval);
        CSeq_loc_CI::TRange range = it.GetRange();
        CConstRef<CSeq_id> best(&it.GetSeq_id());
        if ( best->IsGi() ) {
            CConstRef<CSynonymsSet> syns = scope.GetSynonyms(*best);
            vector< CRef<CSeq_id> > ids;
            ITERATE (CSynonymsSet, id_iter, *syns) {
                CConstRef<CSeq_id> id = (*id_iter)->first.GetSeqId();
                CRef<CSeq_id> sip(const_cast<CSeq_id*>(id.GetPointerOrNull()));
                ids.push_back(sip);
            }
            best.Reset(FindBestChoice(ids, CSeq_id::Score));
        }
        ival->SetAccession(best->GetSeqIdString(true));  
        if ( range.GetLength() == 1 ) {  // point
            ival->SetPoint(range.GetFrom() + 1);
        } else {
            TSeqPos from, to;
            if ( range.IsWhole() ) {
                from = 1;
                to = sequence::GetLength(it.GetSeq_loc(), &scope);
            } else {
                from = range.GetFrom() + 1;
                to = range.GetTo() + 1;
            }
            if ( it.GetStrand() == eNa_strand_minus ) {
                swap(from, to);
            }
            ival->SetFrom(from);
            ival->SetTo(to);
        }
        
        intervals.push_back(ival);
    }
}


static void s_SetQuals(CGBFeature::TQuals& gbquals,
                       const CFlatFeature::TQuals& quals)
{
    ITERATE (CFlatFeature::TQuals, it, quals) {
        CRef<CGBQualifier> qual(new CGBQualifier);
        qual->SetName((*it)->GetName());
        if ((*it)->GetStyle() != CFormatQual::eEmpty) {
            qual->SetValue((*it)->GetValue());
            s_GBSeqQualCleanup(qual->SetValue());
        }
        gbquals.push_back(qual);
    }
}


void CGBSeqFormatter::FormatFeature
(const CFeatureItemBase& f,
 IFlatTextOStream&)
{
    CConstRef<CFlatFeature> feat = f.Format();

    CRef<CGBFeature>    gbfeat(new CGBFeature);
    gbfeat->SetKey(feat->GetKey());
    
    string location = feat->GetLoc().GetString();
    s_GBSeqStringCleanup(location, true);
    gbfeat->SetLocation(location);
    if ( feat->GetKey() != "source" ) {
        s_SetIntervals(gbfeat->SetIntervals(), f.GetLoc(), 
            f.GetContext()->GetScope());
    }
    if ( !feat->GetQuals().empty() ) {
        s_SetQuals(gbfeat->SetQuals(), feat->GetQuals());
    }
    
    m_GBSeq->SetFeature_table().push_back(gbfeat);
}


///////////////////////////////////////////////////////////////////////////
//
// SEQUENCE

void CGBSeqFormatter::FormatSequence
(const CSequenceItem& seq,
 IFlatTextOStream&)
{
    string data;

    CSeqVector_CI vec_ci(seq.GetSequence());
    vec_ci.GetSeqData(data, seq.GetSequence().size());

    if ( !m_GBSeq->IsSetSequence() ) {
        m_GBSeq->SetSequence(kEmptyStr);
    }
    m_GBSeq->SetSequence() += data;
}


///////////////////////////////////////////////////////////////////////////
//
// CONTIG

void CGBSeqFormatter::FormatContig
(const CContigItem& contig,
 IFlatTextOStream&)
{
    string assembly = CFlatSeqLoc(contig.GetLoc(), *contig.GetContext(), 
        CFlatSeqLoc::eType_assembly).GetString();
    s_GBSeqStringCleanup(assembly, true);
    m_GBSeq->SetContig(assembly);
}


//=========================================================================//
//                                Private                                  //
//=========================================================================//


void CGBSeqFormatter::x_WriteFileHeader(IFlatTextOStream& text_os)
{
    m_Out.reset(CObjectOStream::Open(eSerial_Xml, m_StrStream));
    const CClassTypeInfo* gbset_info
        = dynamic_cast<const CClassTypeInfo*>(CGBSet::GetTypeInfo());
    m_Out->WriteFileHeader(gbset_info);
    x_StrOStreamToTextOStream(text_os);
}


void CGBSeqFormatter::x_StartWriteGBSet(IFlatTextOStream& text_os)
{
    m_Cont.reset(new SOStreamContainer(*m_Out, CGBSet::GetTypeInfo()));
    x_StrOStreamToTextOStream(text_os);
}


void CGBSeqFormatter::x_WriteGBSeq(IFlatTextOStream& text_os)
{
    m_Cont->WriteElement(ConstObjectInfo(*m_GBSeq));
    x_StrOStreamToTextOStream(text_os);
}


void CGBSeqFormatter::x_EndWriteGBSet(IFlatTextOStream& text_os)
{
    m_Cont.reset();
    x_StrOStreamToTextOStream(text_os);
}


void CGBSeqFormatter::x_StrOStreamToTextOStream(IFlatTextOStream& text_os)
{
    list<string> l;

    // flush ObjectOutputStream to underlying strstream
    m_Out->Flush();
    // read text from strstream
    NStr::Split(CNcbiOstrstreamToString(m_StrStream), "\n", l);
    // add text to TextOStream
    text_os.AddParagraph(l);
    // reset strstream
    m_StrStream.seekp(0);
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.9  2005/02/09 14:56:41  shomrat
* x_FormatRefJournal take a context instead of config as parameter
*
* Revision 1.8  2005/01/12 16:46:09  shomrat
* Changes in reference formatting
*
* Revision 1.7  2004/10/05 18:06:28  shomrat
* in place TruncateSpaces ->TruncateSpacesInPlace
*
* Revision 1.6  2004/10/05 16:04:22  shomrat
* Use non-const TruncateSpaces
*
* Revision 1.5  2004/10/05 15:44:23  shomrat
* Use more efficient NStr::EndsWith function
*
* Revision 1.4  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.3  2004/05/06 17:52:53  shomrat
* CFlatQual -> CFormatQual
*
* Revision 1.2  2004/04/22 15:53:26  shomrat
* Changes in context
*
* Revision 1.1  2004/04/13 16:49:54  shomrat
* Initial revision
*
*
* ===========================================================================
*/

