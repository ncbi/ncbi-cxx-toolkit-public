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
 *      Error and warning messages produced by AGP validator.
 *      CAgpErr: print errors and related AGP lines;
 *      supress repetitive messages and the ones
 *      that user requested to suppress.
 *
 */
#include <ncbi_pch.hpp>
#include "AgpErr.hpp"

USING_NCBI_SCOPE;
BEGIN_NCBI_SCOPE

//// class CAgpErr - static members and functions
const CAgpErr::TStr CAgpErr::msg[]= {
  // Errors
  "duplicate object ",
  "first line of an object must have object_begin=1",
  "part number (column 4) != previous part number + 1",

  "object range length not equal to the gap length",
  "Object range length not equal to component range length",
  "component cannot have an unknown orientation",

  "X must be a positive integer",
  "X overlaps a previous line",
  "object_begin is less than object_end",

  "component_start is less than component_end",
  "invalid value for X",
  kEmptyCStr, // E_Last

  // Codes 0..19 are reserved for errors
  // to avoid shifting warning codes in the future
  kEmptyCStr, kEmptyCStr, kEmptyCStr,
  kEmptyCStr, kEmptyCStr, kEmptyCStr,
  kEmptyCStr, kEmptyCStr,

  // Warnings
  "gap at the end of an object",
  "object begins with a gap",
  "two consequtive gap lines (e.g. a gap at the end of "
    "a scaffold, two non scaffold-breaking gaps, ...)",

  "invalid linkage \"yes\" for gap_type ",
  "the span overlaps a previous span for this component",
  "component span appears out of order",

  "duplicate component id with non-draft type",
  "line with component_type X appears to be a gap line and not a component line",
  "line with component_type X appears to be a component line and not a gap line",

  kEmptyCStr // W_Last
};

const char* CAgpErr::GetMsg(TCode code)
{
  return msg[code];
}

bool CAgpErr::MustSkip(TCode code)
{
  return m_MustSkip[code];
}

void CAgpErr::PrintAllMessages(CNcbiOstream& out)
{
  // to do -- print:
  // Errors:
  //   (e01) duplicate object
  //   . . .
  // Warnings:
  //   . . .
  //   (w33) line with component_type X appears to be a component line and not a gap line
  //         X may be: N (gap), U (gap of unknown size) <== append such lines for a few codes
}

string CAgpErr::GetPrintableCode(int code)
{
  string res = (code>E_Last) ? "w" : "e";
  res += NStr::IntToString(code);
  return res;
}

void CAgpErr::PrintLine(CNcbiOstream& ostr,
  const string& filename, int linenum, const string& content)
{
  if(filename.size()) ostr << filename << ":";
  ostr << linenum  << ":" << content << "\n";
}

void CAgpErr::PrintMessage(CNcbiOstream& ostr, TCode code,
    const string& details, const string& substX)
{
  //string msg = msg[code];
  string msg = GetMsg(code);
  if( substX.size() ) {
    // Substitute X with the real value (usually, column name or value)
    string tmp = string(" ") + msg + " ";
    NStr::Replace(tmp, " X ", string(" ")+substX+" ", msg);
    msg = msg.substr( 1, msg.size()-2 );
  }

  ostr<< "\t" << (code>E_Last?"WARNING":"ERROR") << ": "
      << msg << details << "\n";
}


//// class CAgpErr - constructor
CAgpErr::CAgpErr()
{
  m_messages = new CNcbiOstrstream();
  m_MaxRepeat = 0; // no limit
  m_line_num=1;

  m_line_num_prev=0;
  m_prev_printed=false;

  memset(m_MsgCount , 0, sizeof(m_MsgCount ));
  memset(m_MustSkip , 0, sizeof(m_MustSkip ));

  // A "random check" to make sure enum TCode and msg[]
  // are not out of skew.
  NCBI_ASSERT( string(GetMsg(E_Last))=="",
    "CAgpErr -- GetMsg(E_Last) not empty" );
  NCBI_ASSERT( string(GetMsg( (TCode)(E_Last-1) ))!="",
    "CAgpErr -- GetMsg(E_Last-1) is empty" );
  NCBI_ASSERT( string(GetMsg(W_Last))=="",
    "CAgpErr -- GetMsg(W_Last) not empty" );
  NCBI_ASSERT( string(GetMsg( (TCode)(W_Last-1) ))!="",
    "CAgpErr -- GetMsg(W_Last-1) is empty" );
}


//// class CAgpErr - non-static functions
void CAgpErr::Msg(TCode code, const string& details,
  int appliesTo, const string& substX)
{
  m_MsgCount[code]++;
  if( m_MustSkip[code] ) return;
  if( m_MaxRepeat>0 && m_MsgCount[code] > m_MaxRepeat ) return;

  if(appliesTo & AT_PrevLine) {
    // Print the previous line if it was not printed
    if(!m_prev_printed) PrintLine(cerr, m_filename_prev, m_line_num_prev, m_line_prev);
    m_prev_printed=true;
  }
  if(appliesTo & AT_ThisLine) {
    // Accumulate messages
    PrintMessage(*m_messages, code, details, substX);
  }
  else {
    // Print it now (useful for appliesTo==AT_PrevLine)
    PrintMessage(cerr, code, details, substX);
  }
}

void CAgpErr::LineDone(const string& s, int line_num)
{
  if( m_messages->pcount() ) {
    if(m_prev_printed==false) cerr << "\n";

    PrintLine(cerr, m_filename, line_num, s);
    cerr << (string)CNcbiOstrstreamToString(*m_messages);
    delete m_messages;
    m_messages = new CNcbiOstrstream;

    m_prev_printed=true;
  }
  else {
    m_prev_printed=false;
  }
  m_line_num_prev = line_num;
  m_line_prev = s;
}

void CAgpErr::StartFile(const string& s)
{
  if( s.size() ) {
    m_filename_prev=m_filename;
    m_filename=s;
  }
  else {
    // End of all files:
    // Print statistics for all errors,
    // including statistics for skipped errors?
  }
}

// Initialize m_MustSkip[]
// Return values:
//   ""                        no matches found for str
//   string ending with "\n"   one or more messages that matched
string CAgpErr::SkipMsg(const string& str, bool skip_other)
{
  string res;
  const static char* skipErr  = "Skipping errors, printing warnings.";
  const static char* skipWarn = "Skipping warnings, printing errors.";

  // Keywords: all warn* err*
  int i_from=W_Last;
  int i_to  =0;
  if(str=="all") {
    i_from=0; i_to=W_Last;
    // "-only all" does not make a lot of sense,
    // but we can support it anyway.
    res = skip_other ? "Printing" : "Skipping";
    res+=" all errors and warnings.";
  }
  else if (str.substr(0,4)=="warn" && str.size()<=8 ) { // warn ings
    i_from=E_Last; i_to=W_Last;
    res = skip_other ? skipErr : skipWarn;
  }
  else if (str.substr(0,4)=="err" && str.size()<=6 ) { // err ors
    i_from=0; i_to=E_Last;
    res = skip_other ? skipWarn : skipErr;
  }
  if(i_from<i_to) {
    for( int i=i_from; i<i_to; i++ ) m_MustSkip[i] = !skip_other;
    return res;
  }

  // Error or warning codes, substrings of the messages.
  for( int i=0; i<W_Last; i++ ) {
    bool matchesCode = ( str==GetPrintableCode(i) );
    if( matchesCode || NStr::Find(msg[i], str) != NPOS) {
      m_MustSkip[i] = !skip_other;
      res += "  ";
      res += msg[i];
      res += "\n";
      if(matchesCode) break;
    }
  }

  // Empty: no match;
  // else: matched messages, one per line (which might either be
  // skipped, or retained to the exclusion of others).
  return res;
  /*
  if(res.size()) {
    return string("No matches found for \"")+str+"\"";
  }
  else {
    return string("Skipping the following messages:\n")+res;
  }
  */
}

int CAgpErr::CountErrors(TCode from, TCode to)
{
  if(to==E_First) {
    //// One argument: count errors/warnings/given type
    if     (from==E_Last) { from=E_First; to=E_Last; }
    else if(from==W_Last) { from=W_First; to=W_Last; }
    else if(from<W_Last)  return m_MsgCount[from];
    else return -1; // Invalid "from"
  }

  int count=0;
  for(int i=from; i<to; i++) {
    count += m_MsgCount[i];
  }
  return count;
}

/*
//// Report individual error counts
//// (only if we had a limit on the number of errors printed).
if(m_MaxRepeat) {
  if(out) {
    *out<< setw(8) << c
        << " (" << GetPrintableCode(i) << ") "
        << GetMsg(i) << "\n";
  }
}
*/

void CAgpErr::PrintErrorCounts(CNcbiOstream& ostr, int e_count, int w_count)
{
  if     (e_count==0) ostr << "No errors, ";
  else if(e_count==1) ostr << "1 error, "  ;
  else                ostr << e_count << " errors, ";

  if     (w_count==0) ostr << "no warnings.";
  else if(w_count==1) ostr << "1 warning.";
  else                ostr << w_count << " warnings.";
}

END_NCBI_SCOPE

