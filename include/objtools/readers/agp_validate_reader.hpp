#ifndef AGP_VALIDATE_READER
#define AGP_VALIDATE_READER

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
 *      Global (whole-file) AGP validation and statistics.
 *
 */

#include <corelib/ncbistd.hpp>
#include <iostream>
#include <objtools/readers/agp_util.hpp>
#include <util/range_coll.hpp>

#include <set>

BEGIN_NCBI_SCOPE

// Map of string->int populated by sequence ids->lengths from a FASTA file.
typedef map<string, TAgpLen> TMapStrInt;
class NCBI_XOBJREAD_EXPORT CMapCompLen : public TMapStrInt
{
public:
  // may be less than size() because we add some names twice, e.g.: lcl|id1 and id1
  int m_count;

  typedef pair<TMapStrInt::iterator, bool> TMapStrIntResult;
  // returns 0 on success, or a previous length not equal to the new one
  TAgpLen AddCompLen(const string& acc, TAgpLen len, bool increment_count=true);
  CMapCompLen()
  {
    m_count=0;
  }

};


typedef CRangeCollection<TSeqPos> TRangeColl;
typedef map<string, TRangeColl> TMapStrRangeColl;

 // Determines accession naming patterns, counts accessions.
class CAccPatternCounter;

// Count how many times a atring value appears;
// report values and counts ordered by count.
class NCBI_XOBJREAD_EXPORT CValuesCount : public map<string, int>
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

class NCBI_XOBJREAD_EXPORT CCompVal
{
public:
  TAgpPos beg, end;
  char ori;
  int file_num, line_num;

  enum { ORI_plus, ORI_minus, ORI_zero, ORI_na,  ORI_count };

  void init(CAgpRow& row, int line_num_arg)
  {
    beg=row.component_beg;
    end=row.component_end;
    ori=row.orientation;

    line_num=line_num_arg;
    file_num = ((CAgpErrEx*)(row.GetErrorHandler()))->GetFileNum();
  }
  TAgpLen getLen() const { return end - beg + 1; }

  string ToString(CAgpErrEx* agpErrEx) const
  {
    string s;
    s += NStr::IntToString(beg);
    s += "..";
    s += NStr::IntToString(end);
    s += " at ";
    if(file_num) {
      s += agpErrEx->GetFile(file_num);
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
class NCBI_XOBJREAD_EXPORT CCompSpans : public vector<CCompVal>
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
  typedef pair<iterator, TAgpPos> TCheckSpan;
  TCheckSpan CheckSpan(TAgpPos span_beg, TAgpPos span_end, bool isPlus);
  void AddSpan(const CCompVal& span); // CCompSpans::iterator it,

};

class NCBI_XOBJREAD_EXPORT IAgpRowOutput
{
public:
  virtual void SaveRow(const string& s, CRef<CAgpRow> row, TRangeColl* runs_of_Ns)=0;
  virtual ~IAgpRowOutput() {}
};

class XPrintTotalsItem;
class CAgpValidateReader;
class NCBI_XOBJREAD_EXPORT CAgpValidateReader : public CAgpReader
{
public:

  // false: called from constructor
  // true : after finishing with scaf-fronm-ctg files, before starting with chr-from-scaf
  void Reset(bool for_chr_from_scaf=false);

  CAgpValidateReader(CAgpErrEx& agpErr, CMapCompLen& comp2len, TMapStrRangeColl& comp2range_coll); // , bool checkCompNames=false);
  virtual ~CAgpValidateReader();
  void PrintTotals(CNcbiOstream& out=cout, bool use_xml=false);

  bool m_CheckCompNames;
  bool m_CheckObjLen; // false: check component lengths
  bool m_unplaced;    // check that singleton components are in '+' orientation

  bool m_is_chr, m_explicit_scaf;
  // false false - no extra checks
  // true  false - check telomer/centromer/short_arm counts [-cc; not implemented]
  // false true  - no breaking gaps allowed
  // true true   - no within-scaffold gaps allowed
  // to do: E_UnusedScaf

  void SetRowOutput(IAgpRowOutput* row_output);

  typedef map<int,int> TMapIntInt;
protected:
  void x_PrintTotals(CNcbiOstream& out=cout, bool use_xml=false); // without comment counts or ids not in AGP
  //void x_PrintIdsNotInAgp(CNcbiOstream& out=cout, bool use_xml=false);

  // true: a suspicious mix of ids - some look like GenBank accessions, some do not.
  static bool x_PrintPatterns(CAccPatternCounter& namePatterns, const string& strHeader, int fasta_count, const char* count_label=nullptr, CNcbiOstream& out=cout, bool use_xml=false);

  CAgpErrEx* m_AgpErr;
  int m_CommentLineCount;
  int m_EolComments;

  // Callbacks from CAgpReader
  virtual void OnScaffoldEnd();
  virtual void OnObjectChange();
  virtual void OnGapOrComponent();
  virtual bool OnError();
  virtual void OnComment();

  // for W_ObjOrderNotNumerical (JIRA: GP-773)
  string m_obj_id_pattern; // object_id with each run of conseq digits replaced with '#'
  // >0 number of sorted literally so far - 1 (for the same current m_obj_id_pattern)
  int m_obj_id_sorted; // 0 not established yet <0 sort order violated
  CAccPatternCounter::TDoubleVec* m_obj_id_digits;
  CAccPatternCounter::TDoubleVec* m_prev_id_digits;

  CMapCompLen* m_comp2len; // for optional check of component lengths (or maybe object lengths)
  CMapCompLen m_scaf2len;  // for: -scaf Scaf_AGP_file(s) -chr Chr_AGP_file(s)
  TMapStrRangeColl* m_comp2range_coll;
  TAgpLen m_expected_obj_len;
  int m_comp_name_matches;
  int m_obj_name_matches;

  int m_componentsInLastScaffold;
  int m_componentsInLastObject;
  int m_gapsInLastScaffold;
  int m_gapsInLastObject;
  //bool m_prev_orientation_unknown;
  char m_prev_orientation; // 0 when we need not warn about it (not in singleton, etc)
  TAgpPos m_prev_component_beg, m_prev_component_end;

  string m_prev_component_id; // for W_BreakingGapSameCompId: only set when encountering a breaking gap

  int m_ObjCount;
  int m_ScaffoldCount;
  int m_SingleCompScaffolds;
  int m_SingleCompObjects;
  int m_SingleCompScaffolds_withGaps;
  int m_SingleCompObjects_withGaps;
  int m_NoCompScaffolds;
  // add m_NoCompObjects?

  int m_CompCount;
  int m_GapCount;
  int m_CompOri[4];

  //int m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_yes_count];
  int m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapYes_count];
  // Count component types and N/U gap types.
  CValuesCount m_TypeCompCnt; // column 5: A, D, F, ..., N, U

  // keep track of the object ids to detect duplicates.
  typedef set<string> TObjSet;
  typedef pair<TObjSet::iterator, bool> TObjSetResult;
  TObjSet m_ObjIdSet;

  CAccPatternCounter m_objNamePatterns;

  // keep track of the component and object ids used
  //  in the AGP. Used to detect duplicates and
  //  duplicates with seq range intersections.
  typedef map<string, CCompSpans> TCompId2Spans;
  typedef pair<string, CCompSpans> TCompIdSpansPair;
  TCompId2Spans m_CompId2Spans;

  typedef pair<TAgpPos,int> TPairIntInt;
  TMapIntInt m_ln_ev_flags2count;

  typedef pair<TMapIntInt::iterator, bool> TMapIntIntResult;
  TMapIntInt m_Ngap_ln2count;
  TMapIntInt m_Ugap_ln2count;

  TMapIntInt m_NgapByType_ln2count[CAgpRow::eGapCount+CAgpRow::eGapYes_count];

  // returns: first=plain text string for the end of line, second= attributes for XML tag (cnt, pct, mf_len)
  // uses m_NgapByType_ln2count[]
  void x_GetMostFreqGapsText(int gap_type, string& eol_text, string& attrs);

  void x_PrintGapCountsLine(XPrintTotalsItem& xprint, int gap_type, const string& label=NcbiEmptyString);

  // an optional callback object
  IAgpRowOutput* m_row_output;

  int m_last_scaf_start_file;
  int m_last_scaf_start_line;
  bool m_last_scaf_start_is_obj;

  TAgpPos m_max_comp_beg;
  TAgpPos m_max_obj_beg;
  bool m_has_partial_comp, m_has_comp_of_unknown_len;

  // Former CAgpValidateReader::x_PrintIdsNotInAgp(), split into member functions for the purpose of
  // running CheckIds() earlier, to be able to add this error to the total error/warning counts.
  class CIdsNotInAgp
  {
    CAgpValidateReader& m_reader;
    CAccPatternCounter m_patterns;
    set<string> m_ids;
    int m_cnt;
  public:
    CIdsNotInAgp(CAgpValidateReader& reader) : m_reader(reader)
    {
      m_cnt=0;
    }

    // returns: an error/warning message, to be later passed to Print() or PrintXml();
    // "" if no error.
    string CheckIds();

    void Print(CNcbiOstream& out, const string& msg);
    void PrintXml(CNcbiOstream& out, const string& msg);
  };
  friend class CIdsNotInAgp;
};

END_NCBI_SCOPE

#endif /* AGP_VALIDATE_READER */

