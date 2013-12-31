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
#include <corelib/ncbi_safe_static.hpp>
#include <map>

BEGIN_NCBI_SCOPE

// As time goes on, we should remove support for older versions.
enum EAgpVersion {
    eAgpVersion_auto, ///< auto-detect using the first gap line
    eAgpVersion_1_1,  ///< AGP spec 1.1
    eAgpVersion_2_0   ///< AGP spec 2.0
};

class CAgpErr; // full definition below
class CAgpReader; // full definition below

/// A container for both the original string column values (Get*() methods)
/// and the values converted to int, char, bool types (member variables).
/// Detects formatting errors within a single line, checks that
/// object range length equals gap length or component range length.
class NCBI_XOBJREAD_EXPORT CAgpRow : public CObject
{
protected:
    // constructors are protected to prevent users from creating
    // instances on the stack

    // reader argument is used for notification of version auto-detection via SetVersion()
    CAgpRow(CAgpErr* arg, EAgpVersion agp_version = eAgpVersion_auto,
        CAgpReader* reader = NULL );
    // constructs a default error handler
    CAgpRow(EAgpVersion agp_version = eAgpVersion_auto,
        CAgpReader* reader = NULL);

    CAgpRow(const CAgpRow &);

public:

    virtual ~CAgpRow();

    static CRef<CAgpRow> New(
        CAgpErr* arg, EAgpVersion agp_version = eAgpVersion_auto,
        CAgpReader* reader=NULL)
    {
        return Ref(new CAgpRow(arg, agp_version, reader));
    }

    static CRef<CAgpRow> New(EAgpVersion agp_version = eAgpVersion_auto,
        CAgpReader* reader=NULL)
    {
        return Ref( new CAgpRow(agp_version, reader) );
    }

    CRef<CAgpRow> Clone(void) const {
        return Ref( new CAgpRow(*this) );
    }

    // Returns:
    //   -1 comment line (to be silently skipped)
    //    0 parsed successfully
    //   >0 an error code (copied to last_error)
    int FromString(const string& line);
    // Generates G_CompEndGtLength message and returns false on error.
    string GetErrorMessage();
    string ToString(bool reorder_linkage_evidences=false); // 9 column tab-separated string without EOL comments

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
    string& GetOrientation  ()
    {
        if(cols.size()==8) {
          // prevent out-of-range accesses in GetLinkageEvidence() and GetOrientation()
          cols.push_back(NcbiEmptyString);
        }

        return cols[8];
    }

    string& GetGapLength()        {return cols[5];}
    string& GetGapType  ()        {return cols[6];}
    string& GetLinkage  ()        {return cols[7];}
    string& GetLinkageEvidence () // AGP v. 2.0
    {
        if(cols.size()==8) {
          // prevent out-of-range accesses in GetLinkageEvidence() and GetOrientation()
          cols.push_back(NcbiEmptyString);
        }

        return cols[8];
    }

    //// Parsed columns
    int object_beg, object_end, part_number;
    char component_type;

    bool is_gap;

    int component_beg, component_end;
    enum EOrientation {
        // numeric values of the enum are equal to ASCII values of
        // AGP 1.1's values for
        // backward compatibility.  Intentionally obfuscated to discourage
        // you from using the underlying char representation and because
        // it doesn't match later versions. (e.g. unknown char is now '?' in
        // AGP 2.0, etc.)
        eOrientationPlus =         43,
        eOrientationMinus =        45,
        eOrientationUnknown =      48,
        eOrientationIrrelevant =  110
    };
    EOrientation orientation;

    int gap_length;
    // if you update this enum, make sure to update CAgpRow::gap_types
    enum EGap{
        eGapClone          , // AGP 1.1 only
        eGapFragment       , // AGP 1.1 only
        eGapRepeat         ,
        eGapScaffold       , // AGP 2.0 only

        eGapContig         ,
        eGapCentromere     ,
        eGapShort_arm      ,
        eGapHeterochromatin,
        eGapTelomere       ,

        eGapCount,
        eGapYes_count=eGapScaffold+1
    };
    EGap gap_type;
    bool linkage;

    enum ELinkageEvidence {
        fLinkageEvidence_paired_ends  = (1 << 0),
        fLinkageEvidence_align_genus  = (1 << 1),
        fLinkageEvidence_align_xgenus = (1 << 2),
        fLinkageEvidence_align_trnscpt= (1 << 3),
        fLinkageEvidence_within_clone = (1 << 4),
        fLinkageEvidence_clone_contig = (1 << 5),
        fLinkageEvidence_map          = (1 << 6),
        fLinkageEvidence_strobe       = (1 << 7),
        fLinkageEvidence_pcr          = (1 << 8),
        // update fLinkageEvidence_HIGHEST_BIT_MASK if more added
        fLinkageEvidence_HIGHEST_BIT_MASK = fLinkageEvidence_pcr,

        fLinkageEvidence_unspecified  = 0,
        fLinkageEvidence_INVALID = -1,
        fLinkageEvidence_na = -2
    };
    /// Might have duplicates, and is empty on error or
    /// if there are no actual linkage evidences (e.g. "na" or "unspecified")
    /// It does NOT hold unusual values like fLinkageEvidence_INVALID.
    typedef vector<ELinkageEvidence> TLinkageEvidenceVec;
    TLinkageEvidenceVec linkage_evidences;
    /// a bit map which holds summary of info in linkage_evidences.
    /// In particular, this is useful for weeding out duplicates
    int linkage_evidence_flags;

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
        return gap_type==eGapCentromere || gap_type==eGapTelomere ||
               gap_type==eGapShort_arm  || gap_type==eGapHeterochromatin;
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
        // m_agp_version == eAgpVersion_1_1 &&
        if( gap_type==eGapFragment) return false;
        return linkage==false;
    }
    bool GapValidAtObjectEnd() const
    {
        //return gap_type==eGapCentromere || gap_type==eGapTelomere || gap_type==eGapShort_arm;
        return GapValidAtObjectEnd(gap_type);
    }


    static bool CheckComponentEnd(const string& comp_id, int comp_end, int comp_len,
        CAgpErr& agp_err);
    bool CheckComponentEnd(int comp_len) {
        return CheckComponentEnd(GetComponentId(), component_end, comp_len, *m_AgpErr);
    }

    // terms in the original order
    string LinkageEvidencesToString(void);
    // terms in the preferred order
    static string LinkageEvidenceFlagsToString(int le);
    string LinkageEvidenceFlagsToString()
    {
        return LinkageEvidenceFlagsToString(linkage_evidence_flags);
    }
    static const char* le_str(ELinkageEvidence le);
    static int str_to_le(const string& str);
    /** Returns a string describing suggested replacement, or "" if none is needed.
    Use do_subst=true to do the actial substitution in this object.
    */
    string SubstOldGap(bool do_subst);

    string OrientationToString( EOrientation orientation );

    virtual void SetVersion(EAgpVersion ver);

protected:
    int ParseComponentCols(bool log_errors=true);
    int ParseGapCols(bool log_errors=true);

    typedef const char* TStr;
    static const TStr gap_types[eGapCount];

    typedef map<string, EGap> TMapStrEGap;
    static CSafeStatic<TMapStrEGap> gap_type_codes;

    static TMapStrEGap * gap_type_codes_creator(void);

private:
    EAgpVersion m_agp_version;

    // raw pointer because we *never* own this.
    CAgpReader* m_reader;

    CRef<CAgpErr> m_AgpErr;

public:
    CAgpErr* GetErrorHandler() { return m_AgpErr; }
    void SetErrorHandler(CAgpErr* arg);

    static const char* GapTypeToString(int i)
    {
      if(i<0 || i>=eGapCount) return NcbiEmptyCStr;
      return gap_types[i];
    }

    EAgpVersion GetVersion()
    {
        return m_agp_version;
    }
};

/// Detects scaffolds, object boundaries, errors that involve 2 consecutive lines,
/// and is intended as a superclass for more complex readers.
class NCBI_XOBJREAD_EXPORT CAgpReader
{
public:
    CAgpReader(CAgpErr* arg,
        EAgpVersion agp_version = eAgpVersion_auto );
    // constructs a default error handler for this object instance
    CAgpReader(EAgpVersion agp_version = eAgpVersion_auto );
    virtual ~CAgpReader();

    /// Whether or not the function should call Finalize() when it's done successfully
    enum EFinalize {
        eFinalize_No,
        eFinalize_Yes
    };
    /// Read an AGP file from the given input stream.
    ///
    /// @param is
    ///   The input stream we're reading the AGP from
    /// @param eFinalize
    ///   If we successfully read the AGP file, this param determines
    ///   whether or not we call Finalize().
    /// @return
    ///   Returns the error code, where 0 means "okay".  Non-zero error
    ///   codes are usually values from the unnamed enum in CAgpErr
    ///   that has E_ColumnCount, E_EmptyColumn, etc., but subclasses
    ///   can have their own custom error codes, so other values
    ///   can appear.
    virtual int ReadStream(CNcbiIstream& is, EFinalize eFinalize = eFinalize_Yes );

    /// Deprecated backward-compatibility wrapper.
    /// @deprecated
    NCBI_DEPRECATED
    int ReadStream(CNcbiIstream& is, bool bFinalize)
    {
        return ReadStream(is, (bFinalize ? eFinalize_Yes : eFinalize_No) );
    }

    /// This is called at the end of the file, usually automatically but can be called manually if
    /// the automatic call was intentionally disabled.
    ///
    /// @return
    ///  Returns error code on whether the "Finalize" operation succeeded, where 0 means "okay".
    virtual int Finalize(); // by default, invoked automatically at the end of ReadStream()

    /// Return a string with one (or two, depending on error) source line(s)
    /// on which the error occured, along with the error message(s) themselves.
    ///
    /// @param filename
    ///   If filename is non-empty, each line is preceded with "FILENAME:"
    ///   (useful when reading several files).
    virtual string GetErrorMessage(const string& filename=NcbiEmptyString);

    /// Invoked from ReadStream(), after the row has been parsed, and
    /// seldom needs to be invoked by user.
    /// One occassion is in agp_renumber - to force a line NOT to be skipped
    /// it is called from OnError() when m_line_skipped=true
    /// and m_error_code=E_ObjRangeNeGap or E_ObjRangeNeComp.
    bool ProcessThisRow();

    /// Change what AGP version to use for the next input that's read.
    /// Please prefer setting the version in the CAgpReader constructor instead
    /// and if SetVersion must be used, it's best to use it at a clean stopping
    /// point.  (Example: immediately after a Finalize() call)
    virtual void SetVersion(EAgpVersion ver);

protected:
    EAgpVersion m_agp_version;

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
    bool m_content_line_seen; // True after we see and process a line that is
                              // not just comment or whitespace
    int m_error_code; // Set to a positive value to trigger OnError().
                      // Set to -1 to graciously stop reading of the stream midway:
                      // void OnGapOrComponent()
                      // {
                      //     if(need to stop && m_error_code==0) m_error_code=-1;
                      // }

    CRef<CAgpRow> m_prev_row;
    CRef<CAgpRow> m_this_row;
    int m_line_num, m_prev_line_num;
    string m_line;  // for valid gap/component lines, corresponds to this_row
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
        // unless(m_at_beg): m_prev_row = the last  line of the old object
        // unless(m_at_end): m_this_row = the first line of the new object
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

    CRef<CAgpErr> m_AgpErr; // Error handler

    void Init();

    void x_CheckPragmaComment(void);

public:
    CAgpErr* GetErrorHandler() { return m_AgpErr; }
    void SetErrorHandler(CAgpErr* arg);
    EAgpVersion GetVersion()
    {
        return m_agp_version;
    }
};


class NCBI_XOBJREAD_EXPORT CAgpErr : public CObject
{
public:
    CAgpErr();
    virtual ~CAgpErr() {}

    enum EAppliesTo{
      fAtThisLine= 1,
      fAtPrevLine= 4, // use fAtThisLine|fAtPrevLine when both lines are involved
      fAtNone    = 8, // Not tied to any specifc line(s) (empty file; possibly, taxid errors)
      fAtPpLine  =16  // applies to 3 conseq lines (only W_BreakingGapSameCompId so far)
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
        E_InvalidLinkage    ,

        E_MustBePositive,
        E_ObjEndLtBeg   ,
        E_CompEndLtBeg  ,
        E_ObjRangeNeGap ,
        E_ObjRangeNeComp,

        // Other errors, some detected only by agp_validate.
        // We define the codes here to preserve the historical error codes.
        // CAgpRow and CAgpReader do not know of such errors.
        E_DuplicateObj  ,       // CAgpValidateReader
        E_ObjMustBegin1 ,       // CAgpReader
        E_PartNumberNot1,       // CAgpReader
        E_PartNumberNotPlus1,   // CAgpReader
        E_UnknownOrientation,   // CAgpValidateReader

        E_ObjBegNePrevEndPlus1, // CAgpReader
        E_NoValidLines,         // CAgpReader     (Make it a warning?)
        E_SameConseqGaps,
        E_ScafBreakingGap,
        E_WithinScafGap,

        E_UnknownScaf,       // CAgpValidateReader
        E_UnusedScaf,        // CAgpValidateReader
        E_SameGapLength,     // CAgpValidateReader
        E_Last, E_First=1, E_LastToSkipLine=E_ObjRangeNeComp,

        // Warnings.
        W_GapObjEnd=31,         // CAgpReader
        W_GapObjBegin,          // CAgpReader
        W_ConseqGaps,           // CAgpReader
        W_ObjNoComp,            // CAgpValidateReader
        W_SpansOverlap,         // CAgpValidateReader

        W_SpansOrder,           // CAgpValidateReader
        W_DuplicateComp,        // CAgpValidateReader
        W_LooksLikeGap,         // CAgpRow
        W_LooksLikeComp,        // CAgpRow
        W_ExtraTab,             // CAgpRow

        W_GapLineMissingCol9,   // CAgpRow
        W_NoEolAtEof,           // CAgpReader
        W_GapLineIgnoredCol9,   // CAgpRow
        W_ObjOrderNotNumerical, // CAgpValidateReader
        // NOTE: "Wgs" warnings must come last so that  "Use -g..." hint
        //        printed in CAgpValidateReader::x_PrintTotals()
        //        comes right after the "Wgs" warning counts.
        W_CompIsWgsTypeIsNot,   // CAgpValidateReader

        W_CompIsNotWgsTypeIs,   // CAgpValidateReader
        W_ObjEqCompId,          // CAgpValidateReader
        W_GapSizeNot100,        // CAgpRow
        W_BreakingGapSameCompId,// CAgpValidateReader
        W_UnSingleCompNotInFull,// CAgpValidateReader

        W_UnSingleOriNotPlus,   // CAgpValidateReader
        W_ShortGap          ,   // CAgpValidateReader
        W_SpaceInObjName    ,   // CAgpValidateReader
        W_CommentsAfterStart,   // CAgpRow (v. >= 2.0)
        W_OrientationZeroDeprecated, // CAgpRow (v. >= 2.0)

        W_NaLinkageExpected, // CAgpRow (v. >= 2.0 )
        W_OldGapType,        // CAgpRow (v. >= 2.0 )
        W_AssumingVersion,   // CAgpRow (v. == auto)
        W_ScafNotInFull,     // CAgpValidateReader
        W_MissingLinkage,       // CAgpRow (v. >= 2.0 )

        W_AGPVersionCommentInvalid,
        W_AGPVersionCommentUnnecessary,
        W_DuplicateEvidence,    // CAgpRow (v. >= 2.0 )
        W_CompIsNotHtgTypeIs,
        W_SingletonsOnly,
        W_Last, W_First = 31,

        // "GenBank" checks that rely on information about the sequence
        G_InvalidCompId=71,     // CAgpValidateReader
        G_NotInGenbank,         // CAgpValidateReader
        G_NeedVersion,          // CAgpValidateReader
        G_CompEndGtLength,      // CAgpRow::CheckComponentEnd() (used in agp_validate)
        G_DataError,            // CAgpValidateReader

        G_TaxError,             // CAgpValidateReader
        G_InvalidObjId,         // CAgpValidateReader
        G_BadObjLen,            // CAgpValidateReader
        G_NsWithinCompSpan,
        G_Last,

        G_First = G_InvalidCompId,
    };

    static const char* ErrorWarningOrNote(int code)
    {
        if((code<W_First || code>W_Last) && code!=G_NsWithinCompSpan) return "ERROR";
        if(code==W_ShortGap || code==W_AssumingVersion) return "NOTE";
        return "WARNING";
    }

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


/// Correctly print multiple errors and warnings on consequitive lines;
/// suppress undesired or higly repetitive messages;
/// calculate total counts for each type of error.
class NCBI_XOBJREAD_EXPORT CAgpErrEx : public CAgpErr
{
public:

    enum {
        /// The number of the first CAgpErr error enum
        CODE_First = E_First,
        /// Reserve space for some user errors (to count, or to skip).
        /// The strange value of this enum is meant to give a separation
        /// of 10 or 20 between the last built-in error
        /// (currently G_Last) and the first user errors.
        CODE_Extended = ((((G_Last / 10 ) + 2) * 10) + 1),
        /// This is one past the last code allowed, after built-in and user errors.
        CODE_Last=CODE_Extended+20
    };

    //static const char* GetMsgEx(int code);
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
    bool m_use_xml;

    // Print the source line along with filename and line number.
    static void PrintLine   (CNcbiOstream& ostr,
        const string& filename, int linenum, const string& content);
    static void PrintLineXml(CNcbiOstream& ostr, const string& filename, int linenum, const string& content, bool two_lines_involved);

    /// Print the message by code, preceded by "ERROR" or "WARNING".
    /// See also: CAgpErr::FormatMessage().
    /// Override this you define user errors or warnings -
    /// you cannot override GetMsgEx().
    virtual void PrintMessage(CNcbiOstream& ostr, int code,
        const string& details=NcbiEmptyString);
    virtual void PrintMessageXml(CNcbiOstream& ostr, int code, const string& details, int appliesTo);

    // Construct a readable message on total error & warning counts
    static void PrintTotals(CNcbiOstream& ostr, int e_count, int w_count, int note_count, int skipped_count);
    static void PrintTotals(CNcbiOstream& ostr, int e_count, int w_count, int skipped_count)
    {
      PrintTotals(ostr, e_count, w_count, 0, skipped_count);
    }
    static void PrintTotalsXml(CNcbiOstream& ostr, int e_count, int w_count, int note_count, int skipped_count);

    CAgpErrEx(
        CNcbiOstream* out=&cerr,
        bool use_xml=false,
        EOwnership eOwnsOut = eNoOwnership);

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

    // we override Msg() that comes from CAgpErr
    virtual void Msg(int code, const string& details, int appliesTo=fAtThisLine);
    virtual void Msg(int code, int appliesTo=fAtThisLine)
    {
        Msg(code, NcbiEmptyString, appliesTo);
    }

    // Print any accumulated messages.
    virtual void LineDone(const string& s, int line_num, bool invalid_line=false);

    // No need to call this function when reading from STDIN,
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
    void ResetTotals();

    // Print individual error counts (important if we skipped some errors)
    typedef map<int,string> TMapCcodeToString;
    void PrintMessageCounts(CNcbiOstream& ostr, int from, int to=E_First, bool report_lines_skipped=false, TMapCcodeToString* hints=NULL);

protected:
    // Count errors of each type, including skipped ones.
    int m_MsgCount[CODE_Last];
    bool m_MustSkip[CODE_Last];


    // pp: line before previous (not supported by CAgpReader, but used in agp_validate for W_BreakingGapSameCompId)
    int m_filenum_prev, m_filenum_pp;
    // Not m_line_num-1 when the previous line:
    // - was in the different file;
    // - was a skipped comment line.
    string m_line_prev, m_line_pp;
    int  m_line_num_prev, m_line_num_pp;
    bool m_prev_printed, m_pp_printed;   // true: previous line was already printed
                                         // (probably had another error);
                                         // no need to-reprint "fname:linenum:content"
    bool m_two_lines_involved; // true: do not print "\n" after the previous line
    //bool m_invalid_prev;       // true: suppress errors concerning the previous line

    // a stream to Accumulate messages for the current line.
    // (We immediately print messages that apply only to the previous line.)
    string m_filename;
    int m_line_num;

    // For reporting filenames of the lines we passed a long time ago
    // (intersecting component spans, duplicate objects, etc)
    vector<string> m_InputFiles;

    // If this instance owns m_out, we set this AutoPtr so
    // it will automatically be destroyed.  It is otherwise
    // not used.
    AutoPtr<CNcbiOstream> m_out_destroyer;

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
    AutoPtr<CNcbiOstrstream> m_messages;
    CNcbiOstream* m_out;

    // 0: reading from STDIN or from a single file
    int GetFileNum()
    {
        return m_InputFiles.size();
    }

    const string& GetFile(int num)
    {
        return m_InputFiles[num-1];
    }
};


class CPatternStats; // internal for CAccPatternCounter
/// Accession naming patterns; find ranges for consequtive digits.
/// Sample input : AC123.1 AC456.1 AC789.1 NC8967.4 NC8967.5
///                  ^^^ ^                   ^^^^ ^
/// Sample output: AC[123..789].1 3  NC8967.[4,5] 2
class NCBI_XOBJREAD_EXPORT CAccPatternCounter : public map<string, CPatternStats*>
{
public:
    typedef vector<double> TDoubleVec;
    iterator AddName(const string& name, TDoubleVec* runsOfDigits=NULL);

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
