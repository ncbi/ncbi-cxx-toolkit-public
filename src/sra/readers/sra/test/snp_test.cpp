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
 *   Sample test application for SNP reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <sra/readers/sra/snpread.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqres/seqres__.hpp>

#include <serial/serial.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CSNPTestApp::


class CSNPTestApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test

void CSNPTestApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "snp_test");

    arg_desc->AddKey("file", "File",
                     "SNP file name, accession, or prefix",
                     CArgDescriptions::eString);

    arg_desc->AddFlag("refseq_table", "Dump RefSeq table");

    arg_desc->AddOptionalKey("q", "Query",
                             "Query coordinates in form chr1:100-10000",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("refseq", "RefSeqId",
                             "RefSeq id",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("refpos", "RefSeqPos",
                            "RefSeq position",
                            CArgDescriptions::eInteger,
                            "0");
    arg_desc->AddOptionalKey("refposs", "RefSeqPoss",
                            "RefSeq positions",
                            CArgDescriptions::eString);
    arg_desc->AddDefaultKey("refwindow", "RefSeqWindow",
                            "RefSeq window",
                            CArgDescriptions::eInteger,
                            "0");
    arg_desc->AddOptionalKey("refend", "RefSeqEnd",
                            "RefSeq end position",
                            CArgDescriptions::eInteger);

    arg_desc->AddDefaultKey("limit_count", "LimitCount",
                            "Number of entries to read (0 - unlimited)",
                            CArgDescriptions::eInteger,
                            "100");

    arg_desc->AddFlag("verbose", "Print info about found data");

    arg_desc->AddFlag("make_feat", "Make feature object");
    arg_desc->AddFlag("no_shared_objects", "Do not share created objects");
    arg_desc->AddFlag("print_objects", "Print generated objects");

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output file of ASN.1",
                            CArgDescriptions::eOutputFile,
                            "-");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////


int CSNPTestApp::Run(void)
{
    uint64_t error_count = 0;
    const CArgs& args = GetArgs();

    string path = args["file"].AsString();
    bool verbose = args["verbose"];
    bool make_feat = args["make_feat"];
    bool print = args["print_objects"];
    size_t limit_count = args["limit_count"].AsInteger();

    CNcbiOstream& out = args["o"].AsOutputFile();

    string query_id;
    CRange<TSeqPos> query_range = CRange<TSeqPos>::GetWhole();
    CSeq_id_Handle query_idh;
    
    if ( args["refseq"] ) {
        query_id = args["refseq"].AsString();
        query_range.SetFrom(args["refpos"].AsInteger());
        if ( args["refwindow"] ) {
            TSeqPos window = args["refwindow"].AsInteger();
            if ( window != 0 ) {
                query_range.SetLength(window);
            }
            else {
                query_range.SetTo(kInvalidSeqPos);
            }
        }
        if ( args["refend"] ) {
            query_range.SetTo(args["refend"].AsInteger());
        }
    }
    if ( args["q"] ) {
        string q = args["q"].AsString();
        SIZE_TYPE colon_pos = q.find(':');
        if ( colon_pos == 0 ) {
            ERR_POST(Fatal << "Invalid query format: " << q);
        }
        if ( colon_pos == NPOS ) {
            query_id = q;
            query_range = query_range.GetWhole();
        }
        else {
            query_id = q.substr(0, colon_pos);
            SIZE_TYPE dash_pos = q.find('-', colon_pos+1);
            if ( dash_pos == NPOS ) {
                ERR_POST(Fatal << "Invalid query format: " << q);
            }
            string q_from = q.substr(colon_pos+1, dash_pos-colon_pos-1);
            string q_to = q.substr(dash_pos+1);
            TSeqPos from, to;
            if ( q_from.empty() ) {
                from = 0;
            }
            else {
                from = NStr::StringToNumeric<TSeqPos>(q_from);
            }
            if ( q_to.empty() ) {
                to = kInvalidSeqPos;
            }
            else {
                to = NStr::StringToNumeric<TSeqPos>(q_to);
            }
            query_range.SetFrom(from).SetTo(to);
        }
    }
    if ( !query_id.empty() ) {
        query_idh = CSeq_id_Handle::GetHandle(query_id);
    }

    CVDBMgr mgr;
    CStopWatch sw;
    
    sw.Restart();

    CSNPDb snp_db(mgr, path);
    if ( verbose ) {
        out << "Opened SNP in "<<sw.Restart()
            << NcbiEndl;
    }
    
    if ( args["refseq_table"] ) {
        sw.Restart();
        for ( CSNPDbRefSeqIterator it(snp_db); it; ++it ) {
            out << it->m_Name << " " << it->m_SeqId
                << " len: "<<it.GetSeqLength()
                << " @(" << it->m_RowFirst << "," << it->m_RowLast << ")"
                << NcbiEndl;
        }
        out << "Scanned reftable in "<<sw.Elapsed()
            << NcbiEndl;
        sw.Restart();
    }
    
    if ( query_idh ) {
        sw.Restart();
        
        size_t count = 0;
        CSNPDbIterator::TFlags flags = CSNPDbIterator::fDefaultFlags;
        if ( args["no_shared_objects"] ) {
            flags &= ~CSNPDbIterator::fUseSharedObjects;
        }
        for ( CSNPDbIterator it(snp_db, query_idh, query_range); it; ++it ) {
            if ( verbose ) {
                out << it.GetAccession();
                out << " pos: "<<it.GetSNPPosition();
                out << " len: "<<it.GetSNPLength();
                out << '\n';
            }
            if ( make_feat ) {
                CRef<CSeq_feat> feat = it.GetSeq_feat(flags);
                if ( print ) {
                    out << MSerial_AsnText << *feat;
                }
            }
            if ( limit_count && ++count >= limit_count ) {
                break;
            }
        }

        out << "Found "<<count<<" SNPs in "<<sw.Elapsed()
            << NcbiEndl;
        sw.Restart();
    }

    out << "Success." << NcbiEndl;
    return error_count? 1: 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CSNPTestApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CSNPTestApp().AppMain(argc, argv);
}
