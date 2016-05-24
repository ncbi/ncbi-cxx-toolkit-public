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
 * Authors:  Cheinan Marks
 *
 * File Description:
 * Measure the read speed through a BDB Asn Index.
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbitime.hpp>

#include <db/bdb/bdb_cursor.hpp>

#include <objtools/data_loaders/asn_cache/asn_index.hpp>
#include <objtools/data_loaders/asn_cache/file_names.hpp>

USING_NCBI_SCOPE;




/////////////////////////////////////////////////////////////////////////////
//  CReadIndexSpeedApp


class CReadIndexSpeedApp : public CNcbiApplication
{
public:
    CReadIndexSpeedApp() : m_AsnIndex(CAsnIndex::e_main) {}

private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
    
    CAsnIndex   m_AsnIndex;
    
    void    x_WalkIndex( bool noMultiFetch, bool noGetData, bool prereadIndex );
    void    x_PreReadIndex();
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CReadIndexSpeedApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Update an ASN cache from ID dump updates.");

    arg_desc->AddKey("index", "Index",
                     "Directory path for the ASN index to be measured.",
                     CArgDescriptions::eInputFile);

    arg_desc->AddFlag("nomf", "Use cursor multifetch", false );
    arg_desc->AddFlag("nodata", "Get index data", false );
    arg_desc->AddFlag("nopreread", "Do not preread the dump index "
                        "(Use if the ID dump is not on panfs).", false );

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



int CReadIndexSpeedApp::Run(void)
{
    CFileAPI::SetLogging( eOn );
    const CArgs& args = GetArgs();

    CDir    asn_index_dir( args["index"].AsString() );
    if (! asn_index_dir.Exists() ) {
        LOG_POST( Error << "ASN index directory " << asn_index_dir.GetPath() << " does not exist!" );
        return 1;
    } else if ( ! asn_index_dir.IsDir() ) {
        LOG_POST( Error << asn_index_dir.GetPath() << " does not point to a "
                    << "valid ASN index directory!" );
        return 2;
    }

    m_AsnIndex.SetCacheSize( 1 * 1024 * 1024 * 1024 );
    m_AsnIndex.Open( NASNCacheFileName::GetBDBIndex( asn_index_dir.GetPath(), CAsnIndex::e_main),
                        CBDB_RawFile::eReadOnly );
    
    bool    useMultiFetch = args["nomf"];
    bool    getData = args["nodata"];
    bool    prereadIndex = args["nopreread"];
    
    x_WalkIndex( useMultiFetch, getData, prereadIndex );
    
    return 0;
}


void    CReadIndexSpeedApp::x_WalkIndex( bool useMultiFetch, bool getData, bool prereadIndex )
{
    CStopWatch  sw( CStopWatch::eStart );
    
    if ( prereadIndex ) {
        LOG_POST( Info << "Preread activated" );
        x_PreReadIndex();
    }
    
    CBDB_FileCursor a_cursor( m_AsnIndex );
    if ( useMultiFetch ) {
        a_cursor.InitMultiFetch( 1 * 1024 * 1024 * 1024 );
        LOG_POST( Info << "MultiFetch activated" );
    }
    a_cursor.SetCondition( CBDB_FileCursor::eFirst, CBDB_FileCursor::eLast);
    
    if ( getData ) LOG_POST( Info << "Get data activated." );
    
    Uint4   entry_count = 0;
    while (a_cursor.Fetch() == eBDB_Ok) {
        entry_count++;
        if ( getData ) {
            volatile CAsnIndex::TSeqId       theSeqId = m_AsnIndex.GetSeqId();
            volatile CAsnIndex::TVersion     aVersion = m_AsnIndex.GetVersion();
            volatile CAsnIndex::TGi          theGi = m_AsnIndex.GetGi();
            volatile CAsnIndex::TTimestamp   theTimestamp = m_AsnIndex.GetTimestamp();
            volatile CAsnIndex::TChunkId     theChunkId = m_AsnIndex.GetChunkId();
            volatile CAsnIndex::TOffset      theOffset = m_AsnIndex.GetOffset();
            volatile CAsnIndex::TSize        theSize = m_AsnIndex.GetSize();
            volatile CAsnIndex::TSeqLength   theSeqLength = m_AsnIndex.GetSeqLength();
            volatile CAsnIndex::TTaxId       theTaxId = m_AsnIndex.GetTaxId();
        }
        
    entry_count++;
    }
    
    LOG_POST( Info << "Read " << entry_count << " index entries in " << sw.Elapsed()
                << " seconds." );
}


void    CReadIndexSpeedApp::x_PreReadIndex()
{
    /// Read through the dump index file and throw away the data.  This optimizes
    /// performance when using panfs.
    CStopWatch  preread_sw( CStopWatch::eStart );
    CNcbiIfstream   dump_index_stream( m_AsnIndex.GetFileName().c_str(),
                                        std::ios::binary );
    const size_t    kBufferSize = 64 * 1024 * 1024;
    std::vector<char> buffer( kBufferSize );
    while ( dump_index_stream ) {
        dump_index_stream.read( &buffer[0], kBufferSize );
    }
    
    LOG_POST( Info << m_AsnIndex.GetFileName() << " preread in "
                << preread_sw.Elapsed() << " seconds." );
}




/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CReadIndexSpeedApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CReadIndexSpeedApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
