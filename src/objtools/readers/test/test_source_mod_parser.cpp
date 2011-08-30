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
*   Simple test for CSourceModParser, using external data.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <serial/objistrasn.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objtools/readers/error_container.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/readfeat.hpp>
#include <objtools/readers/source_mod_parser.hpp>
#include <objtools/readers/source_mod_parser_wrapper.hpp>
#include <objtools/readers/table_filter.hpp>

#include <common/test_assert.h>

#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_ci.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CSourceModParserTestApp : public CNcbiApplication
{
    void Init(void);
    int  Run (void);
};

void CSourceModParserTestApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Simple test of CSourceModParser");

    arg_desc->AddOptionalKey
        ("fasta", "InputFile", "Location of (FASTA-format) input",
         CArgDescriptions::eInputFile );

    arg_desc->AddOptionalKey
        ("feattbl", "InputFile", "Location of (5-column feature table) input",
         CArgDescriptions::eInputFile );

    arg_desc->AddDefaultKey
        ("out", "OutputFile", "Where to write (text ASN.1) output",
         CArgDescriptions::eOutputFile, "-");

    arg_desc->AddDefaultKey
        ("org", "Organism", "Organism name to use by default",
         CArgDescriptions::eString, "");

    arg_desc->AddOptionalKey("tbsq", "TitleBioseq", 
        "If you specify this input file, which should contain a CBioseq in "
            "text ASN.1 format, then the program will output that bioseq with its"
            "title processed like in FASTA and turned into features, etc.", 
        CArgDescriptions::eInputFile );

    SetupArgDescriptions(arg_desc.release());
}


static
void s_Visit(CSeq_entry& entry, CObjectOStream& oos /*, const string& org */ )
{
    //if (entry.IsSet()) {
    //    NON_CONST_ITERATE (CBioseq_set::TSeq_set, it,
    //                       entry.SetSet().SetSeq_set()) {
    //        s_Visit(**it, oos, org);
    //    }
    //} else {
    //    CBioseq&         seq = entry.SetSeq();
    //    CSourceModParser smp;
    //    CConstRef<CSeqdesc> title_desc
    //        = seq.GetClosestDescriptor(CSeqdesc::e_Title);
    //    if (title_desc) {
    //        string& title(const_cast<string&>(title_desc->GetTitle()));
    //        title = smp.ParseTitle(title);
    //        smp.ApplyAllMods(seq, org);
    //        smp.GetLabel(&title, CSourceModParser::fUnusedMods);
    //    }
    //    oos << seq;
    //    oos.Flush();
    //}
    if (entry.IsSet()) {
        NON_CONST_ITERATE (CBioseq_set::TSeq_set, it,
                           entry.SetSet().SetSeq_set()) {
            s_Visit(**it, oos);
        }
    } else {
        CBioseq&         seq = entry.SetSeq();
        oos << seq;
        oos.Flush();
    }
}


int CSourceModParserTestApp::Run(void)
{
    const CArgs&      args = GetArgs();

    CObjectOStreamAsn oos(args["out"].AsOutputFile());

    // See if we should do fasta parsing
    if( args["fasta"] )
    {
        CFastaReader      reader( args["fasta"].AsString(), CFastaReader::fAddMods );
        CRef<CSeq_entry>  entry(reader.ReadSet());

        CSimpleTableFilter tbl_filter;
        tbl_filter.AddDisallowedFeatureName("source", ITableFilter::eResult_Disallowed );

        CErrorContainerLenient err_container;
        CFeature_table_reader::ReadSequinFeatureTables( args["feattbl"].AsInputFile(), *entry, 
            CFeature_table_reader::fReportBadKey, 
            &err_container, &tbl_filter );

        // print out result
        s_Visit(*entry, oos /*, args["org"].AsString() */ ); 

        // print out any errors found:
        size_t idx = 0;
        for( ; idx < err_container.Count(); ++idx ) {
            const ILineError &err = err_container.GetError(idx);
            cerr << "Error in seq-id " << err.SeqId() << " on line " << err.Line() << " of severity " << err.SeverityStr() << ": \"" 
                << err.Message() << "\"" << endl;
        }
    }

    // see if we should do bioseq parsing
    if( args["tbsq"] )
    {
        CRef<CScope> scope( new CScope(*CObjectManager::GetInstance()) );

        CObjectIStreamAsn asn_in_strm(args["tbsq"].AsInputFile());
        // CRef<CBioseq> bioseq( new CBioseq );
        CRef<CSeq_entry> seqentry( new CSeq_entry );
        asn_in_strm >> *seqentry;

        // scope->AddBioseq( *bioseq );
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry( *seqentry );

        typedef vector<CBioseq_Handle> TBshVec;
        vector<CBioseq_Handle> bsh_vec;
        CBioseq_CI bsh_ci( seh );
        for( ; bsh_ci; ++bsh_ci ) {
            bsh_vec.push_back( *bsh_ci );
        }

        TBshVec::iterator bsh_vec_iter = bsh_vec.begin();
        for( ; bsh_vec_iter != bsh_vec.end(); ++bsh_vec_iter ) {
            CSourceModParserWrapper::ExtractTitleAndApplyAllMods( 
                *bsh_vec_iter, 
                args["org"].AsString() );
            oos << *bsh_vec_iter->GetCompleteBioseq();
        }
    }

    return 0;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CSourceModParserTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
