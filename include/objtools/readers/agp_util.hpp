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
 *     Generic fast AGP stream reader    (CAgpReader),
 *     and even more generic line parser (CAgpRow).
 *     Example: test/agp_*.cpp
 *
 *     Accession naming patterns (CAccPatternCounter).
 *     Find ranges for consequtive digits. Not related to the above.
 *     Sample input : AC123.1 AC456.1 AC789.1 NC8967.4 NC8967.5
 *                      ^^^ ^                   ^^^^ ^
 *     Sample output: AC[123..789].1 3  NC8967.[4,5] 2
 *     Example: test/pacc.cpp
 */



#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimtx.hpp>
#include <map>

BEGIN_NCBI_SCOPE

class CAgpErr; // full definition below

/// A container for both the original string column values (Get*() methods)
/// and the values converted to int, char, bool types (member variables).
/// Detects formatting errors within a single line, checks that
/// object range length equals gap length or component range length.
class NCBI_XOBJREAD_EXPORT CAgpRow
{
public:
    CAgpRow(CAgpErr* arg);
    CAgpRow(); // constructs a default error handler
    ~CAgpRow();

    // Returns:
    //   -1 comment line (to be silently skipped)
    //    0 parsed successfully
    //   >0 an error code (copied to last_error)
    int FromString(const string& line);
    string GetErrorMessage();
    string ToString(); // 9 column tab-separated string without EOL comments

    //// Unparsed columns
    SIZE_TYPE pcomment; // NPOS if no comment for this line, 0 if the entire line is comment
    vector<string> cols;

    string& GetObject       () {return cols[0];} // no corresponding member variable
    string& GetObjectBeg    () {return cols[1];}
    string& GetObjectEnd    () {return cols[2];}
    string& GetPartNumber   () {return cols[3];}
    string& GetComponentType() {return cols[4];}

    string& GetComponentId  () {return cols[5];}  // no corresponding member variable
    string& GetComponentBeg () {return cols[6];}
    string& GetComponentEnd () {return cols[7];}
    string& GetOrientation  () {return cols[8];}

    string& GetGapLength() {return cols[5];}
    string& GetGapType  () {return cols[6];}
    string& GetLinkage  () {return cols[7];}

    //// Parsed columns
    int object_beg, object_end, part_number;
    char component_type;

    bool is_gap;

    int component_beg, component_end;
    char orientation; // + - 0 n (instead of na)

    int gap_length;
    enum EGap{
        eGapClone          ,
        eGapFragment       ,
        eGapRepeat         ,

        eGapContig         ,
        eGapCentromere     ,
        eGapShort_arm      ,
        eGapHeterochromatin,
        eGapTelomere       ,

        eGapCount,
        eGapYes_count=eGapRepeat+1
    } gap_type;
    bool linkage;

    static bool IsGap(char c)
    {
        return c=='N' || c=='U';
    }
    static bool IsDraftComponent(char c)
    {
        // Active finishing, Draft HTG, Pre draft
        return c=='A' || c=='D' || c=='P';
    }
    static bool GapValidAtObjectEnd(EGap gap_type)
    {
        return gap_type==eGapCentromere || gap_type==eGapTelomere || gap_type==eGapShort_arm;
    }


    bool IsGap() const
    {
      return is_gap;
    }
    bool IsDraftComponent()
    {
        return IsDraftComponent(component_type);
    }

    bool GapEndsScaffold() const
    {
        if(gap_type==eGapFragment) return false;
        return linkage==false;
    }
    bool GapValidAtObjectEnd() const
    {
        //return gap_type==eGapCentromere || gap_type==eGapTelomere || gap_type==eGapShort_arm;
        return GapValidAtObjectEnd(gap_type);
    }

protected:
    int ParseComponentCols(bool log_errors=true);
    int ParseGapCols(bool log_errors=true);

    typedef const char* TStr;
    static const TStr gap_types[eGapCount];

    typedef map<string, EGap> TMapStrEGap;
    static TMapStrEGap* gap_type_codes;

private:
    CAgpErr* m_AgpErr;
    bool m_OwnAgpErr;
    // for initializing gap_type_codes:
    DECLARE_CLASS_STATIC_FAST_MUTEX(init_mutex);
    static void StaticInit();

public:
    CAgpErr* GetErrorHandler() { return m_AgpErr; }
    void SetErrorHandler(CAgpErr* arg);

    static const char* GapTypeToString(int i)
    {
      if(i<0 || i>=eGapCount) return NcbiEmptyCStr;
      return gap_types[i];
    }
};

/// Detects scaffolds, object boundaries, errors that involve 2 consequitive lines.
/// Intented as a superclass for more complex readers.
class NCBI_XOBJREAD_EXPORT CAgpReader
{
public:
    CAgpReader(CAgpErr* arg);
    CAgpReader(); // constructs a default error handler for this object instance
    virtual ~CAgpReader();

    virtual int ReadStream(CNcbiIstream& is, bool finalize=true);
    virtual int Finalize(); // by default, invoked automatically at the end of ReadStream()

    // Print one or two source line(s) on which the error occured,
    // along with error message(s).
    // Source line are preceded with "filename:", if not empty
    // (useful when reading several files).
    virtual string GetErrorMessage(const string& filename=NcbiEmptyString);

    /// Invoked from ReadStream(), after the row has been parsed.
    /// Seldom needs to be invoked by user.
    /// One occassion is in agp_renumber - to force a line NOT to be skipped
    /// it is called from OnError() when m_line_skipped=true
    /// and m_error_code=E_ObjRangeNeGap or E_ObjRangeNeComp.
    bool ProcessThisRow();

protected:
    bool m_at_beg;  // m_this_row is the first valid component or gap line;
                    // m_prev_row undefined.
    bool m_at_end;  // after the last line; could be true only in OnScaffoldEnd(), OnObjectChange().
                    // m_prev_row is the last component or gap line;
                    // m_this_row undefined.

    bool m_line_skipped;      // true after a syntax error or E_Obj/CompEndLtBeg, E_ObjRangeNeGap/Comp
    bool m_prev_line_skipped; // (i.e. single-line errors detected by CAgpRow);
                              // Not affected by comment lines, even though these are skipped, too.

    bool m_new_obj;   // For OnScaffoldEnd(), true if this scaffold ends with an object.
                      // (false if there are scaffold-breaking gaps at object end)
    int m_error_code; // Set to a positive value to trigger OnError().
                      // Set to -1 to graciously stop reading of the stream midway:
                      // void OnGapOrComponent()
                      // {
                      //     if(need to stop && m_error_code==0) m_error_code=-1;
                      // }

    CAgpRow *m_prev_row;
    CAgpRow *m_this_row;
    int m_line_num, m_prev_line_num;
    string  m_line;  // for valid gap/componentr lines, corresponds to this_row
    // To save time, we do not keep the line corresponding to m_prev_row.
    // You can use m_prev_row->ToString(), or save it at the end of OnGapOrComponent():
    //   m_prev_line=m_line; // preserves EOL comments
    //
    // Note that m_prev_line_num != m_line_num - 1:
    // - after skipped lines (syntax errors or comments)
    // - when reading from multiple files without Finalize().

    //// Callbacks, in the order of invocation.
    //// Override to implement custom functionality.
    //// Callbacks can read m_this_row, m_prev_row and all other instance variables.

    virtual void OnScaffoldEnd()
    {
        // m_prev_row = the last line of the scaffold --
        // usually component, but could be non-breaking gap (which generates a warning)
    }

    virtual void OnObjectChange()
    {
        // unless(at_beg): m_prev_row = the last  line of the old object
        // unless(at_end): m_this_row = the first line of the new object
    }

    virtual void OnGapOrComponent()
    {
        // m_this_row = current gap or component (check with m_this_row->IsGap())
    }

    virtual bool OnError()
    {
        // in m_line_skipped :
        // false: Non-fatal error, line saved to m_this_row.
        //        All appropriate preceding callbacks were called before OnError() for this line.
        //        They can check m_error_code, if they need to (not very likely).
        // true  : Syntax error; m_this_row most likely incomplete.
        //         (or complete but inconsistent for E_ObjRangeNeGap, E_ObjRangeNeComp)
        //         No other callbacks invoked for this line.
        //         On the next iteration, m_prev_row will retain the last known valid line.
        // out m_line_skipped: copied to m_prev_line_skipped;
        //         set to true to enable the checks that require 2 lines.

        return false; // abort on any error
    }

    virtual void OnComment()
    {
        // Line that starts with "#".
        // No other callbacks invoked for this line.
    }

private:
    CAgpErr* m_AgpErr; // Error handler
    bool m_OwnAgpErr;
    void Init();

public:
    CAgpErr* GetErrorHandler() { return m_AgpErr; }
    void SetErrorHandler(CAgpErr* arg);
};


class NCBI_XOBJREAD_EXPORT CAgpErr
{
public:
    virtual ~CAgpErr() {}

    enum EAppliesTo{
      fAtThisLine=1,
      //fAtSkipAfterBad=2, // Suppress this error if the previous line was invalid
      fAtPrevLine=4, // use fAtThisLine|fAtPrevLine when both lines are involved
      fAtNone    =8  // Not tied to any specifc line(s) (empty file; possibly, taxid errors)
    };

    // This implementation accumulates multiple errors separately for
    // the current and the previous lines, ignores warnings.
    virtual void Msg(int code, const string& details, int appliesTo=fAtThisLine);
    virtual void Msg(int code, int appliesTo=fAtThisLine)
    {
      Msg(code, NcbiEmptyString, appliesTo);
    }
    void Clear();

    // The following 2 methods are needed for CAgpRow/CAgpReader::GetErrorMessage()
    virtual string GetErrorMessage(int mask=0xFFFFFFFF);
    virtual int AppliesTo(int mask=0xFFFFFFFF);

    // When adding new errors to this enum, also update s_msg[]
    enum {
        // Errors within one line (detected in CAgpRow)
        E_ColumnCount=1 ,
        E_EmptyColumn   ,
        E_EmptyLine     ,
        E_InvalidValue  ,
        E_InvalidYes    ,

        E_MustBePositive,
        E_ObjEndLtBeg   ,
        E_CompEndLtBeg  ,
        E_ObjRangeNeGap ,
        E_ObjRangeNeComp,

        // Other errors, some detected only by agp_validate.
        // We define the codes here to preserve the historical error codes.
        // CAgpRow and CAgpReader do not know of such errors.
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
        W_GapLineIgnoredCol9,   // CAgpRow
        W_CompIsWgsTypeIsNot,   // -- agp_validate --
        W_CompIsNotWgsTypeIs,   // -- agp_validate --
        W_Last, W_First = 21
    };

    static const char* GetMsg(int code);

protected:
    typedef const char* TStr;
    static const TStr s_msg[];
    static string FormatMessage(const string& msg, const string& details);

    string m_messages;
    string m_messages_prev_line; // Messages that apply ONLY to the previous line;
                                 // relatively uncommon - most that apply to previous
                                 // also apply to current, and are saved in m_messages instead.

    int m_apply_to; // which lines to print before the message(s): previous, current, both, whole file
};


class CPatternStats; // internal for CAccPatternCounter
/// Accession naming patterns; find ranges for consequtive digits.
/// Sample input : AC123.1 AC456.1 AC789.1 NC8967.4 NC8967.5
///                  ^^^ ^                   ^^^^ ^
/// Sample output: AC[123..789].1 3  NC8967.[4,5] 2
class NCBI_XOBJREAD_EXPORT CAccPatternCounter : public map<string, CPatternStats*>
{
public:
    void AddName(const string& name);

    // Replace "0"s in a simple pattern like BCM_Spur_v0.0_Scaffold0
    // (which is a key in this map) with real numbers or [ranges].
    static string GetExpandedPattern(value_type* p);
    static int GetCount(value_type* p);

    // Export expanded patterns sorted by accession count.
    typedef multimap<int,string> TMapCountToString;
    void GetSortedPatterns(TMapCountToString& dst);

    ~CAccPatternCounter();
};

END_NCBI_SCOPE

#endif /* OBJTOOLS_READERS___AGP_UTIL__HPP */
