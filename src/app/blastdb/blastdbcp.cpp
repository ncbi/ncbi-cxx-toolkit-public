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
#include <algo/blast/blastinput/cmdline_flags.hpp>
#include <objtools/blast/seqdb_writer/build_db.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(blast);


/////////////////////////////////////////////////////////////////////////////
//  BlastdbCopyApplication::

static const string kTargetOnly("target_only");
static const string kMembershipBits("membership_bits");

static const string kCopyOnly("copy_only");

static const string kSwissprot("swissprot");
static const string kPdb("pdb");
static const string kRefseq("refseq");

class BlastdbCopyApplication : public CNcbiApplication
{
public:
    BlastdbCopyApplication();

private: /* Private Methods */
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    bool x_ShouldParseSeqIds(const string& dbname,
                             CSeqDB::ESeqType seq_type) const;

    bool x_ShouldCopyPIGs(const string& dbname,
                          CSeqDB::ESeqType seq_type) const;

private: /* Private Data */
    bool m_bCheckOnly;

    enum EMembershipBits {
        eUnused,
        eSwissprot,         // bit 1 (LSB?)
        ePdb,               // bit 2
        eRefseq,            // bit 3
        eReserved           // bit 4 (only appears with some bit 3 entries?)
    };

    typedef map<string, EMembershipBits> TMemBitMap;

    TMemBitMap m_MembershipMap;

};

/////////////////////////////////////////////////////////////////////////////
//  Constructor

BlastdbCopyApplication::BlastdbCopyApplication()
  : m_bCheckOnly(false)
{
    CRef<CVersion> version(new CVersion());
    version->SetVersionInfo(1, 0);
    SetFullVersion(version);

    m_MembershipMap[kSwissprot] = EMembershipBits::eSwissprot;
    m_MembershipMap[kPdb]       = EMembershipBits::ePdb;
    m_MembershipMap[kRefseq]    = EMembershipBits::eRefseq;
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

    arg_desc->AddOptionalKey(
            kArgDbTitle,
            "database_title",
            "Title for BLAST database",
            CArgDescriptions::eString
    );

    arg_desc->AddKey(
            kArgGiList,
            "input_file",
            "Text or binary gi file to restrict the BLAST "
            "database provided in -db argument",
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
            bool copy_membership_bits = false
    )
    {
        CStopWatch total_timer, bioseq_timer, memb_timer;
        total_timer.Start();
        for (int i = 0; i < gilist->GetNumGis(); i++) {
            const CSeqDBGiList::SGiOid& elem = gilist->GetGiOid(i);
            int oid = 0;
            if ( !blastdb->GiToOid(elem.gi, oid)) {
                // not found on source BLASTDB, skip
                continue;
            }
            if (m_Oids2Copy.insert(oid).second == false) {
                // don't add the same OID twice to avoid duplicates
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

            if (copy_membership_bits == false) {
                // If we're not copying the membership bits, we're done
                // with this gi, so loop back for the next one.
                continue;
            }

            memb_timer.Start();
            CRef<CBlast_def_line_set> hdr = CSeqDB::ExtractBlastDefline(*bs);
            ITERATE(CBlast_def_line_set::Tdata, itr, hdr->Get()) {
                CRef<CBlast_def_line> bdl = *itr;
                if (bdl->CanGetMemberships() &&
                        !bdl->GetMemberships().empty()) {
                    int memb_bits = bdl->GetMemberships().front();
                    if (memb_bits == 0) {
                        continue;
                    }
                    const string id = bdl->GetSeqid().front()->AsFastaString();
                    m_MembershipBits[memb_bits].push_back(id);
                }
            }
            memb_timer.Stop();
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
    }

    const TLinkoutMap GetMembershipBits() const {
        return m_MembershipBits;
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
    typedef list< CConstRef<CBioseq> > TBioseqs;
    TBioseqs m_Bioseqs;
    set<int> m_Oids2Copy;
    TLinkoutMap m_MembershipBits;
};

bool BlastdbCopyApplication::x_ShouldParseSeqIds(const string& dbname,
                                                 CSeqDB::ESeqType seq_type) const
{
    vector<string> file_paths;
    CSeqDB::FindVolumePaths(dbname, seq_type, file_paths);
    const char type = (seq_type == CSeqDB::eProtein ? 'p' : 'n');
    bool retval = false;
    const char* isam_extensions[] = { "si", "sd", "ni", "nd", NULL };

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


/////////////////////////////////////////////////////////////////////////////
//  Run the program
int BlastdbCopyApplication::Run(void)
{
    int retval = 0;
    const CArgs& args = GetArgs();

    // Setup Logging
    if (args["logfile"]) {
        SetDiagPostLevel(eDiag_Info);
        SetDiagPostFlag(eDPF_All);
        time_t now = time(0);
        LOG_POST( Info << string(72,'-') << "\n" << "NEW LOG - " << ctime(&now) );
    }

    CSeqDB::ESeqType seq_type = CSeqDB::eUnknown;
    try {{

        seq_type = ParseMoleculeTypeString(args[kArgDbType].AsString());
        CRef<CSeqDBGiList> gilist(
                new CSeqDBFileGiList(args[kArgGiList].AsString())
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
                args[kMembershipBits].AsBoolean()
        );
        const bool kIsSparse = false;
        const bool kParseSeqids = x_ShouldParseSeqIds(args[kArgDb].AsString(),
                                                      seq_type);


        const bool kUseGiMask = false;
        CStopWatch timer;
        timer.Start();
        CBuildDatabase destdb(args[kArgOutput].AsString(), title,
                              static_cast<bool>(seq_type == CSeqDB::eProtein),
                              kIsSparse, kParseSeqids, kUseGiMask,
                              &(args["logfile"].HasValue()
                               ? args["logfile"].AsOutputFile() : cerr));
        destdb.SetUseRemote(false);
        //destdb.SetVerbosity(true);
        destdb.SetSourceDb(sourcedb);
        destdb.StartBuild();
        destdb.SetMembBits(bioseq_source.GetMembershipBits(), false);
        destdb.AddSequences(bioseq_source, kCopyPIGs);
        destdb.EndBuild();
        timer.Stop();
        ERR_POST(Info << "Created BLAST database in " << timer.AsSmartString());
    }}
    catch (CException& ex) {
        LOG_POST( Error << ex );
        DeleteBlastDb(args[kArgOutput].AsString(), seq_type);
        retval = -1;
    }
    catch (...) {
        LOG_POST( Error << "Unknown error in BlastdbCopyApplication::Run()" );
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
    return BlastdbCopyApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
