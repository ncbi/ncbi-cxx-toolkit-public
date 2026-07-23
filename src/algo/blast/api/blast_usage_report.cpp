/*  $Id:
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
#include <corelib/ncbifile.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
#endif

static const string kNcbiAppName="standalone-blast";
static const string kIdFile="/sys/class/dmi/id/sys_vendor";
const string CBlastPhoneHomePolicy::kConfigFileName = ".blast-usage-report.ini";
const string CBlastPhoneHomePolicy::kOptInStr = "Enabled";

const string CBlastPhoneHomePolicy::kDoNotTrackEnv = "DO_NOT_TRACK";
const string CBlastPhoneHomePolicy::kUsageReportEnv = "NCBI_USAGE_REPORT_ENABLED";
const string CBlastPhoneHomePolicy::kBlastUsageReportEnv = "BLAST_USAGE_REPORT";
const string CBlastPhoneHomePolicy::kNCBIUsageReportRegistry = "USAGE_REPORT";
const string CBlastPhoneHomePolicy::kNCBIUsageReportRegistryParam = "Enabled";
const string CBlastPhoneHomePolicy::kBlastUsageReportRegistry = "BLAST";
const string CBlastPhoneHomePolicy::kBlastUsageReportRegistryParam = "BLAST_USAGE_REPORT";


void CBlastUsageReport::x_CheckRunEnv()
{
	char * blast_docker = getenv("BLAST_DOCKER");
	if(blast_docker != NULL){
		AddParam(eDocker, true);
	}

	CFile id_file(kIdFile);
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
	if (!phone_home_policy.OptInFileExists()) {
		if (phone_home_policy.IsUsageConfigured()) {
		    phone_home_policy.EnableOptIn(phone_home_policy.IsEnabled());
		}
		else {
		    phone_home_policy.Print();
		    phone_home_policy.SetEnabled(false);
		}
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
m_ConfigFilePath(kEmptyStr), m_OptInFileFound(false)
{
    const string home = CDir::GetHome();
    m_ConfigFilePath = CDirEntry::MakePath(home, kConfigFileName);
}

bool CBlastPhoneHomePolicy::CheckOptInFileConfiguration() {
    try {
        m_OptInFile.Reset();
	    CFile optInFile (m_ConfigFilePath);
	    if (optInFile.Exists()){
	        m_OptInFileFound = true;
	        CNcbiIfstream is(m_ConfigFilePath, IOS_BASE::in);
	        string line;
	        while (getline(is, line)) {
	            // Skip comment or blank line
	            string::size_type pos = line.find_first_not_of(" \t");
	            if ((pos == string::npos) || (line[pos] == '#')) {
	                continue;
	            }
	            if (line.find(kOptInStr) != string::npos) {
	                string p1, p2;
	                if (NStr::SplitInTwo(line, "=", p1, p2)) {
	                    NStr::TruncateSpacesInPlace(p1);
	                    NStr::TruncateSpacesInPlace(p2);
	                    bool value = false;
	                    if (x_ValidateStringToBool(p2, value, kOptInStr)){
	                    m_OptInFile.Set(value);
	                    }
	                    break;
	                }
	            }
	        }
	    }
    } catch (CException & e) {
        m_OptInFile.Reset();
        LOG_POST(Warning << "Opt-in file read error: " << e.GetMsg());
    } catch (...){
        m_OptInFile.Reset();
        LOG_POST(Warning << "Opt-in file read error: Unknown exception ");
    }
	return m_OptInFile.configured;
}

bool CBlastPhoneHomePolicy::UpdatePhoneHomeStatus()
{
    if(m_DoNotTrackEnv.configured) {
        m_DoNotTrackEnv.override = true;
        SetEnabled(m_DoNotTrackEnv.enabled);
    }
    else if(m_NCBIUsageReportEnv.configured) {
        m_NCBIUsageReportEnv.override = true;
        SetEnabled(m_NCBIUsageReportEnv.enabled);
    }
    else if(m_BlastUsageReportEnv.configured) {
        m_BlastUsageReportEnv.override = true;
        SetEnabled(m_BlastUsageReportEnv.enabled);
    }
    else if(m_NCBIUsageReportRegistry.configured) {
        m_NCBIUsageReportRegistry.override = true;
        SetEnabled(m_NCBIUsageReportRegistry.enabled);
    }
    else if(m_BlastUsageReportRegistry.configured) {
        m_BlastUsageReportRegistry.override = true;
        SetEnabled(m_BlastUsageReportRegistry.enabled);
    }
    else if(m_OptInFile.configured) {
        m_OptInFile.override = true;
        SetEnabled(m_OptInFile.enabled);
    }
    else {
        SetEnabled(false);
    }
    return IsEnabled();
}

const string CBlastPhoneHomePolicy::kPrivacyNotice = R"DELIM( 
BLAST+ Usage Reporting and Privacy Notice
-----------------------------------------

NCBI/NLM would like your permission to collect limited usage data from this application to help improve BLAST+ reliability, performance, and long-term support.

Participation is OPTIONAL.

If you choose to Opt In, BLAST+ will transmit limited technical and usage information to NCBI. No query or subject sequences, database contents, search results, or other biological research data are transmitted.

Data transmitted to NCBI may include:
- Application name and BLAST program (e.g. blastp, blastn)
- BLAST+ version
- Operating system type
- Runtime duration and exit status
- Number of query sequences
- Total query length
- Selected BLAST parameters and options
- Database metadata, including:
    - database name
    - database creation date
    - database size
    - number of sequences
- Thread count
- Output format
- Apparent IP address of the host system
- Internal NCBI logging fields required for usage data processing


How changes to data collection will be communicated:
Any changes to usage data collection practices or transmitted data elements will be documented in:
- BLAST+ release notes - https://www.ncbi.nlm.nih.gov/books/NBK131777/
- BLAST_PRIVACY.md documentation - https://github.com/ncbi/ncbi-cxx-toolkit-public/tree/main/include/algo/blast/BLAST_PRIVACY.md 
- the BLAST+ User Manual - https://www.ncbi.nlm.nih.gov/books/NBK569851/
Updated information will also reference the applicable NLM privacy policies.

You may enable or disable usage data sharing at any time by running the utility: blast_usage_report

NLM Web Policies: https://www.nlm.nih.gov/web_policies.html

)DELIM";

void CBlastPhoneHomePolicy::Print()
{
    std::cerr << kPrivacyNotice;
}


void CBlastPhoneHomePolicy::Save()
{
    try {
        const string config_str = kOptInStr + "=" + string(m_OptInFile.enabled? "true" : "false");
        if (OptInFileExists()) {
            CNcbiIfstream in_file(m_ConfigFilePath);
            CNcbiOfstream out_file(m_ConfigFilePath);
            bool config_found = false;
            if (!in_file.is_open() || !out_file.is_open()) {
                NCBI_THROW(CBlastException, eSystem, "Failed to open " + kConfigFileName);
            }
            string line;
            while (getline(in_file, line)) {
                if (line.empty() || line.starts_with('#')) {
                    out_file << line << '\n';
                    continue;
                }
	            if (line.find(kOptInStr) != string::npos) {
	                config_found = true;
	                out_file << config_str << endl;
                }
                else {
                    out_file << line;
                }
            }
            if (!config_found) {
                out_file << config_str << endl;
            }
        }
        else {
            CNcbiOfstream file(m_ConfigFilePath);
            if (file.is_open()) {
    	        file << config_str << endl;
            }
            else {
                NCBI_THROW(CBlastException, eSystem, "Failed to open " + kConfigFileName);
            }
            m_OptInFileFound = true;
        }
        LOG_POST(Info << "Blast Usage Report: " << config_str);
    } catch (CException & e) {
        m_OptInFile.Reset();
        LOG_POST(Warning << "Opt-in file write error: " << e.GetMsg());
    } catch (...){
        m_OptInFile.Reset();
        LOG_POST(Warning << "Opt-in file write error: Unknown exception ");
    }
}

bool CBlastPhoneHomePolicy::x_ValidateStringToBool(const string & input, bool & output, const string & usage_str)
{
    try {
        output = NStr::StringToBool(input);
    }
    catch (CStringException & e){
        ERR_POST(Warning << usage_str << " has invalid boolean value.");
        return false;
    }
    return true;
}

bool CBlastPhoneHomePolicy::CheckBlastUsageConfigurations()
{
    x_ResetUsageConfigs();
    try {
        CNcbiEnvironment env;
        string do_not_track_env = env.Get(kDoNotTrackEnv);
        bool value = false;
        if(!do_not_track_env.empty()){
            if (x_ValidateStringToBool(do_not_track_env, value, kDoNotTrackEnv)){
                m_DoNotTrackEnv.Set(!value);
            }
            //m_DoNotTrackEnv.Set(NStr::StringToBool(do_not_track_env));
        }
        string usage_report_env = env.Get(kUsageReportEnv);
        if(!usage_report_env.empty() ){
            bool value = false;
            if (x_ValidateStringToBool(usage_report_env, value, kUsageReportEnv)){
                m_NCBIUsageReportEnv.Set(value);
            }
        }
        string blast_usage_env = env.Get(kBlastUsageReportEnv);
        if(!blast_usage_env.empty()){
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

void CBlastPhoneHomePolicy::EnableOptIn(bool enable)
{
    m_OptInFile.Set(enable);
    Save();
    UpdatePhoneHomeStatus();
}

void CBlastPhoneHomePolicy::x_FormatUsage(CNcbiOstrstream & ss, const  string & usage_type, const SUsageConfig & config, bool flip)
{
    const int w = 30;
    const int t = 4;

    if (config.configured) {
        string s_str = kEmptyStr;
        if (flip) {
            s_str = config.enabled? "False" : "True";
        }
        else {
            s_str = config.enabled? "True" : "False";
        }
        if (config.override) {
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
    if (!IsUsageConfigured() && !OptInFileExists()) {
        ss << " by default.";
    }

    ss << "\n\nConfiguration source(s):" << endl;
    ss << "  BLAST Usage Report ini file:" << endl;
    if(m_OptInFile.configured) {
        x_FormatUsage(ss, "Opt-in", m_OptInFile, false);
    }
    else {
        ss << "    File not found. Use -on or -off to configure usage reporting." << endl;
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

    if (IsUsageConfigured() || OptInFileExists()) {
        ss << "\n* Marks the setting that determines the current On/Off state." << endl;
    }
    return ss.str();
}
