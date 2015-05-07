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
*   Test of <ctools/asn_converter.hpp>
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <ctools/asn_converter.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <serial/serial.hpp>

#include <objsset.h>

#include <common/test_assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CTestAsnConverterApp : public CNcbiApplication
{
public:
    void Init();
    int  Run();
};


void CTestAsnConverterApp::Init()
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Test of converting objects between C and C++ representations.");

    arg_desc->AddDefaultKey
        ("type", "AsnType", "ASN.1 type of test object",
         CArgDescriptions::eString, "Seq-entry");
    arg_desc->SetConstraint
        ("type", &(*new CArgAllow_Strings, "Seq-entry", "Seq-align", "Score"));

    arg_desc->AddDefaultKey
        ("in", "InputFile",
         "name of file to read from (standard input by default)",
         CArgDescriptions::eString, "-");
    arg_desc->AddDefaultKey("infmt", "InputFormat", "format of input file",
                            CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint
        ("infmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml"));

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
        THROW1_TRACE(runtime_error, "Bad serial format name " + name);
    }
}


int CTestAsnConverterApp::Run()
{
    const CArgs& args = GetArgs();
    auto_ptr<CObjectIStream> in;
    if (args["type"].AsString() != "Score") {
        in.reset(CObjectIStream::Open
                 (s_GetFormat(args["infmt"].AsString()),
                  args["in"].AsString(), eSerial_StdWhenDash));
    }

    if (args["type"].AsString() == "Seq-entry") {
        DECLARE_ASN_CONVERTER(CSeq_entry, SeqEntry, convertor);
        CSeq_entry  cpp1, cpp2;
        SeqEntryPtr c1,   c2;
        *in >> cpp1;
        c1 = convertor.ToC(cpp1);
        convertor.FromC(c1, &cpp2);
        c2 = convertor.ToC(cpp2);
        _ASSERT(AsnIoMemComp(c1, c2, (AsnWriteFunc)SeqEntryAsnWrite) == TRUE);
        _ASSERT(cpp1.Equals(cpp2));
    } else if (args["type"].AsString() == "Seq-align") {
        DECLARE_ASN_CONVERTER(CSeq_align, SeqAlign, convertor);
        CSeq_align  cpp1, cpp2;
        SeqAlignPtr c1,   c2;
        *in >> cpp1;
        c1 = convertor.ToC(cpp1);
        convertor.FromC(c1, &cpp2);
        c2 = convertor.ToC(cpp2);
        _ASSERT(AsnIoMemComp(c1, c2, (AsnWriteFunc)SeqAlignAsnWrite) == TRUE);
        _ASSERT(cpp1.Equals(cpp2));
    } else if (args["type"].AsString() == "Score") {
        // special case: treat "in" as a score...
        DECLARE_ASN_CONVERTER(CSeq_align, SeqAlign, convertor);
        CSeq_align  cpp1, cpp2;
        SeqAlignPtr c1,   c2;
        cpp1.SetType(CSeq_align::eType_global);
        {{
            double d = NStr::StringToDouble(args["in"].AsString());
            clog << setprecision(DBL_DIG) << d << endl;
            CRef<CScore> score(new CScore);
            score->SetValue().SetReal(d);
            cpp1.SetScore().push_back(score);
        }}
        {{
            CRef<CStd_seg> dummy_seg(new CStd_seg);
            CRef<CSeq_loc> dummy_loc(new CSeq_loc);
            dummy_loc->SetNull();
            dummy_seg->SetLoc().push_back(dummy_loc);
            cpp1.SetSegs().SetStd().push_back(dummy_seg);
        }}
        c1 = convertor.ToC(cpp1);
        convertor.FromC(c1, &cpp2);
        c2 = convertor.ToC(cpp2);
        clog << cpp1.GetScore().front()->GetValue().GetReal() << ' '
             << c1->score->value.realvalue << ' '
             << cpp2.GetScore().front()->GetValue().GetReal() << ' '
             << c2->score->value.realvalue << endl;
        _ASSERT(AsnIoMemComp(c1, c2, (AsnWriteFunc)SeqAlignAsnWrite) == TRUE);
        _ASSERT(cpp1.Equals(cpp2));
    } else {
        cerr << "Unrecognized ASN.1 type " + args["type"].AsString() << endl;
        return 1;
    }

    cout << "Test succeeded." << endl;
    return 0;
}


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTestAsnConverterApp().AppMain(argc, argv);
}
