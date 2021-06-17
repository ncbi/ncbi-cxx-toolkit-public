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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Tool to convert ACE file to ASN.1 Seq-entry
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

#include <serial/serial.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objtools/readers/phrap.hpp>

using namespace ncbi;
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Phrap reader application
//


class CAce2AsnApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CAce2AsnApp::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddKey("ace", "AceFile", "Source ACE file",
                     CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("asn", "AsnFile",
                             "Output ASN.1 text file",
                             CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey("asnb", "AsnBinFile",
                             "Output ASN.1 binary file",
                             CArgDescriptions::eOutputFile,
                             CArgDescriptions::fBinary);

    arg_desc->AddDefaultKey("no_complement",
                            "fPhrap_NoComplement",
                            "Ignore 'complemented' flags of traces",
                            CArgDescriptions::eBoolean,
                            "f");
    arg_desc->AddDefaultKey("pack_data",
                            "fPhrap_PackSeqData",
                            "Use best coding to pack sequence data",
                            CArgDescriptions::eBoolean,
                            "t");
    arg_desc->AddDefaultKey("gaps",
                            "fPhrap_FeatGaps",
                            "Add features with list of gaps",
                            CArgDescriptions::eBoolean,
                            "t");
    arg_desc->AddDefaultKey("base_segs",
                            "fPhrap_FeatBaseSegs",
                            "Add features with base segments",
                            CArgDescriptions::eBoolean,
                            "t");
    arg_desc->AddDefaultKey("read_locs",
                            "fPhrap_FeatReadLocs",
                            "Add features with padded read starts",
                            CArgDescriptions::eBoolean,
                            "f");
    arg_desc->AddDefaultKey("tags",
                            "fPhrap_FeatTags",
                            "Convert CT and RT tags to features",
                            CArgDescriptions::eBoolean,
                            "t");
    arg_desc->AddDefaultKey("quality",
                            "fPhrap_FeatQuality",
                            "Add quality/alignment features",
                            CArgDescriptions::eBoolean,
                            "t");
    arg_desc->AddDefaultKey("descr",
                            "fPhrap_Descr",
                            "Add descriptors (DS and WA tags)",
                            CArgDescriptions::eBoolean,
                            "t");
    arg_desc->AddDefaultKey("fuzz",
                            "fPhrap_PadsToFuzz",
                            "Add padded coordinates using int-fuzz",
                            CArgDescriptions::eBoolean,
                            "f");
    arg_desc->AddDefaultKey("align", "AlignType",
                            "Alignment type",
                            CArgDescriptions::eString, "optimized");
    arg_desc->SetConstraint("align",
                            &(*new CArgAllow_Strings,
                            "none", "optimized", "all", "pairs"));
    arg_desc->AddDefaultKey("ace_version", "AceVersion",
                            "ACE format version",
                            CArgDescriptions::eString, "auto");
    arg_desc->SetConstraint("ace_version",
                            &(*new CArgAllow_Strings,
                            "auto", "old", "new"));

    string prog_description = "ACE to ASN.1 converter\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


int CAce2AsnApp::Run(void)
{
    // Process command line args:  get GI to load
    const CArgs& args = GetArgs();

    TPhrapReaderFlags flags = 0;


    if ( args["no_complement"].AsBoolean() ) {
        flags |= fPhrap_NoComplement;
    };
    if ( args["pack_data"].AsBoolean() ) {
        flags |= fPhrap_PackSeqData;
    }
    if ( args["gaps"].AsBoolean() ) {
        flags |= fPhrap_FeatGaps;
    }
    if ( args["base_segs"].AsBoolean() ) {
        flags |= fPhrap_FeatBaseSegs;
    }
    if ( args["read_locs"].AsBoolean() ) {
        flags |= fPhrap_FeatReadLocs;
    }
    if ( args["tags"].AsBoolean() ) {
        flags |= fPhrap_FeatTags;
    }
    if ( args["quality"].AsBoolean() ) {
        flags |= fPhrap_FeatQuality;
    }
    if ( args["descr"].AsBoolean() ) {
        flags |= fPhrap_Descr;
    }
    if ( args["fuzz"].AsBoolean() ) {
        flags |= fPhrap_PadsToFuzz;
    }

    if (args["align"].AsString() == "optimized") {
        flags |= fPhrap_AlignOptimized;
    }
    else if (args["align"].AsString() == "all") {
        flags |= fPhrap_AlignAll;
    }
    else if (args["align"].AsString() == "pairs") {
        flags |= fPhrap_AlignPairs;
    }

    if (args["ace_version"].AsString() == "new") {
        flags |= fPhrap_NewVersion;
    }
    if (args["ace_version"].AsString() == "old") {
        flags |= fPhrap_OldVersion;
    }

    NcbiCout << "Reading ACE file..." << NcbiEndl;
    CRef<CSeq_entry> entry = ReadPhrap(args["ace"].AsInputFile(), flags);
    bool to_file = args["asn"]  ||  args["asnb"];
    if ( to_file ) {
        NcbiCout << "Saving Seq-entry..." << NcbiEndl;
        if (args["asn"]) {
            args["asn"].AsOutputFile() << MSerial_AsnText << *entry;
        }
        if (args["asnb"]) {
            args["asnb"].AsOutputFile() << MSerial_AsnBinary << *entry;
        }
    }
    else {
        NcbiCout << MSerial_AsnText << *entry;
    }
    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CAce2AsnApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
