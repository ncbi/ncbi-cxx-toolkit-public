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
 *   Sample test application for VDB graph reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <sra/readers/sra/graphread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>

#include <klib/rc.h>
#include <klib/writer.h>
#include <align/align-access.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>

#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasnb.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//////////////////////////////////////////////////////////////////////////////
//  CTestApp::


class CTestApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


//////////////////////////////////////////////////////////////////////////////
//  Init test

//#define DEFAULT_FILE NCBI_TRACES04_PATH "/wgs01/NANNOT/GCF_000002125.1_ERS025083_filtered_exons_log"
//#define DEFAULT_FILE NCBI_TRACES04_PATH "/nannot01/000/008/NA000008777.1"
#define DEFAULT_FILE "NA000008777.1"

void CTestApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "graph_test");

    arg_desc->AddOptionalKey("file", "File",
                             "cSRA file name",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("seq_table", "Print list of sequences with graphs");
    arg_desc->AddOptionalKey("q", "Query",
                             "Query coordinates in form chr1:100-10000",
                             CArgDescriptions::eString);

    arg_desc->AddDefaultKey("limit_count", "LimitCount",
                            "Number of cSRA entries to read (0 - unlimited)",
                            CArgDescriptions::eInteger,
                            "100000");
    arg_desc->AddFlag("verbose", "Print verbose info");
    arg_desc->AddFlag("print_objects", "Print generated objects");
    arg_desc->AddFlag("ignore_errors", "Ignore errors in individual entries");

    arg_desc->AddFlag("overview_graphs", "Generate overview graphs");
    arg_desc->AddFlag("mid_graphs", "Generate mid-zoom graphs");
    arg_desc->AddFlag("main_graph", "Generate main graph");
    arg_desc->AddFlag("main_as_table", "Generate main graph as Seq-table");
    arg_desc->AddFlag("main_as_best", "Use smaller of Seq-table or Seq-graph");

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

int CTestApp::Run(void)
{
    const CArgs& args = GetArgs();

    bool verbose = args["verbose"];
    bool print_objects = args["print_objects"];

    CVDBMgr mgr;

    string path = DEFAULT_FILE;
    if ( args["file"] ) {
        path = args["file"].AsString();
    }

    CNcbiOstream& out = cout;
    
    CStopWatch sw;
        
    sw.Restart();
    if ( verbose ) {
        try {
            string resolved = mgr.FindAccPath(path);
            out << "Resolved "<<path<<" -> "<<resolved
                << NcbiEndl;
        }
        catch ( CException& exc ) {
            ERR_POST("FindAccPath failed: "<<exc);
        }
        out << "Opening VDB graph "<< path
            << NcbiEndl;
    }
    CVDBGraphDb db(mgr, path);
    out << "Opened VDB in "<<sw.Elapsed()
        << NcbiEndl;
    sw.Restart();
    
    if ( args["seq_table"] ) {
        for ( CVDBGraphSeqIterator it(db); it; ++it ) {
            out << it->m_SeqId
                << " len: "<<it.GetSeqLength()
                << " @(" << it->m_RowFirst << "," << it->m_RowLast << ")"
                << NcbiEndl;
        }
        out << "Scanned seq table in "<<sw.Elapsed()
            << NcbiEndl;
        sw.Restart();
    }
    
    string query_id;
    CRange<TSeqPos> query_range = CRange<TSeqPos>::GetWhole();

    if ( args["q"] ) {
        string q = args["q"].AsString();
        SIZE_TYPE colon_pos = q.find(':');
        if ( q.empty() || q == "." ) {
            CVDBGraphSeqIterator it(db);
            if ( it ) {
                query_id = it.GetSeqId();
                out << "Querying first sequence: " << query_id
                    << NcbiEndl;
            }
            else {
                out << "There are no sequences in the VDB"
                    << NcbiEndl;
            }
        }
        else if ( colon_pos == NPOS || colon_pos == 0 ) {
            query_id = q;
        }
        else {
            query_id = q.substr(0, colon_pos);
            SIZE_TYPE dash_pos = q.find('-', colon_pos+1);
            if ( dash_pos == NPOS ) {
                ERR_POST(Fatal << "Invalid query format: " << q);
            }
            TSeqPos from =
                NStr::StringToNumeric<TSeqPos>(q.substr(colon_pos+1,
                                                        dash_pos-colon_pos-1));
            TSeqPos to = NStr::StringToNumeric<TSeqPos>(q.substr(dash_pos+1));
            query_range.SetFrom(from).SetTo(to);
        }
    }

    CSeq_id_Handle query_idh;
    if ( !query_id.empty() ) {
        query_idh = CSeq_id_Handle::GetHandle(query_id);
        sw.Restart();

        string name = CFile(path).GetName();
        CVDBGraphSeqIterator it(db, query_idh);

        if ( !it ) {
            out << "No graph found in "<<sw.Elapsed()
                << NcbiEndl;
        }
        else {
            bool main_graph = args["main_graph"];
            bool mid_graphs = args["mid_graphs"];
            bool overview_graphs =
                !(main_graph || mid_graphs) || args["overview_graphs"];
            if ( overview_graphs ) {
                CRef<CSeq_annot> annot =
                    it.GetAnnot(query_range, name,
                                CVDBGraphSeqIterator::fGraphQAll);
                if ( annot && print_objects ) {
                    out << "Overview graphs: " << MSerial_AsnText << *annot;
                }
            }
            if ( mid_graphs ) {
                CRef<CSeq_annot> annot =
                    it.GetAnnot(query_range, name,
                                CVDBGraphSeqIterator::fGraphZoomQAll);
                if ( annot && print_objects ) {
                    out << "Zoom graphs: " << MSerial_AsnText << *annot;
                }
            }
            if ( main_graph ) {
                CVDBGraphSeqIterator::TContentFlags flags = 
                    CVDBGraphSeqIterator::fGraphMain;
                if ( args["main_as_table"] ) {
                    flags |= CVDBGraphSeqIterator::fGraphMainAsTable;
                }
                if ( args["main_as_best"] ) {
                    flags |= CVDBGraphSeqIterator::fGraphMainAsBest;
                }
                CRef<CSeq_annot> annot = it.GetAnnot(query_range, name, flags);
                if ( annot && print_objects ) {
                    out << "Main graph: " << MSerial_AsnText << *annot;
                }
            }
            out << "Loaded graph in "<<sw.Elapsed()
                << NcbiEndl;
        }
        sw.Restart();
    }

    out << "Success." << NcbiEndl;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CTestApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTestApp().AppMain(argc, argv);
}
