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
 *      AGP context-sensitive validation (uses information from several lines).
 *
 *
 */

#include <ncbi_pch.hpp>
#include "ContextValidator.hpp"
//#include "AccessionPatterns.hpp"

#include <algorithm>

USING_NCBI_SCOPE;
BEGIN_NCBI_SCOPE

//// class CAgpContextValidator
CAgpContextValidator::CAgpContextValidator()
{
  prev_end = 0;
  prev_part_num = 0;
  componentsInLastScaffold = 0;
  componentsInLastObject = 0;
  prev_orientation_unknown=false;
  prev_line_gap_type = -1;

  m_ObjCount = 0;
  m_ScaffoldCount = 0;
  m_SingleCompScaffolds = 0;
  m_SingleCompObjects = 0;
  m_CompCount = 0;
  m_GapCount = 0;

  memset(m_CompOri, 0, sizeof(m_CompOri));
  memset(m_GapTypeCnt, 0, sizeof(m_GapTypeCnt));
}

void CAgpContextValidator::EndOfObject(bool afterLastLine)
{
  if(m_ObjCount && componentsInLastObject==0) agpErr.Msg(
    CAgpErrEx::W_ObjNoComp, string(" ") + prev_object,
    CAgpErr::fAtPrevLine
    //afterLastLine ? CAgpErr::fAtPrevLine : (CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine)
  );
  if(componentsInLastScaffold==1) m_SingleCompScaffolds++;
  if(componentsInLastObject  ==1) m_SingleCompObjects++;

  if( prev_line_gap_type>=0 ) {
    // The previous line was a gap at the end of a scaffold & object
    if(! CAgpRow::GapValidAtObjectEnd( (CAgpRow::EGap)prev_line_gap_type ) ) {
      agpErr.Msg( CAgpErrEx::W_GapObjEnd, prev_object,
        CAgpErr::fAtPrevLine);
    }
    if(componentsInLastScaffold==0) m_ScaffoldCount--;
  }

  componentsInLastScaffold=0;
  componentsInLastObject=0;
}

void CAgpContextValidator::InvalidLine()
{
  prev_orientation_unknown=false;
  prev_line_gap_type = -1;
}


void CAgpContextValidator::ValidateRow(CAgpRow& row, int line_num)
{
  //// Context-sensitive code common for GAPs and components.
  //// Checks and statistics.

  // Local variable shared with x_OnGapLine(), x_OnCompLine()
  new_obj = (row.GetObject() != prev_object);

  if(new_obj) {
    m_ObjCount++;
    m_ScaffoldCount++;

    // Do not call when we are at the first line of the first object
    if( prev_object.size() ) EndOfObject();
    prev_object = row.GetObject(); // Must be after EndOfObject()

    TObjSetResult obj_insert_result = m_ObjIdSet.insert(row.GetObject());
    if (obj_insert_result.second == false) {
      agpErr.Msg(CAgpErrEx::E_DuplicateObj, row.GetObject(),
        CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine);
    }

    // object_beg and part_num: must be 1
    if(row.object_beg != 1) {
      agpErr.Msg(CAgpErrEx::E_ObjMustBegin1,
        NcbiEmptyString,
        CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine|CAgpErrEx::fAtSkipAfterBad
      );
    }
    if(row.part_number != 1) {
      agpErr.Msg( CAgpErrEx::E_PartNumberNot1,
        NcbiEmptyString, CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine|CAgpErrEx::fAtSkipAfterBad );
    }
  }
  else {
    // object range and part_num: compare to prev line
    if(row.object_beg != prev_end+1) {
      agpErr.Msg(CAgpErrEx::E_ObjBegNePrevEndPlus1,
        NcbiEmptyString,
        CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine|CAgpErrEx::fAtSkipAfterBad
      );
    }
    if(row.part_number != prev_part_num+1) {
      agpErr.Msg( CAgpErrEx::E_PartNumberNotPlus1,
        NcbiEmptyString, CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine|CAgpErrEx::fAtSkipAfterBad );
    }
  }

  m_TypeCompCnt.add( row.GetComponentType() );

  //// Gap- or component-specific code.
  if( row.IsGap() ) {
    x_OnGapRow(row);
    prev_line_gap_type = row.gap_type;
  }
  else {
    x_OnCompRow(row, line_num);
    prev_line_gap_type = -1;
  }

  prev_end = row.object_end; // cl.obj_end;
  prev_part_num = row.part_number;
}

void CAgpContextValidator::x_OnGapRow(CAgpRow& row)
{
  int i = row.gap_type;
  if(row.linkage) i+= CAgpRow::eGapCount;
  NCBI_ASSERT( i < (int)sizeof(m_GapTypeCnt)/sizeof(m_GapTypeCnt[0]),
    "m_GapTypeCnt[] index out of bounds" );
  m_GapTypeCnt[i]++;
  m_GapCount++;

  //// Check the gap context: is it a start of a new object,
  //// does it follow another gap, is it the end of a scaffold.
  if(new_obj) {
    if(! row.GapValidAtObjectEnd() ) {
      agpErr.Msg(CAgpErrEx::W_GapObjBegin,
        row.GetObject(), CAgpErr::fAtThisLine|CAgpErrEx::fAtSkipAfterBad);
    }
  }
  else if( prev_line_gap_type>=0 ) {
    agpErr.Msg( CAgpErrEx::W_ConseqGaps, NcbiEmptyString,
      CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine|CAgpErrEx::fAtSkipAfterBad );
  }
  else if( row.GapEndsScaffold() ) {
    if(componentsInLastScaffold>0) {
      // A breaking gap after a component; prev line is the last component of a scaffold.
      m_ScaffoldCount++;
      if(componentsInLastScaffold==1) {
        m_SingleCompScaffolds++;
      }
    }
    // else: a breaking gap following the start of the object
  }
  else {
    // a non-breaking gap after a component
    if(prev_orientation_unknown && componentsInLastScaffold==1) {
                             // ^^^can probably ASSERT this^^^
      agpErr.Msg(CAgpErrEx::E_UnknownOrientation,
        NcbiEmptyString, CAgpErr::fAtPrevLine);
      prev_orientation_unknown=false;
    }
  }
  if( row.GapEndsScaffold() ) {
    componentsInLastScaffold=0;
  }
}

void CAgpContextValidator::x_OnCompRow(CAgpRow& row, int line_num)
{
  //// Statistics
  m_CompCount++;
  componentsInLastScaffold++;
  componentsInLastObject++;
  switch(row.orientation) {
    case '+': m_CompOri[CCompVal::ORI_plus ]++; break;
    case '-': m_CompOri[CCompVal::ORI_minus]++; break;
    case '0': m_CompOri[CCompVal::ORI_zero ]++; break;
    case 'n': m_CompOri[CCompVal::ORI_na   ]++; break;
  }

  //// Orientation "0" or "na" only for singletons
  // A saved potential error
  if(prev_orientation_unknown) {
    // Make sure that prev_orientation_unknown
    // is not a leftover from the preceding singleton.
    if( componentsInLastScaffold==2 ) {
      agpErr.Msg(CAgpErrEx::E_UnknownOrientation,
        NcbiEmptyString, CAgpErr::fAtPrevLine);
    }
    prev_orientation_unknown=false;
  }

  if( row.orientation != '+' && row.orientation != '-' ) {
    if(componentsInLastScaffold==1) {
      // Report an error later if the current scaffold
      // turns out not a singleton. Only singletons can have
      // components with unknown orientation.
      //
      // Note: previous component != previous line
      // if there was a non-breaking gap; thus, we check
      // prev_orientation_unknown for such gaps, too.
      prev_orientation_unknown=true;
    }
    else {
      // This error is real, not "potential"; report it now.
      agpErr.Msg(CAgpErrEx::E_UnknownOrientation, NcbiEmptyString);
    }
  }

  //// Check that component spans do not overlap and are in correct order

  // Try to insert to the span as a new entry
  CCompVal comp;
  comp.init(row, line_num);
  TCompIdSpansPair value_pair( row.GetComponentId(), CCompSpans(comp) );
  pair<TCompId2Spans::iterator, bool> id_insert_result =
      m_CompId2Spans.insert(value_pair);

  if(id_insert_result.second == false) {
    // Not inserted - the key already exists.
    CCompSpans& spans = (id_insert_result.first)->second;

    // Chose the most specific warning among:
    //   W_SpansOverlap W_SpansOrder W_DuplicateComp
    // The last 2 are omitted for draft seqs.
    CCompSpans::TCheckSpan check_sp = spans.CheckSpan(
      // can reploace with row.component_beg, etc
      comp.beg, comp.end, row.orientation!='-'
    );
    if( check_sp.second == CAgpErrEx::W_SpansOverlap  ) {
      agpErr.Msg(CAgpErrEx::W_SpansOverlap,
        string(": ")+ check_sp.first->ToString()
      );
    }
    else if( ! row.IsDraftComponent() ) {
      agpErr.Msg(check_sp.second, // W_SpansOrder or W_DuplicateComp
        string("; preceding span: ")+ check_sp.first->ToString()
      );
    }

    // Add the span to the existing entry
    spans.AddSpan(comp);
  }
}

#define ALIGN_W(x) setw(w) << resetiosflags(IOS_BASE::left) << (x)
void CAgpContextValidator::PrintTotals()
{
  //// Counts of errors and warnings
  int e_count=agpErr.CountTotals(CAgpErrEx::E_Last);
  int w_count=agpErr.CountTotals(CAgpErrEx::W_Last);
  if(e_count || w_count || m_ObjCount) {
    cout << "\n";
    agpErr.PrintTotals(cout, e_count, w_count, agpErr.m_msg_skipped);
    if(agpErr.m_MaxRepeatTopped) {
      cout << " (to print all: -limit 0)";
    }
    cout << ".";
    if(agpErr.m_MaxRepeat && (e_count+w_count) ) {
      cout << "\n";
      agpErr.PrintMessageCounts(cout, CAgpErrEx::CODE_First, CAgpErrEx::CODE_Last);
    }
    cout << "\n";
  }
  if(m_ObjCount==0) {
    cout << "No valid AGP lines.\n";
    return;
  }

  cout << "\n";
  if(agpErr.m_lines_skipped) {
    cout << "\nNOTE: " << agpErr.m_lines_skipped << " invalid lines were skipped.\n"
      "Some counts below may be smaller than expected by " << agpErr.m_lines_skipped
      << " or less.\n";
  }

  //// Prepare component/gap types and counts for later printing
  string s_comp, s_gap;

  CValuesCount::TValPtrVec comp_cnt;
  m_TypeCompCnt.GetSortedValues(comp_cnt);

  for(CValuesCount::TValPtrVec::iterator
    it = comp_cnt.begin();
    it != comp_cnt.end();
    ++it
  ) {
    string *s = CAgpRow::IsGap((*it)->first[0]) ? &s_gap : &s_comp;

    if( s->size() ) *s+= ", ";
    *s+= (*it)->first;
    *s+= ":";
    *s+= NStr::IntToString((*it)->second);
  }

  //// Various counts of AGP elements

  // w: width for right alignment
  int w = NStr::IntToString(m_CompId2Spans.size()).size(); // +1;

  cout << "\n"
    "Objects                : "<<ALIGN_W(m_ObjCount           )<<"\n"
    "- with single component: "<<ALIGN_W(m_SingleCompObjects  )<<"\n"
    "\n"
    "Scaffolds              : "<<ALIGN_W(m_ScaffoldCount      )<<"\n"
    "- with single component: "<<ALIGN_W(m_SingleCompScaffolds)<<"\n"
    "\n";


  cout<<
    "Components             : "<< ALIGN_W(m_CompCount);
  if(m_CompCount) {
    if( s_comp.size() ) {
      if( NStr::Find(s_comp, ",")!=NPOS ) {
        // (W: 1234, D: 5678)
        cout << " (" << s_comp << ")";
      }
      else {
        // One type of components: (W) or (invalid type)
        cout << " (" << s_comp.substr( 0, NStr::Find(s_comp, ":") ) << ")";
      }
    }
    cout << "\n"
      "\torientation +  : " << ALIGN_W(m_CompOri[CCompVal::ORI_plus ]) << "\n"
      "\torientation -  : " << ALIGN_W(m_CompOri[CCompVal::ORI_minus]) << "\n"
      "\torientation 0  : " << ALIGN_W(m_CompOri[CCompVal::ORI_zero ]) << "\n"
      "\torientation na : " << ALIGN_W(m_CompOri[CCompVal::ORI_na   ]) << "\n";
  }

  cout << "\n" << "Gaps                   : " << ALIGN_W(m_GapCount);
  if(m_GapCount) {
    // Print (N) if all components are of one type,
    //        or (N: 1234, U: 5678)
    if( s_gap.size() ) {
      if( NStr::Find(s_gap, ",")!=NPOS ) {
        // (N: 1234, U: 5678)
        cout << " (" << s_gap << ")";
      }
      else {
        // One type of gaps: (N)
        cout << " (" << s_gap.substr( 0, NStr::Find(s_gap, ":") ) << ")";
      }
    }

    int linkageYesCnt =
      m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapClone   ]+
      m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapFragment]+
      m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapRepeat  ];
    int linkageNoCnt = m_GapCount - linkageYesCnt;

    int doNotBreakCnt= linkageYesCnt + m_GapTypeCnt[CAgpRow::eGapFragment];
    int breakCnt     = linkageNoCnt  - m_GapTypeCnt[CAgpRow::eGapFragment];

    cout<< "\n- do not break scaffold: "<<ALIGN_W(doNotBreakCnt);
    if(doNotBreakCnt) {
      cout<< "\n  clone   , linkage yes: "<<
            ALIGN_W(m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapClone   ]);
      cout<< "\n  fragment, linkage yes: "<<
            ALIGN_W(m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapFragment]);
      cout<< "\n  fragment, linkage no : "<<
            ALIGN_W(m_GapTypeCnt[                   CAgpRow::eGapFragment]);
      cout<< "\n  repeat  , linkage yes: "<<
            ALIGN_W(m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapRepeat  ]);
    }

    cout<< "\n- break it, linkage no : "<<ALIGN_W(breakCnt);
    if(breakCnt) {
      for(int i=0; i<CAgpRow::eGapCount; i++) {
        if(i==CAgpRow::eGapFragment) continue;
        cout<< "\n\t"
            << setw(15) << setiosflags(IOS_BASE::left) << CAgpRow::GapTypeToString(i)
            << ": " << ALIGN_W( m_GapTypeCnt[i] );
      }
    }

  }
  cout << "\n";

  if(m_ObjCount) {
    {
      CAccPatternCounter objNamePatterns;
      //objNamePatterns.AddNames(m_ObjIdSet);
      for(TObjSet::const_iterator it = m_ObjIdSet.begin();
          it != m_ObjIdSet.end(); ++it
      ) {
        objNamePatterns.AddName(*it);
      }

      x_PrintPatterns(objNamePatterns, "Object names");
    }
  }

  if(m_CompId2Spans.size()) {
    {
      CAccPatternCounter compNamePatterns;
      for(TCompId2Spans::iterator it = m_CompId2Spans.begin();
        it != m_CompId2Spans.end(); ++it)
      {
        compNamePatterns.AddName(it->first);
      }
      x_PrintPatterns(compNamePatterns, "Component names");
    }
  }
}

// Sort by accession count, print not more than MaxPatterns or 2*MaxPatterns
void CAgpContextValidator::x_PrintPatterns(
  CAccPatternCounter& namePatterns, const string& strHeader)
{
  const int MaxPatterns=10;

  // Sorted by count to print most frequent first
  CAccPatternCounter::TMapCountToString cnt_pat; // multimap<int,string>
  namePatterns.GetSortedPatterns(cnt_pat);
  SIZE_TYPE cnt_pat_size=cnt_pat.size();

  cout << "\n";

  // Calculate width of columns 1 (wPattern) and 2 (w, count)
  int w = NStr::IntToString(
      cnt_pat.rbegin()->first // the biggest count
    ).size()+1;

  int wPattern=strHeader.size()-2;
  int totalCount=0;
  int patternsPrinted=0;
  for(
    CAccPatternCounter::TMapCountToString::reverse_iterator
    it = cnt_pat.rbegin(); it != cnt_pat.rend(); it++
  ) {
    if( ++patternsPrinted<=MaxPatterns ||
        cnt_pat_size<=2*MaxPatterns
    ) {
      int i = it->second.size();
      if( i > wPattern ) wPattern = i;
    }
    else {
      if(w+15>wPattern) wPattern = w+15;
    }
    totalCount+=it->first;
  }

  // Print the total
  cout<< setw(wPattern+2) << setiosflags(IOS_BASE::left)
      << strHeader << ": " << ALIGN_W(totalCount) << "\n";

  // Print the patterns
  patternsPrinted=0;
  int accessionsSkipped=0;
  for(
    CAccPatternCounter::TMapCountToString::reverse_iterator
    it = cnt_pat.rbegin(); it != cnt_pat.rend(); it++
  ) {
    // Limit the number of lines to MaxPatterns or 2*MaxPatterns
    if( ++patternsPrinted<=MaxPatterns ||
        cnt_pat_size<=2*MaxPatterns
    ) {
      cout<< "  " << setw(wPattern) << setiosflags(IOS_BASE::left)
          << it->second                   // pattern
          << ": " << ALIGN_W( it->first ) // : count
          << "\n";
    }
    else {
      // accessionsSkipped += CAccPatternCounter::GetCount(*it);
      accessionsSkipped += it->first;
    }
  }

  if(accessionsSkipped) {
    string s = "other ";
    s+=NStr::IntToString(cnt_pat_size - 10);
    s+=" patterns";
    cout<< "  " << setw(wPattern) << setiosflags(IOS_BASE::left) << s
        << ": " << ALIGN_W( accessionsSkipped )
        << "\n";

  }
}

//// class CValuesCount

void CValuesCount::GetSortedValues(TValPtrVec& out)
{
  out.clear(); out.reserve( size() );
  for(iterator it = begin();  it != end(); ++it) {
    out.push_back(&*it);
  }
  std::sort( out.begin(), out.end(), x_byCount );
}

void CValuesCount::add(const string& c)
{
  iterator it = find(c);
  if(it==end()) {
    (*this)[c]=1;
  }
  else{
    it->second++;
  }
}

int CValuesCount::x_byCount( value_type* a, value_type* b )
{
  if( a->second != b->second ){
    return a->second > b->second; // by count, largest first
  }
  return a->first < b->first; // by name
}

//// class CCompSpans - stores data for all preceding components
CCompSpans::TCheckSpan CCompSpans::CheckSpan(int span_beg, int span_end, bool isPlus)
{
  // The lowest priority warning (to be ignored for draft seqs)
  TCheckSpan res( begin(), CAgpErrEx::W_DuplicateComp );

  for(iterator it = begin();  it != end(); ++it) {
    // A high priority warning
    if( it->beg <= span_beg && span_beg <= it->end )
      return TCheckSpan(it, CAgpErrEx::W_SpansOverlap);

    // A lower priority warning (to be ignored for draft seqs)
    if( ( isPlus && span_beg < it->beg) ||
        (!isPlus && span_end > it->end)
    ) {
      res.first  = it;
      res.second = CAgpErrEx::W_SpansOrder;
    }
  }

  return res;
}

void CCompSpans::AddSpan(const CCompVal& span)
{
  push_back(span);
}


END_NCBI_SCOPE

