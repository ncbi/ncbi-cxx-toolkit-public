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
 *      information in the AGP file.
 *
 *
 */

#include <ncbi_pch.hpp>
#include "SyntaxValidator.hpp"
#include "AccessionPatterns.hpp"

USING_NCBI_SCOPE;
BEGIN_NCBI_SCOPE

//// class CGapVal
TValuesMap CGapVal::typeStrToInt;
TValuesMap CGapVal::validLinkage;
const CGapVal::TStr CGapVal::typeIntToStr[CGapVal::GAP_count+CGapVal::GAP_yes_count] = {
  "clone",
  "fragment",
  "repeat",

  "contig",
  "centromere",
  "short_arm",
  "heterochromatin",
  "telomere"
};

CGapVal::CGapVal()
{
  // Populate the static maps when invoked for the first time.
  if( typeStrToInt.size()==0 ) {
    for(int i=0; i<GAP_count; i++) {
      typeStrToInt[ CGapVal::typeIntToStr[i] ] = i;
    }

    validLinkage["no" ] = LINKAGE_no;
    validLinkage["yes"] = LINKAGE_yes;
  }
}

// Check individual field's values;
// return false if there are errors.
bool CGapVal::init(const SDataLine& dl, bool log_errors)
{
  len     = x_CheckIntField( dl.gap_length, "gap_length (column 6)", log_errors);
  type    = x_CheckValues( typeStrToInt   , dl.gap_type, "gap_type", log_errors);
  linkage = x_CheckValues( validLinkage, dl.linkage, "linkage", log_errors);
  if(len <=0 || -1 == type || -1 == linkage ) return false;

  return true;
}

//// class CCompVal
TValuesMap CCompVal::validOri;

CCompVal::CCompVal()
{
  // Populate the static maps when invoked for the first time.
  if( validOri.size()==0 ) {
    validOri["+" ] = ORI_plus;
    validOri["-" ] = ORI_minus;
    validOri["0" ] = ORI_zero;
    validOri["na"] = ORI_na;
  }

}

// Check individual field's values and comp_end < comp_end;
// return false if there are errors;
// save file and line numbers.
bool CCompVal::init(const SDataLine& dl, bool log_errors)
{
  // Error messages are issued within x_Check* functions
  beg = x_CheckIntField( dl.component_beg, "component_beg (column 7)", log_errors );
  end = x_CheckIntField( dl.component_end, "component_end (column 8)", log_errors );
  ori = x_CheckValues( validOri, dl.orientation,
    "orientation (column 9)", log_errors);
  if( beg <= 0 || end <= 0 || -1 == ori ) return false;

  if( end < beg ) {
    if(log_errors) {
      agpErr.Msg(CAgpErr::E_CompEndLtBeg);
    }
    return false;
  }

  file_num=agpErr.GetFileNum();
  line_num=dl.line_num;
  return true;
}

string CCompVal::ToString() const
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


//// class CAgpSyntaxValidator
CAgpSyntaxValidator::CAgpSyntaxValidator()
{
  //objNamePatterns = new CAccPatternCounter();

  prev_end = 0;
  prev_part_num = 0;
  componentsInLastScaffold = 0;
  componentsInLastObject = 0;
  prev_orientation_unknown=false;

  m_ObjCount = 0;
  m_ScaffoldCount = 0;
  m_SingleCompScaffolds = 0;
  m_SingleCompObjects = 0;
  m_CompCount = 0;
  m_GapCount = 0;

  memset(m_CompOri, 0, sizeof(m_CompOri));
  memset(m_GapTypeCnt, 0, sizeof(m_GapTypeCnt));

  m_ComponentTypeValues.insert("A");
  m_ComponentTypeValues.insert("D");
  m_ComponentTypeValues.insert("F");
  m_ComponentTypeValues.insert("G");
  m_ComponentTypeValues.insert("P");
  m_ComponentTypeValues.insert("N"); // gap
  m_ComponentTypeValues.insert("O");
  m_ComponentTypeValues.insert("U"); // gap of unknown size
  m_ComponentTypeValues.insert("W");

}

void CAgpSyntaxValidator::EndOfObject(bool afterLastLine)
{
  if(componentsInLastObject==0) agpErr.Msg(
    CAgpErr::W_ObjNoComp, string(" ") + prev_object,
    afterLastLine ? AT_PrevLine : (AT_ThisLine|AT_PrevLine)
  );
  if(componentsInLastScaffold==1) m_SingleCompScaffolds++;
  if(componentsInLastObject  ==1) m_SingleCompObjects++;
  componentsInLastScaffold=0;
  componentsInLastObject=0;

  if( IsGapType(prev_component_type) ) agpErr.Msg(
    // The previous line was a gap at the end of a scaffold & object
    CAgpErr::W_GapObjEnd, string(" ")+prev_object,
    AT_PrevLine
    //afterLastLine ? AT_PrevLine : (AT_ThisLine|AT_PrevLine)
  );
}

bool CAgpSyntaxValidator::ValidateLine( const SDataLine& dl,
  const string& text_line)
{
  //// Simple checks for common columns:
  //// x_CheckIntField(), x_CheckValues(), obj_beg < obj_end.
  //// Skip the line if there are such errors.

  // (error messages are issued within x_Check* functions)
  int obj_begin = x_CheckIntField( dl.begin , "object_beg (column 2)" );
  int obj_end   = x_CheckIntField( dl.end   , "object_end (column 3)" );
  int part_num  = x_CheckIntField( dl.part_num, "part_num (column 4)" );
  bool is_type_valid = x_CheckValues( m_ComponentTypeValues,
    dl.component_type, "component_type (column 5)");

  if( obj_begin <= 0 || obj_end <= 0 || part_num <=0 || !is_type_valid ) {
    // suppress possibly spurious errors
    prev_orientation_unknown=false; return false;
  }

  if(obj_end < obj_begin) {
    agpErr.Msg(CAgpErr::E_ObjEndLtBegin);
    // suppress possibly spurious errors
    prev_orientation_unknown=false; return false;
  }

  //// Simple checks for gap- or component-specific  columns:
  //// x_CheckIntField(), x_CheckValues(), comp_beg < comp_end,
  //// gap or component length != object range.
  //// Skip the line if there are such errors.

  bool simpleError = false;
  CCompVal compSpan;
  CGapVal gap;
  bool is_gap = IsGapType(dl.component_type);
  int obj_range_len = obj_end - obj_begin + 1;

  if(is_gap) {
    if( !gap.init(dl) ) {
      simpleError=true;

      if( compSpan.init(dl, NO_LOG) ) {
        agpErr.Msg(CAgpErr::W_LooksLikeComp,
          NcbiEmptyString, AT_ThisLine, // defaults
          dl.component_type );
      }
    }
    else if(gap.getLen() != obj_range_len) {
      simpleError=true;
      agpErr.Msg(CAgpErr::E_ObjRangeNeGap, string(": ") +
        NStr::IntToString(obj_range_len) + " != " +
        NStr::IntToString(gap.len      ) );
    }
  }
  else {
    if( !compSpan.init(dl) ) {
      simpleError=true;

      if( gap.init(dl, NO_LOG) ) {
        agpErr.Msg(CAgpErr::W_LooksLikeGap,
          NcbiEmptyString, AT_ThisLine, // defaults
          dl.component_type );
      }
    }
    else if(compSpan.getLen() != obj_range_len) {
      simpleError=true;
      agpErr.Msg(CAgpErr::E_ObjRangeNeComp);
    }
  }

  if(simpleError) {
    // suppress possibly spurious errors
    prev_orientation_unknown=false; return false;
  }


  //// Context-sensetive code common for GAPs and components.
  //// Checks and statistics.

  // Local variables shared with x_OnGapLine(), x_OnCompLine()
  new_obj = (dl.object != prev_object);

  if(new_obj) {
    m_ObjCount++;
    m_ScaffoldCount++; // to do: detect or avoid scaffold with 0 components?

    // Do not call when we are at the first line of the first object
    if( prev_object.size() ) EndOfObject();
    prev_object = dl.object; // Must be after EndOfObject()

    TObjSetResult obj_insert_result = m_ObjIdSet.insert(dl.object);
    if (obj_insert_result.second == false) {
      agpErr.Msg(CAgpErr::E_DuplicateObj, dl.object,
        AT_ThisLine|AT_PrevLine);
    }

    // object_beg and part_num: must be 1
    if(obj_begin != 1) {
      agpErr.Msg(CAgpErr::E_ObjMustBegin1,
        NcbiEmptyString,
        AT_ThisLine|AT_PrevLine
      );
    }
    if(part_num != 1) {
      agpErr.Msg( CAgpErr::E_PartNumberNot1,
        NcbiEmptyString, AT_ThisLine|AT_PrevLine );
    }
  }
  else {
    // object range and part_num: compare to prev line
    if(obj_begin != prev_end+1) {
      agpErr.Msg(CAgpErr::E_ObjBegNePrevEndPlus1,
        NcbiEmptyString,
        AT_ThisLine|AT_PrevLine
      );
    }
    if(part_num != prev_part_num+1) {
      agpErr.Msg( CAgpErr::E_PartNumberNotPlus1,
        NcbiEmptyString, AT_ThisLine|AT_PrevLine );
    }
  }

  m_TypeCompCnt.add( dl.component_type );

  //// Gap- or component-specific code.
  if( is_gap ) {
    x_OnGapLine(dl, gap);
  }
  else {
    x_OnCompLine(dl, compSpan);
  }

  prev_end = obj_end;
  prev_part_num = part_num;
  prev_component_type = dl.component_type;

  // allow the checks that use both this line and the next
  return true;
}

void CAgpSyntaxValidator::x_OnGapLine(
  const SDataLine& dl, const CGapVal& gap)
{
  m_GapCount++;
  m_GapTypeCnt[
    gap.type +
    (gap.linkage==CGapVal::LINKAGE_yes ? CGapVal::GAP_count : 0)
  ]++;

  //// Check whether  gap_type and linkage combination is valid,
  //// and if this combination ends the scaffold.
  bool endsScaffold = true;
  if(gap.type==CGapVal::GAP_fragment) {
    endsScaffold=false;
  }
  else if(gap.linkage==CGapVal::LINKAGE_yes) {
    endsScaffold=false;
    if( gap.type==CGapVal::GAP_clone || gap.type==CGapVal::GAP_repeat ) {
      // Ok
    }
    else {
      agpErr.Msg(CAgpErr::E_InvalidYes, dl.gap_type);
    }
  }
  // else: endsScaffold remains true

  //// Check the gap context: is it a start of a new object,
  //// does it follow another gap, is it the end of a scaffold.
  if(new_obj) {
    agpErr.Msg(CAgpErr::W_GapObjBegin);
    // NcbiEmptyString, AT_ThisLine|AT_PrevLine);
  }
  else if( IsGapType(prev_component_type) ) {
    agpErr.Msg( CAgpErr::W_ConseqGaps, NcbiEmptyString,
      AT_ThisLine|AT_PrevLine );
  }
  else if( endsScaffold ) {
    // A breaking gap after a component.
    // Previous line is the last component of a scaffold
    m_ScaffoldCount++; // to do: detect or do not count scaffold with 0 components?
    if(componentsInLastScaffold==1) {
      m_SingleCompScaffolds++;
    }
  }
  else {
    // a non-breaking gap after a component
    if(prev_orientation_unknown && componentsInLastScaffold==1) {
                             // ^^^can probably ASSERT this^^^
      agpErr.Msg(CAgpErr::E_UnknownOrientation,
        NcbiEmptyString, AT_PrevLine);
      prev_orientation_unknown=false;
    }
  }
  if( endsScaffold ) {
    componentsInLastScaffold=0;
  }
}

void CAgpSyntaxValidator::x_OnCompLine(
  const SDataLine& dl, const CCompVal& comp )
{
  //// Statistics
  m_CompCount++;
  componentsInLastScaffold++;
  componentsInLastObject++;
  m_CompOri[comp.ori]++;

  //// Orientation "0" or "na" only for singletons
  // A saved potential error
  if(prev_orientation_unknown) {
    // Make sure that prev_orientation_unknown
    // is not a leftover from the preceding singleton.
    if( componentsInLastScaffold==2 ) {
      agpErr.Msg(CAgpErr::E_UnknownOrientation,
        NcbiEmptyString, AT_PrevLine);
    }
    prev_orientation_unknown=false;
  }

  if( comp.ori != CCompVal::ORI_plus && comp.ori != CCompVal::ORI_minus ) {
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
      agpErr.Msg(CAgpErr::E_UnknownOrientation);
    }
  }

  //// Component spans do not overlap and are in correct order

  // Try to insert to the span as a new entry
  TCompIdSpansPair value_pair( dl.component_id, CCompSpans(comp) );
  pair<TCompId2Spans::iterator, bool> id_insert_result =
      m_CompId2Spans.insert(value_pair);

  if(id_insert_result.second == false) {
    // Not inserted - the key already exists.
    CCompSpans& spans = (id_insert_result.first)->second;

    // Issue at most 1 warning
    CCompSpans::TCheckSpan check_sp = spans.CheckSpan(
      comp.beg, comp.end, comp.ori!=CCompVal::ORI_minus
    );
    if( check_sp.second == CAgpErr::W_SpansOverlap  ) {
      agpErr.Msg(CAgpErr::W_SpansOverlap,
        string(": ")+ check_sp.first->ToString()
      );
    }
    else if(
      dl.component_type!="A" && // Active Finishing
      dl.component_type!="D" && // Draft HTG
      dl.component_type!="P"    // Pre Draft
    ) {
      // Non-draft sequence
      agpErr.Msg(check_sp.second, // W_SpansOrder or W_DuplicateComp
        string("; preceding span: ")+ check_sp.first->ToString()
      );
    }

    // Add the span to the existing entry
    spans.AddSpan(comp);
  }
}

int x_CheckIntField( const string& field,
  const string& field_name, bool log_error)
{
  int field_value = 0;
  try {
      field_value = NStr::StringToInt(field);
  } catch (...) {
  }

  if (field_value <= 0  &&  log_error) {
    agpErr.Msg(CAgpErr::E_MustBePositive,
        NcbiEmptyString, AT_ThisLine, // defaults
        field_name );
  }
  return field_value;
}

bool x_CheckValues(const TValuesSet& values,
  const string& value, const string& field_name, bool log_error)
{
  if(values.count(value) == 0) {
    if(log_error) {
      agpErr.Msg(CAgpErr::E_InvalidValue,
        NcbiEmptyString, AT_ThisLine, // defaults
        field_name );
    }
    return false;
  }
  return true;
}

int x_CheckValues(const TValuesMap& values,
  const string& value, const string& field_name, bool log_error)
{
  TValuesMap::const_iterator it = values.find(value);
  if( it==values.end() ) {
    if(log_error) {
      agpErr.Msg(CAgpErr::E_InvalidValue,
        NcbiEmptyString, AT_ThisLine, // defaults
        field_name );
    }
    return -1;
  }
  return it->second;
}

#define ALIGN_W(x) setw(w) << resetiosflags(IOS_BASE::left) << (x)
void CAgpSyntaxValidator::PrintTotals()
{
  //// Counts of errors and warnings
  int e_count=agpErr.CountTotals(CAgpErr::E_Last);
  int w_count=agpErr.CountTotals(CAgpErr::W_Last);
  cout << "\n";
  agpErr.PrintTotals(cout, e_count, w_count, agpErr.m_msg_skipped);
  if(agpErr.m_MaxRepeatTopped) {
    cout << " (to print all: -limit 0)";
  }
  cout << ".";
  if(agpErr.m_MaxRepeat && (e_count+w_count) ) {
    cout << "\n";
    agpErr.PrintMessageCounts(cout, CAgpErr::CODE_First, CAgpErr::CODE_Last);
  }
  if(m_ObjCount==0) {
    cout << "\nNo valid AGP lines.\n";
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

  CValuesCount::pv_vector comp_cnt;
  m_TypeCompCnt.GetSortedValues(comp_cnt);

  for(CValuesCount::pv_vector::iterator
    it = comp_cnt.begin();
    it != comp_cnt.end();
    ++it
  ) {
    string *s = IsGapType((*it)->first) ? &s_gap : &s_comp;

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
      m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_clone   ]+
      m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_fragment]+
      m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_repeat  ];
    int linkageNoCnt = m_GapCount - linkageYesCnt;
    /*
    cout << "\n- linkage yes -------- : "<<ALIGN_W(linkageYesCnt);
    if(linkageYesCnt) {
      for(int i=0; i<CGapVal::GAP_yes_count; i++) {
        cout<< "\n\t"
            << setw(15) << setiosflags(ios_base::left) << CGapVal::typeIntToStr[i]
            << ": " << ALIGN_W( m_GapTypeCnt[CGapVal::GAP_count+i] );
      }
    }

    cout << "\n- linkage no  -------- : "<<ALIGN_W(linkageNoCnt);
    if(linkageNoCnt) {
      for(int i=0; i<CGapVal::GAP_count; i++) {
        cout<< "\n\t"
            << setw(15) << setiosflags(ios_base::left) << CGapVal::typeIntToStr[i]
            << ": " << ALIGN_W( m_GapTypeCnt[i] );
      }
    }
    */

    int doNotBreakCnt= linkageYesCnt + m_GapTypeCnt[CGapVal::GAP_fragment];
    int breakCnt     = linkageNoCnt  - m_GapTypeCnt[CGapVal::GAP_fragment];

    cout<< "\n- do not break scaffold: "<<ALIGN_W(doNotBreakCnt);
    if(doNotBreakCnt) {
      cout<< "\n  clone   , linkage yes: "<<
            ALIGN_W(m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_clone   ]);
      cout<< "\n  fragment, linkage yes: "<<
            ALIGN_W(m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_fragment]);
      cout<< "\n  fragment, linkage no : "<<
            ALIGN_W(m_GapTypeCnt[                   CGapVal::GAP_fragment]);
      cout<< "\n  repeat  , linkage yes: "<<
            ALIGN_W(m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_repeat  ]);
    }

    cout<< "\n- break it, linkage no : "<<ALIGN_W(breakCnt);
    if(breakCnt) {
      for(int i=0; i<CGapVal::GAP_count; i++) {
        if(i==CGapVal::GAP_fragment) continue;
        cout<< "\n\t"
            << setw(15) << setiosflags(ios_base::left) << CGapVal::typeIntToStr[i]
            << ": " << ALIGN_W( m_GapTypeCnt[i] );
      }
    }

  }
  cout << "\n";

  if(m_ObjCount) {
    {
      CAccPatternCounter objNamePatterns;
      objNamePatterns.AddNames(m_ObjIdSet);
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
void CAgpSyntaxValidator::x_PrintPatterns(
  CAccPatternCounter& namePatterns, const string& strHeader)
{
  const int MaxPatterns=10;

  // Get a vector with sorted pointers to map values
  CAccPatternCounter::pv_vector pat_cnt;
  namePatterns.GetSortedValues(pat_cnt);

  /*
  if(pat_cnt.size()==1) {
    // Continue on the same line, do not print counts
    cout<< " " << CAccPatternCounter::GetExpandedPattern( pat_cnt[0] )
        << "\n";
    return;
  }
  */
  cout << "\n";


  // Calculate width of columns 1 (pattern) and 2 (count)
  int w = NStr::IntToString(
      CAccPatternCounter::GetCount(pat_cnt[0])
    ).size()+1;

  int wPattern=strHeader.size()-2;
  int totalCount=0;
  int patternsPrinted=0;
  for(CAccPatternCounter::pv_vector::iterator it =
      pat_cnt.begin(); it != pat_cnt.end(); ++it
  ) {
    if( ++patternsPrinted<=MaxPatterns || pat_cnt.size()<=2*MaxPatterns ) {
      int i = CAccPatternCounter::GetExpandedPattern(*it).size();
      if( i > wPattern ) wPattern = i;
    }
    else {
      if(w+15>wPattern) wPattern = w+15;
    }
    totalCount+=CAccPatternCounter::GetCount(*it);
  }

  // Print the total
  cout<< setw(wPattern+2) << setiosflags(IOS_BASE::left)
      << strHeader << ": " << ALIGN_W(totalCount) << "\n";

  // Print the patterns
  patternsPrinted=0;
  int accessionsSkipped=0;
  for(CAccPatternCounter::pv_vector::iterator it =
      pat_cnt.begin(); it != pat_cnt.end(); ++it
  ) {
    // Limit the number of lines to MaxPatterns or 2*MaxPatterns
    if( ++patternsPrinted<=MaxPatterns || pat_cnt.size()<=2*MaxPatterns ) {
      cout<< "  " << setw(wPattern) << setiosflags(IOS_BASE::left)
          << CAccPatternCounter::GetExpandedPattern(*it)
          << ": " << ALIGN_W( CAccPatternCounter::GetCount(*it) )
          << "\n";
    }
    else {
      accessionsSkipped += CAccPatternCounter::GetCount(*it);
    }
  }

  if(accessionsSkipped) {
    string s = "other ";
    s+=NStr::IntToString(pat_cnt.size() - 10);
    s+=" patterns";
    cout<< "  " << setw(wPattern) << setiosflags(IOS_BASE::left) << s
        << ": " << ALIGN_W( accessionsSkipped )
        << "\n";

  }
}

bool CAgpSyntaxValidator::IsGapType(const string& type)
{
  return type=="N" || type=="U";
}

//// class CValuesCount

void CValuesCount::GetSortedValues(pv_vector& out)
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
  TCheckSpan res( begin(), CAgpErr::W_DuplicateComp );

  for(iterator it = begin();  it != end(); ++it) {
    // A high priority warning
    if( it->beg <= span_beg && span_beg <= it->end )
      return TCheckSpan(it, CAgpErr::W_SpansOverlap);

    // A lower priority error (would even be ignored for draft seqs)
    if( ( isPlus && span_beg < it->beg) ||
        (!isPlus && span_end > it->end)
    ) {
      res.first  = it;
      res.second = CAgpErr::W_SpansOrder;
    }
  }

  return res;
}

void CCompSpans::AddSpan(const CCompVal& span)
{
  push_back(span);
  //if(span.beg < beg) beg = span.beg;
  //if(end < span.end) end = span.end;
}


END_NCBI_SCOPE

