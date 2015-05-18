#ifndef REGEXP__LOC__HPP
#define REGEXP__LOC__HPP

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
 * Authors:  Clifford Clausen
 *
 */

/// @file regexp_loc.hpp
/// header file for creating CSeq_locs from CRegexps.
///
/// Class definition file for CRegexp_loc which is used to convert a PCRE
/// match to a char* sequence into a CSeq_loc.


#include <corelib/ncbistd.hpp>
#include <util/xregexp/regexp.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <memory>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// Class used to convert a PCRE match to a char* sequence into a CSeq_loc.

class NCBI_XALGOSEQ_EXPORT CRegexp_loc
{
public:
    /// Constructor for CRegexp_loc

    /// Compiles the Perl Compatible Regular Expression (PCRE) pat, and sets
    /// compiled pattern options. See CRegexp.hpp for more information.
    CRegexp_loc(const string &pat,            // Perl regular expression
                CRegexp::TCompile flags = 0); // Compile options
    virtual  ~CRegexp_loc();

    /// Sets PCRE pattern

    /// Sets and compiles Perl Compatible Regular Expression (PCRE) pat, and
    /// sets compiled pattern options. See CRegexp.hpp for more information.
    void Set(const string &pat,             // Perl regular expression
             CRegexp::TCompile flags = 0);  // Compile options

    ///Gets a CSeq_loc for PCRE match to char* sequence

    /// Gets a CSeq_loc (loc) of match between currently set regular
    /// expression and seq. Returned loc is of type CPacked_seqint. The first
    /// CSeq_interval in the CPacked_seqint is the overall match. Subsequent
    /// CSeq_intervals are matches to sub-patterns. Begins search of seq at 0
    /// based offset. Returns 0 based position of first character in seq of
    /// match.  If no match found, returns kInvalidSeqPos and loc is set to
    /// an empty packed-int type CSeq_loc. See CRegexp.hpp for information
    /// about flags.
    TSeqPos GetLoc (const char *seq,            // Sequence to search
                    CSeq_loc *loc,              // Pattern location(s)
                    TSeqPos offset = 0,         // Starting offset in sequence
                    CRegexp::TMatch flags = 0); // Match options

private:
    // Disable copy constructor and assignment operator
    CRegexp_loc(const CRegexp_loc &);
    void operator= (const CRegexp_loc &);

    /// PCRE used to match against char* sequence passed as argument to GetLoc
    auto_ptr<CRegexp> m_regexp;
};


END_NCBI_SCOPE

#endif /*REGEXP__LOC__HPP*/
