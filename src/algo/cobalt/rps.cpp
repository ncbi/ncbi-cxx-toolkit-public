static char const rcsid[] = "$Id$";

/*
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
* ===========================================================================*/

/*****************************************************************************

File name: rps.cpp

Author: Jason Papadopoulos

Contents: Use RPS blast to find domain hits

******************************************************************************/

#include <ncbi_pch.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <algo/blast/api/blast_rps_options.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/cobalt/cobalt.hpp>

#include <objects/blast/Blast4_request.hpp>
#include <objects/blast/Blast4_request_body.hpp>
#include <objects/blast/Blast4_queue_search_reques.hpp>
#include <objects/blast/Blast4_queries.hpp>
#include <objects/blast/Blas_get_searc_resul_reply.hpp>

#include <serial/iterator.hpp>

#include <algorithm>

/// @file rps.cpp
/// Use RPS blast to find domain hits

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

USING_SCOPE(blast);
USING_SCOPE(objects);

/// Given an RPS blast database, load a list of block offsets
/// for each database sequence. The list is resident in a text
/// file, where each line is as follows
/// <pre>
/// [seq ID] [oid of block] [start block offset] [end block offset]
/// </pre>
/// Note that block offsets are zero-based
/// @param blockfile Name of file containing list of offsets [in]
/// @param blocklist the list of offsets read from file [out]
///
void
CMultiAligner::x_LoadBlockBoundaries(string blockfile,
                      vector<SSegmentLoc>& blocklist)
{
    CNcbiIfstream blockstream(blockfile.c_str());
    if (blockstream.bad() || blockstream.fail())
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Cannot open RPS blockfile");

    char buf[64];
    SSegmentLoc tmp;
    int oid = 0;
    int block_idx;
    int start, end;

    blockstream >> buf;
    blockstream >> block_idx;
    blockstream >> start;
    blockstream >> end;
    blocklist.push_back(SSegmentLoc(oid, start, end));

    while (!blockstream.eof()) {
        blockstream >> buf;
        // This allows for new line at the end of block file
         if (blockstream.eof()) { 
            break;
        }
        blockstream >> block_idx;
        blockstream >> start;
        blockstream >> end;

        if (block_idx == 0)
            oid++;

        blocklist.push_back(SSegmentLoc(oid, start, end));
    }
}


void
CMultiAligner::x_RealignBlocks(CHitList& rps_hits,
                               vector<SSegmentLoc>& blocklist,
                               CProfileData& profile_data)
{
    // scale up the gap penalties used by the aligner, to match
    // the scaling used by the RPS PSSMs

    /// @todo FIXME the scale factor should be chosen dynamically
    
    m_Aligner.SetWg(kRpsScaleFactor * m_Options->GetGapOpenPenalty());
    m_Aligner.SetWs(kRpsScaleFactor * m_Options->GetGapExtendPenalty());
    m_Aligner.SetStartWg(kRpsScaleFactor / 2 * m_Options->GetGapOpenPenalty());
    m_Aligner.SetStartWs(kRpsScaleFactor * m_Options->GetGapExtendPenalty());
    m_Aligner.SetEndWg(kRpsScaleFactor / 2 * m_Options->GetGapOpenPenalty());
    m_Aligner.SetEndWs(kRpsScaleFactor * m_Options->GetGapExtendPenalty());
    m_Aligner.SetEndSpaceFree(false, false, false, false);

    // for each RPS hit

    for (int i = 0; i < rps_hits.Size(); i++) {

        CHit *hit = rps_hits.GetHit(i);
        CSequence& query = m_QueryData[hit->m_SeqIndex1];
        int db_seq = hit->m_SeqIndex2;
        int *db_seq_offsets = profile_data.GetSeqOffsets();
        int **pssm = profile_data.GetPssm() + db_seq_offsets[db_seq];
        int db_seq_length = db_seq_offsets[db_seq + 1] - db_seq_offsets[db_seq];
        int last_fudge = 0;

        _ASSERT(!(hit->HasSubHits()));

        // ignore this alignment if its extent is less than
        // 60% of the extent of query and DB sequence

        if ((hit->m_SeqRange1.GetLength() < 0.6 * query.GetLength()) &&
            (hit->m_SeqRange2.GetLength() < 0.6 * db_seq_length)) {
            rps_hits.SetKeepHit(i, false);
            continue;
        }

        SSegmentLoc target(db_seq, hit->m_SeqRange2.GetFrom(), 
                           hit->m_SeqRange2.GetTo());
    
        // locate the first block in the subject sequence
        // that contains a piece of the HSP
    
        vector<SSegmentLoc>::iterator 
                    itr = lower_bound(blocklist.begin(), blocklist.end(),
                                      target, compare_sseg_db_idx());

        _ASSERT(itr != blocklist.end() &&
               target.seq_index == itr->seq_index);

        // walk up to the first block that is not
        // in front of the alignment

        while (itr != blocklist.end() &&
               itr->seq_index == target.seq_index &&
               itr->GetTo() < target.GetFrom()) {
            itr++;
        }

        vector<SSegmentLoc>::iterator prev_itr(itr);
        vector<SSegmentLoc>::iterator next_itr(itr);
		if (itr != blocklist.begin()) {
            prev_itr--;
        }
        next_itr++;
    
        // for each block that contains a portion of the
        // original alignment
    
        while (itr != blocklist.end() && itr->seq_index == db_seq
               && itr->GetFrom() < target.GetTo()) {
    
            const int kMaxFudge = 6;
            TRange q_range, new_s_range; 
            TRange tback_range;

            // calculate the offsets into the subject sequence
            // that correspond to the current block
    
            TRange s_range(itr->range.IntersectionWith(target.range));
            _ASSERT(!s_range.Empty() && itr->range.Contains(s_range));
    
            int left_fudge, right_fudge;

            // calculate how much extra room on the query sequence
            // to allow for realignment. The size of the 'fudge' 
            // to the left is the different between the length of
            // the loop region to the left and the length of the
            // previous fudge, up to a limit of kMaxFudge

            if (itr == blocklist.begin() || 
                prev_itr == blocklist.begin() ||
                prev_itr->seq_index != db_seq) {
                left_fudge = 0;
            }
            else {
                left_fudge = s_range.GetFrom() - 
                        prev_itr->GetTo() - last_fudge - 1;
                left_fudge = min(left_fudge, kMaxFudge);
            }

            // The extra room on the right is half the length
            // of the loop region to the right, up to the same limit

            if (itr == blocklist.end() || 
                next_itr == blocklist.end() ||
                next_itr->seq_index != db_seq) {
                right_fudge = 0;
            }
            else {
                right_fudge = (next_itr->GetFrom() - s_range.GetTo() - 1) / 2;
                right_fudge = min(right_fudge, kMaxFudge);
            }

            last_fudge = right_fudge;

            // compute the start and stop offsets into the
            // query sequence that correspond to the subject range
            // specified by the current block.
    
            hit->GetRangeFromSeq2(s_range, q_range, new_s_range, tback_range);

            // pre-advance the iterators

            if (prev_itr != itr) {
                prev_itr++;
            }
            itr++;
            if (next_itr != blocklist.end()) {
                next_itr++;
            }
    
            // Throw away alignments whose query range is too small
    
            if (q_range.GetLength() <= CHit::kMinHitSize)
                continue;
    
            // or for which the difference between query and database
            // regions is too large (i.e. query sequence has a big gap)

            if (s_range.GetLength() > 3 * q_range.GetLength() / 2) {
                if (m_Options->GetVerbose()) {
                    printf("ignore aligning query %d %d-%d db %d block %d-%d\n",
                        hit->m_SeqIndex1, q_range.GetFrom(), q_range.GetTo(),
                        db_seq, s_range.GetFrom(), s_range.GetTo());
                }
                continue;
            }

            q_range.SetFrom(max(hit->m_SeqRange1.GetFrom(),
                                q_range.GetFrom() - left_fudge));
            q_range.SetTo(min(hit->m_SeqRange1.GetTo(),
                              q_range.GetTo() + right_fudge));

            // Now realign the block to the query sequence

            m_Aligner.SetSequences((const int **)(pssm + s_range.GetFrom()),
                          s_range.GetLength(),
                          (const char *)query.GetSequence() + q_range.GetFrom(),
                          q_range.GetLength());
    
            int score = m_Aligner.Run();
            const CNWAligner::TTranscript tback(m_Aligner.GetTranscript(false));
            int tback_size = tback.size();
            CEditScript final_script;

            if ((tback[0] == CNWAligner::eTS_Delete &&
                 tback[tback_size-1] == CNWAligner::eTS_Insert) ||
                (tback[0] == CNWAligner::eTS_Insert &&
                 tback[tback_size-1] == CNWAligner::eTS_Delete)) {

                // The query region falls outside the DB region.
                // Throw away the alignment and reuse the original one.

                hit->GetRangeFromSeq2(s_range, q_range, s_range, tback_range);

                // throw away alignments that are too small

                if (q_range.GetLength() <= CHit::kMinHitSize || 
                    s_range.GetLength() <= CHit::kMinHitSize)
                    continue;
                score = hit->GetEditScript().GetScore(
                               tback_range, 
                               TOffsetPair(hit->m_SeqRange1.GetFrom(),
                                           hit->m_SeqRange2.GetFrom()),
                               query, pssm,
                               m_Aligner.GetWg(), m_Aligner.GetWs());
                final_script = hit->GetEditScript().MakeEditScript(tback_range);
            }
            else {

                // strip off leading and trailing gaps in the
                // database sequence. Modify the alignment score
                // accordingly

                int first_tback = 0;
                int last_tback = tback_size - 1;
                int q_start = q_range.GetFrom();
                int q_stop = q_range.GetTo();
                int s_start = s_range.GetFrom();
                int s_stop = s_range.GetTo();

                for (int k = 0; k < tback_size && 
                            tback[k] != CNWAligner::eTS_Match; k++) {
                    first_tback++;
                    if (tback[k] == CNWAligner::eTS_Delete)
                        s_start++;
                    else if (tback[k] == CNWAligner::eTS_Insert)
                        q_start++;

                    score -= m_Aligner.GetWs();
                    if (k == 0)
                        score -= m_Aligner.GetEndWg();
                    else if (tback[k] != tback[k-1])
                        score -= m_Aligner.GetWg();
                }

                for (int k = tback_size - 1; k >= 0 && 
                                     tback[k] != CNWAligner::eTS_Match; k--) {
                    last_tback--;
                    if (tback[k] == CNWAligner::eTS_Delete)
                        s_stop--;
                    else if (tback[k] == CNWAligner::eTS_Insert)
                        q_stop--;

                    score -= m_Aligner.GetWs();
                    if (k == tback_size - 1)
                        score -= m_Aligner.GetEndWg();
                    else if (tback[k] != tback[k+1])
                        score -= m_Aligner.GetWg();
                }

                // throw away alignments that are too small

                q_range.Set(q_start, q_stop);
                s_range.Set(s_start, s_stop);
                if (q_range.GetLength() <= CHit::kMinHitSize || 
                    s_range.GetLength() <= CHit::kMinHitSize)
                    continue;

                _ASSERT(tback[first_tback] == CNWAligner::eTS_Match);
                _ASSERT(tback[last_tback] == CNWAligner::eTS_Match);

                final_script = CEditScript::MakeEditScript(tback, 
                                 TRange(first_tback, last_tback));
            }

            // save the new block alignment if the rounded-down
            // version of its score is positive

            if (score > kRpsScaleFactor / 2) {
                hit->InsertSubHit(new CHit(hit->m_SeqIndex1,
                                           hit->m_SeqIndex2,
                                           q_range, s_range,
                                           score, final_script));
            }
        }

        // finish processing hit i

        if (hit->HasSubHits()) {
            hit->ResolveSubHitConflicts(query, pssm,
                                        m_Aligner.GetWg(), 
                                        m_Aligner.GetWs());
            hit->AddUpSubHits();
        }
        else {
            rps_hits.SetKeepHit(i, false);
        }

        // check for interrupt
        if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
            NCBI_THROW(CMultiAlignerException, eInterrupt,
                       "Alignment interrupted");
        }
    }

    // remove RPS hits that do not have block alignments,
    // or were deleted for some other reason

    rps_hits.PurgeUnwantedHits();

    // restore the original gap penalties

    m_Aligner.SetWg(m_Options->GetGapOpenPenalty());
    m_Aligner.SetWs(m_Options->GetGapExtendPenalty());
    m_Aligner.SetStartWg(m_Options->GetEndGapOpenPenalty());
    m_Aligner.SetStartWs(m_Options->GetEndGapExtendPenalty());
    m_Aligner.SetEndWg(m_Options->GetEndGapOpenPenalty());
    m_Aligner.SetEndWs(m_Options->GetEndGapExtendPenalty());
}


void
CMultiAligner::x_FindRPSHits(TSeqLocVector& queries,
                             const vector<int>& indices,
                             CHitList& rps_hits)
{
    _ASSERT(queries.size() == indices.size());

    int num_queries = queries.size();

    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eRPSBlast));

    // deliberately set the cutoff e-value too high, to
    // account for alignments where the gapped score is
    // very different from the ungapped score

    opts->SetEvalueThreshold(max(m_Options->GetRpsEvalue(), 10.0));
    opts->SetFilterString("F");
    opts->SetHitlistSize(m_Options->GetDomainHitlistSize());

    // run RPS blast

    CSearchDatabase search_database(m_Options->GetRpsDb(),
                                    CSearchDatabase::eBlastDbIsProtein);
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(queries));

    CLocalBlast blaster(query_factory, opts, search_database);
    CSearchResultSet results = *blaster.Run();

    // convert the results to the internal format used by
    // the rest of CMultiAligner

    CSeqDB seqdb(m_Options->GetRpsDb(), CSeqDB::eProtein);

    // iterate over queries

    for (int i = 0; i < num_queries; i++) {

        // iterate over hitlists

        ITERATE(CSeq_align_set::Tdata, itr, results[i].GetSeqAlign()->Get()) {

            // iterate over hits

                const CSeq_align& s = **itr;
                const CDense_seg& denseg = s.GetSegs().GetDenseg();
                int align_score = 0;
                double evalue = 0;

                // compute the score of the hit

                ITERATE(CSeq_align::TScore, score_itr, s.GetScore()) {
                    const CScore& curr_score = **score_itr;
                    if (curr_score.GetId().GetStr() == "score")
                        align_score = curr_score.GetValue().GetInt();
                    else if (curr_score.GetId().GetStr() == "e_value")
                        evalue = curr_score.GetValue().GetReal();
                }

                // check if the hit is worth saving
                if (evalue > m_Options->GetRpsEvalue())
                    continue;

                // locate the ID of the database sequence that
                // produced the hit, and save the hit

                int db_oid;
                seqdb.SeqidToOid(*denseg.GetIds()[1], db_oid);
                rps_hits.AddToHitList(new CHit(indices[i], db_oid, 
                                               align_score, denseg));
            }

        // check for interrupt
        if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
            NCBI_THROW(CMultiAlignerException, eInterrupt,
                       "Alignment interrupted");
        }
    }


    //-------------------------------------------------------
    if (m_Options->GetVerbose()) {
        printf("RPS hits:\n");
        for (int i = 0; i < rps_hits.Size(); i++) {
            CHit *hit = rps_hits.GetHit(i);
            printf("query %d %4d - %4d db %d %4d - %4d score %d\n",
                         hit->m_SeqIndex1, 
                         hit->m_SeqRange1.GetFrom(), 
                         hit->m_SeqRange1.GetTo(),
                         hit->m_SeqIndex2,
                         hit->m_SeqRange2.GetFrom(), 
                         hit->m_SeqRange2.GetTo(),
                         hit->m_Score);
        }
        printf("\n\n");
    }
    //-------------------------------------------------------
}


void 
CMultiAligner::x_AssignRPSResFreqs(CHitList& rps_hits,
                                   CProfileData& profile_data)
{
    if (rps_hits.Empty()) {
        return;
    }

    rps_hits.SortByScore();

    // for each hit

    for (int i = 0; i < rps_hits.Size(); i++) {
        CHit *hit = rps_hits.GetHit(i);

        _ASSERT(hit->HasSubHits());

        // skip hit i if it overlaps on the query sequence
        // with a higher-scoring HSP.

        int j;
        for (j = 0; j < i; j++) {
            CHit *better_hit = rps_hits.GetHit(j);

            if (better_hit->m_SeqIndex1 != hit->m_SeqIndex1)
                continue;

            if (rps_hits.GetKeepHit(j) == true &&
                better_hit->m_SeqRange1.IntersectingWith(hit->m_SeqRange1))
                break;
        }
        if (j < i) {
            continue;
        }

        // The hit does not conflict; use the traceback of each block
        // to locate each position where a substitution occurs,
        // and assign the appropriate column of residue frequencies
        // at that position

        CSequence& query = m_QueryData[hit->m_SeqIndex1];
        CSequence::TFreqMatrix& matrix = query.GetFreqs();
        _ASSERT(hit->m_SeqIndex1 < (int)m_RPSLocs.size());
        m_RPSLocs[hit->m_SeqIndex1].clear();

        double **ref_freqs = profile_data.GetResFreqs() + 
                             (profile_data.GetSeqOffsets())[hit->m_SeqIndex2];

        double domain_res_freq_boost = m_Options->GetDomainResFreqBoost();

        NON_CONST_ITERATE(vector<CHit *>, itr, hit->GetSubHit()) {
            CHit *subhit = *itr;
            vector<TOffsetPair> sub_list(
                         subhit->GetEditScript().ListMatchRegions(
                                 TOffsetPair(subhit->m_SeqRange1.GetFrom(),
                                             subhit->m_SeqRange2.GetFrom()) ));

            for (j = 0; j < (int)sub_list.size(); j += 2) {
                TOffsetPair& start_pair(sub_list[j]);
                TOffsetPair& stop_pair(sub_list[j+1]);
                int q = start_pair.first;
                int s = start_pair.second;

                _ASSERT(stop_pair.second - stop_pair.first ==
                       start_pair.second - start_pair.first);
                _ASSERT(stop_pair.first-1 < query.GetLength());

                for (int k = 0; k < stop_pair.first - start_pair.first; k++) {
                    for (int m = 0; m < kAlphabetSize; m++) {
                        matrix(q+k, m) = 
                              (1 - domain_res_freq_boost) * ref_freqs[s+k][m];

                    }
                    matrix(q+k, query.GetLetter(q+k)) += domain_res_freq_boost;
                }
                // mark range as RPS-identified conserved domain
                m_RPSLocs[hit->m_SeqIndex1].push_back(TRange(start_pair.first,
                                                             stop_pair.first));
            }
        }

        // check for interrupt
        if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
            NCBI_THROW(CMultiAlignerException, eInterrupt,
                       "Alignment interrupted");
        }
    }
}


void
CMultiAligner::x_AssignDefaultResFreqs()
{
    // Assign background residue frequencies to otherwise
    // unassigned columns. The actual residue at a given
    // position is upweighted by a specified amount, and
    // all other frequencies are downweighted

    BlastScoreBlk *sbp = BlastScoreBlkNew(BLASTAA_SEQ_CODE, 1);
    Blast_ResFreq *std_freqs = Blast_ResFreqNew(sbp);
    Blast_ResFreqStdComp(sbp, std_freqs);
    double local_res_freq_boost = m_Options->GetLocalResFreqBoost();

    for (size_t i = 0; i < m_QueryData.size(); i++) {
        CSequence& query = m_QueryData[i];
        CSequence::TFreqMatrix& matrix = query.GetFreqs();
        
        for (int j = 0; j < query.GetLength(); j++) {
            for (int k = 0; k < kAlphabetSize; k++) {
                matrix(j, k) = (1 - local_res_freq_boost) *
                                   std_freqs->prob[k];
            }
            matrix(j, query.GetLetter(j)) += local_res_freq_boost;
        }

        // check for interrupt
        if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
            NCBI_THROW(CMultiAlignerException, eInterrupt,
                       "Alignment interrupted");
        }
    }

    if (m_ClustAlnMethod == CMultiAlignerOptions::eToPrototype) {

        for (size_t i = 0; i < m_AllQueryData.size(); i++) {
            CSequence& query = m_AllQueryData[i];
            CSequence::TFreqMatrix& matrix = query.GetFreqs();
            for (int j = 0; j < query.GetLength(); j++) {
                for (int k = 0; k < kAlphabetSize; k++) {
                    matrix(j, k) = (1 - local_res_freq_boost) * 
                        std_freqs->prob[k];
                }
                matrix(j, query.GetLetter(j)) += local_res_freq_boost;
            }
        }

        x_MakeClusterResidueFrequencies();
    }

    Blast_ResFreqFree(std_freqs);
    BlastScoreBlkFree(sbp);
}


bool compare_seqids(const pair<const CSeq_id*, int>& a,
                       const pair<const CSeq_id*, int>& b)
{
    _ASSERT(a.first && b.first);
    return a.first->CompareOrdered(*b.first) > 0;
}

void
CMultiAligner::SetDomainHits(const CBlast4_archive& archive)
{
    // This function sets pre-computed alignments with of queries
    // with conserved domains. Note that the results need not include all
    // cobalt queries and not all domain queries need to be cobalt sequences

    if (m_tQueries.empty()) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput, "Sequences to align "
                   "must be set before domain hits");
    }

    if (m_Options->GetRpsDb().empty()) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Domain database must be set in options if pre-computed "
                   "domain hits are used");
    }

    // initialized all queries as not searched for conserved domains
    m_IsDomainSearched.resize(m_tQueries.size(), false);
    m_DomainHits.PurgeAllHits();

    // create a sorted list query seq_ids
    vector< pair<const CSeq_id*, int> > queries;
    queries.reserve(m_tQueries.size());
    for (size_t i=0;i < m_tQueries.size();i++) {
        _ASSERT(m_tQueries[i]->GetId());
        queries.push_back(make_pair(m_tQueries[i]->GetId(), (int)i));
    }
    sort(queries.begin(), queries.end(), compare_seqids);

    // mark queries for which domain search was done,
    // we use query list here in case the search retruned no results
    const CBlast4_queries& b4_queries
        = archive.GetRequest().GetBody().GetQueue_search().GetQueries();

    if (!b4_queries.IsSeq_loc_list() && !b4_queries.IsBioseq_set()) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput, "Unsupported BLAST"
                   " 4 archive format");
    }

    // if domain queries are seq_locs
    if (b4_queries.IsSeq_loc_list()) {
        ITERATE (list< CRef<CSeq_loc> >, it, b4_queries.GetSeq_loc_list()) {

            // iterate over domain queries

            // search for the query in the list of sequence to align
            pair<const CSeq_id*, int> p((*it)->GetId(), -1);
            vector< pair<const CSeq_id*, int> >::iterator id_itr
                = lower_bound(queries.begin(), queries.end(), p,
                              compare_seqids);

            // if query was found, then mark it as searched
            if (id_itr != queries.end()
                && id_itr->first->CompareOrdered(*p.first) == 0) {
                m_IsDomainSearched[id_itr->second] = true;
            }
        }
    }

    // if domain queries are bioseqs
    if (b4_queries.IsBioseq_set()) {
        CTypeConstIterator<CBioseq> itr(ConstBegin(b4_queries.GetBioseq_set(),
                                                   eDetectLoops));
        // iterate over domain queries
        for (; itr; ++itr) {

            // search for the query in the list of sequences to align
            pair<const CSeq_id*, int> p(itr->GetFirstId(), -1);
            vector< pair<const CSeq_id*, int> >::iterator id_itr
                = lower_bound(queries.begin(), queries.end(), p,
                              compare_seqids);

            // if query was found, then mark it as searched
            if (id_itr != queries.end()
                && id_itr->first->CompareOrdered(*p.first) == 0) {
                m_IsDomainSearched[id_itr->second] = true;
            }
        }
    }
    //-------------------------------------------------------
    if (m_Options->GetVerbose()) {
        printf("Pre-computed RPS queries:\n");
        for (size_t i=0;i < m_tQueries.size();i++) {
            if (m_IsDomainSearched[i]) {
                printf("query: %d\n", (int)i);
            }
        }
        printf("\n");
    }
    //-------------------------------------------------------

    // check if at least one domain query matched cobalt query
    bool is_presearched = false;
    ITERATE (vector<bool>, it, m_IsDomainSearched) {
        if (*it) {
            is_presearched = true;
        }
    }
    // if not, there is no need to analyze domain hits
    if (!is_presearched) {
        // empty array indicates that pre-computed domain hits are not set
        m_IsDomainSearched.clear();
        return;
    }

    CSeqDB seqdb(m_Options->GetRpsDb(), CSeqDB::eProtein);
    is_presearched = false;

    // get domain hits
    const CSeq_align_set& aligns = archive.GetResults().GetAlignments();
    int query_idx = -1;
    const CSeq_id* last_query_id = NULL;
    ITERATE (CSeq_align_set::Tdata, it, aligns.Get()) {

        // iterate over hits

        const CSeq_align& s = **it;
        const CDense_seg& denseg = s.GetSegs().GetDenseg();
        int align_score = 0;
        double evalue = 0;

        // find query index in m_tQueries
        const CSeq_id& query_id = s.GetSeq_id(0);

        // search for query in sequences to align only if the current hit
        // query is different from the previous one
        if (!last_query_id || query_id.CompareOrdered(*last_query_id) != 0) {

            // find query seq id
            pair<const CSeq_id*, int> p(&query_id, -1);
            vector< pair<const CSeq_id*, int> >::iterator id_itr
                = lower_bound(queries.begin(), queries.end(), p,
                              compare_seqids);

            // if the hit query is not to be aligned, then skip processing
            // this Seq_align
            if (id_itr == queries.end()
                || id_itr->first->CompareOrdered(*p.first) != 0) {

                query_idx = -1;
                continue;
            }
            query_idx = id_itr->second;
            last_query_id = id_itr->first;
        }
        if (query_idx < 0) {
            continue;
        }

        // compute the score of the hit

        ITERATE(CSeq_align::TScore, score_itr, s.GetScore()) {
            const CScore& curr_score = **score_itr;
            if (curr_score.GetId().GetStr() == "score")
                align_score = curr_score.GetValue().GetInt();
            else if (curr_score.GetId().GetStr() == "e_value")
                evalue = curr_score.GetValue().GetReal();
        }

        // check if the hit is worth saving
        if (evalue > m_Options->GetRpsEvalue())
            continue;

        // locate the ID of the database sequence that
        // produced the hit, and save the hit

        int db_oid;
        seqdb.SeqidToOid(*denseg.GetIds()[1], db_oid);
        if (db_oid < 0) {
            NCBI_THROW(CMultiAlignerException, eInvalidInput, "The pre-computed"
                       " subject domain " + denseg.GetIds()[1]->AsFastaString()
                       + " does not exist in the domain database "
                       + m_Options->GetRpsDb());
        }
        m_DomainHits.AddToHitList(new CHit(query_idx, db_oid, 
                                           align_score, denseg));

        is_presearched = true;
    }

    if (!is_presearched) {
        m_IsDomainSearched.clear();
    }

    //-------------------------------------------------------
    if (m_Options->GetVerbose()) {
        printf("Pre-computed RPS hits:\n");
        for (int i = 0; i < m_DomainHits.Size(); i++) {
            CHit *hit = m_DomainHits.GetHit(i);
            printf("query %d %4d - %4d db %d %4d - %4d score %d\n",
                         hit->m_SeqIndex1, 
                         hit->m_SeqRange1.GetFrom(), 
                         hit->m_SeqRange1.GetTo(),
                         hit->m_SeqIndex2,
                         hit->m_SeqRange2.GetFrom(), 
                         hit->m_SeqRange2.GetTo(),
                         hit->m_Score);
        }
        printf("\n\n");
    }
    //-------------------------------------------------------


}

void
CMultiAligner::x_FindDomainHits(TSeqLocVector& queries,
                                const vector<int>& indices)
{
    string rps_db = m_Options->GetRpsDb();
    string blockfile = rps_db + ".blocks";
    string freqfile = rps_db + ".freq";

    if (rps_db.empty()) {
        return;
    }

    m_ProgressMonitor.stage = eDomainHitsSearch;

    // empty previously found hits
    m_CombinedHits.PurgeAllHits();

        
    // if there are no pre-computed domain hits search for domain in all
    // queries
    if (m_IsDomainSearched.empty()) {
        m_DomainHits.PurgeAllHits();

        // run RPS blast

        x_FindRPSHits(queries, indices, m_DomainHits);
    }
    else {
        // otherwise, search only queries that were not searched for 
        // pre-computed results

        _ASSERT(m_IsDomainSearched.size() == m_tQueries.size());

        // find if there is at least one query that was not pre-searched
        bool do_search = false;
        for (size_t i=0;i < indices.size();i++) {
            _ASSERT(indices[i] < (int)m_IsDomainSearched.size());
            if (!m_IsDomainSearched[indices[i]]) {
                do_search = true;
                break;
            }
        }

        // search for domains
        if (do_search) {
            TSeqLocVector queries_not_searched;
            vector<int> indices_not_searched;
            for (size_t i=0;i < queries.size();i++) {
                if (!m_IsDomainSearched[indices[i]]) {
                    queries_not_searched.push_back(queries[i]);
                    indices_not_searched.push_back(indices[i]);
                }
            }
            // run RPS blast
            x_FindRPSHits(queries_not_searched, indices_not_searched,
                          m_DomainHits);
        }
    }

    // check for interrupt
    if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
        NCBI_THROW(CMultiAlignerException, eInterrupt,
                   "Alignment interrupted");
    }
        
    vector<SSegmentLoc> blocklist;
    x_LoadBlockBoundaries(blockfile, blocklist);

    // Load the RPS PSSMs and perform block realignment

    CProfileData profile_data;
    profile_data.Load(CProfileData::eGetPssm, rps_db);
    x_RealignBlocks(m_DomainHits, blocklist, profile_data);
    blocklist.clear();
    profile_data.Clear();

    //-------------------------------------------------------
    if (m_Options->GetVerbose()) {
        printf("\n\nBlock alignments with conflicts resolved:\n");
        for (int i = 0; i < m_DomainHits.Size(); i++) {
            CHit *hit = m_DomainHits.GetHit(i);
            NON_CONST_ITERATE(vector<CHit *>, itr, hit->GetSubHit()) {
                CHit *subhit = *itr;
                printf("query %d %4d - %4d db %d %4d - %4d score %d ",
                     subhit->m_SeqIndex1, 
                     subhit->m_SeqRange1.GetFrom(), 
                     subhit->m_SeqRange1.GetTo(),
                     subhit->m_SeqIndex2, 
                     subhit->m_SeqRange2.GetFrom(), 
                     subhit->m_SeqRange2.GetTo(),
                     subhit->m_Score);
    
                printf("\n");
            }
        }
        printf("\n\n");
    }
    //-------------------------------------------------------

    if (m_DomainHits.Empty())
        return;

    x_AssignDefaultResFreqs();

    // propagate the residue frequencies of the best
    // RPS hits onto the query sequences

    m_RPSLocs.resize(m_tQueries.size());
    profile_data.Load(CProfileData::eGetResFreqs, rps_db, freqfile);
    x_AssignRPSResFreqs(m_DomainHits, profile_data);
    profile_data.Clear();

    // Connect together RPS hits to the same region of the
    // same database sequence

    m_DomainHits.MatchOverlappingSubHits(m_CombinedHits);

    // Remove the scaling on the scores

    const int kRpsScale = CMultiAligner::kRpsScaleFactor;
    for (int i = 0; i < m_CombinedHits.Size(); i++) {
        CHit *hit = m_CombinedHits.GetHit(i);
        hit->m_Score = (hit->m_Score + kRpsScale/2) / kRpsScale;
        NON_CONST_ITERATE(CHit::TSubHit, subitr, hit->GetSubHit()) {
            CHit *subhit = *subitr;
            subhit->m_Score = (subhit->m_Score + kRpsScale/2) / kRpsScale;
        }
    }

    //-------------------------------------------------------
    if (m_Options->GetVerbose()) {
        printf("\n\nMatched block alignments:\n");
        for (int i = 0; i < m_CombinedHits.Size(); i++) {
            CHit *hit = m_CombinedHits.GetHit(i);
            NON_CONST_ITERATE(vector<CHit *>, itr, hit->GetSubHit()) {
                CHit *subhit = *itr;
                printf("query %d %4d - %4d query %d %4d - %4d score %d\n",
                         subhit->m_SeqIndex1, 
                         subhit->m_SeqRange1.GetFrom(), 
                         subhit->m_SeqRange1.GetTo(),
                         subhit->m_SeqIndex2, 
                         subhit->m_SeqRange2.GetFrom(), 
                         subhit->m_SeqRange2.GetTo(),
                         subhit->m_Score);
            }
        }
        printf("\n\n");
    }
    //-------------------------------------------------------
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
