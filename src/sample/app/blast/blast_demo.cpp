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
 * Authors:  Tom Madden
 *
 * File Description:
 *   Sample application for the running a blast search.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objmgr/object_manager.hpp>

#include <objects/seqalign/Seq_align_set.hpp>

#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/blast_prot_options.hpp>

#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(blast);


/////////////////////////////////////////////////////////////////////////////
//  CBlastDemoApplication::


class CBlastDemoApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    void ProcessCommandLineArgs(CRef<CBlastOptionsHandle> opts_handle);

};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CBlastDemoApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "BLAST demo program");

    arg_desc->AddKey
        ("program", "ProgramName",
         "One of blastn, megablast, dc-megablast, blastp, blastx, tblastn, tblastx, rpsblast",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("program", &(*new CArgAllow_Strings,
                "blastn", "megablast", "dc-megablast", "blastp", "blastx", "tblastn", "tblastx", "rpsblast"));

    arg_desc->AddDefaultKey
        ("db", "DataBase",
         "This is the name of the database",
         CArgDescriptions::eString, "nr");

    arg_desc->AddDefaultKey("in", "Queryfile",
                        "A file with the query", CArgDescriptions::eInputFile, "stdin");

    arg_desc->AddDefaultKey("out", "Outputfile",
                        "The output file", CArgDescriptions::eOutputFile, "stdout");

    arg_desc->AddDefaultKey("evalue", "evalue",
                        "E-value threshold for saving hits", CArgDescriptions::eDouble, "0");

    arg_desc->AddDefaultKey("penalty", "penalty", "Penalty score for a mismatch",
                            CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey("reward", "reward", "Reward score for a match",
                            CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey("matrix", "matrix", "Scoring matrix name",
                            CArgDescriptions::eString, "BLOSUM62");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

/// Modify BLAST options from defaults based upon command-line args.
///
/// @param opts_handle already created CBlastOptionsHandle to modify [in]
void CBlastDemoApplication::ProcessCommandLineArgs(CRef<CBlastOptionsHandle> opts_handle)

{
	const CArgs& args = GetArgs();

        // Expect value is a supported option for all flavors of BLAST.
        if(args["evalue"].AsDouble())
          opts_handle->SetEvalueThreshold(args["evalue"].AsDouble());
        
        // The first branch is used if the program is blastn or a flavor of megablast
        // as reward and penalty is a valid option.
        //
        // The second branch is used for all other programs except rpsblast as matrix
        // is a valid option for blastp and other programs that perform protein-protein
        // comparisons.
        //
        if (CBlastNucleotideOptionsHandle* nucl_handle =
              dynamic_cast<CBlastNucleotideOptionsHandle*>(&*opts_handle)) {

              if (args["reward"].AsInteger())
                nucl_handle->SetMatchReward(args["reward"].AsInteger());
            
              if (args["penalty"].AsInteger())
                nucl_handle->SetMismatchPenalty(args["penalty"].AsInteger());
        }
        else if (CBlastProteinOptionsHandle* prot_handle =
               dynamic_cast<CBlastProteinOptionsHandle*>(&*opts_handle)) {
              if (args["matrix"]) 
                prot_handle->SetMatrixName(args["matrix"].AsString().c_str());
        }

        return;
}


/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)


int CBlastDemoApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    EProgram program = ProgramNameToEnum(args["program"].AsString());

    bool db_is_aa = (program == eBlastp || program == eBlastx ||
                     program == eRPSBlast || program == eRPSTblastn);

    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(program));

    ProcessCommandLineArgs(opts);

    opts->Validate();  // Can throw CBlastException::eInvalidOptions for invalid option.


    // This will dump the options to stderr.
    // opts->GetOptions().DebugDumpText(cerr, "opts", 1);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    if (!objmgr) {
         throw std::runtime_error("Could not initialize object manager");
    }

    const bool is_protein =
        !!Blast_QueryIsProtein(opts->GetOptions().GetProgramType());
    SDataLoaderConfig dlconfig(is_protein);
    CBlastInputSourceConfig iconfig(dlconfig);
    CBlastFastaInputSource fasta_input(args["in"].AsInputFile(), iconfig);
    CScope scope(*objmgr);

    CBlastInput blast_input(&fasta_input);

    TSeqLocVector query_loc = blast_input.GetAllSeqLocs(scope);

    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(query_loc));

    const CSearchDatabase target_db(args["db"].AsString(),
        db_is_aa ? CSearchDatabase::eBlastDbIsProtein : CSearchDatabase::eBlastDbIsNucleotide);

    CLocalBlast blaster(query_factory, opts, target_db);

    CSearchResultSet results = *blaster.Run();

    // Get warning messages.
    for (unsigned int i = 0; i < results.GetNumResults(); i++) 
    {
        TQueryMessages messages = results[i].GetErrors(eBlastSevWarning);
        if (messages.size() > 0)
        {
            CConstRef<CSeq_id> seq_id = results[i].GetSeqId();
            if (seq_id.NotEmpty())
                cerr << "ID: " << seq_id->AsFastaString() << endl;
            else
                cerr << "ID: " << "Unknown" << endl;

            ITERATE(vector<CRef<CSearchMessage> >, it, messages)
                cerr << (*it)->GetMessage() << endl;
        }
    }
    
    CNcbiOstream& out = args["out"].AsOutputFile();

    for (unsigned int i = 0; i < results.GetNumResults(); i++) {
         CConstRef<CSeq_align_set> sas = results[i].GetSeqAlign();
         out << MSerial_AsnText << *sas;
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CBlastDemoApplication::Exit(void)
{
    // Do your after-Run() cleanup here
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBlastDemoApplication().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
