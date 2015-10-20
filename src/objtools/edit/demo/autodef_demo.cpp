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
* Author:  Aaron Ucko
*
* File Description:
*   Simple program demonstrating the use of serializable objects (in this
*   case, biological sequences).  Does NOT use the object manager.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objtools/edit/autodef.hpp>

#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/scope_transaction.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/data_loader.hpp>
#include <objmgr/edits_db_saver.hpp>
#include <objmgr/bioseq_ci.hpp>


USING_SCOPE(ncbi);
USING_SCOPE(objects);

class CAutoDefDemo : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);

    // Member variable to help illustrate our naming conventions
    CSeq_entry m_Entry;
};


void CAutoDefDemo::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Object serialization demo program: Seq-entry translator");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("in", "InputFile",
         "name of file to read from (standard input by default)",
         CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey("infmt", "InputFormat", "format of input file",
                            CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint
        ("infmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml"));

    arg_desc->AddDefaultKey
        ("out", "OutputFile",
         "name of file to write to (standard output by default)",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey("outfmt", "OutputFormat", "format of output file",
                            CArgDescriptions::eString, "xml");
    arg_desc->SetConstraint
        ("outfmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml"));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


static ESerialDataFormat s_GetFormat(const string& name)
{
    if (name == "asn") {
        return eSerial_AsnText;
    } else if (name == "asnb") {
        return eSerial_AsnBinary;
    } else if (name == "xml") {
        return eSerial_Xml;
    } else {
        // Should be caught by argument processing, but as an illustration...
        THROW1_TRACE(runtime_error, "Bad serial format name " + name);
    }
}


int CAutoDefDemo::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    // Read the entry
    {{
        auto_ptr<CObjectIStream> in
            (CObjectIStream::Open(s_GetFormat(args["infmt"].AsString()),
                                   args["in"].AsInputFile()));
        *in >> m_Entry;
    }}


    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry (m_Entry);

    objects::CAutoDef autodef;

    // add to autodef 
    autodef.AddSources (seh);

    CRef<CAutoDefModifierCombo> mod_combo (new CAutoDefModifierCombo ());
    mod_combo.Reset(autodef.GetEmptyCombo());
    if (!mod_combo->AreFeatureClausesUnique()) {
        mod_combo.Reset(autodef.FindBestModifierCombo());
    }
    mod_combo->SetUseModifierLabels(true);
    CBioseq_CI seq_iter(seh);
    for ( ; seq_iter; ++seq_iter ) {
       CBioseq_Handle bh (*seq_iter);
       //Display ID of sequence
       CConstRef<CSeq_id> id = bh.GetSeqId();
       id->WriteAsFasta(NcbiCout);
       NcbiCout << NcbiEndl;

       // original defline
       NcbiCout << "Original Defline";
       NcbiCout << NcbiEndl;
       for (CSeqdesc_CI desc_it(bh, CSeqdesc::e_Title, 1); desc_it; ++desc_it) {
           NcbiCout << desc_it->GetTitle();
           NcbiCout << NcbiEndl;
       }
       
       NcbiCout << "New Defline";
       NcbiCout << NcbiEndl;
       string defline = autodef.GetOneDefLine(mod_combo, bh);
       NcbiCout << defline;
       NcbiCout << NcbiEndl;
       NcbiCout << NcbiEndl;
    }


    // Write the entry
    {{
        auto_ptr<CObjectOStream> out
            (CObjectOStream::Open(s_GetFormat(args["outfmt"].AsString()),
                                  args["out"].AsOutputFile()));
        *out << m_Entry;
    }}

    // Exit successfully
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CAutoDefDemo().AppMain(argc, argv, 0, eDS_Default, 0);
}
