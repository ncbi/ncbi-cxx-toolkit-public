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
*   new (early 2003) flat-file generator -- GBSeq output
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/flat/flat_gbseq_formatter.hpp>
#include <objtools/flat/flat_items.hpp>

#include <serial/classinfo.hpp>

#include <objects/gbseq/gbseq__.hpp>

#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CFlatGBSeqFormatter::CFlatGBSeqFormatter(CObjectOStream& out, CScope& scope,
                                         IFlatFormatter::EMode mode,
                                         IFlatFormatter::EStyle style,
                                         IFlatFormatter::TFlags flags)
    : IFlatFormatter(scope, mode, style, flags), m_Set(new CGBSet), m_Out(&out)
{
    const CClassTypeInfo* gbset_info
        = dynamic_cast<const CClassTypeInfo*>(CGBSet::GetTypeInfo());
    const CMemberInfo* member = gbset_info->GetMemberInfo(kEmptyStr);
    out.WriteFileHeader(gbset_info);
    // We have to manage the stack ourselves, since BeginXxx() doesn't. :-/
    out.PushFrame(CObjectStackFrame::eFrameClass, gbset_info);
    out.BeginClass(gbset_info);
    // out.PushFrame(CObjectStackFrame::eFrameClassMember, member->GetId());
    // out.BeginClassMember(member->GetId());
    out.PushFrame(CObjectStackFrame::eFrameArray, member->GetTypeInfo());
    const CContainerTypeInfo* cont_info
        = dynamic_cast<const CContainerTypeInfo*>(member->GetTypeInfo());
    out.BeginContainer(cont_info);
    out.PushFrame(CObjectStackFrame::eFrameArrayElement,
                  cont_info->GetElementType());
    out.Flush();
}


CFlatGBSeqFormatter::~CFlatGBSeqFormatter()
{
    if (m_Out) {
        // We have to manage the stack ourselves, since EndXxx() doesn't. :-/
        m_Out->PopFrame();
        m_Out->EndContainer();
        m_Out->PopFrame();
        // m_Out->EndClassMember();
        // m_Out->PopFrame();
        m_Out->EndClass();
        m_Out->PopFrame();
        m_Out->EndOfWrite();
    }
}


void CFlatGBSeqFormatter::BeginSequence(CFlatContext& context)
{
    IFlatFormatter::BeginSequence(context);
    m_Seq.Reset(new CGBSeq);
}


void CFlatGBSeqFormatter::FormatHead(const CFlatHead& head)
{
    m_Seq->SetLocus (head.GetLocus());
    m_Seq->SetLength(m_Context->GetLength());

    switch (head.GetStrandedness()) {
    case CSeq_inst::eStrand_ss:
        m_Seq->SetStrandedness(CGBSeq::eStrandedness_single_stranded);
        break;
    case CSeq_inst::eStrand_ds:
        m_Seq->SetStrandedness(CGBSeq::eStrandedness_double_stranded);
        break;
    case CSeq_inst::eStrand_mixed:
        m_Seq->SetStrandedness(CGBSeq::eStrandedness_mixed_stranded);
        break;
    default:
        break;
    }

    {{
        CGBSeq::TMoltype mt = CGBSeq::eMoltype_nucleic_acid;
        switch (m_Context->GetBiomol()) {
        case CMolInfo::eBiomol_genomic:
        case CMolInfo::eBiomol_other_genetic:
        case CMolInfo::eBiomol_genomic_mRNA:
            mt = CGBSeq::eMoltype_dna;
            break;

        case CMolInfo::eBiomol_pre_RNA:
        case CMolInfo::eBiomol_cRNA:
        case CMolInfo::eBiomol_transcribed_RNA:
            mt = CGBSeq::eMoltype_rna;
            break;

        case CMolInfo::eBiomol_mRNA:     mt = CGBSeq::eMoltype_mrna;     break;
        case CMolInfo::eBiomol_rRNA:     mt = CGBSeq::eMoltype_rrna;     break;
        case CMolInfo::eBiomol_tRNA:     mt = CGBSeq::eMoltype_trna;     break;
        case CMolInfo::eBiomol_snRNA:    mt = CGBSeq::eMoltype_urna;     break;
        case CMolInfo::eBiomol_scRNA:    mt = CGBSeq::eMoltype_snrna;    break;
        case CMolInfo::eBiomol_peptide:  mt = CGBSeq::eMoltype_peptide;  break;
        case CMolInfo::eBiomol_snoRNA:   mt = CGBSeq::eMoltype_snorna;   break;

        default:
            switch (m_Context->GetMol()) {
            case CSeq_inst::eMol_dna:  mt = CGBSeq::eMoltype_dna;      break;
            case CSeq_inst::eMol_rna:  mt = CGBSeq::eMoltype_rna;      break;
            case CSeq_inst::eMol_aa:   mt = CGBSeq::eMoltype_peptide;  break;
            default:                   break; // already nucleic-acid
            }
        }
        m_Seq->SetMoltype(mt);
    }}

    if (head.GetTopology() == CSeq_inst::eTopology_circular) {
        m_Seq->SetTopology(CGBSeq::eTopology_circular);
        // otherwise, stays at linear (default)
    }

    m_Seq->SetDivision(head.GetDivision());
    FormatDate(head.GetUpdateDate(), m_Seq->SetUpdate_date());
    FormatDate(head.GetCreateDate(), m_Seq->SetCreate_date());
    m_Seq->SetDefinition(head.GetDefinition());
    if (m_Context->GetPrimaryID().GetTextseq_Id()) {
        m_Seq->SetPrimary_accession
            (m_Context->GetPrimaryID().GetSeqIdString(false));
        m_Seq->SetAccession_version(m_Context->GetAccession());
    }

    {{
        // why "other", then?
        CGBSeqid id(m_Context->GetPrimaryID().AsFastaString());
        m_Seq->SetOther_seqids().push_back(id);
    }}
    ITERATE (CBioseq::TId, it, head.GetOtherIDs()) {
        CGBSeqid id((*it)->AsFastaString());
        m_Seq->SetOther_seqids().push_back(id);
    }
    ITERATE (list<string>, it, head.GetSecondaryIDs()) {
        CGBSecondary_accn accn(*it);
        m_Seq->SetSecondary_accessions().push_back(accn);
    }

    if ( !head.GetDBSource().empty() ) {
        m_Seq->SetSource_db(NStr::Join(head.GetDBSource(), " "));
    }
}


void CFlatGBSeqFormatter::FormatKeywords(const CFlatKeywords& keys)
{
    ITERATE (list<string>, it, keys.GetKeywords()) {
        CGBKeyword key(*it);
        m_Seq->SetKeywords().push_back(key);
    }
}


void CFlatGBSeqFormatter::FormatSegment(const CFlatSegment& segment)
{
    m_Seq->SetSegment(NStr::IntToString(segment.GetNum()) + " of "
                      + NStr::IntToString(segment.GetCount()));
}


void CFlatGBSeqFormatter::FormatSource(const CFlatSource& source)
{
    {{
        string name = source.GetFormalName();
        if ( !source.GetCommonName().empty() ) {
            name += " (" + source.GetCommonName() + ")";
        }
        m_Seq->SetSource(name);
    }}
    m_Seq->SetOrganism(source.GetFormalName());
    m_Seq->SetTaxonomy(source.GetLineage());
}


void CFlatGBSeqFormatter::FormatReference(const CFlatReference& ref)
{
    CRef<CGBReference> gbref(new CGBReference);
    gbref->SetReference(NStr::IntToString(ref.GetSerial())
                        + ref.GetRange(*m_Context));
    ITERATE (list<string>, it, ref.GetAuthors()) {
        CGBAuthor author(*it);
        gbref->SetAuthors().push_back(author);
    }
    if ( !ref.GetConsortium().empty() ) {
        gbref->SetConsortium(ref.GetConsortium());
    }
    ref.GetTitles(gbref->SetTitle(), gbref->SetJournal(), *m_Context);
    if ( gbref->GetTitle().empty() ) {
        gbref->ResetTitle();
    }
    if ( !ref.GetMUIDs().empty() ) {
        gbref->SetMedline(*ref.GetMUIDs().begin());
    }
    if ( !ref.GetPMIDs().empty() ) {
        gbref->SetPubmed(*ref.GetPMIDs().begin());
    }
    if ( !ref.GetRemark().empty() ) {
        gbref->SetRemark(ref.GetRemark());
    }
    m_Seq->SetReferences().push_back(gbref);
}


void CFlatGBSeqFormatter::FormatComment(const CFlatComment& comment)
{
    if ( !comment.GetComment().empty() ) {
        m_Seq->SetComment(comment.GetComment());
    }
}


void CFlatGBSeqFormatter::FormatPrimary(const CFlatPrimary& primary)
{
    m_Seq->SetPrimary(primary.GetHeader());
    ITERATE (CFlatPrimary::TPieces, it, primary.GetPieces()) {
        m_Seq->SetPrimary() += '~';
        it->Format(m_Seq->SetPrimary());
    }
}


void CFlatGBSeqFormatter::FormatFeature(const IFlattishFeature& f)
{
    const CFlatFeature& feat = *f.Format();
    CRef<CGBFeature>    gbfeat(new CGBFeature);
    gbfeat->SetKey(feat.GetKey());
    gbfeat->SetLocation(feat.GetLoc().GetString());
    ITERATE (vector<CFlatLoc::SInterval>, it, feat.GetLoc().GetIntervals()) {
        CRef<CGBInterval> ival(new CGBInterval);
        if (it->m_Range.GetLength() == 1) {
            ival->SetPoint(it->m_Range.GetFrom());
        } else {
            ival->SetFrom(it->m_Range.GetFrom());
            ival->SetTo  (it->m_Range.GetTo());
        }
        ival->SetAccession(it->m_Accession);
        gbfeat->SetIntervals().push_back(ival);
    }
    ITERATE (vector<CRef<CFlatQual> >, it, feat.GetQuals()) {
        CRef<CGBQualifier> qual(new CGBQualifier);
        qual->SetName((*it)->GetName());
        if ((*it)->GetStyle() != CFlatQual::eEmpty) {
            NStr::Replace((*it)->GetValue(), " \b", kEmptyStr,
                          qual->SetValue());
        }
        gbfeat->SetQuals().push_back(qual);
    }
    m_Seq->SetFeature_table().push_back(gbfeat);
}


void CFlatGBSeqFormatter::FormatData(const CFlatData& data)
{
    CSeqVector v = m_Context->GetHandle().GetSequenceView
        (data.GetLoc(), CBioseq_Handle::eViewConstructed,
         CBioseq_Handle::eCoding_Iupac);
    v.GetSeqData(0, v.size(), m_Seq->SetSequence());
}


void CFlatGBSeqFormatter::FormatContig(const CFlatContig& contig)
{
    m_Seq->SetContig(CFlatLoc(contig.GetLoc(), *m_Context).GetString());
}


void CFlatGBSeqFormatter::EndSequence(void)
{
    m_Set->Set().push_back(m_Seq);
    if (m_Out) {
        m_Out->WriteContainerElement(ObjectInfo(*m_Seq));
        m_Out->Flush();
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2004/05/21 21:42:53  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.7  2003/11/04 20:00:28  ucko
* Edit " \b" sequences (used as hints for wrapping) out from qualifier values
*
* Revision 1.6  2003/10/21 13:48:50  grichenk
* Redesigned type aliases in serialization library.
* Fixed the code (removed CRef-s, added explicit
* initializers etc.)
*
* Revision 1.5  2003/06/02 16:06:42  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.4  2003/04/10 20:08:22  ucko
* Arrange to pass the item as an argument to IFlatTextOStream::AddParagraph
*
* Revision 1.3  2003/03/21 18:49:17  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.2  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
