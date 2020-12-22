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
 * Author: Christiam Camacho
 *
 */

/** @file blastdbcmd.cpp
 * Command line tool to examine the contents of BLAST databases. This is the
 * successor to fastacmd from the C toolkit
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/version.hpp>
#include <objtools/blast/seqdb_reader/seqdbexpert.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbtax.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <objtools/blast/blastdb_format/seq_formatter.hpp>
#include <objtools/blast/blastdb_format/blastdb_formatter.hpp>
#include <objtools/blast/blastdb_format/blastdb_seqid.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include "../blast/blast_app_util.hpp"
#include <iomanip>


#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
#endif

static const string NA = "N/A";

/// The application class
class CBlastDBCmdApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CBlastDBCmdApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
        m_StopWatch.Start();
        if (m_UsageReport.IsEnabled()) {
        	m_UsageReport.AddParam(CBlastUsageReport::eVersion, GetVersion().Print());
        	m_UsageReport.AddParam(CBlastUsageReport::eProgram, (string) "blastdbcmd");
        }
    }
    ~CBlastDBCmdApp() {
    	m_UsageReport.AddParam(CBlastUsageReport::eRunTime, m_StopWatch.Elapsed());
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    /// Handle to BLAST database
    CRef<CSeqDBExpert> m_BlastDb;
    /// Is the database protein
    bool m_DbIsProtein;
    /// output is FASTA
    bool m_FASTA;
    /// output is ASN.1 defline
    bool m_Asn1Bioseq;
    /// should we find duplicate entries?
    bool m_GetDuplicates;
    /// should we output target sequence only?
    bool m_TargetOnly;

    CBlastDB_FormatterConfig m_Config;

    set<TTaxId> m_TaxIdList;

    CBlastUsageReport m_UsageReport;
    CStopWatch m_StopWatch;

    /// Initializes Blast DB
    void x_InitBlastDB();
    void x_InitBlastDB_TaxIdList();

    string x_InitSearchRequest();

    /// Prints the BLAST database information (e.g.: handles -info command line
    /// option)
    void x_PrintBlastDatabaseInformation();

    /// Processes all requests except printing the BLAST database information
    /// @return 0 on success; 1 if some sequences were not retrieved
    int x_ProcessSearchRequest();

    /// Process batch entry with range, strand and filter id
    /// @param args program input args
    /// @param seq_fmt sequence formatter object
    /// @return 0 on sucess; 1 if some queries were not processed
    int x_ProcessBatchEntry(CBlastDB_Formatter & seq_fmt);

    int x_ProcessBatchEntry_NoDup(CBlastDB_Formatter & fmt);

    /// Process entry with range, strand and filter id
    /// @param args program input args
    /// @param seq_fmt sequence formatter object
    /// @return 0 on sucess; 1 if some queries were not processed
    int x_ProcessEntry(CBlastDB_Formatter & fmt);

    int x_ProcessTaxIdList(CBlastDB_Formatter & fmt);

    int x_ProcessSearchType(CBlastDB_Formatter & fmt);

    bool x_GetOids(const string & acc, vector<int> & oids);

    int x_ModifyConfigForBatchEntry(const string & config);

    bool x_UseLongSeqIds();

    void x_PrintBlastDatabaseTaxInformation();

    int x_ProcessBatchPig(CBlastDB_Formatter & fmt);

    void x_AddCmdOptions();
};


string s_PreProcessAccessionsForDBv5(const string & id)
{
	string rv = id;
	if ((id.find('|') != NPOS) || (id.find('_') != NPOS)) {

		CRef<CSeq_id> seqid;
		try {
			seqid = new CSeq_id(id, CSeq_id::fParse_RawText | CSeq_id::fParse_AnyLocal | CSeq_id::fParse_PartialOK);
		}
		catch(...) {
		}

		if(seqid.NotEmpty()) {
			if(seqid->IsPir() || seqid->IsPrf()) {
				return seqid->AsFastaString();
			}
			else if (seqid->IsPdb()) {
				string tmp = seqid->GetSeqIdString();
				rv = tmp.substr(0,4);
				NStr::ToUpper(rv);
				rv += tmp.substr(4);
				return (rv);
			}
			return seqid->GetSeqIdString(true);
		}
	}

	return NStr::ToUpper(rv);

}


bool
CBlastDBCmdApp::x_GetOids(const string & id, vector<int> & oids)
{
	string acc = id;
	if(m_BlastDb->GetBlastDbVersion() == EBlastDbVersion::eBDB_Version5) {
		acc = s_PreProcessAccessionsForDBv5(id);
	}
	TGi num_id = NStr::StringToNumeric<TGi>(acc, NStr::fConvErr_NoThrow);
	if(!errno) {
		int gi_oid = -1;
		m_BlastDb->GiToOidwFilterCheck(num_id, gi_oid);
		if(gi_oid < 0) {
			m_BlastDb->AccessionToOids(acc, oids);
		}
		else {
			oids.push_back(gi_oid);
		}

	}
	else {
		m_BlastDb->AccessionToOids(acc, oids);
	}
	if(oids.empty()) {
		ERR_POST(Error <<  "Entry not found: " << acc);
		return false;
	}
	return true;
}

int
CBlastDBCmdApp::x_ProcessEntry(CBlastDB_Formatter & fmt)
{
	unsigned int err_found = 0;
    const CArgs& args = GetArgs();
    _ASSERT(m_BlastDb.NotEmpty());

   	if (args["ipg"].HasValue()) {
    	CSeqDB::TOID oid;
    	m_BlastDb->PigToOid(args["ipg"].AsInteger(),oid);
   		fmt.Write(oid, m_Config);
    } else if (args["entry"].HasValue()) {
    	static const string kDelim(",");
    	const string& entry = args["entry"].AsString();

    	vector<string> queries;
       	if (entry.find(kDelim[0]) != string::npos) {
           	NStr::Split(entry, kDelim, queries);
       	} else {
       		queries.resize(1);
       		queries[0]  = entry;
     	}
   		for(unsigned int i=0; i < queries.size(); i++) {
     		vector<CSeqDB::TOID> oids;
     		if(x_GetOids(queries[i], oids)) {
   				for(unsigned int j=0; j < oids.size(); j++) {
   					if(m_TargetOnly) {
   						fmt.Write(oids[j], m_Config, queries[i]);
   					}
   					else {
   						fmt.Write(oids[j], m_Config);
   					}
     			}
     		}
     		else {
     			err_found ++;
     		}
     	}
   		if(err_found == queries.size()) {
   			NCBI_THROW(CInputException, eInvalidInput,
   		               "Entry or entries not found in BLAST database");
   		}
    }
   	return (err_found) ? 1:0;
}

bool s_IsMaskAlgoIdValid(CSeqDB & blastdb, int id)
{
	if (id >= 0) {
	    vector<int> algo_id(1, id);
	    vector<int> invalid_algo_ids = blastdb.ValidateMaskAlgorithms(algo_id);
	    if ( !invalid_algo_ids.empty()) {
	    	ERR_POST(Error << "Invalid filtering algorithm ID: " << NStr::IntToString(id));
	    	return false;
	    }
	}
	return true;
}

int CBlastDBCmdApp::x_ModifyConfigForBatchEntry(const string & format)
{
	int status = 0;
	if (!m_DbIsProtein) {
		m_Config.m_Strand = eNa_strand_plus;
	}
   	m_Config.m_SeqRange = TSeqRange::GetEmpty();
   	m_Config.m_FiltAlgoId = -1;
   	if(!format.empty()) {
   		vector<string> tmp;
   		NStr::Split(format, " \t", tmp, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
   		for(unsigned int i=0; i < tmp.size(); i++) {
   			if(tmp[i].find('-')!= string::npos) {
   				try {
   					m_Config.m_SeqRange = ParseSequenceRangeOpenEnd(tmp[i]);
   				} catch (...) {
   				}
   			}
   			else if (!m_DbIsProtein && NStr::EqualNocase(tmp[i].c_str(), "minus")) {
   				m_Config.m_Strand = eNa_strand_minus;
   			}
   			else {
   				m_Config.m_FiltAlgoId = NStr::StringToNonNegativeInt(tmp[i]);
   				if(!s_IsMaskAlgoIdValid(*m_BlastDb, m_Config.m_FiltAlgoId)){
   					status = 1;
   				}
   			}
   		}
   	}
   	return status;
}

int
CBlastDBCmdApp::x_ProcessTaxIdList(CBlastDB_Formatter & fmt)
{
    vector<blastdb::TOid> oids;
    m_BlastDb->TaxIdsToOids(m_TaxIdList, oids);
    if(oids.size() == 0) {
		ERR_POST (Error << "No seq found in db for taxonomy list");
		return 1;
    }
    for(unsigned i=0; i < oids.size(); i++) {
    	fmt.Write(oids[i], m_Config);
    }
    return  0;
}


void
CBlastDBCmdApp::x_InitBlastDB_TaxIdList()
{
   	const CArgs& args = GetArgs();
    vector<string> ids;
    if(args[kArgTaxIdList].HasValue()) {
    	string input = args[kArgTaxIdList].AsString();
    	NStr::Split(input, ",", ids);
    }
    else {
    	CNcbiIstream& input = args[kArgTaxIdListFile].AsInputFile();
    	while (input) {
    		string line;
    	    NcbiGetlineEOL(input, line);
    	    if ( !line.empty() ) {
    	       	ids.push_back(line);
    	    }
    	}
    }
    for(unsigned int i=0; i < ids.size(); i++) {
    	m_TaxIdList.insert(NStr::StringToNumeric<TTaxId>(ids[i], NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces));
    }

    CSeqDB::ESeqType seqtype = ParseMoleculeTypeString(args[kArgDbType].AsString());
    m_DbIsProtein = static_cast<bool>(seqtype == CSeqDB::eProtein);
    m_TargetOnly = args["target_only"];
   	if(m_TargetOnly) {
    	CRef<CSeqDBGiList> taxid_list(new CSeqDBGiList());
    	taxid_list->AddTaxIds(m_TaxIdList);
   		m_BlastDb.Reset(new CSeqDBExpert(args[kArgDb].AsString(), seqtype, taxid_list.GetPointer()));
    }
   	else {
   		m_BlastDb.Reset(new CSeqDBExpert(args[kArgDb].AsString(), seqtype));
   	}
}


int
CBlastDBCmdApp::x_ProcessBatchEntry_NoDup(CBlastDB_Formatter & fmt)
{
	int err_found = 0;
   	const CArgs& args = GetArgs();
    CNcbiIstream& input = args["entry_batch"].AsInputFile();
    vector<string> ids, formats;
    vector<CSeqDB::TOID> oids;
    while (input) {
        string line;
        NcbiGetlineEOL(input, line);
        if ( !line.empty() ) {
        	string id, format;
        	NStr::SplitInTwo(line, " \t", id, format, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
        	if(id.empty()) {
        		continue;
        	}
        	ids.push_back(id);
        	formats.push_back(format);
        }
    }

    if(m_BlastDb->GetBlastDbVersion() == EBlastDbVersion::eBDB_Version5) {
    	for(unsigned int i=0; i < ids.size(); i++) {
    		ids[i] = s_PreProcessAccessionsForDBv5(ids[i]);
    	}
    }
    try {
    m_BlastDb->AccessionsToOids(ids, oids);
    }
    catch (CSeqDBException & e) {
    	if (e.GetMsg().find("DB contains no accession info") == NPOS){
    		NCBI_RETHROW_SAME(e, e.GetMsg());
    	}
    }
    for(unsigned i=0; i < ids.size(); i++) {
    	if(oids[i] == kSeqDBEntryNotFound) {
    		TGi num_id = NStr::StringToNumeric<TGi>(ids[i], NStr::fConvErr_NoThrow);
    		if(!errno) {
    			int gi_oid = -1;
    			m_BlastDb->GiToOidwFilterCheck(num_id, gi_oid);
    			if(gi_oid >= 0) {
    				oids[i] = gi_oid;
    			}
    		}
    		if(oids[i] == kSeqDBEntryNotFound) {
    			err_found ++;
    			ERR_POST (Error << "Skipped " << ids[i]);
    			continue;
    		}
    	}
    	if(x_ModifyConfigForBatchEntry(formats[i]))  {
    		err_found ++;
    		ERR_POST (Error << "Skipped " << ids[i]);
    		continue;
    	}
    	if(m_TargetOnly) {
    		fmt.Write(oids[i], m_Config, ids[i]);
    	}
    	else {
    	   	fmt.Write(oids[i], m_Config);
    	}
    }
    return (err_found) ? 1 : 0;
}

int
CBlastDBCmdApp::x_ProcessBatchEntry(CBlastDB_Formatter & fmt)
{
	int err_found = 0;
   	const CArgs& args = GetArgs();
    CNcbiIstream& input = args["entry_batch"].AsInputFile();

    while (input) {
        string line;
        NcbiGetlineEOL(input, line);
        if ( !line.empty() ) {
        	string id, format;
        	NStr::SplitInTwo(line, " \t", id, format, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
        	if(id.empty()) {
        		continue;
        	}
        	if(x_ModifyConfigForBatchEntry(format))  {
        		err_found ++;
        		ERR_POST (Error << "Skipped " << id);
        		continue;
        	}
   			vector<int> oids;
         	if(!x_GetOids(id, oids)) {
         		err_found ++;
        		ERR_POST (Error << "Skipped " << id);
        		continue;
         	}

         	if (m_GetDuplicates) {
         		for(unsigned int j=0; j < oids.size(); j++) {
         			fmt.Write(oids[j], m_Config);
         		}
           	}
         	else {
         		if(m_TargetOnly) {
         			fmt.Write(oids[0], m_Config, id);
         		}
         		else {
         			fmt.Write(oids[0], m_Config);
         		}
         	}
        }
    }
    return (err_found) ? 1 : 0;
}


int
CBlastDBCmdApp::x_ProcessBatchPig(CBlastDB_Formatter & fmt)
{
	int err_found = 0;
   	const CArgs& args = GetArgs();
    CNcbiIstream& input = args["ipg_batch"].AsInputFile();

    while (input) {
        string line;
        NcbiGetlineEOL(input, line);
        if ( !line.empty() ) {
        	string id, format;
        	NStr::SplitInTwo(line, " \t", id, format, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
        	if(id.empty()) {
        		continue;
        	}
        	if(x_ModifyConfigForBatchEntry(format))  {
        		err_found ++;
        		ERR_POST (Error << "Skipped IPG : " << id);
        		continue;
        	}
   			int oid;
   			int pig = NStr::StringToInt(id, NStr::fConvErr_NoThrow );
   			m_BlastDb->PigToOid(pig,oid);
   			if (oid == -1) {
         		err_found ++;
        		ERR_POST (Error << "Skipped IPG: " << id);
        		continue;
         	}

   			fmt.Write(oid, m_Config);
        }
    }
    return (err_found) ? 1 : 0;
}

void
CBlastDBCmdApp::x_InitBlastDB()
{
    const CArgs& args = GetArgs();

    CSeqDB::ESeqType seqtype = ParseMoleculeTypeString(args[kArgDbType].AsString());
    m_BlastDb.Reset(new CSeqDBExpert(args[kArgDb].AsString(), seqtype));
    m_DbIsProtein = static_cast<bool>(m_BlastDb->GetSequenceType() == CSeqDB::eProtein);
}

void
CBlastDBCmdApp::x_PrintBlastDatabaseInformation()
{
    _ASSERT(m_BlastDb.NotEmpty());
    static const NStr::TNumToStringFlags kFlags = NStr::fWithCommas;
    const string kLetters = m_DbIsProtein ? "residues" : "bases";
    const string kVersion = (m_BlastDb->GetBlastDbVersion() == EBlastDbVersion::eBDB_Version5) ? "5":"4";
    const CArgs& args = GetArgs();

    CNcbiOstream& out = args[kArgOutput].AsOutputFile();

    // Print basic database information
    out << "Database: " << m_BlastDb->GetTitle() << endl
        << "\t" << NStr::IntToString(m_BlastDb->GetNumSeqs(), kFlags)
        << " sequences; ";
        if(args["exact_length"])
        	out << NStr::UInt8ToString(m_BlastDb->GetExactTotalLength(), kFlags);
        else
        	out << NStr::UInt8ToString(m_BlastDb->GetTotalLength(), kFlags);
    out << " total " << kLetters << endl << endl
        << "Date: " << m_BlastDb->GetDate()
        << "\tLongest sequence: "
        << NStr::IntToString(m_BlastDb->GetMaxLength(), kFlags) << " "
        << kLetters << endl << endl;

    out << "BLASTDB Version: " << kVersion << endl;

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    // Print filtering algorithms supported
    out << m_BlastDb->GetAvailableMaskAlgorithmDescriptions();
#endif

    // Print volume names
    vector<string> volumes;
    m_BlastDb->FindVolumePaths(volumes,false);
    out << endl << "Volumes:" << endl;
    ITERATE(vector<string>, file_name, volumes) {
        out << "\t" << *file_name << endl;
    }
}

class CPrintTaxFields
{
private:
	enum {
		eTaxID,
		eSciName,
		eCommonName,
		eSuperKingdom,
		eBlastName,
		eMaxFields
	};
	CNcbiOstream & m_Out;
	vector<int> m_Fields;
	vector<string> m_Seperators;
	bool m_NeedTaxInfoLookup;
public:
	CPrintTaxFields(CNcbiOstream & out, const string & fmt): m_Out(out), m_NeedTaxInfoLookup(true) {
		vector<string> fields;
        string sp = kEmptyStr;
		if(fmt == "%f") {
			m_Seperators.push_back(sp);
			for(unsigned int i=eTaxID; i < eMaxFields; i++){
				m_Fields.push_back(i);
				m_Seperators.push_back("\t");
			}
			return;
		}

	    for (unsigned int i = 0; i < fmt.size(); i++) {
	    	if (fmt[i] == '%') {
		        if (fmt[i+1] == '%') {
		            sp += fmt[i];
		            continue;
		        }
		        i++;
		        switch (fmt[i]) {
		        case 'T' :
       				m_Fields.push_back(eTaxID);
       			break;
		        case 'S' :
		        	m_Fields.push_back(eSciName);
                break;
		        case 'L' :
		        	m_Fields.push_back(eCommonName);
		        break;
		        case 'K' :
		        	m_Fields.push_back(eSuperKingdom);
                break;
		        case 'B' :
		        	m_Fields.push_back(eBlastName);
		        break;
		        default:
	                sp += fmt[i-1];
	                sp += fmt[i];
	                continue;
	            break;
		        }
	            m_Seperators.push_back(sp);
	            sp = kEmptyStr;
	    	}
	    	else {
		        sp += fmt[i];
	    	}
	    }
		m_Seperators.push_back(sp);

		if(m_Fields.empty()) {
			NCBI_THROW(CInputException, eInvalidInput,
				       "Invalid format options for tax_info.");
		}
		if((m_Fields.size() == 1) && (m_Fields[0] == eTaxID)){
			m_NeedTaxInfoLookup = false;
		}
	}

	void PrintEntry(const SSeqDBTaxInfo & t){
		for(unsigned int i=0; i < m_Fields.size(); i++) {
			m_Out << m_Seperators[i];
			switch (m_Fields[i]){
				case eTaxID:
					m_Out << t.taxid;
				break;
				case eSciName:
					m_Out << t.scientific_name;
				break;
				case eCommonName:
					m_Out << t.common_name;
				break;
				case eSuperKingdom:
					m_Out << t.s_kingdom;
				break;
				case eBlastName:
					m_Out << t.blast_name;
				break;
				default:
					NCBI_THROW(CInputException, eInvalidInput,
					           "Invalid format options for tax_info.");
				break;
			}
		}
		m_Out << m_Seperators.back();
		m_Out << "\n";
	}
	bool NeedTaxNames(){return m_NeedTaxInfoLookup;}
};


void
CBlastDBCmdApp::x_PrintBlastDatabaseTaxInformation()
{
    _ASSERT(m_BlastDb.NotEmpty());
    const CArgs& args = GetArgs();

    CNcbiOstream& out = args[kArgOutput].AsOutputFile();
    const string& kFmt = args["outfmt"].AsString();
    CPrintTaxFields tf(out, kFmt);
    set<TTaxId> tax_ids;
    m_BlastDb->GetDBTaxIds(tax_ids);
    // Print basic database information
    out << "# of Tax IDs in Database: " << tax_ids.size() << endl;
	SSeqDBTaxInfo info;
    ITERATE(set<TTaxId>, itr, tax_ids) {
    	SSeqDBTaxInfo info;
    	if(tf.NeedTaxNames()){
    		CSeqDBTaxInfo::GetTaxNames(*itr, info);
    		if(info.taxid == ZERO_TAX_ID){
    			info.taxid = *itr;
   				info.scientific_name = NA;
   				info.common_name = NA;
   				info.blast_name = NA;
   				info.s_kingdom = NA;
   			}
    	}
    	else {
   			info.taxid = *itr;
    	}
   		tf.PrintEntry(info);
    }
}


string
CBlastDBCmdApp::x_InitSearchRequest()
{
   	const CArgs& args = GetArgs();
    m_GetDuplicates = args["get_dups"];
    m_TargetOnly = args["target_only"];

    string outfmt = kEmptyStr;
    if (args["outfmt"].HasValue()) {
    	outfmt = args["outfmt"].AsString();
        m_FASTA = false;
        m_Asn1Bioseq = false;

        if ((outfmt.find("%f") != string::npos &&
           	(outfmt.find("%b") != string::npos || outfmt.find("%d") != string::npos)) ||
            (outfmt.find("%b") != string::npos && outfmt.find("%d") != string::npos)) {
           	NCBI_THROW(CInputException, eInvalidInput,
                    	"The %f, %b, %d output format options cannot be specified together.");
        }

        if (outfmt.find("%b") != string::npos) {
           	outfmt = "%b";
           	m_Asn1Bioseq = true;
        }

        // If "%f" is found within outfmt, discard everything else
        if (outfmt.find("%f") != string::npos) {
           	outfmt = "%f";
           	m_FASTA = true;
        }

        if (outfmt.find("%d") != string::npos) {
           	outfmt = "%d";
        }

        if (outfmt.find("%m") != string::npos) {
           	int algo_id = 0;
           	size_t i = outfmt.find("%m") + 2;
           	bool found = false;
           	while (i < outfmt.size() && outfmt[i] >= '0' && outfmt[i] <= '9') {
           		algo_id = algo_id * 10 + (outfmt[i] - '0');
               	outfmt.erase(i, 1);
               	found = true;
            }
            if (!found) {
               	NCBI_THROW(CInputException, eInvalidInput,
                       	   "The option '-outfmt %m' is not followed by a masking algo ID.");
            }
            m_Config.m_FmtAlgoId = algo_id;
    		if(!s_IsMaskAlgoIdValid(*m_BlastDb, m_Config.m_FmtAlgoId)) {
    			NCBI_THROW(CInvalidDataException, eInvalidInput,
    				                   "Invalid filtering algorithm ID for outfmt %m.");
    		}
        }
    }

    if (args["strand"].HasValue() && !m_DbIsProtein) {
    	if (args["strand"].AsString() == "plus") {
                m_Config.m_Strand = eNa_strand_plus;
            } else if (args["strand"].AsString() == "minus") {
                m_Config.m_Strand = eNa_strand_minus;
            } else {
            	NCBI_THROW(CInputException, eInvalidInput,
            	           "Both strands is not supported");
            }
    }
    m_Config.m_UseCtrlA = args["ctrl_a"];
    if (args["mask_sequence_with"].HasValue()) {
    	m_Config.m_FiltAlgoId = -1;
        m_Config.m_FiltAlgoId = NStr::StringToInt(args["mask_sequence_with"].AsString(), NStr::fConvErr_NoThrow);
        if(errno) {
        	m_Config.m_FiltAlgoId = m_BlastDb->GetMaskAlgorithmId(args["mask_sequence_with"].AsString());
        }
   		if(!s_IsMaskAlgoIdValid(*m_BlastDb, m_Config.m_FiltAlgoId)){
   			NCBI_THROW(CInvalidDataException, eInvalidInput,
   		               "Invalid filtering algorithm ID for mask_sequence_with.");
   		}
    }
    if (args["range"].HasValue()) {
    	m_Config.m_SeqRange = ParseSequenceRangeOpenEnd(args["range"].AsString());
    }
     return outfmt;
}

int
CBlastDBCmdApp::x_ProcessSearchType(CBlastDB_Formatter & fmt)
{
   	const CArgs& args = GetArgs();
	if (args["entry"].HasValue() && args["entry"].AsString() == "all") {
		fmt.DumpAll(m_Config);
	}
	else if (args["entry_batch"].HasValue()) {
		if(m_GetDuplicates) {
			return x_ProcessBatchEntry(fmt);
		}
		else {
			return x_ProcessBatchEntry_NoDup(fmt);
		}
	}
	else if (args["entry"].HasValue() || args["ipg"].HasValue()) {
		return x_ProcessEntry(fmt);
	}
	else if (args["ipg_batch"].HasValue()) {
		return x_ProcessBatchPig(fmt);
	}
	else if(args[kArgTaxIdList].HasValue()||
			args[kArgTaxIdListFile].HasValue()) {
		 return x_ProcessTaxIdList(fmt);
	}
	else {
		NCBI_THROW(CInputException, eInvalidInput,
		       	   "Must specify query type: one of 'entry', 'entry_batch', or 'pig'");
	}
	return 0;
}

bool CBlastDBCmdApp::x_UseLongSeqIds()
{
	const CArgs& args = GetArgs();
	if (args["long_seqids"].AsBoolean()) {
		return true;
	}
	CNcbiApplication* app = CNcbiApplication::Instance();
	if (app) {
		 const CNcbiRegistry& registry = app->GetConfig();
		 if (registry.Get("BLAST", "LONG_SEQID") == "1") {
			 return true;
		 }
	}
	return false;
}

int
CBlastDBCmdApp::x_ProcessSearchRequest()
{
   	int err_found = 0;
    try {
    	const CArgs& args = GetArgs();
    	CNcbiOstream& out = args[kArgOutput].AsOutputFile();
    	string outfmt = x_InitSearchRequest();
    	/* Special case: full db dump when no range and mask data is specified */
    	if (m_FASTA) {
    		CBlastDB_FastaFormatter fasta_fmt(*m_BlastDb, out, args["line_length"].AsInteger(), x_UseLongSeqIds());
    		err_found = x_ProcessSearchType(fasta_fmt);
    	}
    	else if (m_Asn1Bioseq) {
    		CBlastDB_BioseqFormatter bioseq_fmt(*m_BlastDb, out);
    		err_found = x_ProcessSearchType(bioseq_fmt);
    	}
    	else {
    		CBlastDB_SeqFormatter seq_fmt(outfmt, *m_BlastDb, out);
    		err_found = x_ProcessSearchType(seq_fmt);
    	}
    }
    catch (const CException& e) {
    	ERR_POST(Error << e.GetMsg());
        err_found = 1;
    } catch (...) {
        ERR_POST(Error << "Failed to retrieve requested item");
        err_found = 1;
    }
	return err_found;
}


void CBlastDBCmdApp::Init()
{
    HideStdArgs(fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                  "BLAST database client, version " + CBlastVersion().Print());

    arg_desc->SetCurrentGroup("BLAST database options");
    arg_desc->AddDefaultKey(kArgDb, "dbname", "BLAST database name",
                            CArgDescriptions::eString, "nr");

    arg_desc->AddDefaultKey(kArgDbType, "molecule_type",
                            "Molecule type stored in BLAST database",
                            CArgDescriptions::eString, "guess");
    arg_desc->SetConstraint(kArgDbType, &(*new CArgAllow_Strings,
                                        "nucl", "prot", "guess"));

    arg_desc->SetCurrentGroup("Retrieval options");
    arg_desc->AddOptionalKey("entry", "sequence_identifier",
                     "Comma-delimited search string(s) of sequence identifiers"
                     ":\n\te.g.: 555, AC147927, 'gnl|dbname|tag', or 'all' "
                     "to select all\n\tsequences in the database",
                     CArgDescriptions::eString);

    arg_desc->AddOptionalKey("entry_batch", "input_file",
                 "Input file for batch processing (Format: one entry per line, seq id \n"
		 "followed by optional space-delimited specifier(s) [range|strand|mask_algo_id]",
                 CArgDescriptions::eInputFile);
    arg_desc->SetDependency("entry_batch", CArgDescriptions::eExcludes, "entry");
    arg_desc->SetDependency("entry_batch", CArgDescriptions::eExcludes, "range");
    arg_desc->SetDependency("entry_batch", CArgDescriptions::eExcludes, "strand");
    arg_desc->SetDependency("entry_batch", CArgDescriptions::eExcludes, "mask_sequence_with");

    arg_desc->AddOptionalKey("ipg", "IPG", "IPG to retrieve",
                             CArgDescriptions::eInteger);
    arg_desc->SetConstraint("ipg", new CArgAllowValuesGreaterThanOrEqual(0));
    arg_desc->SetDependency("ipg", CArgDescriptions::eExcludes, "entry");
    arg_desc->SetDependency("ipg", CArgDescriptions::eExcludes, "entry_batch");
    arg_desc->SetDependency("ipg", CArgDescriptions::eExcludes, "target_only");
    arg_desc->SetDependency("ipg", CArgDescriptions::eExcludes, "ipg_batch");

    arg_desc->AddOptionalKey("ipg_batch", "input_file",
                     "Input file for batch processing (Format: one entry per line, IPG \n"
    		         "followed by optional space-delimited specifier(s) [range|strand|mask_algo_id]",
                     CArgDescriptions::eInputFile);
        arg_desc->SetDependency("ipg_batch", CArgDescriptions::eExcludes, "entry");
        arg_desc->SetDependency("ipg_batch", CArgDescriptions::eExcludes, "entry_batch");
        arg_desc->SetDependency("ipg_batch", CArgDescriptions::eExcludes, "range");
        arg_desc->SetDependency("ipg_batch", CArgDescriptions::eExcludes, "strand");
        arg_desc->SetDependency("ipg_batch", CArgDescriptions::eExcludes, "mask_sequence_with");

    arg_desc->AddOptionalKey(kArgTaxIdList, "taxonomy_ids",
    						"Comma-delimited taxonomy identifiers", CArgDescriptions::eString);
    arg_desc->SetDependency(kArgTaxIdList, CArgDescriptions::eExcludes, "entry");
    arg_desc->SetDependency(kArgTaxIdList, CArgDescriptions::eExcludes, "entry_batch");
    arg_desc->SetDependency(kArgTaxIdList, CArgDescriptions::eExcludes, "pig");

    arg_desc->AddOptionalKey(kArgTaxIdListFile, "input_file",
    						"Input file for taxonomy identifiers", CArgDescriptions::eInputFile);
    arg_desc->SetDependency(kArgTaxIdListFile, CArgDescriptions::eExcludes, "entry");
    arg_desc->SetDependency(kArgTaxIdListFile, CArgDescriptions::eExcludes, "entry_batch");
    arg_desc->SetDependency(kArgTaxIdListFile, CArgDescriptions::eExcludes, "pig");
    arg_desc->SetDependency(kArgTaxIdListFile, CArgDescriptions::eExcludes, kArgTaxIdList);

    arg_desc->AddFlag("info", "Print BLAST database information", true);
    // All other options to this program should be here
    const char* exclusions[]  = { "entry", "entry_batch", "outfmt", "strand",
        "target_only", "ctrl_a", "get_dups", "pig", "range",
        "mask_sequence", "list", "remove_redundant_dbs", "recursive",
        "list_outfmt", kArgTaxIdListFile.c_str(), kArgTaxIdList.c_str()};
    for (size_t i = 0; i < sizeof(exclusions)/sizeof(*exclusions); i++) {
        arg_desc->SetDependency("info", CArgDescriptions::eExcludes,
                                string(exclusions[i]));
    }

    arg_desc->AddFlag("tax_info",
    		          "Print taxonomic information contained in this BLAST database.\n"
    		          "Use -outfmt to customize output. Format specifiers supported are:\n"
    		          "\t\t%T means taxid\n"
    		          "\t\t%L means common taxonomic name\n"
    		          "\t\t%S means scientific name\n"
    		          "\t\t%K means taxonomic super kingdom\n"
    		          "\t\t%B means BLAST name\n"
    		          "By default it prints: '%T %S %L %K %B'\n", true);
    // All other options to this program should be here
    const char* tax_info_exclusions[]  = { "info", "entry", "entry_batch", "strand",
        "target_only", "ctrl_a", "get_dups", "pig", "range",
        "mask_sequence", "list", "remove_redundant_dbs", "recursive",
        "list_outfmt", kArgTaxIdListFile.c_str(), kArgTaxIdList.c_str() };
    for (size_t i = 0; i < sizeof(tax_info_exclusions)/sizeof(*tax_info_exclusions); i++) {
        arg_desc->SetDependency("tax_info", CArgDescriptions::eExcludes,
                                string(tax_info_exclusions[i]));
    }

    arg_desc->SetCurrentGroup("Sequence retrieval configuration options");
    arg_desc->AddOptionalKey("range", "numbers",
                         "Range of sequence to extract in 1-based offsets "
                         "(Format: start-stop, for start to end of sequence use start - )",
                         CArgDescriptions::eString);

    arg_desc->AddDefaultKey("strand", "strand",
                            "Strand of nucleotide sequence to extract",
                            CArgDescriptions::eString, "plus");
    arg_desc->SetConstraint("strand", &(*new CArgAllow_Strings, "minus",
                                        "plus"));

    arg_desc->AddOptionalKey("mask_sequence_with", "mask_algo_id",
                             "Produce lower-case masked FASTA using the "
                             "algorithm ID specified",
                             CArgDescriptions::eString);

    arg_desc->SetCurrentGroup("Output configuration options");
    arg_desc->AddDefaultKey(kArgOutput, "output_file", "Output file name",
                            CArgDescriptions::eOutputFile, "-");

    // The format specifiers below should be handled in
    // CSeqFormatter::x_Builder
    arg_desc->AddDefaultKey("outfmt", "format",
            "Output format, where the available format specifiers are:\n"
            "\t\t%f means sequence in FASTA format\n"
            "\t\t%s means sequence data (without defline)\n"
            "\t\t%a means accession\n"
            "\t\t%g means gi\n"
            "\t\t%o means ordinal id (OID)\n"
            "\t\t%i means sequence id\n"
            "\t\t%t means sequence title\n"
            "\t\t%l means sequence length\n"
            "\t\t%h means sequence hash value\n"
            "\t\t%T means taxid\n"
            "\t\t%X means leaf-node taxids\n"
            "\t\t%e means membership integer\n"
            "\t\t%L means common taxonomic name\n"
            "\t\t%C means common taxonomic names for leaf-node taxids\n"
            "\t\t%S means scientific name\n"
            "\t\t%N means scientific names for leaf-node taxids\n"
            "\t\t%B means BLAST name\n"     /* Is this useful outside NCBI? */
#if _DEBUG
            "\t\t%n means a list of links integers separated by ';'\n"
#endif /* _DEBUG */
            "\t\t%K means taxonomic super kingdom\n"
            "\t\t%P means PIG\n"
#if _DEBUG
            "\t\t%d means defline in text ASN.1 format\n"
            "\t\t%b means Bioseq in text ASN.1 format\n"
#endif /* _DEBUG */
            "\t\t%m means sequence masking data.\n"
            "\t\t   Masking data will be displayed as a series of 'N-M' values\n"
            "\t\t   separated by ';' or the word 'none' if none are available.\n"
#if _DEBUG
            "\tIf '%f' or '%d' are specified, all other format specifiers are ignored.\n"
            "\tFor every format except '%f' and '%d', each line of output will "
#else
            "\tIf '%f' is specified, all other format specifiers are ignored.\n"
            "\tFor every format except '%f', each line of output will "
#endif /* _DEBUG */
            "correspond\n\tto a sequence.\n",
            CArgDescriptions::eString, "%f");

    //arg_desc->AddDefaultKey("target_only", "value",
    //                        "Definition line should contain target gi only",
    //                        CArgDescriptions::eBoolean, "false");
    arg_desc->AddFlag("target_only",
                      "Definition line should contain target entry only", true);

    //arg_desc->AddDefaultKey("get_dups", "value",
    //                        "Retrieve duplicate accessions",
    //                        CArgDescriptions::eBoolean, "false");
    arg_desc->AddFlag("get_dups", "Retrieve duplicate accessions", true);
    arg_desc->SetDependency("get_dups", CArgDescriptions::eExcludes,
                            "target_only");

    arg_desc->SetCurrentGroup("Output configuration options for FASTA format");
    arg_desc->AddDefaultKey("line_length", "number", "Line length for output",
                        CArgDescriptions::eInteger,
                        NStr::IntToString(80));
    arg_desc->SetConstraint("line_length",
                            new CArgAllowValuesGreaterThanOrEqual(1));

    arg_desc->AddFlag("ctrl_a",
                      "Use Ctrl-A as the non-redundant defline separator",true);

    const char* exclusions_discovery[]  = { "entry", "entry_batch", "outfmt",
        "strand", "target_only", "ctrl_a", "get_dups", "pig", "range", kArgDb.c_str(),
        "info", "mask_sequence", "line_length" };
    arg_desc->SetCurrentGroup("BLAST database configuration and discovery options");
    arg_desc->AddFlag("show_blastdb_search_path",
                      "Displays the default BLAST database search paths", true);
    arg_desc->AddOptionalKey("list", "directory",
                             "List BLAST databases in the specified directory",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("remove_redundant_dbs",
                      "Remove the databases that are referenced by another "
                      "alias file in the directory in question", true);
    arg_desc->AddFlag("recursive",
                      "Recursively traverse the directory structure to list "
                      "available BLAST databases", true);
    arg_desc->AddDefaultKey("list_outfmt", "format",
            "Output format for the list option, where the available format specifiers are:\n"
            "\t\t%f means the BLAST database absolute file name path\n"
            "\t\t%p means the BLAST database molecule type\n"
            "\t\t%t means the BLAST database title\n"
            "\t\t%d means the date of last update of the BLAST database\n"
            "\t\t%l means the number of bases/residues in the BLAST database\n"
            "\t\t%n means the number of sequences in the BLAST database\n"
            "\t\t%U means the number of bytes used by the BLAST database\n"
            "\t\t%v means the BLAST database format version\n"
            "\tFor every format each line of output will "
            "correspond to a BLAST database.\n",
            CArgDescriptions::eString, "%f %p");
    for (size_t i = 0; i <
         sizeof(exclusions_discovery)/sizeof(*exclusions_discovery); i++) {
        arg_desc->SetDependency("list", CArgDescriptions::eExcludes,
                                string(exclusions_discovery[i]));
        arg_desc->SetDependency("recursive", CArgDescriptions::eExcludes,
                                string(exclusions_discovery[i]));
        arg_desc->SetDependency("remove_redundant_dbs", CArgDescriptions::eExcludes,
                                string(exclusions_discovery[i]));
        arg_desc->SetDependency("list_outfmt", CArgDescriptions::eExcludes,
                                string(exclusions_discovery[i]));
        arg_desc->SetDependency("show_blastdb_search_path", CArgDescriptions::eExcludes,
                                string(exclusions_discovery[i]));
    }
    arg_desc->SetDependency("show_blastdb_search_path", CArgDescriptions::eExcludes,
                            "list");
    arg_desc->SetDependency("show_blastdb_search_path", CArgDescriptions::eExcludes,
                            "recursive");
    arg_desc->SetDependency("show_blastdb_search_path", CArgDescriptions::eExcludes,
                            "list_outfmt");
    arg_desc->SetDependency("show_blastdb_search_path", CArgDescriptions::eExcludes,
                            "remove_redundant_dbs");

    arg_desc->AddFlag("exact_length", "Get exact length for db info", true);
    arg_desc->SetDependency("exact_length", CArgDescriptions::eRequires,
                            "info");
    arg_desc->AddFlag("long_seqids", "Use long seq id for fasta deflines", true);
    arg_desc->SetDependency("long_seqids", CArgDescriptions::eExcludes, "info");
    SetupArgDescriptions(arg_desc.release());
}

int CBlastDBCmdApp::Run(void)
{
    int status = 0;
    const CArgs& args = GetArgs();

    // Silences warning in CSeq_id for CSeq_id::fParse_PartialOK
    SetDiagFilter(eDiagFilter_Post, "!(1306.10)");
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostPrefix("blastdbcmd");

    try {
        CNcbiOstream& out = args["out"].AsOutputFile();
        if (args["show_blastdb_search_path"]) {
            out << CSeqDB::GenerateSearchPath() << NcbiEndl;
            return status;
        } else if (args["list"]) {
            const string& blastdb_dir = args["list"].AsString();
            const bool recurse = args["recursive"];
            const bool remove_redundant_dbs = args["remove_redundant_dbs"];
            const string dbtype = args[kArgDbType]
                ? args[kArgDbType].AsString()
                : "guess";
            const string& kOutFmt = args["list_outfmt"].AsString();
            const vector<SSeqDBInitInfo> dbs =
                FindBlastDBs(blastdb_dir, dbtype, recurse, true,
                             remove_redundant_dbs);
            CBlastDbFormatter blastdb_fmt(kOutFmt);
            ITERATE(vector<SSeqDBInitInfo>, db, dbs) {
                out << blastdb_fmt.Write(*db) << NcbiEndl;
            }
            return status;
        }

        if (args["info"]) {
        	x_InitBlastDB();
            x_PrintBlastDatabaseInformation();
        }
        else if (args["tax_info"]) {
        	x_InitBlastDB();
            x_PrintBlastDatabaseTaxInformation();
        }
        else if(args[kArgTaxIdList].HasValue() ||
                args[kArgTaxIdListFile].HasValue()) {
    		x_InitBlastDB_TaxIdList();
       		status = x_ProcessSearchRequest();
    	}
        else {
        	x_InitBlastDB();
       		status = x_ProcessSearchRequest();
        }
    	x_AddCmdOptions();

    } CATCH_ALL(status)

    m_UsageReport.AddParam(CBlastUsageReport::eExitStatus, status);
    return status;
}

void CBlastDBCmdApp::x_AddCmdOptions()
{
	const CArgs & args = GetArgs();
    if (args["info"]) {
    	 m_UsageReport.AddParam(CBlastUsageReport::eDBInfo, true);
    }
    else if (args["tax_info"]) {
    	 m_UsageReport.AddParam(CBlastUsageReport::eDBTaxInfo, true);
    }
    else if(args[kArgTaxIdList].HasValue() || args[kArgTaxIdListFile].HasValue()) {
    	 m_UsageReport.AddParam(CBlastUsageReport::eTaxIdList, true);
	}
    else if(args["ipg"].HasValue() || args["ipg_batch"].HasValue()) {
    	 m_UsageReport.AddParam(CBlastUsageReport::eIPGList, true);
    }
    else if(args["entry"].HasValue() || args["entry_batch"].HasValue()) {
    	 m_UsageReport.AddParam(CBlastUsageReport::eDBEntry, true);
    	 if (args["entry"].HasValue() && args["entry"].AsString() == "all") {
    	 	m_UsageReport.AddParam(CBlastUsageReport::eDBDumpAll, true);
    	}
		else {
    	 	m_UsageReport.AddParam(CBlastUsageReport::eDBEntry, true);
		}
    }
    if(args["outfmt"].HasValue()) {
    	m_UsageReport.AddParam(CBlastUsageReport::eOutputFmt, args["outfmt"].AsString());
    }


	string db_name = m_BlastDb->GetDBNameList();
	int off = db_name.find_last_of(CFile::GetPathSeparator());
    if (off != -1) {
    	db_name.erase(0, off+1);
	}
	m_UsageReport.AddParam(CBlastUsageReport::eDBName, db_name);
	m_UsageReport.AddParam(CBlastUsageReport::eDBLength, (Int8) m_BlastDb->GetTotalLength());
	m_UsageReport.AddParam(CBlastUsageReport::eDBNumSeqs, m_BlastDb->GetNumSeqs());
	m_UsageReport.AddParam(CBlastUsageReport::eDBDate, m_BlastDb->GetDate());
}



#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastDBCmdApp().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
