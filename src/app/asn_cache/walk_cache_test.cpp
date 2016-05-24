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
 * Authors:  Mike Diccucio Cheinan Marks
 *
 * File Description:
 * Program to walk the chunks in an ASN cache and test the magic number.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <serial/serial.hpp>
#include <serial/objistrasnb.hpp>

#include <objtools/data_loaders/asn_cache/asn_cache.hpp>
#include <objtools/data_loaders/asn_cache/chunk_file.hpp>
#include <objtools/data_loaders/asn_cache/Cache_blob.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//  CAsnCacheApplication::


class CAsnCacheApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CAsnCacheApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddKey("cache", "ASNCache",
                     "Path to ASN.1 cache",
                     CArgDescriptions::eInputFile);

    arg_desc->AddKey("chunk", "ChunkToWalk",
                        "The ID number of the chunk to be tested",
                        CArgDescriptions::eInteger);

    arg_desc->AddDefaultKey("n", "EntryNumber",
                             "Number of entries to read",
			     CArgDescriptions::eInteger, "-1");
 

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "File to place ASN seq-entries in",
                            CArgDescriptions::eOutputFile,
                            "-");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



int CAsnCacheApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();
    Int4   chunk_id = args["chunk"].AsInteger();
    Int4   num_to_read = args["n"].AsInteger();
    CNcbiOstream &  output_stream = args["o"].AsOutputFile();

    CStopWatch sw;
    sw.Start();

    std::string cache_path( args["cache"].AsString() );
    CAsnCache cache( cache_path );

    std::string chunk_file_name( chunk_id > 0
                                 ? CChunkFile::s_MakeChunkFileName( cache_path, chunk_id ) 
                                 : NASNCacheFileName::GetSeqIdChunk( cache_path ));
    LOG_POST( Info << "Walking chunk file " << chunk_file_name );
    CNcbiIfstream   chunk_stream( chunk_file_name.c_str() );
    CObjectIStreamAsnBinary chunk_object_stream( chunk_stream );
    
    Uint4   count = 0;

    CCache_blob  the_blob;
    CSeq_id id;
    try {
        while ( num_to_read < 0 || count < num_to_read )
          if(chunk_id > 0){
            chunk_object_stream >> the_blob;
            
            output_stream << "Blob " << count << " at offset "
                << chunk_object_stream.GetStreamPos() << '\n';
            if ( CCache_blob::kMagicNum != the_blob.GetMagic() ) {
                LOG_POST( Error << "Blob number " << count << " has a bad magic number of 0x"
                            << std::hex << the_blob.GetMagic() );
            }
            
            count++;
          } else {
            chunk_object_stream >> id;
            output_stream << id.AsFastaString() << endl;
          }
    } catch( CEofException & ) {
        // Ignore this -- we reached the end of the file.
    } catch (...) {
        LOG_POST( Error << "Object stream exception on blob number " << count );
        throw;
    }

    double elapsed_time = sw.Elapsed();
    LOG_POST( Info << "Done walking " << count << " blobs in " << elapsed_time << " seconds." );

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CAsnCacheApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CAsnCacheApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
