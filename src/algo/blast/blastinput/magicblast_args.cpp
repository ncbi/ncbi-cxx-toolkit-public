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
 * Author: Greg Boratyn
 *
 */

/** @file blastmapper_args.cpp
 * Implementation of the BLASTMAPPER command line arguments
 */

#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/magicblast_args.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
//#include <algo/blast/api/version.hpp>
#include <algo/blast/api/magicblast_options.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

/// Special generic search arguments for blastmapper
class CMapperGenericSearchArgs : public CGenericSearchArgs
{
public:
    CMapperGenericSearchArgs(void) : CGenericSearchArgs(false) {}

    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc) {

        arg_desc.SetCurrentGroup("General search options");

        arg_desc.AddOptionalKey(kArgWordSize, "int_value", "Word size for "
                                "wordfinder algorithm (length of best perfect "
                                "match)", CArgDescriptions::eInteger);

        arg_desc.SetConstraint(kArgWordSize,
                               new CArgAllowValuesGreaterThanOrEqual(12));


        // gap open penalty
        arg_desc.AddOptionalKey(kArgGapOpen, "open_penalty",
                                "Cost to open a gap",
                                CArgDescriptions::eInteger);

        // gap extend penalty
        arg_desc.AddOptionalKey(kArgGapExtend, "extend_penalty",
                               "Cost to extend a gap", 
                               CArgDescriptions::eInteger);

        // FIXME: not sure if this one is needed
        arg_desc.SetCurrentGroup("Restrict search or results");
        arg_desc.AddOptionalKey(kArgPercentIdentity, "float_value",
                                "Percent identity",
                                CArgDescriptions::eDouble);
        arg_desc.SetConstraint(kArgPercentIdentity,
                               new CArgAllow_Doubles(0.0, 100.0));
    }
};

/// Nucleotide args with no reward score
class CMapperNuclArgs : public CNuclArgs
{
public:
    CMapperNuclArgs(void) : CNuclArgs() {}

    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc) {
        arg_desc.SetCurrentGroup("General search options");
        // blastn mismatch penalty
        arg_desc.AddOptionalKey(kArgMismatch, "penalty", 
                                "Penalty for a nucleotide mismatch", 
                                CArgDescriptions::eInteger);
        arg_desc.SetConstraint(kArgMismatch, 
                               new CArgAllowValuesLessThanOrEqual(0));
        arg_desc.SetCurrentGroup("");
    }
};

/// Formatting args advertising only SAM and fast tabular formats
class CMapperFormattingArgs : public CFormattingArgs
{
public:

    CMapperFormattingArgs(void) : CFormattingArgs() {}

    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc) {
        arg_desc.SetCurrentGroup("Formatting options");
        string kOutputFormatDescription = string(
        "alignment view options:\n"
        "sam = SAM format,\n"
        "tabular = Tabular format,\n"
        "text ASN.1\n");

        arg_desc.AddDefaultKey(align_format::kArgOutputFormat, "format", 
                               kOutputFormatDescription,
                               CArgDescriptions::eString, 
                               "sam");

        set<string> allowed_formats = {"sam", "tabular", "asn"};
        arg_desc.SetConstraint(align_format::kArgOutputFormat,
                               new CArgAllowStringSet(allowed_formats));

        arg_desc.SetCurrentGroup("");
    }

    virtual void ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& opt) {
        if (args.Exist(align_format::kArgOutputFormat)) {
            string fmt_choice = args[align_format::kArgOutputFormat].AsString();
            if (fmt_choice == "sam") {
                m_OutputFormat = eSAM;
            }
            else if (fmt_choice == "tabular") {
                m_OutputFormat = eTabular;
            }
            else if (fmt_choice == "asn") {
                m_OutputFormat = eAsnText;
            }
            else {
                CNcbiOstrstream os;
                os << "'" << fmt_choice << "' is not a valid output format";
                string msg = CNcbiOstrstreamToString(os);
                NCBI_THROW(CInputException, eInvalidInput, msg);
            }
        }

        m_ShowGis = true;
        m_Html = false;

        // only the fast tabular format is able to show merged HSPs with
        // common query bases
        if (m_OutputFormat != eTabular) {
            // FIXME: This is a hack. Merging should be done by the formatter,
            // but is currently done by HSP stream writer. This is an easy
            // switch until merging is implemented properly.
            setenv("MAPPER_NO_OVERLAPPED_HSP_MERGE", "1", 0);
        }
    }

    virtual bool ArchiveFormatRequested(const CArgs& args) const {
        return false;
    }
};

/// Longest intron size with non-zero defalut value
class CMapperLargestIntronSizeArgs : public CLargestIntronSizeArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc) {
        arg_desc.SetCurrentGroup("General search options");
        // largest intron length
        arg_desc.AddDefaultKey(kArgMaxIntronLength, "length", 
                    "Length of the largest intron allowed in a translated "
                    "nucleotide sequence when linking multiple distinct "
                    "alignments",
                    CArgDescriptions::eInteger,
                    NStr::IntToString(2000));
        arg_desc.SetConstraint(kArgMaxIntronLength,
                               new CArgAllowValuesGreaterThanOrEqual(0));
        arg_desc.SetCurrentGroup("");
    }
};

/// RemoteArgs with no option for remote
class CMapperRemoteArgs : public CRemoteArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc) {}    
};


/// Program description without BLAST+ version
class CMapperProgramDescriptionArgs : public CProgramDescriptionArgs
{
public:
    CMapperProgramDescriptionArgs(const string& program_name,
                                  const string& program_desc)
        : CProgramDescriptionArgs(program_name, program_desc) {}

    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc) {
        arg_desc.SetUsageContext(m_ProgName, m_ProgDesc);
    }
};

CMagicBlastAppArgs::CMagicBlastAppArgs()
{
    // remove search strategy args added in parent class constructor
    m_Args.clear();

    CRef<IBlastCmdLineArgs> arg;
    static const char kProgram[] = "magicblast";
    arg.Reset(new CMapperProgramDescriptionArgs(kProgram, "Short read mapper"));
    m_Args.push_back(arg);

    m_BlastDbArgs.Reset(new CBlastDatabaseArgs(false, false, false, true));
    m_BlastDbArgs->SetDatabaseMaskingSupport(true);
    arg.Reset(m_BlastDbArgs);
    m_Args.push_back(arg);

    m_StdCmdLineArgs.Reset(new CStdCmdLineArgs);
    m_StdCmdLineArgs->SetGzipEnabled(true);
    arg.Reset(m_StdCmdLineArgs);
    m_Args.push_back(arg);

    arg.Reset(new CMapperGenericSearchArgs);
    m_Args.push_back(arg);

    arg.Reset(new CMapperNuclArgs);
    m_Args.push_back(arg);

    m_QueryOptsArgs.Reset(new CMapperQueryOptionsArgs);
    arg.Reset(m_QueryOptsArgs);
    m_Args.push_back(arg);

    arg.Reset(new CMapperFormattingArgs);
    m_FormattingArgs.Reset(dynamic_cast<CFormattingArgs*>(
                                           arg.GetNonNullPointer()));
    m_Args.push_back(arg);

    m_MTArgs.Reset(new CMTArgs);
    arg.Reset(m_MTArgs);
    m_Args.push_back(arg);

    m_RemoteArgs.Reset(new CMapperRemoteArgs);
    arg.Reset(m_RemoteArgs);
    m_Args.push_back(arg);

    m_DebugArgs.Reset(new CDebugArgs);
    arg.Reset(m_DebugArgs);
    m_Args.push_back(arg);

    arg.Reset(new CMapperLargestIntronSizeArgs);
    m_Args.push_back(arg);

    arg.Reset(new CMappingArgs);
    m_Args.push_back(arg);
}

CRef<CBlastOptionsHandle> 
CMagicBlastAppArgs::x_CreateOptionsHandle(
                                         CBlastOptions::EAPILocality locality,
                                         const CArgs& args)
{
    return CRef<CBlastOptionsHandle>(new CMagicBlastOptionsHandle(locality));
}

int
CMagicBlastAppArgs::GetQueryBatchSize() const
{
    bool is_remote = (m_RemoteArgs.NotEmpty() && m_RemoteArgs->ExecuteRemotely());
    return blast::GetQueryBatchSize(ProgramNameToEnum(GetTask()), m_IsUngapped, is_remote, false);
}

END_SCOPE(blast)
END_NCBI_SCOPE

