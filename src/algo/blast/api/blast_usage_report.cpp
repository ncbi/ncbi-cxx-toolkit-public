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
 * Authors:  Amelia Fong
 *
 */

/** @file blast_usage_report.cpp
 *  BLAST usage report api
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_usage_report.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbireg.hpp>
#include <algorithm>
#include <vector>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
#endif

static const string kNcbiAppName="standalone-blast";
static const string kIdFile="/sys/class/dmi/id/sys_vendor";
static const string kNcbiEnvRef("$NCBI");
static const string kNcbiIniConfigFileName("ncbi.ini");
static const string kSystemRootEnv("SYSTEMROOT");

const string CBlastPhoneHomePolicy::kNcbiRegistrySection = "NCBI";
const string CBlastPhoneHomePolicy::kNcbiInheritsParam = ".Inherits";
const string CBlastPhoneHomePolicy::kNcbiEnv = "NCBI";
const string CBlastPhoneHomePolicy::kDoNotTrackEnv = "DO_NOT_TRACK";
const string CBlastPhoneHomePolicy::kUsageReportEnv = "NCBI_USAGE_REPORT_ENABLED";
const string CBlastPhoneHomePolicy::kBlastUsageReportKey = "BLAST_USAGE_REPORT";
const string CBlastPhoneHomePolicy::kBlastUsageReportEnv =
    CBlastPhoneHomePolicy::kBlastUsageReportKey;
const string CBlastPhoneHomePolicy::kNCBIUsageReportRegistry = "USAGE_REPORT";
const string CBlastPhoneHomePolicy::kNCBIUsageReportRegistryParam = "Enabled";
const string CBlastPhoneHomePolicy::kBlastUsageReportRegistry = "BLAST";
const string CBlastPhoneHomePolicy::kBlastUsageReportRegistryParam =
    CBlastPhoneHomePolicy::kBlastUsageReportKey;


string CBlastPhoneHomePolicy::GetLocalNcbiConfigFileName()
{
#if defined(NCBI_OS_MSWIN)
    return kNcbiIniConfigFileName;
#else
    return CNcbiRegistry::sm_SysRegName;
#endif
}


string CBlastPhoneHomePolicy::GetLocalNcbiConfigFilePath()
{
    return CDirEntry::MakePath(CDir::GetHome(), GetLocalNcbiConfigFileName());
}


string CBlastPhoneHomePolicy::MakeOptionalNcbiInheritPath
    (const string& dir, const string& file_name)
{
    return dir.empty() ? kEmptyStr :
        "-" + CDirEntry::MakePath(dir, file_name);
}


static void s_AddInheritPath(vector<string>& paths,
                             const string& dir,
                             const string& file_name)
{
    const string path =
        CBlastPhoneHomePolicy::MakeOptionalNcbiInheritPath(dir, file_name);
    if (!path.empty() &&
        find(paths.begin(), paths.end(), path) == paths.end()) {
        paths.push_back(path);
    }
}


vector<string> CBlastPhoneHomePolicy::GetDefaultNcbiInherits()
{
    vector<string> paths;
    CNcbiEnvironment env;
    s_AddInheritPath(paths, kNcbiEnvRef, GetLocalNcbiConfigFileName());
#if defined(NCBI_OS_MSWIN)
    const string system_root = env.Get(kSystemRootEnv);
    if (!system_root.empty()) {
        s_AddInheritPath(paths, system_root, GetLocalNcbiConfigFileName());
    }
#else
    s_AddInheritPath(paths, "/etc", CNcbiRegistry::sm_SysRegName);
#endif
    return paths;
}


void CBlastPhoneHomePolicy::ReadRegistryFile(const string& path,
                                             CMemoryRegistry& registry)
{
    CNcbiIfstream in_file(path, IOS_BASE::in | IOS_BASE::binary);
    if (!in_file.is_open()) {
        NCBI_THROW(CBlastException, eSystem, "Failed to open " + path);
    }

    registry.Read(in_file, 0, CDirEntry(path).GetDir());
}


static string s_GetInheritPathWithoutPrefix(const string& path)
{
    string value(path);
    NStr::TruncateSpacesInPlace(value);
    if (!value.empty() && (value[0] == '-' || value[0] == '+')) {
        value.erase(0, 1);
        NStr::TruncateSpacesInPlace(value);
    }
    return value;
}


static bool s_InheritsPathExists(const string& inherits, const string& path)
{
    const string desired_path = s_GetInheritPathWithoutPrefix(path);
    string::size_type start = 0;
    while (start <= inherits.size()) {
        string::size_type comma = inherits.find(',', start);
        string entry = inherits.substr(start,
            comma == NPOS ? NPOS : comma - start);

        if (s_GetInheritPathWithoutPrefix(entry) == desired_path) {
            return true;
        }

        if (comma == NPOS) {
            break;
        }
        start = comma + 1;
    }

    return false;
}


static string s_MergeNcbiInherits(const string& current_value)
{
    string value(current_value);
    NStr::TruncateSpacesInPlace(value);

    const vector<string> default_paths =
        CBlastPhoneHomePolicy::GetDefaultNcbiInherits();
    ITERATE(vector<string>, iter, default_paths) {
        if (s_InheritsPathExists(value, *iter)) {
            continue;
        }
        if (!value.empty()) {
            value += ", ";
        }
        value += *iter;
    }

    return value;
}


static void s_SetRegistryValue(CMemoryRegistry& registry,
                               const string& section,
                               const string& name,
                               const string& value)
{
    if (!registry.Set(section, name, value, IRegistry::fPersistent)) {
        NCBI_THROW(CBlastException, eSystem,
                   "Failed to set [" + section + "] " + name);
    }
}


static void s_EnsureNcbiInherits(CMemoryRegistry& registry)
{
    string current_value;
    if (registry.HasEntry(CBlastPhoneHomePolicy::kNcbiRegistrySection,
                          CBlastPhoneHomePolicy::kNcbiInheritsParam,
                          IRegistry::fPersistent)) {
        current_value = registry.Get
            (CBlastPhoneHomePolicy::kNcbiRegistrySection,
             CBlastPhoneHomePolicy::kNcbiInheritsParam,
             IRegistry::fPersistent);
    }
    current_value = s_MergeNcbiInherits(current_value);
    if (current_value.empty()) {
        return;
    }

    s_SetRegistryValue(registry, CBlastPhoneHomePolicy::kNcbiRegistrySection,
                       CBlastPhoneHomePolicy::kNcbiInheritsParam,
                       current_value);
}


static void s_WriteRegistryFile(const string& path,
                                const IRegistry& registry)
{
    const string dir = CDirEntry(path).GetDir();
    if (!dir.empty()) {
        CDir config_dir(dir);
        if (!config_dir.Exists() && !config_dir.CreatePath()) {
            NCBI_THROW(CBlastException, eSystem,
                       "Failed to create directory " + dir);
        }
    }

    const string tmp_path = CDirEntry::GetTmpNameEx
        (dir, ".blast_usage_report_", CDirEntry::eTmpFileCreate);
    if (tmp_path.empty()) {
        NCBI_THROW(CBlastException, eSystem,
                   "Failed to create temporary config file name");
    }

    CNcbiOfstream out_file(tmp_path.c_str(),
                           IOS_BASE::out | IOS_BASE::trunc |
                           IOS_BASE::binary);
    if (!out_file.is_open()) {
        CDirEntry(tmp_path).RemoveEntry();
        NCBI_THROW(CBlastException, eSystem, "Failed to open " + tmp_path);
    }

    if (!registry.Write(out_file, IRegistry::fPersistent)) {
        CDirEntry(tmp_path).RemoveEntry();
        NCBI_THROW(CBlastException, eSystem, "Failed to write " + tmp_path);
    }
    out_file.close();
    if (!out_file.good()) {
        CDirEntry(tmp_path).RemoveEntry();
        NCBI_THROW(CBlastException, eSystem, "Failed to write " + tmp_path);
    }

    if (!CDirEntry(tmp_path).Rename(path, CDirEntry::fRF_Overwrite)) {
        CDirEntry(tmp_path).RemoveEntry();
        NCBI_THROW(CBlastException, eSystem,
                   "Failed to replace " + path);
    }
}


void CBlastUsageReport::x_CheckRunEnv()
{
	char * blast_docker = getenv("BLAST_DOCKER");
	if(blast_docker != NULL){
		AddParam(eDocker, true);
	}

	const CFile id_file(kIdFile);
	if(id_file.Exists()){
		CNcbiIfstream s(id_file.GetPath().c_str(), IOS_BASE::in);
		string line;
		NcbiGetlineEOL(s, line);
		NStr::ToUpper(line);
		if (line.find("GOOGLE") != NPOS) {
			AddParam(eGCP, true);
		}
		else if (line.find("AMAZON")!= NPOS){
			AddParam(eAWS, true);
		}
	}

	char* elb_job_id = getenv("BLAST_ELB_JOB_ID");
	if(elb_job_id != NULL){
		string j_id(elb_job_id);
		AddParam(eELBJobId, j_id);
	}
	char* elb_batch_num = getenv("BLAST_ELB_BATCH_NUM");
	if(elb_batch_num != NULL){
		int bn = NStr::StringToInt(CTempString(elb_batch_num), NStr::fConvErr_NoThrow);
		AddParam(eELBBatchNum, bn);
	}
	char* elb_version = getenv("BLAST_ELB_VERSION");
	if(elb_version != NULL){
        string ev(elb_version);
		AddParam(eELBVersion, ev);
	}
}

CBlastUsageReport::CBlastUsageReport()
{
	SetUsageReport(false);
	CBlastPhoneHomePolicy phone_home_policy;
	phone_home_policy.Restore();
	if (!phone_home_policy.HasUserUsageReportPreference() &&
        !phone_home_policy.IsUsageConfigured()) {
		phone_home_policy.Print();
		phone_home_policy.SetEnabled(false);
	}
	SetUsageReport(phone_home_policy.IsEnabled());

	AddParam(eApp, kNcbiAppName);
	if (IsEnabled()) {
        CUsageReportAPI::SetRetries(kNumRetries);
        CTimeout t_out(kTimeout, 0);
        CUsageReportAPI::SetTimeout(t_out);
	}
	x_CheckRunEnv();
}

CBlastUsageReport::~CBlastUsageReport()
{
	if (IsEnabled()) {
		Send(m_Params);
		Wait( CUsageReport::eSkipIfNoConnection);
		Finish();
	}
}

string CBlastUsageReport::x_EUsageParamsToString(EUsageParams p)
{
    string retval;
    switch (p) {
    	case eApp:				retval.assign("ncbi_app"); break;
    	case eVersion:			retval.assign("version"); break;
    	case eProgram:          retval.assign("program"); break;
    	case eTask:        		retval.assign("task"); break;
    	case eExitStatus:    	retval.assign("exit_status"); break;
    	case eRunTime:    		retval.assign("run_time"); break;
    	case eDBName:    		retval.assign("db_name"); break;
    	case eDBLength:			retval.assign("db_length"); break;
    	case eDBNumSeqs:		retval.assign("db_num_seqs"); break;
		case eDBDate:			retval.assign("db_date"); break;
    	case eBl2seq:    		retval.assign("bl2seq"); break;
    	case eNumSubjects:		retval.assign("num_subjects"); break;
		case eSubjectsLength:	retval.assign("subjects_length"); break;
    	case eNumQueries:		retval.assign("num_queries"); break;
    	case eTotalQueryLength:	retval.assign("queries_length"); break;
    	case eEvalueThreshold:	retval.assign("evalue_threshold"); break;
    	case eNumThreads:		retval.assign("num_threads"); break;
    	case eHitListSize:		retval.assign("hitlist_size"); break;
    	case eOutputFmt:		retval.assign("output_fmt"); break;
    	case eTaxIdList:		retval.assign("taxidlist"); break;
    	case eNegTaxIdList:		retval.assign("negative_taxidlist"); break;
    	case eGIList:			retval.assign("gilist"); break;
    	case eNegGIList:		retval.assign("negative_gilist"); break;
    	case eSeqIdList:		retval.assign("seqidlist"); break;
    	case eNegSeqIdList:		retval.assign("negative_seqidlist"); break;
    	case eIPGList:			retval.assign("ipglist"); break;
    	case eNegIPGList:		retval.assign("negative_ipglist"); break;
    	case eMaskAlgo:			retval.assign("mask_algo"); break;
    	case eCompBasedStats:	retval.assign("comp_based_stats"); break;
    	case eRange:			retval.assign("range"); break;
    	case eMTMode:			retval.assign("mt_mode"); break;
    	case eNumQueryBatches:	retval.assign("num_query_batches"); break;
    	case eNumErrStatus:		retval.assign("num_error_status"); break;
    	case ePSSMInput:		retval.assign("pssm_input"); break;
    	case eConverged:	    retval.assign("converged"); break;
    	case eArchiveInput:	    retval.assign("archive"); break;
    	case eRIDInput:	    	retval.assign("rid"); break;
    	case eDBInfo:			retval.assign("db_info"); break;
		case eDBTaxInfo:		retval.assign("db_tax_info"); break;
		case eDBEntry:			retval.assign("db_entry"); break;
		case eDBDumpAll:		retval.assign("db_entry_all"); break;
		case eDBType:			retval.assign("db_type"); break;
		case eInputType:		retval.assign("input_type"); break;
		case eParseSeqIDs:		retval.assign("parse_seqids"); break;
		case eSeqType:			retval.assign("seq_type"); break;
		case eDBTest:			retval.assign("db_test"); break;
		case eDBAliasMode:		retval.assign("db_alias_mode"); break;
		case eDocker:			retval.assign("docker"); break;
		case eGCP:				retval.assign("gcp"); break;
		case eAWS:				retval.assign("aws"); break;
		case eELBJobId:			retval.assign("elb_job_id"); break;
		case eELBBatchNum:		retval.assign("elb_batch_num"); break;
        case eSRA:              retval.assign("sra"); break;
        case eELBVersion:       retval.assign("elb_version"); break;
    	default:
        	LOG_POST(Warning <<"Invalid usage params: " << (int)p);
        	abort();
        	break;
    }
    return retval;
}

void CBlastUsageReport::AddParam(EUsageParams p, int val)
{
	if (IsEnabled()){
		string t = x_EUsageParamsToString(p);
		m_Params.Add(t, NStr::IntToString(val));
	}
}

void CBlastUsageReport::AddParam(EUsageParams p, const string & val)
{
	if (IsEnabled()) {
		string t = x_EUsageParamsToString(p);
		m_Params.Add(t, val);
	}
}

void CBlastUsageReport::AddParam(EUsageParams p, const double & val)
{
	if (IsEnabled()) {
		string t = x_EUsageParamsToString(p);
		m_Params.Add(t, val);
	}
}

void CBlastUsageReport::SetUsageReport(bool enable) {
    SetEnabled(enable);
    // Global setting, bypass disable for not having DO_NOT_TRACK in env
    CUsageReportAPI::SetEnabled(enable);
}

void CBlastUsageReport::AddParam(EUsageParams p, Int8 val)
{
	if (IsEnabled()) {
		string t = x_EUsageParamsToString(p);
		m_Params.Add(t, val);
	}

}

void CBlastUsageReport::AddParam(EUsageParams p, bool val)
{
	if (IsEnabled()) {
		string t = x_EUsageParamsToString(p);
		m_Params.Add(t, val);
	}

}

/*****************************************************************************/
CBlastPhoneHomePolicy::CBlastPhoneHomePolicy():
CBlastPhoneHomePolicy(GetLocalNcbiConfigFilePath())
{
}

CBlastPhoneHomePolicy::CBlastPhoneHomePolicy(const string& config_file_path):
m_UserNcbiConfigFilePath(config_file_path), m_UserNcbiConfigFileFound(false)
{
}

bool CBlastPhoneHomePolicy::CheckOptInFileConfiguration() {
    try {
        m_UserNcbiConfigFile.Reset();
        m_UserNcbiConfigFileFound = false;

        CFile config_file(m_UserNcbiConfigFilePath);
        if (config_file.Exists()) {
            CMemoryRegistry registry;
            ReadRegistryFile(m_UserNcbiConfigFilePath, registry);
            if (registry.HasEntry(kBlastUsageReportRegistry,
                                  kBlastUsageReportRegistryParam,
                                  IRegistry::fPersistent)) {
                const string value_str =
                    registry.Get(kBlastUsageReportRegistry,
                                 kBlastUsageReportRegistryParam,
                                 IRegistry::fPersistent);
                bool value = false;
                if (x_ValidateStringToBool(value_str, value,
                                           kBlastUsageReportKey)) {
                    m_UserNcbiConfigFile.Set(value);
                    m_UserNcbiConfigFileFound = true;
                }
            }
        }
    } catch (CException & e) {
        m_UserNcbiConfigFile.Reset();
        m_UserNcbiConfigFileFound = false;
        LOG_POST(Warning << "Local NCBI config read error: " << e.GetMsg());
    } catch (...){
        m_UserNcbiConfigFile.Reset();
        m_UserNcbiConfigFileFound = false;
        LOG_POST(Warning << "Local NCBI config read error: Unknown exception ");
    }
	return m_UserNcbiConfigFile.configured;
}

bool CBlastPhoneHomePolicy::UpdatePhoneHomeStatus()
{
    if(m_DoNotTrackEnv.configured) {
        m_DoNotTrackEnv.selected = true;
        SetEnabled(m_DoNotTrackEnv.enabled);
    }
    else if(m_NCBIUsageReportEnv.configured) {
        m_NCBIUsageReportEnv.selected = true;
        SetEnabled(m_NCBIUsageReportEnv.enabled);
    }
    else if(m_BlastUsageReportEnv.configured) {
        m_BlastUsageReportEnv.selected = true;
        SetEnabled(m_BlastUsageReportEnv.enabled);
    }
    else if(m_NCBIUsageReportRegistry.configured) {
        m_NCBIUsageReportRegistry.selected = true;
        SetEnabled(m_NCBIUsageReportRegistry.enabled);
    }
    else if(m_BlastUsageReportRegistry.configured) {
        m_BlastUsageReportRegistry.selected = true;
        SetEnabled(m_BlastUsageReportRegistry.enabled);
    }
    else if(m_UserNcbiConfigFile.configured) {
        m_UserNcbiConfigFile.selected = true;
        SetEnabled(m_UserNcbiConfigFile.enabled);
    }
    else {
        SetEnabled(false);
    }
    return IsEnabled();
}

const string CBlastPhoneHomePolicy::kPrivacyNotice = R"DELIM( 
BLAST+ Usage Reporting and Privacy Notice
-----------------------------------------

NCBI would like your permission to collect limited usage data for BLAST programs to help improve reliability, performance, and long-term support.
Help prioritize software features and improvements. Make sure that your favorite BLAST features are supported.

Participation is OPTIONAL. 

To opt in, run blast_usage_report -on or set the environment variable BLAST_USAGE_REPORT=1. You do not need to do anything to opt out. 

See https://www.ncbi.nlm.nih.gov/books/NBK569851/ for more information.

)DELIM";

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CNcbiOstream& operator<<(CNcbiOstream& out,
                         const CBlastPhoneHomePolicy& /* policy */)
{
    return out << CBlastPhoneHomePolicy::kPrivacyNotice;
}

END_SCOPE(blast)
END_NCBI_SCOPE


void CBlastPhoneHomePolicy::Print()
{
    std::cerr << *this;
}


void CBlastPhoneHomePolicy::Save()
{
    try {
        CMemoryRegistry registry;
        if (CFile(m_UserNcbiConfigFilePath).Exists()) {
            ReadRegistryFile(m_UserNcbiConfigFilePath, registry);
        }

        const string value = NStr::BoolToString(m_UserNcbiConfigFile.enabled);
        s_EnsureNcbiInherits(registry);
        s_SetRegistryValue(registry, kBlastUsageReportRegistry,
                           kBlastUsageReportRegistryParam, value);
        s_WriteRegistryFile(m_UserNcbiConfigFilePath, registry);

        m_UserNcbiConfigFileFound = true;
        LOG_POST(Info << "Blast Usage Report: [" <<
                 kBlastUsageReportRegistry << "] " <<
                 kBlastUsageReportRegistryParam << "=" << value);
    } catch (CException & e) {
        m_UserNcbiConfigFile.Reset();
        m_UserNcbiConfigFileFound = false;
        LOG_POST(Warning << "Local NCBI config write error: " << e.GetMsg());
    } catch (...){
        m_UserNcbiConfigFile.Reset();
        m_UserNcbiConfigFileFound = false;
        LOG_POST(Warning << "Local NCBI config write error: Unknown exception ");
    }
}

bool CBlastPhoneHomePolicy::x_ValidateStringToBool(const string & input, bool & output, const string & usage_str)
{
    try {
        output = NStr::StringToBool(input);
    }
    catch (CStringException & e){
        ERR_POST(Warning << usage_str << " has an invalid boolean value: '" << input << "'.");
        return false;
    }
    return true;
}

bool CBlastPhoneHomePolicy::CheckBlastUsageConfigurations()
{
    x_ResetUsageConfigs();
    try {
        CNcbiEnvironment env;
        bool found = false;
        const string do_not_track_env = env.Get(kDoNotTrackEnv, &found);
        if(found) {
            bool value = true;
            try {
                value = NStr::StringToBool(do_not_track_env);
            } catch (CStringException&) {
            }
            m_DoNotTrackEnv.Set(!value);
        }
        const string usage_report_env = env.Get(kUsageReportEnv, &found);
        if(found) {
            bool value = false;
            if (x_ValidateStringToBool(usage_report_env, value, kUsageReportEnv)){
                m_NCBIUsageReportEnv.Set(value);
            }
        }
        const string blast_usage_env = env.Get(kBlastUsageReportEnv, &found);
        if(found) {
            bool value = false;
            if (x_ValidateStringToBool(blast_usage_env, value, kBlastUsageReportEnv)){
                m_BlastUsageReportEnv.Set(value);
            }
        }

        CNcbiIstrstream empty_stream(kEmptyStr);
        CRef<CNcbiRegistry> registry(new CNcbiRegistry(empty_stream, IRegistry::fWithNcbirc));
        if (registry->HasEntry(kNCBIUsageReportRegistry, kNCBIUsageReportRegistryParam)) {
            bool value = false;
            if (x_ValidateStringToBool(registry->Get(kNCBIUsageReportRegistry, kNCBIUsageReportRegistryParam),
                                       value, kNCBIUsageReportRegistry)){
                m_NCBIUsageReportRegistry.Set(value);
            }
        }
        if (registry->HasEntry(kBlastUsageReportRegistry, kBlastUsageReportRegistryParam)) {
            bool value = false;
            if (x_ValidateStringToBool(registry->Get(kBlastUsageReportRegistry, kBlastUsageReportRegistryParam),
                                       value, kBlastUsageReportRegistryParam)) {
                m_BlastUsageReportRegistry.Set(value);
            }
        }

    } catch(CException & e) {
        // This catches CStringException as well
        x_ResetUsageConfigs();
        ERR_POST(Warning << "Usage configuration error: " << e.GetMsg());

    }catch (...) {
        x_ResetUsageConfigs();
        LOG_POST(Warning << "Usage configuration unknown error");

    }

    return IsUsageConfigured();
}

void CBlastPhoneHomePolicy::Restore()
{
    CheckBlastUsageConfigurations();
    CheckOptInFileConfiguration();
    UpdatePhoneHomeStatus();
};

void CBlastPhoneHomePolicy::SetUserUsageReportPreference(bool enable)
{
    m_UserNcbiConfigFile.Set(enable);
    Save();
    Restore();
}

void CBlastPhoneHomePolicy::x_FormatUsage(CNcbiOstrstream & ss, const  string & usage_type, const SUsageConfig & config, bool flip)
{
    const int w = 30;
    const int t = 4;

    if (config.configured) {
        string s_str = kEmptyStr;
        if (flip) {
            s_str = NStr::BoolToString(!config.enabled);
        } else {
            s_str = NStr::BoolToString(config.enabled);
        }
        if (config.selected) {
            s_str += "*";
        }
        ss << string(t, ' ') << std::left << std::setw(w) << usage_type << s_str << endl;
    }
}

string CBlastPhoneHomePolicy::PhoneHomeStatusReport()
{
    Restore();

    string s_str = IsEnabled()? "Enabled" : "Disabled";
    CNcbiOstrstream ss;
    ss << "BLAST Usage Report : " << s_str;
    if (!IsUsageConfigured() && !HasUserUsageReportPreference()) {
        ss << " by default.";
    }

    ss << "\n\nConfiguration source(s):" << endl;
    ss << "  Local NCBI config file (" << m_UserNcbiConfigFilePath << "):" << endl;
    if(m_UserNcbiConfigFile.configured) {
        x_FormatUsage(ss, kBlastUsageReportRegistryParam, m_UserNcbiConfigFile, false);
    }
    else {
        ss << "    " << kBlastUsageReportRegistryParam
           << " not found. Use -on or -off to configure usage reporting."
           << endl;
    }

    if(m_DoNotTrackEnv.configured ||
       m_NCBIUsageReportEnv.configured ||
       m_BlastUsageReportEnv.configured)
    {
        ss << "\n  Environment Variable: " << endl;
        x_FormatUsage(ss, kDoNotTrackEnv, m_DoNotTrackEnv, true);
        x_FormatUsage(ss, kUsageReportEnv, m_NCBIUsageReportEnv, false);
        x_FormatUsage(ss, kBlastUsageReportEnv, m_BlastUsageReportEnv, false);
    }

    if (m_NCBIUsageReportRegistry.configured ||
        m_BlastUsageReportRegistry.configured) {
        ss << "\n  NCBI Registry File: " << endl;
        const string registry_usage_report = "[" + kNCBIUsageReportRegistry + "] " + kNCBIUsageReportRegistryParam;
        const string registry_blast_usage_report = "[" + kBlastUsageReportRegistry + "] " + kBlastUsageReportRegistryParam;
        x_FormatUsage(ss, registry_usage_report, m_NCBIUsageReportRegistry, false);
        x_FormatUsage(ss, registry_blast_usage_report, m_BlastUsageReportRegistry, false);
    }

    if (IsUsageConfigured() || HasUserUsageReportPreference()) {
        ss << "\n* Marks the setting that determines the current On/Off state." << endl;
    }
    return ss.str();
}
