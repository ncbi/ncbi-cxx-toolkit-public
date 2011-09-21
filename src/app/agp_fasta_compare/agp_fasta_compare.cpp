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
 * Authors:  Mike DiCuccio, Michael Kornbluh
 *
 * File Description:
 *     Makes sure an AGP file builds the same sequence found in a FASTA
 *     file.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <util/checksum.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/lds2/lds2_dataloader.hpp>
#include <objtools/lds2/lds2.hpp>
#include <objtools/readers/agp_read.hpp>
#include <objtools/readers/fasta.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace {
    // command-line argument names
    const static string kArgnameAgp = "agp";
    const static string kArgnameObjFasta = "objfasta";
    const static string kArgnameCompFasta = "compfasta";
}

/////////////////////////////////////////////////////////////////////////////
//  CAgpFastaCompareApplication::


class CAgpFastaCompareApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    // TKey pair is: MD5 checksum and sequence length
    typedef pair<string, TSeqPos> TKey;
    typedef map<TKey, CSeq_id_Handle> TUniqueSeqs;

    void x_ProcessObjectFasta(CNcbiIstream& istr,
                        TUniqueSeqs& seqs );
    void x_ProcessAgp(CNcbiIstream& istr,
                      TUniqueSeqs& seqs);

    void x_Process(const CSeq_entry_Handle seh,
                   TUniqueSeqs& seqs);

};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CAgpFastaCompareApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddKey(kArgnameAgp, "Agp",
                     "AGP data to process",
                     CArgDescriptions::eInputFile);

    arg_desc->AddKey(kArgnameObjFasta, "ObjFasta",
                     "Fasta sequences to process, which contains the "
                     "objects we're comparing against the agp file.",
                     CArgDescriptions::eInputFile );

    arg_desc->AddOptionalKey(kArgnameCompFasta, "CompFasta",
        "Optional Fasta sequences to process, which contains the "
        "components.  This is useful if the components are not yet in genbank",
        CArgDescriptions::eInputFile );

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)


int CAgpFastaCompareApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    CRef<CObjectManager> om(CObjectManager::GetInstance());

    // load local component FASTA sequences and Genbank into 
    // local scope for lookups using local data storage
    auto_ptr<CTmpFile> ldsdb_file;
    CRef<CLDS2_Manager> lds_mgr;
    if( args[kArgnameCompFasta] ) {
        ldsdb_file.reset( new CTmpFile ); // file deleted on object destruction
        cout << "Using temporary FASTA database at " 
            << ldsdb_file->GetFileName() << endl;
        lds_mgr.Reset(new CLDS2_Manager( ldsdb_file->GetFileName() ));
        lds_mgr->AddDataFile( args[kArgnameCompFasta].AsString() );
        lds_mgr->UpdateData();
        CLDS2_DataLoader::RegisterInObjectManager(*om, ldsdb_file->GetFileName(), -1,
            CObjectManager::eDefault, 1 );
    }
    CGBDataLoader::RegisterInObjectManager(*om, 0, 
        CObjectManager::eDefault, 2 );

    // calculate checksum of the AGP sequences and the FASTA sequences
    CNcbiIstream& istr_fasta = args[kArgnameObjFasta].AsInputFile();
    CNcbiIstream& istr_agp = args[kArgnameAgp].AsInputFile();

    TUniqueSeqs fasta_ids;
    x_ProcessObjectFasta(istr_fasta, fasta_ids);

    TUniqueSeqs agp_ids;
    x_ProcessAgp(istr_agp, agp_ids);

    TUniqueSeqs::const_iterator iter1 = fasta_ids.begin();
    TUniqueSeqs::const_iterator iter1_end = fasta_ids.end();

    TUniqueSeqs::const_iterator iter2 = agp_ids.begin();
    TUniqueSeqs::const_iterator iter2_end = agp_ids.end();

    // make sure set of sequences in obj FASTA match AGP's objects.
    // Print discrepancies.
    bool bThereWereDifferences = false;
    LOG_POST(Error << "Reporting differences...");
    for ( ;  iter1 != iter1_end  &&  iter2 != iter2_end;  ) {
        if (iter1->first < iter2->first) {
            LOG_POST(Error << "  " << iter1->second << ": in FASTA only");
            bThereWereDifferences = true;
            ++iter1;
        }
        else if (iter2->first < iter1->first) {
            LOG_POST(Error << "  " << iter2->second << ": in AGP only");
            bThereWereDifferences = true;
            ++iter2;
        }
        else {
            ++iter1;
            ++iter2;
        }
    }

    for ( ;  iter1 != iter1_end;  ++iter1) {
        LOG_POST(Error << "  " << iter1->second << ": in FASTA only");
        bThereWereDifferences = true;
    }

    for ( ;  iter2 != iter2_end;  ++iter2) {
        LOG_POST(Error << "  " << iter2->second << ": in AGP only");
        bThereWereDifferences = true;
    }

    // success only if there were no differences
    return ( bThereWereDifferences ? 1 : 0 );
}


void CAgpFastaCompareApplication::x_Process(const CSeq_entry_Handle seh,
                                            TUniqueSeqs& seqs)
{
    for (CBioseq_CI bioseq_it(seh);  bioseq_it;  ++bioseq_it) {
        CSeqVector vec(*bioseq_it, CBioseq_Handle::eCoding_Iupac);
        string data;
        vec.GetSeqData(0, bioseq_it->GetBioseqLength() - 1, data);

        CChecksum cks(CChecksum::eMD5);
        cks.AddLine(data);

        string md5;
        cks.GetMD5Digest(md5);

        CSeq_id_Handle idh = sequence::GetId(*bioseq_it,
                                             sequence::eGetId_Best);

        TKey key(md5, bioseq_it->GetBioseqLength());
        seqs.insert(TUniqueSeqs::value_type(key, idh));

        CNcbiOstrstream os;
        ITERATE (string, i, key.first) {
            os << setw(2) << setfill('0') << hex << (int)((unsigned char)*i);
        }

        LOG_POST(Error << "  " << idh << ": "
                 << string(CNcbiOstrstreamToString(os))
                 << " / " << key.second);
    }
}


void CAgpFastaCompareApplication::x_ProcessObjectFasta(CNcbiIstream& istr,
                                                 TUniqueSeqs& fasta_ids)
{
    LOG_POST(Error << "Processing FASTA...");
    CFastaReader reader(istr);
    while (istr) {
        try {
            CRef<CSeq_entry> entry = reader.ReadOneSeq();

            CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
            CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
            x_Process(seh, fasta_ids);
        }
        catch (CException& ) {
            break;
        }
    }
}


void CAgpFastaCompareApplication::x_ProcessAgp(CNcbiIstream& istr,
                                               TUniqueSeqs& agp_ids)
{
    LOG_POST(Error << "Processing AGP...");
    while (istr) {
        vector< CRef<CSeq_entry> > entries;
        AgpRead(istr, entries);
        ITERATE (vector< CRef<CSeq_entry> >, it, entries) {
            CRef<CSeq_entry> entry = *it;

            CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
            CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
            scope->AddDefaults();

            x_Process(seh, agp_ids);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CAgpFastaCompareApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CAgpFastaCompareApplication().AppMain(argc, argv);
}
