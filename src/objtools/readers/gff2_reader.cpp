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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   GFF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <util/line_reader.hpp>

#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Feat_id.hpp>

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>

#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/gff2_reader.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ----------------------------------------------------------------------------
CGff2Reader::CGff2Reader(
    int iFlags,
    const string& name,
    const string& title,
    SeqIdResolver resolver,
    CReaderListener* pRL):
//  ----------------------------------------------------------------------------
    CReaderBase(iFlags, name, title, resolver, pRL),
    m_pErrors(0),
    mCurrentFeatureCount(0),
    mParsingAlignment(false),
    mSequenceSize(0),
    mAtSequenceData(false)
{
}

//  ----------------------------------------------------------------------------
CGff2Reader::~CGff2Reader()
//  ----------------------------------------------------------------------------
{
}

//  --------------------------------------------------------------------------- 
void
CGff2Reader::ReadSeqAnnots(
    TAnnots& annots,
    CNcbiIstream& istr,
    ILineErrorListener* pMessageListener )
//  ---------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    ReadSeqAnnots( annots, lr, pMessageListener );
}
 
//  ---------------------------------------------------------------------------                       
void
CGff2Reader::ReadSeqAnnots(
    TAnnots& annots,
    ILineReader& lr,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    xProgressInit(lr);
    while (!lr.AtEOF()  &&  !mAtSequenceData) {
        CRef<CSeq_annot> pNext = this->ReadSeqAnnot(lr, pEC);
        if (pNext) {
            annots.push_back(pNext);
        }
    }
    return;
}

//  ----------------------------------------------------------------------------                
CRef< CSeq_entry >
CGff2Reader::ReadSeqEntry(
    ILineReader& lr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------                
{ 
    xProgressInit(lr);

    TAnnots annots;
    ReadSeqAnnots( annots, lr, pMessageListener );
    
    CRef<CSeq_entry> pSeqEntry(new CSeq_entry());
    pSeqEntry->SetSet();

    for (TAnnots::iterator it = annots.begin();
            it != annots.end(); ++it) {
        CRef<CBioseq> pSeq( new CBioseq() );
        pSeq->SetAnnot().push_back(*it);
        pSeq->SetId().push_back( CRef<CSeq_id>( 
            new CSeq_id(CSeq_id::e_Local, "gff-import") ) );
        pSeq->SetInst().SetRepr(CSeq_inst::eRepr_not_set);
        pSeq->SetInst().SetMol(CSeq_inst::eMol_not_set);

        CRef<CSeq_entry> pEntry(new CSeq_entry());
        pEntry->SetSeq(*pSeq);
        pSeqEntry->SetSet().SetSeq_set().push_back( pEntry );
    }
    return pSeqEntry;
}

//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CGff2Reader::ReadObject(
    ILineReader& lr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef<CSerialObject> object( 
        ReadSeqEntry( lr, pMessageListener ).ReleaseOrNull() );
    return object;
}
 
//  ----------------------------------------------------------------------------
void CGff2Reader::xPostProcessAnnot(
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    xAssignAnnotId(annot);
    if (!(m_iFlags & fGenbankMode)) {
        //xAssignTrackData(pAnnot);
        xAddConversionInfo(annot, nullptr);
        xGenerateParentChildXrefs(annot);
    }
}

//  ----------------------------------------------------------------------------
void
CGff2Reader::xGetData(
    ILineReader& lr,
    TReaderData& readerData)
//  ----------------------------------------------------------------------------
{
    readerData.clear();
    string line;
    if (!xGetLine(lr, line)) {
        return;
    }
    if (xNeedsNewSeqAnnot(line)) {
        return;
    }
    if (xIsTrackLine(line)) {
        if (!mCurrentFeatureCount) {
            xParseTrackLine(line);
            xGetData(lr, readerData);
            return;
        }
        m_PendingLine = line;
        return;
    }
    if (xIsTrackTerminator(line)) {
        if (!mCurrentFeatureCount) {
            xParseTrackLine("track");
            xGetData(lr, readerData);
        }
        return;
    }
    if (xIsSequenceRegion(line)) {
        vector<string> tokens;
        NStr::Split(line, " \n", tokens, NStr::fSplit_MergeDelimiters);
        if (tokens.size() < 4) {
            mSequenceSize = 0;
        }
        else {
            mSequenceSize = NStr::StringToNonNegativeInt(tokens[3]);
            m_iFlags |= fAssumeCircularSequence;
        }
        if (mSequenceSize == -1) {
            mSequenceSize = 0;
            CReaderMessage warning(
                ncbi::eDiag_Warning,
                m_uLineNumber,
                "Bad sequence-region pragma - ignored.");
            throw warning;
        }
        if (!mCurrentFeatureCount) {
            xParseTrackLine("track");
            xGetData(lr, readerData);
        }
        return;
    }
    if (xIsFastaMarker(line)) {
        mAtSequenceData = true;
        readerData.clear();
        return;
    }
    if (!xIsCurrentDataType(line)) {
        xUngetLine(lr);
        return;
    }
    readerData.push_back(TReaderLine{m_uLineNumber, line});
    ++m_uDataCount;
}

//  ----------------------------------------------------------------------------
void CGff2Reader::xAssignAnnotId(
    CSeq_annot& annot,
    const string& givenId)
//  ----------------------------------------------------------------------------
{
    if (givenId.empty() && annot.GetData().IsAlign()) {
        return;
    }

    string annotId(givenId);
    if (annotId.empty() && 
        (!(m_iFlags & fGenbankMode)) &&
        m_pTrackDefaults) {
        annotId = m_pTrackDefaults->Name();
    }
    if (annotId.empty()) {
        return;
    }    
    CRef< CAnnot_id > pAnnotId(new CAnnot_id);
    pAnnotId->SetLocal().SetStr(annotId);
    annot.SetId().push_back(pAnnotId);   
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xParseStructuredComment(
    const string& strLine)
//  ----------------------------------------------------------------------------
{
    if (NStr::StartsWith(strLine, "###")) {
        return false;
    }
    if (!NStr::StartsWith( strLine, "##")) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CGff2Reader::xParseFeature(
    const string& line,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    if (CGff2Reader::IsAlignmentData(line)) {
        return false;
    }

    //parse record:
    shared_ptr<CGff2Record> pRecord(x_CreateRecord());
    try {
        if (!pRecord->AssignFromGff(line)) {
			return false;
		}
    }
    catch(CObjReaderLineException& err) {
        ProcessError(err, pEC);
        return false;
    }

    //make sure we are interested:
    if (xIsIgnoredFeatureType(pRecord->Type())) {
        return true;
    }
    if (xIsIgnoredFeatureId(pRecord->Id())) {
        return true;
    }

    //append feature to annot:
    if (!xUpdateAnnotFeature(*pRecord, annot, pEC)) {
        return false;
    }

    ++mCurrentFeatureCount;
    mParsingAlignment = false;
    return true;
}


//  ----------------------------------------------------------------------------
void CGff2Reader::x_GetAlignmentScores(const CSeq_align& alignment, 
                                       TScoreValueMap& score_values) const
//  ----------------------------------------------------------------------------
{
    // Start with empty scores
    score_values.clear();

    if (!alignment.IsSetScore()) {
        return;
    }

    for (const CRef<CScore>& score : alignment.GetScore()) {

        if (!score->IsSetId() ||
            !score->GetId().IsStr() ||
            !score->IsSetValue()) {
            continue;
        }
        const string name = score->GetId().GetStr();
        const CScore::TValue& value = score->GetValue();
        score_values[name] = Ref(new CScore::TValue());
        score_values[name]->Assign(value);
    }
}


//  ----------------------------------------------------------------------------
bool s_CompareValues(const CScore::TValue& score_val1, 
                     const CScore::TValue& score_val2)
//  ----------------------------------------------------------------------------
{

    if (score_val1.IsInt() && 
        score_val2.IsInt() &&
        score_val1.GetInt() == score_val2.GetInt()) {
        return true;
    }

    if (score_val1.IsReal() &&
        score_val2.IsReal() &&
        score_val1.GetReal() == score_val2.GetReal()) {
        return true;
    }

    return false;
}

// Result is a set of matching scores
//  ----------------------------------------------------------------------------
void CGff2Reader::x_FindMatchingScores(const TScoreValueMap& scores_1,
                                       const TScoreValueMap& scores_2,
                                       set<string>& matching_scores) const
//  ----------------------------------------------------------------------------
{
    matching_scores.clear();

    for (const auto& score1 : scores_1) {
        const string& name = score1.first;
        const CScore::TValue& value = *(score1.second);

        const auto& it = scores_2.find(name);
        if (it != scores_2.end() &&
            s_CompareValues(value, *(it->second))) {
            matching_scores.insert(name);
        }
    }
}


//  ----------------------------------------------------------------------------
void CGff2Reader::x_ProcessAlignmentsGff(const list<string>& id_list,
                            const map<string, list<CRef<CSeq_align>>>& alignments,
                            CRef<CSeq_annot> pAnnot) 
//  ----------------------------------------------------------------------------
{
    if (pAnnot.IsNull()) {
        pAnnot = Ref(new CSeq_annot());
    }

    for (const string& id : id_list) {
        CRef<CSeq_align> pAlign = Ref(new CSeq_align());
        if (x_MergeAlignments(alignments.at(id), pAlign)) {
            // if available, add current browser information
            if ( m_CurrentBrowserInfo ) {
                pAnnot->SetDesc().Set().push_back( m_CurrentBrowserInfo );
            }

            pAnnot->SetNameDesc("alignments");

            if ( !m_AnnotTitle.empty() ) {
                pAnnot->SetTitleDesc(m_AnnotTitle);
            }
            // Add alignment
            pAnnot->SetData().SetAlign().push_back(pAlign);
        }
    }
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ParseAlignmentGff(
    const string& strLine, 
    list<string>& id_list, // Add id to alignment
    map<string, list<CRef<CSeq_align>>>& alignments)
//  ----------------------------------------------------------------------------
{
    unique_ptr<CGff2Record> pRecord(x_CreateRecord());

    if ( !pRecord->AssignFromGff(strLine) ) {
        return false;
    }
    
    string id;
    if ( !pRecord->GetAttribute("ID", id) ) {
        id = pRecord->Id();
    }

    if (alignments.find(id) == alignments.end()) {
       id_list.push_back(id);
    }

    CRef<CSeq_align> alignment;
    if (!x_CreateAlignment(*pRecord, alignment)) {
        return false;
    }

    alignments[id].push_back(alignment);

    ++mCurrentFeatureCount;
    mParsingAlignment = true;
    return true;
}



//  ----------------------------------------------------------------------------
void CGff2Reader::x_InitializeScoreSums(const TScoreValueMap score_values, 
        map<string, TSeqPos>& summed_scores) const
//  ----------------------------------------------------------------------------
{
    const list<string> score_names {"num_ident", "num_mismatch"};
    
    for (const string& score_name : score_names) {
        if (score_values.find(score_name) != score_values.end()) {
            summed_scores[score_name] = score_values.at(score_name)->GetInt();
        }    
    }
}


//  ----------------------------------------------------------------------------
void CGff2Reader::x_ProcessAlignmentScores(const CSeq_align& alignment, 
    map<string, TSeqPos>& summed_scores, 
    TScoreValueMap& common_scores) const
//  ----------------------------------------------------------------------------
{
    const list<string> summed_score_names {"num_ident", "num_mismatch"};

    TScoreValueMap new_scores;
    x_GetAlignmentScores(alignment, new_scores);

    for (const string& score_name : summed_score_names) {
        if (new_scores.find(score_name) == new_scores.end()) {
            summed_scores.erase(score_name);
        } else if (summed_scores.find(score_name) != summed_scores.end()) {
            summed_scores[score_name] += new_scores[score_name]->GetInt();
            new_scores.erase(score_name);
        }
    }

    set<string> matching_score_names;
    x_FindMatchingScores(common_scores, 
        new_scores, 
        matching_score_names);

    common_scores.clear(); 
    for (string score_name : matching_score_names) {
        common_scores[score_name] = Ref(new CScore::TValue());
        common_scores[score_name]->Assign(*new_scores[score_name]);
    }
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::x_MergeAlignments(
        const list<CRef<CSeq_align>>& alignment_list,
        CRef<CSeq_align>& processed)
//  ----------------------------------------------------------------------------
{
    if (alignment_list.empty()) {
        return false;
    }

    if (alignment_list.size() == 1) {
        processed = alignment_list.front();
        return true;
    }

    map<string, TSeqPos> summed_scores;
    const list<string> summed_score_names {"num_ident", "num_mismatch"};

    // Factor out identical scores
    list<CRef<CSeq_align>>::const_iterator align_it = alignment_list.begin();
    TScoreValueMap score_values;
    x_GetAlignmentScores(**align_it, score_values);

    x_InitializeScoreSums(score_values, 
        summed_scores);
    ++align_it;

    while (align_it != alignment_list.end() &&
           !score_values.empty()) {

        x_ProcessAlignmentScores(**align_it, summed_scores, score_values);
        ++align_it;
    }
    // At this point, the score_values map should contain the scores that 
    // do not change over the rows

    const auto first_alignment = alignment_list.front();
    if (first_alignment->IsSetSegs() && 
        first_alignment->GetSegs().IsSpliced()) {
        
        processed->SetType(CSeq_align::eType_global);

        if (first_alignment->IsSetDim()) {
            processed->SetDim(first_alignment->GetDim());
        }

        for (auto& kv : summed_scores) {
            auto score = Ref(new CScore());
            score->SetId().SetStr(kv.first);
            score->SetValue().SetInt(kv.second);
            processed->SetScore().push_back(score);
        }

        for (auto& kv : score_values) {
            auto score = Ref(new CScore());
            score->SetId().SetStr(kv.first);
            score->SetValue().Assign(*(kv.second));
            processed->SetScore().push_back(score);
        }

        CRef<CSpliced_seg> spliced = Ref(new CSpliced_seg());
        spliced->Assign(first_alignment->GetSegs().GetSpliced());
        processed->SetSegs().SetSpliced(*spliced);

        auto align_it = alignment_list.cbegin();
        ++align_it;

        while(align_it != alignment_list.end()) {
            const auto& spliced_seg = (*align_it)->GetSegs().GetSpliced();
            if (spliced_seg.IsSetExons()) {
                for (auto exon : spliced_seg.GetExons()) {
                    processed->SetSegs().SetSpliced().SetExons().push_back(exon);
                }
            }
            ++align_it;
        }
        return true;
    }


    processed->SetType(CSeq_align::eType_disc);

    for (auto& kv : summed_scores) {
        auto score = Ref(new CScore());
        score->SetId().SetStr(kv.first);
        score->SetValue().SetInt(kv.second);
        processed->SetScore().push_back(score);
    }

    for (auto& kv : score_values) {
        auto score = Ref(new CScore());
        score->SetId().SetStr(kv.first);
        score->SetValue().Assign(*(kv.second));
        processed->SetScore().push_back(score);
    }

    for (auto current : alignment_list) {
        auto new_align = Ref(new CSeq_align());
        new_align->Assign(*current);
        new_align->ResetScore();

        for (CRef<CScore> score : current->GetScore()) {
            const string& score_name = score->GetId().GetStr();
            if (score_values.find(score_name) == score_values.end()) {
                new_align->SetScore().push_back(score);
            }
        }
        processed->SetSegs().SetDisc().Set().push_back(new_align);
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool
CGff2Reader::xIsCurrentDataType(
    const string& line)
//  ----------------------------------------------------------------------------
{
    if (CGff2Reader::IsAlignmentData(line)) {
        return (mParsingAlignment  ||  !mCurrentFeatureCount);
    }
    return (!mParsingAlignment  ||  !mCurrentFeatureCount);
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::xUpdateAnnotFeature(
    const CGff2Record&,
    CSeq_annot&,
    ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    return false;
}


bool CGff2Reader::x_CreateAlignment(
        const CGff2Record& gff, 
        CRef<CSeq_align>& pAlign ) 
{
    pAlign = Ref(new CSeq_align());
    pAlign->SetType(CSeq_align::eType_partial);
    pAlign->SetDim(2);

    //score
    if (!xAlignmentSetScore(gff, pAlign)) {
        return false;
    }

    if (!xAlignmentSetSegment(gff, pAlign)) {
        return false;
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::x_UpdateAnnotAlignment(
    const CGff2Record& gff,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_align> pAlign( new CSeq_align );
    pAlign->SetType(CSeq_align::eType_partial);
    pAlign->SetDim(2);

    //score
    if (!xAlignmentSetScore(gff, pAlign)) {
        return false;
    }
    if (!xAlignmentSetSegment(gff, pAlign)) {
        return false;
    }
    annot.SetData().SetAlign().push_back( pAlign ) ;
    return true;
}



bool CGff2Reader::xUpdateSplicedAlignment(const CGff2Record& gff,
                                          CRef<CSeq_align> pAlign) const
{
    if (!pAlign->IsSetType()) {
        pAlign->SetType(CSeq_align::eType_partial);
    }
    // Need to set a whole bunch of things

    if (!xUpdateSplicedSegment(gff, pAlign->SetSegs().SetSpliced())) {
        return false;
    }

    return true;
}



bool CGff2Reader::xUpdateSplicedSegment(
        const CGff2Record& gff, 
        CSpliced_seg& segment) const
{
    if (segment.IsSetProduct_type()) {
        segment.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
    }
    

    CRef<CSpliced_exon> pExon = Ref(new CSpliced_exon());
    if (!xSetSplicedExon(gff, pExon)) {
        return false;
    }

    segment.SetExons().push_back(pExon);

    return true;
}



//  ----------------------------------------------------------------------------
bool CGff2Reader::xSetSplicedExon(
        const CGff2Record& gff, 
        CRef<CSpliced_exon> pExon) const
//  ----------------------------------------------------------------------------
{
    vector<string> targetParts;
    if (!xGetTargetParts(gff, targetParts)) {
        return false;
    }

    pExon->SetGenomic_start(static_cast<TSeqPos>(gff.SeqStart()-1));
    pExon->SetGenomic_end(static_cast<TSeqPos>(gff.SeqStop()-1));
    if (gff.IsSetStrand()) {
        pExon->SetGenomic_strand(gff.Strand());
    }

    const int product_start = NStr::StringToInt(targetParts[1])-1;
    const int product_end = NStr::StringToInt(targetParts[2])-1;

    // Check to see that product start and product end are
    // non-negative and that product_end >= product_start

    pExon->SetProduct_start().SetNucpos(product_start);
    pExon->SetProduct_end().SetNucpos(product_end);

    ENa_strand targetStrand = eNa_strand_plus;
    if (targetParts[3] == "-") {
        targetStrand = eNa_strand_minus;
    }
    pExon->SetProduct_strand(targetStrand);

    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xGetTargetParts(const CGff2Record& gff, vector<string>& targetParts) const
//  ----------------------------------------------------------------------------
{
    string targetInfo;
    if (!gff.GetAttribute("Target", targetInfo)) {
        return false;
    }

    NStr::Split(targetInfo, " ", targetParts);
    if (targetParts.size() != 4) {
        return false;
    }
   
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xGetStartsOnMinusStrand(TSeqPos offset, 
        const vector<string>& gapParts,
        const bool isTarget,
        vector<int>& starts) const
//  ----------------------------------------------------------------------------
{
    starts.clear();
    const size_t gapCount = gapParts.size();

    for (size_t i=0; i<gapCount; ++i) {
        char changeType = gapParts[i][0];
        int changeSize = NStr::StringToInt(gapParts[i].substr(1));
        switch (changeType) {
        default:
            return false;

        case 'M': 
            starts.push_back(offset+1-changeSize);
            offset -= changeSize;
            break;

        case 'I':
            if (isTarget) {
                starts.push_back(offset+1-changeSize);
                offset -= changeSize;
            } else {
                starts.push_back(-1);
            }
            break;

        case 'D':
            if (isTarget) {
                starts.push_back(-1);
            } else {
                starts.push_back(offset+1-changeSize);
                offset -= changeSize;
            }
            break;
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xGetStartsOnPlusStrand(TSeqPos offset, 
        const vector<string>& gapParts,
        const bool isTarget,
        vector<int>& starts) const
//  ----------------------------------------------------------------------------
{
    starts.clear();
    const auto gapCount = gapParts.size();

    for (auto i=0; i<gapCount; ++i) {
        char changeType = gapParts[i][0];
        int changeSize = NStr::StringToInt(gapParts[i].substr(1));
        switch (changeType) {
        default:
            return false;

        case 'M': 
            starts.push_back(offset);
            offset += changeSize;
            break;

        case 'I':
            if (isTarget) {
                starts.push_back(offset);
                offset += changeSize;
            } else {
                starts.push_back(-1);
            }
            break;

        case 'D':
            if (isTarget) {
                starts.push_back(-1);
            } else {
                starts.push_back(offset);
                offset += changeSize;
            }
            break;
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xSetDensegStarts(const vector<string>& gapParts, 
                                   const ENa_strand identStrand,
                                   const ENa_strand targetStrand,
                                   const TSeqPos targetStart,
                                   const TSeqPos targetEnd,
                                   const CGff2Record& gff,
                                   CSeq_align::C_Segs::TDenseg& denseg)
//  ----------------------------------------------------------------------------
{
    const size_t gapCount = gapParts.size();

    const bool isTarget = true;
    vector<int> targetStarts;
    if (targetStrand == eNa_strand_minus) {
        if( !xGetStartsOnMinusStrand(targetEnd,
            gapParts,
            isTarget,
            targetStarts)) {
            return false;
        }
    }
    else {
        if (!xGetStartsOnPlusStrand(targetStart, 
            gapParts,
            isTarget,
            targetStarts)) {
            return false;
        }
    }

    vector<int> identStarts;
    const bool isIdent = !isTarget;

    if (identStrand == eNa_strand_minus) {

        if ( !xGetStartsOnMinusStrand(
            static_cast<TSeqPos>(gff.SeqStop()),
            gapParts,
            isIdent,
            identStarts)) {
            return false;
        }
    }
    else {
        if ( !xGetStartsOnPlusStrand(
            static_cast<TSeqPos>(gff.SeqStart()),
            gapParts,
            isIdent,
            identStarts)) {
            return false;
        }
    }

    for (auto i=0; i<gapCount; ++i) {
        denseg.SetStarts().push_back(targetStarts[i]);
        denseg.SetStarts().push_back(identStarts[i]);
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xAlignmentSetSegment(
    const CGff2Record& gff,
    CRef<CSeq_align> pAlign)
//  ----------------------------------------------------------------------------
{
    const string& type = gff.Type();

    if (type == "cDNA_match" || 
        type == "EST_match"  || 
        type == "translated_nucleotide_match") {
        return xAlignmentSetSpliced_seg(gff, pAlign);
    }

    return xAlignmentSetDenseg(gff, pAlign);
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xAlignmentSetSpliced_seg(
    const CGff2Record& gff,
    CRef<CSeq_align> pAlign)
//  ----------------------------------------------------------------------------
{
    vector<string> targetParts;
    if (!xGetTargetParts(gff, targetParts)) {
        return false;
    }

    CSeq_align::TSegs& segs = pAlign->SetSegs();
    
    auto& spliced_seg = segs.SetSpliced();

    const string& type = gff.Type();
    if (type == "translated_nucleotide_match") {
        spliced_seg.SetProduct_type(CSpliced_seg::eProduct_type_protein);
    } 
    else {
        spliced_seg.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
    }
    CRef<CSeq_id> product_id = mSeqIdResolve(targetParts[0], 0, true);
    spliced_seg.SetProduct_id(*product_id);

    CRef<CSeq_id> genomic_id = mSeqIdResolve(gff.Id(), 0, true);
    spliced_seg.SetGenomic_id(*genomic_id);

    if (targetParts[3] == "+") {
        spliced_seg.SetProduct_strand(eNa_strand_plus);
    }
    else
    if (targetParts[3] == "-") {
        spliced_seg.SetProduct_strand(eNa_strand_minus);
    }

    if (gff.IsSetStrand()) {
        ENa_strand ident_strand = gff.Strand();
        spliced_seg.SetGenomic_strand(ident_strand);
    }

    CRef<CSpliced_exon> exon(new CSpliced_exon());
    exon->SetProduct_start().SetNucpos(NStr::StringToInt(targetParts[1])-1);
    exon->SetProduct_end().SetNucpos(NStr::StringToInt(targetParts[2])-1);

    exon->SetGenomic_start(static_cast<TSeqPos>(gff.SeqStart()));
    exon->SetGenomic_end(static_cast<TSeqPos>(gff.SeqStop()));

    string gapInfo;
    vector<string> gapParts;
    if (gff.GetAttribute("Gap", gapInfo)) {
        NStr::Split(gapInfo, " ", gapParts);
    }
    else {
        gapParts.push_back(string("M") + NStr::NumericToString(gff.SeqStop()-gff.SeqStart()+1));
    }

    const auto gapCount = gapParts.size();

    for (auto i=0; i<gapCount; ++i) {
        CRef<CSpliced_exon_chunk> chunk(new CSpliced_exon_chunk());
        char changeType = gapParts[i][0];
        int changeSize = NStr::StringToInt(gapParts[i].substr(1));
        switch (changeType) {
        default:
            return false;

        case 'M': 
            chunk->SetMatch(changeSize);
            break;

        case 'I':
            chunk->SetProduct_ins(changeSize);
            break;

        case 'D':
            chunk->SetGenomic_ins(changeSize);
            break;

        }
        exon->SetParts().push_back(chunk);
    }

    spliced_seg.SetExons().push_back(exon);

    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xAlignmentSetDenseg(
    const CGff2Record& gff,
    CRef<CSeq_align> pAlign)
//  ----------------------------------------------------------------------------
{
    vector<string> targetParts;
    if (!xGetTargetParts(gff, targetParts)) {
        return false;
    }

    //strands
    ENa_strand targetStrand = eNa_strand_plus;
    if (targetParts[3] == "-") {
        targetStrand = eNa_strand_minus;
    }
    ENa_strand identStrand = eNa_strand_plus;
    if (gff.IsSetStrand()) {
        identStrand = gff.Strand();
    }


    string gapInfo;
    vector<string> gapParts;
    if (gff.GetAttribute("Gap", gapInfo)) {
        NStr::Split(gapInfo, " ", gapParts);
    }
    else {
        gapParts.push_back(string("M") + NStr::NumericToString(gff.SeqStop()-gff.SeqStart()+1));
    }

    int gapCount = static_cast<int>(gapParts.size());

    //meta
    CSeq_align::TSegs& segs = pAlign->SetSegs();
    CSeq_align::C_Segs::TDenseg& denseg = segs.SetDenseg();
    denseg.SetDim(2);
    denseg.SetNumseg(gapCount);

    //ids
    denseg.SetIds().push_back(
        mSeqIdResolve(targetParts[0], 0, true));
    denseg.SetIds().push_back(
        mSeqIdResolve(gff.Id(), 0, true));

    const TSeqPos targetStart = NStr::StringToInt(targetParts[1])-1;
    const TSeqPos targetEnd   = NStr::StringToInt(targetParts[2])-1;

    if (!xSetDensegStarts(gapParts, 
                          identStrand,
                          targetStrand,
                          targetStart,
                          targetEnd,
                          gff,
                          denseg)) {
        return false;
    }

    //lengths
    for (int i=0; i < gapCount; ++i) {
        denseg.SetLens().push_back(NStr::StringToInt(CTempString(gapParts[i],1,string::npos)));
    }

    for (int i=0; i < gapCount; ++i) {
        denseg.SetStrands().push_back(targetStrand);
        denseg.SetStrands().push_back(identStrand);
    }
    return true;
}




//  ----------------------------------------------------------------------------
bool CGff2Reader::xAlignmentSetScore(
    const CGff2Record& gff,
    CRef<CSeq_align> pAlign)
//  ----------------------------------------------------------------------------
{
    if (gff.IsSetScore()) {
        pAlign->SetNamedScore(CSeq_align::eScore_Score, 
            int(gff.Score()));
    }

    string extraScore;

    const string intScores[] = {
        //official
        "score",
        "align_length",
        "num_ident",
        "num_positives",
        "num_negatives",
        "num_mismatch",
        "num_gap",

        //picked up from real data files
        "common_component",
        "filter_score",
        "for_remapping",
        "merge_aligner",
        "rank",
        "reciprocity",
        "batch_id",
        "align_id",
    };

    const size_t intCount(sizeof(intScores)/sizeof(string));
    for (size_t i=0; i < intCount; ++i) {
        if (gff.GetAttribute(intScores[i], extraScore)) {
            pAlign->SetNamedScore(
                intScores[i], int(NStr::StringToDouble(extraScore)));
        }
    }

    const string realScores[] = {
        //official
        "bit_score",
        "e_value",
        "pct_identity_gap",
        "pct_identity_ungap",
        "pct_identity_gapopen_only",
        "pct_coverage",
        "sum_e",
        "comp_adjustment_method",
        "pct_coverage_hiqual",

        //picked up from real data files
        "inversion_merge_alignmer",
        "expansion",
    };

    const size_t realCount(sizeof(realScores)/sizeof(string));
    for (size_t i=0; i < realCount; ++i) {
        if (gff.GetAttribute(realScores[i], extraScore)) {
            pAlign->SetNamedScore(
                realScores[i], NStr::StringToDouble(extraScore));
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ProcessQualifierSpecialCase(
    CGff2Record::TAttrCit it,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    return false;
}  

//  ----------------------------------------------------------------------------
bool CGff2Reader::xFeatureSetQualifier(
    const string& key,
    const string& value,
    CRef<CSeq_feat> pTargetFeature)
//  ----------------------------------------------------------------------------
{
    if (!pTargetFeature) {
        return false;
    }
    pTargetFeature->AddOrReplaceQualifier(key, value);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_GetFeatureById(
    const string & strId, 
    ncbi::CRef<CSeq_feat>& pFeature )
//  ----------------------------------------------------------------------------
{
    map< string, CRef< CSeq_feat > >::iterator it;
    it = m_MapIdToFeature.find(strId);
	if(it != m_MapIdToFeature.end()) {
        pFeature = it->second;
		return true;
	}
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::xAddFeatureToAnnot(
    CRef< CSeq_feat > pFeature,
    CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    annot.SetData().SetFtable().push_back(pFeature);
    return true;
}

//  ============================================================================
CRef< CDbtag >
CGff2Reader::x_ParseDbtag(
    const string& str )
//  ============================================================================
{
    CRef< CDbtag > pDbtag( new CDbtag() );
    static const char* digits = "0123456789";
    string strDb, strTag;
    NStr::SplitInTwo( str, ":", strDb, strTag );

    // dbtag names for Gff2 do not always match the names for genbank.
    // special case known fixups here:
    if ( strDb == "NCBI_gi" ) {
        strDb = "GI";
    }
    // todo: all the other ones


    if ( ! strTag.empty() ) {
        pDbtag->SetDb( strDb );
        if (strTag.find_first_not_of(digits, 0) == string::npos)
            pDbtag->SetTag().SetId( NStr::StringToUInt( strTag ) );
        else
            pDbtag->SetTag().SetStr( strTag );

    }
    else {
        pDbtag->SetDb( "unknown" );
        pDbtag->SetTag().SetStr( str );
    }
    return pDbtag;
}

//  ============================================================================
bool CGff2Reader::xAnnotPostProcess(
    CSeq_annot& annot)
//  ============================================================================
{
    if (!xGenerateParentChildXrefs(annot)) {
        return false;
    }
    return true;
}

//  ============================================================================
bool CGff2Reader::xGenerateParentChildXrefs(
    CSeq_annot& annot)
//  ============================================================================
{
    typedef list<CRef<CSeq_feat> > FTABLE;
    typedef list<string> PARENTS;

    if (!annot.IsFtable()) {
        return true;
    }
    FTABLE& ftable = annot.SetData().SetFtable();
    for (auto featIt = ftable.begin(); featIt != ftable.end(); ++featIt) {
        CSeq_feat& feat = **featIt;
        const string& parentStr = feat.GetNamedQual("Parent");
        PARENTS parents;
        NStr::Split(parentStr, ",", parents, 0);
        for (auto parentIt = parents.begin(); parentIt != parents.end(); ++parentIt) {
            const string& parent = *parentIt; 
            xSetAncestryLine(feat, parent);
        }
    }
    return true;
}

//  ============================================================================
void CGff2Reader::xSetAncestryLine(
    CSeq_feat& feat,
    const string& directParentStr)
//  ============================================================================
{
    typedef list<string> PARENTS;

    string ancestorStr(directParentStr);
    CRef<CSeq_feat> pAncestor;
    while (!ancestorStr.empty()) {
        if (!x_GetFeatureById(ancestorStr, pAncestor)) {
            return;
        }
        xSetAncestorXrefs(feat, *pAncestor);
        ancestorStr = pAncestor->GetNamedQual("Parent");
        PARENTS ancestors;
        NStr::Split(ancestorStr, ",", ancestors, 0);
        for (PARENTS::iterator it = ancestors.begin(); it != ancestors.end(); ++it) {
            const string& ancestorStr = *it;
            xSetAncestryLine(feat, ancestorStr);
        }
    }
}

//  ============================================================================
bool sFeatureHasXref(
    const CSeq_feat& feat,
    const CFeat_id& featId)
//  ============================================================================
{
    typedef vector<CRef<CSeqFeatXref> > XREFS;
    if (!feat.IsSetXref()) {
        return false;
    }
    if (!featId.IsLocal()) {
        return false;
    }
    const auto& local = featId.GetLocal();
    if (local.IsId()) {
        auto xrefId = local.GetId();
        const XREFS& xrefs = feat.GetXref();
        for (XREFS::const_iterator cit = xrefs.begin(); cit != xrefs.end(); ++cit) {
            const CSeqFeatXref& ref = **cit; 
            if (!ref.GetId().IsLocal()  ||  !ref.GetId().GetLocal().IsId()) {
                continue;
            }
            auto contentId = ref.GetId().GetLocal().GetId();
            if (contentId == xrefId) {
                return true;
            }
        }
        return false;
    }
    if (local.IsStr()) {
        auto xrefId = local.GetStr();
        const XREFS& xrefs = feat.GetXref();
        for (XREFS::const_iterator cit = xrefs.begin(); cit != xrefs.end(); ++cit) {
            const CSeqFeatXref& ref = **cit; 
            if (!ref.GetId().IsLocal()  ||  !ref.GetId().GetLocal().IsStr()) {
                continue;
            }
            auto contentId = ref.GetId().GetLocal().GetStr();
            if (contentId == xrefId) {
                return true;
            }
        }
        return false;
    }
    return false;
}

//  ============================================================================
void CGff2Reader::xSetXrefFromTo(
    CSeq_feat& from,
    CSeq_feat& to)
//  ============================================================================
{
    if (!sFeatureHasXref(from, to.GetId())) {
        CRef<CFeat_id> pToId(new CFeat_id);
        pToId->Assign(to.GetId());
        CRef<CSeqFeatXref> pToXref(new CSeqFeatXref);
        pToXref->SetId(*pToId);
        from.SetXref().push_back(pToXref);
    }
}

//  ============================================================================
void CGff2Reader::xSetAncestorXrefs(
    CSeq_feat& descendent,
    CSeq_feat& ancestor)
//  ============================================================================
{
    xSetXrefFromTo(descendent, ancestor);
    xSetXrefFromTo(ancestor, descendent);
}

//  ============================================================================
bool CGff2Reader::IsAlignmentData(
    const string& line)
//  ============================================================================
{
    vector<CTempStringEx> columns;
    CGff2Record::TokenizeGFF(columns, line);
    if (columns.size() < 9) {
        return false;
    }
    if (NStr::StartsWith(columns[2], "match") || 
        NStr::EndsWith(columns[2], "_match")) {
        return true;
    }
    return false;
}

//  ============================================================================
bool CGff2Reader::xIsIgnoredFeatureType(
    const string& type)
//  ============================================================================
{
    return false;
}

//  ============================================================================
bool CGff2Reader::xIsIgnoredFeatureId(
    const string& type)
//  ============================================================================
{
    return false;
}

//  ---------------------------------------------------------------------------
bool
CGff2Reader::xNeedsNewSeqAnnot(
    const string& line)
//  ---------------------------------------------------------------------------
{
    if (IsInGenbankMode()) {
        vector<string> columns;
        NStr::Split(line, "\t ", columns, NStr::eMergeDelims);
        string seqId = columns[0];
        if (m_CurrentSeqId == seqId) {
            return false;
        }
        m_CurrentSeqId = seqId;
        if (mCurrentFeatureCount == 0) {
            return false;
        }
        m_PendingLine = line;
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::IsInGenbankMode() const
//  ----------------------------------------------------------------------------
{
    return (m_iFlags & CGff2Reader::fGenbankMode);
}

//  -------------------------------------------------------------------------------
bool CGff2Reader::xIsSequenceRegion(
    const string& line) 
//  -------------------------------------------------------------------------------
{
    string lineLowerCase(line);
    NStr::ToLower(lineLowerCase);
    return NStr::StartsWith(lineLowerCase, "##sequence-region");
}

//  -------------------------------------------------------------------------------
bool CGff2Reader::xIsFastaMarker(
    const string& line) 
//  -------------------------------------------------------------------------------
{
    string lineLowerCase(line);
    NStr::ToLower(lineLowerCase);
    return NStr::StartsWith(lineLowerCase, "##fasta");
}


END_objects_SCOPE
END_NCBI_SCOPE
