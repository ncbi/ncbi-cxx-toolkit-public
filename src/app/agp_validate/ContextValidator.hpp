#ifndef AGP_VALIDATE_ContextValidator
#define AGP_VALIDATE_ContextValidator

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
 *      AGP context-sensetive validation (i.e. information from several lines).
 *
 */

#include "AgpErrEx.hpp"

// #include "LineValidator.hpp"
#include <set>

BEGIN_NCBI_SCOPE

 // Determines accession naming patterns, counts accessions.
class CAccPatternCounter;

// Count how many times a atring value appears;
// report values and counts ordered by count.
class CValuesCount : public map<string, int>
{
public:
  void add(const string& c);

  // >pointer to >value_type vector for sorting
  typedef vector<value_type*> TValPtrVec;
  void GetSortedValues(TValPtrVec& out);

private:
  // For sorting by value count
  static int x_byCount( value_type* a, value_type* b );
};

extern CAgpErrEx agpErr;
class CCompVal
{
public:
  int beg, end;
  char ori;
  int file_num, line_num;

  enum { ORI_plus, ORI_minus, ORI_zero, ORI_na,  ORI_count };
  // static TValuesMap validOri;
  // CCompVal(); no validation needed

  void init(const CAgpRow& row, int line_num_arg)
  {
    beg=row.component_beg;
    end=row.component_end;
    ori=row.orientation;

    line_num=line_num_arg;
    file_num=agpErr.GetFileNum();
  }
  int getLen() const { return end - beg + 1; }

  string ToString() const
  {
    string s;
    s += NStr::IntToString(beg);
    s += "..";
    s += NStr::IntToString(end);
    s += " at ";
    if(file_num) {
      s += agpErr.GetFile(file_num);
      s += ":";
    }
    else {
      s += "line ";
    }
    s += NStr::IntToString(line_num);
    return s;
  }
};

// To save memory, this is a vector instead of a map.
// Multiple spans on one component are uncommon.
class CCompSpans : public vector<CCompVal>
{
public:
  // Construct a vector with one element
  CCompSpans(const CCompVal& src)
  {
    push_back(src);
  }

  // Returns the first overlapping span and CAgpErr::W_SpansOverlap,
  // or the first span out of order and CAgpErr::W_SpansOrder,
  // or begin() and CAgpErr::W_DuplicateComp.
  // The caller can ignore the last 2 warnings for draft seqs.
  typedef pair<iterator, int> TCheckSpan;
  TCheckSpan CheckSpan(int span_beg, int span_end, bool isPlus);
  void AddSpan(const CCompVal& span); // CCompSpans::iterator it,

};


class CAgpContextValidator
{
public:
  CAgpContextValidator();
  //void ValidateLine(const SDataLine& dl, const CAgpLine& cl);
  void ValidateRow(CAgpRow& row, int line_num);

  void EndOfObject(bool afterLastLine=false);

  // Adjust the context after an invalid line
  // (to suppress some spurious errors)
  void InvalidLine();


  void PrintTotals();

protected:
  // Assigned in ValidateRow(), used in x_OnGapRow() x_OnCompRow()
  bool new_obj;

  // Private subroutines for ValidateRow()
  void x_OnGapRow (CAgpRow& row);
  void x_OnCompRow(CAgpRow& row, int line_num);

  /*
  // Private subroutines for ValidateLine()
  void x_OnGapLine(const SDataLine& dl, const CGapVal& gap);
  void x_OnCompLine(const SDataLine& dl, const CCompVal& comp);
  */

  int m_ObjCount;
  int m_ScaffoldCount;
  int m_SingleCompScaffolds;
  int m_SingleCompObjects;
  int m_CompCount;
  int m_GapCount;
  //int m_CompOri[CCompVal::ORI_count];
  int m_CompOri[4];

  //int m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_yes_count];
  int m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapYes_count];
  // Count component types and N/U gap types.
  CValuesCount m_TypeCompCnt; // column 5: A, D, F, ..., N, U

  // keep track of the object ids to detect duplicates.
  typedef set<string> TObjSet;
  typedef pair<TObjSet::iterator, bool> TObjSetResult;
  TObjSet m_ObjIdSet;

  // keep track of the component and object ids used
  //  in the AGP. Used to detect duplicates and
  //  duplicates with seq range intersections.
  typedef map<string, CCompSpans> TCompId2Spans;
  typedef pair<string, CCompSpans> TCompIdSpansPair;
  TCompId2Spans m_CompId2Spans;

  string prev_object;
  //CAgpRow::EGap
  int prev_line_gap_type;

  // Used for the first component in a scaffold.
  // We do not know right away whether the scaffold is a singleton,
  // and unknown orientation is not an error for singletons.
  bool prev_orientation_unknown;

   // End of previous element in object coords (column 3).
   // Used for checking for intersection.
  int  prev_end;
  int  prev_part_num;
  int  componentsInLastScaffold;
  int  componentsInLastObject;

  static void x_PrintPatterns(CAccPatternCounter& namePatterns, const string& strHeader);

};

END_NCBI_SCOPE

#endif /* AGP_VALIDATE_ContextValidator */

