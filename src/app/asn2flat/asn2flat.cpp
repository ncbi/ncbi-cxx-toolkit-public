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
* Author:  Aaron Ucko, Mati Shomrat, NCBI
*
* File Description:
*   flat-file generator application
*
* ===========================================================================
*/
#include <corelib/ncbiapp.hpp>

#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/flat_expt.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CAsn2FlatApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run (void);

private:
    CObjectIStream* x_OpenIStream(const CArgs& args);

    CFlatFileGenerator* x_CreateFlatFileGenerator(CScope& scope, 
        const CArgs& args);
    TFormat x_GetFormat(const CArgs& args);
    TMode   x_GetMode(const CArgs& args);
    TStyle  x_GetStyle(const CArgs& args);
    TFlags  x_GetFlags(const CArgs& args);
    TFilter x_GetFilter(const CArgs& args);
};


void CAsn2FlatApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Convert an ASN.1 Seq-entry into a flat report",
        false);
    
    // input
    {{
        // name
        arg_desc->AddOptionalKey("i", "InputFile", 
            "Input file name", CArgDescriptions::eInputFile);
        
        // input file serial format (AsnText\AsnBinary\XML, default: AsnText)
        arg_desc->AddDefaultKey("serial", "SerialFormat", "Input file format",
            CArgDescriptions::eString, "text");
        arg_desc->SetConstraint("serial", &(*new CArgAllow_Strings,
            "text", "binary", "XML"));
        
        // compression
        arg_desc->AddFlag("c", "Compressed file");
        
        // batch processing
        arg_desc->AddOptionalKey("batch", "BatchMode",
            "Process NCBI release file", CArgDescriptions::eString);
    }}
    
    // output
    {{ 
        // name
        arg_desc->AddOptionalKey("o", "OutputFile", 
            "Output file name", CArgDescriptions::eOutputFile);
    }}
    
    // report
    {{
        // format (default: genbank)
        arg_desc->AddDefaultKey("format", "Format", "Output format",
            CArgDescriptions::eString, "genbank");
        arg_desc->SetConstraint("format", &(*new CArgAllow_Strings,
            "genbank", "embl", "ddbj", "gbseq", "ftable", "gff"));

        // mode (default: dump)
        arg_desc->AddDefaultKey("mode", "Mode", "Restriction level",
            CArgDescriptions::eString, "gbench");
        arg_desc->SetConstraint("mode", 
            &(*new CArgAllow_Strings, "release", "entrez", "gbench", "dump"));

        // style (default: normal)
        arg_desc->AddDefaultKey("style", "Style", "Formatting style",
            CArgDescriptions::eString, "normal");
        arg_desc->SetConstraint("style", 
            &(*new CArgAllow_Strings, "normal", "segment", "master", "contig"));

        // flags (default: 0)
        arg_desc->AddDefaultKey("flags", "Flags", "Custom flags",
            CArgDescriptions::eInteger, "0");

        // view (default: nucleotide)
        arg_desc->AddDefaultKey("view", "View", "View",
            CArgDescriptions::eString, "nuc");
        arg_desc->SetConstraint("view", 
            &(*new CArgAllow_Strings, "all", "prot", "nuc"));
        
        // propagate top descriptors
        
        // remote fetching
        
        // from
        arg_desc->AddOptionalKey("from", "From", 
            "Begining of shown range", CArgDescriptions::eInteger);
        
        // to
        arg_desc->AddOptionalKey("to", "To", 
            "End of shown range", CArgDescriptions::eInteger);
        
        // strand
        
        // accession to extract
    }}
    
    SetupArgDescriptions(arg_desc.release());
}


int CAsn2FlatApp::Run(void)
{
    CRef<CObjectManager> objmgr(new CObjectManager);
    if ( !objmgr ) {
        NCBI_THROW(CFlatException, eInternal, "Could not create object manager");
    }
    objmgr->RegisterDataLoader(*new CGBDataLoader("ID"),
                                 CObjectManager::eDefault);

    CRef<CScope> scope(new CScope(*objmgr));
    if ( !scope) {
        NCBI_THROW(CFlatException, eInternal, "Could not create scope");
    }
    scope->AddDefaults();
    
    const CArgs&   args = GetArgs();

    // open the input file (default: stdin)
    auto_ptr<CObjectIStream> in(x_OpenIStream(args));

    // read in the seq-entry
    // !!! differ batch processing handling 
    CRef<CSeq_entry> se(new CSeq_entry);
    in->Read(ObjectInfo(*se));

    // add entry to scope    
    scope->AddTopLevelSeqEntry(*se);

    // open the output stream
    CNcbiOstream* os = args["o"] ? &(args["o"].AsOutputFile()) : &cout;

    // create the flat-file generator
    CRef<CFlatFileGenerator> ffg(x_CreateFlatFileGenerator(*scope, args));
    
    // generate flat file
    ffg->Generate(*se, *os);

    return 0;
}


CObjectIStream* CAsn2FlatApp::x_OpenIStream(const CArgs& args)
{
    // file format
    ESerialDataFormat format = eSerial_AsnText;
    if ( args["serial"] ) {
        string serial = args["serial"].AsString();
        if ( serial == "text" ) {
            format = eSerial_AsnText;
        } else if ( serial == "binary" ) {
            format = eSerial_AsnBinary;
        } else if ( serial == "XML" ) {
            format = eSerial_Xml;
        }
    }

    if ( args["i"] ) {
        return CObjectIStream::Open(args["i"].AsString(), format);
    } else {
        return CObjectIStream::Open(format, cin);
    }

    return 0;
}


CFlatFileGenerator* CAsn2FlatApp::x_CreateFlatFileGenerator
(CScope& scope,
 const CArgs& args)
{
    // !!! need to get format, style. mode etc. currently using defaults
    TFormat format = x_GetFormat(args);
    TMode   mode   = x_GetMode(args);
    TStyle  style  = x_GetStyle(args);
    TFlags  flags  = x_GetFlags(args);
    TFilter filter = x_GetFilter(args);

    return new CFlatFileGenerator(scope, format, mode, style, filter, flags);
}


TFormat CAsn2FlatApp::x_GetFormat(const CArgs& args)
{
    const string& format = args["format"].AsString();
    if ( format == "genbank" ) {
        return eFormat_GenBank;
    } else if ( format == "embl" ) {
        return eFormat_EMBL;
    } else if ( format == "ddbj" ) {
        return eFormat_DDBJ;
    } else if ( format == "gbseq" ) {
        return eFormat_GBSeq;
    } else if ( format == "ftable" ) {
        return eFormat_FTable;
    } else if ( format == "gff" ) {
        return eFormat_GFF;
    }

    return eFormat_GenBank;
}


TMode CAsn2FlatApp::x_GetMode(const CArgs& args)
{
    const string& mode = args["mode"].AsString();
    if ( mode == "release" ) {
        return eMode_Release;
    } else if ( mode == "entrez" ) {
        return eMode_Entrez;
    } else if ( mode == "gbench" ) {
        return eMode_GBench;
    } else if ( mode == "dump" ) {
        return eMode_Dump;
    } 

    return eMode_GBench;
}


TStyle CAsn2FlatApp::x_GetStyle(const CArgs& args)
{
    const string& style = args["style"].AsString();
    if ( style == "normal" ) {
        return eStyle_Normal;
    } else if ( style == "segment" ) {
        return eStyle_Segment;
    } else if ( style == "master" ) {
        return eStyle_Master;
    } else if ( style == "contig" ) {
        return eStyle_Contig;
    } 

    return eStyle_Normal;
}


TFlags CAsn2FlatApp::x_GetFlags(const CArgs& args)
{
    return args["flags"].AsInteger();
}


TFilter CAsn2FlatApp::x_GetFilter(const CArgs& args)
{
    const string& view = args["view"].AsString();
    if ( view == "all" ) {
        return fSkipNone;
    } else if ( view == "prot" ) {
        return fSkipNucleotides;
    } else if ( view == "nuc" ) {
        return fSkipProteins;
    } 

    return fSkipProteins;
}


END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char** argv)
{
    return CAsn2FlatApp().AppMain(argc, argv);
}

/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2004/03/05 18:51:45  shomrat
* use view instead of filter
*
* Revision 1.3  2004/02/11 22:58:26  shomrat
* using flags from flag file
*
* Revision 1.2  2004/01/20 20:41:53  ucko
* Don't try to return auto_ptrs, or construct them with "=".
*
* Revision 1.1  2004/01/14 15:03:29  shomrat
* Initial Revision
*
*
* ===========================================================================
*/
