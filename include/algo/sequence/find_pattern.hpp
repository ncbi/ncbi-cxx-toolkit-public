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

#ifndef ALGO_SEQUENCE___FIND_PATTERN__HPP
#define ALGO_SEQUENCE___FIND_PATTERN__HPP

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE


class NCBI_XALGOSEQ_EXPORT CFindPattern {
public:
    /// Find non-overlapping matches of regular expression in sequence.
    static void Find(const string& seq, const string& pattern,
                     vector<TSeqPos>& starts, vector<TSeqPos>& ends);
    /// Find cases of at least min_repeats consecutive occurrences of any
    /// *particular* match to pattern.
    /// N.B.: pattern = "[ag]c" and min_repeats = 2 will match
    /// "acac" and "gcgc" but NOT "acgc" or "gcac".
    static void FindRepeatsOf(const string& seq, const string& pattern,
                              int min_repeats,
                              vector<TSeqPos>& starts, vector<TSeqPos>& ends);
    /// Find all cases of at least min_repeats consecutive occurrences
    /// of any n-mer consisting of unambiguous nucleotides ({a, g, c, t}).
    /// Note that, e.g., dinucelotide repeats can also qualify as
    /// tetranucleotide repeats.
    static void FindNucNmerRepeats(const string& seq,
                                   int n, int min_repeats,
                                   vector<TSeqPos>& starts,
                                   vector<TSeqPos>& ends);
};

END_NCBI_SCOPE

#endif   // ALGO_SEQUENCE___FIND_PATTERN__HPP

/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2004/04/01 14:14:01  lavr
 * Spell "occurred", "occurrence", and "occurring"
 *
 * Revision 1.10  2003/12/16 20:10:16  jcherry
 * Added export specifier
 *
 * Revision 1.9  2003/12/16 18:02:21  jcherry
 * Moved find_pattern to algo/sequence
 *
 * Revision 1.8  2003/12/15 21:20:02  jcherry
 * Added simple repeat searches
 *
 * Revision 1.7  2003/12/15 20:16:09  jcherry
 * Changed CFindPattern::Find to take a string rather than a CSeqVector
 *
 * Revision 1.6  2003/12/15 19:51:07  jcherry
 * CRegexp::GetMatch now takes a string&, not a char*
 *
 * Revision 1.5  2003/11/04 17:49:23  dicuccio
 * Changed calling parameters for plugins - pass CPluginMessage instead of paired
 * CPluginCommand/CPluginReply
 *
 * Revision 1.4  2003/08/04 20:07:13  jcherry
 * Added standard #ifndef wrapper
 *
 * Revision 1.3  2003/07/03 19:14:12  jcherry
 * Initial version
 *
 * Revision 1.1  2003/07/03 19:06:39  jcherry
 * Initial version
 *
 * ===========================================================================
 */
