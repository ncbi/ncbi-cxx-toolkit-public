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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   Test application for the CFormatGuess component
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <util/format_guess.hpp>

#include <serial/objistrasnb.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrxml.hpp>
#include <serial/objistrjson.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>

#include <misc/xmlwrapp/attributes.hpp>
#include <misc/xmlwrapp/document.hpp>
#include <misc/xmlwrapp/node.hpp>

typedef std::map<ncbi::CFormatGuess::EFormat, std::string> FormatMap;
typedef FormatMap::iterator FormatIter;

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
class CFormatGuessApp
//  ============================================================================
     : public CNcbiApplication
{
private:
    static string guess_object_type(CObjectIStream & obj_istrm);

    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};

//  ============================================================================
string CFormatGuessApp::guess_object_type(CObjectIStream & obj_istrm)
//  ============================================================================
{
    set<TTypeInfo> known_types = {
        CType<CSeq_entry>().GetTypeInfo(),
        CType<CSeq_submit>().GetTypeInfo(),
        CType<CBioseq_set>().GetTypeInfo(),
        CType<CBioseq>().GetTypeInfo()
    };

    set<TTypeInfo> types = obj_istrm.GuessDataType(known_types);
    if ( types.size() != 1 ) {
        return "unknown";
    }
    return (*types.begin())->GetName();
}

//  ============================================================================
void CFormatGuessApp::Init(void)
//  ============================================================================
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "CFormatGuess front end: Guess various file formats");

    //
    //  shared flags and parameters:
    //        
    arg_desc->AddDefaultKey
        ("i", "InputFile",
         "Input File Name or '-' for stdin.",
         CArgDescriptions::eInputFile,
         "-");
         
    arg_desc->AddFlag(
        "canonical-name",
        "Use the canonical name which is the name of the format as "
        "given by the underlying C++ format guesser.");
        
    arg_desc->AddFlag(
        "show-object-type",
        "Make output include the type of the object.   If it cannot be "
        "determined or does not make sense for the given format then it "
        "considers it 'unknown'"
    );
    
    arg_desc->AddDefaultKey(
        "output-format", "OutputFormat",
        "How this program should send the results of its guesses.",
        CArgDescriptions::eString,
        "text"
    );
    arg_desc->SetConstraint("output-format", &(*new CArgAllow_Strings,
                                               "text", "XML"));
    

    SetupArgDescriptions(arg_desc.release());
}

//  ============================================================================
int 
CFormatGuessApp::Run(void)
//  ============================================================================
{
    const CArgs& args = GetArgs();
    CNcbiIstream & input_stream = args["i"].AsInputFile(CArgValue::fBinary);
    string name_of_input_stream = args["i"].AsString();
    if( name_of_input_stream.empty() || name_of_input_stream == "-" ) {
        name_of_input_stream = "stdin";
    }

    CFormatGuess guesser( input_stream );
    CFormatGuess::EFormat uFormat = guesser.GuessFormat();
    
    string format_name;
    if( args["canonical-name"] ) {
        // caller wants to always use the format-guesser's name
        format_name = CFormatGuess::GetFormatName(uFormat);
    } else {
        // caller wants special names for some types
        FormatMap FormatStrings;
        FormatStrings[ CFormatGuess::eUnknown ] = "Format not recognized";
        FormatStrings[ CFormatGuess::eBinaryASN ] = "Binary ASN.1";
        FormatStrings[ CFormatGuess::eTextASN ] = "Text ASN.1";
        FormatStrings[ CFormatGuess::eFasta ] = "FASTA sequence record";
        FormatStrings[ CFormatGuess::eXml ] = "XML";
        FormatStrings[ CFormatGuess::eRmo ] = "RepeatMasker Out";
        FormatStrings[ CFormatGuess::eGlimmer3 ] = "Glimmer3 prediction";
        FormatStrings[ CFormatGuess::ePhrapAce ] = "Phrap ACE assembly file";
        FormatStrings[ CFormatGuess::eGtf ] = "GFF/GTF style annotation";
        FormatStrings[ CFormatGuess::eAgp ] = "AGP format assembly";
        FormatStrings[ CFormatGuess::eNewick ] = "Newick tree";
        FormatStrings[ CFormatGuess::eDistanceMatrix ] = "Distance matrix";
        FormatStrings[ CFormatGuess::eFiveColFeatureTable ] = 
            "Five column feature table";
        FormatStrings[ CFormatGuess::eTaxplot ] = "Tax plot";
        FormatStrings[ CFormatGuess::eTable ] = "Generic table";
        FormatStrings[ CFormatGuess::eAlignment ] = "Text alignment";
        FormatStrings[ CFormatGuess::eFlatFileSequence ] = 
            "Flat file sequence portion";
        FormatStrings[ CFormatGuess::eSnpMarkers ] = "SNP marker flat file";
        FormatStrings[ CFormatGuess::eWiggle ] = "UCSC Wiggle file";
        FormatStrings[ CFormatGuess::eBed ] = "UCSC BED file";
        FormatStrings[ CFormatGuess::eBed15 ] = "UCSC microarray file";
        FormatStrings[ CFormatGuess::eHgvs ] = "HGVS Variation file";
        FormatStrings[ CFormatGuess::eGff2 ] = "GFF2 feature table";
        FormatStrings[ CFormatGuess::eGff3 ] = "GFF3 feature table";
        FormatStrings[ CFormatGuess::eGvf ] = "GVF gene variation data";
        FormatStrings[ CFormatGuess::eVcf ] = "VCF Variant Call Format";
            
        FormatIter it = FormatStrings.find( uFormat );
        if ( it == FormatStrings.end() ) {
            // cout << "Unmapped format [" << uFormat << "]";
            format_name = CFormatGuess::GetFormatName(uFormat);
        }
        else {
            format_name = it->second;
        }
    }
    
    string object_type_to_show;
    if( args["show-object-type"] ) {
        AutoPtr<CObjectIStream> obj_istrm;
        switch( uFormat ) {
            case CFormatGuess::eBinaryASN:
                obj_istrm.reset(
                    new CObjectIStreamAsnBinary(input_stream, eNoOwnership));
                break;
            case CFormatGuess::eTextASN:
                obj_istrm.reset(
                    new CObjectIStreamAsn(input_stream, eNoOwnership));
                break;
            case CFormatGuess::eXml:
                obj_istrm.reset(
                    new CObjectIStreamXml(input_stream, eNoOwnership));
                break;
            case CFormatGuess::eJSON:
                obj_istrm.reset(
                    new CObjectIStreamJson(input_stream, eNoOwnership));
                break;
            default:
                // obj_istrm will be unset
                break;
        }
        
        if( obj_istrm.get() ) {
            object_type_to_show = guess_object_type(*obj_istrm);
        } else {
            object_type_to_show = "unknown";
        }

        // If caller requested the object type then it should be set
        // even if it's unknown
        _ASSERT( ! object_type_to_show.empty() );
    }
    
    const string output_format = args["output-format"].AsString();
    
    if( output_format == "text" ) {
        cout << name_of_input_stream << " :   ";
        
        _ASSERT( ! format_name.empty() ); // should be non-empty even if unknown
        cout << format_name;
        
        // second line is object type line, if applicable.
        if( ! object_type_to_show.empty() ) {
            cout << ", object type:   " << object_type_to_show;
        }
        
        cout << endl;
    } else if( output_format == "XML" ) {
        
        xml::node output_node("formatguess");
        
        // input_stream_node for each input specified. 
        // However, there's currently only one so no loop here yet.
        xml::node input_stream_node("input_stream");
        xml::attributes & stream_attribs = input_stream_node.get_attributes();
        stream_attribs.insert("name", name_of_input_stream.c_str());
        stream_attribs.insert("format_name", format_name.c_str());
         if( ! object_type_to_show.empty() ) {
            stream_attribs.insert("object_type", object_type_to_show.c_str());
         }
        
        output_node.push_back(input_stream_node);
        
        xml::document output_doc(output_node);
        output_doc.save_to_stream(cout);
    } else {
        _TROUBLE;
    }

    return 0;
}

//  ============================================================================
void CFormatGuessApp::Exit(void)
//  ============================================================================
{
    SetDiagStream(0);
}

//  ============================================================================
int main(int argc, const char* argv[])
//  ============================================================================
{
    // Execute main application function
    return CFormatGuessApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

