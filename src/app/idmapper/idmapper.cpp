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

#include <objtools/idmapper/ucscid.hpp>
#include <objtools/idmapper/idmapper.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);          

//  ============================================================================
class CIdMapperApp : 
//  ============================================================================
    public CNcbiApplication
{
public:
    void Init();
    int Run();

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
    
    arg_desc->AddDefaultKey( "t", 
        "mapsource",
        "Source for mappings",
        CArgDescriptions::eString, ""); 

    arg_desc->AddOptionalKey( "umap",
        "usermap",
        "Source for user defined mappings",
        CArgDescriptions::eInputFile );
        
    arg_desc->AddOptionalKey( "db",
        "usermap",
        "Source for database provided mappings",
        CArgDescriptions::eString );
        
    arg_desc->AddExtra( 1, 100, "ids to look up", CArgDescriptions::eString );
    
    SetupArgDescriptions(arg_desc.release());
}


//  ============================================================================
int CIdMapperApp::Run()
//  ============================================================================
{
    const CArgs& args = GetArgs();
    
    CIdMapper* pIdMapper = CIdMapper::GetIdMapper( args );
    pIdMapper->Dump( cout );
    cout << endl;    
    
    for ( unsigned int u = 1; u <= args.GetNExtra(); ++u ) {
        string strKey = UcscID::Label( "", args[ u ].AsString() );
        unsigned int uLength;
        CSeq_id_Handle idh = pIdMapper->MapID( strKey, uLength );
        cout << strKey << " ===> " << idh.GetSeqId()->AsFastaString() << endl;
    }
    delete pIdMapper;
        
    return 0;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//
// Main

int main(int argc, const char** argv)
{
    return CIdMapperApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
