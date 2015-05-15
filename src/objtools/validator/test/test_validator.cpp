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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objectio.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objtools/validator/validator.hpp>

#include <objects/seqset/Bioseq_set.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <common/test_assert.h>  /* This header must go last */


using namespace ncbi;
using namespace objects;
using namespace validator;


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


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

    CConstRef<CValidError> ProcessSeqEntry(void);
    CConstRef<CValidError> ProcessSeqSubmit(void);
    CConstRef<CValidError> ProcessSeqAnnot(void);
    void ProcessReleaseFile(const CArgs& args);
    CRef<CSeq_entry> ReadSeqEntry(void);
    SIZE_TYPE PrintValidError(CConstRef<CValidError> errors, 
        const CArgs& args);
    SIZE_TYPE PrintBatchErrors(CConstRef<CValidError> errors,
        const CArgs& args);

    void PrintValidErrItem(const CValidErrItem& item, CNcbiOstream& os);
    void PrintBatchItem(const CValidErrItem& item, CNcbiOstream& os, bool printAcc = false);

    CRef<CObjectManager> m_ObjMgr;
    auto_ptr<CObjectIStream> m_In;
    unsigned int m_Options;
    bool m_Continue;
    bool m_OnlyAnnots;

    size_t m_Level;
    size_t m_Reported;
};


CTest_validatorApplication::CTest_validatorApplication(void) :
    m_ObjMgr(0), m_In(0), m_Options(0), m_Continue(false),
    m_OnlyAnnots(false), m_Level(0), m_Reported(0)
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
    arg_desc->AddFlag("annot", "Verify Seq-annots only");
    arg_desc->AddFlag("acc", "Print Accession with errors");

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
    CConstRef<CValidError> eval;
    if ( args["t"] ) {          // Release file
        ProcessReleaseFile(args);
        return 0;
    } else {
        string header = m_In->ReadFileHeader();

        if ( args["s"]  &&  header != "Seq-submit" ) {
            NCBI_THROW(CException, eUnknown,
                "Conflict: '-s' flag is specified but file is not Seq-submit");
        } 
        if ( args["s"]  ||  header == "Seq-submit" ) {  // Seq-submit
            eval = ProcessSeqSubmit();
        } else if ( header == "Seq-entry" ) {           // Seq-entry
            eval = ProcessSeqEntry();
        } else if ( header == "Seq-annot" ) {           // Seq-annot
            eval = ProcessSeqAnnot();
        } else {
            NCBI_THROW(CException, eUnknown, "Unhandaled type " + header);
        }
    }

    unsigned int result = 0;
    if ( eval ) {
        result = PrintValidError(eval, args);
    }

    return result;
}


void CTest_validatorApplication::ReadClassMember
(CObjectIStream& in,
 const CObjectInfo::CMemberIterator& member)
{
    m_Level++;

    if ( m_Level == 1 ) {
        // Read each element separately to a local TSeqEntry,
        // process it somehow, and... not store it in the container.
        for ( CIStreamContainerIterator i(in, member); i; ++i ) {
            try {
                // Get seq-entry to validate
                CRef<CSeq_entry> se(new CSeq_entry);
                i >> *se;

                // Validate Seq-entry
                CValidator validator(*m_ObjMgr);
                CScope scope(*m_ObjMgr);
                if ( m_OnlyAnnots ) {
                    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*se);
                    for (CSeq_annot_CI ni(seh); ni; ++ni) {
                        const CSeq_annot_Handle& sah = *ni;
                        CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
                        if ( eval ) {
                            m_Reported += PrintBatchErrors(eval, GetArgs());
                        }
                    }
                } else {
                    // CConstRef<CValidError> eval = validator.Validate(*se, &scope, m_Options);
                    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*se);
                    CConstRef<CValidError> eval = validator.Validate(seh, m_Options);
                    if ( eval ) {
                        m_Reported += PrintBatchErrors(eval, GetArgs());
                    }
                }
            } catch (exception e) {
                if ( !m_Continue ) {
                    throw;
                }
                // should we issue some sort of warning?
            }
        }
    } else {
        in.ReadClassMember(member);
    }

    m_Level--;
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

    NcbiCerr << m_Reported << " messages reported" << endl;
}


SIZE_TYPE CTest_validatorApplication::PrintBatchErrors
(CConstRef<CValidError> errors,
 const CArgs& args)
{
    if ( !errors  ||  errors->TotalSize() == 0 ) {
        return 0;
    }

    bool    printAcc = args["acc"];
    EDiagSev show  = static_cast<EDiagSev>(args["q"].AsInteger());
    CNcbiOstream* os = args["x"] ? &(args["x"].AsOutputFile()) : &NcbiCout;
    SIZE_TYPE reported = 0;

    for ( CValidError_CI vit(*errors); vit; ++vit ) {
        if ( vit->GetSeverity() < show ) {
            continue;
        }
        PrintBatchItem(*vit, *os, printAcc);
        ++reported;
    }

    return reported;
}


CRef<CSeq_entry> CTest_validatorApplication::ReadSeqEntry(void)
{
    CRef<CSeq_entry> se(new CSeq_entry);
    m_In->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);

    return se;
}


CConstRef<CValidError> CTest_validatorApplication::ProcessSeqEntry(void)
{
    // Get seq-entry to validate
    CRef<CSeq_entry> se(ReadSeqEntry());

    // Validate Seq-entry
    CValidator validator(*m_ObjMgr);
    CScope scope(*m_ObjMgr);
    if ( m_OnlyAnnots ) {
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*se);
        for (CSeq_annot_CI ni(seh); ni; ++ni) {
            const CSeq_annot_Handle& sah = *ni;
            CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
            if ( eval ) {
                m_Reported += PrintBatchErrors(eval, GetArgs());
            }
        }
        return CConstRef<CValidError>();
    }
    scope.AddTopLevelSeqEntry(*se);
    return validator.Validate(*se, &scope, m_Options);
}


CConstRef<CValidError> CTest_validatorApplication::ProcessSeqSubmit(void)
{
    CRef<CSeq_submit> ss(new CSeq_submit);

    // Get seq-submit to validate
    m_In->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);

    // Validae Seq-submit
    CValidator validator(*m_ObjMgr);
    CScope scope(*m_ObjMgr);
    return validator.Validate(*ss, &scope, m_Options);
}


CConstRef<CValidError> CTest_validatorApplication::ProcessSeqAnnot(void)
{
    CRef<CSeq_annot> sa(new CSeq_annot);

    // Get seq-annot to validate
    m_In->Read(ObjectInfo(*sa), CObjectIStream::eNoFileHeader);

    // Validae Seq-annot
    CValidator validator(*m_ObjMgr);
    CScope scope(*m_ObjMgr);
    CSeq_annot_Handle sah = scope.AddSeq_annot(*sa);
    return validator.Validate(sah, m_Options);
}


void CTest_validatorApplication::Setup(const CArgs& args)
{
    // Setup application registry and logs for CONNECT library
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

    // Create object manager
    m_ObjMgr = CObjectManager::GetInstance();
    if ( args["g"] ) {
        // Create GenBank data loader and register it with the OM.
        // The last argument "eDefault" informs the OM that the loader must
        // be included in scopes during the CScope::AddDefaults() call.
        CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
    }

    m_OnlyAnnots = args["annot"];

    SetupValidatorOptions(args);
}


void CTest_validatorApplication::SetupValidatorOptions(const CArgs& args)
{
    // Set validator options
    m_Options = 0;

    m_Options |= args["nonascii"] ? CValidator::eVal_non_ascii    : 0;
    m_Options |= args["context"]  ? CValidator::eVal_no_context   : 0;
    m_Options |= args["align"]    ? CValidator::eVal_val_align    : 0;
    m_Options |= args["exon"]     ? CValidator::eVal_val_exons    : 0;
    m_Options |= args["ovlpep"]   ? CValidator::eVal_ovl_pep_err  : 0;
    m_Options |= args["taxid"]    ? CValidator::eVal_need_taxid   : 0;
    m_Options |= args["isojta"]   ? CValidator::eVal_need_isojta  : 0;
    m_Options |= args["g"]        ? CValidator::eVal_remote_fetch : 0;

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


SIZE_TYPE CTest_validatorApplication::PrintValidError
(CConstRef<CValidError> errors, 
 const CArgs& args)
{
    EDiagSev show = static_cast<EDiagSev>(args["q"].AsInteger());
    EDiagSev count = static_cast<EDiagSev>(args["r"].AsInteger());

    CNcbiOstream* os = args["x"] ? &(args["x"].AsOutputFile()) : &NcbiCout;

    if ( errors->TotalSize() == 0 ) {
        *os << "All entries are OK!" << endl;
        os->flush();
        return 0;
    }

    SIZE_TYPE result = 0;
    SIZE_TYPE reported = 0;

    for ( CValidError_CI vit(*errors); vit; ++vit) {
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

    *os << "Total number of errors: " << errors->TotalSize() << endl
        << "Info: "     << errors->InfoSize()     << endl
        << "Warning: "  << errors->WarningSize()  << endl
        << "Error: "    << errors->ErrorSize()    << endl
        << "Critical: " << errors->CriticalSize() << endl
        << "Fatal: "    << errors->FatalSize()    << endl;

    return result;
}


void CTest_validatorApplication::PrintValidErrItem
(const CValidErrItem& item,
 CNcbiOstream& os)
{
    os << CValidErrItem::ConvertSeverity(item.GetSeverity()) << ":       " 
       << item.GetErrGroup() << " "
       << item.GetErrCode() << endl << endl
       << "Message: " << item.GetMsg() << " " << item.GetObjDesc() << endl << endl;
     //<< "Verbose: " << item.GetVerbose() << endl << endl;
}



void CTest_validatorApplication::PrintBatchItem
(const CValidErrItem& item,
 CNcbiOstream&os, bool printAcc)
{
    if (printAcc) {
        os << item.GetAccession() << ":";
    }
    os << CValidErrItem::ConvertSeverity(item.GetSeverity()) << ": [" << item.GetErrCode() <<"] ["
       << item.GetMsg() << "]" << endl;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CTest_validatorApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
