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
 *      Validation tests that can be performed on one line in the AGP file.
 *      If any of these fails, the rest of the tests (which may use multiple lines
 *      or GenBank info) are skipped.
 *
 *
 */

#include <ncbi_pch.hpp>
#include "LineValidator.hpp"

USING_NCBI_SCOPE;
BEGIN_NCBI_SCOPE

//// class CGapVal
TValuesMap CGapVal::typeStrToInt;
TValuesMap CGapVal::validLinkage;
// +CGapVal::GAP_yes_count
const CGapVal::TStr CGapVal::typeIntToStr[CGapVal::GAP_count] = {
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
  type    = x_CheckValues( typeStrToInt   , dl.gap_type, "gap_type (column 7)", log_errors);
  linkage = x_CheckValues( validLinkage, dl.linkage, "linkage (column 8)", log_errors);
  if(len <=0 || -1 == type || -1 == linkage ) return false;

  if(linkage==LINKAGE_yes) {
    if( type!=GAP_clone && type!=GAP_repeat && type!=GAP_fragment ) {
      //if(log_errors)
      agpErr.Msg(CAgpErr::E_InvalidYes, dl.gap_type);
      return false;
    }
  }
  return true;
}

bool CGapVal::endsScaffold() const
{
  if(type==GAP_fragment) return false;
  return linkage==LINKAGE_no;
}

bool CGapVal::validAtObjectEnd() const
{
  return type==GAP_centromere || type==GAP_telomere || type==GAP_short_arm;
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


//// class CAgpLine
TValuesMap CAgpLine::componentTypeValues;

CAgpLine::CAgpLine()
{
  if( componentTypeValues.size()==0 ) {
    componentTypeValues["A"]='A';
    componentTypeValues["D"]='D';
    componentTypeValues["F"]='F';
    componentTypeValues["G"]='G';
    componentTypeValues["P"]='P';
    componentTypeValues["N"]='N'; // gap
    componentTypeValues["O"]='O';
    componentTypeValues["U"]='U'; // gap of unknown size
    componentTypeValues["W"]='W';
  }
}

bool CAgpLine::init( const SDataLine& dl)
{
  //// Simple checks for common columns:
  //// x_CheckIntField(), x_CheckValues(), obj_beg < obj_end.
  //// Skip the line if there are such errors.

  // (error messages are issued within x_Check* functions)
  obj_begin = x_CheckIntField( dl.begin , "object_beg (column 2)" );
  obj_end   = x_CheckIntField( dl.end   , "object_end (column 3)" );
  part_num  = x_CheckIntField( dl.part_num, "part_num (column 4)" );
  comp_type = x_CheckValues( componentTypeValues,
    dl.component_type, "component_type (column 5)");

  if( obj_begin <= 0 || obj_end <= 0 || part_num <=0 || comp_type<0 ) {
    return false;
  }

  if(obj_end < obj_begin) {
    agpErr.Msg(CAgpErr::E_ObjEndLtBegin);
    return false;
  }

  //// Simple checks for gap- or component-specific  columns:
  //// x_CheckIntField(), x_CheckValues(), comp_beg < comp_end,
  //// gap or component length != object range.
  //// Skip the line if there are such errors.

  bool valid = true;
  is_gap = IsGapType(dl.component_type);
  int obj_range_len = obj_end - obj_begin + 1;

  if(is_gap) {
    if( !gap.init(dl) ) {
      valid=false;

      if( compSpan.init(dl, NO_LOG) ) {
        agpErr.Msg(CAgpErr::W_LooksLikeComp,
          NcbiEmptyString, AT_ThisLine, // defaults
          dl.component_type );
      }
    }
    else if(gap.getLen() != obj_range_len) {
      valid=false;
      agpErr.Msg(CAgpErr::E_ObjRangeNeGap, string(": ") +
        NStr::IntToString(obj_range_len) + " != " +
        NStr::IntToString(gap.len      ) );
    }
  }
  else {
    if( !compSpan.init(dl) ) {
      valid=false;

      if( gap.init(dl, NO_LOG) ) {
        agpErr.Msg(CAgpErr::W_LooksLikeGap,
          NcbiEmptyString, AT_ThisLine, // defaults
          dl.component_type );
      }
    }
    else if(compSpan.getLen() != obj_range_len) {
      valid=false;
      agpErr.Msg( CAgpErr::E_ObjRangeNeComp, string(": ") +
        NStr::IntToString( obj_range_len ) + " != " +
        NStr::IntToString( compSpan.getLen() )
      );
    }
  }

  return valid;
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

bool CAgpLine::IsGapType(const string& type)
{
  return type=="N" || type=="U";
}

END_NCBI_SCOPE

