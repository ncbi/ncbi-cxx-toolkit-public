/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* Author: Ilya Dondoshansky
*
* ===========================================================================
*/

/// @file blastxml_format.cpp
/// Formatting of BLAST results in XML form, using the BLAST XML specification.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>

#include <algo/blast/format/blastxml_format.hpp>
#include <algo/blast/format/blastfmtutil.hpp>
#include <objtools/align_format/showdefline.hpp>
#include <objtools/align_format/align_format_util.hpp>

#include <serial/objostrxml.hpp>

#include <algo/blast/api/version.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(blast);
USING_SCOPE(align_format);

ncbi::TMaskedQueryRegions mask;

/// Auxiliary structure used for sorting CRange<int> objects in increasing
/// order of starting positions.
struct SRangeStartSort {
    bool operator()(CRange<int>* const& range1, CRange<int>* const& range2) 
    {
        return (range1->GetFrom() < range2->GetFrom());
    }
};

/// Wrapper class for a vector of CRange<ing> objects, needed to avoid
/// manual memory deallocation.
class CRangeVector : public vector<CRange<int>* >
{
public:
    /// Overrides the default destructor to deallocate all vector elements.
    ~CRangeVector()
    {
        for (iterator pItem = begin(); pItem != end(); ++pItem)
            delete *pItem;
    }
};

// helper function: serialize given object (could be partially initialized) 
// to string buffer and return it in two parts before and after given tag.
// object to serialize
// tag to devide by, often "</TAG_NAME>"
// start_part beginning of a serialized data before tag
// end_part   end of a serialized data starting from tag
// add_reference_dtd boolen flag, if true  - print a DOCTYPE DTD reference
// add_xml_versioni boolena flag, if true prin "xml version" open priabula
static bool s_SerializeAndSplitBy(const CSerialObject &object,
	const char *tag,
	string &start_part,
	string &end_part,
	bool add_reference_dtdi = false,
	bool add_xml_versioni = false );

/// Masks a query sequence string corresponding to an alignment, given a list
/// of mask locations.
/// @param alnvec One alignment [in]
/// @param query_seq Query string corresponding to this alignment [in] [out]
/// @param mask_info List of masking locations [in]
/// @param mask_char How should sequence be masked? [in]
/// @param query_frame If query is translated, what query frame is this 
///                    alignment for?
static void
s_MaskQuerySeq(CAlnVec& alnvec, string& query_seq, 
               const ncbi::TMaskedQueryRegions& mask_info, 
               CDisplaySeqalign::SeqLocCharOption mask_char,
               int query_frame)
{
    const int kNumSegs = alnvec.GetNumSegs();
    vector<CRange<int> > segs_v;
    for (int index = 0; index < kNumSegs; ++index) {
        CRange<int> range(alnvec.GetAlnStart(index), 
                          alnvec.GetAlnStop(index));
        segs_v.push_back(range);
    }

    CRangeVector masks_v;
    int aln_stop = query_seq.size() - 1;
    ITERATE(ncbi::TMaskedQueryRegions, mask_iter, mask_info) {
        if ((*mask_iter)->GetFrame() != query_frame)
            continue;
        int start = 
            alnvec.GetAlnPosFromSeqPos(0, 
                                       (*mask_iter)->GetInterval().GetFrom());
        int stop = 
            alnvec.GetAlnPosFromSeqPos(0, 
                                       (*mask_iter)->GetInterval().GetTo());
        // For negative frames, start and stop must be swapped.
        if (query_frame < 0) {
            int tmp = start;
            start = stop;
            stop = tmp;
        }
        if (start >= 0) {
            if (stop < 0)
                stop = aln_stop;
            CRange<int>*  range = new CRange<int>(start, stop);
            masks_v.push_back(range);
        }
    }

    sort(masks_v.begin(), masks_v.end(), SRangeStartSort());

    // Mask the sequence
    int mask_index = 0;
    for (int seg_index = 0; 
         seg_index < (int) segs_v.size() && mask_index < (int) masks_v.size(); 
         ++seg_index) {
        if (segs_v[seg_index].Empty())
            continue;
        int seg_start = segs_v[seg_index].GetFrom();
        int seg_stop = segs_v[seg_index].GetTo();
        int mask_pos;
        while (mask_index < (int) masks_v.size() &&
               (mask_pos = max(seg_start, masks_v[mask_index]->GetFrom()))
               <= seg_stop) {
            int mask_stop = min(seg_stop, masks_v[mask_index]->GetTo());
            // Mask the respective part of the sequence
            for ( ; mask_pos <= mask_stop; ++mask_pos) {
		if(  query_seq[mask_pos] == '-' ) continue; // preserve gap
                if (mask_char == CDisplaySeqalign::eX) {
                    query_seq[mask_pos] = 'X';
                } else if (mask_char == CDisplaySeqalign::eN){
                    query_seq[mask_pos]='N';
                } else if (mask_char == CDisplaySeqalign::eLowerCase) {
                    query_seq[mask_pos] =
                        tolower((unsigned char)query_seq[mask_pos]);
                } 
            }
            // Advance to the next mask if this mask is done with. Otherwise
            // break out of the loop.
            if (mask_pos < seg_stop)
                ++mask_index;
            else 
                break;
        }
    }
}

/// Returns translation frame given the strand, alignment endpoints and
/// total sequence length.
/// @param plus_strand Is this position on a forward strand? [in]
/// @param start Starting position, in 1-offset coordinates. [in]
/// @param end Ending position in 1-offset coordinates [in]
/// @param seq_length Total length of sequence [in]
/// @return Frame number.
static int 
s_GetTranslationFrame(bool plus_strand, int start, int end, int seq_length)
{
    int frame;

    if (plus_strand) {
        frame = (start - 1) % 3 + 1;
    } else {
        frame = -((seq_length - end) % 3 + 1);
    }
    
    return frame;
}

/// Creates a list of CHsp structures for the XML output, given a list of 
/// Seq-aligns.
/// @param xhsp_list List of CHsp's to populate [in] [out]
/// @param alnset Set of alignments to get data from [in]
/// @param scope Scope for retrieving sequences [in]
/// @param matrix 256x256 matrix for calculating positives for a protein search.
///               NULL is passed for a nucleotide search.
/// @param mask_info Masking locations [in]
static void
s_SeqAlignSetToXMLHsps(list<CRef<CHsp> >& xhsp_list, 
                       const CSeq_align_set& alnset, CScope* scope, 
                       const CBlastFormattingMatrix* matrix,
                       const ncbi::TMaskedQueryRegions* mask_info,
                       int master_gentice_code, int slave_genetic_code)
{
    int index = 1;
    ITERATE(CSeq_align_set::Tdata, iter, alnset.Get()) {
        CRef<CHsp> xhsp(new CHsp());
        const CSeq_align& kAlign = *(*iter);
        xhsp->SetNum(index);
        ++index;
        bool query_is_na, subject_is_na;
        int query_length, subject_length;

        int score, num_ident;
        double bit_score;
        double evalue;
        int sum_n;
        list<int> use_this_gi;
        CBlastFormatUtil::GetAlnScores(kAlign, score, bit_score, evalue, sum_n, 
                                       num_ident, use_this_gi);
        xhsp->SetBit_score(bit_score);
        xhsp->SetScore(score);
        xhsp->SetEvalue(evalue);

        // Extract the full list of subject ids
        try {
            const CBioseq_Handle& kQueryBioseqHandle = 
                scope->GetBioseqHandle(kAlign.GetSeq_id(0));
            query_is_na = kQueryBioseqHandle.IsNa();
            query_length = kQueryBioseqHandle.GetBioseqLength();
            const CBioseq_Handle& kSubjBioseqHandle = 
                scope->GetBioseqHandle(kAlign.GetSeq_id(1));
            subject_is_na = kSubjBioseqHandle.IsNa();
            subject_length = kSubjBioseqHandle.GetBioseqLength();
        } catch (const CException&) {
            // Either query or subject sequence not found - the remaining 
            // information cannot be correctly filled. Add this HSP as is
            // and continue.
            xhsp->SetQuery_from(0);
            xhsp->SetQuery_to(0);
            xhsp->SetHit_from(0);
            xhsp->SetHit_to(0);
            xhsp->SetIdentity(num_ident); // This may be inaccurate when 
                                          // alignment contains filtered regions.
            xhsp->SetQseq(NcbiEmptyString);
            xhsp->SetHseq(NcbiEmptyString);
            xhsp_list.push_back(xhsp);
            continue;
        }

        CRef<CSeq_align> final_aln(0);
   
        // Convert Std-seg and Dense-diag alignments to Dense-seg.
        // Std-segs are produced only for translated searches; Dense-diags only 
        // for ungapped, not translated searches.
        const bool kTranslated = kAlign.GetSegs().IsStd();
        
        if (kTranslated) {
            CRef<CSeq_align> densegAln = kAlign.CreateDensegFromStdseg();
            // When both query and subject are translated, i.e. tblastx, convert
            // to a special type of Dense-seg.
            if (query_is_na && subject_is_na)
                final_aln = densegAln->CreateTranslatedDensegFromNADenseg();
            else
                final_aln = densegAln;
        } else if (kAlign.GetSegs().IsDendiag()) {
            final_aln = CBlastFormatUtil::CreateDensegFromDendiag(kAlign);
        }
        
        const CDense_seg& kDenseg = (final_aln ? final_aln->GetSegs().GetDenseg() :
                                kAlign.GetSegs().GetDenseg());

        CRef<CAlnVec> aln_vec;

        // For non-transalted reverse strand alignments, show plus strand on 
        // query and minus strand on subject. To accomplish this, Dense-seg must
        // be reversed.
        if (!kTranslated && kDenseg.IsSetStrands() && 
            kDenseg.GetStrands().front() == eNa_strand_minus) {
            CRef<CDense_seg> reversed_ds(new CDense_seg);
            reversed_ds->Assign(kDenseg);
            reversed_ds->Reverse();
            aln_vec.Reset(new CAlnVec(*reversed_ds, *scope));   
        } else {
            aln_vec.Reset(new CAlnVec(kDenseg, *scope));
        }    

        //Note: do not switch the set order per calnvec specs.
        aln_vec->SetGenCode(slave_genetic_code);
        aln_vec->SetGenCode(master_gentice_code, 0);

        int align_length, num_gaps, num_gap_opens;
        CBlastFormatUtil::GetAlignLengths(*aln_vec, align_length, num_gaps, 
                                          num_gap_opens);
        
        int q_start, q_end, s_start, s_end, q_frame=0, s_frame=0;
        
        q_start = aln_vec->GetSeqStart(0) + 1;
        q_end = aln_vec->GetSeqStop(0) + 1;
        s_start = aln_vec->GetSeqStart(1) + 1;
        s_end = aln_vec->GetSeqStop(1) + 1;

        if (!kTranslated && query_is_na && subject_is_na) {
            q_frame = s_frame = 1;
            // For reverse strand alignment, set subject frame to -1 and
            // swap start and end coordinates.
            if (aln_vec->IsNegativeStrand(1)) {
                s_frame = -1;
                int tmp = s_start;
                s_start = s_end;
                s_end = tmp;
            }
        } else if (kTranslated) {
            if (query_is_na)
                q_frame = s_GetTranslationFrame(aln_vec->IsPositiveStrand(0), 
                                                q_start, q_end, query_length);
            if (subject_is_na)
                s_frame = s_GetTranslationFrame(aln_vec->IsPositiveStrand(1), 
                                                s_start, s_end, subject_length); 
        }

        xhsp->SetQuery_frame(q_frame);
        xhsp->SetHit_frame(s_frame);

        xhsp->SetQuery_from(q_start);
        xhsp->SetQuery_to(q_end);
        xhsp->SetHit_from(s_start);
        xhsp->SetHit_to(s_end);

        // Do not trust the identities count in the Seq-align, because if masking 
        // was used, then masked residues were not counted as identities. 
        // Hence retrieve the sequences present in the alignment and count the 
        // identities again.
        string query_seq;
        string subject_seq;
        string middle_seq;
        aln_vec->SetGapChar('-');
        aln_vec->GetWholeAlnSeqString(0, query_seq);

        // For blastn search, the matches are shown as '|', and mismatches as 
        // ' '; For all other searches matches are shown as matched characters,
        // mismatches as ' ', and positives as '+'.
        // This is a blastn search if and only if both query and subject are 
        // nucleotide, and it is not a translated search.
        const bool kIsBlastn = 
            (query_is_na && subject_is_na && !kTranslated);

        aln_vec->GetWholeAlnSeqString(1, subject_seq);

        num_ident = 0;
        int num_positives = 0;
        middle_seq = query_seq;
        // The query and subject sequence strings must be the same size in a 
        // correct alignment, but if alignment extends beyond the end of sequence
        // because of a bug, one of the sequence strings may be truncated, hence 
        // it is necessary to take a minimum here.
        // FIXME: Should an exception be thrown instead? 
        const unsigned int kMaxOffset = min(query_seq.size(),
                                            subject_seq.size());
        for (unsigned int i = 0; i < kMaxOffset; ++i) {
            if (query_seq[i] == subject_seq[i]) {
                ++num_ident;
                ++num_positives;
                if (kIsBlastn)
                    middle_seq[i] = '|';
            } else if (matrix &&
                       (*matrix)(query_seq[i], subject_seq[i]) > 0 &&
                       !kIsBlastn) {
                ++num_positives;
                middle_seq[i] = kIsBlastn ? ' ' : '+';
            } else {
                middle_seq[i] = ' ';
            }
        }
        
        xhsp->SetIdentity(num_ident);
        xhsp->SetGaps(num_gaps);
        xhsp->SetAlign_len(align_length);

        // Only now, after identities and positives have been computed, it's OK
        // to mask the filtered locations on the query sequence.
        if (mask_info) {
            const CDisplaySeqalign::SeqLocCharOption kMaskCharOpt =
                (kIsBlastn ? CDisplaySeqalign::eN : CDisplaySeqalign::eX);
            s_MaskQuerySeq(*aln_vec, query_seq, *mask_info, kMaskCharOpt, 
                           q_frame);
        }

        xhsp->SetQseq(query_seq);
        xhsp->SetHseq(subject_seq);
        xhsp->SetMidline(middle_seq);
        xhsp->SetPositive(num_positives);


        xhsp_list.push_back(xhsp);
    }
}

/// Fill the CHit object in BLAST XML output, given an alignment and other
/// information.
/// @param hit CHit object to fill [in] [out]
/// @param align_in Sequence alignment [in]
/// @param scope Scope for retrieving sequences [in]
/// @param matrix ASCII-alphabet matrix for calculation of positives [in]
/// @param mask_info List of masking locations [in]
/// @param ungapped Is this an ungapped search? [in]
static void 
s_SeqAlignToXMLHit(CRef<CHit>& hit, const CSeq_align& align_in, CScope* scope,
                   const CBlastFormattingMatrix* matrix, 
                   const ncbi::TMaskedQueryRegions* mask_info, 
                   bool ungapped, int master_gentice_code, int slave_genetic_code)
{
    _ASSERT(align_in.GetSegs().IsDisc());
    const CSeq_align_set& kAlignSet = align_in.GetSegs().GetDisc();

    // Check if the list is empty. Then there is nothing to fill.
    if (kAlignSet.Get().empty())
        return;

    // Create the new CHit object.
    hit.Reset(new CHit());

    const CSeq_id& kSeqId = kAlignSet.Get().front()->GetSeq_id(1);

    try {
        const CBioseq_Handle& kSubjBioseqHandle = scope->GetBioseqHandle(kSeqId);
        /// @todo FIXME Should this be passed somehow? For now the following
        /// list is empty.
        list<int> use_this_gi; 
        string seqid;
        string defline;
        /// @todo FIXME Should the "show gi" option be passed to the XML 
        /// formatter? At this time gis are shown unconditionally.
        CShowBlastDefline::GetBioseqHandleDeflineAndId(kSubjBioseqHandle, 
                                                       use_this_gi, seqid, 
                                                       defline, true);
        if (defline == NcbiEmptyString)
            defline = "No definition line";
        
        hit->SetId(seqid);
        hit->SetDef(defline);

        // Find the "best" Seq-id, and retrieve accession (without version).
        CSeq_id_Handle idh = 
            sequence::GetId(kSubjBioseqHandle, sequence::eGetId_Best); 
        string accession = CAlignFormatUtil::GetLabel(idh.GetSeqId());
        hit->SetAccession(accession);
        
        int length = sequence::GetLength(kSeqId, scope);
        hit->SetLen(length);
    } catch (const CException&) {
        // If Bioseq handle didn't return some of the information, and not all
        // mandatory couldn't be filled, skip this hit completely.
        //hit.Reset(NULL);
        hit->SetId(kSeqId.AsFastaString());
        hit->SetDef("Unknown");
        hit->SetAccession("Unknown");
        hit->SetLen(0);
    };
        
    // For ungapped search, multiple HSPs, possibly from different strands,
    // are packed into a single Seq-align.
    // The C++ utility functions cannot deal with such Seq-aligns, as they
    // expect one Seq-align per alignment (HSP). Hence we need to expand the
    // Seq-align-set obtained for an ungapped search.
    if (ungapped) {
        CRef<CSeq_align_set> expanded_align_set =
            CDisplaySeqalign::PrepareBlastUngappedSeqalign(kAlignSet);
        
        s_SeqAlignSetToXMLHsps(hit->SetHsps(), *expanded_align_set, scope, 
                               matrix, mask_info, master_gentice_code, slave_genetic_code);
    } else {
        s_SeqAlignSetToXMLHsps(hit->SetHsps(), kAlignSet, scope, matrix, 
                               mask_info, master_gentice_code, slave_genetic_code);
    }
}

/// Retrieves subject Seq-id from a Seq-align
/// @param align Seq-align object [in]
/// @return Subject Seq-id for this Seq-align.
static const CSeq_id*
s_GetSubjectId(const CSeq_align& align)
{
    if (align.GetSegs().IsDenseg()) {
        return align.GetSegs().GetDenseg().GetIds()[1];
    } else if (align.GetSegs().IsDendiag()) {
        return align.GetSegs().GetDendiag().front()->GetIds()[1];
    } else if (align.GetSegs().IsStd()) {
        return align.GetSegs().GetStd().front()->GetIds()[1];
    }

    return NULL;
}
 
/// Fills the list of CHit objects, given a list of Seq-aligns. 
/// @param hits List of CHit objects to fill [in] [out]
/// @param alnset Seq-align-set object containing a list of sequence 
///               alignments. [in]
/// @param scope Scope for retrieving sequences. [in]
/// @param matrix ASCII-alphabet matrix for calculation of positives. [in]
/// @param mask_info List of masking locations. [in]
/// @param ungapped Is this an ungapped search? [in]
static void
s_SeqAlignSetToXMLHits(list <CRef<CHit> >& hits, const CSeq_align_set& alnset,
                       CScope* scope, const CBlastFormattingMatrix* matrix, 
                       const ncbi::TMaskedQueryRegions* mask_info,
                       bool ungapped, int master_gentice_code, int slave_genetic_code,
                       CNcbiOstream *out_stream)
{
    // If there are no hits for this query, return with empty Hits list.
    if (alnset.Get().empty())
        return;
    
    CSeq_align_set::Tdata::const_iterator iter = alnset.Get().begin();

    int index = 1;
    bool incremental_output = (bool)out_stream;
    while (iter != alnset.Get().end()) {
        CRef<CHit> new_hit;
        // Retrieve the next set of results for a single subject sequence.
        // If the next Seq-align is discontinuous, then take it as is, 
        // otherwise go along the chain of Seq-aligns until the subject Seq-id
        // changes, then wrap the single subject list into a discontinuous 
        // Seq-align.
        if ((*iter)->GetSegs().IsDisc()) {
            s_SeqAlignToXMLHit(new_hit, *(*iter), scope, matrix, mask_info, 
                               ungapped, master_gentice_code, slave_genetic_code);
            ++iter;
        } else {
            CSeq_align_set one_subject_alnset;
            CConstRef<CSeq_id> current_id(s_GetSubjectId(*(*iter)));
            for ( ; iter != alnset.Get().end(); ++iter) {
                CConstRef<CSeq_id> next_id(s_GetSubjectId(*(*iter)));
                if (!current_id->Match(*next_id)) {
                    break;
                }
                one_subject_alnset.Set().push_back(*iter);
            }
            CSeq_align disc_align_wrap;
            disc_align_wrap.SetSegs().SetDisc(one_subject_alnset);
            s_SeqAlignToXMLHit(new_hit, disc_align_wrap, scope, matrix, 
                               mask_info, ungapped, master_gentice_code, slave_genetic_code);
        }
        
        if (new_hit) {
            new_hit->SetNum(index);
            ++index;
            if( !incremental_output ) hits.push_back(new_hit);
	    else
	    {
		CNcbiOstrstream  one_hit_os;
		auto_ptr<CObjectOStreamXml> xml_one_hit_os (new CObjectOStreamXml (one_hit_os,false));
		xml_one_hit_os->SetEncoding(eEncoding_Ascii);
		xml_one_hit_os->SetReferenceDTD(false);
		xml_one_hit_os->Write( &(*new_hit), new_hit->GetThisTypeInfo() ); 
		// remove leading xml version 
		string out_str = string(CNcbiOstrstreamToString(one_hit_os));
		string::size_type start_xml_pos = out_str.find("<?xml");
		if( start_xml_pos != string::npos ) {
		    string::size_type  end_xml_pos = out_str.find_first_of("\n\r");
		    out_str.erase(0,end_xml_pos+1);
		}
		*out_stream << out_str ; 
	    }

        }
    }
}

/// Add an "iteration" to the BLAST XML report, corresponding to all alignments
/// for a single query.
/// @param bxmlout BLAST XML output object [in]
/// @param alnset Set of aligments for a given query. [in]
/// @param seqloc This query's Seq-loc. [in]
/// @param scope Scope for retrieving sequences. [in]
/// @param matrix ASCII-alphabet matrix for calculation of positives. [in]
/// @param mask_info List of masking locations. [in]
/// @param index This query's index [in]
/// @param stat Search statistics for this query, already filled. [in]
/// @param is_ungapped Is this an ungapped search? [in]
/// @param out_stream Stream for incremental output, ignore if NULL [out]
static void
s_BlastXMLAddIteration(CBlastOutput& bxmlout, const CSeq_align_set* alnset,
                       const CSeq_loc& seqloc, CScope* scope, 
                       const CBlastFormattingMatrix* matrix, 
                       const ncbi::TMaskedQueryRegions* mask_info,
                       int index, CStatistics& stat, bool is_ungapped,
                       int master_gentice_code, int slave_genetic_code,
                       const vector<string>& messages,
		       CNcbiOstream *out_stream)
{
    bool incremental_output = (bool) out_stream;
    list<CRef<CIteration> >& iterations = bxmlout.SetIterations();

    CRef<CIteration> one_query_iter(new CIteration());
    
    one_query_iter->SetIter_num(index + 1);
    
    string query_def = NcbiEmptyString;

    // If Bioseq handle cannot return a title string here, it is not critical.
    // But make sure the exceptions are caught.
    try {
        CBioseq_Handle bh = scope->GetBioseqHandle(seqloc);
        // Get the full query Seq-id string.
        const CBioseq& kQueryBioseq = *bh.GetBioseqCore();
        one_query_iter->SetQuery_ID(
            CBlastFormatUtil::GetSeqIdString(kQueryBioseq));
        query_def = sequence::GetTitle(bh);
    } catch (const CException&) {
        const CSeq_id& kSeqId = sequence::GetId(seqloc, scope);
        one_query_iter->SetQuery_ID(kSeqId.AsFastaString());
    };

    if (query_def == NcbiEmptyString)
        query_def = "No definition line";
    one_query_iter->SetQuery_def(query_def);

    one_query_iter->SetQuery_len(sequence::GetLength(seqloc, scope));
    one_query_iter->SetStat(stat);
    if (messages.size() > 0 && !messages[index].empty())
       one_query_iter->SetMessage(messages[index]);
    // have serialized CIteration split and output first portion before hits
    string serial_xml_start, serial_xml_end;
    if( incremental_output ) {
    //bool add_dtd_reference = false, add_xml_version = false;	
        s_SerializeAndSplitBy( *one_query_iter, "</Iteration_query-len>",
                               serial_xml_start, serial_xml_end); 
        *out_stream << serial_xml_start << "\n<Iteration_hits>\n"; // PART BEFORE HITS
    }
    
    // Only add hits if they exist.
    if (alnset) {
        s_SeqAlignSetToXMLHits(one_query_iter->SetHits(), *alnset,
                               scope, matrix, mask_info, is_ungapped, 
                               master_gentice_code, slave_genetic_code,
			       out_stream);
    }

    if( incremental_output ) *out_stream << "</Iteration_hits>" << serial_xml_end;
    else 
    iterations.push_back(one_query_iter);
}

/// Fills the parameters part of the BLAST XML output.
/// @param bxmlout BLAST XML output object [in] [out]
/// @param data Data structure, from which all necessary information can be 
///             retrieved [in]
static void
s_SetBlastXMLParameters(CBlastOutput& bxmlout, const IBlastXMLReportData* data)
{
    CParameters& params = bxmlout.SetParam();
    string matrix_name = data->GetMatrixName();
    if (matrix_name != NcbiEmptyString)
        params.SetMatrix(matrix_name);
    params.SetExpect(data->GetEvalueThreshold());
    params.SetGap_open(data->GetGapOpeningCost());
    params.SetGap_extend(data->GetGapExtensionCost());

    int val;
    if ((val = data->GetMatchReward()) != 0)
        params.SetSc_match(val);

    if ((val = data->GetMismatchPenalty()) != 0)
        params.SetSc_mismatch(val);

    string str;
    if ((str = data->GetPHIPattern()) != NcbiEmptyString)
        params.SetPattern(str);

    if ((str = data->GetFilterString()) != NcbiEmptyString)
        params.SetFilter(str);
}

/// Fills the search statistics part of the BLAST XML output for all queries.
/// @param stat_vec Vector of the CStatistics objects, to be filled. [in] [out]
/// @param data Data structure, from which all necessary information can be 
///             retrieved [in] 
static void
s_BlastXMLGetStatistics(vector<CRef<CStatistics> >& stat_vec,
                        const IBlastXMLReportData* data)
{
    int db_numseq = data->GetDbNumSeqs();
    Int8 db_length = data->GetDbLength();

    for (unsigned int index = 0; index < data->GetNumQueries(); ++index) {
        CRef<CStatistics> stat(new CStatistics());
        stat->SetDb_num(db_numseq);
        stat->SetDb_len(db_length);
        stat->SetHsp_len(data->GetLengthAdjustment(index));
        stat->SetEff_space((double)data->GetEffectiveSearchSpace(index));
        stat->SetKappa(data->GetKappa(index));
        stat->SetLambda(data->GetLambda(index));
        stat->SetEntropy(data->GetEntropy(index));
        stat_vec.push_back(stat);
    }
}

/// Given BLAST task, returns enumerated value for the publication to be 
/// referenced.
/// @param program BLAST task [in]
/// @return What publication to reference?
static CReference::EPublication
s_GetBlastPublication(EProgram program)
{
    CReference::EPublication publication = CReference::eMaxPublications;

    switch (program) {
    case eMegablast:
        publication = CReference::eMegaBlast; break;
    case ePHIBlastp: case ePHIBlastn:
        publication = CReference::ePhiBlast; break;
    case ePSIBlast:
        publication = CReference::eCompBasedStats; break;
    default:
        publication = CReference::eGappedBlast; break;
    }
    return publication;
}

/// Fills all fields in the data structure for a BLAST XML report.
/// @param bxmlout BLAST XML report data structure to fill [in] [out]
/// @param data  Data structure, from which all necessary information can be 
///             retrieved [in]
/// @param out_stream Output  stream for incremental output, ignore if NULL [out]
void 
BlastXML_FormatReport(CBlastOutput& bxmlout, const IBlastXMLReportData* data, CNcbiOstream *out_stream)
{
    bool incremental_output = (bool)out_stream;
    string program_name = data->GetBlastProgramName();
    bxmlout.SetProgram(program_name);
    bxmlout.SetVersion(CBlastFormatUtil::BlastGetVersion(program_name));
    EProgram blast_task = data->GetBlastTask();
    bxmlout.SetReference(CReference::GetString(s_GetBlastPublication(blast_task)));
    bxmlout.SetDb(data->GetDatabaseName());

    const CSeq_loc* kSeqLoc = data->GetQuery(0);
    if (!kSeqLoc)
        NCBI_THROW(CException, eUnknown, "Query Seq-loc is not available");

    CRef<CScope> scope(data->GetScope(0));
    
    string query_def = NcbiEmptyString;

    // Try to retrieve all Seq-ids, using a Bioseq handle. If this fails,
    // report the one available Seq-id, retrieved from the query Seq-loc.
    try {
        CBioseq_Handle bh = scope->GetBioseqHandle(*kSeqLoc);
        // Get the full query Seq-id string.
        const CBioseq& kQueryBioseq = *bh.GetBioseqCore();
        bxmlout.SetQuery_ID(CBlastFormatUtil::GetSeqIdString(kQueryBioseq));
        query_def = sequence::GetTitle(bh);
    } catch (const CException&) {
        const CSeq_id& seqid = sequence::GetId(*kSeqLoc, scope);
        bxmlout.SetQuery_ID(seqid.AsFastaString());
    };

    if (query_def == NcbiEmptyString)
        query_def = "No definition line";

    bxmlout.SetQuery_def(query_def);

    bxmlout.SetQuery_len(sequence::GetLength(*kSeqLoc, scope));

    s_SetBlastXMLParameters(bxmlout, data);

    auto_ptr< CBlastFormattingMatrix > matrix(data->GetMatrix());

    vector<CRef<CStatistics> > stat_vec;
    s_BlastXMLGetStatistics(stat_vec, data);
    //serialized data before and after BlastOutput_param
    string serial_xml_start, serial_xml_end;
    if( incremental_output ) {
	bool add_dtd_reference = true, add_xml_version = true;
	s_SerializeAndSplitBy( bxmlout, "</BlastOutput_param>", 
		serial_xml_start, serial_xml_end,
		add_dtd_reference, add_xml_version );
	// incremental_output	
        *out_stream << serial_xml_start << "\n<BlastOutput_iterations>" ; 
    }

    for (unsigned int index = 0; index < data->GetNumQueries(); ++index) {
        // Check that this query's Seq-loc is available.
        const CSeq_loc* seqloc = data->GetQuery(index);
        if (!seqloc) {
            string message = 
                "Unable to retrieve query " + NStr::IntToString(index);
            NCBI_THROW(CException, eUnknown, message);
        }
        s_BlastXMLAddIteration(bxmlout, data->GetAlignment(index), *seqloc, 
                               data->GetScope(index), matrix.get(), 
                               data->GetMaskLocations(index), index, 
                               *stat_vec[index], !data->GetGappedMode(),
                               data->GetMasterGeneticCode(),  data->GetSlaveGeneticCode(),
                               data->GetMessages(),
			       out_stream);
    }
    if(incremental_output) *out_stream <<  "\n</BlastOutput_iterations>" << serial_xml_end << endl;
}

/// serialize givem object and split data by provided XML tag for futher manual integrationa
//  <start of a  XML data ><TAG_NAME></TAG_NAME>< .., end of XML data>
//static bool s_SerializeAndSplit(TConstObjectPtr object, TTypeInfo typeInfo )
static bool s_SerializeAndSplitBy(const CSerialObject &object,
		const char *tag, // tag name to break XML data by in form </TAG_NAME>
		string &start_part,   // part before </TAG_NAME>
		string &end_part,
		bool add_reference_dtd,    // part starting from </TAG_NAME> 
		bool add_xml_version )
{
    bool res_code = false; // not  implemented
    TTypeInfo typeInfo = object.GetThisTypeInfo();
    string breake_by_tag = tag;
    start_part="<NOT SET>";
    end_part="</NOT SET>";
    CNcbiOstrstream one_iter_ss_os;
    {
	auto_ptr<CObjectOStreamXml> xml_one_iter_os(new CObjectOStreamXml (one_iter_ss_os,false));
	xml_one_iter_os->SetEncoding(eEncoding_Ascii);
	xml_one_iter_os->SetVerifyData( eSerialVerifyData_No );
	xml_one_iter_os->SetReferenceDTD(add_reference_dtd);
	if( add_xml_version )
	    xml_one_iter_os->Write(&object, typeInfo );
	else 
	    xml_one_iter_os->WriteObject(&object, typeInfo );
    }
    string out_str = string(CNcbiOstrstreamToString(one_iter_ss_os));
    string::size_type  iterations_insert_point = out_str.find( breake_by_tag );
    if( iterations_insert_point != string::npos ){
	iterations_insert_point += breake_by_tag.length();
	start_part = out_str.substr(0,iterations_insert_point);
	end_part = out_str.substr(iterations_insert_point);
	res_code = true;
    }
    else {
	start_part = out_str;
    }
    return res_code;
}

END_NCBI_SCOPE
