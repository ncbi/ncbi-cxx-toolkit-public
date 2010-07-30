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
#include <algo/blast/core/hspfilter_besthit.h>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <util/format_guess.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <algo/blast/blastinput/blast_input.hpp>    // for CInputException
#include <connect/ncbi_connutil.h>

#include <algo/blast/api/msa_pssm_input.hpp>    // for CPsiBlastInputClustalW
#include <algo/blast/api/pssm_engine.hpp>       // for CPssmEngine

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);
USING_SCOPE(align_format);

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
                             CBlastVersion().Print());
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

    if ( !m_IsRpsBlast && !m_IsTblastx) {
        // gap open penalty
        arg_desc.AddOptionalKey(kArgGapOpen, "open_penalty", 
                                "Cost to open a gap", 
                                CArgDescriptions::eInteger);

        // gap extend penalty
        arg_desc.AddOptionalKey(kArgGapExtend, "extend_penalty",
                               "Cost to extend a gap", 
                               CArgDescriptions::eInteger);
    }


    if (m_ShowPercentIdentity) {
        arg_desc.SetCurrentGroup("Restrict search or results");
        arg_desc.AddOptionalKey(kArgPercentIdentity, "float_value",
                                "Percent identity",
                                CArgDescriptions::eDouble);
        arg_desc.SetConstraint(kArgPercentIdentity,
                               new CArgAllow_Doubles(0.0, 100.0));
    }

    arg_desc.SetCurrentGroup("Extension options");
    // ungapped X-drop
    // Default values: blastn=20, megablast=10, others=7
    arg_desc.AddOptionalKey(kArgUngappedXDropoff, "float_value", 
                            "X-dropoff value (in bits) for ungapped extensions",
                            CArgDescriptions::eDouble);

    // Tblastx is ungapped only.
    if (!m_IsTblastx) {
         // initial gapped X-drop
         // Default values: blastn=30, megablast=20, tblastx=0, others=15
         arg_desc.AddOptionalKey(kArgGappedXDropoff, "float_value", 
                 "X-dropoff value (in bits) for preliminary gapped extensions",
                 CArgDescriptions::eDouble);

         // final gapped X-drop
         // Default values: blastn/megablast=50, tblastx=0, others=25
         arg_desc.AddOptionalKey(kArgFinalGappedXDropoff, "float_value", 
                         "X-dropoff value (in bits) for final gapped alignment",
                         CArgDescriptions::eDouble);
    }

    arg_desc.SetCurrentGroup("Statistical options");
    // effective search space
    // Default value is the real size
    arg_desc.AddOptionalKey(kArgEffSearchSpace, "int_value", 
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
         BLAST_GetProteinGapExistenceExtendParams
             (args[kArgMatrixName].AsString().c_str(), &gap_open, &gap_extend);

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

    if (args.Exist(kArgGappedXDropoff) && args[kArgGappedXDropoff]) {
        opt.SetGapXDropoff(args[kArgGappedXDropoff].AsDouble());
    }

    if (args.Exist(kArgFinalGappedXDropoff) && args[kArgFinalGappedXDropoff]) {
        opt.SetGapXDropoffFinal(args[kArgFinalGappedXDropoff].AsDouble());
    }

    if (args[kArgWordSize]) {
        if (m_QueryIsProtein && args[kArgWordSize].AsInteger() > 5)
           opt.SetLookupTableType(eCompressedAaLookupTable);
        opt.SetWordSize(args[kArgWordSize].AsInteger());
    }

    if (args[kArgEffSearchSpace]) {
        opt.SetEffectiveSearchSpace(args[kArgEffSearchSpace].AsInt8());
    }

    if (args.Exist(kArgPercentIdentity) && args[kArgPercentIdentity]) {
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
        
        arg_desc.AddOptionalKey(kArgWindowMaskerTaxId, "window_masker_taxid",
                "Enable WindowMasker filtering using a Taxonomic ID",
                CArgDescriptions::eInteger);

        arg_desc.AddOptionalKey(kArgWindowMaskerDatabase, "window_masker_db",
                "Enable WindowMasker filtering using this repeats database.",
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
    if (output.size() != 3) {
        NCBI_THROW(CInputException, eInvalidInput,
                   "Invalid number of arguments to filtering option");
    }
}

void
CFilteringArgs::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& opt)
{
    if (args[kArgLookupTableMaskingOnly]) {
        opt.SetMaskAtHash(args[kArgLookupTableMaskingOnly].AsBoolean());
    }

    vector<string> tokens;

    try {
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
    } catch (const CStringException& e) {
        if (e.GetErrCode() == CStringException::eConvert) {
            NCBI_THROW(CInputException, eInvalidInput,
                       "Invalid input for filtering parameters");
        }
    }
    
    int filter_dbs = 0;
    
    if (args.Exist(kArgFilteringDb) && args[kArgFilteringDb]) {
        opt.SetRepeatFilteringDB(args[kArgFilteringDb].AsString().c_str());
        filter_dbs++;
    }
    
    if (args.Exist(kArgWindowMaskerTaxId) &&
        args[kArgWindowMaskerTaxId]) {
        
        opt.SetWindowMaskerTaxId
            (args[kArgWindowMaskerTaxId].AsInteger());
        
        filter_dbs++;
    }
    
    if (args.Exist(kArgWindowMaskerDatabase) &&
        args[kArgWindowMaskerDatabase]) {
        
        opt.SetWindowMaskerDatabase
            (args[kArgWindowMaskerDatabase].AsString().c_str());
        
        filter_dbs++;
    }
    
    if (filter_dbs > 1) {
        string msg =
            string("Please specify at most one of ") + kArgFilteringDb + ", " +
            kArgWindowMaskerTaxId + ", or " + kArgWindowMaskerDatabase + ".";
        
        NCBI_THROW(CInputException, eInvalidInput, msg);
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
COffDiagonalRangeArg::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Extension options");
    // 2-hit wordfinder off diagonal range
    arg_desc.AddDefaultKey(kArgOffDiagonalRange, "int_value", 
                            "Number of off-diagonals to search for the 2nd hit, "
                            "use 0 to turn off",
                            CArgDescriptions::eInteger,
                            NStr::IntToString(kDfltOffDiagonalRange));
    arg_desc.SetConstraint(kArgOffDiagonalRange, 
                           new CArgAllowValuesGreaterThanOrEqual(0));
    arg_desc.SetCurrentGroup("");
}

void
COffDiagonalRangeArg::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& opt)
{
    if (args[kArgOffDiagonalRange]) {
        opt.SetOffDiagonalRange(args[kArgOffDiagonalRange].AsInteger());
    } else {
        opt.SetOffDiagonalRange(0);
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
    arg_desc.AddOptionalKey(kArgMatrixName, "matrix_name",
                           "Scoring matrix name (normally BLOSUM62)",
                           CArgDescriptions::eString); 
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
    if (cmd_line_args[kArgMismatch]) {
        options.SetMismatchPenalty(cmd_line_args[kArgMismatch].AsInteger());
    }
    if (cmd_line_args[kArgMatch]) {
        options.SetMatchReward(cmd_line_args[kArgMatch].AsInteger());
    }

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
    // composition based statistics, keep in sync with ECompoAdjustModes
    // documentation in composition_constants.h
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

    arg_desc.SetCurrentGroup("Miscellaneous options");
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
    if (program == eBlastp || program == eTblastn || 
        program == ePSIBlast || program == ePSITblastn) {

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

        if(program == ePSITblastn) {
            compo_mode = eNoCompositionBasedStats;
        }

        if (ungapped && *ungapped && compo_mode != eNoCompositionBasedStats) {
            NCBI_THROW(CInputException, eInvalidInput, 
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
                               "Genetic code to use to translate "
                               "database/subjects",
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
        arg_desc.SetDependency(kArgPSIInputChkPntFile,
                                CArgDescriptions::eExcludes,
                                kArgRemote);
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
        arg_desc.AddOptionalKey(kArgAsciiPssmOutputFile, "ascii_mtx_file",
                                "File name to store ASCII version of PSSM",
                                CArgDescriptions::eOutputFile);
        // MSA restart file
        arg_desc.AddOptionalKey(kArgMSAInputFile, "align_restart",
                                "File name of multiple sequence alignment to "
                                "restart PSI-BLAST",
                                CArgDescriptions::eInputFile);
        arg_desc.SetDependency(kArgMSAInputFile,
                               CArgDescriptions::eExcludes,
                               kArgPSIInputChkPntFile);
        arg_desc.SetDependency(kArgMSAInputFile,
                               CArgDescriptions::eExcludes,
                               kArgQuery);
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

/// Auxiliary function to create a PSSM from a multiple sequence alignment file
static CRef<CPssmWithParameters>
s_CreatePssmFromMsa(CNcbiIstream& input_stream, CBlastOptions& opt,
                    bool save_ascii_pssm)
{
    // FIXME get these from CBlastOptions
    CPSIBlastOptions psiblast_opts;
    PSIBlastOptionsNew(&psiblast_opts); 

    CPSIDiagnosticsRequest diags(PSIDiagnosticsRequestNewEx(save_ascii_pssm));
    // FIXME: if query is provided, pass it in in ncbistdaa + query length!
    CPsiBlastInputClustalW pssm_input(input_stream, *psiblast_opts,
                                      opt.GetMatrixName(), diags, NULL, 0,
                                      opt.GetGapOpeningCost(),
                                      opt.GetGapExtensionCost());
    CPssmEngine pssm_engine(&pssm_input);
    return pssm_engine.Run();
}

void
CPsiBlastArgs::ExtractAlgorithmOptions(const CArgs& args,
                                       CBlastOptions& opt)
{
    if (m_DbTarget == eProteinDb) {
        if (args[kArgPSINumIterations]) {
            m_NumIterations = args[kArgPSINumIterations].AsInteger();
        }
        if (args.Exist(kArgPSIOutputChkPntFile) &&
            args[kArgPSIOutputChkPntFile]) {
            m_CheckPointOutput.Reset
                (new CAutoOutputFileReset
                 (args[kArgPSIOutputChkPntFile].AsString())); 
        }
        const bool kSaveAsciiPssm = args[kArgAsciiPssmOutputFile];
        if (kSaveAsciiPssm) {
            m_AsciiMatrixOutput.Reset
                (new CAutoOutputFileReset
                 (args[kArgAsciiPssmOutputFile].AsString()));
        }
        if (args[kArgMSAInputFile]) {
            CNcbiIstream& in = args[kArgMSAInputFile].AsInputFile();
            m_Pssm = s_CreatePssmFromMsa(in, opt, kSaveAsciiPssm);
        }
    }

    if (args.Exist(kArgPSIInputChkPntFile) && args[kArgPSIInputChkPntFile]) {
        CNcbiIstream& in = args[kArgPSIInputChkPntFile].AsInputFile();
        _ASSERT(m_Pssm.Empty());
        m_Pssm.Reset(new CPssmWithParameters);
        try {
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
                NCBI_THROW(CInputException, eInvalidInput, 
                           "Unsupported format for PSSM");
            }
        } catch (const CSerialException&) {
            string msg("Unrecognized format for PSSM in ");
            msg += args[kArgPSIInputChkPntFile].AsString() + " (must be ";
            msg += "PssmWithParameters)";
            NCBI_THROW(CInputException, eInvalidInput, msg);
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
        CNcbiIstream& in = args[kArgPHIPatternFile].AsInputFile();
        in.clear();
        in.seekg(0);
        char buffer[4096];
        string line;
        string pattern;
        string name;
        while (in.getline(buffer, 4096)) {
           line = buffer;
           string ltype = line.substr(0, 2);
           if (ltype == "ID") 
             name = line.substr(5);
           else if (ltype == "PA")
             pattern = line.substr(5);
        }
        if (!pattern.empty())
            opt.SetPHIPattern(pattern.c_str(), 
               (Blast_QueryIsNucleotide(opt.GetProgramType())
               ? true : false));
        else
            NCBI_THROW(CInputException, eInvalidInput, 
                       "PHI pattern not read");
    }
}

void
CQueryOptionsArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{

    arg_desc.SetCurrentGroup("Query filtering options");
    // lowercase masking
    arg_desc.AddFlag(kArgUseLCaseMasking, 
         "Use lower case filtering in query and subject sequence(s)?", true);

    arg_desc.SetCurrentGroup("Input query options");
    // query location
    arg_desc.AddOptionalKey(kArgQueryLocation, "range", 
                            "Location on the query sequence in 1-based offsets "
                            "(Format: start-stop)",
                            CArgDescriptions::eString);

    if ( !m_QueryCannotBeNucl ) {
        // search strands
        arg_desc.AddDefaultKey(kArgStrand, "strand", 
                         "Query strand(s) to search against database/subject",
                         CArgDescriptions::eString, kDfltArgStrand);
        arg_desc.SetConstraint(kArgStrand, &(*new CArgAllow_Strings, 
                                             kDfltArgStrand, "plus", "minus"));
    }

    arg_desc.SetCurrentGroup("Miscellaneous options");
    arg_desc.AddFlag(kArgParseDeflines,
                 "Should the query and subject defline(s) be parsed?", true);

    arg_desc.SetCurrentGroup("");
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
        m_Range = ParseSequenceRange(args[kArgQueryLocation].AsString(), 
                                     "Invalid specification of query location");
    }

    m_UseLCaseMask = static_cast<bool>(args[kArgUseLCaseMasking]);
    m_ParseDeflines = static_cast<bool>(args[kArgParseDeflines]);
}

CBlastDatabaseArgs::CBlastDatabaseArgs(bool request_mol_type /* = false */,
                                       bool is_rpsblast /* = false */)
    : m_RequestMoleculeType(request_mol_type), m_IsRpsBlast(is_rpsblast),
    m_IsProtein(true), m_SupportsDatabaseMasking(false)
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

    vector<string> database_args;
    database_args.push_back(kArgDb);
    database_args.push_back(kArgGiList);
    database_args.push_back(kArgSeqIdList);
    database_args.push_back(kArgNegativeGiList);
    if (m_SupportsDatabaseMasking) {
        database_args.push_back(kArgDbSoftMask);
    }

    // DB size
    arg_desc.SetCurrentGroup("Statistical options");
    arg_desc.AddOptionalKey(kArgDbSize, "num_letters", 
                            "Effective length of the database ",
                            CArgDescriptions::eInt8);

    arg_desc.SetCurrentGroup("Restrict search or results");
    // GI list
    arg_desc.AddOptionalKey(kArgGiList, "filename", 
                            "Restrict search of database to list of GI's",
                            CArgDescriptions::eString);
    // SeqId list
    arg_desc.AddOptionalKey(kArgSeqIdList, "filename", 
                            "Restrict search of database to list of SeqId's",
                            CArgDescriptions::eString);
    // Negative GI list
    arg_desc.AddOptionalKey(kArgNegativeGiList, "filename", 
        "Restrict search of database to everything except the listed GIs",
        CArgDescriptions::eString);
    arg_desc.SetDependency(kArgGiList, CArgDescriptions::eExcludes, 
                           kArgNegativeGiList);
    arg_desc.SetDependency(kArgGiList, CArgDescriptions::eExcludes, 
                           kArgSeqIdList);
    arg_desc.SetDependency(kArgSeqIdList, CArgDescriptions::eExcludes, 
                           kArgNegativeGiList);
    // Entrez Query
    arg_desc.AddOptionalKey(kArgEntrezQuery, "entrez_query", 
                            "Restrict search with the given Entrez query",
                            CArgDescriptions::eString);

    // For now, disable pairing -remote with either -gilist or
    // -negative_gilist as this is not implemented in the BLAST server
    arg_desc.SetDependency(kArgGiList, CArgDescriptions::eExcludes, 
                           kArgRemote);
    arg_desc.SetDependency(kArgSeqIdList, CArgDescriptions::eExcludes, 
                           kArgRemote);
    arg_desc.SetDependency(kArgNegativeGiList, CArgDescriptions::eExcludes, 
                           kArgRemote);

    // Entrez query currently requires the -remote option
    arg_desc.SetDependency(kArgEntrezQuery, CArgDescriptions::eRequires, 
                           kArgRemote);

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    // Masking of database
    if (m_SupportsDatabaseMasking) {
        arg_desc.AddOptionalKey(kArgDbSoftMask, 
                "filtering_algorithm",
                "Filtering algorithm ID to apply to the BLAST database as soft "
                "masking",
                CArgDescriptions::eString);
    }
#endif

    // There is no RPS-BLAST 2 sequences
    if ( !m_IsRpsBlast ) {
        arg_desc.SetCurrentGroup("BLAST-2-Sequences options");
        // subject sequence input (for bl2seq)
        arg_desc.AddOptionalKey(kArgSubject, "subject_input_file",
                                "Subject sequence(s) to search",
                                CArgDescriptions::eInputFile);
        ITERATE(vector<string>, dbarg, database_args) {
            arg_desc.SetDependency(kArgSubject, CArgDescriptions::eExcludes, 
                                   *dbarg);
        }

        // subject location
        arg_desc.AddOptionalKey(kArgSubjectLocation, "range", 
                        "Location on the subject sequence in 1-based offsets "
                        "(Format: start-stop)",
                        CArgDescriptions::eString);
        ITERATE(vector<string>, dbarg, database_args) {
            arg_desc.SetDependency(kArgSubjectLocation, 
                                   CArgDescriptions::eExcludes, 
                                   *dbarg);
        }
        // Because Blast4-subject does not support Seq-locs, specifying a
        // subject range does not work for remote searches
        arg_desc.SetDependency(kArgSubjectLocation, 
                               CArgDescriptions::eExcludes, kArgRemote);
    }

    arg_desc.SetCurrentGroup("");
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

        if (args.Exist(kArgGiList) && args[kArgGiList]) {
            string fn(SeqDB_ResolveDbPath(args[kArgGiList].AsString()));
            m_SearchDb->SetGiList(CRef<CSeqDBGiList> (new CSeqDBFileGiList(fn)));
        } else if (args.Exist(kArgNegativeGiList) && args[kArgNegativeGiList]) {
            string fn(SeqDB_ResolveDbPath(args[kArgNegativeGiList].AsString()));
            m_SearchDb->SetNegativeGiList(CRef<CSeqDBGiList> (new CSeqDBFileGiList(fn)));
        } else if (args.Exist(kArgSeqIdList) && args[kArgSeqIdList]) {
            string fn(SeqDB_ResolveDbPath(args[kArgSeqIdList].AsString()));
            m_SearchDb->SetSeqIdList(CRef<CSeqDBGiList> (new CSeqDBFileGiList(fn,
                             CSeqDBFileGiList::eSeqIdList)));
        }

        if (args.Exist(kArgEntrezQuery) && args[kArgEntrezQuery])
            m_SearchDb->SetEntrezQueryLimitation(args[kArgEntrezQuery].AsString());

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
        if (args.Exist(kArgDbSoftMask) && args[kArgDbSoftMask]) {
            // TODO only soft masking for now
            m_SearchDb->SetFilteringAlgorithm(args[kArgDbSoftMask].AsString(), DB_MASK_SOFT);
        }
#endif
    } else if (args.Exist(kArgSubject) && args[kArgSubject]) {

        CNcbiIstream& subj_input_stream = args[kArgSubject].AsInputFile();
        TSeqRange subj_range;
        if (args.Exist(kArgSubjectLocation) && args[kArgSubjectLocation]) {
            subj_range = 
                ParseSequenceRange(args[kArgSubjectLocation].AsString(), 
                            "Invalid specification of subject location");
        }

        const bool parse_deflines = args.Exist(kArgParseDeflines) 
            ? args[kArgParseDeflines]
            : kDfltArgParseDeflines;
        const bool use_lcase_masks = args.Exist(kArgUseLCaseMasking)
            ? args[kArgUseLCaseMasking]
            : kDfltArgUseLCaseMasking;
        CRef<blast::CBlastQueryVector> subjects;
        m_Scope = ReadSequencesToBlast(subj_input_stream, IsProtein(),
                                       subj_range, parse_deflines,
                                       use_lcase_masks, subjects);
        m_Subjects.Reset(new blast::CObjMgr_QueryFactory(*subjects));

    } else {
        NCBI_THROW(CInputException, eInvalidInput,
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

    const string kOutputFormatDescription = string(
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
    "  9 = Binary ASN.1,\n"
    " 10 = Comma-separated values,\n"
    " 11 = BLAST archive format (ASN.1) \n\n"
    "Options 6, 7, and 10 can be additionally configured to produce\n"
    "a custom format specified by space delimited format specifiers.\n"
    "The supported format specifiers are:\n") +
        DescribeTabularOutputFormatSpecifiers() + 
        string("\n");

    // alignment view
    arg_desc.AddDefaultKey(kArgOutputFormat, "format", kOutputFormatDescription,
                           CArgDescriptions::eString, 
                           NStr::IntToString(kDfltArgOutputFormat));

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

    /// Hit list size, listed here for convenience only
    arg_desc.SetCurrentGroup("Restrict search or results");
    arg_desc.AddOptionalKey(kArgMaxTargetSequences, "num_sequences",
                            "Maximum number of aligned sequences to keep",
                            CArgDescriptions::eInteger);
    arg_desc.SetConstraint(kArgMaxTargetSequences,
                           new CArgAllowValuesGreaterThanOrEqual(1));

    arg_desc.SetCurrentGroup("");
}

bool 
CFormattingArgs::ArchiveFormatRequested(const CArgs& args) const
{
    EOutputFormat output_fmt;
    string ignore;
    ParseFormattingString(args, output_fmt, ignore);
    return (output_fmt == eArchiveFormat ? true : false);
}

void
CFormattingArgs::ParseFormattingString(const CArgs& args, 
                                       EOutputFormat& fmt_type, 
                                       string& custom_fmt_spec)
{
    custom_fmt_spec.clear();
    if (args[kArgOutputFormat]) {
        string fmt_choice = 
            NStr::TruncateSpaces(args[kArgOutputFormat].AsString());
        string::size_type pos;
        if ( (pos = fmt_choice.find_first_of(' ')) != string::npos) {
            custom_fmt_spec.assign(fmt_choice, pos+1, 
                                   fmt_choice.size()-(pos+1));
            fmt_choice.erase(pos);
        }
        int val = 0;
        try { val = NStr::StringToInt(fmt_choice); }
        catch (const CStringException&) {   // probably a conversion error
            CNcbiOstrstream os;
            os << "'" << fmt_choice << "' is not a valid output format";
            string msg = CNcbiOstrstreamToString(os);
            NCBI_THROW(CInputException, eInvalidInput, msg);
        }
        if (val < 0 || val >= static_cast<int>(eEndValue)) {
            string msg("Formatting choice is out of range");
            throw std::out_of_range(msg);
        }
        fmt_type = static_cast<EOutputFormat>(val);
        if ( !(fmt_type == eTabular ||
               fmt_type == eTabularWithComments ||
               fmt_type == eCommaSeparatedValues) ) {
               custom_fmt_spec.clear();
        }
    }
}

void
CFormattingArgs::ExtractAlgorithmOptions(const CArgs& args,
                                         CBlastOptions& opt)
{
    ParseFormattingString(args, m_OutputFormat, m_CustomOutputFormatSpec);
    m_ShowGis = static_cast<bool>(args[kArgShowGIs]);

    if (args[kArgNumDescriptions]) {
        m_NumDescriptions = args[kArgNumDescriptions].AsInteger();
    } 

    if (args[kArgNumAlignments]) {
        m_NumAlignments = args[kArgNumAlignments].AsInteger();
    }

    TSeqPos hitlist_size = 0;
    if (args[kArgMaxTargetSequences]) {
        hitlist_size = args[kArgMaxTargetSequences].AsInteger();
        if (hitlist_size > 0 && m_OutputFormat == ePairwise) {
            /* Only non-default values will be overriden */
            string warnings = CalculateFormattingParams(hitlist_size,
                      m_NumDescriptions != kDfltArgNumDescriptions 
                      ? &m_NumDescriptions : 0,
                      m_NumAlignments != kDfltArgNumAlignments 
                      ? &m_NumAlignments : 0);
            if ( !warnings.empty() ) {
                ERR_POST(Warning << warnings);
            }
        }
    }

    if (m_NumDescriptions == 0 && m_NumAlignments == 0 && hitlist_size == 0) {
        string msg("Either -");
        msg += kArgMaxTargetSequences + ", -";
        msg += kArgNumDescriptions + ", or -" + kArgNumAlignments + " must ";
        msg += "be non-zero";
        NCBI_THROW(CInputException, eInvalidInput, msg);
    }
    else if (hitlist_size != 0) {
        opt.SetHitlistSize(hitlist_size);
    } else {
        opt.SetHitlistSize(MAX(m_NumDescriptions, m_NumAlignments));
    }

    m_Html = static_cast<bool>(args[kArgProduceHtml]);
}

void
CMTArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    const int kMinValue = static_cast<int>(CThreadable::kMinNumThreads);

    // number of threads
    arg_desc.SetCurrentGroup("Miscellaneous options");
    arg_desc.AddDefaultKey(kArgNumThreads, "int_value",
                           "Number of threads (CPUs) to use in the BLAST search",
                           CArgDescriptions::eInteger, 
                           NStr::IntToString(kMinValue));
    arg_desc.SetConstraint(kArgNumThreads, 
                           new CArgAllowValuesGreaterThanOrEqual(kMinValue));
    arg_desc.SetDependency(kArgNumThreads,
                           CArgDescriptions::eExcludes,
                           kArgRemote);
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
    arg_desc.SetCurrentGroup("Miscellaneous options");
    arg_desc.AddFlag(kArgRemote, "Execute search remotely?", true);

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
#if _BLAST_DEBUG
    arg_desc.SetCurrentGroup("Miscellaneous options");
    arg_desc.AddFlag("verbose", "Produce verbose output (show BLAST options)",
                     true);
    arg_desc.AddFlag("remote_verbose", 
                     "Produce verbose output for remote searches", true);
    arg_desc.AddFlag("use_test_remote_service", 
                     "Send remote requests to test servers", true);
    arg_desc.SetCurrentGroup("");
#endif /* _BLAST_DEBUG */
}

void
CDebugArgs::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& /* opts */)
{
#if _BLAST_DEBUG
    m_DebugOutput = static_cast<bool>(args["verbose"]);
    m_RmtDebugOutput = static_cast<bool>(args["remote_verbose"]);
    if (args["use_test_remote_service"]) {
        IRWRegistry& reg = CNcbiApplication::Instance()->GetConfig();
        reg.Set("BLAST4", DEF_CONN_REG_SECTION "_" REG_CONN_SERVICE_NAME,
                "blast4_test");
    }
#endif /* _BLAST_DEBUG */
}

void
CHspFilteringArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // culling limit
    arg_desc.SetCurrentGroup("Restrict search or results");
    arg_desc.AddOptionalKey(kArgCullingLimit, "int_value",
                     "If the query range of a hit is enveloped by that of at "
                     "least this many higher-scoring hits, delete the hit",
                     CArgDescriptions::eInteger);
    arg_desc.SetConstraint(kArgCullingLimit, 
    // best hit algorithm arguments
               new CArgAllowValuesGreaterThanOrEqual(kDfltArgCullingLimit));

    arg_desc.AddOptionalKey(kArgBestHitOverhang, "float_value", 
                            "Best Hit algorithm overhang value "
                            "(recommended value: " +
                            NStr::DoubleToString(kDfltArgBestHitOverhang) +
                            ")",
                            CArgDescriptions::eDouble);
    arg_desc.SetConstraint(kArgBestHitOverhang, 
                           new CArgAllowValuesBetween(kBestHit_OverhangMin, 
                                                      kBestHit_OverhangMax));
    arg_desc.SetDependency(kArgBestHitOverhang,
                           CArgDescriptions::eExcludes,
                           kArgCullingLimit);

    arg_desc.AddOptionalKey(kArgBestHitScoreEdge, "float_value", 
                            "Best Hit algorithm score edge value "
                            "(recommended value: " +
                            NStr::DoubleToString(kDfltArgBestHitScoreEdge) +
                            ")",
                            CArgDescriptions::eDouble);
    arg_desc.SetConstraint(kArgBestHitScoreEdge, 
                           new CArgAllowValuesBetween(kBestHit_ScoreEdgeMin, 
                                                      kBestHit_ScoreEdgeMax));
    arg_desc.SetDependency(kArgBestHitScoreEdge,
                           CArgDescriptions::eExcludes,
                           kArgCullingLimit);
    arg_desc.SetCurrentGroup("");
}

void
CHspFilteringArgs::ExtractAlgorithmOptions(const CArgs& args, 
                                      CBlastOptions& opts)
{
    if (args[kArgCullingLimit]) {
        opts.SetCullingLimit(args[kArgCullingLimit].AsInteger());
    }
    if (args[kArgBestHitOverhang]) {
        opts.SetBestHitOverhang(args[kArgBestHitOverhang].AsDouble());
    }
    if (args[kArgBestHitScoreEdge]) {
        opts.SetBestHitScoreEdge(args[kArgBestHitScoreEdge].AsDouble());
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

bool
CMbIndexArgs::HasBeenSet(const CArgs& args)
{
    if ( (args.Exist(kArgUseIndex) && args[kArgUseIndex].HasValue()) ||
         (args.Exist(kArgIndexName) && args[kArgIndexName].HasValue()) ) {
        return true;
    }
    return false;
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
                NCBI_THROW(CInputException, eInvalidInput,
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
    m_IsUngapped = false;
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
        m_QueryOptsArgs->ExtractAlgorithmOptions(args, opts);
        m_StdCmdLineArgs->ExtractAlgorithmOptions(args, opts);
        m_RemoteArgs->ExtractAlgorithmOptions(args, opts);
        m_DebugArgs->ExtractAlgorithmOptions(args, opts);
        m_FormattingArgs->ExtractAlgorithmOptions(args, opts);
        if (CBlastDatabaseArgs::HasBeenSet(args)) {
            m_BlastDbArgs->ExtractAlgorithmOptions(args, opts);
        }
        if (CMbIndexArgs::HasBeenSet(args)) {
            NON_CONST_ITERATE(TBlastCmdLineArgs, arg, m_Args) {
                if (dynamic_cast<CMbIndexArgs*>(arg->GetPointer()) != NULL) {
                    (*arg)->ExtractAlgorithmOptions(args, opts);
                }
            }
        }
        m_HspFilteringArgs->ExtractAlgorithmOptions(args, opts);
        m_IsUngapped = !opts.GetGappedMode();
        try { m_OptsHandle->Validate(); }
        catch (const CBlastException& e) {
            NCBI_THROW(CInputException, eInvalidInput, e.GetMsg());
        }
        return m_OptsHandle;
    }

    CBlastOptions::EAPILocality locality = 
        (args.Exist(kArgRemote) && args[kArgRemote]) 
        ? CBlastOptions::eRemote 
        : CBlastOptions::eLocal;

    // This is needed as a CRemoteBlast object and its options are instantiated
    // to create the search strategy
    if (GetExportSearchStrategyStream(args) || 
           m_FormattingArgs->ArchiveFormatRequested(args)) {
        locality = CBlastOptions::eBoth;
    }

    CRef<CBlastOptionsHandle> retval(x_CreateOptionsHandle(locality, args));
    CBlastOptions& opts = retval->SetOptions();
    NON_CONST_ITERATE(TBlastCmdLineArgs, arg, m_Args) {
        (*arg)->ExtractAlgorithmOptions(args, opts);
    }

    m_IsUngapped = !opts.GetGappedMode();
    try { retval->Validate(); }
    catch (const CBlastException& e) {
        NCBI_THROW(CInputException, eInvalidInput, e.GetMsg());
    }
    return retval;
}

void CBlastAppArgs::SetTask(const string& task)
{
#if _BLAST_DEBUG
    ThrowIfInvalidTask(task);
#endif
    m_Task.assign(task);
}

CArgDescriptions* 
SetUpCommandLineArguments(TBlastCmdLineArgs& args)
{
    auto_ptr<CArgDescriptions> retval(new CArgDescriptions);

    // Create the groups so that the ordering is established
    retval->SetCurrentGroup("Input query options");
    retval->SetCurrentGroup("General search options");
    retval->SetCurrentGroup("BLAST database options");
    retval->SetCurrentGroup("BLAST-2-Sequences options");
    retval->SetCurrentGroup("Formatting options");
    retval->SetCurrentGroup("Query filtering options");
    retval->SetCurrentGroup("Restrict search or results");
    retval->SetCurrentGroup("Discontiguous MegaBLAST options");
    retval->SetCurrentGroup("Statistical options");
    retval->SetCurrentGroup("Search strategy options");
    retval->SetCurrentGroup("Extension options");
    retval->SetCurrentGroup("");


    NON_CONST_ITERATE(TBlastCmdLineArgs, arg, args) {
        (*arg)->SetArgumentDescriptions(*retval);
    }
    return retval.release();
}

END_SCOPE(blast)
END_NCBI_SCOPE
