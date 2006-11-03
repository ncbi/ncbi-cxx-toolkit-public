#ifndef AGP_VALIDATE_SyntaxValidator
#define AGP_VALIDATE_SyntaxValidator

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
 *      Lou Friedman, Victor Sapojnikov
 *
 * File Description:
 *      Syntactic validation tests that can be preformed solely by the
 *      information that in the AGP file.
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistr.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <util/range_coll.hpp>

#include <iostream>
#include "AgpErr.hpp"

#define NO_LOG false

BEGIN_NCBI_SCOPE

extern CAgpErr agpErr;

struct SDataLine {
  int     line_num;
  string  object;
  string  begin;
  string  end;
  string  part_num;
  string  component_type;
  string  component_id;
  string  component_beg;
  string  component_end;
  string  orientation;
  string  gap_length;
  string  gap_type;
  string  linkage;
};

 // Determines accession naming patterns, counts accessions.
class CAccPatternCounter;

// Count how many times a atring value appears;
// report values and counts ordered by count.
class CValuesCount : public map<string, int>
{
public:
  void add(const string& c);

  // >pointer to >value_type vector for sorting
  typedef vector<value_type*> pv_vector;
  void GetSortedValues(pv_vector& out);

private:
  // For sorting by value count
  static int x_byCount( value_type* a, value_type* b );
};

// proper values for the different fields in the AGP
typedef set<string> TValuesSet;
// Compared to TValuesSet. provides code optimization by avoiding
// repetetive string comparisons.
typedef map<string, int> TValuesMap;

// Values for Validatable component columns (7a-9a)
// plus file number and line number.
// Use init() for simple syntax validation.
class CCompVal
{
public:
  int beg, end, ori, file_num, line_num;

  enum {
    ORI_plus,
    ORI_minus,
    ORI_zero,
    ORI_na,

    ORI_count
  };
  static TValuesMap validOri;

  CCompVal();

  bool init(const SDataLine& dl, bool log_errors = true);
  int getLen() const { return end - beg + 1; }

  string ToString() const;
};

class CGapVal
{
public:
  int len, type, linkage;

  enum {
    GAP_clone          ,
    GAP_fragment       ,
    GAP_repeat         ,

    GAP_contig         ,
    GAP_centromere     ,
    GAP_short_arm      ,
    GAP_heterochromatin,
    GAP_telomere       ,

    //GAP_scaffold       ,
    //GAP_split_finished ,

    GAP_count,
    GAP_yes_count=GAP_repeat+1
  };

  typedef const char* TStr;
  static const TStr typeIntToStr[GAP_count+GAP_yes_count];

  // Initialized on the first invokation of the constructor from typeIntToStr[]
  static TValuesMap typeStrToInt;

  enum {
    LINKAGE_no ,
    LINKAGE_yes ,
    LINKAGE_count
  };
  // Initialized on the first invokation of the constructor
  static TValuesMap validLinkage;

  CGapVal();

  bool init(const SDataLine& dl, bool log_errors = true);
  int getLen() const { return len; }
  bool endsScaffold() const;
};

// To save memoty, this is a vector instead of a map.
// Multiple spans on one component are uncommon.
class CCompSpans : public vector<CCompVal>
{
public:
  // Construct a vector with one element
  CCompSpans(const CCompVal& src)
  {
    push_back(src);
    //beg = src.beg;
    //end = src.end;
  }

  // Returns the first overlapping span and CAgpErr::W_SpansOverlap,
  // or the first span out of order and CAgpErr::W_SpansOrder,
  // or back() and CAgpErr::W_DuplicateComp.
  // The caller can ignore the last 2 warnings for draft seqs.
  typedef pair<iterator, CAgpErr::TCode> TCheckSpan;
  TCheckSpan CheckSpan(int span_beg, int span_end, bool isPlus);
  void AddSpan(const CCompVal& span); // CCompSpans::iterator it,

  // int GetTo  () { return end; }
  // int GetFrom() { return beg; }

};

int x_CheckIntField(const string& field,
  const string& field_name, bool log_error = true);
bool x_CheckValues(const TValuesSet& values,
  const string& value, const string& field_name,
  bool log_error = true);
// Returns an integer constant mapped to the allowed text value,
// -1 if the value is unknowm
int x_CheckValues(const TValuesMap& values,
  const string& value, const string& field_name,
  bool log_error = true);

class CAgpSyntaxValidator
{
public:
  CAgpSyntaxValidator();
  bool ValidateLine(const SDataLine& dl, const string& text_line);
  void EndOfObject(bool afterLastLine=false);

  // static bool GapBreaksScaffold(int type, int linkage);
  static bool IsGapType(const string& type);
  void PrintTotals();

protected:
  // Vars assigned in ValidateLine(),
  // further validated in x_OnGapLine() x_OnCompLine()
  int obj_range_len;
  bool new_obj;

  // Private subroutines for ValidateLine()
  void x_OnGapLine(const SDataLine& dl, const CGapVal& gap);
  void x_OnCompLine(const SDataLine& dl, const CCompVal& comp);

  int m_ObjCount;
  int m_ScaffoldCount;
  int m_SingleCompScaffolds;
  int m_SingleCompObjects;
  int m_CompCount;
  int m_GapCount;
  int m_CompOri[CCompVal::ORI_count];


  //CValuesCount m_TypeGapCnt;  // column 7: fragment, clone, ...
  int m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_yes_count];
  CValuesCount m_TypeCompCnt; // column 5: A, D, F, ..., N, U

  // Count component types and N/U gap types.
  int  m_LineTypes;
  int* m_LineTypeCnt;

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

  TValuesSet m_ComponentTypeValues;





  string prev_object;
  string prev_component_type;

  // Used for the first component in a scaffold;
  // we do not know whether the scaffold is a singleton,
  // and whether unlnown orientation is an error.
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

#endif /* AGP_VALIDATE_SyntaxValidator */

