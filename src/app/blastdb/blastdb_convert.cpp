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

/** @file blastdb_convert.cpp
 * Command line tool to convert BLAST databases from one version to another.
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/cmdline_flags.hpp>

#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbgeneral.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdb_lmdb.hpp>
#include <objtools/blast/seqdb_writer/writedb_error.hpp>
#include <objtools/blast/seqdb_writer/writedb_files.hpp>
#include <objtools/blast/seqdb_writer/writedb_lmdb.hpp>
#include <objtools/blast/seqdb_writer/build_db.hpp>

#include "../blast/blast_app_util.hpp"

#ifdef _OPENMP
#include <omp.h>
#endif

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif /* SKIP_DOXYGEN_PROCESSING */

/*** Constants ************/
/// Command line flag to represent the input
static const string kInput("in");
static const string kMapSize("map_size");
/// Alias file keyword for volumes
static const char* kDbList = "DBLIST";
/// Alias file keyword for oidlist
static const char* kOidList = "OIDLIST";
/// Used to copy index files
static const SIZE_TYPE kBufSize(4096);

class CWriteDB_FileAutoFlush : public CWriteDB_File {
public:
    CWriteDB_FileAutoFlush(const string & basename,
                           const string & extension,
                           int            index,
                           Uint8          max_file_size,
                           bool           always_create)
    : CWriteDB_File(basename, extension, index, max_file_size, always_create)
    {}
private:
    virtual void x_Flush() {}
};

/// The main application class
class CBlastdbConvertApp : public CNcbiApplication {
public:
    /** @inheritDoc */
    CBlastdbConvertApp():m_LogFile(NULL)
    {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
        m_StopWatch.Start();
        if (m_UsageReport.IsEnabled()) {
        	m_UsageReport.AddParam(CBlastUsageReport::eVersion, GetVersion().Print());
        	m_UsageReport.AddParam(CBlastUsageReport::eProgram, (string) "blastdb_convert");
        }
    }
    ~CBlastdbConvertApp() {
    	m_UsageReport.AddParam(CBlastUsageReport::eRunTime, m_StopWatch.Elapsed());
    }

private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    CNcbiOstream * m_LogFile;
    CBlastUsageReport m_UsageReport;
    CStopWatch m_StopWatch;
};

void CBlastdbConvertApp::Init()
{
    HideStdArgs(fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                  "Converts a BLAST databases from version 4 to "
                  "version 5, version " + CBlastVersion().Print());

    arg_desc->SetCurrentGroup("Input options");
    arg_desc->AddKey(kInput, "database_name", "Input database name",
                     CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey(kArgDbType, "molecule_type",
                            "Molecule type of the BLAST database to read",
                            CArgDescriptions::eString, "prot");
    arg_desc->SetConstraint(kArgDbType, &(*new CArgAllow_Strings,
                                        "nucl", "prot"));

    arg_desc->SetCurrentGroup("Configuration options");
    arg_desc->AddFlag("update_timestamp",
                      "Update the date of last update in the output database",
                      true);
    arg_desc->AddFlag("new_index",
                      "Generate vol index for filename",
                      true);
    arg_desc->AddDefaultKey(kMapSize, "memory_map_size_limit",
                                "Max mempry map size of output file",
                                CArgDescriptions::eInt8, "1000000000000");

    arg_desc->SetCurrentGroup("Output options");
    arg_desc->AddKey(kArgOutput, "database_name",
                     "Name of BLAST database to be created",
                     CArgDescriptions::eString);

    arg_desc->AddOptionalKey("logfile", "File_Name",
                             "File to which the program log should be redirected",
                             CArgDescriptions::eOutputFile,
                             CArgDescriptions::fAppend);

    SetupArgDescriptions(arg_desc.release());
}

static void s_LookForOidlistInAliasFile(const string& fname)
{
    _TRACE("Looking for " << kOidList << " in " << fname);
    CNcbiIfstream in(fname.c_str());
    string line;
    while (NcbiGetlineEOL(in, line)) {
        if (NStr::Find(line, kOidList) != NPOS) {
            NCBI_THROW(CBlastException, eNotSupported,
                       "databases with OID lists aren't supported");
        }
    }
}

/// Fixes the alias file contents to match the new database name
static void s_UpdateVolumesInAliasFile(const string & orig_alias_file,
									   const string & alias_file_name,
                                       const vector<string> & new_vol_names)
{

	CNcbiIfstream in(orig_alias_file.c_str());
    CNcbiOfstream out(alias_file_name.c_str());
    string line;
    while (NcbiGetlineEOL(in, line)) {
    	if (NStr::Find(line, kDbList) == NPOS) {
    		out << line <<endl;
    	}
    	else {
    		out << kDbList << " ";
    		for(unsigned int i=0; i < new_vol_names.size(); i++) {
    			out << new_vol_names[i] << " ";
    		}
    		out << endl;
    	}
    }
}

///
/// @param vol_num is the volume number attached to the source database, and it
///         doesn't have to match the ordinal_vol_num, but it often does
///         (except for some testing databases)
/// @param ordinal_vol_num this is the ordinal number for this volume (among
//          all the volumes being processed, regardless of the volume name)
static void s_ConvertV4toV5(const string& input_idx_fname,
		                    const string & output_idx_basename,
                            const string& lmdb_base,
                            bool is_prot,
                            int vol_num,
                            CDirEntry::TCopyFlags copy_flags)
{
    const Uint4 kFileSize = CFile(input_idx_fname).GetLength();
    CMemoryFileMap memfile(input_idx_fname);
    char* data = (char*)memfile.Map(0, 0);
    if (!data) {
        throw runtime_error("Failed to memory map " + input_idx_fname);
    }
    memfile.MemMapAdvise((void*)data, CMemoryFileMap::eMMA_Sequential);
    const string lmdb_fname =BuildLMDBFileName(lmdb_base, is_prot);
    const string idx_ext = is_prot?"pin":"nin";
    CWriteDB_FileAutoFlush out(output_idx_basename, idx_ext ,-1, 0, true);
    Uint4 read_offset = 0;

    // BLASTDB version
    Uint4 dbver = SeqDB_GetStdOrd((Uint4*)(data+read_offset));
    _ASSERT(dbver == 4);
    out.WriteInt4(static_cast<int>(eBDB_Version5));
    read_offset += sizeof(dbver);

    // Molecule type
    Int4 mol_type = SeqDB_GetStdOrd((Int4*)(data+read_offset));

    out.WriteInt4(mol_type);
    read_offset += sizeof(mol_type);

    // Volume number
    out.WriteInt4(vol_num);

    // Title length
    Uint4 title_len = SeqDB_GetStdOrd((Uint4*)(data+read_offset));
    read_offset += sizeof(title_len);

    string title;
    title.reserve(title_len);
    s_SeqDB_QuickAssign(title, data+read_offset, data+read_offset+title_len);
    read_offset += title_len;
    out.WriteInt4(title_len);
    out.Write(title);

    // lmdb file name
    out.WriteInt4(lmdb_fname.size());
    out.Write(lmdb_fname);

    // Date
    Uint4 date_len = SeqDB_GetStdOrd((Uint4*)(data+read_offset));
    read_offset += sizeof(date_len);

    string date;
    date.reserve(date_len);
    s_SeqDB_QuickAssign(date, data+read_offset, data+read_offset+date_len);
    read_offset += date_len;

    // FIXME: handle update timestamp case
    if (copy_flags & CDirEntry::fCF_PreserveTime) {
        out.WriteInt4(date_len);
        out.Write(date);
    } else {
        CTime now(CTime::eCurrent);
        date = now.AsString(CSeqDB::kBlastDbDateFormat);
       cerr << "FXIME: NEED TO WRITE UPDATED DATE" << endl;
        out.WriteInt4(date_len);
        out.Write(date);
    }

    string raw_data;
    raw_data.reserve(kBufSize);
    while ((kFileSize - read_offset) > kBufSize) {
        s_SeqDB_QuickAssign(raw_data, data+read_offset, data+read_offset+kBufSize);
        out.Write(raw_data);
        read_offset += kBufSize;
    }
    s_SeqDB_QuickAssign(raw_data, data+read_offset, data+read_offset+(kFileSize-read_offset));
    out.Write(raw_data);

    memfile.UnmapAll();
    out.Close();
}

void s_GetProfileDBsExt(vector<string> & extns)
{
	extns.push_back("rps");
	extns.push_back("loo");
	extns.push_back("aux");
	extns.push_back("freq");
	extns.push_back("blocks");
	extns.push_back("wcounts");
	extns.push_back("obsr");
}
void s_CheckDuplicateVols(const vector<string> & vols, const string & vol_name)
{
	for(unsigned int i =0; i < vols.size(); i++){
		if(vols[i] == vol_name){
			NCBI_THROW(CInvalidDataException, eInvalidInput,
			            "Duplicate db volumes");
		}
	}
}

int CBlastdbConvertApp::Run(void)
{
    const CArgs& args = GetArgs();
    const string& kInputDb = args[kInput].AsString();
    const string& kOutput = args[kArgOutput].AsString();
    const string& kMol(args[kArgDbType].AsString());
    const Int8 kMemoryMapSize(args[kMapSize].AsInt8());
    const CSeqDB::ESeqType seqtype = ParseMoleculeTypeString(kMol);
    const bool kIsProt = (seqtype == CSeqDB::eProtein);
    string kOutputAbsPath = CDirEntry::CreateAbsolutePath(kOutput);
    const bool kNewIndex = args["new_index"].HasValue() ? true: false;

    m_LogFile = & (args["logfile"].HasValue()? args["logfile"].AsOutputFile() : cout);
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostPrefix("blastdb_convert");

    int status = 0;
    bool cleanup = false;
    try {

    	if ( !CMemoryFileMap::IsSupported()) {
    	        NCBI_THROW(CBlastException, eNotSupported,
    	                   "Memory mapping not supported in this platform");
    	}

    	if(kOutput == kInputDb) {
    		 NCBI_THROW(CInvalidDataException, eInvalidInput,
    		            "Cannot create a BLAST database from an existing one without "
    		            "changing the output name, please provide a (different) database name ");
    	}

    	CBuildDatabase::CreateDirectories(kOutput);
    	m_LogFile = & (args["logfile"].HasValue() ? args["logfile"].AsOutputFile() : cout);

        // Check the appropriate version
        {{
            CRef<CSeqDB> source_blastdb(new CSeqDB(kInputDb, seqtype));
            if (source_blastdb && source_blastdb->GetBlastDbVersion() != eBDB_Version4) {
                NCBI_THROW(CBlastException, eNotSupported, "Only BLASTDBv4 can be used as input");
            }
        }}

        vector<string> paths, alias_files;
        bool use_index_in_filename = true;
        // FIXME: add error checking for non-handled cases
        if (getenv("NONDFLT") == NULL) {
            // gives the full real file paths. What to do with 16Smicrobial?
        	CSeqDB::FindVolumePaths(kInputDb, seqtype, paths, &alias_files, true, false);
        }
        else {
            CSeqDB::FindVolumePaths(kInputDb, seqtype, paths, &alias_files);
        }
        if(alias_files.size() > 1) {
        	NCBI_THROW(CBlastException, eNotSupported,
        	                       "database with multiple alias files is not supported");
        }
        if(alias_files.size() == 1) {
        	s_LookForOidlistInAliasFile (alias_files[0]);
        }

    	if (DeleteBlastDb(kOutputAbsPath, seqtype)) {
    		cerr << kOutputAbsPath << endl;
    	        	*m_LogFile << "Deleted existing BLAST database with identical name." << endl;
        }

        if(alias_files.size() == 0) {
        	use_index_in_filename = false;
        }

        vector<string> extns;
        SeqDB_GetFileExtensions(kIsProt, extns);
        if (kIsProt) {
        	s_GetProfileDBsExt(extns);
        }
        CDirEntry::TCopyFlags copy_flags = CDirEntry::fCF_Overwrite | CDirEntry::fCF_FollowLinks |
            CDirEntry::fCF_PreserveAll;
        if (args["update_timestamp"]) {
            copy_flags &= ~CDirEntry::fCF_PreserveTime;
        }

        int vol_ctr(0);
        int oid_total(0);
        vector<string> vol_names;
        vector<blastdb::TOid> vol_num_oids;
        CDirEntry output_dir(kOutput);

        const string lmdb_fname_w_path = BuildLMDBFileName(kOutput, kIsProt);
        unique_ptr<CWriteDB_LMDB> lmdbdb (new CWriteDB_LMDB(lmdb_fname_w_path, kMemoryMapSize));
        unique_ptr<CWriteDB_TaxID> taxdb (new CWriteDB_TaxID(
        		                              GetFileNameFromExistingLMDBFile(lmdb_fname_w_path, ELMDBFileType::eTaxId2Offsets),
        		                              kMemoryMapSize));
        cleanup = true;
        for (unsigned int p=0; p < paths.size(); p++) {
        	string & vol_path = paths[p];
            _TRACE("Processing " << vol_path);
            string vol_num = NStr::UIntToString(p);;
            string kOutputVol = output_dir.GetName();
            if(kNewIndex && use_index_in_filename) {
            	string zero_padding = kEmptyStr;
            	const string path_size_str = NStr::IntToString((int) paths.size());
            	unsigned int l = (path_size_str.size() < 2) ? 2 : path_size_str.size();
            	for (unsigned int x = vol_num.size(); x < l; x++){
            		zero_padding +='0';
            	}
            	kOutputVol += "." + zero_padding + vol_num;
            }
            else if(use_index_in_filename){
            	vector<string> parts;
            	NStr::Split(vol_path, ".", parts);
            	if(parts.size() < 2) {
            		NCBI_THROW(CInputException, eInvalidInput, "v4 db has no vol index in filename");
            	}
            	try {
            	   NStr::StringToInt(parts.back());
            	 } catch (CStringException &){
            		NCBI_THROW(CInputException, eInvalidInput, "v4 db has no vol index in filename");
            	 }

            	kOutputVol += "." + parts.back();
            }
            CRef<CSeqDB> vol(new CSeqDB(vol_path, seqtype));
            _ASSERT(vol);

             blastdb::TOid kNumOids(vol->GetNumOIDs());
             for (int oid = 0; oid < kNumOids; oid++) {
                  list<CRef<CSeq_id>> ids = vol->GetSeqIDs(oid);
                  lmdbdb->InsertEntries(ids, oid_total+oid);
            	  set<TTaxId> tax_ids;
            	  vol->GetAllTaxIDs(oid, tax_ids);
                  taxdb->InsertEntries(tax_ids, oid_total+oid);
             }

             s_CheckDuplicateVols(vol_names, kOutputVol);
             vol_names.push_back(kOutputVol);
             vol_num_oids.push_back(kNumOids);
             oid_total += kNumOids;

             for (auto ext : extns) {
            	 if (ext == "psi" || ext == "psd" ||
            		 ext == "nsi" || ext == "nsd") {
            		 continue;
            	 }
                 const string& kInputFile = vol_path + "." + ext;
                 CDirEntry de(kInputFile);
                 if (!de.Exists()) {
                	 continue;
                 }
                 const string kOutputFile = output_dir.GetDir() + kOutputVol + "." + ext;
                 if (ext == "pin" || ext == "nin") {
                	 s_ConvertV4toV5(de.GetPath(), output_dir.GetDir() + kOutputVol,
                			         output_dir.GetName(), kIsProt, p, copy_flags);
                 } else {
                	 _TRACE("cp -p " << de.GetPath() << " " << kOutputFile);
                	 if (!de.Copy(kOutputFile, copy_flags)) {
                		 throw runtime_error("Failed to cp " + de.GetPath()
                                             + " to " + kOutputFile);
                	 }
                 }
            }
            vol_ctr++;
        }

        lmdbdb->InsertVolumesInfo(vol_names, vol_num_oids);

        taxdb.reset();
        lmdbdb.reset();
        if(alias_files.size() == 1) {
        	const string kOutputFile = kOutput + "." + (kIsProt ? 'p' : 'n') + "al";
        	s_UpdateVolumesInAliasFile(alias_files[0],kOutputFile, vol_names);
        }

    } CATCH_ALL(status)

    if((status != 0) && cleanup) {
    	DeleteBlastDb(kOutputAbsPath, seqtype);
    }
    return status;
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastdbConvertApp().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
