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

#include <objects/flat/flat_gbseq_formatter.hpp>
#include <objects/flat/flat_items.hpp>

#include <serial/classinfo.hpp>

#include <objects/gbseq/gbseq__.hpp>

#include <objects/objmgr/seq_vector.hpp>

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


void CFlatGBSeqFormatter::BeginSequence(SFlatContext& context)
{
    IFlatFormatter::BeginSequence(context);
    m_Seq.Reset(new CGBSeq);
}


void CFlatGBSeqFormatter::FormatHead(const SFlatHead& head)
{
    m_Seq->SetLocus (head.m_Locus);
    m_Seq->SetLength(m_Context->m_Length);

    switch (head.m_Strandedness) {
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
        switch (m_Context->m_Biomol) {
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
            switch (m_Context->m_Mol) {
            case CSeq_inst::eMol_dna:  mt = CGBSeq::eMoltype_dna;      break;
            case CSeq_inst::eMol_rna:  mt = CGBSeq::eMoltype_rna;      break;
            case CSeq_inst::eMol_aa:   mt = CGBSeq::eMoltype_peptide;  break;
            default:                   break; // already nucleic-acid
            }
        }
        m_Seq->SetMoltype(mt);
    }}

    if (head.m_Topology == CSeq_inst::eTopology_circular) {
        m_Seq->SetTopology(CGBSeq::eTopology_circular);
        // otherwise, stays at linear (default)
    }

    m_Seq->SetDivision(head.m_Division);
    FormatDate(*head.m_UpdateDate, m_Seq->SetUpdate_date());
    FormatDate(*head.m_CreateDate, m_Seq->SetCreate_date());
    m_Seq->SetDefinition(head.m_Definition);
    if (m_Context->m_PrimaryID->GetTextseq_Id()) {
        m_Seq->SetPrimary_accession
            (m_Context->m_PrimaryID->GetSeqIdString(false));
        m_Seq->SetAccession_version(m_Context->m_Accession);
    }

    {{
        // why "other", then?
        CRef<CGBSeqid> id
            (new CGBSeqid(m_Context->m_PrimaryID->AsFastaString()));
        m_Seq->SetOther_seqids().push_back(id);
    }}
    ITERATE (CBioseq::TId, it, head.m_OtherIDs) {
        CRef<CGBSeqid> id(new CGBSeqid((*it)->AsFastaString()));
        m_Seq->SetOther_seqids().push_back(id);
    }
    ITERATE (list<string>, it, head.m_SecondaryIDs) {
        CRef<CGBSecondary_accn> accn(new CGBSecondary_accn(*it));
        m_Seq->SetSecondary_accessions().push_back(accn);
    }

    if ( !head.m_DBSource.empty() ) {
        m_Seq->SetSource_db(NStr::Join(head.m_DBSource, " "));
    }
}


void CFlatGBSeqFormatter::FormatKeywords(const SFlatKeywords& keys)
{
    ITERATE (list<string>, it, keys.m_Keywords) {
        CRef<CGBKeyword> key(new CGBKeyword(*it));
        m_Seq->SetKeywords().push_back(key);
    }
}


void CFlatGBSeqFormatter::FormatSegment(const SFlatSegment& segment)
{
    m_Seq->SetSegment(NStr::IntToString(segment.m_Num) + " of "
                      + NStr::IntToString(segment.m_Count));
}


void CFlatGBSeqFormatter::FormatSource(const SFlatSource& source)
{
    {{
        string name = source.m_FormalName;
        if ( !source.m_CommonName.empty() ) {
            name += " (" + source.m_CommonName + ")";
        }
        m_Seq->SetSource(name);
    }}
    m_Seq->SetOrganism(source.m_FormalName);
    m_Seq->SetTaxonomy(source.m_Lineage);
}


void CFlatGBSeqFormatter::FormatReference(const SFlatReference& ref)
{
    CRef<CGBReference> gbref(new CGBReference);
    gbref->SetReference(NStr::IntToString(ref.m_Serial)
                        + ref.GetRange(*m_Context));
    ITERATE (list<string>, it, ref.m_Authors) {
        CRef<CGBAuthor> author(new CGBAuthor(*it));
        gbref->SetAuthors().push_back(author);
    }
    if ( !ref.m_Consortium.empty() ) {
        gbref->SetConsortium(ref.m_Consortium);
    }
    ref.GetTitles(gbref->SetTitle(), gbref->SetJournal(), *m_Context);
    if ( gbref->GetTitle().empty() ) {
        gbref->ResetTitle();
    }
    if ( !ref.m_MUIDs.empty() ) {
        gbref->SetMedline(*ref.m_MUIDs.begin());
    }
    if ( !ref.m_PMIDs.empty() ) {
        gbref->SetPubmed(*ref.m_PMIDs.begin());
    }
    if ( !ref.m_Remark.empty() ) {
        gbref->SetRemark(ref.m_Remark);
    }
    m_Seq->SetReferences().push_back(gbref);
}


void CFlatGBSeqFormatter::FormatComment(const SFlatComment& comment)
{
    if ( !comment.m_Comment.empty() ) {
        m_Seq->SetComment(comment.m_Comment);
    }
}


void CFlatGBSeqFormatter::FormatPrimary(const SFlatPrimary& primary)
{
    m_Seq->SetPrimary(primary.GetHeader());
    ITERATE (SFlatPrimary::TPieces, it, primary.m_Pieces) {
        m_Seq->SetPrimary() += '~';
        it->Format(m_Seq->SetPrimary());
    }
}


void CFlatGBSeqFormatter::FormatFeature(const SFlatFeature& feat)
{
    CRef<CGBFeature> gbfeat(new CGBFeature);
    gbfeat->SetKey(feat.m_Key);
    gbfeat->SetLocation(feat.m_Loc->m_String);
    ITERATE (vector<SFlatLoc::SInterval>, it, feat.m_Loc->m_Intervals) {
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
    ITERATE (vector<CRef<SFlatQual> >, it, feat.m_Quals) {
        CRef<CGBQualifier> qual(new CGBQualifier);
        qual->SetName((*it)->m_Name);
        if ((*it)->m_Style != SFlatQual::eEmpty) {
            qual->SetValue((*it)->m_Value);
        }
        gbfeat->SetQuals().push_back(qual);
    }
    m_Seq->SetFeature_table().push_back(gbfeat);
}


void CFlatGBSeqFormatter::FormatData(const SFlatData& data)
{
    CSeqVector v = m_Context->m_Handle.GetSequenceView
        (*data.m_Loc, CBioseq_Handle::eViewConstructed,
         CBioseq_Handle::eCoding_Iupac);
    v.GetSeqData(0, v.size(), m_Seq->SetSequence());
}


void CFlatGBSeqFormatter::FormatContig(const SFlatContig& contig)
{
    m_Seq->SetContig(SFlatLoc(*contig.m_Loc, *m_Context).m_String);
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
* Revision 1.2  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
