#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

Author: Jason Papadopoulos

******************************************************************************/

/** @file blastall.cpp
 * Search one or more query sequences against a database 
 * using the C++ BLAST engine
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/seqsrc_seqdb.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blast_args.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include "blast_format.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

class CBlastall : public CNcbiApplication
{
private:
    virtual void Init();
    virtual int Run();

    CRef<CObjectManager> m_ObjMgr;
    CRef<CBlastallArgs> m_ArgDesc;
};


void CBlastall::Init()
{
    // get the object manager instance
    m_ObjMgr = CObjectManager::GetInstance();
    if (!m_ObjMgr) {
        throw std::runtime_error("Could not initialize object manager");
    }

    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    // formulate command line arguments

    m_ArgDesc.Reset(new CBlastallArgs());

    // read the command line

    SetupArgDescriptions(m_ArgDesc->SetCommandLine());
}


int CBlastall::Run(void)
{
    // Allow the fasta reader to complain on 
    // invalid sequence input
    SetDiagPostLevel(eDiag_Warning);

    const CArgs& args = GetArgs();
    
    EProgram program = ProgramNameToEnum(m_ArgDesc->GetProgram(args));

    // resolve strand specification

    ENa_strand strand = m_ArgDesc->GetQueryStrand(args);

    // resolve any ranges on query sequences

    Int4 from, to;
    m_ArgDesc->GetQueryLoc(args, from, to);

    // get ready to read the query from input file

    CBlastFastaInputSource fasta(*m_ObjMgr, 
                                 m_ArgDesc->GetQueryFile(args), strand,
                                 m_ArgDesc->GetLowercase(args),
                                 m_ArgDesc->GetBelieveQuery(args),
                                 from, to);
    CBlastInput input(&fasta, m_ArgDesc->GetQueryBatchSize(args));

    // initialize the database

    bool db_is_aa = (program == eBlastp || program == eBlastx);
    CSeqDB seqdb(m_ArgDesc->GetDatabase(args),
                 db_is_aa ? CSeqDB::eProtein : CSeqDB::eNucleotide);
    BlastSeqSrc* seq_src = SeqDbBlastSeqSrcInit(&seqdb);

    char* error_str = BlastSeqSrcGetInitError(seq_src);
    if (error_str) {
        string msg(error_str);
        sfree(error_str);
        NCBI_THROW(CBlastException, eSeqSrcInit, msg);
    }

    // the blast formatter requires that the database coexist in
    // the same scope with the query sequences

    CBlastDbDataLoader::RegisterInObjectManager(*m_ObjMgr,
                                m_ArgDesc->GetDatabase(args),
                                db_is_aa? (CBlastDbDataLoader::eProtein) : 
                                          (CBlastDbDataLoader::eNucleotide),
                                CObjectManager::eDefault);

    string loader_name = CBlastDbDataLoader::GetLoaderNameFromArgs(
                                m_ArgDesc->GetDatabase(args),
                                db_is_aa? (CBlastDbDataLoader::eProtein) :
                                          (CBlastDbDataLoader::eNucleotide));
    fasta.GetScope()->AddDataLoader(loader_name); 

    // fill in the blast options, and add a few tweaks now
    // that auxiliary information is known

    CRef<CBlastOptionsHandle> opts(m_ArgDesc->SetOptions(args));
    CBlastOptions& opt = opts->SetOptions();

    opt.SetStrandOption(strand);
    if (seq_src && !opt.GetEffectiveSearchSpace()) {
        opt.SetDbSeqNum(BlastSeqSrcGetNumSeqs(seq_src));
        if (m_ArgDesc->GetDBSize(args))
            opt.SetDbLength((Int8) m_ArgDesc->GetDBSize(args));
        else
            opt.SetDbLength(BlastSeqSrcGetTotLen(seq_src));
    }

    // set up the output

    CBlastFormat format(m_ArgDesc->GetProgram(args),
                        m_ArgDesc->GetDatabase(args),
                        m_ArgDesc->GetFormatType(args),
                        db_is_aa,
                        m_ArgDesc->GetBelieveQuery(args),
                        m_ArgDesc->GetOutputFile(args),
                        m_ArgDesc->GetAsnOutputFile(args),
                        m_ArgDesc->GetNumDescriptions(args),
                        opt.GetMatrixName(),
                        m_ArgDesc->GetShowGi(args),
                        m_ArgDesc->GetHtml(args),
                        opt.GetQueryGeneticCode(),
                        opt.GetDbGeneticCode(),
                        opt.GetSumStatisticsMode());

    format.PrintProlog();

    // run the search

    int status = 0;

    while (!fasta.End()) {
        // get the next batch of query sequences
        TSeqLocVector query_loc = input.GetNextSeqLocBatch();

        try {
            CRef<IQueryFactory> query_factory(
                                     new CObjMgr_QueryFactory(query_loc));
            CLocalBlast blaster(query_factory, opts, seq_src);

            // we have three separate entities to represent the
            // database: a seqsrc for blast, a data loader for the
            // formatter, and a raw SeqDB object that is needed
            // to reset the internal sequence iterator to the 
            // beginning of the database. This should probably change
            seqdb.ResetInternalChunkBookmark();

            // perform the search on the current batch
            blaster.SetNumberOfThreads(m_ArgDesc->GetNumThreads(args));
            CSearchResultSet results = blaster.Run();

            // output the alignments for the current batch
            for (size_t q = 0; q < query_loc.size(); q++) {
                format.PrintOneAlignSet(results[q], *fasta.GetScope(), opt);
            }

        } catch (const CBlastException& exptn) {
            cerr << exptn.what() << endl;
            status = exptn.GetErrCode();
        }
    }

    // finish up
    format.PrintEpilog(opt);
    BlastSeqSrcFree(seq_src);
    return status;
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastall().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
