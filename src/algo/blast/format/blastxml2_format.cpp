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
* Author:Amelia Fong
*
* ===========================================================================
*/

/// @file blastxml2_format.cpp
/// Formatting of BLAST results in XML2 form, using the BLAST XML2 specification.

#include <ncbi_pch.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>

#include <algo/blast/format/blastxml2_format.hpp>
#include <algo/blast/format/blastfmtutil.hpp>
#include <objtools/align_format/showdefline.hpp>
#include <objtools/align_format/align_format_util.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>


#include <objects/blastxml2/blastxml2__.hpp>
#include <serial/objostrxml.hpp>
#include <serial/objostrjson.hpp>

#include <algo/blast/api/version.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(blast);
USING_SCOPE(align_format);


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

/// Creates a list of blastxml2::CHsp structures for the XML output, given a list of
/// Seq-aligns.
/// @param xhsp_list List of blastxml2::CHsp's to populate [in] [out]
/// @param alnset Set of alignments to get data from [in]
/// @param scope Scope for retrieving sequences [in]
/// @param matrix 256x256 matrix for calculating positives for a protein search.
///               NULL is passed for a nucleotide search.
/// @param mask_info Masking locations [in]
static void
s_SeqAlignSetToXMLHsps(list<CRef<blastxml2::CHsp> >& xhsp_list,
                       const CSeq_align_set& alnset, CRef<CScope> scope,
                       const CBlastFormattingMatrix* matrix,
                       const ncbi::TMaskedQueryRegions & mask_info,
                       int master_gentic_code, int slave_genetic_code)
{
    int index = 1;
    ITERATE(CSeq_align_set::Tdata, iter, alnset.Get()) {
        CRef<blastxml2::CHsp> xhsp(new blastxml2::CHsp());
        const CSeq_align& kAlign = *(*iter);
        xhsp->SetNum(index);
        ++index;
        bool query_is_na, subject_is_na;
        int query_length, subject_length;

        int score, num_ident;
        double bit_score;
        double evalue;
        int sum_n;
        list<TGi> use_this_gi;
        CBlastFormatUtil::GetAlnScores(kAlign, score, bit_score, evalue, sum_n, 
                                       num_ident, use_this_gi);

        //Print 6 significant digits for double values
        char tmp[512];
        sprintf(tmp,"%.*g", 6, bit_score );
        bit_score = atof(tmp);
        sprintf(tmp,"%.*g", 6, evalue );
        evalue = atof(tmp);

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




        // Do not trust the identities count in the Seq-align, because if masking 
        // was used, then masked residues were not counted as identities. 
        // Hence retrieve the sequences present in the alignment and count the 
        // identities again.
        string query_seq;
        string subject_seq;
        string middle_seq;
        string masked_query_seq;

        // For blastn search, the matches are shown as '|', and mismatches as
        // ' '; For all other searches matches are shown as matched characters,
        // mismatches as ' ', and positives as '+'.
        // This is a blastn search if and only if both query and subject are
        // nucleotide, and it is not a translated search.
        const bool kIsBlastn =
            (query_is_na && subject_is_na && !kTranslated);

        const CDense_seg * ds_pt = &kDenseg;
        CRef<CDense_seg> reversed_ds;
        // For non-transalted reverse strand alignments, show plus strand on
        // query and minus strand on subject. To accomplish this, Dense-seg must
        // be reversed.
        if (!kTranslated && kDenseg.IsSetStrands() &&
            kDenseg.GetStrands().front() == eNa_strand_minus)
        {
        	reversed_ds.Reset(new CDense_seg);
            reversed_ds->Assign(kDenseg);
            reversed_ds->Reverse();
            ds_pt = &(*reversed_ds);
       }

        int q_start, q_end, s_start, s_end, q_frame=0, s_frame=0;

        unsigned int num_gaps = 0;
        int align_length = 0;

        if (kAlign.GetSegs().IsDendiag())
        {
        	align_length = final_aln->GetAlignLength();
        	q_start = final_aln->GetSeqStart(0) + 1;
        	q_end = final_aln->GetSeqStop(0) + 1;
        	s_start = final_aln->GetSeqStart(1) + 1;
        	s_end = final_aln->GetSeqStop(1) + 1;
        }
        else
        {
        	if(!kTranslated)
        	{
        		num_gaps = kAlign.GetTotalGapCount();
        		align_length = kAlign.GetAlignLength();
        	}
        	q_start = kAlign.GetSeqStart(0) + 1;
        	q_end = kAlign.GetSeqStop(0) + 1;
        	s_start = kAlign.GetSeqStart(1) + 1;
        	s_end = kAlign.GetSeqStop(1) + 1;
        }

        if (!kTranslated && query_is_na && subject_is_na) {
           	xhsp->SetQuery_strand("Plus");
            if (eNa_strand_minus == kAlign.GetSeqStrand(0)){
            	xhsp->SetQuery_strand("Minus");
                int tmp = s_start;
                s_start = s_end;
                s_end = tmp;
            }
            else {
            	xhsp->SetQuery_strand("Plus");
            }

            if (eNa_strand_minus == kAlign.GetSeqStrand(1))
            	xhsp->SetQuery_strand("Minus");
            else
            	xhsp->SetHit_strand("Plus");

        } else if (kTranslated) {
        	align_length = final_aln->GetAlignLength();
        	num_gaps = final_aln->GetTotalGapCount();

            if (query_is_na) {
                q_frame = s_GetTranslationFrame(eNa_strand_minus != final_aln->GetSeqStrand(0),
                                                q_start, q_end, query_length);
        		xhsp->SetQuery_frame(q_frame);
        	}
            if (subject_is_na) {
                s_frame = s_GetTranslationFrame(eNa_strand_minus != final_aln->GetSeqStrand(1),
                                                s_start, s_end, subject_length);
                xhsp->SetHit_frame(s_frame);
            }
        }

        xhsp->SetQuery_from(q_start);
        xhsp->SetQuery_to(q_end);
        xhsp->SetHit_from(s_start);
        xhsp->SetHit_to(s_end);

       if (mask_info.empty())
        {
        	CBlastFormatUtil::GetWholeAlnSeqStrings(query_seq,
        	                					   subject_seq,
        	                					   *ds_pt,
        	                					   *scope,
        	                					   master_gentic_code,
        	                					   slave_genetic_code);
        }
       else
       {
          	  CDisplaySeqalign::SeqLocCharOption kMaskCharOpt =
                                 (kIsBlastn ? CDisplaySeqalign::eN : CDisplaySeqalign::eX);

                CBlastFormatUtil::GetWholeAlnSeqStrings(query_seq,
              		  	  	  	  	  	  	  	  	  masked_query_seq,
                 								   	   	  subject_seq,
                      								  *ds_pt,
                      								  *scope,
                      								  master_gentic_code,
                      								  slave_genetic_code,
                      								  mask_info,
                      								  kMaskCharOpt,
                      								  q_frame);
       }

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

        if (mask_info.empty())
    		xhsp->SetQseq(query_seq);
        else
    		xhsp->SetQseq(masked_query_seq);
        xhsp->SetHseq(subject_seq);
        xhsp->SetMidline(middle_seq);
        if(!(query_is_na && subject_is_na && !kTranslated) )
        	xhsp->SetPositive(num_positives);

        xhsp_list.push_back(xhsp);
    }
}

/// Fill the blastxml2::CHit object in BLAST XML output, given an alignment and other
/// information.
/// @param hit blastxml2::CHit object to fill [in] [out]
/// @param align_in Sequence alignment [in]
/// @param scope Scope for retrieving sequences [in]
/// @param matrix ASCII-alphabet matrix for calculation of positives [in]
/// @param mask_info List of masking locations [in]
/// @param ungapped Is this an ungapped search? [in]
/// @param master_genetic_code query genetic code [in]
/// @param slave_genetic_code subject genetic code [in]
/// @param hasTaxDB Have access to taxonomy file [in]
static void 
s_SeqAlignToXMLHit(CRef<blastxml2::CHit>& hit,
				   const CSeq_align& align_in, CRef<CScope> scope,
				   const CBlastFormattingMatrix* matrix,
				   const ncbi::TMaskedQueryRegions & mask_info,
				   bool ungapped, int master_gentice_code,
				   int slave_genetic_code, bool hasTaxDB)
{
    _ASSERT(align_in.GetSegs().IsDisc());
    const CSeq_align_set& kAlignSet = align_in.GetSegs().GetDisc();

    const CSeq_id& kSeqId = kAlignSet.Get().front()->GetSeq_id(1);

    try {
        const CBioseq_Handle& subj_handle = scope->GetBioseqHandle(kSeqId);

        CRef<CBlast_def_line_set> bdlRef =
                                       CSeqDB::ExtractBlastDefline(subj_handle);
        list <CRef<blastxml2::CHitDescr> >  & descr_list = hit->SetDescription();
        
        if(bdlRef.NotEmpty() && bdlRef->IsSet() && (!bdlRef->Get().empty())) {
        	ITERATE(list<CRef<CBlast_def_line> >, itr, bdlRef->Get()) {
        		const CBlast_def_line & defline = **itr;
        		CRef<blastxml2::CHitDescr> hit_exp(new blastxml2::CHitDescr);
        		hit_exp->SetId(CShowBlastDefline::GetSeqIdListString(defline.GetSeqid(), true));

        		 CRef<CSeq_id> best_id = FindBestChoice(defline.GetSeqid(), CSeq_id::Score);
        		 CSeq_id_Handle id_handle = CSeq_id_Handle::GetHandle(*best_id);
        		 string accession = CAlignFormatUtil::GetLabel(id_handle.GetSeqId());
        		 if(accession != kEmptyStr)
        			 hit_exp->SetAccession(accession);

        		if(defline.IsSetTitle())
        			hit_exp->SetTitle(defline.GetTitle());

        		if(defline.IsSetTaxid() && defline.GetTaxid() != 0) {
        			int tax_id = defline.GetTaxid();
       				hit_exp->SetTaxid(tax_id);
        			if(hasTaxDB) {
        				 SSeqDBTaxInfo taxinfo;
        				 CSeqDB::GetTaxInfo(tax_id, taxinfo);
        				 hit_exp->SetSciname(taxinfo.scientific_name);
        			}
        		}
        		descr_list.push_back(hit_exp);
        	}
        }
        else {
        	CRef<blastxml2::CHitDescr> hit_exp(new blastxml2::CHitDescr);
        	list<CRef<objects::CSeq_id> > ids;
        	CShowBlastDefline::GetSeqIdList(subj_handle, ids);
        	hit_exp->SetId(CShowBlastDefline::GetSeqIdListString(ids, true));
   			hit_exp->SetTitle(sequence::CDeflineGenerator().GenerateDefline(subj_handle));
        	descr_list.push_back(hit_exp);
        }

        
        int length = subj_handle.GetBioseqLength();
        hit->SetLen(length);
    } catch (const CException&) {
    	CRef<blastxml2::CHitDescr> hit_exp(new blastxml2::CHitDescr);
    	hit_exp->SetId(kSeqId.AsFastaString());
        hit->SetDescription().push_back(hit_exp);
        hit->SetLen(sequence::GetLength(kSeqId, scope));
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
 



/// Fills the list of blastxml2::CHit objects, given a list of Seq-aligns.
/// @param hits List of blastxml2::CHit objects to fill [in] [out]

static void
s_SetBlastXMlHitList(list<CRef<blastxml2::CHit> >& hits, const IBlastXML2ReportData* data, int num)
{
    

	CConstRef<objects::CSeq_align_set>  alnset = data->GetAlignmentSet(num);
    CSeq_align_set::Tdata::const_iterator iter = alnset->Get().begin();

    CRef<CScope> scope = data->GetScope();
    const CBlastFormattingMatrix* matrix = data->GetMatrix();
    const ncbi::TMaskedQueryRegions & mask_info = data->GetMaskLocations();
    bool ungapped = !(data->IsGappedSearch());
    int master_gentice_code = data->GetQueryGeneticCode();
    int slave_genetic_code = data->GetDbGeneticCode();
    bool hasTaxDB = data->CanGetTaxInfo();

    int index = 1;
    while (iter != alnset->Get().end()) {
        CRef<blastxml2::CHit> new_hit(new blastxml2::CHit);
        new_hit->SetNum(index);
        index ++;
        // Retrieve the next set of results for a single subject sequence.
        // If the next Seq-align is discontinuous, then take it as is, 
        // otherwise go along the chain of Seq-aligns until the subject Seq-id
        // changes, then wrap the single subject list into a discontinuous 
        // Seq-align.
        if ((*iter)->GetSegs().IsDisc()) {
            s_SeqAlignToXMLHit(new_hit, *(*iter), scope, matrix, mask_info,
                               ungapped, master_gentice_code, slave_genetic_code, hasTaxDB);
            ++iter;
        } else {
            CSeq_align_set one_subject_alnset;
            CConstRef<CSeq_id> current_id(s_GetSubjectId(*(*iter)));
            for ( ; iter != alnset->Get().end(); ++iter) {
                CConstRef<CSeq_id> next_id(s_GetSubjectId(*(*iter)));
                if (!current_id->Match(*next_id)) {
                    break;
                }
                one_subject_alnset.Set().push_back(*iter);
            }
            CSeq_align disc_align_wrap;
            disc_align_wrap.SetSegs().SetDisc(one_subject_alnset);
            s_SeqAlignToXMLHit(new_hit, disc_align_wrap, scope, matrix,
                               mask_info, ungapped, master_gentice_code, slave_genetic_code, hasTaxDB);
        }
        
        hits.push_back(new_hit);
    }
}


/// Fills the parameters part of the BLAST XML output.
/// @param bxmlout BLAST XML output object [in] [out]
/// @param data Data structure, from which all necessary information can be 
///             retrieved [in]
static void
s_SetBlastXMLParameters(blastxml2::CParameters & params, const IBlastXML2ReportData* data)
{
    string matrix_name = data->GetMatrixName();
    if (matrix_name != NcbiEmptyString)
        params.SetMatrix(matrix_name);

    params.SetExpect(data->GetEvalueThreshold());

    int val;
    string str;
    if ((val = data->GetMatchReward()) != 0)
        params.SetSc_match(val);

    if ((val = data->GetMismatchPenalty()) != 0)
        params.SetSc_mismatch(val);

    if(data->IsGappedSearch()) {
    	params.SetGap_open(data->GetGapOpeningCost());
    	params.SetGap_extend(data->GetGapExtensionCost());
    }
    if ((str = data->GetPHIPattern()) != NcbiEmptyString)
        params.SetPattern(str);

    if ((str = data->GetFilterString()) != NcbiEmptyString)
        params.SetFilter(str);

    if ((str = data->GetBl2seqMode()) != NcbiEmptyString)
    	params.SetBl2seq_mode(str);

    if((val = data->GetCompositionBasedStats()) != 0)
    	params.SetCbs(val);

    if((str = data->GetEntrezQuery()) != NcbiEmptyString)
    	params.SetEntrez_query(str);

    if((val = data->GetQueryGeneticCode()) != 0)
    	params.SetQuery_gencode(val);

    if((val = data->GetDbGeneticCode()) != 0)
    	params.SetDb_gencode(val);
}

/// Fills the search statistics part of the BLAST XML output for all queries.
/// @param stat_vec Vector of the blastxml2::CStatics objects, to be filled. [in] [out]
/// @param data Data structure, from which all necessary information can be 
///             retrieved [in] 
static void
s_SetBlastXMLStatistics(blastxml2::CStatistics & stats,
                        const IBlastXML2ReportData* data, int num)
{
	if(!data->IsBl2seq()) {
    	stats.SetDb_num(data->GetDbNumSeqs());
    	stats.SetDb_len(data->GetDbLength());
	}

    stats.SetHsp_len(data->GetLengthAdjustment(num));
    stats.SetEff_space(data->GetEffectiveSearchSpace(num));
    stats.SetKappa(data->GetKappa(num));
    stats.SetLambda(data->GetLambda(num));
    stats.SetEntropy(data->GetEntropy(num));
}


static void
s_SetBlastXMLSearch(blastxml2::CSearch & search,
                    const IBlastXML2ReportData* data, int num)
{
	 CConstRef<objects::CSeq_loc> q_loc = data->GetQuerySeqLoc();
	 const CSeq_id * q_id = q_loc->GetId();
	 CRef<CScope> scope = data->GetScope();
	 try {
	        CBioseq_Handle bh = scope->GetBioseqHandle(*q_id);
	        // Get the full query Seq-id string.
	        const CBioseq& q_bioseq = *bh.GetBioseqCore();
	        search.SetQuery_id(CBlastFormatUtil::GetSeqIdString(q_bioseq));
	        string q_title = sequence::CDeflineGenerator().GenerateDefline(bh);
	        if(q_title != kEmptyStr)
	        	search.SetQuery_title(q_title);
	    } catch (const CException&) {
	        search.SetQuery_id(q_id->AsFastaString());
	 }

	search.SetQuery_len(sequence::GetLength(*q_loc, &(*scope)));

	if(!data->GetMaskLocations().empty()) {
		list<CRef< blastxml2::CRange> > & masks = search.SetQuery_masking();
		TMaskedQueryRegions mask_locs = data->GetMaskLocations();
		ITERATE(TMaskedQueryRegions,  itr, mask_locs) {
			CRef<CSeqLocInfo> loc = *itr;
			if(loc->GetStrand() == eNa_strand_minus)
				continue;
			CRef<blastxml2::CRange> rng (new blastxml2::CRange);
			rng->SetFrom(loc->GetInterval().GetFrom());
			rng->SetTo(loc->GetInterval().GetTo());
			masks.push_back(rng);
		}
	}

	blastxml2::CStatistics & stats = search.SetStat();
	s_SetBlastXMLStatistics(stats, data, num);

	string msg = data->GetMessages(num);
	// Check if the list is empty. Then there is nothing to fill.
	if (data->GetAlignmentSet(num).Empty()) {
		msg += " \n";
		msg += CBlastFormatUtil::kNoHitsFound;
	   	search.SetMessage(msg);
	   	return;
	}

	if(msg != kEmptyStr)
	   	search.SetMessage(msg);

	list<CRef<blastxml2::CHit> > & hit_list = search.SetHits();
	s_SetBlastXMlHitList(hit_list, data, num);
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
    case eDeltaBlast:
        publication = CReference::eDeltaBlast; break;
    default:
        publication = CReference::eGappedBlast; break;
    }
    return publication;
}

static void s_FillBlastOutput(blastxml2::CBlastOutput& bxmlout, const IBlastXML2ReportData* data)
{
	if(data == NULL)
		 NCBI_THROW(CException, eUnknown, "blastxml2: NULL XML2ReportData pointer");

	bxmlout.Reset();
	blastxml2::CReport & report = bxmlout.SetReport();
	string program_name = data->GetBlastProgramName();
	report.SetProgram(program_name);
	report.SetVersion(CBlastFormatUtil::BlastGetVersion(program_name));
	EProgram blast_task = data->GetBlastTask();
	report.SetReference(CReference::GetString(s_GetBlastPublication(blast_task)));
	if(!data->GetSubjectIds().empty()) {
		report.SetSearch_target().SetSubjects() = data->GetSubjectIds();
	}
	else {
		report.SetSearch_target().SetDb(data->GetDatabaseName());
	}

	blastxml2::CParameters & params = report.SetParams();
	s_SetBlastXMLParameters(params, data);

	blastxml2::CResults & results = report.SetResults();
	if(data->IsBl2seq()) {
		list<CRef<blastxml2::CSearch> > & bl2seq = results.SetBl2seq();
		for(int i=0; i < data->GetNumOfSearchResults(); i++ ) {
			CRef<blastxml2::CSearch>  search (new blastxml2::CSearch);
			s_SetBlastXMLSearch(*search, data, i);
			bl2seq.push_back(search);
		}

	}
	else if(data->IsIterativeSearch()) {
		list<CRef<blastxml2::CIteration> > & iterations = results.SetIterations();
		for(int i=0; i < data->GetNumOfSearchResults(); i++ ) {
			CRef<blastxml2::CIteration> itr (new blastxml2::CIteration);
			itr->SetIter_num(i+1);
			blastxml2::CSearch & search = itr->SetSearch();
			s_SetBlastXMLSearch(search, data, i);
			iterations.push_back(itr);
		}
	}
	else {
		blastxml2::CSearch & search = results.SetSearch();
		s_SetBlastXMLSearch(search, data, 0);
	}

}

static void
s_WriteXML2Object(blastxml2::CBlastOutput& bxmlout, CNcbiOstream *out_stream)
{
    TTypeInfo typeInfo = bxmlout.GetThisTypeInfo();
    auto_ptr<CObjectOStreamXml> xml_out(new CObjectOStreamXml (*out_stream, eNoOwnership));
    xml_out->SetEncoding(eEncoding_Ascii);
    xml_out->SetVerifyData( eSerialVerifyData_No );
    //xml_out->SetReferenceDTD();
    xml_out->SetReferenceSchema();
    xml_out->SetUseSchemaLocation(true);
    xml_out->SetEnforcedStdXml();
    xml_out->SetDefaultSchemaNamespace("http://blast.ncbi.nlm.nih.gov/");
    xml_out->Write(&bxmlout, typeInfo );
}

/// Fills all fields in the data structure for a BLAST XML report.
/// @param bxmlout BLAST XML report data structure to fill [in] [out]
/// @param data  Data structure, from which all necessary information can be
///             retrieved [in]
/// @param out_stream Output  stream for incremental output, ignore if NULL [out]
void
BlastXML2_FormatReport(const IBlastXML2ReportData* data, CNcbiOstream *out_stream )
{
	blastxml2::CBlastOutput bxmlout;
	try {
		s_FillBlastOutput(bxmlout, data);
		s_WriteXML2Object(bxmlout, out_stream);
	}
	catch(CException &e){
	    ERR_POST(Error << e.GetMsg() << e.what() );
	    return;
	}
	catch(...){
	    ERR_POST(Error << "XML format failed" );
	    return;
	}
}

void
BlastXML2_FormatReport(const IBlastXML2ReportData* data, string file_name)
{
	blastxml2::CBlastOutput bxmlout;
	try {
		CNcbiOfstream out_stream;
		out_stream.open(file_name.c_str(), IOS_BASE::out);
		if(!out_stream.is_open())
			 NCBI_THROW(CException, eInvalid, "Cannot open output file");

		s_FillBlastOutput(bxmlout, data);
		s_WriteXML2Object(bxmlout, &out_stream);
	}
	catch(CException &e){
	    ERR_POST(Error << e.GetMsg() << e.what() );
	    return;
	}
	catch(...){
	    ERR_POST(Error << "XML format failed" );
	    return;
	}
}


void
BlastXML2_FormatError(int exit_code, string err_msg,
                      CNcbiOstream *out_stream)
{
	blastxml2::CBlastOutput bxmlout;
	bxmlout.SetError().SetCode(exit_code);
	if(err_msg != kEmptyStr) {
		bxmlout.SetError().SetMessage(err_msg);
	}
	s_WriteXML2Object(bxmlout, out_stream);
}

static void
s_WriteJSONObject(blastxml2::CBlastOutput& bxmlout, CNcbiOstream *out_stream)
{
    TTypeInfo typeInfo = bxmlout.GetThisTypeInfo();
    auto_ptr<CObjectOStreamJson> json_out(new CObjectOStreamJson (*out_stream, eNoOwnership));
    json_out->SetDefaultStringEncoding(eEncoding_Ascii);
    //json_out.SetUseIndentation(true);
    //json_out.SetUseEol(true);
    json_out->Write(&bxmlout, typeInfo );
}


void
BlastJSON_FormatReport(const IBlastXML2ReportData* data, string file_name)
{
	blastxml2::CBlastOutput bxmlout;
	try {
		CNcbiOfstream out_stream;
		out_stream.open(file_name.c_str(), IOS_BASE::out);
		if(!out_stream.is_open())
			 NCBI_THROW(CException, eInvalid, "Cannot open output file");

		s_FillBlastOutput(bxmlout, data);
		s_WriteJSONObject(bxmlout, &out_stream);
	}
	catch(CException &e){
	    ERR_POST(Error << e.GetMsg() << e.what() );
	    return;
	}
	catch(...){
	    ERR_POST(Error << "JSON format failed" );
	    return;
	}
}

void
BlastJSON_FormatReport(const IBlastXML2ReportData* data, CNcbiOstream *out_stream )
{
	blastxml2::CBlastOutput bxmlout;
	try {
		s_FillBlastOutput(bxmlout, data);
		s_WriteJSONObject(bxmlout, out_stream);
	}
	catch(CException &e){
	    ERR_POST(Error << e.GetMsg() << e.what() );
	    return;
	}
	catch(...){
	    ERR_POST(Error << "JSON format failed" );
	    return;
	}
}

END_NCBI_SCOPE
