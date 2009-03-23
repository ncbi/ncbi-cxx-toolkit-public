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
 * Author: Christiam Camacho
 *
 */

/** @file neutral_seqalign.hpp
 * Neutral representation of a sequence alignment produced by BLAST
 */

#ifndef _NEUTRAL_SEQALIGN_HPP_
#define _NEUTRAL_SEQALIGN_HPP_

#include <vector>
#include <list>
#include <util/range.hpp>
#include <corelib/ncbimisc.hpp>
#include <objects/seqloc/Na_strand.hpp>

BEGIN_SCOPE(ncbi)
BEGIN_SCOPE(blast)
BEGIN_SCOPE(qa)

typedef ncbi::objects::ENa_strand TStrand;

struct SeqLoc {
    size_t id;
    TSeqRange range;
    TStrand strand;
};

typedef std::list<SeqLoc> TSeqLocList;

const int kInvalidIntValue = -1;
const double kInvalidDoubleValue = -1.0;

/// Class to contain the gis of the aligned sequences
class CAlignedGis {
public:

    enum { kUnassigned = -1 };

    CAlignedGis() 
        : m_Gis(make_pair<int, int>(kUnassigned, kUnassigned)) {}

    CAlignedGis(int query_gi, int subj_gi) 
        : m_Gis(make_pair(query_gi, subj_gi)) {}

    CAlignedGis(std::pair<int, int> gis)
        : m_Gis(gis) {}

    int GetQuery() const { return m_Gis.first; }
    int GetSubject() const { return m_Gis.second; }
    void SetQuery(int gi) { m_Gis.first = gi; }
    void SetSubject(int gi) { m_Gis.second = gi; }

private:
    std::pair<int, int> m_Gis;
};

/// Neutral sequence alignment (for representing an HSP in BLAST)
struct SeqAlign {

    /// Number of dimensions expected in the alignments
    enum { kNumDimensions = 2 };

    int score;                      ///< HSP score
    int num_ident;                  ///< Number of identical residues
    double evalue;                  ///< HSP evalue
    double bit_score;               ///< HSP bit score
    std::vector<int> starts;        ///< Query/Subject starting offsets
    std::vector<TSeqPos> lengths;   ///< Lengths of aligned segments
    int query_strand;               ///< Strand of the query sequence
    int subject_strand;             ///< Strand of the subject sequence
    CAlignedGis sequence_gis;       ///< Gis of the aligned sequences

    TSeqLocList loc_list;

    /// Default constructor, initializes all fields to kInvalid*Value or empty
    SeqAlign()
    : score         (kInvalidIntValue),
      num_ident     (kInvalidIntValue),
      evalue        (kInvalidDoubleValue),
      bit_score     (kInvalidDoubleValue),
      starts        (0),
      lengths       (0),
      query_strand  (0),
      subject_strand(0),
      sequence_gis  (),
      loc_list      (0)
    {}

    /// Return the number of segments in the HSP
    int GetNumSegments() const;
};

/// Vector of neutral sequence alignments
typedef std::vector<SeqAlign> TSeqAlignSet;

enum EScoreType {
    eScore_Generic,
    eScore_Evalue,
    eScore_BitScore,
    eScore_NumIdent,
    eScore_Ignore
};


inline int
SeqAlign::GetNumSegments() const
{
    return lengths.size();
}

END_SCOPE(qa)
END_SCOPE(blast)
END_SCOPE(ncbi)

#endif /* _NEUTRAL_SEQALIGN_HPP_ */
