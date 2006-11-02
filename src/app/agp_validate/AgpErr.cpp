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

// When adding entries to msg[], also update enum TCode
const CAgpErr::TStr CAgpErr::msg[]= {
  kEmptyCStr,

  // Content Errors (codes 1..20)
  "expecting 9 tab-separated columns", // 8 or
  "column X is empty",
  "duplicate object ",
  "first line of an object must have object_beg=1",
  "first line of an object must have part_number=1",

  "part number (column 4) != previous part number + 1",
  "object range length not equal to the gap length",
  "object range length not equal to component range length",
  "0 or na component orientation may only be used in a singleton scaffold",
  "X must be a positive integer",

  "object_beg != previous object_end + 1",
  "object_end is less than object_beg",
  "component_end is less than component_beg",
  "invalid value for X",
  "invalid linkage \"yes\" for gap_type ",

  "empty line",
  kEmptyCStr, // E_Last
  kEmptyCStr,
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
  kEmptyCStr, // W_Last
  kEmptyCStr,
  kEmptyCStr,

  kEmptyCStr,
  kEmptyCStr,
  kEmptyCStr,
  kEmptyCStr,
  kEmptyCStr,

  // GenBank-related errors
  "invalid component_id",
  "component_id not in GenBank",
  "component_id X is ambiguous without an explicit version",
  "component_end greater than sequence length",
  "taxonomic data is invalid or unavailable",

  kEmptyCStr  // G_Last

  // "cannot retrieve the taxonomic id",
  // "cannot retrieve taxonomic data for taxid",
  // "taxid X is above species level",

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
  out << "### Errors ###\n";
  for(int i=E_First; i<E_Last; i++) {
    out << GetPrintableCode(i) << "\t" << GetMsg((TCode)i);
    if(i==E_EmptyColumn) {
      out << " (X: 1..8)";
    }
    else if(i==E_InvalidValue) {
      out << " (X: component_type, gap_type, linkage, orientation)";
    }
    else if(i==E_MustBePositive) {
      out << " (X: object_beg, object_end, part_num, gap_length, component_beg, component_end)";
    }

    out << "\n";
  }

  out << "### Warnings ###\n";
  for(int i=W_First; i<W_Last; i++) {
    out << GetPrintableCode(i) << "\t" << GetMsg((TCode)i);
    if(i==W_GapLineMissingCol9) {
      out << " (only the total count is printed unless you specify: -only " << GetPrintableCode(i) << ")";
    }
    out << "\n";
  }

  out << "### Errors for GenBank-based validation (-alt) ###\n";
  for(int i=G_First; i<G_Last; i++) {
    out << GetPrintableCode(i) << "\t" << GetMsg((TCode)i);
    out << "\n";
  }
}

string CAgpErr::GetPrintableCode(int code)
{
  string res =
    (code<E_Last) ? "e" :
    (code<W_Last) ? "w" : "g";
  if(code<10) res += "0";
  res += NStr::IntToString(code);
  return res;
}

void CAgpErr::PrintLine(CNcbiOstream& ostr,
  const string& filename, int linenum, const string& content)
{
  string line=content.size()<200 ? content : content.substr(0,160)+"...";

  // Mark the first space that is not inside a EOL comment
  SIZE_TYPE posComment = NStr::Find(line, "#");
  SIZE_TYPE posSpace   = NStr::Find(line, " ", 0, posComment);
  if(posSpace!=NPOS) {
    posSpace++;
    line = line.substr(0, posSpace) + "<<<SPACE!" + line.substr(posSpace);
  }

  if(filename.size()) ostr << filename << ":";
  ostr<< linenum  << ":" << line << "\n";
}

void CAgpErr::PrintMessage(CNcbiOstream& ostr, TCode code,
    const string& details, const string& substX)
{
  string msg = GetMsg(code);
  if( substX.size() ) {
    // Substitute X with the real value (usually, column name or value)
    string tmp = string(" ") + msg + " ";
    NStr::Replace(tmp, " X ", string(" ")+substX+" ", msg);
    msg = msg.substr( 1, msg.size()-2 );
  }

  ostr<< "\t" << (
    (code>=W_First && code<W_Last) ? "WARNING" : "ERROR"
  ) << ": " << msg << details << "\n";
}


//// class CAgpErr - constructor
CAgpErr::CAgpErr()
{
  m_messages = new CNcbiOstrstream();
  m_MaxRepeat = 0; // no limit
  m_MaxRepeatTopped = false;
  m_msg_skipped=0;
  m_lines_skipped=0;
  m_line_num=1;

  m_line_num_prev=0;
  m_prev_printed=false;
  m_two_lines_involved=false;

  memset(m_MsgCount , 0, sizeof(m_MsgCount ));
  memset(m_MustSkip , 0, sizeof(m_MustSkip ));
  // likely to be a systematic error, skip it by default
  m_MustSkip[W_GapLineMissingCol9]=true;

  // A "random check" to make sure enum TCode and msg[]
  // are not out of skew.
  //cerr << sizeof(msg)/sizeof(msg[0]) << "\n";
  //cerr << G_Last+1 << "\n";
  NCBI_ASSERT( sizeof(msg)/sizeof(msg[0])==G_Last+1,
    "msg[] size  != G_Last+1" );
  NCBI_ASSERT( string(GetMsg(E_Last))=="",
    "CAgpErr -- GetMsg(E_Last) not empty" );
  NCBI_ASSERT( string(GetMsg( (TCode)(E_Last-1) ))!="",
    "CAgpErr -- GetMsg(E_Last-1) is empty" );
  NCBI_ASSERT( string(GetMsg(W_Last))=="",
    "CAgpErr -- GetMsg(W_Last) not empty" );
  NCBI_ASSERT( string(GetMsg( (TCode)(W_Last-1) ))!="",
    "CAgpErr -- GetMsg(W_Last-1) is empty" );
  NCBI_ASSERT( string(GetMsg(G_Last))=="",
    "CAgpErr -- GetMsg(G_Last) not empty" );
  NCBI_ASSERT( string(GetMsg( (TCode)(G_Last-1) ))!="",
    "CAgpErr -- GetMsg(G_Last-1) is empty" );
}


//// class CAgpErr - non-static functions
void CAgpErr::Msg(TCode code, const string& details,
  int appliesTo, const string& substX)
{
  // Ignore possibly spurious errors generated after
  // an unacceptable line with wrong # of columns
  if(m_invalid_prev && (appliesTo&AT_PrevLine) ) return;

  // Suppress some messages while still counting them
  m_MsgCount[code]++;
  if( m_MustSkip[code]) {
    m_msg_skipped++;
    return;
  }
  if( m_MaxRepeat>0 && m_MsgCount[code] > m_MaxRepeat) {
    m_MaxRepeatTopped=true;
    m_msg_skipped++;
    return;
  }

  if(appliesTo & AT_PrevLine) {
    // Print the previous line if it was not printed
    if( !m_prev_printed && m_line_prev.size() ) {
      if( !m_two_lines_involved ) cerr << "\n";
      PrintLine(cerr, m_filename_prev, m_line_num_prev, m_line_prev);
    }
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

  if( appliesTo == (AT_PrevLine|AT_ThisLine) ) m_two_lines_involved=true;
}

void CAgpErr::LineDone(const string& s, int line_num, bool invalid_line)
{
  if( m_messages->pcount() ) {
    if( !m_two_lines_involved ) cerr << "\n";

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
  if(invalid_line) {
    m_invalid_prev = true;
    m_lines_skipped++;
  }


  m_two_lines_involved=false;
}

void CAgpErr::StartFile(const string& s)
{
  m_filename_prev=m_filename;
  m_filename=s;
  m_InputFiles.push_back(s);
}

// Initialize m_MustSkip[]
// Return values:
//   ""                          no matches found for str
//   string beginning with "  "  one or more messages that matched
string CAgpErr::SkipMsg(const string& str, bool skip_other)
{
  string res;
  const static char* skipErr  = "Skipping errors, printing warnings.";
  const static char* skipWarn = "Skipping warnings, printing errors.";

  // Keywords: all warn* err*
  int i_from=CODE_Last;
  int i_to  =0;
  if(str=="all") {
    i_from=0; i_to=CODE_Last;
    // "-only all" does not make a lot of sense,
    // but we can support it anyway.
    res = skip_other ? "Printing" : "Skipping";
    res+=" all errors and warnings.";
  }
  else if (str.substr(0,4)=="warn" && str.size()<=8 ) { // warn ings
    i_from=W_First; i_to=W_Last;
    res = skip_other ? skipErr : skipWarn;
  }
  else if (str.substr(0,4)=="err" && str.size()<=6 ) { // err ors
    i_from=E_First; i_to=E_Last;
    res = skip_other ? skipWarn : skipErr;
  }
  if(i_from<i_to) {
    for( int i=i_from; i<i_to; i++ ) m_MustSkip[i] = !skip_other;
    return res;
  }

  // Error or warning codes, substrings of the messages.
  for( int i=E_First; i<CODE_Last; i++ ) {
    bool matchesCode = ( str==GetPrintableCode(i) );
    if( matchesCode || NStr::Find(msg[i], str) != NPOS) {
      m_MustSkip[i] = !skip_other;
      res += "  (";
      res += GetPrintableCode(i);
      res += ") ";
      res += GetMsg((TCode)i);
      res += "\n";
      if(matchesCode) break;
    }
  }

  return res;
}

int CAgpErr::CountTotals(TCode from, TCode to)
{
  if(to==E_First) {
    //// One argument: count errors/warnings/genbank errors/given type
    if     (from==E_Last) { from=E_First; to=E_Last; }
    else if(from==W_Last) { from=W_First; to=W_Last; }
    else if(from==G_Last) { from=G_First; to=G_Last; }
    else if(from<CODE_Last)  return m_MsgCount[from];
    else return -1; // Invalid "from"
  }

  int count=0;
  for(int i=from; i<to; i++) {
    count += m_MsgCount[i];
  }
  return count;
}

void CAgpErr::PrintMessageCounts(CNcbiOstream& ostr, TCode from, TCode to)
{
  if(to==E_First) {
    //// One argument: count errors/warnings/genbank errors/given type
    if     (from==E_Last) { from=E_First; to=E_Last; }
    else if(from==W_Last) { from=W_First; to=W_Last; }
    else if(from==G_Last) { from=G_First; to=G_Last; }
    else if(from<CODE_Last)  { to=(TCode)(from+1); }
    else {
      ostr << "Internal error in CAgpErr::PrintMessageCounts().";
    }
  }
  ostr<< setw(7) << "count" << "  description\n"; // code
  for(int i=from; i<to; i++) {
    if( m_MsgCount[i] ) {
      ostr<< setw(7) << m_MsgCount[i] << "  "
          // << "(" << GetPrintableCode(i) << ") "
          << GetMsg((TCode)i) << "\n";
    }
  }
}

void CAgpErr::PrintTotals(CNcbiOstream& ostr, int e_count, int w_count, int skipped_count)
{
  if     (e_count==0) ostr << "No errors, ";
  else if(e_count==1) ostr << "1 error, "  ;
  else                ostr << e_count << " errors, ";

  if     (w_count==0) ostr << "no warnings";
  else if(w_count==1) ostr << "1 warning";
  else                ostr << w_count << " warnings";

  if(skipped_count) {
    ostr << "; " << skipped_count << " not printed";
  }
}

END_NCBI_SCOPE

