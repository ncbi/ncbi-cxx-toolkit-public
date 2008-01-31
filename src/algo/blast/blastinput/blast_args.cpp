/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_args.cpp

Author: Jason Papadopoulos

******************************************************************************/

/** @file blast_args.cpp
 * convert blast-related command line
 * arguments into blast options
*/

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif

#include <ncbi_pch.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/blastinput/blast_args.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/objmgr_query_data.hpp> /* for CObjMgrQueryFactory */
#include <algo/blast/core/blast_nalookup.h>
#include <objtools/blast_format/blastfmtutil.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <util/format_guess.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>
#include "blast_input_aux.hpp"
#include <sstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

/// Class to constrain the values of an argument to those greater than or equal
/// to the value specified in the constructor
class CArgAllowValuesGreaterThanOrEqual : public CArgAllow
{
public:
    /// Constructor taking an integer
    CArgAllowValuesGreaterThanOrEqual(int min) : m_MinValue(min) {}
    /// Constructor taking a double
    CArgAllowValuesGreaterThanOrEqual(double min) : m_MinValue(min) {}

protected:
    /// Overloaded method from CArgAllow
    virtual bool Verify(const string& value) const {
        return NStr::StringToDouble(value) >= m_MinValue;
    }

    /// Overloaded method from CArgAllow
    virtual string GetUsage(void) const {
        return ">=" + NStr::DoubleToString(m_MinValue);
    }
    
private:
    double m_MinValue;  /**< Minimum value for this object */
};

/// Class to constrain the values of an argument to those less than or equal
/// to the value specified in the constructor
class CArgAllowValuesLessThanOrEqual : public CArgAllow
{
public:
    /// Constructor taking an integer
    CArgAllowValuesLessThanOrEqual(int max) : m_MaxValue(max) {}
    /// Constructor taking a double
    CArgAllowValuesLessThanOrEqual(double max) : m_MaxValue(max) {}

protected:
    /// Overloaded method from CArgAllow
    virtual bool Verify(const string& value) const {
        return NStr::StringToDouble(value) <= m_MaxValue;
    }

    /// Overloaded method from CArgAllow
    virtual string GetUsage(void) const {
        return "<=" + NStr::DoubleToString(m_MaxValue);
    }
    
private:
    double m_MaxValue;  /**< Maximum value for this object */
};

/** 
 * @brief Macro to create a subclass of CArgAllow that allows the specification
 * of sets of data
 * 
 * @param ClassName Name of the class to be created [in]
 * @param DataType data type of the allowed arguments [in]
 * @param String2DataTypeFn Conversion function from a string to DataType [in]
 */
#define DEFINE_CARGALLOW_SET_CLASS(ClassName, DataType, String2DataTypeFn)  \
class ClassName : public CArgAllow                                          \
{                                                                           \
public:                                                                     \
    ClassName(const set<DataType>& values)                                  \
        : m_AllowedValues(values)                                           \
    {                                                                       \
        if (values.empty()) {                                               \
            throw runtime_error("Allowed values set must not be empty");    \
        }                                                                   \
    }                                                                       \
                                                                            \
protected:                                                                  \
    virtual bool Verify(const string& value) const {                        \
        DataType value2check = String2DataTypeFn(value);                    \
        ITERATE(set<DataType>, itr, m_AllowedValues) {                      \
            if (*itr == value2check) {                                      \
                return true;                                                \
            }                                                               \
        }                                                                   \
        return false;                                                       \
    }                                                                       \
                                                                            \
    virtual string GetUsage(void) const {                                   \
        ostringstream os;                                                   \
        os << "Permissible values: ";                                       \
        ITERATE(set<DataType>, itr, m_AllowedValues) {                      \
            os << "'" << *itr << "' ";                                      \
        }                                                                   \
        return os.str();                                                    \
    }                                                                       \
                                                                            \
private:                                                                    \
    /* Set containing the permissible values */                             \
    set<DataType> m_AllowedValues;                                          \
}

#ifndef SKIP_DOXYGEN_PROCESSING
DEFINE_CARGALLOW_SET_CLASS(CArgAllowIntegerSet, int, NStr::StringToInt);
DEFINE_CARGALLOW_SET_CLASS(CArgAllowStringSet, string, string);
#endif /* SKIP_DOXYGEN_PROCESSING */

void
IBlastCmdLineArgs::ExtractAlgorithmOptions(const CArgs& /* cmd_line_args */,
                                           CBlastOptions& /* options */)
{}

CProgramDescriptionArgs::CProgramDescriptionArgs(const string& program_name, 
                                                 const string& program_desc)
    : m_ProgName(program_name), m_ProgDesc(program_desc)
{}

void
CProgramDescriptionArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // program description
    arg_desc.SetUsageContext(m_ProgName, m_ProgDesc + " " + 
                             blast::Version.Print());
}

CTaskCmdLineArgs::CTaskCmdLineArgs(const set<string>& supported_tasks,
                                   const string& default_task)
: m_SupportedTasks(supported_tasks), m_DefaultTask(default_task)
{
    _ASSERT( !m_SupportedTasks.empty() );
    if ( !m_DefaultTask.empty() ) {
        _ASSERT(m_SupportedTasks.find(m_DefaultTask) != m_SupportedTasks.end());
    }
}

void
CTaskCmdLineArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("General search options");
    if ( !m_DefaultTask.empty() ) {
        arg_desc.AddDefaultKey(kTask, "task_name", "Task to execute", 
                               CArgDescriptions::eString, m_DefaultTask);
    } else {
        arg_desc.AddKey(kTask, "task_name", "Task to execute",
                        CArgDescriptions::eString);
    }
    arg_desc.SetConstraint(kTask, new CArgAllowStringSet(m_SupportedTasks));
    arg_desc.SetCurrentGroup("");
   
}

void
CTaskCmdLineArgs::ExtractAlgorithmOptions(const CArgs& /* cmd_line_args */,
                                          CBlastOptions& /* options */)
{
    // N.B.: handling of tasks occurs at the application level to ensure that
    // only relevant tasks are added (@sa CBlastnAppArgs)
}

void
CGenericSearchArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("General search options");

    // evalue cutoff
    arg_desc.AddDefaultKey(kArgEvalue, "evalue", 
                     "Expectation value (E) threshold for saving hits ",
                     CArgDescriptions::eDouble,
                     NStr::DoubleToString(BLAST_EXPECT_VALUE));

    // word size
    // Default values: blastn=11, megablast=28, others=3
    const string description = m_QueryIsProtein 
        ? "Word size for wordfinder algorithm"
        : "Word size for wordfinder algorithm (length of best perfect match)";
    arg_desc.AddOptionalKey(kArgWordSize, "int_value", description,
                            CArgDescriptions::eInteger);
    arg_desc.SetConstraint(kArgWordSize, m_QueryIsProtein 
                           ? new CArgAllowValuesGreaterThanOrEqual(2)
                           : new CArgAllowValuesGreaterThanOrEqual(4));

    if ( !m_IsRpsBlast ) {
        // gap open penalty
        arg_desc.AddOptionalKey(kArgGapOpen, "open_penalty", 
                                "Cost to open a gap", 
                                CArgDescriptions::eInteger);

        // gap extend penalty
        arg_desc.AddOptionalKey(kArgGapExtend, "extend_penalty",
                               "Cost to extend a gap", 
                               CArgDescriptions::eInteger);
    }


    arg_desc.SetCurrentGroup("Restrict search or results");
    arg_desc.AddOptionalKey(kArgPercentIdentity, "float_value",
                            "Percent identity",
                            CArgDescriptions::eDouble);
    arg_desc.SetConstraint(kArgPercentIdentity,
                           new CArgAllow_Doubles(0.0, 100.0));

    arg_desc.SetCurrentGroup("Extension options");
    // ungapped X-drop
    // Default values: blastn=20, megablast=10, others=7
    arg_desc.AddOptionalKey(kArgUngappedXDropoff, "float_value", 
                            "X dropoff value (in bits) for ungapped extensions",
                            CArgDescriptions::eDouble);

    // initial gapped X-drop
    // Default values: blastn=30, megablast=20, tblastx=0, others=15
    arg_desc.AddOptionalKey(kArgGappedXDropoff, "float_value", 
                 "X-dropoff value (in bits) for preliminary gapped extensions",
                 CArgDescriptions::eDouble);

    // final gapped X-drop
    // Default values: blastn/megablast=50, tblastx=0, others=25
    arg_desc.AddOptionalKey(kArgFinalGappedXDropoff, "float_value", 
                         "X dropoff value (in bits) for final gapped alignment",
                         CArgDescriptions::eDouble);

    arg_desc.SetCurrentGroup("Statistical options");
    // effective search space
    // Default value is the real size
    arg_desc.AddOptionalKey(kArgEffSearchSpace, "float_value", 
                            "Effective length of the search space",
                            CArgDescriptions::eInt8);
    arg_desc.SetConstraint(kArgEffSearchSpace, 
                           new CArgAllowValuesGreaterThanOrEqual(0));

#if 0
    arg_desc.AddDefaultKey(kArgMaxHSPsPerSubject, "int_value",
                           "Maximum number of HPSs per subject to save "
                           "( " + NStr::IntToString(kDfltArgMaxHSPsPerSubject)
                           + " means infinite)",
                           CArgDescriptions::eInteger,
                           NStr::IntToString(kDfltArgMaxHSPsPerSubject));
    arg_desc.SetConstraint(kArgMaxHSPsPerSubject,
                           new CArgAllowValuesGreaterThanOrEqual(0));
#endif

    arg_desc.SetCurrentGroup("");
}

void
CGenericSearchArgs::ExtractAlgorithmOptions(const CArgs& args, 
                                            CBlastOptions& opt)
{
    if (args[kArgEvalue]) {
        opt.SetEvalueThreshold(args[kArgEvalue].AsDouble());
    }

    int gap_open=0, gap_extend=0;
    if (args.Exist(kArgMatrixName) && args[kArgMatrixName])
         BLAST_GetProteinGapExistenceExtendParams(args[kArgMatrixName].AsString().c_str(), &gap_open, &gap_extend);

    if (args.Exist(kArgGapOpen) && args[kArgGapOpen]) {
        opt.SetGapOpeningCost(args[kArgGapOpen].AsInteger());
    }
    else if (args.Exist(kArgMatrixName) && args[kArgMatrixName]) {
        opt.SetGapOpeningCost(gap_open);
    }

    if (args.Exist(kArgGapExtend) && args[kArgGapExtend]) {
        opt.SetGapExtensionCost(args[kArgGapExtend].AsInteger());
    }
    else if (args.Exist(kArgMatrixName) && args[kArgMatrixName]) {
        opt.SetGapExtensionCost(gap_extend);
    }

    if (args[kArgUngappedXDropoff]) {
        opt.SetXDropoff(args[kArgUngappedXDropoff].AsDouble());
    }

    if (args[kArgGappedXDropoff]) {
        opt.SetGapXDropoff(args[kArgGappedXDropoff].AsDouble());
    }

    if (args[kArgFinalGappedXDropoff]) {
        opt.SetGapXDropoffFinal(args[kArgFinalGappedXDropoff].AsDouble());
    }

    if (args[kArgWordSize]) {
        opt.SetWordSize(args[kArgWordSize].AsInteger());
    }

    if (args[kArgEffSearchSpace]) {
        opt.SetEffectiveSearchSpace(args[kArgEffSearchSpace].AsInt8());
    }

    if (args[kArgPercentIdentity]) {
        opt.SetPercentIdentity(args[kArgPercentIdentity].AsDouble());
    }

#if 0
    if (args[kArgMaxHSPsPerSubject]) {
        const int value = args[kArgMaxHSPsPerSubject].AsInteger();
        if (value != kDfltArgMaxHSPsPerSubject) {
            opt.SetMaxNumHspPerSequence(value);
        }
    }
#endif
}

void
CFilteringArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Query filtering options");

    if (m_QueryIsProtein) {
        arg_desc.AddDefaultKey(kArgSegFiltering, "SEG_options",
                        "Filter query sequence with SEG "
                        "(Format: '" + kDfltArgApplyFiltering + "', " +
                        "'window locut hicut', or '" + kDfltArgNoFiltering +
                        "' to disable)",
                        CArgDescriptions::eString, m_FilterByDefault
                        ? kDfltArgSegFiltering : kDfltArgNoFiltering);
        arg_desc.AddDefaultKey(kArgLookupTableMaskingOnly, "soft_masking",
                        "Apply filtering locations as soft masks",
                        CArgDescriptions::eBoolean, "false");
    } else {
        arg_desc.AddDefaultKey(kArgDustFiltering, "DUST_options",
                        "Filter query sequence with DUST "
                        "(Format: '" + kDfltArgApplyFiltering + "', " + 
                        "'level window linker', or '" + kDfltArgNoFiltering +
                        "' to disable)",
                        CArgDescriptions::eString, m_FilterByDefault
                        ? kDfltArgDustFiltering : kDfltArgNoFiltering);
        arg_desc.AddOptionalKey(kArgFilteringDb, "filtering_database",
                "BLAST database containing filtering elements (i.e.: repeats)",
                CArgDescriptions::eString);
        arg_desc.AddDefaultKey(kArgLookupTableMaskingOnly, "soft_masking",
                        "Apply filtering locations as soft masks",
                        CArgDescriptions::eBoolean, "true");
    }

    arg_desc.SetCurrentGroup("");
}

void 
CFilteringArgs::x_TokenizeFilteringArgs(const string& filtering_args, 
                                        vector<string>& output) const
{
    output.clear();
    NStr::Tokenize(filtering_args, " ", output);
}

void
CFilteringArgs::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& opt)
{
    if (args[kArgLookupTableMaskingOnly] && 
        args[kArgLookupTableMaskingOnly].AsBoolean()) {
        opt.SetMaskAtHash(true);
    }

    vector<string> tokens;

    if (m_QueryIsProtein && args[kArgSegFiltering]) {
        const string& seg_opts = args[kArgSegFiltering].AsString();
        if (seg_opts == kDfltArgNoFiltering) {
            opt.SetSegFiltering(false);
        } else if (seg_opts == kDfltArgApplyFiltering) {
            opt.SetSegFiltering(true);
        } else {
            x_TokenizeFilteringArgs(seg_opts, tokens);
            opt.SetSegFilteringWindow(NStr::StringToInt(tokens[0]));
            opt.SetSegFilteringLocut(NStr::StringToDouble(tokens[1]));
            opt.SetSegFilteringHicut(NStr::StringToDouble(tokens[2]));
        }
    }

    if ( !m_QueryIsProtein && args[kArgDustFiltering]) {
        const string& dust_opts = args[kArgDustFiltering].AsString();
        if (dust_opts == kDfltArgNoFiltering) {
            opt.SetDustFiltering(false);
        } else if (dust_opts == kDfltArgApplyFiltering) {
            opt.SetDustFiltering(true);
        } else {
            x_TokenizeFilteringArgs(dust_opts, tokens);
            opt.SetDustFilteringLevel(NStr::StringToInt(tokens[0]));
            opt.SetDustFilteringWindow(NStr::StringToInt(tokens[1]));
            opt.SetDustFilteringLinker(NStr::StringToInt(tokens[2]));
        }
    }

    if (args.Exist(kArgFilteringDb) && args[kArgFilteringDb]) {
        opt.SetRepeatFilteringDB(args[kArgFilteringDb].AsString().c_str());
    }
}

void
CWindowSizeArg::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Extension options");
    // 2-hit wordfinder window size
    arg_desc.AddOptionalKey(kArgWindowSize, "int_value", 
                            "Multiple hits window size, use 0 to specify "
                            "1-hit algorithm",
                            CArgDescriptions::eInteger);
    arg_desc.SetConstraint(kArgWindowSize, 
                           new CArgAllowValuesGreaterThanOrEqual(0));
    arg_desc.SetCurrentGroup("");
}

void
CWindowSizeArg::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& opt)
{
    if (args[kArgWindowSize]) {
        opt.SetWindowSize(args[kArgWindowSize].AsInteger());
    } else {
        int window = -1;
        BLAST_GetSuggestedWindowSize(opt.GetProgramType(), 
                                     opt.GetMatrixName(), 
                                     &window);
        if (window != -1) {
            opt.SetWindowSize(window);
        }
    }
}

void
CWordThresholdArg::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("General search options");
    // lookup table word score threshold
    arg_desc.AddOptionalKey(kArgWordScoreThreshold, "float_value", 
                 "Minimum word score such that the word is added to the "
                 "BLAST lookup table",
                 CArgDescriptions::eDouble);
    arg_desc.SetConstraint(kArgWordScoreThreshold, 
                           new CArgAllowValuesGreaterThanOrEqual(0));
    arg_desc.SetCurrentGroup("");
}

void
CWordThresholdArg::ExtractAlgorithmOptions(const CArgs& args, 
                                           CBlastOptions& opt)
{
    if (args[kArgWordScoreThreshold]) {
        opt.SetWordThreshold(args[kArgWordScoreThreshold].AsDouble());
    } else {
        double threshold = -1;
        BLAST_GetSuggestedThreshold(opt.GetProgramType(),
                                    opt.GetMatrixName(),
                                    &threshold);
        if (threshold != -1) {
            opt.SetWordThreshold(threshold);
        }
    }
}

void
CMatrixNameArg::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("General search options");
    arg_desc.AddDefaultKey(kArgMatrixName, "matrix_name", 
                           "Scoring matrix name",
                           CArgDescriptions::eString, 
                           string(BLAST_DEFAULT_MATRIX));
    arg_desc.SetCurrentGroup("");
}

void
CMatrixNameArg::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& opt)
{
    if (args[kArgMatrixName]) {
        opt.SetMatrixName(args[kArgMatrixName].AsString().c_str());
    }
}

void
CNuclArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // TLM arg_desc.SetCurrentGroup("Nucleotide scoring options");

    arg_desc.SetCurrentGroup("General search options");
    // blastn mismatch penalty
    arg_desc.AddOptionalKey(kArgMismatch, "penalty", 
                           "Penalty for a nucleotide mismatch", 
                           CArgDescriptions::eInteger);
    arg_desc.SetConstraint(kArgMismatch, 
                           new CArgAllowValuesLessThanOrEqual(0));

    // blastn match reward
    arg_desc.AddOptionalKey(kArgMatch, "reward", 
                           "Reward for a nucleotide match", 
                           CArgDescriptions::eInteger); 
    arg_desc.SetConstraint(kArgMatch, 
                           new CArgAllowValuesGreaterThanOrEqual(0));


    arg_desc.SetCurrentGroup("Extension options");
    arg_desc.AddFlag(kArgNoGreedyExtension,
                     "Use non-greedy dynamic programming extension",
                     true);

    arg_desc.SetCurrentGroup("");
}

void
CNuclArgs::ExtractAlgorithmOptions(const CArgs& cmd_line_args,
                                   CBlastOptions& options)
{
    if (cmd_line_args[kArgMismatch])
        options.SetMismatchPenalty(cmd_line_args[kArgMismatch].AsInteger());
    if (cmd_line_args[kArgMatch])
        options.SetMatchReward(cmd_line_args[kArgMatch].AsInteger());

    if (cmd_line_args[kArgNoGreedyExtension]) {
        options.SetGapExtnAlgorithm(eDynProgScoreOnly);
        options.SetGapTracebackAlgorithm(eDynProgTbck);
    }
}

const string CDiscontiguousMegablastArgs::kTemplType_Coding("coding");
const string CDiscontiguousMegablastArgs::kTemplType_Optimal("optimal");
const string 
CDiscontiguousMegablastArgs::kTemplType_CodingAndOptimal("coding_and_optimal");

void
CDiscontiguousMegablastArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Extension options");
    // FIXME: this can be applied to any program, but since it was only offered
    // in megablast, we're putting it here 
    arg_desc.AddOptionalKey(kArgMinRawGappedScore, "int_value",
                            "Minimum raw gapped score to keep an alignment "
                            "in the preliminary gapped and traceback stages",
                            CArgDescriptions::eInteger);

    arg_desc.SetCurrentGroup("Discontiguous MegaBLAST options");

    arg_desc.AddOptionalKey(kArgDMBTemplateType, "type", 
                 "Discontiguous MegaBLAST template type",
                 CArgDescriptions::eString);
    arg_desc.SetConstraint(kArgDMBTemplateType, &(*new CArgAllow_Strings, 
                                                  kTemplType_Coding,
                                                  kTemplType_Optimal,
                                                  kTemplType_CodingAndOptimal));
    arg_desc.SetDependency(kArgDMBTemplateType,
                           CArgDescriptions::eRequires,
                           kArgDMBTemplateLength);

    arg_desc.AddOptionalKey(kArgDMBTemplateLength, "int_value", 
                 "Discontiguous MegaBLAST template length",
                 CArgDescriptions::eInteger);
    set<int> allowed_values;
    allowed_values.insert(16);
    allowed_values.insert(18);
    allowed_values.insert(21);
    arg_desc.SetConstraint(kArgDMBTemplateLength, 
                           new CArgAllowIntegerSet(allowed_values));
    arg_desc.SetDependency(kArgDMBTemplateLength,
                           CArgDescriptions::eRequires,
                           kArgDMBTemplateType);

    arg_desc.SetCurrentGroup("");
}

void
CDiscontiguousMegablastArgs::ExtractAlgorithmOptions(const CArgs& args,
                                                     CBlastOptions& options)
{
    if (args[kArgMinRawGappedScore]) {
        options.SetCutoffScore(args[kArgMinRawGappedScore].AsInteger());
    }

    if (args[kArgDMBTemplateType]) {
        const string& type = args[kArgDMBTemplateType].AsString();
        EDiscWordType temp_type = eMBWordCoding;

        if (type == kTemplType_Coding) {
            temp_type = eMBWordCoding;
        } else if (type == kTemplType_Optimal) {
            temp_type = eMBWordOptimal;
        } else if (type == kTemplType_CodingAndOptimal) {
            temp_type = eMBWordTwoTemplates;
        } else {
            abort();
        }
        options.SetMBTemplateType(static_cast<unsigned char>(temp_type));
    }

    if (args[kArgDMBTemplateLength]) {
        unsigned char tlen = 
            static_cast<unsigned char>(args[kArgDMBTemplateLength].AsInteger());
        options.SetMBTemplateLength(tlen);
    }

    // FIXME: should the window size be adjusted if this is set?
}

void
CCompositionBasedStatsArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("General search options");
    // composition based statistics
    arg_desc.AddDefaultKey(kArgCompBasedStats, "compo", 
                      "Use composition-based statistics for blastp / tblastn:\n"
                      "    D or d: default (equivalent to 2)\n"
                      "    0 or F or f: no composition-based statistics\n"
                      "    1: Composition-based statistics "
                                      "as in NAR 29:2994-3005, 2001\n"
                      "    2 or T or t : Composition-based score adjustment as in "
                                      "Bioinformatics 21:902-911,\n"
                      "    2005, conditioned on sequence properties\n"
                      "    3: Composition-based score adjustment as in "
                                      "Bioinformatics 21:902-911,\n"
                      "    2005, unconditionally\n"
                      "For programs other than tblastn, must either be "
                      "absent or be D, F or 0",
                      CArgDescriptions::eString, "2");

    arg_desc.SetCurrentGroup("misc");
    // Use Smith-Waterman algorithm in traceback stage
    // FIXME: available only for gapped blastp/tblastn, and with
    // composition-based statistics
    arg_desc.AddFlag(kArgUseSWTraceback, 
                     "Compute locally optimal Smith-Waterman alignments?",
                     true);
    arg_desc.SetCurrentGroup("");
}

/** 
 * @brief Auxiliary function to set the composition based statistics and smith
 * waterman options
 * 
 * @param opt BLAST options object [in|out]
 * @param comp_stat_string command line value for composition based statistics
 * [in]
 * @param smith_waterman_value command line value for determining the use of
 * the smith-waterman algorithm [in]
 * @param ungapped pointer to the value which determines whether the search
 * should be ungapped or not. It is NULL if ungapped searches are not
 * applicable
 */
static void
s_SetCompositionBasedStats(CBlastOptions& opt,
                           const string& comp_stat_string,
                           bool smith_waterman_value,
                           bool* ungapped = NULL)
{
    const EProgram program = opt.GetProgram();
    if (program == eBlastp || program == eTblastn || program == ePSIBlast) {

        ECompoAdjustModes compo_mode = eNoCompositionBasedStats;
    
        switch (comp_stat_string[0]) {
            case '0': case 'F': case 'f':
                compo_mode = eNoCompositionBasedStats;
                break;
            case '1':
                compo_mode = eCompositionBasedStats;
                break;
            case 'D': case 'd':
            case '2': case 'T': case 't':
                compo_mode = eCompositionMatrixAdjust;
                break;
            case '3':
                compo_mode = eCompoForceFullMatrixAdjust;
                break;
        }

        if (ungapped && *ungapped && compo_mode != eNoCompositionBasedStats) {
            NCBI_THROW(CBlastException, eInvalidArgument, 
                       "Composition-adjusted searched are not supported with "
                       "an ungapped search, please add -comp_based_stats F or "
                       "do a gapped search");
        }

        opt.SetCompositionBasedStats(compo_mode);
        if (program == eBlastp &&
            compo_mode != eNoCompositionBasedStats &&
            tolower(comp_stat_string[1]) == 'u') {
            opt.SetUnifiedP(1);
        }
        opt.SetSmithWatermanMode(smith_waterman_value);
    }
}

void
CCompositionBasedStatsArgs::ExtractAlgorithmOptions(const CArgs& args,
                                                    CBlastOptions& opt)
{
    if (args[kArgCompBasedStats]) {
        auto_ptr<bool> ungapped(args.Exist(kArgUngapped) 
            ? new bool(args[kArgUngapped]) : 0);
        s_SetCompositionBasedStats(opt, 
                                   args[kArgCompBasedStats].AsString(),
                                   args[kArgUseSWTraceback],
                                   ungapped.get());
    }

}

void
CGappedArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // perform gapped search
#if 0
    arg_desc.AddOptionalKey(ARG_GAPPED, "gapped", 
                 "Perform gapped alignment (default T, but "
                 "not available for tblastx)",
                 CArgDescriptions::eBoolean,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc.AddAlias("-gapped", ARG_GAPPED);
#endif
    arg_desc.SetCurrentGroup("Extension options");
    arg_desc.AddFlag(kArgUngapped, "Perform ungapped alignment only?", true);
    arg_desc.SetCurrentGroup("");
}

void
CGappedArgs::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& options)
{
#if 0
    if (args[ARG_GAPPED] && options.GetProgram() != eTblastx) {
        options.SetGappedMode(args[ARG_GAPPED].AsBoolean());
    }
#endif
    options.SetGappedMode( !args[kArgUngapped] );
}

void
CLargestIntronSizeArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("General search options");
    // largest intron length
    arg_desc.AddDefaultKey(kArgMaxIntronLength, "length", 
                    "Length of the largest intron allowed in a translated "
                    "nucleotide sequence when linking multiple distinct "
                    "alignments (a negative value disables linking)",
                    CArgDescriptions::eInteger,
                    NStr::IntToString(kDfltArgMaxIntronLength));
    arg_desc.SetCurrentGroup("");
}

void
CLargestIntronSizeArgs::ExtractAlgorithmOptions(const CArgs& args,
                                                CBlastOptions& opt)
{
    if ( !args[kArgMaxIntronLength] ) {
        return;
    }

    if (args[kArgMaxIntronLength].AsInteger() < 0) {
        opt.SetSumStatisticsMode(false);
    } else {
        opt.SetSumStatisticsMode();
        opt.SetLongestIntronLength(args[kArgMaxIntronLength].AsInteger());
    }
}

void
CFrameShiftArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("General search options");
    // applicable in blastx/tblastn, off by default
    arg_desc.AddOptionalKey(kArgFrameShiftPenalty, "frameshift",
                            "Frame shift penalty (for use with out-of-frame "
                            "gapped alignment in blastx or tblastn, default "
                            "ignored)",
                            CArgDescriptions::eInteger);
    arg_desc.SetConstraint(kArgFrameShiftPenalty, 
                           new CArgAllowValuesGreaterThanOrEqual(1));
    arg_desc.SetCurrentGroup("");
}

void
CFrameShiftArgs::ExtractAlgorithmOptions(const CArgs& args,
                                         CBlastOptions& opt)
{
    if (args[kArgFrameShiftPenalty]) {
        opt.SetOutOfFrameMode();
        opt.SetFrameShiftPenalty(args[kArgFrameShiftPenalty].AsInteger());
    }
}

void
CGeneticCodeArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    if (m_Target == eQuery) {
        arg_desc.SetCurrentGroup("Input query options");
        // query genetic code
        arg_desc.AddDefaultKey(kArgQueryGeneticCode, "int_value", 
                               "Genetic code to use to translate query",
                               CArgDescriptions::eInteger,
                               NStr::IntToString(BLAST_GENETIC_CODE));
    } else {
        arg_desc.SetCurrentGroup("General search options");
        // DB genetic code
        arg_desc.AddDefaultKey(kArgDbGeneticCode, "int_value", 
                               "Genetic code to use to translate database",
                               CArgDescriptions::eInteger,
                               NStr::IntToString(BLAST_GENETIC_CODE));
    }
    arg_desc.SetCurrentGroup("");
}

void
CGeneticCodeArgs::ExtractAlgorithmOptions(const CArgs& args,
                                          CBlastOptions& opt)
{
    const EProgram program = opt.GetProgram();

    if (m_Target == eQuery && args[kArgQueryGeneticCode]) {
        opt.SetQueryGeneticCode(args[kArgQueryGeneticCode].AsInteger());
    }
  
    if (m_Target == eDatabase && args[kArgDbGeneticCode] &&
        (program == eTblastn || program == eTblastx) ) {
        opt.SetDbGeneticCode(args[kArgDbGeneticCode].AsInteger());
    }
}

void
CGapTriggerArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Extension options");

    const double default_value = m_QueryIsProtein
        ? BLAST_GAP_TRIGGER_PROT : BLAST_GAP_TRIGGER_NUCL;
    arg_desc.AddDefaultKey(kArgGapTrigger, "float_value", 
                           "Number of bits to trigger gapping",
                           CArgDescriptions::eDouble,
                           NStr::DoubleToString(default_value));
    arg_desc.SetCurrentGroup("");
}

void
CGapTriggerArgs::ExtractAlgorithmOptions(const CArgs& args,
                                         CBlastOptions& opt)
{
    if (args[kArgGapTrigger]) {
        opt.SetGapTrigger(args[kArgGapTrigger].AsDouble());
    }
}

void
CPssmEngineArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("PSSM engine options");

    // Pseudo count
    arg_desc.AddDefaultKey(kArgPSIPseudocount, "pseudocount",
                           "Pseudo-count value used when constructing PSSM",
                           CArgDescriptions::eInteger,
                           NStr::IntToString(PSI_PSEUDO_COUNT_CONST));

    // Evalue inclusion threshold
    arg_desc.AddDefaultKey(kArgPSIInclusionEThreshold, "ethresh", 
                   "E-value inclusion threshold for pairwise alignments", 
                   CArgDescriptions::eDouble,
                   NStr::DoubleToString(PSI_INCLUSION_ETHRESH));

    arg_desc.SetCurrentGroup("");
}

void
CPssmEngineArgs::ExtractAlgorithmOptions(const CArgs& args,
                                         CBlastOptions& opt)
{
    if (args[kArgPSIPseudocount]) {
        opt.SetPseudoCount(args[kArgPSIPseudocount].AsInteger());
    }

    if (args[kArgPSIInclusionEThreshold]) {
        opt.SetInclusionThreshold(args[kArgPSIInclusionEThreshold].AsDouble());
    }
}

void
CPsiBlastArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{

    if (m_DbTarget == eNucleotideDb) {
        arg_desc.SetCurrentGroup("PSI-TBLASTN options");

        // PSI-tblastn checkpoint
        arg_desc.AddOptionalKey(kArgPSIInputChkPntFile, "psi_chkpt_file", 
                                "PSI-TBLASTN checkpoint file",
                                CArgDescriptions::eInputFile);
    } else {
        arg_desc.SetCurrentGroup("PSI-BLAST options");

        // Number of iterations
        arg_desc.AddDefaultKey(kArgPSINumIterations, "int_value",
                               "Number of iterations to perform",
                               CArgDescriptions::eInteger,
                               NStr::IntToString(1));
        arg_desc.SetConstraint(kArgPSINumIterations, 
                               new CArgAllowValuesGreaterThanOrEqual(1));
        arg_desc.SetDependency(kArgPSINumIterations,
                               CArgDescriptions::eExcludes,
                               kArgRemote);
        // checkpoint file
        arg_desc.AddOptionalKey(kArgPSIOutputChkPntFile, "checkpoint_file",

                                "File name to store checkpoint file",
                                CArgDescriptions::eOutputFile);
        // ASCII matrix file
        arg_desc.AddOptionalKey(ARG_ASCII_MATRIX, "ascii_mtx_file",
                                "File name to store ASCII version of PSSM",
                                CArgDescriptions::eOutputFile,
                                CArgDescriptions::fOptionalSeparator);
        // MSA restart file
        arg_desc.AddOptionalKey(ARG_MSA_RESTART, "align_restart",
                                "File name of multiple sequence alignment to "
                                "restart PSI-BLAST",
                                CArgDescriptions::eInputFile,
                                CArgDescriptions::fOptionalSeparator);
        // PSI-BLAST checkpoint
        arg_desc.AddOptionalKey(kArgPSIInputChkPntFile, "psi_chkpt_file", 
                                "PSI-BLAST checkpoint file",
                                CArgDescriptions::eInputFile);
    }

    arg_desc.SetDependency(kArgPSIInputChkPntFile,
                           CArgDescriptions::eExcludes,
                           kArgQuery);
    arg_desc.SetCurrentGroup("");
}

void
CPsiBlastArgs::ExtractAlgorithmOptions(const CArgs& args,
                                       CBlastOptions& /* opt */)
{
    if (m_DbTarget == eProteinDb) {
        if (args[kArgPSINumIterations]) {
            m_NumIterations = args[kArgPSINumIterations].AsInteger();
        }
        if (args.Exist(kArgPSIOutputChkPntFile) &&
            args[kArgPSIOutputChkPntFile]) {
            m_CheckPointOutputStream = 
                &args[kArgPSIOutputChkPntFile].AsOutputFile(); 
        }
        if (args[ARG_ASCII_MATRIX]) {
            cerr << "Warning: ASCII MATRIX NOT HANDLED\n";
        }
        if (args[ARG_MSA_RESTART]) {
            cerr << "Warning: INPUT ALIGNMENT FILE NOT HANDLED\n";
        }
    }

    if (args.Exist(kArgPSIInputChkPntFile) && args[kArgPSIInputChkPntFile]) {
        CNcbiIstream& in = args[kArgPSIInputChkPntFile].AsInputFile();
        m_Pssm.Reset(new CPssmWithParameters);
        switch (CFormatGuess().Format(in)) {
        case CFormatGuess::eBinaryASN:
            in >> MSerial_AsnBinary >> *m_Pssm;
            break;
        case CFormatGuess::eTextASN:
            in >> MSerial_AsnText >> *m_Pssm;
            break;
        case CFormatGuess::eXml:
            in >> MSerial_Xml >> *m_Pssm;
            break;
        default:
            NCBI_THROW(CBlastException, eInvalidArgument, 
                       "Unsupported format for PSSM");
        }
        _ASSERT(m_Pssm.NotEmpty());
    }
}

void
CPhiBlastArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("PHI-BLAST options");

    arg_desc.AddOptionalKey(kArgPHIPatternFile, "file",
                            "File name containing pattern to search",
                            CArgDescriptions::eInputFile);
    arg_desc.SetDependency(kArgPHIPatternFile,
                           CArgDescriptions::eExcludes,
                           kArgPSIInputChkPntFile);

    arg_desc.SetCurrentGroup("");
}

void
CPhiBlastArgs::ExtractAlgorithmOptions(const CArgs& args,
                                       CBlastOptions& opt)
{
    if (args.Exist(kArgPHIPatternFile) && args[kArgPHIPatternFile]) {
        //CNcbiIstream& in = args[kArgPHIPatternFile].AsInputFile();
        string pattern("FIXME - NOT VALID");
        // FIXME: need to port code from pssed3.c get_pat function
        opt.SetPHIPattern(pattern.c_str(), 
              static_cast<bool>(Blast_QueryIsNucleotide(opt.GetProgramType())));
        throw runtime_error("Reading of pattern file not implemented");
    }
}

void
CQueryOptionsArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{

    arg_desc.SetCurrentGroup("Query filtering options");
    // lowercase masking
    arg_desc.AddFlag(kArgUseLCaseMasking, 
                     "Use lower case filtering in query sequence(s)?", true);

    arg_desc.SetCurrentGroup("Input query options");
    // query location
    arg_desc.AddOptionalKey(kArgQueryLocation, "range", 
                            "Location on the query sequence "
                            "(Format: start-stop)",
                            CArgDescriptions::eString);

    // believe query ID
    arg_desc.AddFlag(kArgParseQueryDefline,
                     "Should the query defline(s) be parsed?", true);

    if ( !m_QueryCannotBeNucl ) {
        // search strands
        arg_desc.AddDefaultKey(kArgStrand, "strand", 
                         "Query strand(s) to search against database",
                         CArgDescriptions::eString, kDfltArgStrand);
        arg_desc.SetConstraint(kArgStrand, &(*new CArgAllow_Strings, 
                                             kDfltArgStrand, "plus", "minus"));
    }

    arg_desc.SetCurrentGroup("");
}

static void s_TokenizeSequenceRange(const string& locations, 
                                    TSeqRange& range,
                                    const string& msg)
{
    static const string delimiters("-");
    vector<string> tokens;
    NStr::Tokenize(locations, delimiters, tokens);
    if (tokens.size() != 2) {
        NCBI_THROW(CBlastException, eInvalidArgument, msg);
    }
    range.SetFrom(NStr::StringToInt(tokens.front()));
    range.SetToOpen(NStr::StringToInt(tokens.back()));
}

void
CQueryOptionsArgs::ExtractAlgorithmOptions(const CArgs& args, 
                                           CBlastOptions& opt)
{
    // Get the strand
    {
        m_Strand = eNa_strand_unknown;

        if (!Blast_QueryIsProtein(opt.GetProgramType()) && args[kArgStrand]) {
            const string& kStrand = args[kArgStrand].AsString();
            if (kStrand == "both") {
                m_Strand = eNa_strand_both;
            } else if (kStrand == "plus") {
                m_Strand = eNa_strand_plus;
            } else if (kStrand == "minus") {
                m_Strand = eNa_strand_minus;
            } else {
                abort();
            }
        }
    }

    // set the sequence range
    if (args[kArgQueryLocation]) {
        s_TokenizeSequenceRange(args[kArgQueryLocation].AsString(), m_Range, 
                                "Invalid specification of query location");
    }

    m_UseLCaseMask = static_cast<bool>(args[kArgUseLCaseMasking]);
    m_BelieveQueryDefline = static_cast<bool>(args[kArgParseQueryDefline]);
}

CBlastDatabaseArgs::CBlastDatabaseArgs(bool request_mol_type /* = false */,
                                       bool is_rpsblast /* = false */)
    : m_RequestMoleculeType(request_mol_type), m_IsRpsBlast(is_rpsblast),
    m_IsProtein(true)
{}

bool
CBlastDatabaseArgs::HasBeenSet(const CArgs& args)
{
    if ( (args.Exist(kArgDb) && args[kArgDb].HasValue()) ||
         (args.Exist(kArgSubject) && args[kArgSubject].HasValue()) ) {
        return true;
    }
    return false;
}

void
CBlastDatabaseArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("General search options");
    // database filename
    arg_desc.AddOptionalKey(kArgDb, "database_name", "BLAST database name", 
                            CArgDescriptions::eString);
    arg_desc.SetCurrentGroup("");

    if (m_RequestMoleculeType) {
        arg_desc.AddKey(kArgDbType, "database_type", 
                        "BLAST database molecule type",
                        CArgDescriptions::eString);
        arg_desc.SetConstraint(kArgDbType, 
                               &(*new CArgAllow_Strings, "prot", "nucl"));
    }

    const string* kDatabaseArgs[] =  
        { &kArgDb, &kArgGiList, &kArgNegativeGiList, NULL };

    // DB size
    arg_desc.SetCurrentGroup("Statistical options");
    arg_desc.AddOptionalKey(kArgDbSize, "num_letters", 
                            "Effective length of the database ",
                            CArgDescriptions::eInt8);

    if ( !m_IsRpsBlast) {

        arg_desc.SetCurrentGroup("Restrict search or results");
        // GI list
        arg_desc.AddOptionalKey(kArgGiList, "filename", 
                                "Restrict search of database to list of GIs",
                                CArgDescriptions::eString);
        arg_desc.AddOptionalKey(kArgNegativeGiList, "filename", 
            "Restrict search of database to everything except the listed GIs",
            CArgDescriptions::eString);
        arg_desc.SetDependency(kArgGiList, CArgDescriptions::eExcludes, 
                               kArgNegativeGiList);

        // For now, disable pairing -remote with either -gilist or
        // -negative_gilist as this is not implemented in the BLAST server
        arg_desc.SetDependency(kArgGiList, CArgDescriptions::eExcludes, 
                               kArgRemote);
        arg_desc.SetDependency(kArgNegativeGiList, CArgDescriptions::eExcludes, 
                               kArgRemote);

        arg_desc.SetCurrentGroup("BLAST-2-Sequences options");
        // subject sequence input (for bl2seq)
        arg_desc.AddOptionalKey(kArgSubject, "subject_input_file",
                                "Subject sequence(s) to search",
                                CArgDescriptions::eInputFile);
        for (size_t i = 0; kDatabaseArgs[i] != NULL; i++) {
            arg_desc.SetDependency(kArgSubject, CArgDescriptions::eExcludes, 
                                   *kDatabaseArgs[i]);
        }

        // subject location
        arg_desc.AddOptionalKey(kArgSubjectLocation, "range", 
                                "Location on the subject sequence "
                                "(Format: start-stop)",
                                CArgDescriptions::eString);
        for (size_t i = 0; kDatabaseArgs[i] != NULL; i++) {
            arg_desc.SetDependency(kArgSubjectLocation, 
                                   CArgDescriptions::eExcludes, 
                                   *kDatabaseArgs[i]);
        }
    }

    arg_desc.SetCurrentGroup("");
}

/** 
 * @brief Process gi lists command line arguments
 * 
 * @param args CArgs object representing command line arguments read [in]
 * @param argument_name name of the command line option [in]
 * @param filename the value of the option [out]
 * @param gis the contents of the file, if a remote BLAST search is needed (if
 * not, this will be empty upon function exit [out]
 */
static void
s_ProcessGiListArgument(const CArgs& args, 
                        const string& argument_name, 
                        string& filename, 
                        vector<int>& gis)
{
    gis.clear();
    if (args.Exist(argument_name) && args[argument_name]) {
        filename.assign(args[argument_name].AsString());
        /// This is only needed if the gi list is to be submitted remotely as
        /// it needs to be sent over the network OR if we need to export the
        /// object as a search strategy
        if ((args.Exist(kArgRemote) && args[kArgRemote] && 
            CFile(filename).Exists()) ||
            (args[kArgOutputSearchStrategy].HasValue())) {
            SeqDB_ReadGiList(filename, gis);
        }
    }
}

void
CBlastDatabaseArgs::ExtractAlgorithmOptions(const CArgs& args,
                                            CBlastOptions& opts)
{
    EMoleculeType mol_type = Blast_SubjectIsNucleotide(opts.GetProgramType())
        ? CSearchDatabase::eBlastDbIsNucleotide
        : CSearchDatabase::eBlastDbIsProtein;
    m_IsProtein = (mol_type == CSearchDatabase::eBlastDbIsProtein);
    
    if (args.Exist(kArgDb) && args[kArgDb]) {

        m_SearchDb.Reset(new CSearchDatabase(args[kArgDb].AsString(), 
                                             mol_type));

        vector<int> gis;
        s_ProcessGiListArgument(args, kArgGiList, m_GiListFileName, gis);
        if ( !gis.empty() ) 
            m_SearchDb->SetGiListLimitation(gis);

        s_ProcessGiListArgument(args, kArgNegativeGiList,
                                m_NegativeGiListFileName, gis);
        if ( !gis.empty() ) 
            m_SearchDb->SetNegativeGiListLimitation(gis);

    } else if (args.Exist(kArgSubject) && args[kArgSubject]) {

        if (args.Exist(kArgRemote) && args[kArgRemote]) {
            NCBI_THROW(CBlastException, eInvalidArgument,
               "Submission of remote BLAST-2-Sequences is not supported\n"
               "Please visit the NCBI web site to submit your search");
        }
        else {
            CNcbiIstream& subj_input_stream = args[kArgSubject].AsInputFile();
            TSeqRange subj_range;
            if (args.Exist(kArgSubjectLocation) && args[kArgSubjectLocation]) {
                s_TokenizeSequenceRange(args[kArgSubjectLocation].AsString(), 
                                subj_range,
                                "Invalid specification of subject location");
            }

            CRef<blast::CBlastQueryVector> subjects;
            m_Scope = ReadSequencesToBlast(subj_input_stream, IsProtein(),
                                           subj_range, subjects);
            m_Subjects.Reset(new blast::CObjMgr_QueryFactory(*subjects));
        }
    } else {
        NCBI_THROW(CBlastException, eInvalidArgument,
           "Either a BLAST database or subject sequence(s) must be specified");
    }

    if (opts.GetEffectiveSearchSpace() != 0) {
        // no need to set any other options, as this trumps them
        return;
    }

    if (args[kArgDbSize]) {
        opts.SetDbLength(args[kArgDbSize].AsInt8());
    }

}

void
CFormattingArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Formatting options");

    // alignment view
    arg_desc.AddDefaultKey(kArgOutputFormat, "format", 
                    "alignment view options:\n"
                    "  0 = pairwise,\n"
                    "  1 = query-anchored showing identities,\n"
                    "  2 = query-anchored no identities,\n"
                    "  3 = flat query-anchored, show identities,\n"
                    "  4 = flat query-anchored, no identities,\n"
                    "  5 = XML Blast output,\n"
                    "  6 = tabular,\n"
                    "  7 = tabular with comment lines,\n"
                    "  8 = Text ASN.1,\n"
                    "  9 = Binary ASN.1\n",
                   CArgDescriptions::eInteger,
                   NStr::IntToString(kDfltArgOutputFormat));
    arg_desc.SetConstraint(kArgOutputFormat, new CArgAllow_Integers(0, 9));

    // show GIs in deflines
    arg_desc.AddFlag(kArgShowGIs, "Show NCBI GIs in deflines?", true);

    // number of one-line descriptions to display
    arg_desc.AddDefaultKey(kArgNumDescriptions, "int_value",
                 "Number of database sequences to show one-line "
                 "descriptions for",
                 CArgDescriptions::eInteger,
                 NStr::IntToString(kDfltArgNumDescriptions));
    arg_desc.SetConstraint(kArgNumDescriptions, 
                           new CArgAllowValuesGreaterThanOrEqual(0));

    // number of alignments per DB sequence
    arg_desc.AddDefaultKey(kArgNumAlignments, "int_value",
                 "Number of database sequences to show alignments for",
                 CArgDescriptions::eInteger, 
                 NStr::IntToString(kDfltArgNumAlignments));
    arg_desc.SetConstraint(kArgNumAlignments, 
                           new CArgAllowValuesGreaterThanOrEqual(0));

    // Produce HTML?
    arg_desc.AddFlag(kArgProduceHtml, "Produce HTML output?", true);

    arg_desc.SetCurrentGroup("");
}

void
CFormattingArgs::ExtractAlgorithmOptions(const CArgs& args,
                                         CBlastOptions& opt)
{
    if (args[kArgOutputFormat]) {
        int val(args[kArgOutputFormat].AsInteger());
        if (val < 0 || val >= static_cast<int>(eEndValue)) {
            string msg("Formatting choice is out of range");
            throw std::out_of_range(msg);
        }
        m_OutputFormat = static_cast<EOutputFormat>(val);
    }

    m_ShowGis = static_cast<bool>(args[kArgShowGIs]);

    if (args[kArgNumDescriptions]) {
        m_NumDescriptions = args[kArgNumDescriptions].AsInteger();
    } 

    if (args[kArgNumAlignments]) {
        m_NumAlignments = args[kArgNumAlignments].AsInteger();
    }

    if (m_NumDescriptions == 0 && m_NumAlignments == 0) {
        string msg("Either -");
        msg += kArgNumDescriptions + " or -" + kArgNumAlignments + " must ";
        msg += "be non-zero";
        NCBI_THROW(CBlastException, eInvalidArgument, msg);
    }
    else
        opt.SetHitlistSize(MAX(m_NumDescriptions, m_NumAlignments));

    m_Html = static_cast<bool>(args[kArgProduceHtml]);
}

void
CMTArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    const int kMinValue = static_cast<int>(CThreadable::kMinNumThreads);

    // number of threads
    arg_desc.SetCurrentGroup("misc");
    arg_desc.AddDefaultKey(kArgNumThreads, "int_value",
                           "Number of threads to use in preliminary stage "
                           "of the search", CArgDescriptions::eInteger, 
                           NStr::IntToString(kMinValue));
    arg_desc.SetConstraint(kArgNumThreads, 
                           new CArgAllowValuesGreaterThanOrEqual(kMinValue));
    arg_desc.SetCurrentGroup("");
}

void
CMTArgs::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& /* opts */)
{
    if (args.Exist(kArgNumThreads) &&
        args[kArgNumThreads].HasValue()) {  // could be cancelled by the exclusion in CRemoteArgs
        m_NumThreads = args[kArgNumThreads].AsInteger();
    }
}

void
CRemoteArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("misc");
    arg_desc.AddFlag(kArgRemote, "Execute search remotely?", true);
    arg_desc.SetDependency(kArgRemote,
                           CArgDescriptions::eExcludes,
                           kArgNumThreads);

    arg_desc.SetCurrentGroup("");
}

void
CRemoteArgs::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& /* opts */)
{
    if (args.Exist(kArgRemote)) {
        m_IsRemote = static_cast<bool>(args[kArgRemote]);
    }
}

void
CDebugArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
#if _DEBUG
    arg_desc.SetCurrentGroup("misc");
    arg_desc.AddFlag("verbose", "Produce verbose output (show BLAST options)?",
                     true);
    arg_desc.AddFlag("remote_verbose", 
                     "Produce verbose output for remote searches?", true);
    arg_desc.SetCurrentGroup("");
#endif /* DEBUG */
}

void
CDebugArgs::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& /* opts */)
{
#if _DEBUG
    m_DebugOutput = static_cast<bool>(args["verbose"]);
    m_RmtDebugOutput = static_cast<bool>(args["remote_verbose"]);
#endif /* DEBUG */
}

void
CCullingArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // culling limit
    arg_desc.SetCurrentGroup("Restrict search or results");
    arg_desc.AddDefaultKey(kArgCullingLimit, "int_value",
                     "If the query range of a hit is enveloped by that of at "
                     "least this many higher-scoring hits, delete the hit",
                     CArgDescriptions::eInteger,
                     NStr::IntToString(kDfltArgCullingLimit));
    arg_desc.SetConstraint(kArgCullingLimit, 
                           new CArgAllowValuesGreaterThanOrEqual(0));
    arg_desc.SetCurrentGroup("");
}

void
CCullingArgs::ExtractAlgorithmOptions(const CArgs& args,
                                      CBlastOptions& opts)
{
    if (args[kArgCullingLimit]) {
        opts.SetCullingLimit(args[kArgCullingLimit].AsInteger());
    }
}

void
CMbIndexArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("General search options");
    arg_desc.AddOptionalKey( 
            kArgUseIndex, "boolean",
            "Use MegaBLAST database index",
            CArgDescriptions::eBoolean );
    arg_desc.AddOptionalKey(
            kArgIndexName, "string",
            "MegaBLAST database index name",
            CArgDescriptions::eString );
    arg_desc.SetCurrentGroup( "" );
}

void
CMbIndexArgs::ExtractAlgorithmOptions(const CArgs& args,
                                      CBlastOptions& opts)
{
    // MB Index does not apply to Blast2Sequences
    if( args.Exist( kArgUseIndex ) &&
        !(args.Exist( kArgSubject ) && args[kArgSubject])) {

        bool use_index   = true;
        bool force_index = false;

        if( args[kArgUseIndex] ) {
            if( args[kArgUseIndex].AsBoolean() ) force_index = true;
            else use_index = false;
        }

        if( args.Exist( kTask ) && args[kTask] && 
                args[kTask].AsString() != "megablast" ) {
            use_index = false;
        }

        if( use_index ) {
            string index_name;

            if( args.Exist( kArgIndexName ) && args[kArgIndexName] ) {
                index_name = args[kArgIndexName].AsString();
            }
            else if( args.Exist( kArgDb ) && args[kArgDb] ) {
                index_name = args[kArgDb].AsString();
            }
            else {
                NCBI_THROW( CBlastException, eInvalidArgument,
                        "Can not deduce database index name" );
            }
    
            opts.SetUseIndex( true, index_name, force_index );
        }
    }
}

void
CStdCmdLineArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Input query options");

    // query filename
    arg_desc.AddDefaultKey(kArgQuery, "input_file", 
                     "Input file name",
                     CArgDescriptions::eInputFile, kDfltArgQuery);

    arg_desc.SetCurrentGroup("General search options");

    // report output file
    arg_desc.AddDefaultKey(kArgOutput, "output_file", 
                   "Output file name",
                   CArgDescriptions::eOutputFile, "-");

    arg_desc.SetCurrentGroup("");
}

void
CStdCmdLineArgs::ExtractAlgorithmOptions(const CArgs& args,
                                         CBlastOptions& /* opt */)
{
    if (args.Exist(kArgQuery) && args[kArgQuery].HasValue() &&
        m_InputStream == NULL) {
        m_InputStream = &args[kArgQuery].AsInputFile();
    }
    m_OutputStream = &args[kArgOutput].AsOutputFile();
}

CNcbiIstream&
CStdCmdLineArgs::GetInputStream() const
{
    // programmer must ensure the ExtractAlgorithmOptions method is called
    // before this method is invoked
    if ( !m_InputStream ) {
        abort();
    }
    return *m_InputStream;
}

CNcbiOstream&
CStdCmdLineArgs::GetOutputStream() const
{
    // programmer must ensure the ExtractAlgorithmOptions method is called
    // before this method is invoked
    _ASSERT(m_OutputStream);
    return *m_OutputStream;
}

void
CStdCmdLineArgs::SetInputStream(CRef<CTmpFile> input_file)
{
    m_QueryTmpInputFile = input_file;
    m_InputStream = &input_file->AsInputFile(CTmpFile::eIfExists_Throw);
}

void
CSearchStrategyArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Search strategy options");

    arg_desc.AddOptionalKey(kArgInputSearchStrategy,
                            "filename",
                            "Search strategy to use", 
                            CArgDescriptions::eInputFile);
    arg_desc.AddOptionalKey(kArgOutputSearchStrategy,
                            "filename",
                            "File name to record the search strategy used", 
                            CArgDescriptions::eOutputFile);
    arg_desc.SetDependency(kArgInputSearchStrategy,
                           CArgDescriptions::eExcludes,
                           kArgOutputSearchStrategy);

    arg_desc.SetCurrentGroup("");
}

void
CSearchStrategyArgs::ExtractAlgorithmOptions(const CArgs& /* cmd_line_args */,
                                             CBlastOptions& /* options */)
{
}

CNcbiIstream* 
CSearchStrategyArgs::GetImportStream(const CArgs& args) const
{
    CNcbiIstream* retval = NULL;
    if (args[kArgInputSearchStrategy].HasValue()) {
        retval = &args[kArgInputSearchStrategy].AsInputFile();
    }
    return retval;
}

CNcbiOstream* 
CSearchStrategyArgs::GetExportStream(const CArgs& args) const
{
    CNcbiOstream* retval = NULL;
    if (args[kArgOutputSearchStrategy].HasValue()) {
        retval = &args[kArgOutputSearchStrategy].AsOutputFile();
    }
    return retval;
}

CBlastAppArgs::CBlastAppArgs()
{
    m_SearchStrategyArgs.Reset(new CSearchStrategyArgs);
    m_Args.push_back(CRef<IBlastCmdLineArgs>(&*m_SearchStrategyArgs));
}

CArgDescriptions*
CBlastAppArgs::SetCommandLine()
{
    return SetUpCommandLineArguments(m_Args);
}

CRef<CBlastOptionsHandle>
CBlastAppArgs::SetOptions(const CArgs& args)
{
    // We're recovering from a saved strategy, so we need to still extract
    // certain options from the command line, include overriding query
    // and/or database
    if (m_OptsHandle.NotEmpty()) {
        CBlastOptions& opts = m_OptsHandle->SetOptions();
        // invoke ExtractAlgorithmOptions on certain argument classes
        m_FormattingArgs->ExtractAlgorithmOptions(args, opts);
        m_QueryOptsArgs->ExtractAlgorithmOptions(args, opts);
        m_StdCmdLineArgs->ExtractAlgorithmOptions(args, opts);
        m_RemoteArgs->ExtractAlgorithmOptions(args, opts);
        m_DebugArgs->ExtractAlgorithmOptions(args, opts);
        if (CBlastDatabaseArgs::HasBeenSet(args)) {
            m_BlastDbArgs->ExtractAlgorithmOptions(args, opts);
        }
        return m_OptsHandle;
    }

    CBlastOptions::EAPILocality locality = 
        (args.Exist(kArgRemote) && args[kArgRemote]) 
        ? CBlastOptions::eRemote 
        : CBlastOptions::eLocal;

    // This is needed as a CRemoteBlast object and its options are instantiated
    // to create the search strategy
    if (GetExportSearchStrategyStream(args)) {
        locality = CBlastOptions::eBoth;
    }

    CRef<CBlastOptionsHandle> handle(x_CreateOptionsHandle(locality, args));
    CBlastOptions& opts = handle->SetOptions();
    NON_CONST_ITERATE(TBlastCmdLineArgs, arg, m_Args) {
        (*arg)->ExtractAlgorithmOptions(args, opts);
    }
    return handle;
}

void CBlastAppArgs::SetTask(const string& task)
{
#if _DEBUG
    ThrowIfInvalidTask(task);
#endif
    m_Task.assign(task);
}

END_SCOPE(blast)
END_NCBI_SCOPE
