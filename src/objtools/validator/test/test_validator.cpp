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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko
 *
 * File Description:
 *   Test Program for validator
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/validator/validator.hpp>

// Object Manager includes
#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/objmgr/desc_ci.hpp>
#include <objects/objmgr/feat_ci.hpp>
#include <objects/objmgr/align_ci.hpp>
#include <objects/objmgr/graph_ci.hpp>
#include <objects/objmgr/gbloader.hpp>
#include <objects/objmgr/reader_id1.hpp>


using namespace ncbi;
using namespace objects;
using namespace validator;


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//
template<class T>
void display_object(const T& obj)
{
    auto_ptr<CObjectOStream> os (CObjectOStream::Open(eSerial_AsnText, cout));
    *os << obj;
    cout << endl;
}


class CTest_validatorApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CTest_validatorApplication::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    
    arg_desc->AddDefaultKey
        ("i", "ASNFile", "Seq-entry/Seq_submit ASN.1 text file",
        CArgDescriptions::eString, "current.prt");
    
    arg_desc->AddDefaultKey(
        "o", "UseID", "If true, registers ID loader",
        CArgDescriptions::eBoolean, "false");
    
    arg_desc->AddDefaultKey(
        "a", "CheckAlign", "Validate Alignments",
        CArgDescriptions::eBoolean, "false");
    
    arg_desc->AddDefaultKey(
        "n", "NonAsciiError", "Report Non Ascii Error",
        CArgDescriptions::eBoolean, "false");
    
    arg_desc->AddDefaultKey(
        "s", "SpliceAsError", "Splice error as error, else warning",
        CArgDescriptions::eBoolean, "false");
    
    arg_desc->AddDefaultKey(
        "c", "SuppressContext", "Suppress context in error msgs",
        CArgDescriptions::eBoolean, "false");
    
    arg_desc->AddDefaultKey
        ("e", "SeqEntry", "Input is Seq-entry [T/F]",
        CArgDescriptions::eBoolean, "false");
    
    arg_desc->AddDefaultKey
        ("b", "SeqSubmit", "Input is Seq-submit [T/F]",
        CArgDescriptions::eBoolean, "false");

    // Program description
    string prog_description = "Test driver for Validate()\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


int CTest_validatorApplication::Run(void)
{
    // Setup application registry and logs for CONNECT library
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

    // Process command line args:  get CSeq_entry
    const CArgs& args = GetArgs();
    string fname = args["i"].AsString();

    // Create object manager
    // Use CRef<> here to automatically delete the OM on exit.
    CRef<CObjectManager> obj_mgr(new CObjectManager);

    if (args["o"].AsBoolean()) {
        // Create GenBank data loader and register it with the OM.
        // The last argument "eDefault" informs the OM that the loader must
        // be included in scopes during the CScope::AddDefaults() call.
        obj_mgr->RegisterDataLoader
            (*new CGBDataLoader("ID", new CId1Reader, 2),
            CObjectManager::eDefault);
    }

    // Set validator options
    unsigned int options = 0;
    options |= args["n"].AsBoolean() ? CValidError::eVal_non_ascii : 0;
    options |= args["s"].AsBoolean() ? CValidError::eVal_splice_err : 0;
    options |= args["a"].AsBoolean() ? CValidError::eVal_val_align : 0;
    options |= args["c"].AsBoolean() ? CValidError::eVal_no_context : 0;

    auto_ptr<CObjectIStream> in(CObjectIStream::Open(fname, eSerial_AsnText));
    auto_ptr<CValidError> eval;

    if ( args["e"].AsBoolean() ) {
        // Create CSeq_entry to be validated from file
        CRef<CSeq_entry> se(new CSeq_entry);
        
        // Get seq-entry to validate
        in->Read(ObjectInfo(*se));
        
        // Validae Seq-entry
        eval.reset(new CValidError(*obj_mgr, *se, options));
    } else if ( args["b"].AsBoolean() ) {
        
        // Create CSeq_submit to be validated from file
        CRef<CSeq_submit> ss(new CSeq_submit);
        
        // Get seq-entry to validate
        in->Read(ObjectInfo(*ss));
        
        // Validae Seq-entry
        eval.reset(new CValidError(*obj_mgr, *ss, options));
    }
    
    // Display error messages
    if ( eval.get() != 0 ) {
        for (CValidError_CI vit(*eval); vit; ++vit) {
            cout << "Error code: " << vit->GetErrCode() << endl << endl;
            cout << "Message: " << vit->GetMsg() << endl << endl;
            cout << "Verbose: " << vit->GetVerbose() << endl << endl;
        }
    }

    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CTest_validatorApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}




/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.5  2003/01/28 16:01:22  shomrat
 * Bug fixes
 *
 * Revision 1.4  2003/01/24 22:04:33  shomrat
 * Added flags to specify input format (Seq-entry or Seq-submit)
 *
 * Revision 1.3  2003/01/07 20:04:26  shomrat
 * GetMessage changed to GetMsg
 *
 * Revision 1.2  2003/01/07 17:57:05  ucko
 * Use the new path to the validator's public header.
 *
 * Revision 1.1  2002/12/23 21:11:59  shomrat
 * Moved from objects/objmgr/test
 *
 * Revision 1.3  2002/11/04 21:29:14  grichenk
 * Fixed usage of const CRef<> and CRef<> constructor
 *
 * Revision 1.2  2002/10/08 13:40:38  clausen
 * Changed current.asn to current.prt
 *
 * Revision 1.1  2002/10/03 18:34:13  clausen
 * Initial version
 *
 *
 * ===========================================================================
 */
