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
* Author:  Andrei Gourianov
*
* File Description:
*   Eead/write CUser_field object using C++ or C toolkit
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serial.hpp>

#include <serial/objostrxml.hpp>
#include <serial/objostrjson.hpp>

#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>

#include <objgen.h>


USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CTestApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);
};


void CTestApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test of CUser_field serialization");
    
    arg_desc->AddKey("in",   "Inputfile",  "Input data file",  CArgDescriptions::eInputFile);
    arg_desc->AddKey("infmt","Inputformat","Input data file format", CArgDescriptions::eString);
    arg_desc->SetConstraint( "infmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml", "json"));

    arg_desc->AddKey("out",   "Outputfile",  "Output data file", CArgDescriptions::eOutputFile);
    arg_desc->AddKey("outfmt","Outputformat","Output data file format", CArgDescriptions::eString);
    arg_desc->SetConstraint( "outfmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml", "json"));

    arg_desc->AddDefaultKey("mode", "Mode", "Serialization mode: read/write or copy", CArgDescriptions::eString, "rw");
    arg_desc->SetConstraint( "mode", &(*new CArgAllow_Strings, "rw", "copy"));

    arg_desc->AddDefaultKey("toolkit", "Toolkit", "Which toolkit to use", CArgDescriptions::eString, "cpp");
    arg_desc->SetConstraint( "toolkit", &(*new CArgAllow_Strings, "cpp", "c"));

    SetupArgDescriptions(arg_desc.release());
}


int CTestApp::Run(void)
{
    const CArgs& args = GetArgs();
    string inFmt   = args["infmt"].AsString();
    string outFmt  = args["outfmt"].AsString();
    string mode    = args["mode"].AsString();
    string toolkit = args["toolkit"].AsString();

    ESerialDataFormat inFormat = eSerial_AsnText;
    if (inFmt == "asn") {
        inFormat = eSerial_AsnText;
    } else if (inFmt == "asnb") {
        inFormat = eSerial_AsnBinary;
    } else if (inFmt == "xml") {
        inFormat = eSerial_Xml;
    } else if (inFmt == "json") {
        inFormat = eSerial_Json;
    }
    ESerialDataFormat outFormat = eSerial_AsnText;
    if (outFmt == "asn") {
        outFormat = eSerial_AsnText;
    } else if (outFmt == "asnb") {
        outFormat = eSerial_AsnBinary;
    } else if (outFmt == "xml") {
        outFormat = eSerial_Xml;
    } else if (outFmt == "json") {
        outFormat = eSerial_Json;
    }

    if (toolkit == "cpp") {
        CNcbiIstream& inFile  = args["in"].AsInputFile();
        CNcbiOstream& outFile = args["out"].AsOutputFile();
        auto_ptr<CObjectIStream> in( CObjectIStream::Open( inFormat,  inFile));
        auto_ptr<CObjectOStream> out(CObjectOStream::Open(outFormat, outFile));
#if 0
    {
        CUser_field  sample;
        sample.SetLabel().SetStr("object label");
        sample.SetData().SetStrs().push_back( CUtf8::AsUTF8( L"strs one"));
        sample.SetData().SetStrs().push_back( CUtf8::AsUTF8( L"strs two"));
        sample.SetData().SetStrs().push_back( CUtf8::AsUTF8( L"здравствуйте"));
        sample.SetData().SetStrs().push_back( CUtf8::AsUTF8( L"早安"));
        sample.SetNum( sample.GetData().GetStrs().size());
        if (outFormat == eSerial_Json) {
            ((CObjectOStreamJson*)(out.get()))->SetDefaultStringEncoding(eEncoding_UTF8);
        } else if (outFormat == eSerial_Xml) {
            ((CObjectOStreamXml*)(out.get()))->SetDefaultStringEncoding(eEncoding_UTF8);
        }
        *out << sample;
    }
#else
        if (mode == "copy") {
            CObjectStreamCopier copier(*in, *out);
            copier.Copy(CUser_field::GetTypeInfo());
        }
        else {
            CUser_field  obj;
            *in >> obj;
            *out << obj;
        }
#endif
    } else { // c

        string file_in = args["in"].AsString();
        string file_out = args["out"].AsString();

        const char *fmt_in = (inFormat == eSerial_AsnBinary) ? "rb" : "r";
        AsnIoPtr aip = AsnIoOpen( const_cast<char *>(file_in.c_str()), const_cast<char *>(fmt_in));
        UserFieldPtr obj = UserFieldAsnRead(aip, NULL);
        aip = AsnIoClose(aip);

        const char *fmt_out = (outFormat == eSerial_AsnBinary) ? "wb" : "w";
        AsnIoPtr aip_out = AsnIoOpen( const_cast<char *>(file_out.c_str()), const_cast<char *>(fmt_out));
        UserFieldAsnWrite(obj,aip_out, NULL);
        aip_out = AsnIoClose(aip_out);
        UserFieldFree(obj);
    }
    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestApp().AppMain(argc, argv);
}
