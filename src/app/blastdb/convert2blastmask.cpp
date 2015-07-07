/*  # $Id$
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
 * Author:  Ning Ma
 *
 */

/** @file convert2blastmask.cpp
 * extracts mask info from lower case masked FASTA file in ASN or XML formats 
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/seqmasks_io/mask_writer_blastdb_maskinfo.hpp>

#include "../blast/blast_app_util.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);
#endif /* SKIP_DOXYGEN_PROCESSING */

//-----------------------------------------------------------
// A hacked fasta file reader that extracts mask info
class CMaskFromFasta : public CFastaReader {
private:
    bool m_hasMask;
    CMaskWriter::TMaskList m_mask;
    TSeqPos m_from;

public:
    CMaskFromFasta(CNcbiIstream & input, bool parse_seqids)
        : CFastaReader(input, (parse_seqids ? 0 : CFastaReader::fNoParseID)) {}

    bool HasMask() const {
        return m_hasMask;
    }

    const CMaskWriter::TMaskList & GetMask() const {
        return m_mask;
    }

    bool GetNextSequence() {
        m_hasMask = false;
        m_mask.clear();
        if (AtEOF()) return false;
        SaveMask();
        ReadOneSeq();
        return true;
    }

    // hack to deal with interval format
    virtual void ParseDataLine(const TStr &s, 
        ILineErrorListener * pMessageListener) 
    {
        if (s[0] >= '0' && s[0] <= '9' && s.find('-') > 0) {
            string s1, s2;
            NStr::SplitInTwo(s,"-",s1,s2);
            m_hasMask = true;
            m_mask.push_back(CMaskWriter::TMaskedInterval(
                                 NStr::StringToUInt(NStr::TruncateSpaces(s1)),
                                 NStr::StringToUInt(NStr::TruncateSpaces(s2))));
            // fake a sequence data to make CFastaReader happy
            CFastaReader::ParseDataLine("A", pMessageListener);
        } else {
            CFastaReader::ParseDataLine(s, pMessageListener);
        }
    }
            
    // hack to deal with fasta format
    virtual void x_OpenMask(void) {
        CFastaReader::x_OpenMask();
        m_from = GetCurrentPos(ePosWithGapsAndSegs);
    }

    virtual void x_CloseMask(void) {
        CFastaReader::x_CloseMask();
        m_hasMask = true;
        m_mask.push_back(CMaskWriter::TMaskedInterval(m_from,
                                 GetCurrentPos(ePosWithGapsAndSegs)-1));
    }
};   
    

class CConvert2BlastMaskApplication : public CNcbiApplication {
public:
    CConvert2BlastMaskApplication() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
    }

private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    CMaskFromFasta* x_GetReader();
    CMaskWriterBlastDbMaskInfo* x_GetWriter();

    /// Contains the description of this application
    static const char * const USAGE_LINE;
};

const char * const CConvert2BlastMaskApplication::USAGE_LINE 
    = "Convert masking information in lower-case masked FASTA input to file formats suitable for makeblastdb";

void CConvert2BlastMaskApplication::Init(void) {
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              USAGE_LINE);

    arg_desc->AddDefaultKey("in", "input_file_name",
                            "Input file name",
                            CArgDescriptions::eInputFile, "-");

    arg_desc->AddDefaultKey("out", "output_file_name",
                            "Output file name",
                            CArgDescriptions::eOutputFile, "-");

    arg_desc->AddDefaultKey("outfmt", "output_format",
                            "Output file format",
                            CArgDescriptions::eString, "maskinfo_asn1_text");

    CArgAllow_Strings* strings_allowed = new CArgAllow_Strings();
    strings_allowed->Allow("maskinfo_asn1_text");
    strings_allowed->Allow("maskinfo_asn1_bin");
    strings_allowed->Allow("maskinfo_xml");
    strings_allowed->Allow("interval");
    arg_desc->SetConstraint("outfmt", strings_allowed);

    arg_desc->AddFlag      ( "parse_seqids",
                             "Parse Seq-ids in FASTA input", true );

    arg_desc->AddKey       ("masking_algorithm", "mask_program_name",
                            "Masking algorithm name (e.g.: dust, seg, "
                            "windowmasker, repeat). Use 'other' for "
                            "user-defined type",
                            CArgDescriptions::eString);
                            
    arg_desc->AddKey     ("masking_options", "mask_program_options",
                          "Masking algorithm options to create the masked input"
                          " (free text to describe/include (command line) "
                          "options used to create the masking)",
                          CArgDescriptions::eString);
                            
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

CMaskFromFasta*
CConvert2BlastMaskApplication::x_GetReader() {
    const CArgs& args = GetArgs();
    CNcbiIstream& input = args["in"].AsInputFile();
    return(new CMaskFromFasta(input, args["parse_seqids"]));
}

CMaskWriterBlastDbMaskInfo*
CConvert2BlastMaskApplication::x_GetWriter() {
    const CArgs& args = GetArgs();
    const string& format(args["outfmt"].AsString());
    CNcbiOstream& output = args["out"].AsOutputFile();

    string algo=args["masking_algorithm"].AsString();
    NStr::ToLower(algo);
    EBlast_filter_program prog;
    if     (algo == "not_set"     ) prog = eBlast_filter_program_not_set;
    else if(algo == "dust"        ) prog = eBlast_filter_program_dust;
    else if(algo == "seg"         ) prog = eBlast_filter_program_seg;
    else if(algo == "windowmasker") prog = eBlast_filter_program_windowmasker;
    else if(algo == "repeat"      ) prog = eBlast_filter_program_repeat;
    else                            prog = eBlast_filter_program_other;

    return(new CMaskWriterBlastDbMaskInfo(output, format, 0, 
                      prog, args["masking_options"].AsString()));
}

int CConvert2BlastMaskApplication::Run(void) {
    int retval = 0;

    try {
        auto_ptr<CMaskFromFasta> reader(x_GetReader());
        auto_ptr<CMaskWriterBlastDbMaskInfo> writer(x_GetWriter());

        while (reader->GetNextSequence()) {
            if(reader->HasMask()) writer->Print(reader->GetBestID(), reader->GetMask());
        }
    } catch (const CException& e) {
        cerr << e.what() << endl;
        retval = 1;
    }
    return retval;
}

void CConvert2BlastMaskApplication::Exit(void)
{
    SetDiagStream(0);
}

#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[])
{
    // Execute main application function
    return CConvert2BlastMaskApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */

