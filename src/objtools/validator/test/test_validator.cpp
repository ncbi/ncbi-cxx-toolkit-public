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
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objectio.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/validator/validator.hpp>

#include <objects/seqset/Bioseq_set.hpp>

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
/*
template<class T>
void display_object(const T& obj)
{
    auto_ptr<CObjectOStream> os (CObjectOStream::Open(eSerial_AsnText, cout));
    *os << obj;
    cout << endl;
}
*/

class CTest_validatorApplication : public CNcbiApplication, CReadClassMemberHook
{
public:
    CTest_validatorApplication(void);

    virtual void Init(void);
    virtual int  Run (void);

    void ReadClassMember(CObjectIStream& in,
            const CObjectInfo::CMemberIterator& member);

private:

    void Setup(const CArgs& args);
    void SetupValidatorOptions(const CArgs& args);

    CObjectIStream* OpenFile(const CArgs& args);

    CValidError* ProcessSeqEntry(void);
    CValidError* ProcessSeqSubmit(void);
    void ProcessReleaseFile(const CArgs& args);
    CRef<CSeq_entry> ReadSeqEntry(void);
    unsigned int PrintValidError(const CValidError& errors, const CArgs& args);
    void PrintValidErrItem(const CValidErrItem& item, CNcbiOstream& os);


    CRef<CObjectManager> m_ObjMgr;
    auto_ptr<CObjectIStream> m_In;
    unsigned int m_Options;
    bool m_Continue;
};


CTest_validatorApplication::CTest_validatorApplication(void) :
    m_ObjMgr(0), m_In(0), m_Options(0), m_Continue(false)
{
}


void CTest_validatorApplication::Init(void)
{
    // Prepare command line descriptions

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    
    arg_desc->AddDefaultKey
        ("i", "ASNFile", "Seq-entry/Seq_submit ASN.1 text file",
        CArgDescriptions::eInputFile, "current.prt");
    

    arg_desc->AddFlag("s", "Input is Seq-submit");
    arg_desc->AddFlag("t", "Input is Seq-set (NCBI Release file)");
    arg_desc->AddFlag("b", "Input is in binary format");
    arg_desc->AddFlag("c", "Continue on ASN.1 error");

    arg_desc->AddFlag("g", "Registers ID loader");
    
    arg_desc->AddFlag("nonascii", "Report Non Ascii Error");
    arg_desc->AddFlag("context", "Suppress context in error msgs");
    arg_desc->AddFlag("align", "Validate Alignments");
    arg_desc->AddFlag("exon", "Validate exons");
    arg_desc->AddFlag("splice", "Report splice error as error");
    arg_desc->AddFlag("ovlpep", "Report overlapping peptide as error");
    arg_desc->AddFlag("taxid", "Requires taxid");
    arg_desc->AddFlag("isojta", "Requires ISO-JTA");

    arg_desc->AddOptionalKey(
        "x", "OutFile", "Output file for error messages",
        CArgDescriptions::eOutputFile);
    arg_desc->AddDefaultKey(
        "q", "SevLevel", "Lowest severity error to show",
        CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey(
        "r", "SevCount", "Severity error to count in return code",
        CArgDescriptions::eInteger, "2");
    CArgAllow* constraint = new CArgAllow_Integers(eDiagSevMin, eDiagSevMax);
    arg_desc->SetConstraint("q", constraint);
    arg_desc->SetConstraint("r", constraint);


    // !!!
    // { DEBUG
    // This flag should be removed once testing is done. It is intended for
    // performance testing.
    arg_desc->AddFlag("debug", "Disable suspected performance bottlenecks");
    // }

    // Program description
    string prog_description = "Test driver for Validate()\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());
}


int CTest_validatorApplication::Run(void)
{
    const CArgs& args = GetArgs();
    Setup(args);
    
    // Open File 
    m_In.reset(OpenFile(args));

    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    auto_ptr<CValidError> eval;
    if ( args["t"] ) {          // Release file
        ProcessReleaseFile(args);
        return 0;
    } else {
        string header = m_In->ReadFileHeader();

        if ( args["s"]  &&  header != "Seq-submit" ) {
            NCBI_THROW(CException, eUnknown,
                "Conflict: '-s' flag is specified but file is not Seq-submit");
        } 
        if ( args["s"]  ||  header == "Seq-submit" ) {   // Seq-submit
            eval.reset(ProcessSeqSubmit());
        } else {                    // default: Seq-entry
            eval.reset(ProcessSeqEntry());
        }
    }


    unsigned int result = 0;
    result = PrintValidError(*eval, args);
    
    return result;
}


void CTest_validatorApplication::ReadClassMember
(CObjectIStream& in,
 const CObjectInfo::CMemberIterator& member)
{
    // Read each element separately to a local TSeqEntry,
    // process it somehow, and... not store it in the container.
    for ( CIStreamContainerIterator i(in, member); i; ++i ) {
        try {
            // Get seq-entry to validate
            CRef<CSeq_entry> se(new CSeq_entry);
            i >> *se;
            
            // Validate Seq-entry
            auto_ptr<CValidError> eval(new CValidError(*m_ObjMgr, *se, m_Options));
            PrintValidError(*eval, GetArgs());
            
        } catch (exception e) {
            if ( !m_Continue ) {
                 throw;
            }
            // should we issue some sort of warning?
        }
    }
}


void CTest_validatorApplication::ProcessReleaseFile
(const CArgs& args)
{
    CRef<CBioseq_set> seqset(new CBioseq_set);

    // Register the Seq-entry hook
    CObjectTypeInfo set_type = CType<CBioseq_set>();
    set_type.FindMember("seq-set").SetLocalReadHook(*m_In, this);

    m_Continue = args["c"];

    // Read the CBioseq_set, it will call the hook object each time we 
    // encounter a Seq-entry
    *m_In >> *seqset;
}


CRef<CSeq_entry> CTest_validatorApplication::ReadSeqEntry(void)
{
    CRef<CSeq_entry> se(new CSeq_entry);
    m_In->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);

    return se;
}


CValidError* CTest_validatorApplication::ProcessSeqEntry(void)
{
    // Get seq-entry to validate
    CRef<CSeq_entry> se(ReadSeqEntry());
    
    // Validate Seq-entry
    return new CValidError(*m_ObjMgr, *se, m_Options);
}


CValidError* CTest_validatorApplication::ProcessSeqSubmit(void)
{
    CRef<CSeq_submit> ss(new CSeq_submit);
    
    // Get seq-entry to validate
    m_In->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);
    
    // Validae Seq-entry
    return new CValidError(*m_ObjMgr, *ss, m_Options);
}



void CTest_validatorApplication::Setup(const CArgs& args)
{    
    // Setup application registry and logs for CONNECT library
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

    // Create object manager
    m_ObjMgr.Reset(new CObjectManager);
    if ( args["g"] ) {
        // Create GenBank data loader and register it with the OM.
        // The last argument "eDefault" informs the OM that the loader must
        // be included in scopes during the CScope::AddDefaults() call.
        m_ObjMgr->RegisterDataLoader(*new CGBDataLoader("ID"),
            CObjectManager::eDefault);
    }

    SetupValidatorOptions(args);
}


void CTest_validatorApplication::SetupValidatorOptions(const CArgs& args)
{
    // Set validator options
    m_Options = 0;

    m_Options |= args["nonascii"] ? CValidError::eVal_non_ascii :   0;
    m_Options |= args["context"]  ? CValidError::eVal_no_context :  0;
    m_Options |= args["align"]    ? CValidError::eVal_val_align :   0;
    m_Options |= args["exon"]     ? CValidError::eVal_val_exons :   0;
    m_Options |= args["splice"]   ? CValidError::eVal_splice_err :  0;
    m_Options |= args["ovlpep"]   ? CValidError::eVal_ovl_pep_err : 0;
    m_Options |= args["taxid"]    ? CValidError::eVal_need_taxid :  0;
    m_Options |= args["isojta"]   ? CValidError::eVal_need_isojta : 0;

    // !!!  DEBUG {
    // For testing only. Should be removed in the future
    m_Options |= args["debug"].HasValue() ? CValidError::eVal_perf_bottlenecks : 0;
    // }
}


CObjectIStream* CTest_validatorApplication::OpenFile
(const CArgs& args)
{
    // file name
    string fname = args["i"].AsString();

    // file format 
    ESerialDataFormat format = eSerial_AsnText;
    if ( args["b"] ) {
        format = eSerial_AsnBinary;
    }
    
    return CObjectIStream::Open(fname, format);
}


unsigned int CTest_validatorApplication::PrintValidError
(const CValidError& errors, 
 const CArgs& args)
{
    EDiagSev show = static_cast<EDiagSev>(args["q"].AsInteger());
    EDiagSev count = static_cast<EDiagSev>(args["r"].AsInteger());
    
    CNcbiOstream* os = args["x"] ? &(args["x"].AsOutputFile()) : &cout;

    

    if ( errors.size() == 0 ) {
        *os << "All entries are OK!" << endl;
        os->flush();
        return 0;
    }

    unsigned int result = 0;
    unsigned int reported = 0;

    for ( CValidError_CI vit(errors); vit; ++vit) {
        if ( vit->GetSeverity() >= count ) {
            ++result;
        }
        if ( vit->GetSeverity() < show ) {
            continue;
        }
        PrintValidErrItem(*vit, *os);
        ++reported;
    }
    *os << reported << " messages reported" << endl;
    os->flush();

    return result;
}


void CTest_validatorApplication::PrintValidErrItem
(const CValidErrItem& item,
 CNcbiOstream& os)
{
    os << item.GetSevAsStr() << ":       " << item.GetErrCode() << endl << endl
       << "Message: " << item.GetMsg() << endl << endl
       << "Verbose: " << item.GetVerbose() << endl << endl;
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
 * Revision 1.12  2003/02/24 20:36:15  shomrat
 * Added several application flags, including batch processing
 *
 * Revision 1.11  2003/02/14 21:55:13  shomrat
 * Output the severity of an error
 *
 * Revision 1.10  2003/02/07 21:26:01  shomrat
 * More detailed error report
 *
 * Revision 1.9  2003/02/05 00:28:07  ucko
 * +<serial/objistr.hpp> (formerly pulled in via reader_id1.hpp)
 *
 * Revision 1.8  2003/02/03 20:22:25  shomrat
 * Added flag to supress performance bottlenecks
 *
 * Revision 1.7  2003/02/03 17:51:06  shomrat
 * Bug fix - Trailing comma in a parameter list
 *
 * Revision 1.6  2003/02/03 17:06:48  shomrat
 * Changed parameters to CGBDataLoader
 *
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
