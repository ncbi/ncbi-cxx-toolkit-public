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
 * Authors:  Josh Cherry
 *
 * File Description:  code for finding and representing ORFs
 *
 */


#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include "seq_match.hpp"

BEGIN_NCBI_SCOPE

/// This class provides functions for finding all the ORFs
/// of a specified minimum length in a DNA sequence,
/// and a way of representating and ORF.

class COrf
{
public:
    COrf(TSeqPos from, TSeqPos to, ENa_strand strand) :
        m_from(from), m_to(to), m_strand(strand) {}

    /// Member access functions.
    TSeqPos GetFrom(void) const {return m_from;}
    TSeqPos GetTo(void) const {return m_to;}
    ENa_strand GetStrand(void) const {return m_strand;}

    /// Find all ORFs in both orientations that
    /// are at least min_length_bp long.
    /// Report results as COrf objects.
    /// seq must be in ncbi8na
    template<class Seq>
    static void FindOrfs(const Seq& seq, vector<COrf>& results,
                         unsigned int min_length_bp = 3)
    {
        vector<TSeqPos> begins, ends;

        // find ORFs on the forward sequence and report them as-is
        FindForwardOrfs(seq, begins, ends, min_length_bp);
        for (unsigned int i = 0;  i < begins.size();  i++) {
            COrf orf(begins[i], ends[i], eNa_strand_plus);
            results.push_back(orf);
        }

        // find ORFs on the complement and munge the numbers
        begins.clear();
        ends.clear();
        Seq comp(seq);
        CSeqMatch::CompNcbi8na(comp);
        FindForwardOrfs(comp, begins, ends, min_length_bp);
        for (unsigned int i = 0;  i < begins.size();  i++) {
            COrf orf(comp.length() - ends[i] - 1,
                     comp.length() - begins[i] - 1,
                     eNa_strand_minus);
            results.push_back(orf);
        }
    }

    /// Find all ORFs in forward orientation with
    /// length in *base pairs* >= min_length_bp.
    /// seq must be in ncbi8na.
    template<class Seq>
    static void FindForwardOrfs(const Seq& seq, vector<TSeqPos>& begins,
                                vector<TSeqPos>& ends,
                                unsigned int min_length_bp = 3)
    {
        string scodons1, scodons2;
        CSeqMatch::IupacToNcbi8na("TAR", scodons1);  // uaa and uag
        CSeqMatch::IupacToNcbi8na("TRA", scodons2);  // uaa and uga
    
        // find only those positions that definitely match a stop
        vector<TSeqPos> stops1, stops2, dummy, all_stops;
        CSeqMatch::FindMatchesNcbi8na(seq, scodons1, stops1, dummy);
        CSeqMatch::FindMatchesNcbi8na(seq, scodons2, stops2, dummy);
        set_union(stops1.begin(), stops1.end(),
                  stops2.begin(), stops2.end(),
                  back_inserter(all_stops));

        // now sort them out by reading frame
        vector<vector<TSeqPos> > stops;
        stops.resize(3);
        for (unsigned int i = 0;  i < all_stops.size();  i++) {
            stops[all_stops[i] % 3].push_back(all_stops[i]);
        }

        TSeqPos from, to;
        // for each reading frame, calculate the orfs
        for (int frame = 0;  frame < 3;  frame++) {

            if (stops[frame].empty()) {
                // no stops in this frame; the whole sequence,
                // minus some scraps at the ends, is one ORF
                from = frame;
                // 'to' should be the largest index within the
                // sequence length that gives an ORF length
                // divisible by 3
                to = ((seq.size() - from) / 3) * 3 + from - 1;
                if (to - from + 1 >= min_length_bp) {
                    begins.push_back(from);
                    ends.push_back(to);
                }
                continue;  // we're done for this reading frame
            }
        
            // deal specially with first ORF
            from = frame;
            to = stops[frame].front() - 1;
            if (to - from + 1 >= min_length_bp) {
                begins.push_back(from);
                ends.push_back(to);
            }

            for (unsigned int i = 0;  i < stops[frame].size() - 1;  i++) {
                from = stops[frame][i] + 3;
                to = stops[frame][i + 1] - 1;
                if (to - from + 1 >= min_length_bp) {
                    begins.push_back(from);
                    ends.push_back(to);
                }
            }
        
            // deal specially with last ORF
            from = stops[frame].back() + 3;
            // 'to' should be the largest index within the
            // sequence length that gives an orf length
            // divisible by 3
            to = ((seq.size() - from) / 3) * 3 + from - 1;
            if (to - from + 1 >= min_length_bp) {
                begins.push_back(from);
                ends.push_back(to);
            }
        }
    }
private:
    TSeqPos m_from;
    TSeqPos m_to;
    ENa_strand m_strand;
};




END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/08/15 12:17:38  dicuccio
 * COmpilation fixes for MSVC
 *
 * Revision 1.2  2003/08/14 21:18:20  ucko
 * Don't repeat default argument specifications in definitions.
 *
 * Revision 1.1  2003/08/14 17:59:22  jcherry
 * Initial version
 *
 * ===========================================================================
 */
