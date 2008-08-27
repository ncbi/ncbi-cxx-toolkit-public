#ifndef AGP_VALIDATE_AgpErrEx
#define AGP_VALIDATE_AgpErrEx

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

#include <corelib/ncbistd.hpp>
#include <iostream>
#include <objtools/readers/agp_util.hpp>

BEGIN_NCBI_SCOPE

// Print errors for this and previous lines in the format:
//   [filename:]linenum:agp lline
//           WARNING: text
//           ERROR: text
// Skip selected and repetitive errors; count skipped errors.

/* Basic usage:
   CAgpErrEx agpErr;

   // Apply options (normally specified in the command-line)
   agpErr.m_MaxRepeat = 0; // Print all
   agpErr.SkipMsg("warn"); // Skip warnings

   // Loop files
   for(...) {
     // Do not call if reading stdin, or only one file
     agpErr.StartFile(filename);
     // Loop lines
     for(...) {
       . . .
       // The order of the calls below is not very significant:
       if(error_in_the_previous_line) agpErr.Msg(code, "", CAgpErrEx::CAgpErr::fAtPrevLine);
       if(error_in_this_line        ) agpErr.Msg(code, "", CAgpErrEx::CAgpErr::fAtThisLine);
       if(error_in_the_last_pair_of_lines) // E.g. 2 conseq. gap lines
         agpErr.Msg(code, "", CAgpErrEx::CAgpErr::fAtThisLine|CAgpErrEx::CAgpErr::fAtPrevLine);
       . . .
     }
   }
*/

/*
enum TAppliesTo{
  //CAgpErr::fAtUnknown=0, -- just print the message without the content line
  CAgpErr::fAtThisLine=1,     // Accumulate messages
  CAgpErrEx::fAtSkipAfterBad=2, // Suppress this error if the previous line was invalid
                     // (e.g. had wrong # of columns)
  CAgpErr::fAtPrevLine    =4  // Print the previous line if it was not printed;
                     // print the message now
  // CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine -- both lines are involved:
  // print the previous line now (if it was not printed already)
};
*/

class CAgpErrEx : public CAgpErr
{
public:
  // When adding entries to this enum, also update msg_ex[]
  enum {
    G_InvalidCompId=41,
    G_NotInGenbank,
    G_NeedVersion,
    G_CompEndGtLength,
    G_DataError,

    G_TaxError,

    G_Last,
    G_First = G_InvalidCompId,

    // G_NoTaxid,
    // G_NoOrgRef,
    // G_AboveSpeciesLevel,

    CODE_First=1, CODE_Last=G_Last
  };

  // Must be dropped after switch to CAgpReader -
  // skipping conditionals are checked outside of CAgpErr.
  enum EAppliesToEx{
    fAtSkipAfterBad=2 // Suppress this error if the previous line was invalid
  };

  static const char* GetMsgEx(int code);
  static string GetPrintableCode(int code); // Returns a string like e01 w12
  static void PrintAllMessages(CNcbiOstream& out);


  // The max number of times to print a given error message.
  int m_MaxRepeat;
  // If this turns true, we can inform user about -limit 0 option
  bool m_MaxRepeatTopped;

  // Warnings + errors skipped,
  // either because of m_MaxRepeat or MustSkip().
  int m_msg_skipped;
  int m_lines_skipped;


  // Print the source line along with filename and line number.
  static void PrintLine   (CNcbiOstream& ostr,
    const string& filename, int linenum, const string& content);

  // Print the message by code, prepended by \tERROR: or \tWARNING:
  // details: append at the end of the message
  // substX : substitute it instead of the word "X" in msg[code]
  static void PrintMessage(CNcbiOstream& ostr, int code,
    const string& details=NcbiEmptyString);

  // Construct a readable message on total error & warning counts
  static void PrintTotals(CNcbiOstream& ostr, int e_count, int w_count, int skipped_count=0);

  CAgpErrEx();

  // Can skip unwanted messages, record a message for printing (CAgpErr::fAtThisLine),
  // print it immediately if it applies to the previous line (CAgpErr::fAtPrevLine),
  // print the previous line and record the message
  // if it applies to the 2 last lines (CAgpErr::fAtPrevLine|CAgpErr::fAtThisLine).
  //
  // The resulting output format works well when:
  // 1)there are multiple errors in one line:
  //   file:linenum:agp content
  //        MSG1
  //        MSG2
  // 2)there is an error that involves 2 lines:
  //   file1:linenum1:agp content1
  //   file2:linenum2:agp content2
  //        MSG
  // The format is still acceptable if both 1 and 2 are true.
  //
  // When the message applies to 2 non-consequitive lines
  // (e.g. intersecting component spans), we do not print the first line involved.
  // This is still possible to do as follows:
  //   // Print the first line involved
  //   if( !agpErr.MustSkip(code) ) CAgpErrEx::PrintLine(...);
  //   // Print the current line, then the message
  //   agpErr.Msg(..CAgpErr::fAtThisLine..);
  //
  // The worst could happen when there are 2 errors involving
  // different pairs of lines: (N-1, N) (N-M, N)

  // we override Msg() that comes from CAgpErr
  virtual void Msg(int code, const string& details, int appliesTo=fAtThisLine);
  virtual void Msg(int code, int appliesTo=fAtThisLine)
  {
    Msg(code, NcbiEmptyString, appliesTo);
  }

  // Print any accumulated messages.
  // invalid_line=true: for the next line, suppress
  //   CAgpErrEx::CAgpErr::fAtThisLine|CAgpErrEx::CAgpErr::fAtPrevLine messages
  void LineDone(const string& s, int line_num, bool invalid_line=false);

  // No invocations are needed when reading from cin,
  // or when reading only one file. For multiple files,
  // invoke with the next filename prior to reading it.
  void StartFile(const string& s);

  // 'fgrep' errors out, or keep only the given errors (skip_other=true)
  // Can include/exclude by code (see GetPrintableCode()), or by substring.
  // Return values:
  //   ""                          no matches found for str
  //   string beginning with "  "  one or more messages that matched
  //   else                        printable [error] message
  // Note: call SkipMsg("all") before SkipMsg(smth, true)
  string SkipMsg(const string& str, bool skip_other=false);
  void SkipMsg(int code, bool skip_other=false)
  {
    if(code>=E_First && code<CODE_Last) m_MustSkip[code] = !skip_other;
  }

  bool MustSkip(int code);

  // 1 argument:
  //   E_Last: count errors
  //   W_Last: count warnings
  //   G_Last: count GenBank errors
  //   other: errors/warnings of one given type
  // 2 arguments: range of int-s
  int CountTotals(int from, int to=E_First);

  // Print individual error counts (important if we skipped some errors)
  void PrintMessageCounts(CNcbiOstream& ostr, int from, int to=E_First);

private:
  typedef const char* TStr;
  static const TStr msg_ex[];

  // Total # of errors for each type, including skipped ones.
  int m_MsgCount[CODE_Last];
  bool m_MustSkip[CODE_Last];

  //string m_filename_prev;
  int m_filenum_prev;
  // Not m_line_num-1 when the previous line:
  // - was in the different file;
  // - was a skipped comment line.
  string m_line_prev;
  int  m_line_num_prev;
  bool m_prev_printed;    // true: previous line was already printed
                          // (probably had another error);
                          // no need to-reprint "fname:linenum:content"
  bool m_two_lines_involved; // true: do not print "\n" after the previous line
  bool m_invalid_prev;       // true: suppress errors concerning the previous line

  // a stream to Accumulate messages for the current line.
  // (We immediately print messages that apply only to the previous line.)
  string m_filename;
  int m_line_num;

  // For reporting filenames of the lines we passed a long time ago
  // (intersecting component spans, duplicate objects, etc)
  vector<string> m_InputFiles;

public:
  // m_messages is public because:
  // Genbank validator may stow away the syntax errors for the current line
  // while it processes a batch of preceding lines without syntax errors;
  // afterwards, it can put the stowed m_messages back, and print them in the
  // correct place, following any Genbank validation errors for the batch.
  //   CNcbiOstrstream* tmp = agpErr.m_messages;
  //   agpErr.m_messages =  new CNcbiOstrstream();
  //   << process a batch of preceding lines >>
  //   agpErr.m_messages = tmp;
  //   agpErr.LineDone(line_orig, line_num, true);
  CNcbiOstrstream* m_messages;

  // 0: reading from cin or from a single file
  int GetFileNum()
  {
    return m_InputFiles.size();
  }

  const string& GetFile(int num)
  {
    return m_InputFiles[num-1];
  }
};

END_NCBI_SCOPE

#endif /* AGP_VALIDATE_AgpErrEx */

