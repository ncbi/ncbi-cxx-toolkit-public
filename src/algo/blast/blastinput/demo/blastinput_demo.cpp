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

/** @file blastinput_demo.cpp
 *  Demonstration application of the sequence input functionality of the
 *  blastinput library
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objmgr/util/sequence.hpp>
#include <algo/blast/blastinput/cmdline_flags.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

/////////////////////////////////////////////////////////////////////////////
//  CBlastInputDemoApplication::


class CBlastInputDemoApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CBlastInputDemoApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
      "blastinput library demo application to read sequence input");

    arg_desc->AddDefaultKey(kArgQuery, "input_file", "Input file name",
                     CArgDescriptions::eInputFile, kDfltArgQuery);

    arg_desc->AddDefaultKey(kArgOutput, "output_file", "Output file name",
                   CArgDescriptions::eOutputFile, "-");

    arg_desc->AddKey("mol_type", "molecule_type",
                     "Molecule type of the data being read",
                     CArgDescriptions::eString);
    arg_desc->SetConstraint("mol_type",
                            &(*new CArgAllow_Strings, "prot", "nucl"));

    arg_desc->AddDefaultKey("collect_stats", "boolean_value",
                            "Collect statistics about data being read?",
                            CArgDescriptions::eBoolean, "true");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

class CSequenceInputStats : public CObject {
public:
    CSequenceInputStats() : m_NumQueries(0), m_NumLetters(0), m_NumBatches(0) {}

    void AddQueryBatch(const CBlastQueryVector& query_batch) {
        m_NumQueries += query_batch.size();

        ITERATE(CBlastQueryVector, query, query_batch) {
            m_NumLetters += sequence::GetLength(*(*query)->GetQuerySeqLoc(),
                                                (*query)->GetScope());
        }
        m_NumBatches++;
    }

    unsigned int GetNumQueries() const { return m_NumQueries; }
    unsigned int GetNumBatches() const { return m_NumBatches; }
    Uint8 GetNumLetters() const { return m_NumLetters; }

    void PrintReport(CNcbiOstream& out, bool is_prot, CStopWatch& sw) const {
        out << "Elapsed time: " << sw.AsString() << " seconds" << endl;
        out << "Number of queries: " << GetNumQueries() << endl;
        out << "Number of " << (is_prot ? "residues" : "bases") << ": " 
            << GetNumLetters() << endl;
        out << "Number of batches: " << GetNumBatches() << endl;
    }

private:
    unsigned int m_NumQueries;
    Uint8 m_NumLetters;
    unsigned int m_NumBatches;
};


/////////////////////////////////////////////////////////////////////////////
//  Run demo


int CBlastInputDemoApplication::Run(void)
{
    const CArgs& args = GetArgs();
    int retval = 0;

    try {

        CNcbiIstream& in = args[kArgQuery].AsInputFile();
        CNcbiOstream& out = args[kArgOutput].AsOutputFile();
        bool collect_stats = args["collect_stats"].AsBoolean();
        bool is_prot = static_cast<bool>(args["mol_type"].AsString() == "prot");
        const EProgram kProgram = is_prot ? eBlastp : eBlastn;

        const SDataLoaderConfig dlconfig(is_prot);
        CBlastInputSourceConfig iconfig(dlconfig); // use defaults
        CBlastFastaInputSource fasta(in, iconfig);
        CBlastInput input(&fasta, GetQueryBatchSize(kProgram));
        CRef<CScope> scope = CBlastScopeSource(dlconfig).NewScope();
        CRef<CSequenceInputStats> stats;
        CStopWatch sw;

        if (collect_stats) {
            stats.Reset(new CSequenceInputStats);
            sw.Start();
        }

        // This is the idiomatic use of the CBlastInput class
        for (; !input.End(); scope->ResetHistory()) {
            CRef<CBlastQueryVector> query_batch(input.GetNextSeqBatch(*scope));
            
            if (collect_stats) {
                stats->AddQueryBatch(*query_batch);
            }
        }

        if (collect_stats) {
            sw.Stop();
            stats->PrintReport(out, is_prot, sw);
        }

    } catch (const CException& exptn) {
        cerr << "Error: " << exptn.GetMsg() << endl;
        retval = exptn.GetErrCode();
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        retval = -1;
    } catch (...) {
        cerr << "Unknown exception" << endl;
        retval = -1;
    }

    return retval;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CBlastInputDemoApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBlastInputDemoApplication().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
