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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   new (early 2003) flat-file generator -- sample application
*
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>

#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/scope.hpp>

#include <objects/flat/flat_gbseq_formatter.hpp>
#include <objects/flat/flat_text_formatter.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CFlatteningApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run (void);
};


void CFlatteningApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Convert an ASN.1 Seq-entry into a flat report",
                              false);

    arg_desc->AddKey("in", "InputFile", "File to read the Seq-entry from",
                     CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey("format", "Format", "Output format",
                            CArgDescriptions::eString, "genbank");

    SetupArgDescriptions(arg_desc.release());
}


int CFlatteningApp::Run(void)
{
    const CArgs&   args = GetArgs();
    CObjectManager objmgr;
    CScope         scope(objmgr);

    CRef<CSeq_entry> entry(new CSeq_entry);
    {{
        auto_ptr<CObjectIStream> in
            (CObjectIStream::Open(args["in"].AsString(), eSerial_AsnText));
        *in >> *entry;
    }}
    scope.AddTopLevelSeqEntry(*entry);

    auto_ptr<CObjectOStream> oos;
    auto_ptr<IFlatFormatter> formatter;
    {{
        const string&         format = args["format"].AsString();
        IFlatFormatter::EMode mode   = IFlatFormatter::eMode_Dump;
        if ( !NStr::CompareNocase(format, "gbseq") ) {
            oos.reset(CObjectOStream::Open(eSerial_Xml, cout));
            formatter.reset(new CFlatGBSeqFormatter(*oos, scope, mode));
        } else {
            IFlatFormatter::EDatabase db;
            if        ( !NStr::CompareNocase(format, "genbank") ) {
                db = IFlatFormatter::eDB_NCBI;
            } else if ( !NStr::CompareNocase(format, "embl") ) {
                db = IFlatFormatter::eDB_EMBL;
            } else if ( !NStr::CompareNocase(format, "ddbj") ) {
                db = IFlatFormatter::eDB_DDBJ;
            } else {
                ERR_POST(Fatal << "Bad output format " << format);
            }
            formatter.reset(CFlatTextFormatter::New
                            (*new CFlatTextOStream(cout), scope, mode, db));
        }
    }}
    formatter->Format(*entry, *formatter);

    return 0;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char** argv)
{
    return CFlatteningApp().AppMain(argc, argv);
}

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
