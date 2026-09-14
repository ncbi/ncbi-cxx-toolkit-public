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
 * Authors: Amelia Fong
 *
 */

/** @file blast_usage_report.hpp
 *  BLAST usage report api
 */

#ifndef ALGO_BLAST_API___BLAST_USAGE_REPORT__HPP
#define ALGO_BLAST_API___BLAST_USAGE_REPORT__HPP

#include <connect/ncbi_usage_report.hpp>
#include <algo/blast/core/blast_export.h>
#include <corelib/phone_home_policy.hpp>
#include <vector>

BEGIN_NCBI_SCOPE

class CMemoryRegistry;

BEGIN_SCOPE(blast)

class NCBI_XBLAST_EXPORT CBlastPhoneHomePolicy : public IPhoneHomePolicy
{
public:
    /// Constructor
    /// Check phone home configurations
    CBlastPhoneHomePolicy();
    explicit CBlastPhoneHomePolicy(const string& config_file_path);

    /// Destructor
    ~CBlastPhoneHomePolicy() override = default;

    /// Apply policy for an application.
    void Apply(CNcbiApplicationAPI* ) override {}

    /// Print a message about collecting data, disabling telemetry and
    /// privacy policies.
    void Print() override;

    /// Save policy configuration to the user's NCBI config file
    void Save();

    /// Resolve the effective policy from environment, registry, and user
    /// configuration.
    void Restore() override;

    /// Automatically called from the destructor.
    void Finish() {}

    /// @return true if the user's NCBI config file contains the BLAST
    /// usage reporting setting
    bool HasUserUsageReportPreference() const { return m_UserNcbiConfigFileFound; }

    /// Check if any usage config (exclude opt-in) exists
    bool IsUsageConfigured() const {
        return (m_DoNotTrackEnv.configured ||
            m_NCBIUsageReportEnv.configured ||
            m_BlastUsageReportEnv.configured ||
            m_NCBIUsageReportRegistry.configured ||
            m_BlastUsageReportRegistry.configured);
    }

    /// Set the user's BLAST usage-report preference in the NCBI config file.
    /// @input TRUE to enable, FALSE to disable
    void SetUserUsageReportPreference(bool enable);

    string PhoneHomeStatusReport();

    static const string kNcbiRegistrySection;
    static const string kNcbiInheritsParam;
    static const string kNcbiEnv;
    static const string kDoNotTrackEnv;
    static const string kUsageReportEnv;
    static const string kBlastUsageReportKey;
    static const string kBlastUsageReportEnv;
    static const string kNCBIUsageReportRegistry;
    static const string kNCBIUsageReportRegistryParam;
    static const string kBlastUsageReportRegistry;
    static const string kBlastUsageReportRegistryParam;
    static const string kPrivacyNotice;

    static string GetLocalNcbiConfigFileName();
    static string GetLocalNcbiConfigFilePath();
    static string MakeOptionalNcbiInheritPath(const string& dir,
                                              const string& file_name);
    static vector<string> GetDefaultNcbiInherits();
    static void ReadRegistryFile(const string& path,
                                 CMemoryRegistry& registry);

private:

    /// One possible usage-report configuration source.
    /// configured means a value was present and valid; enabled is that value;
    /// selected marks the source that wins after priority resolution.
    struct SUsageConfig {
        void Set(bool b) {
            configured = true;
            enabled = b;
        }
        void Reset() {
            configured = false;
            enabled = false;
            selected = false;
        }
        bool configured = false; ///< Source supplied a valid boolean value.
        bool enabled = false;    ///< The source's boolean value.
        bool selected = false;   ///< This source determines the final state.
    };

    SUsageConfig m_DoNotTrackEnv;
    SUsageConfig m_NCBIUsageReportEnv;
    SUsageConfig m_BlastUsageReportEnv;
    SUsageConfig m_NCBIUsageReportRegistry;
    SUsageConfig m_BlastUsageReportRegistry;
    SUsageConfig m_UserNcbiConfigFile;

    string m_UserNcbiConfigFilePath;
    bool m_UserNcbiConfigFileFound;

    /// Read the local BLAST usage reporting setting saved by Save().
    /// Does not change the file.
    /// @return TRUE if the local opt-in configuration is found
    bool CheckOptInFileConfiguration();

    /// Check for usage params in the environment and Toolkit registry.
    /// ResolveUsageReportPolicy() combines this with CheckOptInFileConfiguration(); explicit
    /// environment and registry settings take precedence over the local opt-in.
    /// @return TRUE if usage setting is found
    bool CheckBlastUsageConfigurations();

    /// Set usage status based on usage config and ranking priority
    /// Call after config check or config change functions
    /// @return TRUE if usage report enabled
    bool UpdatePhoneHomeStatus();

    /// Helper function to format usage configuration
    /// @param string stream  output for formatted string
    /// @usage_type config type
    /// @config config param to format
    /// @flip reverse logic true for parameter DO_NOT_TRACK
    void x_FormatUsage(CNcbiOstrstream & ss, const  string & usage_type,
                       const SUsageConfig & config, bool flip);

    /// Reset config parms
    void x_ResetUsageConfigs() {
        m_DoNotTrackEnv.Reset();
        m_NCBIUsageReportEnv.Reset();
        m_BlastUsageReportEnv.Reset();
        m_NCBIUsageReportRegistry.Reset();
        m_BlastUsageReportRegistry.Reset();
    }

    /// Convert string to bool with valiadtaion
    /// @param input string value to convert to boolean
    /// @param output converted value, no change if fucntion return false
    /// @param  usage_str Used in warning message to identify error setting
    /// @return true if input string is valid and input successfully converted
    /// @return false for invalid input, CStringException handled and warning message posted
    bool x_ValidateStringToBool(const string & input, bool & output, const string & usage_type);
};

NCBI_XBLAST_EXPORT
CNcbiOstream& operator<<(CNcbiOstream& out,
                         const CBlastPhoneHomePolicy& policy);



class NCBI_XBLAST_EXPORT CBlastUsageReport : public CUsageReport
{
public:
    enum EUsageParams {
        eApp,
        eVersion,
        eProgram,
        eTask,
        eExitStatus,
        eRunTime,
        eDBName,
        eDBLength,
        eDBNumSeqs,
        eDBDate,
        eBl2seq,
        eNumSubjects,
        eSubjectsLength,
        eNumQueries,
        eTotalQueryLength,
        eEvalueThreshold,
        eNumThreads,
        eHitListSize,
        eOutputFmt,
        eTaxIdList,
        eNegTaxIdList,
        eGIList,
        eNegGIList,
        eSeqIdList,
        eNegSeqIdList,
        eIPGList,
        eNegIPGList,
        eMaskAlgo,
        eCompBasedStats,
        eRange,
        eMTMode,
        eNumQueryBatches,
        eNumErrStatus,
        ePSSMInput,
        eConverged,
        eArchiveInput,
        eRIDInput,
        eDBInfo,
        eDBTaxInfo,
        eDBEntry,
        eDBDumpAll,
        eDBType,
        eInputType,
        eParseSeqIDs,
        eSeqType,
        eDBTest,
        eDBAliasMode,
        eDocker,
        eGCP,
        eAWS,
        eELBJobId,
        eELBBatchNum,
        eSRA,
        eELBVersion
    };

    static const int kNumRetries=0;
    static const int kTimeout=10;

    CBlastUsageReport();
    ~CBlastUsageReport();
    void AddParam(EUsageParams p, int val);
    void AddParam(EUsageParams p, const string & val);
    void AddParam(EUsageParams p, const double & val);
    void AddParam(EUsageParams p, Int8 val);
    void AddParam(EUsageParams p, bool val);

    void SetUsageReport(bool enable);

private:
    void x_CheckRunEnv();
    string x_EUsageParamsToString(EUsageParams p);
    CUsageReportParameters m_Params;
};




END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_API___BLAST_USAGE_REPORT__HPP */
