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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Sample test application for SRA reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <sra/readers/sra/sraread.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/scope.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

#include <klib/rc.h>
#include <klib/writer.h>
#include <sra/impl.h>
#include <sra/types.h>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CSRATestApp::


class CSRATestApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test


void CSRATestApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddOptionalKey("sra", "SRA_Accession",
                            "Accession and spot to load",
                            CArgDescriptions::eString);
    arg_desc->AddDefaultKey("sra_all", "SRA_Accession_All",
                            "Accession to get and test set of SRA sequences",
                            CArgDescriptions::eString,
                            "SRR000010.1.2");

    arg_desc->AddFlag("noclip", "Do not clip reads by quality");

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output file of ASN.1",
                            CArgDescriptions::eOutputFile,
                            "-");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test

void CheckRc(rc_t rc)
{
    if ( rc ) {
        char buffer[1024];
        size_t error_len;
        RCExplain(rc, buffer, sizeof(buffer), &error_len);
        cerr << "SRA Error: 0x" << hex << rc << dec << ": " << buffer << endl;
        exit(1);
    }
}

int CSRATestApp::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    CSraMgr::ETrim trim = CSraMgr::eTrim;
    if ( args["noclip"] ) {
        trim = CSraMgr::eNoTrim;
    }
    CSraMgr mgr("", "", trim);
    
    if ( args["sra"] ) {
        string sra = args["sra"].AsString();
        CRef<CSeq_entry> entry = mgr.GetSpotEntry(args["sra"].AsString());
        args["o"].AsOutputFile() << MSerial_AsnText << *entry << NcbiFlush;
    }
    else {
        CConstRef<CSeq_entry> all_entries;
        {{
            string sra = args["sra_all"].AsString();
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("gnl|SRA|"+sra);
            NcbiCout << "Loading data for " << idh << NcbiEndl;
            CRef<CObjectManager> om = CObjectManager::GetInstance();
            CGBDataLoader::RegisterInObjectManager(*om);
            CScope scope(*om);
            scope.AddDefaults();
            CBioseq_Handle bh = scope.GetBioseqHandle(idh);
            if ( !bh ) {
                ERR_POST(Fatal <<
                         "Cannot load reference SRA sequences from GenBank.");
            }
            all_entries = bh.GetTSE_Handle().GetCompleteTSE();
        }}
        NcbiCout << "Scanning data." << NcbiEndl;
        size_t count = 0;
        CSraRun run;
        ITERATE(CBioseq_set::TSeq_set, it, all_entries->GetSet().GetSeq_set()){
            const CSeq_entry& e1 = **it;
            const CBioseq& seq = e1.IsSeq()? e1.GetSeq():
                e1.GetSet().GetSeq_set().front()->GetSeq();
            string tag = seq.GetId().front()->GetGeneral().GetTag().GetStr();
            string sar = tag.substr(0, tag.rfind('.'));
            CRef<CSeq_entry> e2 = mgr.GetSpotEntry(sar, run);
            if ( !e2->Equals(e1) ) {
                NcbiCout << "Reference: "<<MSerial_AsnText<<e1;
                NcbiCout << "Generated: "<<MSerial_AsnText<<*e2;
                ERR_POST(Fatal<<"Different!");
            }
            ++count;
            //if ( count >= 10 ) break;
        }
        NcbiCout << "Equal records: " << count << NcbiEndl;
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CSRATestApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CSRATestApp().AppMain(argc, argv);
}
