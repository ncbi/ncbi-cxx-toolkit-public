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

#include <ncbi_pch.hpp>
#include "AccessionPatterns.hpp"
#include <corelib/ncbi_limits.hpp>

#include <algorithm>

USING_NCBI_SCOPE;

BEGIN_NCBI_SCOPE

//// Internal classes for CAccPatternCounter

// Stores minimal and maximal values for a run of up to 15 digits.
// A more precise name might be be "CRunOfDigitsStatistics"
// We preserve leading zeroes by storing min/max values as strings.
// We also count how many times min and max values occur.
class CRunOfDigits
{
public:
  double min_val, max_val;
  string min_str, max_str;
  int min_val_count, max_val_count, total_count;

  // If these lengths are not equal, we would NOT move zeroes out of [], like
  //   AC[00001..0085] => AC00[01..85]
  int min_len, max_len;

  CRunOfDigits()
  {
    min_val=kMax_Double; max_val=0;
    min_val_count=max_val_count=total_count=0;
    min_len=100; max_len=0;
  }

  void AddString(const string& s)
  {
    total_count++;

    double d=NStr::StringToDouble(s);
    if(d<min_val) {
      min_val=d;
      min_str=s;
      min_val_count=1;
    }
    else if(d==min_val) {
      min_val_count++;
    }

    if(d>max_val) {
      max_val=d;
      max_str=s;
      max_val_count=1;
    }
    else if(d==max_val) {
      max_val_count++;
    }

    int i=s.size();
    if(i<min_len) min_len=1;
    if(i>max_len) max_len=i;
  }

  // Returns a string represeting the range of values in this run of digits.
  // Possible formats:
  //   [min_val..max_val]
  //   min_val            when there was just one value in this run;
  //
  //   [min_val,max_val]  when there were exactly 2 values in this run;
  //   prefix[from..to]   | when min_val and max_val start with a common prefix
  //   prefix[from,to]    | and all submitted strings have the same length
  string GetString() const
  {
    if(min_val==max_val) return min_str;
    int prefix_len=0;
    if(min_len==max_len) {
      // Can optimize: // [0001..0085] -> 00[01..85]
      while( prefix_len<min_len &&
        min_str[prefix_len] == max_str[prefix_len]
      ) prefix_len++;
    }

    string sep=".."; // A range
    if(min_val_count+max_val_count==total_count) {
      sep=","; // Just 2 values
    }
    return min_str.substr(0,prefix_len) + "[" +
      min_str.substr(prefix_len) + sep +
      max_str.substr(prefix_len) + "]";
  }
};

typedef vector<CRunOfDigits> TRunsOfDigits;
class CPatternStats
{
public:
  typedef vector<string> TStrVec;

  int acc_count;
  TRunsOfDigits* runs;

  // runs_count is the number of continuous digit runs,
  // e.g. 2 for "AC01234.5". runs_count is the same
  // for all accessions of the same pattern.
  CPatternStats(int runs_count)
  {
    acc_count=0;
    runs = new TRunsOfDigits(runs_count);
  }
  ~CPatternStats()
  {
    delete runs;
  }

  void AddAccRuns(TStrVec& runs_str)
  {
    acc_count++;
    for( SIZE_TYPE i=0; i<runs_str.size(); i++ ) {
      (*runs)[i].AddString(runs_str[i]);
    }
  }

  // Replace "#" in a simple pattern like BCM_Spur_v#.#_Scaffold#
  // with digits or numerical [ranges].
  string ExpandPattern(const string& pattern) const
  {
    int i = 0;
    SIZE_TYPE pos = 0;
    string res = pattern;

    for(;;) {
      pos = NStr::Find(res, "#", pos);
      if(pos==NPOS) break;

      res.replace( pos, 1, (*runs)[i].GetString() );
      i++;
    }
    return res;
  }
};


//// class CAccPatternCounter --------------------------
void CAccPatternCounter::AddName(const string& name)
{
  string s;
  s.reserve(name.size());

  // Replace runs of digits with # -> s;
  // collect those runs in digrun.
  bool prev_digit=false;
  vector<string> digrun;
  for(SIZE_TYPE i=0; i<name.size(); i++) {
    if( isdigit(name[i]) ) {
      if(!prev_digit) {
        s+="#";
        prev_digit=true;
        digrun.push_back(NcbiEmptyString);
      }
      digrun.back() += name[i];
    }
    else{
      prev_digit=false;
      s+=name[i];
    }
  }

  // Using the "#pattern" (s) as a key in this map,
  // add digrun values to the statistics.
  iterator it = find(s);
  CPatternStats* ps;
  if(it==end()) {
    ps = new CPatternStats( digrun.size() );
    insert( value_type(s, ps) );
  }
  else {
    ps = it->second;
  }

  ps->AddAccRuns(digrun);
}

// Replace "#" in a simple pattern like BCM_Spur_v#.#_Scaffold#
// with digits or numerical [ranges].
// static
string CAccPatternCounter::GetExpandedPattern(value_type* p)
{
  return p->second->ExpandPattern(p->first);
}

// static
int CAccPatternCounter::GetCount(value_type* p)
{
  return p->second->acc_count;
}

void CAccPatternCounter::GetSortedPatterns(CAccPatternCounter::TMapCountToString& dst)
{
  for(iterator it = begin();  it != end(); ++it) {
    dst.insert(TMapCountToString::value_type(
      GetCount(&*it),
      GetExpandedPattern(&*it)
    ));
  }
}

CAccPatternCounter::~CAccPatternCounter()
{
  for(iterator it = begin();  it != end(); ++it) {
    delete it->second;
  }
}
END_NCBI_SCOPE

