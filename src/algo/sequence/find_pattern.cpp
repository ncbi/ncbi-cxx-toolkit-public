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
 * File Description:  Find occurrences of a regular expression in a sequence
 *
 */


#include <ncbi_pch.hpp>
#include <algo/sequence/find_pattern.hpp>
#include <util/xregexp/regexp.hpp>

BEGIN_NCBI_SCOPE


// Find all occurrences of a pattern (regular expression) in a sequence.
void CFindPattern::Find(const string& seq, const string& pattern,
                       vector<TSeqPos>& starts, vector<TSeqPos>& ends) {

    // do a case-insensitive r.e. search for "all" (non-overlapping) occurrences
    // note that it's somewhat ambiguous what this means

    // want to ignore case, and to allow white space (and comments) in pattern
    CRegexp re(pattern, CRegexp::fCompile_ignore_case | CRegexp::fCompile_extended);

    starts.clear();
    ends.clear();
    unsigned int offset = 0;
    while (!re.GetMatch(seq, offset).empty()) {  
        // (empty string means no match)
        const int *res = re.GetResults(0);
        starts.push_back(res[0]);
        ends.push_back(res[1] - 1);
        offset = res[1];
    }
}


/// Find cases of at least min_repeats consecutive occurrences of any
/// *particular* match to pattern.
/// N.B.: pattern = "[ag]c" and min_repeats = 2 will match
/// "acac" and "gcgc" but NOT "acgc" or "gcac".
void CFindPattern::FindRepeatsOf(const string& seq, const string& pattern,
                                 int min_repeats,
                                 vector<TSeqPos>& starts,
                                 vector<TSeqPos>& ends)
{
    string total_pattern;
    total_pattern = "(" + pattern + ")\\1{"
        + NStr::IntToString(min_repeats - 1) + ",}";
    Find(seq, total_pattern, starts, ends);
}


/// Find all cases of at least min_repeats consecutive occurrences
/// of any n-mer consisting of unambiguous nucleotides ({a, g, c, t}).
/// Note that, e.g., dinucelotide repeats can also qualify as
/// tetranucleotide repeats.
void CFindPattern::FindNucNmerRepeats(const string& seq,
                                      int n, int min_repeats,
                                      vector<TSeqPos>& starts,
                                      vector<TSeqPos>& ends)
{
    string pattern;
    for (int i = 0;  i < n;  ++i) {
        pattern += "[agct]";
    }
    FindRepeatsOf(seq, pattern, min_repeats, starts, ends);
}


END_NCBI_SCOPE
