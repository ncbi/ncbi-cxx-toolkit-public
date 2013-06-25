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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Unit tests for VDB graph data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <sra/data_loaders/vdbgraph/vdbgraphloader.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_table_ci.hpp>
#include <objects/seqtable/seqtable__.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

static string s_LoaderName;

static CRef<CScope> s_MakeScope(const string& file_name = kEmptyStr)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    if ( !s_LoaderName.empty() ) {
        om->RevokeDataLoader(s_LoaderName);
        s_LoaderName.erase();
    }
    if ( file_name.empty() ) {
        s_LoaderName =
            CVDBGraphDataLoader::RegisterInObjectManager(*om)
            .GetLoader()->GetName();
    }
    else {
        s_LoaderName =
            CVDBGraphDataLoader::RegisterInObjectManager(*om, file_name)
            .GetLoader()->GetName();
    }
    CRef<CScope> scope(new CScope(*om));
    scope->AddDataLoader(s_LoaderName);
    return scope;
}


BOOST_AUTO_TEST_CASE(FetchSeq1)
{
    CRef<CScope> scope =
        s_MakeScope(NCBI_TRACES04_PATH "/nannot01/000/008/NA000008777.1");
    CSeq_id seqid1("NC_002333.2");
    
    BOOST_REQUIRE_EQUAL(kInvalidSeqPos, scope->GetSequenceLength(seqid1));
    
    CBioseq_Handle handle1 = scope->GetBioseqHandle(seqid1);
    BOOST_REQUIRE(!handle1);
    SAnnotSelector sel;
    sel.SetSearchUnresolved();
    CSeq_loc loc;
    loc.SetWhole(seqid1);
    CGraph_CI graph_it(*scope, loc, sel);
    BOOST_CHECK(graph_it.GetSize());
    if ( 0 ) {
        for ( ; graph_it; ++graph_it ) {
            NcbiCout << MSerial_AsnText << graph_it->GetOriginalGraph();
        }
    }
    if ( 1 ) {
        for ( ; graph_it; ++graph_it ) {
            const CSeq_graph& graph = graph_it->GetOriginalGraph();
            const CSeq_interval& interval = graph.GetLoc().GetInt();
            NcbiCout << "Graph "<<graph_it.GetAnnot().GetName()
                     << " " << interval.GetFrom()<<"-"<<interval.GetTo()
                     << NcbiEndl;
        }
    }
}


BOOST_AUTO_TEST_CASE(FetchSeq2)
{
    CRef<CScope> scope = s_MakeScope();
    CSeq_id seqid1("NC_002333.2");
    
    BOOST_REQUIRE_EQUAL(kInvalidSeqPos, scope->GetSequenceLength(seqid1));
    
    CBioseq_Handle handle1 = scope->GetBioseqHandle(seqid1);
    BOOST_REQUIRE(!handle1);
    SAnnotSelector sel;
    sel.SetSearchUnresolved();
    sel.IncludeNamedAnnotAccession("NA000008777.1");
    CSeq_loc loc;
    loc.SetWhole(seqid1);
    CGraph_CI graph_it(*scope, loc, sel);
    CSeq_table_CI table_it(*scope, loc, sel);
    BOOST_CHECK(graph_it.GetSize()||table_it.GetSize());
    if ( 0 ) {
        for ( ; graph_it; ++graph_it ) {
            NcbiCout << MSerial_AsnText << graph_it->GetOriginalGraph();
        }
    }
    if ( 1 ) {
        for ( ; graph_it; ++graph_it ) {
            const CSeq_graph& graph = graph_it->GetOriginalGraph();
            const CSeq_interval& interval = graph.GetLoc().GetInt();
            NcbiCout << "Graph "<<graph_it.GetAnnot().GetName()
                     << " " << interval.GetFrom()<<"-"<<interval.GetTo()
                     << NcbiEndl;
        }
        for ( ; table_it; ++table_it ) {
            CSeq_annot_Handle annot = table_it.GetAnnot();
            const CSeq_table& table =
                annot.GetCompleteObject()->GetData().GetSeq_table();
            const CSeqTable_column& col =
                table.GetColumn("Seq-table location");
            CConstRef<CSeq_loc> loc = col.GetSeq_loc(0);
            const CSeq_interval& interval = loc->GetInt();
            NcbiCout << "Table "<<annot.GetName()
                     << " " << interval.GetFrom()<<"-"<<interval.GetTo()
                     << NcbiEndl;
        }
    }
}

BOOST_AUTO_TEST_CASE(FetchSeq3)
{
    CRef<CScope> scope0 = s_MakeScope();
    CSeq_id seqid1("NC_002333.2");
    
    for ( int t = 0; t < 4; ++t ) {
        CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
        scope->AddDataLoader(s_LoaderName);

        BOOST_REQUIRE_EQUAL(kInvalidSeqPos, scope->GetSequenceLength(seqid1));
        
        CBioseq_Handle handle1 = scope->GetBioseqHandle(seqid1);
        BOOST_REQUIRE(!handle1);
        SAnnotSelector sel;
        sel.SetSearchUnresolved();
        int first_na = 8777, last_na = t < 2? 8781: 8788;
        for ( int na = first_na; na <= last_na; ++na ) {
            char buf[99];
            sprintf(buf, "NA%09d.1", na);
            sel.IncludeNamedAnnotAccession(buf);
        }
        CSeq_loc loc;
        loc.SetWhole(seqid1);
        CGraph_CI graph_it(*scope, loc, sel);
        CSeq_table_CI table_it(*scope, loc, sel);
        BOOST_CHECK(graph_it.GetSize()||table_it.GetSize());
        if ( 0 ) {
            for ( ; graph_it; ++graph_it ) {
                NcbiCout << MSerial_AsnText << graph_it->GetOriginalGraph();
            }
        }
        if ( 1 ) {
            for ( ; graph_it; ++graph_it ) {
                const CSeq_graph& graph = graph_it->GetOriginalGraph();
                const CSeq_interval& interval = graph.GetLoc().GetInt();
                NcbiCout << "Graph "<<graph_it.GetAnnot().GetName()
                         << " " << interval.GetFrom()<<"-"<<interval.GetTo()
                         << NcbiEndl;
            }
            for ( ; table_it; ++table_it ) {
                CSeq_annot_Handle annot = table_it.GetAnnot();
                const CSeq_table& table =
                    annot.GetCompleteObject()->GetData().GetSeq_table();
                const CSeqTable_column& col =
                    table.GetColumn("Seq-table location");
                CConstRef<CSeq_loc> loc = col.GetSeq_loc(0);
                const CSeq_interval& interval = loc->GetInt();
                NcbiCout << "Table "<<annot.GetName()
                         << " " << interval.GetFrom()<<"-"<<interval.GetTo()
                         << NcbiEndl;
            }
        }
    }
}

