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
 * Author:  Victor Sapojnikov
 *
 * File Description:
 *   The most generic fast AGP stream reader.
 */

#include <ncbi_pch.hpp>
#include <objtools/readers/agp_util.hpp>

BEGIN_NCBI_SCOPE

//// class CAgpErr
const CAgpErr::TStr CAgpErr::msg[]= {
  kEmptyCStr,

  // Content Errors (codes 1..20)
  "expecting 9 tab-separated columns", // 8 or
  "column X is empty",
  "empty line",
  "invalid value for X",
  "invalid linkage \"yes\" for gap_type ",

  "X must be a positive integer",
  "object_end is less than object_beg",
  "component_end is less than component_beg",
  "object range length not equal to the gap length",
  "object range length not equal to component range length",

  "duplicate object ",
  "first line of an object must have object_beg=1",
  "first line of an object must have part_number=1",
  "part number (column 4) != previous part number + 1",
  "0 or na component orientation may only be used in a singleton scaffold",

  "object_beg != previous object_end + 1",
  "no valid AGP lines",
  kEmptyCStr, // E_Last
  kEmptyCStr,
  kEmptyCStr,

  // Content Warnings
  "gap at the end of an object",
  "object begins with a gap",
  "two consequtive gap lines (e.g. a gap at the end of "
    "a scaffold, two non scaffold-breaking gaps, ...)",
  "no components in object",
  "the span overlaps a previous span for this component",

  "component span appears out of order",
  "duplicate component with non-draft type",
  "line with component_type X appears to be a gap line and not a component line",
  "line with component_type X appears to be a component line and not a gap line",
  "extra <TAB> character at the end of line",

  "gap line missing column 9 (null)",
  "missing line separator at the end of file",
  kEmptyCStr // W_Last
};

const char* CAgpErr::GetMsg(int code)
{
  return msg[code];
}

string CAgpErr::FormatMessage(int code, const string& details)
{
  string msg = GetMsg(code);
  if( details.size()==0 ) return msg;

  SIZE_TYPE pos = NStr::Find( string(" ") + msg + " ", " X " );
  if(pos!=NPOS) {
    // Substitute "X" with the real value (e.g. a column name or value)
    return msg.substr(0, pos) + details + msg.substr(pos+1);
  }
  else{
    return msg + details;
  }
}

// For the sake of speed, we do not care about warnings
// (unless they follow a previous error message).
void CAgpErr::Msg(int code, const string& details, int appliesTo)
{
  if(code<E_Last) {
    last_error = code;
    messages_apply_to |= appliesTo;
    messages += "\tERROR: ";
    messages += FormatMessage(code, details);
    messages += "\n";
  }
  else if(messages_apply_to) {
    // Append warnings to preexisting errors.
    // To collect all warnings, Msg() should be overrided in a derived class.
    messages_apply_to |= appliesTo;
    messages += "\tWARNING: ";
    messages += FormatMessage(code, details);
    messages += "\n";
  }
}

//// class CAgpRow
const CAgpRow::TStr CAgpRow::gap_types[CAgpRow::GAP_count] = {
  "clone",
  "fragment",
  "repeat",

  "contig",
  "centromere",
  "short_arm",
  "heterochromatin",
  "telomere"
};

CAgpErr* CAgpRow::agpErr = NULL;
map<string, CAgpRow::TGap> CAgpRow::gap_type_codes;

CAgpRow::CAgpRow()
{
  if(gap_type_codes.empty()) {
    // initialize this static map once
    for(int i=0; i<GAP_count; i++) {
      gap_type_codes[ gap_types[i] ] = (TGap)i;
    }

    // agp_validate overrides this default error handler
    if(agpErr==NULL) agpErr=new CAgpErr();
  }
}

int CAgpRow::FromString(const string& line)
{
  // Comments
  cols.clear();
  SIZE_TYPE pcomment = NStr::Find(line, "#");

  bool tabsStripped=false;
  //has_eol_comment = (pcomment != NPOS);
  if( pcomment != NPOS  ) {
    // Strip whitespace before "#"
    while( pcomment>0 && (line[pcomment-1]==' ' || line[pcomment-1]=='\t') ) {
      if( line[pcomment-1]=='\t' ) tabsStripped=true;
      pcomment--;
    }
    if(pcomment==0) return -1; // A comment line; to be skipped.
    //line.resize(pos);
    NStr::Tokenize(line.substr(0, pcomment), "\t", cols);
  }
  else {
    NStr::Tokenize(line, "\t", cols);
  }

  if(line.size() == 0) {
    agpErr->Msg(CAgpErr::E_EmptyLine);
    return CAgpErr::E_EmptyLine;
  }

  // Column count
  if( cols.size()==10 && cols[9]=="") {
    agpErr->Msg(CAgpErr::W_ExtraTab);
  }
  else if( cols.size() < 8 || cols.size() > 9 ) {
    // skip this entire line, report an error
    agpErr->Msg(CAgpErr::E_ColumnCount,
      string(", found ") + NStr::IntToString(cols.size()) );
    return CAgpErr::E_ColumnCount;
  }

  // Empty columns
  for(int i=0; i<8; i++) {
    if(cols[i].size()==0) {
      agpErr->Msg(CAgpErr::E_EmptyColumn, NStr::IntToString(i+1) );
      return CAgpErr::E_EmptyColumn;
    }
  }

  // object_beg, object_end, part_number
  object_beg = NStr::StringToNumeric( GetObjectBeg() );
  if(object_beg<=0) agpErr->Msg(CAgpErr::E_MustBePositive, "object_beg (column 2)");
  object_end = NStr::StringToNumeric( GetObjectEnd() );
  if(object_end<=0) {
    agpErr->Msg(CAgpErr::E_MustBePositive, "object_end (column 4)");
  }
  part_number = NStr::StringToNumeric( GetPartNumber() );
  if(part_number<=0) {
    agpErr->Msg(CAgpErr::E_MustBePositive, "part_number (column 4)");
    return CAgpErr::E_MustBePositive;
  }
  if(object_beg<=0 || object_end<=0) return CAgpErr::E_MustBePositive;
  if(object_end < object_beg) {
    agpErr->Msg(CAgpErr::E_ObjEndLtBeg);
    return CAgpErr::E_ObjEndLtBeg;
  }
  int object_range_len = object_end - object_beg + 1;

  // component_type, type-specific columns
  if(GetComponentType().size()!=1) {
    agpErr->Msg(CAgpErr::E_InvalidValue, "component_type (column 5)");
    return CAgpErr::E_InvalidValue;
  }
  component_type=GetComponentType()[0];
  switch(component_type) {
    case 'A':
    case 'D':
    case 'F':
    case 'G':
    case 'P':
    case 'O':
    case 'W':
    {
      is_gap=false;
      if(cols.size()==8) {
        if(tabsStripped) {
          agpErr->Msg(CAgpErr::E_EmptyColumn, "9");
          return CAgpErr::E_EmptyColumn;
        }
        else {
          agpErr->Msg(CAgpErr::E_ColumnCount, ", found 8" );
          return CAgpErr::E_ColumnCount;
        }
      }

      int code=ParseComponentCols();
      if(code==0) {
        int component_range_len=component_end-component_beg+1;
        if(component_range_len != object_range_len) {
          agpErr->Msg( CAgpErr::E_ObjRangeNeComp, string(": ") +
            NStr::IntToString(object_range_len   ) + " != " +
            NStr::IntToString(component_range_len)
          );
          return CAgpErr::E_ObjRangeNeComp;
        }
        return 0;
      }
      else {
        if(ParseGapCols(false)==0) {
          agpErr->Msg( CAgpErr::W_LooksLikeGap, GetComponentType() );
        }
        return code;
      }
    }

    case 'N':
    case 'U':
    {
      is_gap=true;
      if(cols.size()==8 && tabsStripped==false) {
        agpErr->Msg(CAgpErr::W_GapLineMissingCol9);
      }

      int code=ParseGapCols();
      if(code==0) {
        if(gap_length != object_range_len) {
          agpErr->Msg( CAgpErr::E_ObjRangeNeGap, string(": ") +
            NStr::IntToString(object_range_len   ) + " != " +
            NStr::IntToString(gap_length)
          );
          return CAgpErr::E_ObjRangeNeGap;
        }
        return 0;
      }
      else {
        if(ParseComponentCols(false)==0) {
          agpErr->Msg( CAgpErr::W_LooksLikeComp, GetComponentType() );
        }
        return code;
      }

    }
    default :
      agpErr->Msg(CAgpErr::E_InvalidValue, "component_type (column 5)");
      return CAgpErr::E_InvalidValue;
  }
}

int CAgpRow::ParseComponentCols(bool log_errors)
{
  // component_beg, component_end
  component_beg = NStr::StringToNumeric( GetComponentBeg() );
  if(component_beg<=0 && log_errors) {
    agpErr->Msg(CAgpErr::E_MustBePositive, "component_beg (column 7)" );
  }
  component_end = NStr::StringToNumeric( GetComponentEnd() );
  if(component_end<=0 && log_errors) {
    agpErr->Msg(CAgpErr::E_MustBePositive, "component_end (column 8)" );
  }
  if(component_beg<=0 || component_end<=0) return CAgpErr::E_MustBePositive;

  if( component_end < component_beg ) {
    if(log_errors) {
      agpErr->Msg(CAgpErr::E_CompEndLtBeg);
    }
    return CAgpErr::E_CompEndLtBeg;
  }

  // orientation
  if(GetOrientation()=="na") {
    orientation='n';
    return 0;
  }
  if(GetOrientation().size()==1) {
    orientation=GetOrientation()[0];
    switch( orientation )
    {
      case '+':
      case '-':
      case '0':
        return 0;
    }
  }
  if(log_errors) {
    agpErr->Msg(CAgpErr::E_InvalidValue,"orientation (column 9)");
  }
  return CAgpErr::E_InvalidValue;
}

int CAgpRow::ParseGapCols(bool log_errors)
{
  gap_length = NStr::StringToNumeric( GetGapLength() );
  if(gap_length<=0) {
    if(log_errors) agpErr->Msg(CAgpErr::E_MustBePositive, "gap_length (column 6)" );
    return CAgpErr::E_MustBePositive;
  }

  map<string, TGap>::const_iterator it = gap_type_codes.find( GetGapType() );
  if(it==gap_type_codes.end()) {
    if(log_errors) agpErr->Msg(CAgpErr::E_InvalidValue, "gap_type (column 7)");
    return CAgpErr::E_InvalidValue;
  }
  gap_type=it->second;

  if(GetLinkage()=="yes") {
    linkage=true;
  }
  else if(GetLinkage()=="no") {
    linkage=false;
  }
  else {
    if(log_errors) agpErr->Msg(CAgpErr::E_InvalidValue, "linkage (column 8)");
    return CAgpErr::E_InvalidValue;
  }

  if(linkage) {
    if( gap_type!=GAP_clone && gap_type!=GAP_repeat && gap_type!=GAP_fragment ) {
      if(log_errors) agpErr->Msg(CAgpErr::E_InvalidYes, GetGapType() );
      return CAgpErr::E_InvalidYes;
    }
  }

  return 0;
}

string CAgpRow::ToString()
{
  string res=
    GetObject() + "\t" +
    NStr::IntToString(object_beg ) + "\t" +
    NStr::IntToString(object_end ) + "\t" +
    NStr::IntToString(part_number) + "\t";

  res+=component_type;
  res+='\t';

  if(is_gap) {
    res +=
      NStr::IntToString(gap_length) + "\t" +
      gap_types[gap_type] + "\t" +
      (linkage?"yes":"no") + "\t";
  }
  else{
    res +=
      GetComponentId  () + "\t" +
      NStr::IntToString(component_beg) + "\t" +
      NStr::IntToString(component_end) + "\t";
    if(orientation=='n') res+="na";
    else res+=orientation;
  }

  return res;
}

const string& CAgpRow::GetErrors()
{
  return agpErr->messages;
}


//// class CAgpReader
CAgpReader::CAgpReader()
{
  prev_row=new CAgpRow();
  this_row=new CAgpRow();
  at_beg=true;
  prev_line_num=-1;
  //at_end=false;
  //line_num = 0;
}

CAgpReader::~CAgpReader()
{
  delete prev_row;
  delete this_row;
}

int CAgpReader::ReadStream(CNcbiIstream& is, bool finalize)
{
  at_end=false;
  line_num=0;
  if(at_beg) {
    //// The first line
    row_skipped=false;
    new_obj=true;
    while( NcbiGetline(is, line, "\r\n") ) {
      line_num++;
      int code=this_row->FromString(line);
      if(code==0) {
        if( !row_skipped ) {
          if(this_row->object_beg !=1) CAgpRow::agpErr->Msg(CAgpErr::E_ObjMustBegin1);
          if(this_row->part_number!=1) CAgpRow::agpErr->Msg(CAgpErr::E_PartNumberNot1);
        }
        OnObjectChange();
        if(this_row->is_gap) {
          if( !row_skipped && !this_row->GapValidAtObjectEnd() ) {
            CAgpRow::agpErr->Msg(CAgpErr::W_GapObjBegin);
          }
          //OnGap();
        }
        //else OnComponent();
        OnGapOrComponent();
        at_beg=false;
        break;
      }
      else if(code==-1) {
        OnComment();
      }
      else {
        row_skipped=true;
        if( !OnError(code) ) return code;
      }
    }
    if(at_beg) {
      CAgpRow::agpErr->Msg(CAgpErr::E_NoValidLines, CAgpErr::AT_None);
      return CAgpErr::E_NoValidLines;
    }
    else if( CAgpRow::agpErr->last_error) {
      if(!OnError(0)) return CAgpRow::agpErr->last_error;
      CAgpRow::agpErr->clear();
    }

    // swap this_row and prev_row
    CAgpRow* t=this_row;
    this_row=prev_row;
    prev_row=t;
    prev_line_num=line_num;
  }
  //// Second and later lines
  //// Adding context-sensetive code that uses prev_row
  while( NcbiGetline(is, line, "\r\n") ) {
    line_num++;
    int code=this_row->FromString(line);
    if(code==0) {

      new_obj=prev_row->GetObject() != this_row->GetObject();
      if( new_obj ) {
        if( !row_skipped ) {
          if(this_row->object_beg !=1) CAgpRow::agpErr->Msg(CAgpErr::E_ObjMustBegin1, CAgpErr::AT_ThisLine|CAgpErr::AT_PrevLine);
          if(this_row->part_number!=1) CAgpRow::agpErr->Msg(CAgpErr::E_PartNumberNot1, CAgpErr::AT_ThisLine|CAgpErr::AT_PrevLine);
          if(prev_row->is_gap && !prev_row->GapValidAtObjectEnd()) {
            CAgpRow::agpErr->Msg(CAgpErr::W_GapObjEnd, CAgpErr::AT_PrevLine);
          }
        }
        if(!( prev_row->is_gap && prev_row->GapEndsScaffold() )) {
          OnScaffoldEnd();
        }
        OnObjectChange();
      }
      else {
        if(!row_skipped) {
          if(this_row->part_number != prev_row->part_number+1) {
            CAgpRow::agpErr->Msg(CAgpErr::E_PartNumberNotPlus1, CAgpErr::AT_ThisLine|CAgpErr::AT_PrevLine);
          }
          if(this_row->object_beg != prev_row->object_end+1) {
            CAgpRow::agpErr->Msg(CAgpErr::E_ObjBegNePrevEndPlus1, CAgpErr::AT_ThisLine|CAgpErr::AT_PrevLine);
          }
        }
      }

      if(this_row->is_gap) {
        if(!row_skipped) {
          if( new_obj && !this_row->GapValidAtObjectEnd() ) {
            CAgpRow::agpErr->Msg(CAgpErr::W_GapObjBegin); // , CAgpErr::AT_ThisLine|CAgpErr::AT_PrevLine
          }
          else if(prev_row->is_gap) {
            CAgpRow::agpErr->Msg(CAgpErr::W_ConseqGaps, CAgpErr::AT_ThisLine|CAgpErr::AT_PrevLine);
          }
        }
        if(!new_obj) {
          if( this_row->GapEndsScaffold() && !(
            prev_row->is_gap && prev_row->GapEndsScaffold()
          )) OnScaffoldEnd();
        }
        //OnGap();
      }
      //else { OnComponent(); }
      OnGapOrComponent();

      row_skipped=false; // means "after one-line error", not two-line error
      if(CAgpRow::agpErr->last_error){
        if( !OnError(0) ) return CAgpRow::agpErr->last_error;
        CAgpRow::agpErr->clear();
      }
    }
    else if(code==-1) {
      OnComment();
      continue; // for OnObjectChange(), keep the line before previous as previous
    }
    else {
      row_skipped=true;
      if( !OnError(code) ) return code;
      CAgpRow::agpErr->clear();
      continue; // for OnObjectChange(), keep the line before previous as previous
    }

    {
      // swap this_row and prev_row
      CAgpRow* t=this_row;
      this_row=prev_row;
      prev_row=t;
      at_beg=false;
      prev_line_num=line_num;
    }

    if(is.eof()) {
      CAgpRow::agpErr->Msg(CAgpErr::W_NoEolAtEof);
    }
  }

  return finalize ? Finalize() : 0;
}

//// By default, called at the end of ReadStream
//// Only needs to be called manually after reading all input lines
//// via ReadStream(stream, false) or via (projected) ReadLine()s.
int CAgpReader::Finalize()
{
  at_end=true;
  if(!at_beg) {
    new_obj=true; // The only meaning here: scaffold ended because object ended

    if( !row_skipped ) {
      if(prev_row->is_gap && !prev_row->GapValidAtObjectEnd()) {
        CAgpRow::agpErr->Msg(CAgpErr::W_GapObjEnd, CAgpErr::AT_PrevLine);
      }
    }

    if(!( prev_row->is_gap && prev_row->GapEndsScaffold() )) {
      OnScaffoldEnd();
    }
    OnObjectChange();
  }
  int code=0;
  if(CAgpRow::agpErr->last_error){
    if( !OnError(0) ) code=CAgpRow::agpErr->last_error;
    CAgpRow::agpErr->clear();
  }

  at_beg=true; // In preparation for the next file
  return code;
}

END_NCBI_SCOPE

