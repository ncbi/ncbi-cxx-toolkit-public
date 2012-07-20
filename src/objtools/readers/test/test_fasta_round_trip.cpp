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
*   Test round-trip FASTA <-> ASN.1 conversion
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>

#include <objmgr/util/sequence.hpp>
#include <objtools/readers/fasta.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CFastaRoundTripTestApp : public CNcbiApplication
{
    void Init(void);
    int  Run(void);
};

void CFastaRoundTripTestApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test round-trip FASTA <-> ASN.1 conversion",
                              false);
    arg_desc->AddKey("in", "InputFile", "Input FASTA file to read",
                     CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("inflags", "Flags", "Flags for CFastaReader",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddOptionalKey("outflags", "Flags", "Flags for CFastaOstream",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("gapmode", "Mode", "CFastaOstream gap mode",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey
        ("expected", "InputFile",
         "Expected round-trip FASTA output, if different from original input",
         CArgDescriptions::eInputFile);

    SetupArgDescriptions(arg_desc.release());
}

static
auto_ptr<CFastaOstream> s_GetFastaOstream(CNcbiOstream& os, const CArgs& args)
{
    auto_ptr<CFastaOstream> out(new CFastaOstream(os));
    if (args["outflags"]) {
        out->SetAllFlags(args["outflags"].AsInteger());
    }
    if (args["gapmode"]) {
        out->SetGapMode(static_cast<CFastaOstream::EGapMode>
                        (args["gapmode"].AsInteger()));
    }
    return out;
}

int CFastaRoundTripTestApp::Run(void)
{
    const CArgs&          args = GetArgs();
    auto_ptr<CMemoryFile> mf(new CMemoryFile(args["in"].AsString()));
    CRef<CSeq_entry>      se, se2;
    CRef<CSeq_loc>        mask;
    string                str;
    int                   status = 0;

    {{
        CMemoryLineReader    lr(mf.get());
        CFastaReader         reader(lr, args["inflags"].AsInteger());
        CFastaReader::TMasks masks;
        reader.SaveMasks(&masks);
        se = reader.ReadSet();
        ITERATE (CFastaReader::TMasks, it, masks) {
            if ((*it)->IsNull()) {
                continue;
            } else if (mask.Empty()) {
                mask = *it;
            } else {
                mask->Add(**it);
            }
        }
    }}

    {{
        CNcbiOstrstream         oss;
        auto_ptr<CFastaOstream> fos(s_GetFastaOstream(oss, args));
        fos->SetMask(CFastaOstream::eSoftMask, mask);
        fos->Write(*se);
        str = CNcbiOstrstreamToString(oss);
    }}

    if (args["expected"]) {
        mf.reset(new CMemoryFile(args["expected"].AsString()));        
    }
    {{
        const void *p = mf->Map();
        CTempString ts(static_cast<const char*>(p), mf->GetSize());
        if (ts != str) {
            ERR_POST("FASTA discrepancy: expected\n"
                     << ts << "but got\n" << str);
            status |= 1;
        }
    }}

    {{
        CMemoryLineReader    lr(str.data(), str.size());
        CFastaReader         reader(lr, args["inflags"].AsInteger());
        se2 = reader.ReadSet();
    }}

    if (!se2->Equals(*se)) {
        ERR_POST("Seq-entry discrepancy: got first\n"
                 << MSerial_AsnText << *se << "then\n" << *se2);
        status |= 2;
    }

    return status;
}

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CFastaRoundTripTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
