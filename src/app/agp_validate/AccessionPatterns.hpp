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
 * Authors:
 *      Victor Sapojnikov
 *
 * File Description:
 *     Accession naming patterns are derived from the input accessions
 *     by locating all groups of consecutive digits, and either
 *     replacing these with numerical ranges, or keeping them as is
 *     if this group has the same value for all input accessions.
 *     We also count the number of accesion for each pattern.
 *
 *     Sample input : AC123.1 AC456.1 AC789.1 NC8967.4 NC8967.5
 *                      ^^^ ^                   ^^^^ ^
 *     Sample output: AC[123..789].1 3  NC8967.[4,5] 2
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <map>

BEGIN_NCBI_SCOPE

class CPatternStats;
class CAccPatternCounter : public map<string, CPatternStats*>
{
public:
  void AddName(const string& name); // , int times=1

  // Replace "#" in a simple pattern like BCM_Spur_v#.#_Scaffold#
  // with digits or numerical [ranges].
  static string GetExpandedPattern(value_type* p);
  static int GetCount(value_type* p);

  // Export expanded patterns sorted by accession count.
  typedef multimap<int,string> TMapCountToString;
  void GetSortedPatterns(TMapCountToString& dst);

  ~CAccPatternCounter();
};

/* How to use:

  // Add accessions
  CAccPatternCounter pc;
  for(...) {
     pc.AddName(...);
  }

  // Print simple patterns. sorted alphabetically, and counts.
  // Runs of digits are replaced with "#".
  for(CAccPatternCounter::iterator it =
    pc->begin(); it != pc->end(); ++it)
  {
    // pattern accession_count
    cout<<  it->first << "\t"
        << CAccPatternCounter::GetCount(it) << "\n";
    );
  }

  // Print expanded patterns and counts, most frequent patterns first.
  // Runs of digits are replaced with ranges, or kept as is.
  CAccPatternCounter::TMapCountToString cnt_pat; // multimap<int,string>
  pc.GetSortedPatterns(cnt_pat);

  for(CAccPatternCounter::TMapCountToString::reverse_iterator
      it = cnt_pat.rbegin(); it != cnt_pat.rend(); ++it
  ) {
    // pattern <tab> count
    cout<< it->second << "\t" << it->first  << "\n";
  }

*/


END_NCBI_SCOPE

