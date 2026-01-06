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
 * Author: Amelia Fong
 *
 */

/** @file makeclusterdb.cpp
 * Command line tool to create cluster databases.
 */

#include <ncbi_pch.hpp>
#include <serial/objostrjson.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <corelib/ncbiapp.hpp>

#include <serial/iterator.hpp>
#include <objmgr/util/create_defline.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seq/Seqdesc.hpp>
#include <objects/general/User_object.hpp>

#include <objects/seqfeat/Org_ref.hpp>
#include <objects/blastdb/Blast_db_mask_info.hpp>
#include <objects/blastdb/Blast_mask_list.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <objtools/blast/seqdb_writer/writedb.hpp>
#include <objtools/blast/seqdb_writer/writedb_error.hpp>
#include <util/format_guess.hpp>
#include <util/util_exception.hpp>
#include <objtools/blast/seqdb_writer/build_db.hpp>

#include <serial/objostrjson.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include "../blast/blast_app_util.hpp"
#include "masked_range_set.hpp"

#if defined(HAVE_IPS4O_HPP)
#include <ips4o.hpp>
#endif

#include <iostream>
#include <sstream>
#include <fstream>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif /* SKIP_DOXYGEN_PROCESSING */

class CCluster;

class CClusterSeq : public CObject {
public:
	CClusterSeq(CRef<CCluster> cluster, const string & id, bool is_refseq) :
		m_Cluster(cluster), m_Id(id), m_IsRefSeq(is_refseq) { }
	CRef<CCluster> & GetCluster() { return m_Cluster; }
	const string & GetId() const { return m_Id; }
	bool IsRefSeq() const { return m_IsRefSeq; }
	void SetOid(int64_t oid) { m_Oid = oid; }
	int64_t GetOid() const { return m_Oid; }
private:
	CRef<CCluster> m_Cluster;
	string m_Id;
	bool m_IsRefSeq;
	blastdb::TOid m_Oid;
};

class CCluster : public CObject {
public:
	CCluster (unsigned int cluster_id) : m_ClusterId(cluster_id) {}
	unsigned int GetClusterId() const { return m_ClusterId; }
	CRef<CClusterSeq> & GetRefSeq() { return m_RefSeq; }
	const string & GetRefSeqId() { return(m_RefSeq.Empty() ? kEmptyStr : m_RefSeq->GetId()); }
	void SetRefSeq(CRef<CClusterSeq> & r) {
		m_RefSeq.Reset(r); }
	const vector<CRef<CClusterSeq> > &  GetMemSeqs() { return m_MemSeqs; }
	void AddMemSeq(CRef<CClusterSeq> & m) {
		m_MemSeqs.push_back(m);
	}
	int64_t GetRefSeqOid() const {
		if(m_RefSeq.NotEmpty()) {
			return m_RefSeq->GetOid();
		}
		return -1;
	}
private:
	unsigned int m_ClusterId;
	CRef<CClusterSeq> m_RefSeq;
	vector<CRef<CClusterSeq> > m_MemSeqs;

};

bool SortClusterSeqs(const CRef<CClusterSeq> & a, const CRef<CClusterSeq> & b)
{
	return (a->GetId() < b->GetId());
}

bool SortCluster(const CRef<CCluster> & a, const CRef<CCluster> & b)
{
	return (a->GetRefSeqOid() < b->GetRefSeqOid());
}


class CClusterDBSource : public IRawSequenceSource {
public:
    CClusterDBSource(CRef<CSeqDBExpert> & source_db, vector<CRef<CCluster> > & clusters, CBuildDatabase * outdb);

    virtual ~CClusterDBSource()
    {
    }

    virtual bool GetNext(CTempString               & sequence,
                         CTempString               & ambiguities,
                         CRef<CBlast_def_line_set> & deflines,
                         vector<SBlastDbMaskData>  & mask_range,
                         vector<int>               & column_ids,
                         vector<CTempString>       & column_blobs);

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    virtual void GetColumnNames(vector<string> & names)
    {
        names = m_ColumnNames;
    }

    virtual int GetColumnId(const string & name)
    {
        return m_Source->GetColumnId(name);
    }

    virtual const map<string,string> & GetColumnMetaData(int id)
    {
        return m_Source->GetColumnMetaData(id);
    }
#endif

private:
    CRef<CSeqDBExpert> m_Source;
    vector<CRef<CCluster> > & m_Clusters;
    uint64_t m_CurrentCluster;
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    vector<CBlastDbBlob> m_Blobs;
    vector<int> m_ColumnIds;
    vector<string> m_ColumnNames;
    vector<int> m_MaskIds;
    map<int, int> m_MaskIdMap;
#endif
};

CClusterDBSource::CClusterDBSource(CRef<CSeqDBExpert> & source_db, vector<CRef<CCluster> > & cluster, CBuildDatabase * outdb)
    : m_Source(source_db), m_Clusters(cluster), m_CurrentCluster(0)
{
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    // Process mask meta data
    m_Source->GetAvailableMaskAlgorithms(m_MaskIds);
    ITERATE(vector<int>, algo_id, m_MaskIds) {
        objects::EBlast_filter_program algo;
        string algo_opts, algo_name;
        m_Source->GetMaskAlgorithmDetails(*algo_id, algo, algo_name, algo_opts);
        if (algo != EBlast_filter_program::eBlast_filter_program_other) {
            algo_name += NStr::IntToString(*algo_id);
        }
        m_MaskIdMap[*algo_id] = outdb->RegisterMaskingAlgorithm(algo, algo_opts, algo_name);
    }
    // Process columns
    m_Source->ListColumns(m_ColumnNames);
    for(int i = 0; i < (int)m_ColumnNames.size(); i++) {
        m_ColumnIds.push_back(m_Source->GetColumnId(m_ColumnNames[i]));
    }
#endif
	for (m_CurrentCluster=0; m_CurrentCluster< m_Clusters.size(); m_CurrentCluster++) {
		if(m_Clusters[m_CurrentCluster]->GetRefSeqOid() < 0) {
			LOG_POST(Warning << m_Clusters[m_CurrentCluster]->GetRefSeqId() + " not in source db");
		}
		else {
			break;
		}
	}

	if(m_CurrentCluster == m_Clusters.size()) {
        NCBI_THROW(CSeqDBException, eArgErr, "No valid cluster");
	}
}

bool
CClusterDBSource::GetNext(CTempString               & sequence,
                         CTempString               & ambiguities,
                         CRef<CBlast_def_line_set> & deflines,
                         vector<SBlastDbMaskData>  & mask_range,
                         vector<int>               & column_ids,
                         vector<CTempString>       & column_blobs)
{
	if(m_CurrentCluster >= m_Clusters.size()) {
		return false;
	}

    CRef<CCluster> cluster = m_Clusters[m_CurrentCluster];
	blastdb::TOid ref_oid = cluster->GetRefSeqOid();
	if (! m_Source->CheckOrFindOID(ref_oid)){
	    return false;
	}

	if (ref_oid != cluster->GetRefSeqOid()) {
        NCBI_THROW(CSeqDBException, eArgErr, "Oid not found");
	}

    const char * seq_ptr;
    int slength(0), alength(0);

    m_Source->GetRawSeqAndAmbig(ref_oid, &seq_ptr, & slength, & alength);

    sequence    = CTempString(seq_ptr, slength);
    ambiguities = CTempString(seq_ptr + slength, alength);

    CRef<CBlast_def_line_set> ref_defline_set = m_Source->GetHdr(ref_oid);
    CBlast_def_line_set::Tdata ref_deflines = ref_defline_set->Set();
    CBlast_def_line::TTaxIds taxids;
    CRef<CBlast_def_line> bf;

    CRef<CClusterSeq> ref_seq = cluster->GetRefSeq();
    CSeq_id ref_seqid(ref_seq->GetId());
   	CBlast_def_line::TTaxIds ref_ts;
    NON_CONST_ITERATE(CBlast_def_line_set::Tdata, itr, ref_deflines) {
    	CBlast_def_line::TTaxIds ts = (*itr)->GetTaxIds();
    	taxids.insert(ts.begin(), ts.end());
    	ITERATE(list< CRef<CSeq_id> >, seqid, (*itr)->GetSeqid()) {
    		if (ref_seqid.Match(**seqid)) {
    			bf.Reset(*itr);
    			ref_ts.insert(ts.begin(), ts.end());
    			break;
    		}
    	}
    }

    _ASSERT(bf.NotEmpty());
    const vector<CRef<CClusterSeq> > &  mem_seqs = cluster->GetMemSeqs();
    if (mem_seqs.size() > 0) {
    	vector<blastdb::TOid> mem_oids;
    	for (unsigned int i=0; i < mem_seqs.size(); i++) {
    		int64_t mem_oid = mem_seqs[i]->GetOid();
    		if (mem_oid < 0) {
    			LOG_POST(Warning << mem_seqs[i]->GetId() + " not in source db");
    			continue;
    		}
    		mem_oids.push_back(mem_oid);
    	}
#ifdef HAVE_IPS4O_HPP

#ifdef HAVE_LIBTBB
        ips4o::parallel::sort(mem_oids.begin(), mem_oids.end());
#else
        ips4o::sort(mem_oids.begin(), mem_oids.end());
#endif /* HAVE_LIBTBB */

#else
    	std::sort(mem_oids.begin(), mem_oids.end());
#endif /* HAVE_IPS4O_HPP */
    	set<TTaxId> mem_ts;
    	m_Source->GetTaxIdsForOids(mem_oids, mem_ts);
    	taxids.insert(mem_ts.begin(), mem_ts.end());
    }
	vector<CBlast_def_line::TTaxid> diff_ts;
	diff_ts.resize(taxids.size());
	vector<CBlast_def_line::TTaxid>::iterator diff_ts_itr;

    diff_ts_itr = std::set_difference(taxids.begin(), taxids.end(), ref_ts.begin(), ref_ts.end(), diff_ts.begin());
    diff_ts.resize(diff_ts_itr - diff_ts.begin());
    if (diff_ts.size() > 0) {
    	CBlast_def_line::TTaxIds leaf_ts(diff_ts.begin(), diff_ts.end());
    	const CBlast_def_line::TTaxIds& tx = bf->GetLeafTaxIds();
    	if(tx.size() > 0) {
    		leaf_ts.insert(tx.begin(), tx.end());
    	}
        bf->SetLeafTaxIds(leaf_ts);
    }

    deflines.Reset(new CBlast_def_line_set());
    deflines->Set().push_back(bf);

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    // process masks
    ITERATE(vector<int>, algo_id, m_MaskIds) {

        CSeqDB::TSequenceRanges ranges;
        m_Source->GetMaskData(ref_oid, *algo_id, ranges);

        SBlastDbMaskData mask_data;
        mask_data.algorithm_id = m_MaskIdMap[*algo_id];

        ITERATE(CSeqDB::TSequenceRanges, range, ranges) {
            mask_data.offsets.push_back(pair<TSeqPos, TSeqPos>(range->first, range->second));
        }

        mask_range.push_back(mask_data);
    }

    // The column IDs will be the same each time; another approach is
    // to only send back the IDs for those columns that are non-empty.
    column_ids = m_ColumnIds;
    column_blobs.resize(column_ids.size());
    m_Blobs.resize(column_ids.size());

    for(int i = 0; i < (int)column_ids.size(); i++) {
        m_Source->GetColumnBlob(column_ids[i], ref_oid, m_Blobs[i]);
        column_blobs[i] = m_Blobs[i].Str();
    }
#endif

    m_Source->RetSequence(&seq_ptr);
    m_CurrentCluster++;

    return true;
}


/// The main application class
class CMakeClusterDBApp : public CNcbiApplication {
public:

    /** @inheritDoc */
    CMakeClusterDBApp()
        : m_LogFile(NULL)
    {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
        m_StopWatch.Start();
        if (m_UsageReport.IsEnabled()) {
        	m_UsageReport.AddParam(CBlastUsageReport::eVersion, GetVersion().Print());
        	m_UsageReport.AddParam(CBlastUsageReport::eProgram, (string) "makeclusterdb");
        }
    }
    ~CMakeClusterDBApp() {
    	m_UsageReport.AddParam(CBlastUsageReport::eRunTime, m_StopWatch.Elapsed());
    }

private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    void x_BuildDatabase();

    void x_ProcessInputFile(const string & input_file);
    void x_ProcessInputData(const string & source_db, bool is_protein);

    void x_AddCmdOptions();

    // Data
    CNcbiOstream * m_LogFile;
    CRef<CBuildDatabase> m_DB;
    CRef<CSeqDBExpert> m_SourceDB;
    vector<CRef<CCluster> > m_Clusters;
    vector<CRef<CClusterSeq> > m_ClusterSeqs;

    CBlastUsageReport m_UsageReport;
    CStopWatch m_StopWatch;
};



/// Command line flag to represent the input
static const string kInput("in");
/// Defines token separators when multiple inputs are present
static const string kInputSeparators(" ");
/// Command line flag to represent the output
static const string kOutput("out");

void CMakeClusterDBApp::Init()
{
    HideStdArgs(fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                  "Application to create BLAST databases, version "
                  + CBlastVersion().Print());

    arg_desc->SetCurrentGroup("Input options");
    arg_desc->AddDefaultKey(kInput, "input_file",
                            "Input file",
                            CArgDescriptions::eInputFile, "-");
    arg_desc->AddDefaultKey(kArgDb, "source_db",
                            "Source DB",
                            CArgDescriptions::eString, "nr");

    arg_desc->AddDefaultKey(kArgDbType, "molecule_type",
                     "Molecule type of target db", CArgDescriptions::eString, "prot");
    arg_desc->SetConstraint(kArgDbType, &(*new CArgAllow_Strings,
                                        "nucl", "prot"));

    arg_desc->SetCurrentGroup("Configuration options");
    arg_desc->AddOptionalKey(kArgDbTitle, "database_title",
                             "Title for BLAST database\n",
                             CArgDescriptions::eString);

    arg_desc->SetCurrentGroup("Output options");
    arg_desc->AddOptionalKey(kOutput, "database_name",
                             "Name of BLAST database to be created\n",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("max_file_sz", "number_of_bytes",
                            "Maximum file size for BLAST database files",
                            CArgDescriptions::eString, NStr::UInt8ToString_DataSize(kDefaultVolFileSize, kUint8ToStringFlag, 3));
    arg_desc->SetConstraint("max_file_sz",
                            new CArgAllow_FileSize(kMinVolFileSize, kMaxVolFileSize));
    arg_desc->AddOptionalKey("metadata_output_prefix", "",
    						"Path prefix for location of database files in metadata", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("logfile", "File_Name",
                             "File to which the program log should be redirected",
                             CArgDescriptions::eOutputFile,
                             CArgDescriptions::fAppend);
    arg_desc->AddFlag("verbose", "Produce verbose output", true);

    SetupArgDescriptions(arg_desc.release());
}

/// Converts a Uint8 into a string which contains a data size (converse to
/// NStr::StringToUInt8_DataSize)
/// @param v value to convert [in]
/// @param minprec minimum precision [in]
static string Uint8ToString_DataSize(Uint8 v, unsigned minprec = 10)
{
    static string kMods = "KMGTPEZY";

    size_t i(0);
    for(i = 0; i < kMods.size(); i++) {
        if (v < Uint8(minprec)*1024) {
            v /= 1024;
        }
    }

    string rv = NStr::UInt8ToString(v);

    if (i) {
        rv.append(kMods, i, 1);
        rv.append("B");
    }

    return rv;
}

void CMakeClusterDBApp::x_ProcessInputFile(const string & input_file)
{
	 CNcbiIfstream input_stream(input_file);
	 string line = kEmptyStr;
	 CRef<CCluster> current_cluster;
	 unsigned int cluster_id = 0;
	 while (input_stream) {
		 getline(input_stream, line);
		 if(line.empty() || (line.find_first_not_of(' ') == std::string::npos)) {
			 continue;
		 }

		 vector<string>  cols;
		 NStr::Split(line, " \t", cols);
		 if (cols.size() < 3) {
			 continue;
		 }
		 string ref_id(cols[0]);
		 if(current_cluster.Empty() || (current_cluster->GetRefSeqId() != ref_id)) {
			 current_cluster.Reset(new CCluster(cluster_id));
			 cluster_id ++;
			 CRef<CClusterSeq> r_seq(new CClusterSeq(current_cluster, ref_id, true));
			 current_cluster->SetRefSeq(r_seq);
			 m_Clusters.push_back(current_cluster);
			 m_ClusterSeqs.push_back(r_seq);
		 }
		 string mem_id(cols[1]);
		 if (ref_id != mem_id) {
			 CRef<CClusterSeq> m(new CClusterSeq(current_cluster, mem_id, false));
			 current_cluster->AddMemSeq(m);
			 m_ClusterSeqs.push_back(m);
		 }
	 }

	 LOG_POST(Info <<"Num of Reference Seqs: " << cluster_id);
	 LOG_POST(Info <<"Num of Cluster Seqs: " << m_ClusterSeqs.size());
#ifdef HAVE_IPS4O_HPP

#ifdef HAVE_LIBTBB
        ips4o::parallel::sort(m_ClusterSeqs.begin(), m_ClusterSeqs.end(), SortClusterSeqs);
#else
        ips4o::sort(m_ClusterSeqs.begin(), m_ClusterSeqs.end(), SortClusterSeqs);
#endif /* HAVE_LIBTBB */

#else
	 std::sort(m_ClusterSeqs.begin(), m_ClusterSeqs.end(), SortClusterSeqs);
#endif /* HAVE_IPS4O_HPP */
}

void CMakeClusterDBApp::x_ProcessInputData(const string & source_db, bool is_protein)
{
	vector<string> accs;
	vector<blastdb::TOid> oids;
	accs.reserve(m_ClusterSeqs.size());
	oids.reserve(m_ClusterSeqs.size());
	CSeqDB::ESeqType seq_type = is_protein ? CSeqDB::eProtein : CSeqDB::eNucleotide;
	m_SourceDB.Reset(new CSeqDBExpert(source_db, seq_type));

	NON_CONST_ITERATE(vector<CRef<CClusterSeq> >, itr, m_ClusterSeqs) {
		accs.push_back((*itr)->GetId());
	}
	m_SourceDB->AccessionsToOids(accs, oids);

	if (oids.size() != m_ClusterSeqs.size()) {
        NCBI_THROW(CSeqDBException, eArgErr, " Accessions to Oids look up error");
	}

	for (uint64_t i=0; i < oids.size(); i++) {
		m_ClusterSeqs[i]->SetOid(oids[i]);
	}
#ifdef HAVE_IPS4O_HPP

#ifdef HAVE_LIBTBB
        ips4o::parallel::sort(m_Clusters.begin(), m_Clusters.end(), SortCluster);
#else
        ips4o::sort(m_Clusters.begin(), m_Clusters.end(), SortCluster);
#endif /* HAVE_LIBTBB */

#else
	std::sort(m_Clusters.begin(), m_Clusters.end(), SortCluster);
#endif /* HAVE_IPS4O_HPP */
}

void CMakeClusterDBApp::x_BuildDatabase()
{
    const CArgs & args = GetArgs();

    // Get arguments to the CBuildDatabase constructor.

    bool is_protein = (args[kArgDbType].AsString() == "prot");

    // 1. database name option if present
    // 2. else, kInput
    string out_dbname = (args[kOutput].HasValue() ? args[kOutput] : args[kInput]).AsString();

    string title = args[kArgDbTitle].HasValue() ? args[kArgDbTitle].AsString() : "Cluster " +  args[kArgDb].AsString();

    m_LogFile = & (args["logfile"].HasValue() ? args["logfile"].AsOutputFile() : cout);

    x_ProcessInputFile(args[kInput].AsString());

    x_ProcessInputData(args[kArgDb].AsString(), is_protein);

    bool long_seqids = true;
    bool limit_defline = false;
    m_DB.Reset(new CBuildDatabase(out_dbname, title, is_protein, false, true, false, m_LogFile,
                                  long_seqids, eBDB_Version5, limit_defline, 0));

    if (args["verbose"]) {
        m_DB->SetVerbosity(true);
    }

    // Max file size
    Uint8 bytes = NStr::StringToUInt8_DataSize(args["max_file_sz"].AsString());
    *m_LogFile << "Maximum file size: " << Uint8ToString_DataSize(bytes) << endl;

    m_DB->SetMaxFileSize(bytes);
    TIdToLeafs empty;
    m_DB->SetLeafTaxIds(empty, true);

    CRef<IRawSequenceSource> raw( new CClusterDBSource(m_SourceDB, m_Clusters, m_DB.GetPointer()));
    m_DB->AddSequences(*raw);

    bool success = m_DB->EndBuild();
   	string new_db = m_DB->GetOutputDbName();

#ifdef METADATA_CLUSTERDB  
    if(success) {
    	string new_db = m_DB->GetOutputDbName();
    	CSeqDB::ESeqType t = is_protein? CSeqDB::eProtein: CSeqDB::eNucleotide;
    	CSeqDB sdb(new_db, t);
        string output_prefix = args["metadata_output_prefix"]
                ? args["metadata_output_prefix"].AsString() : kEmptyStr;

        if (!output_prefix.empty() && (output_prefix.back() != CFile::GetPathSeparator())) {
            output_prefix += CFile::GetPathSeparator();
        }
    	CRef<CBlast_db_metadata> m = sdb.GetDBMetaData(output_prefix);
    	string extn (kEmptyStr);
    	SeqDB_GetMetadataFileExtension(is_protein, extn);
    	string metadata_filename = new_db + "." + extn;
        ofstream out(metadata_filename.c_str());
        unique_ptr<CObjectOStreamJson> json_out(new CObjectOStreamJson(out, eNoOwnership));
        json_out->SetDefaultStringEncoding(eEncoding_Ascii);
        json_out->PreserveKeyNames();
        CConstObjectInfo obj_info(m, m->GetTypeInfo());
        json_out->WriteObject(obj_info);
        json_out->Flush();
        out.flush();
        out << NcbiEndl;
    }
#endif
    (void)success; // to pacify compiler warning
}

int CMakeClusterDBApp::Run(void)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostPrefix("makeclusterdb");

    int status = 0;
    try { x_BuildDatabase(); }
    CATCH_ALL(status)
    x_AddCmdOptions();
    m_UsageReport.AddParam(CBlastUsageReport::eExitStatus, status);
    return status;
}

void CMakeClusterDBApp::x_AddCmdOptions()
{
	const CArgs & args = GetArgs();
    if (args[kArgDbType].HasValue()) {
    	 m_UsageReport.AddParam(CBlastUsageReport::eSeqType, args[kArgDbType].AsString());
    }
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CMakeClusterDBApp().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
