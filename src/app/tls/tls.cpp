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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   1. open two files (one has contigs, one has master)
 *   2. for each contig, calculate the targeted locus name and add 
 *      AutodefOptions object with targeted locus name
 *   3. calculate the consensus targeted locus name and
 *      apply it to the master
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiutil.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objectio.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

// Objects includes
#include <objects/general/Object_id.hpp>
#include <objects/seq/Bioseq.hpp>


#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <util/line_reader.hpp>

#include <objects/seqset/Bioseq_set.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/edit/seq_entry_edit.hpp>

#include <serial/objcopy.hpp>


#include <common/test_assert.h>  /* This header must go last */


using namespace ncbi;
using namespace objects;

const char * TLS_APP_VER = "1.0";

/////////////////////////////////////////////////////////////////////////////
//
//  Application
//


class CTLSHandler
{
public:
    CTLSHandler() : m_ObjMgr(0), m_In(0)
    { m_ObjMgr = CObjectManager::GetInstance(); }

    virtual ~CTLSHandler() { }

    virtual void ProcessBioseq(CBioseq_Handle bh) {}

    void OpenInputFile(const string& filename, bool binary);
    void OpenOutputFile(const string& filename, bool binary);
    void ProcessAsnInput(void);
    void ProcessSeqSubmit(void);
    void ProcessSeqEntry(void);
    void ProcessSet(void);

    void ProcessSeqEntry(CRef<CSeq_entry> se);

    CRef<CBioseq_set> ReadBioseqSet(void);
    CRef<CSeq_entry> ReadSeqEntry(void);

    void StreamFile(const string& infile, const string& outfile, bool binary);

protected:
    CRef<CObjectManager> m_ObjMgr;
    auto_ptr<CObjectIStream> m_In;
    auto_ptr<CObjectOStream> m_Out;

    CRef<CScope> BuildScope(void);

};


void CTLSHandler::OpenInputFile(const string& file, bool binary)
{
    // file format 
    ESerialDataFormat format = binary ? eSerial_AsnBinary : eSerial_AsnText;

    m_In.reset(CObjectIStream::Open(file, format));

}


void CTLSHandler::OpenOutputFile(const string& file, bool binary)
{
    // file format 
    ESerialDataFormat format = binary ? eSerial_AsnBinary : eSerial_AsnText;

    m_Out.reset(CObjectOStream::Open(file, format));

}


CRef<CScope> CTLSHandler::BuildScope(void)
{
    CRef<CScope> scope(new CScope(*m_ObjMgr));
    scope->AddDefaults();

    return scope;
}


void CTLSHandler::ProcessAsnInput(void)
{
    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    string header = m_In->ReadFileHeader();

    bool unhandled = false;
    if (header == "Seq-submit") {  // Seq-submit
        ProcessSeqSubmit();
    } else if (header == "Seq-entry") {           // Seq-entry
        ProcessSeqEntry();
    } else if (header == "Bioseq-set") {  // Bioseq-set
        ProcessSet();
    } else {
        unhandled = true;
    }
    if (unhandled) {
        NCBI_THROW(CException, eUnknown, "Unhandled type " + header);
    }

}

void CTLSHandler::ProcessSeqEntry(CRef<CSeq_entry> se)
{
    CRef<CScope> scope = BuildScope();
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*se);
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
        ProcessBioseq(*bi);
        ++bi;
    }
    scope->RemoveTopLevelSeqEntry(seh);
}


void CTLSHandler::ProcessSeqSubmit(void)
{
    CRef<CSeq_submit> ss(new CSeq_submit);

    // Get seq-submit to process
    m_In->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);

    // Process Seq-submit
    CRef<CScope> scope = BuildScope();
    if (ss->GetData().IsEntrys()) {
        ITERATE(CSeq_submit::TData::TEntrys, se, ss->GetData().GetEntrys()) {
            ProcessSeqEntry(*se);
        }
    }
    *m_Out << *ss;

}


void CTLSHandler::ProcessSet(void)
{
    // Get Bioseq-set to process
    CRef<CBioseq_set> set(ReadBioseqSet());
    if (set && set->IsSetSeq_set()) {
        ITERATE(CBioseq_set::TSeq_set, se, set->GetSeq_set()) {
            ProcessSeqEntry(*se);
        }
    }
    *m_Out << *set;
}


void CTLSHandler::ProcessSeqEntry(void)
{
    // Get seq-entry to process
    CRef<CSeq_entry> se(ReadSeqEntry());

    ProcessSeqEntry(se);
    *m_Out << *se;
}



CRef<CBioseq_set> CTLSHandler::ReadBioseqSet(void)
{
    CRef<CBioseq_set> set(new CBioseq_set());
    m_In->Read(ObjectInfo(*set), CObjectIStream::eNoFileHeader);

    return set;
}


CRef<CSeq_entry> CTLSHandler::ReadSeqEntry(void)
{
    CRef<CSeq_entry> se(new CSeq_entry);
    m_In->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);

    return se;
}



class CTLSContigHandler : public CTLSHandler
{
public:
    CTLSContigHandler() : CTLSHandler(), m_Consensus(kEmptyStr) {}
    virtual ~CTLSContigHandler() {}

    virtual void ProcessBioseq(CBioseq_Handle bh);
    const string& GetConsensus() { return m_Consensus; }
    bool first = true;

protected:
    string m_Consensus;
};


void CTLSContigHandler::ProcessBioseq(CBioseq_Handle bh)
{
    string tls = edit::GenerateTargetedLocusName(bh);

    if (!NStr::IsBlank(tls)) {
        if (first || !NStr::IsBlank(m_Consensus)) {
            // if an earlier collision rendered the consensus name blank, it should stay blank
            m_Consensus = edit::GetTargetedLocusNameConsensus(m_Consensus, tls);
        }
        edit::SetTargetedLocusName(bh, tls);
        first = false;
    }
}


class CTLSMasterHandler : public CTLSHandler
{
public:
    CTLSMasterHandler() : CTLSHandler(), m_Consensus(kEmptyStr) {}
    virtual ~CTLSMasterHandler() {}

    void SetConsensus(const string& consensus) { m_Consensus = consensus; }
    virtual void ProcessBioseq(CBioseq_Handle bh);

protected:
    string m_Consensus;
};


void CTLSMasterHandler::ProcessBioseq(CBioseq_Handle bh)
{
    edit::SetTargetedLocusName(bh, m_Consensus);
}


class CTLSApp : public CNcbiApplication
{
public:
    CTLSApp(void);

    virtual void Init(void);
    virtual int  Run (void);

private:

    void Setup(const CArgs& args);

    CTLSContigHandler m_ContigHandler;
    CTLSMasterHandler m_MasterHandler;
};


CTLSApp::CTLSApp(void)
{
}


void CTLSApp::Init(void)
{
    // Prepare command line descriptions

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddKey("master", "MasterRecord", "File with master record",
        CArgDescriptions::eInputFile);
    arg_desc->AddKey("contigs", "Contigs", "File with contigs",
        CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("masterout", "MasterRecordOutputFile",
        "Master Record Output File (defaults to [master].tls)",
        CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey("contigsout", "ContigsOutputFile",
        "Contigs Output File (defaults to [contigs].tls)",
        CArgDescriptions::eOutputFile);
    arg_desc->AddFlag("b", "Input is in binary format");

    // Program description
    string prog_description = "Targeted Locus Name Generator\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());

}


int CTLSApp::Run(void)
{
    const CArgs& args = GetArgs();
    Setup(args);
 
    // process contigs
    const string& contigs_file = args["contigs"].AsString();
    m_ContigHandler.OpenInputFile(contigs_file, args["b"]);
    m_ContigHandler.OpenOutputFile(args["contigsout"] ? args["contigsout"].AsString() : contigs_file + ".tls", args["b"]);
    m_ContigHandler.ProcessAsnInput();    
    const string& consensus = m_ContigHandler.GetConsensus();

    // update master
    m_MasterHandler.SetConsensus(consensus);
    const string& master_file = args["master"].AsString();
    m_MasterHandler.OpenInputFile(master_file, args["b"]);
    m_MasterHandler.OpenOutputFile(args["masterout"] ? args["masterout"].AsString() : master_file + ".tls", args["b"]);
    m_MasterHandler.ProcessAsnInput();

            
    return 0;
}


void CTLSApp::Setup(const CArgs& args)
{
    // Setup application registry and logs for CONNECT library
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

}




/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CTLSApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
