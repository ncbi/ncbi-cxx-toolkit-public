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
 * Authors:  Ning Ma
 *
 */

/** @file seedtop_app.cpp
 * SEEDTOP command line application
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
	"$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/seedtop.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blastp_args.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <objmgr/util/sequence.hpp>
#include "blast_app_util.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

struct SSeedTopPattern
{
    string name;
    string pattern; 
};

// Read one seedtop pattern from file
// Returns "" if EOF or error encountered
static struct SSeedTopPattern s_ReadPattern(CNcbiIstream & in) {
    /* The pattern input file is unique for seedtop. Each pattern contains one ID initialed 
       lines for pattern identification, and one or more PA initialed lines for the actual 
       pattern specified using ProSite syntax.

       Pattern lines should be less than 100 letters long. Longer patterns can be specified 
       by multiple PA lines as given in the example. Here is a pattern input file with a single 
       pattern containing two PA lines. For testing purposes, we can use it with refseq protein 
       records such as YP_471346.1, YP_575330.1, or YP_564843.1. 

       A pattern input file can contain multiple patterns as long as they are separated by a 
       line with a single forward slash (/). 
    */
    struct SSeedTopPattern retv;
    char line[128]; 
    in.getline(line, 128);
    int len = in.gcount();
    if (len < 4 || line[0]!='I' || line[1]!='D' || line[2]!=' ') return retv;
    retv.name = string(&line[3], len-4);
    while(true) {
        in.getline(line, 100);
        len = in.gcount();
        if (len < 4 || line[0]!='P' || line[1]!='A' || line[2]!=' ') return retv;
        while (line[len-2] == ' ') len -= 1;
        if (line[len-2] == '>') line[len-2] = '-';
        else if (line[len-2] == '.') len -= 1;
        retv.pattern += string(&line[3], len-4);
    }
    return retv;
}

class CSeedTopApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CSeedTopApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();
};

static const string kPattern("pattern");
static const string kDb("db");
static const string kSubject("subject");
static const string kOutput("out");

void CSeedTopApp::Init()
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                 "Application to find pattern in BLAST databases or subject sequences, version "
                 + CBlastVersion().Print());

    arg_desc->AddDefaultKey(kPattern, "input_file",
                            "File containing the patterns to be searched",
                            CArgDescriptions::eInputFile, "-");

    arg_desc->AddOptionalKey(kDb, "database_name",
                            "Name of BLAST database to be searched",
                            CArgDescriptions::eString);

    arg_desc->AddOptionalKey(kSubject, "input_file",
                            "File containing the subject sequences in FASTA format",
                            CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey(kOutput, "output_file",
                            "Output file to include results of the search",
                            CArgDescriptions::eOutputFile, "-");

    arg_desc->SetDependency(kDb, CArgDescriptions::eExcludes, kSubject);

    SetupArgDescriptions(arg_desc.release());
}

int CSeedTopApp::Run(void)
{
    int status = BLAST_EXIT_SUCCESS;

    try {

        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);

        /*** Get the BLAST options ***/
        const CArgs& args = GetArgs();

        CNcbiIstream& f_pattern = args[kPattern].AsInputFile();
        CNcbiOstream& f_output  = args[kOutput].AsOutputFile();

        CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
        CRef<CLocalDbAdapter> db_adapter;

        if (args.Exist(kSubject) && args[kSubject]) {
            CNcbiIstream& f_subject = args[kSubject].AsInputFile();
            //TSeqRange subj_range;
            SDataLoaderConfig dlconfig(true);
            dlconfig.OptimizeForWholeLargeSequenceRetrieval();
            CBlastInputSourceConfig iconfig(dlconfig);
            iconfig.SetQueryLocalIdMode();
            CBlastFastaInputSource fasta(f_subject, iconfig);
            CBlastInput input(&fasta);
            CRef<blast::CBlastQueryVector> subjects(input.GetAllSeqs(*scope));
            CRef<IQueryFactory> qf(new blast::CObjMgr_QueryFactory(*subjects));
            CRef<CBlastOptionsHandle> opts_hndl
                                  (CBlastOptionsFactory::Create(eBlastp));
            db_adapter.Reset(new CLocalDbAdapter(qf, opts_hndl));
    
        } else if (args.Exist(kDb) && args[kDb]) {
                
            CRef<CSearchDatabase> db(new CSearchDatabase(args[kDb].AsString(), 
                                         CSearchDatabase::eBlastDbIsProtein));
            CRef<CSeqDB> seqdb = db->GetSeqDb();
            db_adapter.Reset(new CLocalDbAdapter(*db));
            scope->AddDataLoader(RegisterOMDataLoader(seqdb));
    
        } else {
            NCBI_THROW(CInputException, eInvalidInput,
                 "Either a BLAST database or subject sequence(s) must be specified");
        }
        _ASSERT(db_adapter);
    
        while (true) {

            struct SSeedTopPattern pattern = s_ReadPattern(f_pattern);
            if (pattern.pattern == "") break;

            CSeedTop seed_top(pattern.pattern);
            CSeedTop::TSeedTopResults results = seed_top.Run(db_adapter);
            CConstRef<CSeq_id> old_id(new CSeq_id());
            ITERATE(CSeedTop::TSeedTopResults, it, results) {
                const CSeq_id *sid = (*it)->GetId();
                const CBioseq_Handle& bhl = scope->GetBioseqHandle(*sid);

                if (sid->AsFastaString() != old_id->AsFastaString()) {
                    const CBioseq_Handle::TId ids = bhl.GetId();
                    f_output << endl << '>';
                    ITERATE(CBioseq_Handle::TId, id, ids) {
                        string idst((*id).AsString());
                        int index = idst.find_last_not_of('|');
                        f_output << string(idst, 0, index + 1) << "|" ;
                    }
                    
                    f_output << sequence::CDeflineGenerator().GenerateDefline(bhl) << endl << endl;
                    f_output << "ID " << pattern.name << endl;;
                    f_output << "PA " << pattern.pattern << endl;
                    old_id.Reset(sid);
                }

                f_output << "HI";
                ITERATE(CPacked_seqint_Base::Tdata, range, (*it)->GetPacked_int().Get()) {
                    static const ESeqLocExtremes ex = eExtreme_Positional;
                    f_output << " (" << (*range)->GetStart(ex)+1 << " " 
                                    << (*range)->GetStop(ex)+1 << ")";
                }
                f_output << endl;
                CSeqVector sv = bhl.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
                string sq;
                CSeq_loc::TRange tot_range = (*it)->GetTotalRange();
                sv.GetSeqData(tot_range.GetFrom(), tot_range.GetTo()+1, sq);
                f_output << "SQ " << sq << endl;
            }
 
            db_adapter->ResetBlastSeqSrcIteration();
       }

    } CATCH_ALL(status)
    return status;
}

#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CSeedTopApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
