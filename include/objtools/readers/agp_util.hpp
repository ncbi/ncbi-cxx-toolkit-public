#ifndef OBJTOOLS_READERS___AGP_UTIL__HPP
#define OBJTOOLS_READERS___AGP_UTIL__HPP

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
 *   Generic fast AGP stream reader    (CAgpReader),
 *   and even more generic line parser (CAgpRow).
 *   Usage examples in test/agp_*.cpp
 */



#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <map>

BEGIN_NCBI_SCOPE

class CAgpErr; // full definition below

//// A container for both the original string column values (Get*() methods)
//// and the values converted to int, char, bool types (member variables).
//// Detects formatting errors within a single line.
class CAgpRow
{
public:
  // string line; int line_num;  ??
  static CAgpErr* agpErr;

  CAgpRow();

  // Returns:
  //   -1 comment line (to be silently skipped)
  //    0 parsed successfully
  //   >0 an error code (copied to last_error)
  //
  // Sets last_error to the warning code if a warning is generated, but still returns 0.
  // For some errors and warnings, also sets error_details.
  //
  // strips end of line comments from line
  int FromString(const string& line);

  string ToString(); // 9 column tab-separated string without EOL comments

  // Unparsed columns
  SIZE_TYPE pcomment; // NPOS if no comment for this line, 0 if entire line is comment
  vector<string> cols;

  inline string& GetObject       () {return cols[0];} // no corresponding member variable
  inline string& GetObjectBeg    () {return cols[1];}
  inline string& GetObjectEnd    () {return cols[2];}
  inline string& GetPartNumber   () {return cols[3];}
  inline string& GetComponentType() {return cols[4];}

  inline string& GetComponentId  () {return cols[5];}  // no corresponding member variable
  inline string& GetComponentBeg () {return cols[6];}
  inline string& GetComponentEnd () {return cols[7];}
  inline string& GetOrientation  () {return cols[8];}

  inline string& GetGapLength() {return cols[5];}
  inline string& GetGapType  () {return cols[6];}
  inline string& GetLinkage  () {return cols[7];}

  int ParseComponentCols(bool log_errors=true);
  int ParseGapCols(bool log_errors=true);

  //bool has_eol_comment;

  // Parsed columns
  int object_beg, object_end, part_number;
  char component_type;

  bool is_gap;

  int component_beg, component_end;
  char orientation; // + - 0 n (instead of na)

  int gap_length;
  enum TGap{
    GAP_clone          ,
    GAP_fragment       ,
    GAP_repeat         ,

    GAP_contig         ,
    GAP_centromere     ,
    GAP_short_arm      ,
    GAP_heterochromatin,
    GAP_telomere       ,

    GAP_count,
    GAP_yes_count=GAP_repeat+1
  } gap_type;
  bool linkage;

  bool GapEndsScaffold() const
  {
    if(gap_type==GAP_fragment) return false;
    return linkage==false;
  }
  bool GapValidAtObjectEnd() const
  {
    return gap_type==GAP_centromere || gap_type==GAP_telomere || gap_type==GAP_short_arm;
  }

  typedef const char* TStr;
  static const TStr gap_types[GAP_count];
  static map<string, TGap> gap_type_codes;

  static const string& GetErrors();
};

//// Detects scaffolds, object boundaries, errors that involve 2 consequitive lines.
//// Intented as a superclass for more complex readers.
class CAgpReader
{
public:
  CAgpReader();
  virtual ~CAgpReader();

  virtual int ReadStream(CNcbiIstream& is, bool finalize=true);
  virtual int CAgpReader::Finalize(); // by default, invoked automatically at the end of ReadStream()

  bool at_beg;  // at the first valid gap or component line (this_row)
                // prev_row undefined
  bool at_end;  // after the last line; last gap or component in prev_row
                // this_row undefined
  bool row_skipped; // true after a syntax error or E_Obj/CompEndLtBeg, E_ObjRangeNeGap/Comp
                    // (i.e. single-line errors detected by CAgpRow);
                    // not affected by comment lines, even though are skipped, too
  bool new_obj;     // in OnScaffoldEnd(): true if this scaffold ends with an object
                    // (false if there are scaffold-breaking gaps at object end)

  CAgpRow *prev_row;
  CAgpRow *this_row;
  CNcbiIstream* is;
  int line_num, prev_line_num;
  string  line;  // for valid gap/componentr lines, corresponds to this_row
  // We do not save line corresponding to prev_row, to save time.
  // You can use prev_row->ToString(), or save it at the end of OnGapOrComponent():
  //   prev_line=line; // save the previous input line preserving with EOL comments
  //
  // Note that prev_line_num != line_num -1:
  // - after skipped lines (comments or syntax errors)
  // - when reading from multiple files.


  //virtual int ReadLine(const string& str);

  //// Callbacks. Override to implement custom functionality.
  //// Callbacks can use this_row, prev_row and other instance variables.

  // this_row = current gap or component (test this_row->is_gap)
  virtual void OnGapOrComponent(){}

  // prev_row = last line of the scaffold
  virtual void OnScaffoldEnd()  {}

  // unless(at_beg): prev_row = the last  line of the old object
  // unless(at_end): this_row = the first line of the new object
  virtual void OnObjectChange() {}

  virtual bool OnError(int code)
  {
    // code=0: non-fatal error, line saved to this_row; row_skipped=false.
    //     >0: syntax error, row_skipped=true;
    //         this_row most likely incomplete;
    //         prev_row still contains the last known valid line.

    return false; // abort on any error
  }
  virtual void OnComment()       {}

};

//// Programs that use this default error handler
//// are expected to die after a single incorrect line.
//// (agp_validate uses a subclass can better handle
//// errors on multiple lines, in multiple files, etc.)
class CAgpErr
{
public:
  virtual ~CAgpErr() {}

  enum TAppliesTo{
    AT_ThisLine=1,
    //AT_SkipAfterBad=2, // Suppress this error if the previous line was invalid
    AT_PrevLine=4, // use AT_ThisLine|AT_PrevLine when both lines are involved
    AT_None    =8  // Not tied to any specifc line(s) (empty file, possibly taxid errors)
  };

  // Accumulate multiple errors on a single line (messages, messages_apply_to);
  // ignore warnings.
  virtual void Msg(int code, const string& details, int appliesTo=AT_ThisLine);
  inline void Msg(int code, int appliesTo=AT_ThisLine)
  {
    Msg(code, NcbiEmptyString, appliesTo);
  }

  string messages;
  int messages_apply_to; // which lines to print before the message: previous, current, both
  int last_error;
  inline void clear() {last_error=messages_apply_to=0; messages=""; }

  // When adding new errors to enum TCode, also update msg[]
  enum TCode{
    // Errors within one line (detected in CAgpRow)
    E_ColumnCount=1 ,
    E_EmptyColumn,
    E_EmptyLine,
    E_InvalidValue  ,
    E_InvalidYes   ,

    E_MustBePositive,
    E_ObjEndLtBeg ,
    E_CompEndLtBeg,
    E_ObjRangeNeGap ,
    E_ObjRangeNeComp,

    // Other errors, some detected only by agp_validate.
    // We define the codes here to preserve the historical error codes.
    // CAgpRow and CAgpReader no nothing of such errors.
    E_DuplicateObj  ,       // -- agp_validate --
    E_ObjMustBegin1 ,       // CAgpReader
    E_PartNumberNot1,       // CAgpReader
    E_PartNumberNotPlus1,   // CAgpReader
    E_UnknownOrientation,   // -- agp_validate --

    E_ObjBegNePrevEndPlus1, // CAgpReader
    E_NoValidLines,         // CAgpReader
    E_Last, E_First=1, E_LastToSkipLine=E_ObjRangeNeComp,

    // Warnings.
    W_GapObjEnd=21 ,        // CAgpReader
    W_GapObjBegin ,         // CAgpReader
    W_ConseqGaps ,          // CAgpReader
    W_ObjNoComp,            // -- agp_validate --
    W_SpansOverlap,         // -- agp_validate --

    W_SpansOrder ,          // -- agp_validate --
    W_DuplicateComp,        // -- agp_validate --
    W_LooksLikeGap,         // CAgpRow
    W_LooksLikeComp,        // CAgpRow
    W_ExtraTab,             // CAgpRow

    W_GapLineMissingCol9,   // CAgpRow
    W_NoEolAtEof,           // CAgpReader
    W_Last, W_First = 21,


  };

  typedef const char* TStr;
  static const TStr msg[];
  static const char* GetMsg(int code);
  static string FormatMessage(int code, const string& details);
};


END_NCBI_SCOPE
#endif /* OBJTOOLS_READERS___AGP_UTIL__HPP */

