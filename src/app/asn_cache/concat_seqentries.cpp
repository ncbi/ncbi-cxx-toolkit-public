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
 * Concatenate all the seq entry blobs in an ASN cache.  No indexing is done.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistre.hpp>

#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>

#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasnb.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objtools/data_loaders/asn_cache/asn_cache.hpp>
#include <objtools/data_loaders/asn_cache/chunk_file.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//  CConcatSeqEntriesApplication::


class CConcatSeqEntriesApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


void    DumpSeqEntries( CDir & cache_path, CNcbiOstream & output_stream );

/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CConcatSeqEntriesApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Concatenate all the seq entries from a cache.");

    arg_desc->AddKey("cache", "Cache",
                     "Path to ASN.1 cache",
                     CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey( "o", "SeqEntryFile",
                            "Write the seq entries here.",
                            CArgDescriptions::eOutputFile,
                            "-");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



int CConcatSeqEntriesApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    CStopWatch sw;
    sw.Start();

    CDir    cache_dir( args["cache"].AsString() );
    if (! cache_dir.Exists() ) {
        LOG_POST( Error << cache_dir.GetPath() << " does not exist!" );
        return 1;
    } else if ( ! cache_dir.IsDir() ) {
        LOG_POST( Error << cache_dir.GetPath() << " does not point to a "
                    << "valid cache path!" );
        return 2;
    }
    
    DumpSeqEntries( cache_dir, args["o"].AsOutputFile() );

    return 0;
}


void    DumpSeqEntries( CDir & cache_dir, CNcbiOstream & output_stream )
{
    CCache_blob a_blob;
    
    CDir::TEntries  chunk_list
        = cache_dir.GetEntries( NASNCacheFileName::GetChunkPrefix() + "*", 
                                   CDir::eIgnoreRecursive | CDir::fCreateObjects );
    size_t  chunk_count = 0;
    size_t  seq_entry_count = 0;
    ITERATE( CDir::TEntries, chunk_file_iter, chunk_list ) {
        CNcbiIfstream   chunk_stream( (*chunk_file_iter)->GetPath().c_str() );
        CObjectIStreamAsnBinary blob_asn_stream( chunk_stream );
        
        try {
            while ( true ) {
                blob_asn_stream >> a_blob;
                
                CCompressionOStream zip(output_stream,
                        new CZipStreamCompressor(CZipCompression::fGZip),
                        CCompressionStream::fOwnProcessor);

                CObjectOStreamAsnBinary output_asn_stream( zip );
                CSeq_entry  a_seq_entry;
                a_blob.UnPack( a_seq_entry );
                output_asn_stream << a_seq_entry;
                seq_entry_count++;
            }
        } catch( CEofException & ) {
            // Ignore this -- we reached the end of the file.
        } catch (...) {
            LOG_POST( Error << "Object stream exception on chunk number " << chunk_count );
            throw;
        }
            
        chunk_count++;
        LOG_POST( Info << "Finished processing " << (*chunk_file_iter)->GetPath() );
    }
    
    LOG_POST( Info << seq_entry_count << " seq entries from "
                << chunk_count << " chunks written." );
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CConcatSeqEntriesApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CConcatSeqEntriesApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
