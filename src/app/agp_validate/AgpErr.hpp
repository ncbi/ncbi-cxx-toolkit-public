#ifndef AGP_VALIDATE_AgpErr
#define AGP_VALIDATE_AgpErr

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

BEGIN_NCBI_SCOPE

// Print errors for this and previous lines in the format:
//   [filename:]linenum:agp lline
//           WARNING: text
//           ERROR: text
// Skip selected and repetitive errors; count skipped errors.

/* Basic usage:
   CAgpErr agpErr;

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
       if(error_in_the_previous_line) agpErr.Msg(code, "", CAgpErr::AT_PrevLine);
       if(error_in_this_line        ) agpErr.Msg(code, "", CAgpErr::AT_ThisLine);
       if(error_in_the_last_pair_of_lines) // E.g. 2 conseq. gap lines
         agpErr.Msg(code, "", CAgpErr::AT_ThisLine|CAgpErr::AT_PrevLine);
       . . .
     }
   }
*/

enum TAppliesTo{
  //AT_Unknown=0, -- just print the message without the content line
  AT_ThisLine=1, // Accumulate messages
  AT_PrevLine=2  // Print the previous line if it was not printed;
                  // print the message now
  // AT_ThisLine|AT_PrevLine -- both lines are involved:
  // 1) print the previous line now (if it was not printed already)
  // 2) suppress the error message if the previous line was so bad
  //    that it did not even reach the syntax validator (wrong # of columns)
};

class CAgpErr
{
public:

  // When adding entries to enum TCode, also update msg[]
  enum TCode {
    E_ColumnCount=1 ,
    E_EmptyColumn,
    E_DuplicateObj  ,
    E_ObjMustBegin1 ,
    E_PartNumberNot1,

    E_PartNumberNotPlus1,
    E_ObjRangeNeGap ,
    E_ObjRangeNeComp,
    E_UnknownOrientation,
    E_MustBePositive,

    E_ObjBegNePrevEndPlus1,
    E_ObjEndLtBegin ,
    E_CompEndLtBeg,
    E_InvalidValue  ,
    E_InvalidYes   ,

    E_EmptyLine,
    E_Last, E_First=1,

    W_GapObjEnd=21 ,
    W_GapObjBegin ,
    W_ConseqGaps ,
    W_ObjNoComp,
    W_SpansOverlap,

    W_SpansOrder ,
    W_DuplicateComp,
    W_LooksLikeGap,
    W_LooksLikeComp,
    W_ExtraTab,

    W_GapLineMissingCol9,
    W_NoEolAtEof,
    W_Last, W_First = 21,

    G_InvalidCompId=41,
    G_NotInGenbank,
    G_NeedVersion,
    G_CompEndGtLength,
    G_TaxError,

    G_Last,
    G_First = G_InvalidCompId,

    // G_NoTaxid,
    // G_NoOrgRef,
    // G_AboveSpeciesLevel,

    CODE_First=1, CODE_Last=G_Last
  };

  static const char* GetMsg(TCode code);
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
  static void PrintMessage(CNcbiOstream& ostr, TCode code,
    const string& details=NcbiEmptyString,
    const string& substX=NcbiEmptyString);

  // Construct a readable message on total error & warning counts
  static void PrintTotals(CNcbiOstream& ostr, int e_count, int w_count, int skipped_count=0);

  CAgpErr();

  // Can skip unwanted messages, record a message for printing (AT_ThisLine),
  // print it immediately if it applies to the previous line (AT_PrevLine),
  // print the previous line and record the message
  // if it applies to the 2 last lines (AT_PrevLine|AT_ThisLine).
  //
  // The resulting output format shines when:
  // 1)there are multiple errors in one line:
  //   file:linenum:agp content
  //        MSG1
  //        MSG2
  // 2)there is an error that involves 2 lines:
  //   file1:linenum1:agp content1
  //   file2:linenum2:agp content2
  //        MSG
  // It should still be acceptable if both 1 and 2 are true.
  //
  // When the message applies to 2 non-consequitive lines
  // (e.g. intersecting component spans):
  //   // Print the first line involved
  //   if( !agpErr.MustSkip(code) ) CAgpErr::PrintLine(...);
  //   // Print the current line, then the message
  //   agpErr.Msg(..AT_ThisLine..);
  //
  // The worst mixup could happen when there are 2 errors involving
  // different pairs of lines, typically: (N-1, N) (N-M, N)
  // To do: check if this is possible in the current SyntaxValidator

  void Msg(TCode code, const string& details=NcbiEmptyString,
    int appliesTo=AT_ThisLine, const string& substX=NcbiEmptyString);

  // Print any accumulated messages.
  // invalid_line=true: for the next line, suppress
  //   CAgpErr::AT_ThisLine|CAgpErr::AT_PrevLine messages
  void LineDone(const string& s, int line_num, bool invalid_line=false);

  // No invocations are needed when reading from cin,
  // or when reading only one file. For multiple files,
  // invoke with the next filename prior to reading it.
  void StartFile(const string& s);

  // 'fgrep' errors out, or keep just the given errors (skip_other=true)
  // Can also include/exclude by code -- see GetPrintableCode().
  // Return values:
  //   ""                          no matches found for str
  //   string beginning with "  "  one or more messages that matched
  //   else                        printable message
  // Note: call SkipMsg("all") nefore SkipMsg(smth, true)
  string SkipMsg(const string& str, bool skip_other=false);

  bool MustSkip(TCode code);

  // To do: some function to print individual error counts
  //        (important if we skipped some errors)

  // One argument:
  //   E_Last: count errors  W_Last: count warnings
  //   G_Last: count GenBank errors
  //   other: errors/warnings of one given type
  // Two arguments: range of TCode-s
  int CountTotals(TCode from, TCode to=E_First);
  void PrintMessageCounts(CNcbiOstream& ostr, TCode from, TCode to=E_First);

private:
  typedef const char* TStr;
  static const TStr msg[];

  // Total # of errors for each type, including skipped ones.
  int m_MsgCount[CODE_Last];
  bool m_MustSkip[CODE_Last];

  string m_filename_prev;
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
  // (We print right away messages that apply only to the previous line.)
  CNcbiOstrstream* m_messages;
  string m_filename;
  int m_line_num;

  // For reporting the lines we passed a long time ago
  // (intersecting component spans, duplicate objects, etc)
  vector<string> m_InputFiles;

public:
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

#endif /* AGP_VALIDATE_AgpErr */

