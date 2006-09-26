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

#include <set>

BEGIN_NCBI_SCOPE

class CPatternStats;
class CAccPatternCounter : public map<string, CPatternStats*>
{
public:
  void AddName(const string& name);

  typedef set<string> TStrSet;
  void AddNames(const TStrSet& names);

  // Replace "#" in a simple pattern like BCM_Spur_v#.#_Scaffold#
  // with digits or numerical [ranges].
  static string GetExpandedPattern(value_type* p);
  static int GetCount(value_type* p);

  // >pointer to >value_type vector for sorting
  typedef vector<value_type*> pv_vector;
  void GetSortedValues(pv_vector& out);

private:
  // For sorting by accession count
  static int x_byCount( value_type* a, value_type* b );
};

/* How to use:

  // Add accessions
  CAccPatternCounter pc;
  for(...) {
     pc.AddName(...);
  }

  // Print unsorted simple patterns and counts.
  // Runs of digits are replaced with "#".
  for(CAccPatternCounter::iterator it =
    pc->begin(); it != pc->end(); ++it)
  {
    // pattern accession_count
    cout<<  it->first << "\t"
        << CAccPatternCounter::GetCount(it) << "\n";
    );
  }

  // Print expanded patterns and counts; sort by accession count.
  // Runs of digits are replaced with ranges, or kept as is.
  CAccPatternCounter::pv_vector pat_cnt;
  // A vector with sorted pointers to map values
  objNamePatterns->GetSortedValues(pat_cnt);

  for(CAccPatternCounter::pv_vector::iterator it =
      pat_cnt.begin(); it != pat_cnt.end(); ++it
  ) {
    cout<< CAccPatternCounter::GetExpandedPattern(*it) << "\t"
        << CAccPatternCounter::GetCount(*it) << "\n";
  }

*/


END_NCBI_SCOPE

