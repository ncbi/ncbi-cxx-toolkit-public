#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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
 * Author:  Greg Boratyn
 *
 */

/** @file psi_pssm_input.cpp
 * Implementation of the concrete strategy to obtain PSSM input data for
 * PSI-BLAST.
 */

#include <ncbi_pch.hpp>

// BLAST includes
//#include <algo/blast/api/psi_pssm_input.hpp>
#include <algo/blast/api/cdd_pssm_input.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include "../core/blast_psi_priv.h"

// Object includes
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seq/Seq_descr.hpp>

// Object manager includes
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>


/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

#ifndef GAP_IN_ALIGNMENT
    /// Representation of GAP in Seq-align
#   define GAP_IN_ALIGNMENT     ((Uint4)-1)
#endif

//////////////////////////////////////////////////////////////////////////////


CCddInputData::CCddInputData(const Uint1* query, unsigned int query_length,
                             CConstRef<objects::CSeq_align_set> seqaligns,
                             const PSIBlastOptions& opts,
                             const string& dbname,
                             const string& matrix_name /* = "BLOSUM62" */,
                             int gap_existence /* = 0 */,
                             int gap_extension /* = 0 */,
                             PSIDiagnosticsRequest* diags /* = NULL */,
                             const string& query_title /* = "" */)
    : m_QueryTitle(query_title),
      m_DbName(dbname),
      m_SeqalignSet(seqaligns),
      m_Msa(NULL),
      m_Opts(opts),
      m_MatrixName(matrix_name),
      m_DiagnosticsRequest(diags),
      m_MinEvalue(-1.0),
      m_GapExistence(gap_existence),
      m_GapExtension(gap_extension)
{
    if (!query) {
        NCBI_THROW(CBlastException, eInvalidArgument, "NULL query");
    }

    if (seqaligns.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "NULL alignments");
    }

    m_QueryData.resize(query_length);
    memcpy(&m_QueryData[0], query, query_length * sizeof(Uint1));
}


CCddInputData::~CCddInputData()
{
    for (unsigned int i=0;i < m_Hits.size();i++) {
        delete m_Hits[i];
    }

    delete [] m_Msa;
}

void CCddInputData::Process(void)
{
    if (m_MinEvalue > m_Opts.inclusion_ethresh) {

        NCBI_THROW(CBlastException, eInvalidOptions,
                   "Minimum RPS-BLAST e-value is larger than the maximum one");
    }

    m_CddData.query = &m_QueryData[0];

    // process primary alignments
    x_ProcessAlignments(m_MinEvalue, m_Opts.inclusion_ethresh);

    // remove overlaping mutliple hits to the same CD
    x_RemoveMultipleCdHits();

    // this is required by PSSM engine code
    m_MsaDimensions.query_length = m_QueryData.size();
    m_MsaDimensions.num_seqs = m_Hits.size();
    m_CddData.dimensions = &m_MsaDimensions;

    x_FillHitsData();
    // this validation has only assertions, no use calling it 
    // for non-debug builds
    _ASSERT(x_ValidateHits());

    x_CreateMsa();
    // the same validation is done on the core level
    _ASSERT(x_ValidateMsa());

    // extract query as Bioseq, needed so that query information can be stored
    // in PssmWithParameters
    x_ExtractQueryForPssm();

    _ASSERT(m_MsaDimensions.query_length == m_QueryData.size());
    _ASSERT(m_MsaDimensions.num_seqs == m_Hits.size());
}


void CCddInputData::x_ProcessAlignments(double min_evalue, double max_evalue)
{
    ITERATE (CSeq_align_set::Tdata, it, m_SeqalignSet->Get()) {
        double evalue;
        if (!(*it)->GetNamedScore(CSeq_align::eScore_EValue, evalue)) {
            NCBI_THROW(CBlastException, eInvalidArgument,
                       "Evalue not found in Seq-align");
        }

        if (evalue >= min_evalue && evalue < max_evalue) {
            m_Hits.push_back(new CHit((*it)->GetSegs().GetDenseg(), evalue));
        }
    }
}


void CCddInputData::x_RemoveMultipleCdHits(void)
{
    // if less than 2 hits, do nothing
    if (m_Hits.size() < 2) {
        return;
    }

    // sort by accession and e-value
    sort(m_Hits.begin(), m_Hits.end(), compare_hits_by_seqid_eval());
    vector<CHit*> new_hits;
    new_hits.reserve(m_Hits.size());

    new_hits.push_back(m_Hits[0]);

    vector<CHit*>::iterator it(m_Hits.begin());
    ++it;
    
    // for each hit
    for (;it != m_Hits.end();++it) {

        // for each kept hit with the same subject accession as it and better
        // e-value
        for (int i=new_hits.size() - 1;i >= 0
                 && (*it)->m_SubjectId->Match(*new_hits[i]->m_SubjectId);i--) {
                
            const CHit* kept_hit = new_hits[i];

            // find intersection between hits on subjects,
            // intersection needs to have query range from kept_hit for 
            // later subtraction
            CHit intersection(*kept_hit);
            intersection.IntersectWith(**it, CHit::eSubject);

            // subtract the subject intersection using query ranges,
            // hits to different ranges of the same CD are treated as
            // different hits
            (*it)->Subtract(intersection);

            if ((*it)->IsEmpty()) {
                delete *it;
                *it = NULL;
                break;
            }
        }
        if (*it) {
            new_hits.push_back(*it);
        }

    }
    m_Hits.swap(new_hits);
}


void CCddInputData::x_FillHitsData(void)
{
    // initialize seqdb
    CSeqDB seqdb(m_DbName, CSeqDB::eProtein);

    // load residue counts from file
    CRef<CBlastRPSInfo> profile_data(
                  new CBlastRPSInfo(m_DbName, CBlastRPSInfo::fDeltaBlast));

    // Set data for each hit
    NON_CONST_ITERATE (vector<CHit*>, it, m_Hits) {

        _ASSERT(*it);

        (*it)->FillData(seqdb, *profile_data);
    }
}


void CCddInputData::x_CreateMsa(void)
{
    const int kQueryLength = m_QueryData.size();
    const int kNumCds = m_Hits.size();

    // initialize msa map
    PSICdMsaCell cell;
    cell.is_aligned = (Uint1)false;
    cell.data = NULL;
    // allocate memory for num cdds + query
    m_MsaData.resize(kQueryLength * (kNumCds), cell);
    m_Msa = new PSICdMsaCell*[kNumCds];
    if (!m_Msa) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory,
                   "Multiple alignment data structure");
    }
    for (int i=0;i < kNumCds;i++) {
        m_Msa[i] = &m_MsaData[i * (int)kQueryLength];
    }

    // fot each hit
    for (size_t hit_idx=0;hit_idx < m_Hits.size();hit_idx++) {

        // for each hit segment
        NON_CONST_ITERATE(vector<CHitSegment*>, it,
                          m_Hits[hit_idx]->GetSegments()) {

            const int kNumQueryColumns
                = (*it)->m_QueryRange.GetTo() - (*it)->m_QueryRange.GetFrom();

            int q_from = (*it)->m_QueryRange.GetFrom();

            // for each position in the hit segemnt
            for (int i=0;i < kNumQueryColumns; i++) {
                // set as aligned and point to data
                m_Msa[hit_idx][q_from + i].is_aligned = (Uint1)true;
                m_Msa[hit_idx][q_from + i].data = &(*it)->m_MsaData[i];
            }
        }
        m_Hits[hit_idx]->m_MsaIdx = hit_idx;
    }

    m_CddData.msa = m_Msa;
}


bool CCddInputData::x_ValidateMsa(void) const
{
    _ASSERT(m_Msa);
    const int kQueryLength = m_QueryData.size();
    const int kNumCds = m_Hits.size();
    const Uint1 kGapChar = AMINOACID_TO_NCBISTDAA[(int)'-'];
    for (int i=0;i < kNumCds;i++) {
        _ASSERT(m_Msa[i]);
    }

    for (int i=0;i < kNumCds;i++) {
        for (int j=0;j < kQueryLength;j++) {

            if (m_QueryData[i] == kGapChar) {
                NCBI_THROW(CBlastException, eInvalidArgument,
                           "Query sequence cannot contain gaps");
            }

            if (m_Msa[i][j].is_aligned) {
                _ASSERT(m_Msa[i][j].data);
                const PSICdMsaCellData* data = m_Msa[i][j].data;

                // some domain models have incomplete data and are supposed to
                // be removed from the database or search results,
                // this exception checks whether one of these domains
                // has slipped in
                if (data->iobsr <= 0.0) {
                    NCBI_THROW(CBlastException, eInvalidArgument,
                               "Zero independent observations in domain model");
                }

                _ASSERT(data->wfreqs);
                double s = 0;
                for (int k=0;k < kAlphabetSize;k++) {
                    if (data->wfreqs[k] < 0.0) {
                        NCBI_THROW(CBlastException, eInvalidArgument,
                                   "Negative residue frequency in a domain "
                                   "model");
                    }
                    s += data->wfreqs[k];
                }
                // some domain models have incomplete data and are supposed to
                // be removed from the database or search results,
                // this exception checks whether one of these domains
                // has slipped in
                if (fabs(s - 1.0) > 1e-5) {
                    NCBI_THROW(CBlastException, eInvalidArgument,
                               "Domain residue frequencies do not sum to 1");
                }
            }
        }
    }

    return true;
}


CCddInputData::CHit::CHit(const CDense_seg& denseg, double evalue)
    : m_Evalue(evalue), m_MsaIdx(-1)
{
    const int kNumDims = denseg.GetDim();
    const int kNumSegments = denseg.GetNumseg();

    _ASSERT(kNumDims == 2);

    m_SubjectId.Reset(denseg.GetIds()[1].GetNonNullPointer());

    const vector<TSignedSeqPos>& starts = denseg.GetStarts();
    const vector<TSeqPos>& lens = denseg.GetLens();

    TSeqPos query_index = 0;
    TSeqPos subject_index = 1;

    for (int seg=0;seg < kNumSegments;seg++) {
        TSeqPos query_offset = starts[query_index];
        TSeqPos subject_offset = starts[subject_index];

        query_index += kNumDims;
        subject_index += kNumDims;

        // segments of gaps in query or subject are ignored
         if (query_offset != GAP_IN_ALIGNMENT
            && subject_offset != GAP_IN_ALIGNMENT) {

            m_SegmentList.push_back(new CHitSegment(
                      TRange(query_offset, query_offset + lens[seg]),
                      TRange(subject_offset, subject_offset
                                     + lens[seg])));

            query_offset += lens[seg];
            subject_offset += lens[seg];
        }        
    }
}


CCddInputData::CHit::CHit(const CHit& hit)
    : m_SubjectId(hit.m_SubjectId),
      m_Evalue(hit.m_Evalue),
      m_MsaIdx(hit.m_MsaIdx)
{
    m_SegmentList.reserve(hit.m_SegmentList.size());
    ITERATE (vector<CHitSegment*>, it, hit.m_SegmentList) {
        m_SegmentList.push_back(new CHitSegment(**it));
    }
}


CCddInputData::CHit::~CHit()
{
    ITERATE (vector<CHitSegment*>, it, m_SegmentList) {
        delete *it;
    }
}


int CCddInputData::CHit::GetLength(void) const
{
    if (IsEmpty()) {
        return 0;
    }

    unsigned int result = 0;
    ITERATE (vector<CHitSegment*>, it, m_SegmentList) {
        result += (*it)->GetLength();
    }

    return result;
}


void CCddInputData::CHit::FillData(const CSeqDB& seqdb,
                                   const CBlastRPSInfo& profile_data)
{
    // get record index of the CD in the database
    int db_oid;
    seqdb.SeqidToOid(*m_SubjectId, db_oid);

    // fill segment data
    NON_CONST_ITERATE(vector<CHitSegment*>, it, m_SegmentList) {
        (*it)->FillData(db_oid, profile_data);
    }
}


bool CCddInputData::x_ValidateHits(void) const
{
    ITERATE (vector<CHit*>, it, m_Hits) {
        _ASSERT(*it);
        (*it)->Validate();
    }
    return true;
}


void CCddInputData::x_ExtractQueryForPssm(void)
{
    // Test our pre-conditions
    _ASSERT(m_QueryData.size() && m_SeqalignSet.NotEmpty());
    _ASSERT(m_QueryBioseq.Empty());

    m_QueryBioseq.Reset(new CBioseq);

    // set the sequence id
    if (!m_SeqalignSet->Get().empty()) {
        CRef<CSeq_align> aln =
            const_cast<CSeq_align_set*>(&*m_SeqalignSet)->Set().front();
        CRef<CSeq_id> query_id(const_cast<CSeq_id*>(&aln->GetSeq_id(0)));
        m_QueryBioseq->SetId().push_back(query_id);
    }
    
    // set required Seq-inst fields
    m_QueryBioseq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
    m_QueryBioseq->SetInst().SetMol(CSeq_inst::eMol_aa);
    m_QueryBioseq->SetInst().SetLength(GetQueryLength());

    // set the sequence data in ncbistdaa format
    CNCBIstdaa& seq = m_QueryBioseq->SetInst().SetSeq_data().SetNcbistdaa();
    seq.Set().reserve(GetQueryLength());
    for (TSeqPos i = 0; i < GetQueryLength(); i++) {
        seq.Set().push_back(m_QueryData[i]);
    }

    if (!m_QueryTitle.empty()) {
        CRef<CSeqdesc> desc(new CSeqdesc());
        desc->SetTitle(m_QueryTitle);
        m_QueryBioseq->SetDescr().Set().push_back(desc);
    }

    // Test our post-condition
    _ASSERT(m_QueryBioseq.NotEmpty());
}


bool CCddInputData::CHit::Validate(void) const
{
    _ASSERT(!m_SubjectId.Empty());

    ITERATE (vector<CHitSegment*>, it, m_SegmentList) {
        _ASSERT(*it);
        (*it)->Validate();
    }

    return true;
}


bool CCddInputData::CHit::IsEmpty(void) const
{
    if (m_SegmentList.empty()) {
        return true;
    }

    ITERATE (vector<CHitSegment*>, it, m_SegmentList) {
        if (!(*it)->IsEmpty()) {
            return false;
        }
    }

    return true;
}


void CCddInputData::CHit::IntersectWith(const vector<TRange>& ranges,
                                        CCddInputData::CHit::EApplyTo app)
{
    // This function assumes that input ranges and hit segments are sorted
    // by range and mutually exclusive

    vector<TRange>::const_iterator r_itr = ranges.begin();
    vector<CHitSegment*>::iterator seg_it = m_SegmentList.begin();
    vector<CHitSegment*> new_segs;
    while (seg_it != m_SegmentList.end() && r_itr != ranges.end()) {

        // get current hit segment range
        const TRange seg_range
            = (app == eSubject ? (*seg_it)->m_SubjectRange
               : (*seg_it)->m_QueryRange);

        // skip all ranges strictly below current hit segment
        while (r_itr != ranges.end() && r_itr->GetTo() < seg_range.GetFrom()) {
            r_itr++;
        }

        if (r_itr == ranges.end()) {
            break;
        }

        // find intersection with current hit segment
        TRange intersection(seg_range.IntersectionWith(*r_itr));

        // if intersection is the same as hit segment, do nothing
        if (intersection == seg_range) {
            seg_it++;
            continue;
        }

        // if intersection is empty, delete current hit segment
        if (intersection.Empty()) {
            delete *seg_it;
            *seg_it = NULL;

            seg_it++;
            continue;
        }
                
        // otherwise find intersections with current hit segment
        // for each range that intersects with current hit segment
        while (r_itr != ranges.end() && r_itr->GetFrom() < seg_range.GetTo()) {

            // get and save intersection
            int d_from = max(seg_range.GetFrom(),
                             r_itr->GetFrom()) - seg_range.GetFrom();
            int d_to = min(seg_range.GetTo(),
                           r_itr->GetTo()) - seg_range.GetTo();

            CHitSegment* new_seg = new CHitSegment(**seg_it);
            new_seg->AdjustRanges(d_from, d_to);
            _ASSERT(!new_seg->IsEmpty());
            new_segs.push_back(new_seg);

            // move to the next range
            r_itr++;
        }

        // current hit segment will be replaced with intersection, hence it
        // is deleted
        delete *seg_it;
        *seg_it = NULL;
        seg_it++;
    }

    // each hit segment behind the last input range will have an empty 
    // interesection hence it is removed
    while (seg_it != m_SegmentList.end()) {
        delete *seg_it;
        *seg_it = NULL;
        seg_it++;
    }

    // remove empty hit segments, add new intersections and sort the list
    ITERATE (vector<CHitSegment*>, it, m_SegmentList) {
        if (*it) {
            new_segs.push_back(*it);
        }
    }
    sort(new_segs.begin(), new_segs.end(), compare_hitseg_range());

    m_SegmentList.swap(new_segs);
}


void CCddInputData::CHit::IntersectWith(const CCddInputData::CHit& hit,
                                        CCddInputData::CHit::EApplyTo app)
{
    vector<TRange> ranges;
    ranges.reserve(hit.GetSegments().size());
    ITERATE (vector<CHitSegment*>, it, hit.GetSegments()) {
        ranges.push_back(app == eQuery ? (*it)->m_QueryRange
                         : (*it)->m_SubjectRange);
    }

    sort(ranges.begin(), ranges.end(), compare_range());

    IntersectWith(ranges, app);
}


void CCddInputData::CHit::Subtract(const CHit& hit)
{
    // if either hit is empty than the result is the same as current
    // object
    if (IsEmpty() || hit.IsEmpty()) {
        return;
    }

    // This function assumes that input ranges and hit segments are sorted
    // by range and mutually exclusive

    // find alignment start and stop of the hit to be subtracted
    int from = hit.GetSegments().front()->m_QueryRange.GetFrom();
    int to = hit.GetSegments().back()->m_QueryRange.GetTo();

    // if there is no overlap between hits, then do nothing
    if (m_SegmentList.front()->m_QueryRange.GetFrom() >= to
        || m_SegmentList.back()->m_QueryRange.GetTo() <= from) {

        return;
    }

    // iterate over segments
    vector<CHitSegment*>::iterator it = m_SegmentList.begin();
        
    vector<CHitSegment*> new_segments;
    new_segments.reserve(m_SegmentList.size());

    // keep all segments that end before the subtracted hits starts
    // unchanged
    while (it != m_SegmentList.end() && (*it)->m_QueryRange.GetTo() <= from) {

        new_segments.push_back(*it);
        ++it;
    }

    // if all segments end before the subctracted hit starts
    // or none of the segments overlaps with the subtracted hit,
    // there is nothing to subtract, exit
    if (it == m_SegmentList.end() || (*it)->m_QueryRange.GetFrom() > to) {
        return;
    }

    // if the current segment covers the whole subtracted hit
    if ((*it)->m_QueryRange.GetTo() > to) {

        // make two segments for what is to the left and right of
        // the subtracted hit
            
        CHitSegment* new_seg;

        if ((*it)->m_QueryRange.GetFrom() < from) {

            new_seg = new CHitSegment(**it);

            // left part
            int d_to = from - (*it)->m_QueryRange.GetTo();
            _ASSERT(d_to < 0);
            (*it)->AdjustRanges(0, d_to);
            _ASSERT((*it)->m_QueryRange.GetFrom() < (*it)->m_QueryRange.GetTo());
            new_segments.push_back(*it);
        }
        else {
            new_seg = *it;
        }

        // right part
        int d_from = to - new_seg->m_QueryRange.GetFrom();
        _ASSERT(d_from >= 0);
        new_seg->AdjustRanges(d_from, 0);
        _ASSERT((*it)->m_QueryRange.GetFrom() < (*it)->m_QueryRange.GetTo());
        new_segments.push_back(new_seg);

        // the following segments do not intersect with subtracted hit
        ++it;
        for (;it != m_SegmentList.end();++it) {
            new_segments.push_back(*it);
        }            
    }
    else {

        // if the segment overlaps completely with the subtracted hit,
        // delete it
        if ((*it)->m_QueryRange.GetFrom() >= from) {
            delete *it;
            *it = NULL;
        }
        else {

            // otherwise adjust segment end
            int d_to = from - (*it)->m_QueryRange.GetTo();
            _ASSERT(d_to < 0);

            (*it)->AdjustRanges(0, d_to);
            _ASSERT((*it)->m_QueryRange.GetFrom() < (*it)->m_QueryRange.GetTo());
            new_segments.push_back(*it);
        }

        // delete all segments that completely overlap with subtracted hit
        ++it;
        while (it != m_SegmentList.end()
               && (*it)->m_QueryRange.GetTo() <= to) {

            delete *it;
            *it = NULL;

            ++it;
        }

        if (it != m_SegmentList.end()) {

            if ((*it)->m_QueryRange.GetFrom() < to) {
                int d_from = to - (*it)->m_QueryRange.GetFrom();
                _ASSERT(d_from > 0);

                (*it)->AdjustRanges(d_from, 0);
                _ASSERT((*it)->m_QueryRange.GetFrom()
                        < (*it)->m_QueryRange.GetTo());

                new_segments.push_back(*it);
            }
            else {
                delete *it;
                *it = NULL;
            }

            // keep all segments above subtracted hit
            ++it;
            while (it != m_SegmentList.end()) {
                new_segments.push_back(*it);
                ++it;
            }
        }
    }

    m_SegmentList.swap(new_segments);
}


void CCddInputData::CHitSegment::FillData(int db_oid,
                                          const CBlastRPSInfo& profile_data)
{
    PSICdMsaCellData d;
    d.wfreqs = NULL;
    d.iobsr = -1.0;
    m_MsaData.resize(m_QueryRange.GetTo() - m_QueryRange.GetFrom(), d);

    x_FillResidueCounts(db_oid, profile_data);
    x_FillObservations(db_oid, profile_data);
}


bool CCddInputData::CHitSegment::Validate(void) const
{
    _ASSERT(m_QueryRange.GetFrom() >= 0 && m_QueryRange.GetTo() >= 0);
    _ASSERT(m_SubjectRange.GetFrom() >= 0 && m_SubjectRange.GetTo() >= 0);

    const int kQueryLength = m_QueryRange.GetTo() - m_QueryRange.GetFrom();
    const int kSubjectLength = m_SubjectRange.GetTo() - m_SubjectRange.GetFrom();

    if (kQueryLength != kSubjectLength) {
        return false;
    }

    _ASSERT((int)m_WFreqsData.size() == kSubjectLength * kAlphabetSize);
    _ASSERT((int)m_MsaData.size() == kSubjectLength);

    ITERATE (vector<PSICdMsaCellData>, it, m_MsaData) {
        _ASSERT(it->wfreqs);
    }

    return true;
}

void CCddInputData::CHitSegment::AdjustRanges(int d_from, int d_to)
{
    m_QueryRange.SetFrom(m_QueryRange.GetFrom() + d_from);
    m_QueryRange.SetTo(m_QueryRange.GetTo() + d_to);

    m_SubjectRange.SetFrom(m_SubjectRange.GetFrom() + d_from);
    m_SubjectRange.SetTo(m_SubjectRange.GetTo() + d_to);
}


bool CCddInputData::CHitSegment::IsEmpty(void) const
{
    return m_QueryRange.GetFrom() > m_QueryRange.GetTo()
        || m_SubjectRange.GetFrom() > m_SubjectRange.GetTo();
}

void CCddInputData::CHitSegment::x_FillResidueCounts(int db_oid,
                                     const CBlastRPSInfo& profile_data)
{
    _ASSERT(profile_data()->freq_header);

    BlastRPSProfileHeader* header = profile_data()->freq_header;
    int num_profiles = header->num_profiles;

    _ASSERT(db_oid < num_profiles);

    // Get weighted residue counts for CD
    const Int4* db_seq_offsets = header->start_offsets;
    const TFreqs* db_counts =
        (TFreqs*)(header->start_offsets + num_profiles + 1);

    // extract residue counts
    const TFreqs* counts = db_counts + db_seq_offsets[db_oid] * kAlphabetSize;
    int db_seq_length = db_seq_offsets[db_oid + 1] - db_seq_offsets[db_oid];

    // correct seq length for column of zero counts in cdd counts file
    db_seq_length--;
    _ASSERT(db_seq_length > 0);
    _ASSERT(m_SubjectRange.GetTo() <= db_seq_length);


    int num_columns = (int)m_MsaData.size();
    m_WFreqsData.resize(num_columns * kAlphabetSize);
    for (int i=0;i < num_columns;i++) {
        m_MsaData[i].wfreqs = &m_WFreqsData[i * kAlphabetSize];

        // column frequencies for a column must sum to 1, but they may not due
        // to storing in CDD as integers, the difference is distributed equally
        // among all the non-zero frequencies
        TFreqs sum_freqs = 0;
        for (int j=0;j < kAlphabetSize;j++) {
            sum_freqs +=
                counts[(m_SubjectRange.GetFrom() + i) * kAlphabetSize + j];
        }

        for (int j=0;j < kAlphabetSize;j++) {
            m_MsaData[i].wfreqs[j] =
                (double)counts[(m_SubjectRange.GetFrom() + i) * kAlphabetSize + j]
                / (double)sum_freqs;
        }
    }
}

void CCddInputData::CHitSegment::x_FillObservations(int db_oid,
                                            const CBlastRPSInfo& profile_data)
{
    // Get effective numbers of independent observations

    _ASSERT(profile_data()->obsr_header);

    BlastRPSProfileHeader* header = profile_data()->obsr_header;
    int num_profiles = header->num_profiles;

    _ASSERT(db_oid < num_profiles);

    // find poiter to eff number of observations
    const Int4* offsets = header->start_offsets;
    const TObsr* data_start
        = (TObsr*)(header->start_offsets + num_profiles + 1);

    const TObsr* data = data_start + offsets[db_oid];
    int data_size = offsets[db_oid + 1] - offsets[db_oid];

    // extract effective numbers of obaservations
    vector<TObsr> obsr;
    for (int i=0;i < data_size;i+=2) {
        TObsr val = data[i];
        Int4 num = (Int4)data[i + 1];
        _ASSERT(fabs((double)num - data[i + 1]) < 1e-05);

        for (int j=0;j < num;j++) {
            obsr.push_back(val);
        }
    }

    int num_columns = m_SubjectRange.GetTo() - m_SubjectRange.GetFrom();
    for (int i=0;i < num_columns;i++) {
        m_MsaData[i].iobsr =
            (double)obsr[m_SubjectRange.GetFrom() + i] / kRpsScaleFactor;
    }
}


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
