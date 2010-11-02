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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <util/range.hpp>

#include <algo/align/util/genomic_compart.hpp>
#include <objects/seq/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

bool IsIntersecting(const pair<TSeqRange, TSeqRange>& r1,
                    const pair<TSeqRange, TSeqRange>& r2)
{
    return (r1.first.IntersectingWith(r2.first)  ||
            r1.second.IntersectingWith(r2.second));
}

bool IsConsistent(const pair<TSeqRange, TSeqRange>& r1,
                  const pair<TSeqRange, TSeqRange>& r2,
                  ENa_strand s1, ENa_strand s2)
{
    if (s1 == s2) {
        return (r1.first < r2.first  &&  r1.second < r2.second)  ||
               (r1.first > r2.first  &&  r1.second > r2.second);
    }
    else {
        return (r1.first < r2.first  &&  r1.second > r2.second)  ||
               (r1.first > r2.first  &&  r1.second < r2.second);
    }
}


void FindCompartments(const list< CRef<CSeq_align> >& aligns,
                      list< CRef<CSeq_align_set> >& align_sets,
                      TCompartOptions options)
{
    //
    // sort by sequence pair + strand
    //
    typedef pair<CSeq_id_Handle, ENa_strand> TIdStrand;
    typedef pair<TIdStrand, TIdStrand> TIdPair;
    typedef map<TIdPair, list< CRef<CSeq_align> > > TAlignments;
    TAlignments alignments;
    ITERATE (list< CRef<CSeq_align> >, it, aligns) {
        CSeq_id_Handle qid = CSeq_id_Handle::GetHandle((*it)->GetSeq_id(0));
        ENa_strand q_strand = (*it)->GetSeqStrand(0);
        CSeq_id_Handle sid = CSeq_id_Handle::GetHandle((*it)->GetSeq_id(1));
        ENa_strand s_strand = (*it)->GetSeqStrand(1);
        TIdPair p(TIdStrand(qid, q_strand), TIdStrand(sid, s_strand));
        alignments[p].push_back(*it);
    }


    typedef pair<TSeqPos, CRef<CSeq_align_set> > TCompartScore;
    vector<TCompartScore> scored_compartments;


    // we only compartmentalize within each sequence id / strand pair
    ITERATE (TAlignments, align_it, alignments) {
        const TIdPair& id_pair = align_it->first;
        ENa_strand q_strand = id_pair.first.second;
        ENa_strand s_strand = id_pair.second.second;

        const list< CRef<CSeq_align> >& aligns = align_it->second;

        //
        // reduce the list to a sorted set of overall ranges
        //
        typedef pair<TSeqRange, TSeqRange> TRange;
        typedef pair<TRange, CRef<CSeq_align> > TAlignRange;
        vector<TAlignRange> align_ranges;

        ITERATE (list< CRef<CSeq_align> >, iter, aligns) {
            TSeqRange q_range = (*iter)->GetSeqRange(0);
            TSeqRange s_range = (*iter)->GetSeqRange(1);
            TRange r(q_range, s_range);
            align_ranges.push_back(TAlignRange(r, *iter));
        }

        std::sort(align_ranges.begin(), align_ranges.end());

        /**
        cerr << "ranges: "
            << id_pair.first.first << "/" << q_strand
            << " x "
            << id_pair.second.first << "/" << s_strand
            << ":"<< endl;
        ITERATE (vector<TAlignRange>, it, align_ranges) {
            cerr << "  ("
                << it->first.first << ", "
                << it->first.second << ")"
                << endl;
        }
        **/

        //
        // compart should be a straight iteration now
        //
        typedef set<TRange> TRangeCollection;
        typedef pair<TRangeCollection, list< CRef<CSeq_align> > > TRanges;
        list<TRanges> compartments;
        ITERATE (vector<TAlignRange>, it, align_ranges) {
            list<TRanges>::iterator best = compartments.end();
            NON_CONST_ITERATE (list<TRanges>, i, compartments) {
                // everything is sorted - we now only need to check the last
                // element!
                if ((options & fCompart_AllowIntersections) == 0) {
                    if (IsIntersecting(*i->first.rbegin(), it->first)) {
                        continue;
                    }
                }
                if (IsConsistent(*i->first.rbegin(), it->first,
                                 q_strand, s_strand)) {
                    best = i;
                    break;
                }
            }

            if (best == compartments.end()) {
                TRanges ranges;
                ranges.first.insert(it->first);
                ranges.second.push_back(it->second);
                compartments.push_back(ranges);
            }
            else {
                best->first.insert(it->first);
                best->second.push_back(it->second);
            }
        }

        /**
        LOG_POST(Error
                 << align_it->first.first.first << " x "
                 << align_it->first.second.first << ": "
                 << "found " << compartments.size() << " compartments");
                 **/

        ITERATE (list<TRanges>, it, compartments) {
            /**
            cerr << "  [";
            ITERATE (TRangeCollection, i, it->first) {
                cerr << " ("
                    << i->first.GetFrom() << "-" << i->first.GetTo()
                    << ", "
                    << i->second.GetFrom() << "-" << i->second.GetTo()
                    << ")";
            }
            cerr << " ]: ";
            cerr << it->second.size() << " alignments" << endl;
            **/

            CRef<CSeq_align_set> sas(new CSeq_align_set);
            sas->Set() = it->second;

            TSeqPos sum = 0;
            ITERATE (CSeq_align_set::Tdata, it, sas->Get()) {
                sum += (*it)->GetAlignLength();
            }

            TCompartScore sc(sum, sas);
            scored_compartments.push_back(sc);

        }
    }

    //
    // sort our compartments by size descending
    //
    std::sort(scored_compartments.begin(), scored_compartments.end());
    REVERSE_ITERATE (vector<TCompartScore>, it, scored_compartments) {
        align_sets.push_back(it->second);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

