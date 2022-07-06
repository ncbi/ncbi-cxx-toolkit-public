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
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Byte_graph.hpp>
#include <serial/iterator.hpp>
#include <common/test_data_path.h>

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
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

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
    arg_desc->AddFlag("no_sra", "Check for a missing SRA accession");

    arg_desc->AddFlag("noclip", "Do not clip reads by quality");

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output file of ASN.1",
                            CArgDescriptions::eOutputFile,
                            "-");
    arg_desc->AddOptionalKey("save", "SaveFile",
                             "Save new reference entry ASN.1",
                             CArgDescriptions::eOutputFile);

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


pair<CRef<CSeq_entry>, vector<CRef<CSeq_graph>>> x_ExtractGraphs(const CSeq_entry& entry)
{
    pair<CRef<CSeq_entry>, vector<CRef<CSeq_graph>>> ret;
    ret.first = SerialClone(entry);
    for ( CTypeIterator<CSeq_graph> it = Begin(*ret.first); it; ++it ) {
        CSeq_graph& g = *it;
        ret.second.push_back(Ref(SerialClone(g)));
        auto& b = g.SetGraph().SetByte();
        b.SetMin(0);
        b.SetMax(0);
        b.SetAxis(0);
        b.SetValues().clear();
    }
    return ret;
}


template<class Iterator>
size_t x_CountBad(Iterator beg, Iterator end)
{
    const int kBadThreshold = 20;
    size_t ret = 0;
    for ( ; beg != end; ++beg ) {
        ret += (*beg < kBadThreshold);
    }
    return ret;
}


bool x_ToReject(const CSeq_graph& graph)
{
    auto& vv = graph.GetGraph().GetByte().GetValues();
    size_t s = vv.size();
    const size_t kBadSideLength = 10;
    if ( s >= kBadSideLength &&
         (x_CountBad(vv.begin(), vv.begin()+kBadSideLength) == kBadSideLength ||
          x_CountBad(vv.end()-kBadSideLength, vv.end()) == kBadSideLength) ) {
        return true;
    }
    if ( x_CountBad(vv.begin(), vv.end()) > s/2 ) {
        return true;
    }
    return false;
}


CRef<CSeq_graph> x_GetLite(const CSeq_graph& graph, bool reject)
{
    CRef<CSeq_graph> ret(SerialClone(graph));
    char v = reject? 3: 30;
    CByte_graph& b = ret->SetGraph().SetByte();
    fill(b.SetValues().begin(), b.SetValues().end(), v);
    b.SetMin(v);
    b.SetMax(v);
    return ret;
}


CRef<CSeq_graph> x_GetLite(const CSeq_graph& graph)
{
    return x_GetLite(graph, x_ToReject(graph));
}


bool x_Equals(const CSeq_graph& graph, const CSeq_graph& reference)
{
    if ( graph.Equals(reference) ) {
        return true;
    }
    // convert quality graph to lite version and check again
    if ( graph.Equals(*x_GetLite(reference, false)) ||
         graph.Equals(*x_GetLite(reference, true)) ) {
        return true;
    }
    return false;
}


bool x_Equals(const CSeq_entry& entry, const CSeq_entry& reference)
{
    if ( entry.Equals(reference) ) {
        return true;
    }
    // quality graph may be converted to lite version
    auto split_entry = x_ExtractGraphs(entry);
    auto split_reference = x_ExtractGraphs(reference);
    if ( !split_entry.first->Equals(*split_reference.first) ) {
        return false;
    }
    if ( split_entry.second.size() != split_reference.second.size() ) {
        return false;
    }
    for ( size_t i = 0; i < split_entry.second.size(); ++i ) {
        if ( !x_Equals(*split_entry.second[i], *split_reference.second[i]) ) {
            return false;
        }
    }
    return true;
}


int CSRATestApp::Run(void)
{
    SetDiagPostLevel(eDiag_Info);
    CNcbiEnvironment env;
    env.Set("GENBANK_ID2_DEBUG", "5");
    // Get arguments
    const CArgs& args = GetArgs();

    CSraMgr::ETrim trim = CSraMgr::eTrim;
    if ( args["noclip"] ) {
        trim = CSraMgr::eNoTrim;
    }
    CSraMgr mgr(trim);
    
    if ( args["sra"] ) {
        string sra = args["sra"].AsString();
        if ( args["no_sra"] ) {
            try {
                mgr.GetSpotEntry(sra);
                ERR_POST(Fatal<<
                         "GetSpotEntry("<<sra<<") unexpectedly succeeded");
            }
            catch ( CSraException& exc ) {
                if ( exc.GetErrCode() != CSraException::eNotFoundDb ) {
                    ERR_POST(Fatal<<
                             "Wrong exception for a missing SRA acc: "<<
                             sra<<": "<<exc);
                }
            }
            NcbiCout << "Correctly detected missing SRA acc: " << sra
                     << NcbiEndl;
            return 0;
        }
        try {
            string acc = sra;
            // exclude spot id
            SIZE_TYPE dot_pos = acc.find('.');
            if ( dot_pos != NPOS ) {
                acc.resize(dot_pos);
            }
            string path = CSraPath().FindAccPath(acc);
            NcbiCout << "Resolved "<<acc<<" -> "<<path<<NcbiEndl;
        }
        catch ( CSraException& exc ) {
            ERR_POST("FindAccPath failed: "<<exc);
        }
        CRef<CSeq_entry> entry = mgr.GetSpotEntry(sra);
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
            if ( args["no_sra"] ) {
                if ( bh ) {
                    ERR_POST(Fatal<<
                             "GetBioseqHandle("<<idh<<") unexpectedly succeeded");
                }
                string sar = sra.substr(0, sra.rfind('.'));
                try {
                    mgr.GetSpotEntry(sar);
                    ERR_POST(Fatal<<
                             "GetSpotEntry("<<sar<<") unexpectedly succeeded");
                }
                catch ( CSraException& exc ) {
                    if ( exc.GetErrCode() != CSraException::eNotFoundDb ) {
                        ERR_POST(Fatal<<
                                 "Wrong exception for a missing SRA acc: "<<
                                 sar<<": "<<exc);
                    }
                }
                NcbiCout << "Correctly detected missing SRA acc: " << sra
                         << NcbiEndl;
                return 0;
            }
            if ( !bh ) {
                string file_name = string(NCBI_GetTestDataPath())+"/sra/"+sra+".asn";
                NcbiCout << "Loading GenBank version of "<<sra<<" from "<<file_name << NcbiEndl;
                CNcbiIfstream in(file_name.c_str());
                if ( in ) {
                    CRef<CSeq_entry> entry(new CSeq_entry);
                    in >> MSerial_AsnText >> *entry;
                    scope.ResetHistory();
                    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
                    bh = seh.GetTSE_Handle().GetBioseqHandle(idh);
                }
            }
            if ( !bh ) {
                ERR_POST(Fatal <<
                         "Cannot load reference SRA sequences from GenBank.");
            }
            all_entries = bh.GetTSE_Handle().GetCompleteTSE();
        }}
        if ( args["save"] ) {
            CSraRun run;
            CRef<CSeq_entry> new_entries(new CSeq_entry);
            new_entries->SetSet().SetLevel(0);
            new_entries->SetSet().SetClass(CBioseq_set::eClass_other);
            new_entries->SetSet().SetDescr().Assign(all_entries->GetSet().GetDescr());
            ITERATE(CBioseq_set::TSeq_set, it, all_entries->GetSet().GetSeq_set()){
                const CSeq_entry& e1 = **it;
                const CBioseq& seq = e1.IsSeq()? e1.GetSeq():
                    e1.GetSet().GetSeq_set().front()->GetSeq();
                string tag = seq.GetId().front()->GetGeneral().GetTag().GetStr();
                string sar = tag.substr(0, tag.rfind('.'));
                CRef<CSeq_entry> e2 = mgr.GetSpotEntry(sar, run);
                new_entries->SetSet().SetSeq_set().push_back(e2);
            }
            NcbiCout << "Saving reference into "<<args["save"].AsString()<<NcbiEndl;
            args["save"].AsOutputFile() << MSerial_AsnText << *new_entries << NcbiFlush;
        }
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
            if ( !x_Equals(*e2, e1) ) {
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
