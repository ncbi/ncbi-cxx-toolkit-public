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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Sample test application for BAM coverage generator
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <sra/readers/bam/bamgraph.hpp>
#include <sra/readers/bam/bamread.hpp>
#include <sra/readers/bam/bamindex.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <serial/serial.hpp>
#include <serial/objostr.hpp>

#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CBam2GraphApp::


class CBam2GraphApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    void ProcessFile(const string& file);
    void ProcessSrz(string srz_name);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test


#define BAM_DIR "1000genomes/ftp/data"
#define CONFIG_FILE_NAME "analysis.bam.cfg"

void CBam2GraphApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddOptionalKey("srz", "srz",
                             "SRZ accession config file"
                             " (-log and -int options are ignored)",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("file", "File",
                             "BAM file name",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("dir", "Dir",
                             "BAM files files directory",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("ref_label", "RefLabel",
                             "RefSeq id in BAM file",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("seq_id", "SeqId",
                             "RefSeq Seq-id",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("delta", "Delta",
                             "Delta-ext in text ASN.1",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("delta_file", "DeltaFile",
                             "File with Delta-ext in text ASN.1",
                             CArgDescriptions::eInputFile);

    arg_desc->AddFlag("estimated", "Make estimated graph using index only");
    arg_desc->AddFlag("raw-access", "Make graph using raw access to BAM");
    arg_desc->AddOptionalKey("min_quality", "MinMapQuality",
                             "Minimal alignment map quality",
                             CArgDescriptions::eInteger);
    arg_desc->AddFlag("log", "Generate logarithmic graph");
    arg_desc->AddFlag("int", "Generate graph with int values");
    arg_desc->AddOptionalKey("max", "OutlierMax",
                             "Factor over average to treat as outlier",
                             CArgDescriptions::eDouble);
    arg_desc->AddFlag("max_details", "Include detailed outlier info");
    arg_desc->AddOptionalKey("bin_size", "BinSize",
                             "Seq-graph bin size",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("annot_name", "annot_name",
                             "Annot name with generated Seq-graph",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("title", "Title",
                             "Title of generated Seq-graph",
                             CArgDescriptions::eString);

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output file of ASN.1",
                            CArgDescriptions::eOutputFile,
                            "-");
    arg_desc->AddFlag("b", "Write binary ASN.1");
    arg_desc->AddFlag("annot", "Write Seq-annot only");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////


int CBam2GraphApp::Run(void)
{
    SetDiagPostLevel(eDiag_Info);
    // Get arguments
    const CArgs& args = GetArgs();

    if ( args["file"] ) {
        ProcessFile(args["file"].AsString());
    }
    else if ( args["srz"] ) {
        ProcessSrz(args["srz"].AsString());
    }
    return 0;
}


void CBam2GraphApp::ProcessFile(const string& file)
{
    const CArgs& args = GetArgs();

    string path;
    if ( NStr::StartsWith(file, "http://") ||
         NStr::StartsWith(file, "https://") ||
         NStr::StartsWith(file, "ftp://") ||
         CFile(file).Exists() ) {
        path = file;
    }
    else {
        string dir;
        if ( args["dir"] ) {
            dir = args["dir"].AsString();
        }
        else {
            vector<string> reps;
            NStr::Split(NCBI_TEST_BAM_FILE_PATH, ":", reps);
            ITERATE ( vector<string>, it, reps ) {
                string path = CFile::MakePath(*it, BAM_DIR);
                if ( !CDirEntry(dir).Exists() ) {
                    dir = path;
                    break;
                }
            }
        }

        path = CFile::MakePath(dir, file);
        if ( !CFile(path).Exists() ) {
            vector<string> tt;
            NStr::Split(CFile(file).GetBase(), ".", tt);
            if ( tt.size() > 0 ) {
                path = CFile::MakePath(dir, tt[0]);
                path = CFile::MakePath(path, "alignment");
                path = CFile::MakePath(path, file);
            }
        }
    }

    string ref_label;
    if ( args["ref_label"] ) {
        ref_label = args["ref_label"].AsString();
    }
    else {
        vector<string> tt;
        NStr::Split(CFile(file).GetBase(), ".", tt);
        if ( tt.size() > 1 ) {
            ref_label = tt[1];
        }
        else {
            ERR_POST(Fatal<<"Cannot determine RefSeq label");
        }
    }

    CBam2Seq_graph cvt;
    cvt.SetRefLabel(ref_label);
    if ( args["seq_id"] ) {
        CSeq_id seq_id(args["seq_id"].AsString());
        cvt.SetRefId(seq_id);
    }
    else {
        CSeq_id seq_id(CSeq_id::e_Local, ref_label);
        cvt.SetRefId(seq_id);
    }
    if ( args["annot_name"] ) {
        cvt.SetAnnotName(args["annot_name"].AsString());
    }
    if ( args["title"] ) {
        cvt.SetGraphTitle(args["title"].AsString());
    }
    if ( args["min_quality"] ) {
        cvt.SetMinMapQuality(args["min_quality"].AsInteger());
    }
    if ( args["log"] ) {
        cvt.SetGraphType(cvt.eGraphType_logarithmic);
    }
    if ( args["int"] ) {
        cvt.SetGraphValueType(cvt.eGraphValueTyps_int);
    }
    if ( args["bin_size"] ) {
        cvt.SetGraphBinSize(args["bin_size"].AsInteger());
    }
    if ( args["max"] ) {
        cvt.SetOutlierMax(args["max"].AsDouble());
    }
    if ( args["max_details"] ) {
        cvt.SetOutlierDetails();
    }
    if ( args["estimated"] ) {
        cvt.SetEstimated();
    }
    if ( args["raw-access"] ) {
        cvt.SetRawAccess();
    }
    CRef<CDelta_ext> delta;
    if ( args["delta"] ) {
        delta = new CDelta_ext;
        string s = args["delta"].AsString();
        if ( !NStr::StartsWith(s, "Delta-ext") ) {
            s = "Delta-ext ::= "+s;
        }
        CNcbiIstrstream in(s.data(), s.size());
        in >> MSerial_AsnText >> *delta;
    }
    else if ( args["delta_file"] ) {
        delta = new CDelta_ext;
        args["delta_file"].AsInputFile() >> MSerial_AsnText >> *delta;
    }
    if ( delta ) {
        CRef<CSeq_inst> inst(new CSeq_inst);
        inst->SetRepr(CSeq_inst::eRepr_delta);
        inst->SetMol(CSeq_inst::eMol_na);
        inst->SetExt().SetDelta(*delta);
        cvt.SetSeq_inst(inst);
    }

    CRef<CSeq_entry> entry;

    if ( 0 && args["estimated"] ) {
        // faster estimated graph from index only
        CBamHeader header(path);
        CBamIndex index(path+".bai");
        CRef<CSeq_annot> annot =
            index.MakeEstimatedCoverageAnnot(header, ref_label,
                                             cvt.GetRefId(),
                                             cvt.GetAnnotName());
        entry = new CSeq_entry;
        entry->SetSet().SetSeq_set();
        entry->SetSet().SetAnnot().push_back(annot);
    }
    else {
        CBamMgr mgr;
        entry = cvt.MakeSeq_entry(mgr, path);
    }

    CNcbiOstream& out = args["o"].AsOutputFile();
    if ( args["b"] )
        out << MSerial_AsnBinary;
    else
        out << MSerial_AsnText;
    if ( args["annot"] ) {
        out << *entry->GetAnnot().front();
    }
    else {
        out << *entry;
    }
}


void CBam2GraphApp::ProcessSrz(string srz_name)
{
    const CArgs& args = GetArgs();
    
    if ( CFile(srz_name).IsDir() ) {
        srz_name = CFile::MakePath(srz_name, CONFIG_FILE_NAME);
    }
    string dir;
    if ( args["dir"] ) {
        dir = args["dir"].AsString();
    }
    else {
        dir = CFile(srz_name).GetDir();
    }
    
    CBamMgr mgr;
    CBamDb db;
    CBamHeader header;
    CBamIndex index;
    string db_name;

    vector<string> tokens;
    string line;
    CNcbiIfstream srz(srz_name.c_str());
    while ( getline(srz, line) ) {
        tokens.clear();
        NStr::Split(line, "\t", tokens);
        if ( tokens.size() < 4 ) {
            ERR_POST(Fatal<<"Bad def line: \""<<line<<"\"");
        }
        string ref_label = tokens[0];
        string acc = tokens[1];
        CRef<CSeq_id> id;
        if ( tokens[2].empty() ) {
            id = new CSeq_id(acc);
        }
        else {
            id = new CSeq_id(CSeq_id::e_Gi, tokens[2]);
        }
        string bam_name = tokens[3];
        if ( tokens.size() < 5 ) {
            ERR_POST("No coverage requested for "<<ref_label);
            continue;
        }
        string bam_path = CFile::MakePath(dir, bam_name);
        string out_name = tokens[4];
        string out_path = CFile::MakePath(dir, out_name);
        
        LOG_POST("Processing "<<ref_label<<" -> "<<out_path);

        CBam2Seq_graph cvt;
        cvt.SetRefLabel(ref_label);
        cvt.SetRefId(*id);
        if ( args["bin_size"] ) {
            cvt.SetGraphBinSize(args["bin_size"].AsInteger());
        }
        if ( args["annot_name"] ) {
            cvt.SetAnnotName(args["annot_name"].AsString());
        }
        else {
            cvt.SetAnnotName(CFile(bam_name).GetBase());
        }
        if ( args["title"] ) {
            cvt.SetGraphTitle(args["title"].AsString());
        }
        else {
            cvt.SetGraphTitle(bam_name+" "+ref_label+" coverage");
        }
        if ( args["min_quality"] ) {
            cvt.SetMinMapQuality(args["min_quality"].AsInteger());
        }
        cvt.SetOutlierDetails();
        if ( args["estimated"] ) {
            cvt.SetEstimated();
        }
        
        CRef<CSeq_entry> entry;
        if ( 0 && args["estimated"] ) {
            // faster estimated graph from index only
            if ( bam_path != db_name ) {
                db_name = bam_path;
                header.Read(bam_path);
                index.Read(bam_path+".bai");
            }
            CRef<CSeq_annot> annot =
                index.MakeEstimatedCoverageAnnot(header, ref_label,
                                                 cvt.GetRefId(),
                                                 cvt.GetAnnotName());
            entry = new CSeq_entry;
            entry->SetSet().SetSeq_set();
            entry->SetSet().SetAnnot().push_back(annot);
        }
        else {
            if ( bam_path != db_name ) {
                db_name = bam_path;
                db = CBamDb(mgr, bam_path, bam_path+".bai");
            }
            entry = cvt.MakeSeq_entry(db, bam_path);
        }

        AutoPtr<CObjectOStream> out
            (CObjectOStream::Open(eSerial_AsnBinary, out_path));
        *out << *entry;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CBam2GraphApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBam2GraphApp().AppMain(argc, argv);
}
