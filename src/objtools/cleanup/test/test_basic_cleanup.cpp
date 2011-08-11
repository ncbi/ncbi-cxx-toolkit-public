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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko, Michael Kornbluh
 *
 * File Description:
 *   Test Program for cleanup.
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
#include <serial/objostrasn.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/misc/sequence_macros.hpp>

// new_cleanup.hpp

#include <objects/misc/sequence_macros.hpp>
#include <objmgr/scope.hpp>
#include <objtools/cleanup/cleanup_change.hpp>
#include <objtools/cleanup/cleanup.hpp>

using namespace ncbi;
using namespace objects;

/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//

class CTest_cleanupApplication : public CNcbiApplication, CReadClassMemberHook
{
public:
    CTest_cleanupApplication(void);

    virtual void Init(void);
    virtual int  Run (void);

    void ReadClassMember(CObjectIStream& in,
        const CObjectInfo::CMemberIterator& member);

private:

    void Setup(const CArgs& args);
    void SetupCleanupOptions(const CArgs& args);

    vector<CObjectIStream*> OpenIFiles(const CArgs& args);
    vector<CObjectOStream*> OpenOFiles(const CArgs& args);

    CConstRef<CCleanupChange> ProcessSeqEntry(void);
    CConstRef<CCleanupChange> ProcessSeqSubmit(void);
    CConstRef<CCleanupChange> ProcessSeqAnnot(void);
    CConstRef<CCleanupChange> ProcessSeqFeat(void);
    CConstRef<CCleanupChange> ProcessBioseq(void);
    CConstRef<CCleanupChange> ProcessBioseqSet(void);
    void ProcessReleaseFile(const CArgs& args);
    CRef<CSeq_entry> ReadSeqEntry(void);
    SIZE_TYPE PrintChanges(CConstRef<CCleanupChange> errors, 
        const CArgs& args);

    auto_ptr<CObjectIStream> m_In;
    auto_ptr<CObjectOStream> m_Out;
    unsigned int m_Options;
    bool m_Continue;
    bool m_NoCleanup;

    size_t m_Level;
    size_t m_Reported;
    bool m_DoExtendedCleanup;
    bool m_UseHandleVersion;

    CScope m_Scope;
};


CTest_cleanupApplication::CTest_cleanupApplication(void) :
    m_In(0), m_Options(0), m_Continue(false), m_NoCleanup(false), m_Level(0),
    m_Reported(0), m_DoExtendedCleanup(false), m_UseHandleVersion(false),
    m_Scope( *CObjectManager::GetInstance() )
{
}


void CTest_cleanupApplication::Init(void)
{
    // Prepare command line descriptions

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey
        ("i", "ASNFile", "Input Seq-entry/Seq_submit ASN.1 text file",
        CArgDescriptions::eString, "-", CArgDescriptions::fAllowMultiple );
    arg_desc->AddDefaultKey
        ("o", "ASNFile", "Output Seq-entry/Seq_submit ASN.1 text file",
         CArgDescriptions::eString, "-", CArgDescriptions::fAllowMultiple );
    
    arg_desc->AddFlag("s", "Input is Seq-submit");
    arg_desc->AddFlag("t", "Input is Seq-set (NCBI Release file)");
    arg_desc->AddFlag("b", "Input is in binary format");
    arg_desc->AddFlag("c", "Continue on ASN.1 error");
    arg_desc->AddFlag("e", "Do extended cleanup, not just basic cleanup.");
    arg_desc->AddFlag("d", "Use the handle version of cleanup, if supported.");
    arg_desc->AddFlag("n", "Don't do cleanup. Just read and write file");

    arg_desc->AddOptionalKey(
        "x", "OutFile", "Output file for error messages",
        CArgDescriptions::eOutputFile);

    // Program description
    string prog_description = "Test driver for BasicCleanup()\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());
}


int CTest_cleanupApplication::Run(void)
{
    const CArgs& args = GetArgs();
    Setup(args);

    vector<CObjectIStream*> input_files  = OpenIFiles(args);
    vector<CObjectOStream*> output_files = OpenOFiles(args);

    if( input_files.size() != output_files.size() ) {
        NCBI_THROW(CException, eUnknown,
            "You must have the same number of input files as output files");
    }
    if( input_files.size() < 1 ) {
        NCBI_THROW(CException, eUnknown,
            "You must specify at least one input and output file");
    }

    // Open File 
    int file_index = 0;
    for( ; file_index < (int)input_files.size(); ++file_index ) {
        m_In .reset(input_files[file_index]);
        m_Out.reset(output_files[file_index]);

        // Process file based on its content
        // Unless otherwise specified we assume the file in hand is
        // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
        // Release file (batch processing) where we process each Seq-entry
        // at a time.
        CConstRef<CCleanupChange> changes;
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
                changes = ProcessSeqSubmit();
            } else if ( header == "Seq-entry" ) {           // Seq-entry
                changes = ProcessSeqEntry();
            } else if ( header == "Seq-annot" ) {           // Seq-annot
                changes = ProcessSeqAnnot();
            } else if ( header == "Seq-feat" ) {            // Seq-feat
                changes = ProcessSeqFeat();
            } else if ( header == "Bioseq" ) {              // Bioseq
                changes = ProcessBioseq();
            } else if ( header == "Bioseq-set" ) {          // Bioseq-set
                changes = ProcessBioseqSet();
            } else {
                NCBI_THROW(CException, eUnknown, "Unhandled type " + header);
            }
        }

        if ( changes && changes->ChangeCount() > 0 ) {
            PrintChanges(changes, args);
        }
    }

    return 0;
}


void CTest_cleanupApplication::ReadClassMember
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

                // BasicCleanup Seq-entry
                CCleanup cleanup;
                CConstRef<CCleanupChange> changes;
                if ( ! m_NoCleanup) {
                    changes = ( m_DoExtendedCleanup ? 
                        cleanup.ExtendedCleanup( *se, m_Options ) :
                        cleanup.BasicCleanup(*se, m_Options) );
                }
                if ( changes->ChangeCount() > 0 ) {
                    m_Reported += PrintChanges(changes, GetArgs());
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


void CTest_cleanupApplication::ProcessReleaseFile
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


CRef<CSeq_entry> CTest_cleanupApplication::ReadSeqEntry(void)
{
    CRef<CSeq_entry> se(new CSeq_entry);
    m_In->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);

    return se;
}


CConstRef<CCleanupChange> CTest_cleanupApplication::ProcessSeqEntry(void)
{
    // Get seq-entry to validate
    CRef<CSeq_entry> se(ReadSeqEntry());

    // BasicCleanup Seq-entry
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    if ( ! m_NoCleanup) {
        if( m_UseHandleVersion ) {
            CSeq_entry_Handle seh = m_Scope.AddTopLevelSeqEntry(*se);
            changes = cleanup.BasicCleanup( seh, m_Options );
            *m_Out << (*seh.GetCompleteSeq_entry());
        } else {
            changes = ( m_DoExtendedCleanup ?
                cleanup.ExtendedCleanup( *se, m_Options ) :
            cleanup.BasicCleanup(*se, m_Options) );
            *m_Out << (*se);
        }
    }
    return changes;
}


CConstRef<CCleanupChange> CTest_cleanupApplication::ProcessSeqSubmit(void)
{
    CRef<CSeq_submit> ss(new CSeq_submit);

    // Get seq-submit to validate
    m_In->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);

    // Validae Seq-submit
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    if ( ! m_NoCleanup) {
        // There is no handle version for Seq-submit
        changes = ( m_DoExtendedCleanup ?
            cleanup.ExtendedCleanup(*ss, m_Options) : 
            cleanup.BasicCleanup(*ss, m_Options) );
    }
    *m_Out << (*ss);
    return changes;
}


CConstRef<CCleanupChange> CTest_cleanupApplication::ProcessSeqAnnot(void)
{
    CRef<CSeq_annot> sa(new CSeq_annot);

    // Get seq-feat to validate
    m_In->Read(ObjectInfo(*sa), CObjectIStream::eNoFileHeader);

    // Validate Seq-feat
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    if ( ! m_NoCleanup) {
        if( m_UseHandleVersion ) {
            CSeq_annot_Handle sah = m_Scope.AddSeq_annot(*sa);
            changes = cleanup.BasicCleanup( sah, m_Options );
            *m_Out << (*sah.GetCompleteSeq_annot());
        } else {
            changes = ( m_DoExtendedCleanup ?
                cleanup.ExtendedCleanup( *sa, m_Options ) :
            cleanup.BasicCleanup(*sa, m_Options) );
            *m_Out << (*sa);
        }
    }
    return changes;
}

CConstRef<CCleanupChange> CTest_cleanupApplication::ProcessSeqFeat(void)
{
    CRef<CSeq_feat> sf(new CSeq_feat);

    // Get seq-feat to validate
    m_In->Read(ObjectInfo(*sf), CObjectIStream::eNoFileHeader);

    // Validate Seq-feat
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    if ( ! m_NoCleanup) {
        if( m_UseHandleVersion ) {
            // set up a minimal fake Seq-entry so that we can 
            // get a handle for the Seq-feat.
            CRef<CSeq_annot> annot( new CSeq_annot );
            annot->SetData().SetFtable().push_back( sf );
            CRef<CSeq_entry> entry( new CSeq_entry );
            entry->SetSeq().SetAnnot().push_back( annot );
            m_Scope.AddTopLevelSeqEntry( *entry );

            CSeq_feat_Handle sfh = m_Scope.GetSeq_featHandle(*sf);
            changes = cleanup.BasicCleanup(sfh, m_Options);
            *m_Out << (*sfh.GetSeq_feat());
        } else {
            // no ExtendedCleanup for this object type
            changes = cleanup.BasicCleanup(*sf, m_Options);
            *m_Out << (*sf);
        }
    }
    return changes;
}

CConstRef<CCleanupChange> CTest_cleanupApplication::ProcessBioseq(void)
{
    CRef<CBioseq> bs(new CBioseq);

    // Get bioseq to validate
    m_In->Read(ObjectInfo(*bs), CObjectIStream::eNoFileHeader);

    // Validae bioseq
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    if ( ! m_NoCleanup) {
        if( m_UseHandleVersion ) {
            CBioseq_Handle bsh = m_Scope.AddBioseq(*bs);
            changes = cleanup.BasicCleanup(bsh, m_Options);
            *m_Out << (*bsh.GetCompleteBioseq());
        } else {
            // no ExtendedCleanup for this object type
            changes = cleanup.BasicCleanup(*bs, m_Options);
            *m_Out << (*bs);
        }
    }
    return changes;
}

CConstRef<CCleanupChange> CTest_cleanupApplication::ProcessBioseqSet(void)
{
    CRef<CBioseq_set> bss(new CBioseq_set);

    // Get bioseq-set to validate
    m_In->Read(ObjectInfo(*bss), CObjectIStream::eNoFileHeader);

    // Validae bioseq-set
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    if ( ! m_NoCleanup) {
        if( m_UseHandleVersion ) {
            // set up minimal fake Seq-entry so we can get
            // the Bioseq handle.
            CRef<CSeq_entry> entry( new CSeq_entry );
            entry->SetSet( *bss );
            m_Scope.AddTopLevelSeqEntry( *entry );
            CBioseq_set_Handle bssh = m_Scope.GetBioseq_setHandle(*bss);
            changes = cleanup.BasicCleanup(bssh, m_Options);
            *m_Out << (*bssh.GetCompleteBioseq_set());
        } else {
            // no ExtendedCleanup for this object type
            changes = cleanup.BasicCleanup(*bss, m_Options);
            *m_Out << (*bss);
        }
    }
    return changes;
}



void CTest_cleanupApplication::Setup(const CArgs& args)
{
    if (args["n"])
        m_NoCleanup = true;
    if( args["e"] ) {
        m_DoExtendedCleanup = true;
    }
    if( args["d"] ) {
        m_UseHandleVersion = true;
    }
    SetupCleanupOptions(args);
}


void CTest_cleanupApplication::SetupCleanupOptions(const CArgs& args)
{
    // Set cleanup options
    m_Options = 0;

}


vector<CObjectIStream*> CTest_cleanupApplication::OpenIFiles
(const CArgs& args)
{
    vector<CObjectIStream*> result;

    // file name
    const CArgValue::TStringArray &file_name_array = args["i"].GetStringList();
    FOR_EACH_STRING_IN_VECTOR( file_name_iter, file_name_array ) {
        // just let it "leak" because our result will point to it
        // almost the whole time the program is running
        CNcbiIstream *in_file = NULL;
        if( *file_name_iter == "-" ) {
            in_file = &std::cin;
        } else {
            in_file = new CNcbiIfstream( file_name_iter->c_str() );
        }

        // file format 
        ESerialDataFormat format = eSerial_AsnText;
        if ( args["b"] ) {
            format = eSerial_AsnBinary;
        }

        result.push_back( CObjectIStream::Open(format, *in_file ) );
    }

    // copies a vector, but the performance hit is completely negligible, so
    // we do it the easy way.
    return result;
}


vector<CObjectOStream*> CTest_cleanupApplication::OpenOFiles
(const CArgs& args)
{
    vector<CObjectOStream*> result;

    // file name
    const CArgValue::TStringArray &file_name_array = args["o"].GetStringList();
    FOR_EACH_STRING_IN_VECTOR( file_name_iter, file_name_array ) {
        // just let it "leak" because our result will point to it
        // almost the whole time the program is running
        CNcbiOstream *out_file = NULL;
        if( *file_name_iter == "-" ) {
            out_file = &std::cout;
        } else {
            out_file = new CNcbiOfstream( file_name_iter->c_str() );
        }

        // file format 
        ESerialDataFormat format = eSerial_AsnText;

        result.push_back( CObjectOStream::Open(format, *out_file) );
    }

    // copies a vector, but the performance hit is completely negligible, so
    // we do it the easy way.
    return result;
}


SIZE_TYPE CTest_cleanupApplication::PrintChanges
(CConstRef<CCleanupChange> changes, 
 const CArgs& args)
{
    if (args["x"]) {
        string errOutName = args["x"].AsString();

        CNcbiOstream* os = &NcbiCerr;
        if (errOutName != "-")
            os = &(args["x"].AsOutputFile());

        if ( changes->ChangeCount() == 0 ) {
            return 0;
        }

        vector<string> changes_str = changes->GetAllDescriptions();
        ITERATE(vector<string>, vit, changes_str) {
            *os << *vit << endl;
        }

        *os << "Number of changes: " << changes->ChangeCount() << endl;
    }
    return changes->ChangeCount();
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CTest_cleanupApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
