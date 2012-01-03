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
* Author:  Frank Ludwig, Mati Shomrat, NCBI
*
* File Description:
*   
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objects/seqset/gb_release_file.hpp>

#include <objects/entrez2/Entrez2_boolean_element.hpp>
#include <objects/entrez2/Entrez2_boolean_reply.hpp>
#include <objects/entrez2/Entrez2_boolean_exp.hpp>
#include <objects/entrez2/Entrez2_eval_boolean.hpp>
#include <objects/entrez2/Entrez2_id_list.hpp>
#include <objects/entrez2/entrez2_client.hpp>

#include <objtools/cleanup/cleanup.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/idmapper.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);          

//  ============================================================================
class CIdMapperApp : 
//  ============================================================================
    public CNcbiApplication
{
public:
    void Init();
    int Run();

protected:
    CIdMapper*
    GetMapper();
    
    void
    GetSourceIds(
        std::vector< std::string >& );

    CSeq_id_Handle
    GetSourceHandle(
        const std::string& );
        
    string
    GetTargetString(
        CSeq_id_Handle );
                
private:
    CNcbiIstream* m_Input;
    CNcbiOstream* m_Output;
};

//  ============================================================================
void CIdMapperApp::Init()
//  ============================================================================
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Convert foreign ids to GI numbers",
        false);

    arg_desc->AddFlag(
        "reverse",
        "Map in opposite direction",
        true );
            
    arg_desc->AddDefaultKey( "genome", 
        "genome",
        "Source build or genome number",
        CArgDescriptions::eString, 
        ""); 

    arg_desc->AddOptionalKey( "mapfile",
        "mapfile",
        "Mapper configuration file",
        CArgDescriptions::eInputFile );
        
    arg_desc->AddDefaultKey( "mapdb",
        "mapdb",
        "Mapper database in the form \"host:database\"",
        CArgDescriptions::eString,
        "" );
        
    arg_desc->AddExtra( 1, 100, "ids to look up", CArgDescriptions::eString );
    
    SetupArgDescriptions(arg_desc.release());
}


//  ============================================================================
int CIdMapperApp::Run()
//  ============================================================================
{
    CIdMapper* pMapper = GetMapper();
    vector< string > SourceIds;
    GetSourceIds( SourceIds );

    vector<string>::iterator it;
    for ( it = SourceIds.begin(); it != SourceIds.end(); ++it ) {
        string strSource = *it;
        CSeq_id_Handle hSource = GetSourceHandle( strSource );
        CSeq_id_Handle hTarget = pMapper->Map( hSource );
        string strTarget = GetTargetString( hTarget );
        
        cout << strSource << " :  " << strTarget << endl;
    }    
    delete pMapper;
        
    return 0;
}

//  ============================================================================
CIdMapper*
CIdMapperApp::GetMapper()
//  ============================================================================
{
    string strGenome = GetArgs()[ "genome" ].AsString();
    string strMapFile = GetArgs()[ "mapfile" ].AsString();
    string strMapDb = GetArgs()[ "mapdb" ].AsString();
    bool bReverse = GetArgs()[ "reverse" ];
    
    if ( strGenome.empty() && strMapFile.empty() && strMapDb.empty() ) {
        return new CIdMapper( strGenome, bReverse );
    }
    if ( !strMapFile.empty() ) {
        CNcbiIfstream istr( strMapFile.c_str() );
        return new CIdMapperConfig( istr, strGenome, bReverse );
    }
    if ( !strMapDb.empty() ) {
        string strHost;
        string strDatabase;
        NStr::SplitInTwo( strMapDb, ":", strHost, strDatabase );
        return new CIdMapperDatabase( strHost, strDatabase, strGenome, bReverse );
    }
    return new CIdMapperBuiltin( strGenome, bReverse );
};

//  ============================================================================
void
CIdMapperApp::GetSourceIds(
    vector< string >& Ids )
//  ============================================================================
{
    const CArgs& args = GetArgs();
    for ( unsigned int u = 1; u <= args.GetNExtra(); ++u ) {
        Ids.push_back( args[ u ].AsString() );
    }
};

//  ============================================================================
CSeq_id_Handle
CIdMapperApp::GetSourceHandle(
    const string& strSource )
//  ============================================================================
{
    int iGi = NStr::StringToInt( strSource, NStr::fConvErr_NoThrow );
    if ( iGi ) {
        return CSeq_id_Handle::GetGiHandle( iGi );
    }
        
    CSeq_id source( CSeq_id::e_Local, strSource );
    return CSeq_id_Handle::GetHandle( source );
};

//  ============================================================================
string
CIdMapperApp::GetTargetString(
    CSeq_id_Handle hTarget )
//  ============================================================================
{
    if ( !hTarget ) {
        return "<unknown>";
    }
    return hTarget.AsString();
};

/////////////////////////////////////////////////////////////////////////////
//
// Main

int main(int argc, const char** argv)
{
    return CIdMapperApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
