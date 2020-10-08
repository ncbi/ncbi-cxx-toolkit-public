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
 */
/** @file blastdbcp.cpp
 * @author Christiam Camacho
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/cmdline_flags.hpp>
#include <objtools/blast/seqdb_writer/build_db.hpp>
#include <objtools/blast/seqdb_writer/impl/criteria.hpp>
#include <objtools/blast/blastdb_format/invalid_data_exception.hpp>
#include <algo/blast/api/blast_usage_report.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(blast);


/////////////////////////////////////////////////////////////////////////////
//  BlastdbCopyApplication::

class BlastdbCopyApplication : public CNcbiApplication
{
public:
    BlastdbCopyApplication();
    ~BlastdbCopyApplication() {
    	m_UsageReport.AddParam(CBlastUsageReport::eRunTime, m_StopWatch.Elapsed());
    }

private: /* Private Methods */
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    bool x_ShouldParseSeqIds(const string& dbname,
                             CSeqDB::ESeqType seq_type) const;

    bool x_ShouldCopyPIGs(const string& dbname,
                          CSeqDB::ESeqType seq_type) const;

    void x_MakeDBwIDList(const CArgs & args, CSeqDB::ESeqType seq_type, Uint8 bytes);
    void x_CopyDB(const CArgs & args, CSeqDB::ESeqType seq_type, Uint8 bytes);

private: /* Private Data */
    typedef map<string, ICriteria::EMembershipBit> TMemBitMap;

    TMemBitMap m_MembershipMap;

    const string kTargetOnly;
    const string kMembershipBits;
    const string kCopyOnly;

    CBlastUsageReport m_UsageReport;
    CStopWatch m_StopWatch;
};

/////////////////////////////////////////////////////////////////////////////
//  Constructor

BlastdbCopyApplication::BlastdbCopyApplication()
  : kTargetOnly("target_only"),
    kMembershipBits("membership_bits"),
    kCopyOnly("copy_only")
{
    CRef<CVersion> version(new CVersion());
    version->SetVersionInfo(1, 0);
    SetFullVersion(version);
    m_StopWatch.Start();
    if (m_UsageReport.IsEnabled()) {
    	m_UsageReport.AddParam(CBlastUsageReport::eVersion, GetVersion().Print());
    	m_UsageReport.AddParam(CBlastUsageReport::eProgram, (string) "blastdbcp");
    }
}


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void BlastdbCopyApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(
            GetArguments().GetProgramBasename(),
            "Performs a (deep) copy of a subset of a BLAST database"
    );

    arg_desc->SetCurrentGroup("BLAST database options");

    arg_desc->AddDefaultKey(
            kArgDb,
            "dbname",
            "BLAST database name",
            CArgDescriptions::eString, "nr"
    );

    arg_desc->AddDefaultKey(
            kArgDbType,
            "molecule_type",
            "Molecule type stored in BLAST database",
            CArgDescriptions::eString, "prot"
    );
    arg_desc->SetConstraint(
            kArgDbType,
            &(*new CArgAllow_Strings, "nucl", "prot", "guess")
    );

    arg_desc->SetCurrentGroup("Configuration options");

    arg_desc->AddOptionalKey(kArgGiList, "input_file", 
                             "Text or binary gi file to restrict the BLAST "
                             "database provided in -db argument\n"
                             "If text format is provided, it will be converted "
                             "to binary",
                             CArgDescriptions::eInputFile);   

    arg_desc->AddOptionalKey(kArgSeqIdList, "input_file", 
                             "Text sequence id or accession file to restrict "
                             "the BLAST database provided in -db argument",
                             CArgDescriptions::eInputFile);

    arg_desc->SetDependency(kArgSeqIdList, CArgDescriptions::eExcludes,
                            kArgGiList);

    arg_desc->AddOptionalKey(
            kArgDbTitle,
            "database_title",
            "Title for BLAST database",
            CArgDescriptions::eString
    );

    arg_desc->AddFlag(
            kMembershipBits,
            "Copy the membership bits",
            true
    );

    arg_desc->AddFlag(
            kTargetOnly,
            "Copy only entries specified in gi file",
            true
    );

    arg_desc->AddOptionalKey(
            kCopyOnly,
            "membership_bit",
            "Membership bit by which copied entries are filtered",
            CArgDescriptions::eString
    );

    arg_desc->AddFlag(
            "skip_gis",
            "Do not copy GI data",
            true
    );

    arg_desc->AddOptionalKey("blastdb_version", "version",
                             "Version of BLAST database to be created",
                             CArgDescriptions::eInteger);
    arg_desc->SetConstraint("blastdb_version",
                            new CArgAllow_Integers(eBDB_Version4, eBDB_Version5));

    const string kSwissprot("swissprot");
    const string kPdb("pdb");
    const string kRefseq("refseq");

    m_MembershipMap[kSwissprot] = ICriteria::eSWISSPROT;
    m_MembershipMap[kPdb]       = ICriteria::ePDB;
    m_MembershipMap[kRefseq]    = ICriteria::eREFSEQ;

    arg_desc->SetConstraint(
            kCopyOnly,
            &(*new CArgAllow_Strings, kSwissprot, kPdb, kRefseq)
    );

    arg_desc->SetCurrentGroup("Output options");

    arg_desc->AddOptionalKey(
            kArgOutput,
            "database_name",
            "Name of BLAST database to be created",
            CArgDescriptions::eString
    );

    arg_desc->AddDefaultKey("max_file_sz", "number_of_bytes",
                                "Maximum file size for BLAST database files",
                                CArgDescriptions::eString, "1GB");
    HideStdArgs(
            fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun
    );

    SetupArgDescriptions(arg_desc.release());
}

class CBlastDbBioseqSource : public IBioseqSource
{
public:
    CBlastDbBioseqSource(
            CRef<CSeqDBExpert> blastdb,
            CRef<CSeqDBGiList> gilist,
            CSeqDBFileGiList::EIdType idtype,
            bool               copy_membership_bits = false,
            bool               copy_leaf_taxids     = true
    )
    {
        CStopWatch total_timer, bioseq_timer, memb_timer, leaf_timer;
        total_timer.Start();
        int numSeqs = (idtype == CSeqDBFileGiList::eSiList)  ? gilist->GetNumSis() : gilist->GetNumGis(); 
        for (int i = 0; i < numSeqs; i++) {            
           int oid = 0;
            if(!x_GetOidFromSeqID(blastdb,gilist,idtype,i,oid)) {
                // not found on source BLASTDB, skip
                continue;
            }
            //const CSeqDBGiList::SSiOid& elem = gilist->GetSiOid(i);
            if (m_Oids2Copy.insert(oid).second == false) {
                // don't add the same OID twice, to avoid duplicates
                continue;
            }

            bioseq_timer.Start();
            CRef<CBioseq> bs_nc = blastdb->GetBioseq(oid);
            if (bs_nc.Empty()) {
                // If we end up here, all sequences were probably filtered
                // out by the membership bit setting for this gi.
                bioseq_timer.Stop();
                continue;
            }

            CConstRef<CBioseq> bs(&*bs_nc);
            m_Bioseqs.push_back(bs);
            bioseq_timer.Stop();

            if (copy_membership_bits) {
                memb_timer.Start();
                CRef<CBlast_def_line_set> hdr =
                        CSeqDB::ExtractBlastDefline(*bs);
                ITERATE(CBlast_def_line_set::Tdata, itr, hdr->Get()) {
                    CRef<CBlast_def_line> bdl = *itr;
                    if (bdl->CanGetMemberships() &&
                            !bdl->GetMemberships().empty()) {
                        int memb_bits = bdl->GetMemberships().front();
                        if (memb_bits == 0) {
                            continue;
                        }
                        const string id =
                                bdl->GetSeqid().front()->AsFastaString();
                        m_MembershipBits[memb_bits].push_back(id);
                    }
                }
                memb_timer.Stop();
            }

            if (copy_leaf_taxids) {
                leaf_timer.Start();
                CRef<CBlast_def_line_set> hdr =
                        CSeqDB::ExtractBlastDefline(*bs);
                ITERATE(CBlast_def_line_set::Tdata, itr, hdr->Get()) {
                    CRef<CBlast_def_line> bdl = *itr;
                    CBlast_def_line::TTaxIds leafs = bdl->GetLeafTaxIds();
                    const string id =
                            bdl->GetSeqid().front()->AsFastaString();
                    set<TTaxId> ids = m_LeafTaxids[id];
                    ids.insert(leafs.begin(), leafs.end());
                    m_LeafTaxids[id] = ids;
                }
                leaf_timer.Stop();
            }
        }
        total_timer.Stop();
        ERR_POST(
                Info << "Will extract " << m_Bioseqs.size()
                << " sequences from the source database"
        );
        ERR_POST(
                Info << "Processed all input data in "
                << total_timer.AsSmartString()
        );
        ERR_POST(
                Info << "Processed bioseqs in "
                << bioseq_timer.AsSmartString()
        );
        ERR_POST(
                Info << "Processed membership bits in "
                << memb_timer.AsSmartString()
        );
        ERR_POST(
                Info << "Processed leaf taxids in "
                << leaf_timer.AsSmartString()
        );
    }

    

    const TLinkoutMap GetMembershipBits() const
    {
        return m_MembershipBits;
    }

    const TIdToLeafs GetLeafTaxIds() const
    {
        return m_LeafTaxids;
    }

    virtual CConstRef<CBioseq> GetNext()
    {
        if (m_Bioseqs.empty()) {
            return CConstRef<CBioseq>(0);
        }
        CConstRef<CBioseq> retval = m_Bioseqs.back();
        m_Bioseqs.pop_back();
        return retval;
    }
private:
    bool x_GetOidFromSeqID(CRef<CSeqDBExpert> blastdb,CRef<CSeqDBGiList> gilist,CSeqDBFileGiList::EIdType idtype, int ind, int &oid) 
    {
        if(idtype == CSeqDBFileGiList::eGiList) {
            const CSeqDBGiList::SGiOid& elem = gilist->GetGiOid(ind);    
            return blastdb->GiToOid(elem.gi, oid);                
        }
        else { //if(idtype == CSeqDBFileGiList::eSiList) 
            const CSeqDBGiList::SSiOid& elem = gilist->GetSiOid(ind);            
            CSeq_id seqID(elem.si);
            return blastdb->SeqidToOid(seqID, oid);                
        }        
    }

    typedef list< CConstRef<CBioseq> > TBioseqs;

    TBioseqs    m_Bioseqs;
    set<int>    m_Oids2Copy;
    TLinkoutMap m_MembershipBits;
    TIdToLeafs  m_LeafTaxids;
};

class CBlastDbAllBioseqSource : public IBioseqSource
{
public:
    CBlastDbAllBioseqSource(CRef<CSeqDBExpert> blastdb): m_BlastDb (blastdb), m_Oid(0)
    { }

    virtual CConstRef<CBioseq> GetNext();

private:
	CRef<CSeqDBExpert> m_BlastDb;
    int	m_Oid;
};

CConstRef<CBioseq> CBlastDbAllBioseqSource::GetNext()
{
	CConstRef<CBioseq> bs(0);
	while (m_BlastDb->CheckOrFindOID(m_Oid)) {
		CRef<CBioseq> bs_nc = m_BlastDb->GetBioseq(m_Oid);
        if (bs_nc.Empty()) {
              // If we end up here, all sequences were probably filtered
              // out by the membership bit setting for this gi.
        	  m_Oid++;
              continue;
         }

         bs.Reset(&*bs_nc);
         m_Oid++;
         break;
	}
    return bs;
}

bool BlastdbCopyApplication::x_ShouldParseSeqIds(const string& dbname,
                                                 CSeqDB::ESeqType seq_type) const
{
    vector<string> file_paths;
    CSeqDB::FindVolumePaths(dbname, seq_type, file_paths);
    const char type = (seq_type == CSeqDB::eProtein ? 'p' : 'n');
    bool retval = false;
    const char* isam_extensions[] = { "si", "sd", "ni", "nd", "os", NULL };

    ITERATE(vector<string>, f, file_paths) {
        for (int i = 0; isam_extensions[i] != NULL; i++) {
            CNcbiOstrstream oss;
            oss << *f << "." << type << isam_extensions[i];
            const string fname = CNcbiOstrstreamToString(oss);
            CFile file(fname);
            if (file.Exists() && file.GetLength() > 0) {
                retval = true;
                break;
            }
        }
        if (retval) break;
    }

    if (!retval) {
    	 ITERATE(vector<string>, f, file_paths) {
    		std::size_t found = f->find_last_of(".");
            CNcbiOstrstream oss;
            oss << f->substr(0, found) << "." << type << "os";
            const string fname = CNcbiOstrstreamToString(oss);
            CFile file(fname);
            if (file.Exists() && file.GetLength() > 0) {
                retval = true;
                break;
            }
    	 }
    }
    return retval;
}



bool BlastdbCopyApplication::x_ShouldCopyPIGs(const string& dbname,
											  CSeqDB::ESeqType seq_type) const
{
	if(CSeqDB::eProtein != seq_type)
		return false;

	vector<string> file_paths;
	CSeqDB::FindVolumePaths(dbname, CSeqDB::eProtein, file_paths);
    ITERATE(vector<string>, f, file_paths) {
    	CNcbiOstrstream oss;
        oss << *f << "." << "ppd";
        const string fname = CNcbiOstrstreamToString(oss);
        CFile file(fname);
        if (file.Exists() && file.GetLength() > 0)
                return true;
    }
     return false;
}


void BlastdbCopyApplication::x_MakeDBwIDList(const CArgs & args, CSeqDB::ESeqType seq_type, Uint8 bytes)
{
    CSeqDBFileGiList::EIdType idtype = args[kArgSeqIdList] ? CSeqDBFileGiList::eSiList : CSeqDBFileGiList::eGiList;
    string seqListFile = args[kArgSeqIdList] ? args[kArgSeqIdList].AsString() : args[kArgGiList].AsString();
    CRef<CSeqDBGiList> gilist(
            new CSeqDBFileGiList(seqListFile,idtype)
    );

    CSeqDBExpert* dbexpert;
    if (args[kTargetOnly].AsBoolean()) {
        dbexpert = new CSeqDBExpert(
                args[kArgDb].AsString(),
                seq_type,
                &*gilist
        );
    } else {
        dbexpert = new CSeqDBExpert(
                args[kArgDb].AsString(),
                seq_type
        );
    }
    CRef<CSeqDBExpert> sourcedb(dbexpert);

    if (args[kCopyOnly].HasValue()) {
        string mbitName = args[kCopyOnly].AsString();
        TMemBitMap::iterator it = m_MembershipMap.find(mbitName);
        if (it != m_MembershipMap.end()) {
            sourcedb->SetVolsMemBit(it->second);
        }
    }

    string title;
    if (args[kArgDbTitle].HasValue()) {
        title = args[kArgDbTitle].AsString();
    } else {
        CNcbiOstrstream oss;
        oss << "Copy of '" << sourcedb->GetDBNameList() << "': " << sourcedb->GetTitle();
        title = CNcbiOstrstreamToString(oss);
    }

    const bool kCopyPIGs = x_ShouldCopyPIGs(args[kArgDb].AsString(),
                                                          seq_type);
    CBlastDbBioseqSource bioseq_source(
            sourcedb,
            gilist,
            idtype,
            args[kMembershipBits].AsBoolean()
    );
    const bool kIsSparse = false;
    const bool kParseSeqids = (idtype == CSeqDBFileGiList::eGiList) ? x_ShouldParseSeqIds(args[kArgDb].AsString(),seq_type): true;


    const bool kUseGiMask = false;
    EBlastDbVersion dbver =  sourcedb->GetBlastDbVersion();
    if (args["blastdb_version"]) {
    		dbver = static_cast<EBlastDbVersion>(args["blastdb_version"].AsInteger());
    }

    CStopWatch timer;
    timer.Start();
    CBuildDatabase destdb(args[kArgOutput].AsString(), title,
                          static_cast<bool>(seq_type == CSeqDB::eProtein),
                          kIsSparse, kParseSeqids, kUseGiMask,
                          &(args["logfile"].HasValue()
                           ? args["logfile"].AsOutputFile() : cerr), false, dbver);
    destdb.SetUseRemote(false);
	destdb.SetMaxFileSize(bytes);
    destdb.SetSkipCopyingGis(args["skip_gis"]);
    //destdb.SetVerbosity(true);
    destdb.SetSourceDb(sourcedb);
    destdb.StartBuild();
    destdb.SetMembBits(bioseq_source.GetMembershipBits(), false);
    destdb.SetLeafTaxIds(bioseq_source.GetLeafTaxIds(), false);
    destdb.AddSequences(bioseq_source, kCopyPIGs);
    destdb.EndBuild();
    timer.Stop();
    ERR_POST(Info << "Created BLAST database in " << timer.AsSmartString());
}

void BlastdbCopyApplication::x_CopyDB(const CArgs & args, CSeqDB::ESeqType seq_type,  Uint8 bytes)
{
    CRef<CSeqDBExpert> sourcedb(new CSeqDBExpert( args[kArgDb].AsString(), seq_type));

    string title;
	if (args[kArgDbTitle].HasValue()) {
	    title = args[kArgDbTitle].AsString();
	} else {
	    CNcbiOstrstream oss;
	    oss << sourcedb->GetTitle();
	    title = CNcbiOstrstreamToString(oss);
	}

	const bool kCopyPIGs = x_ShouldCopyPIGs(args[kArgDb].AsString(), seq_type);
	CBlastDbAllBioseqSource bioseq_source(sourcedb);
	const bool kIsSparse = false;
	const bool kParseSeqids = x_ShouldParseSeqIds(args[kArgDb].AsString(),seq_type);


	const bool kUseGiMask = false;
	EBlastDbVersion dbver =  sourcedb->GetBlastDbVersion();
	if (args["blastdb_version"]) {
		dbver = static_cast<EBlastDbVersion>(args["blastdb_version"].AsInteger());
	}

    CBuildDatabase destdb(args[kArgOutput].AsString(), title,
	                          static_cast<bool>(seq_type == CSeqDB::eProtein),
	                          kIsSparse, kParseSeqids, kUseGiMask,
	                          &(args["logfile"].HasValue()
	                           ? args["logfile"].AsOutputFile() : cerr), false, dbver);
	destdb.SetUseRemote(false);
	destdb.SetMaxFileSize(bytes);
	destdb.SetSkipCopyingGis(args["skip_gis"]);
	//destdb.SetVerbosity(true);
	destdb.SetSourceDb(sourcedb);
	destdb.StartBuild();
	TLinkoutMap membershipBits;
	TIdToLeafs  leafTaxids;
	destdb.SetMembBits(membershipBits, true);
	destdb.SetLeafTaxIds(leafTaxids, true);
	destdb.AddSequences(bioseq_source, kCopyPIGs);
	destdb.EndBuild();
}

/////////////////////////////////////////////////////////////////////////////
//  Run the program
int BlastdbCopyApplication::Run(void)
{
    int retval = 0;
    const CArgs& args = GetArgs();    
    
    if (!(args.Exist(kArgOutput) && args[kArgOutput].HasValue())) {
        NCBI_THROW(CInputException, eInvalidInput,
                   "Must specify " + kArgOutput);
        
    }
   
    // Setup Logging
    if (args["logfile"]) {
        SetDiagPostLevel(eDiag_Info);
        SetDiagPostFlag(eDPF_All);
	SetDiagPostPrefix("blastdbcp");
        time_t now = time(0);
        LOG_POST( Info << string(72,'-') << "\n" << "NEW LOG - " << ctime(&now) );
    }

    Uint8 bytes = NStr::StringToUInt8_DataSize(args["max_file_sz"].AsString());
    static const Uint8 MAX_VOL_FILE_SIZE = 0x100000000;
    if (bytes >= MAX_VOL_FILE_SIZE) {
    	NCBI_THROW(CInvalidDataException, eInvalidInput, "max_file_sz must be < 4 GiB");
    }

   	CSeqDB::ESeqType seq_type = CSeqDB::eUnknown;
    try {{
   	    seq_type = ParseMoleculeTypeString(args[kArgDbType].AsString());
    	if(args[kArgSeqIdList] || args[kArgGiList]){
    		x_MakeDBwIDList(args, seq_type, bytes);
    	}
    	else {
    		x_CopyDB(args, seq_type, bytes);
    	}

    }}
    catch (CException& ex) {
        ERR_POST( Error << ex );
        DeleteBlastDb(args[kArgOutput].AsString(), seq_type);
        retval = -1;
    }
    catch (...) {
        ERR_POST( Error << "Unknown error in BlastdbCopyApplication::Run()" );
        DeleteBlastDb(args[kArgOutput].AsString(), seq_type);
        retval = -2;
    }

    return retval;
}

/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void BlastdbCopyApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return BlastdbCopyApplication().AppMain(argc, argv);
}
