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
#include <sra/readers/ncbi_traces_path.hpp>

#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CBamGraphTestApp::


class CBamGraphTestApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
                         //  Init test


#define BAM_DIR1 "/1000genomes/ftp/data"
#define BAM_DIR2 "/1kg_pilot_data/data"
#define BAM_DIR3 "/1000genomes2/ftp/data"
#define BAM_DIR4 "/1000genomes2/ftp/phase1/data"

#define BAM_FILE "HG00116.chrom20.ILLUMINA.bwa.GBR.low_coverage.20101123.bam"

void CBamGraphTestApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddOptionalKey("dir", "Dir",
                             "BAM files files directory",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("file", "File",
                            "BAM file name",
                            CArgDescriptions::eString,
                            BAM_FILE);

    arg_desc->AddOptionalKey("ref_label", "RefLabel",
                             "RefSeq id in BAM file",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("seq_id", "SeqId",
                             "RefSeq Seq-id",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("log", "Generate logarithmic graph");
    arg_desc->AddFlag("int", "Generate graph with int values");
    arg_desc->AddOptionalKey("outlier_max", "OutlierMax",
                             "Factor over average to treat as outlier",
                             CArgDescriptions::eDouble);

    arg_desc->AddOptionalKey("bin_size", "BinSize",
                             "Seq-graph bin size",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("title", "Title",
                             "Title of generated Seq-graph",
                             CArgDescriptions::eString);

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output file of ASN.1",
                            CArgDescriptions::eOutputFile,
                            "-");
    arg_desc->AddFlag("bin", "Write binary ASN.1");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////


int CBamGraphTestApp::Run(void)
{
    SetDiagPostLevel(eDiag_Info);
    // Get arguments
    const CArgs& args = GetArgs();

    vector<string> dirs;
    if ( args["dir"] ) {
        dirs.push_back(args["dir"].AsString());
    }
    else {
        vector<string> reps;
        NStr::Tokenize(NCBI_TEST_BAM_FILE_PATH, ":", reps);
        ITERATE ( vector<string>, it, reps ) {
            dirs.push_back(CFile::MakePath(*it, BAM_DIR1));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR2));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR3));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR4));
        }
    }

    string file = args["file"].AsString();
    string path;
    
    ITERATE ( vector<string>, it, dirs ) {
        string dir = *it;
        if ( !CDirEntry(dir).Exists() ) {
            continue;
        }
        path = CFile::MakePath(dir, file);
        if ( !CFile(path).Exists() ) {
            SIZE_TYPE p1 = file.rfind('/');
            if ( p1 == NPOS ) {
                p1 = 0;
            }
            else {
                p1 += 1;
            }
            SIZE_TYPE p2 = file.find('.', p1);
            if ( p2 != NPOS ) {
                path = CFile::MakePath(dir, file.substr(p1, p2-p1) + "/alignment/" + file);
            }
        }
        if ( CFile(path).Exists() ) {
            break;
        }
        path.clear();
    }
    if ( !CFile(path).Exists() ) {
        ERR_POST("Data file "<<args["file"].AsString()<<" not found.");
        return 1;
    }

    string ref_label;
    if ( args["ref_label"] ) {
        ref_label = args["ref_label"].AsString();
    }
    else {
        SIZE_TYPE p1 = file.rfind('/');
        if ( p1 == NPOS ) {
            p1 = 0;
        }
        else {
            p1 += 1;
        }
        vector<string> tt;
        NStr::Tokenize(file.substr(p1), ".", tt);
        if ( tt.size() > 2 ) {
            ref_label = tt[1];
        }
        else {
            ERR_POST(Fatal<<"Cannot determine RefSeq label");
        }
    }
    CBamMgr mgr;
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
    if ( args["log"] ) {
        cvt.SetGraphType(cvt.eGraphType_logarithmic);
    }
    if ( args["int"] ) {
        cvt.SetGraphValueType(cvt.eGraphValueTyps_int);
    }
    if ( args["bin_size"] ) {
        cvt.SetGraphBinSize(args["bin_size"].AsInteger());
    }
    if ( args["outlier_max"] ) {
        cvt.SetOutlierMax(args["outlier_max"].AsDouble());
    }

    CRef<CSeq_entry> entry = cvt.MakeSeq_entry(mgr, path);

    CNcbiOstream& out = args["o"].AsOutputFile();
    if ( args["bin"] )
        out << MSerial_AsnBinary;
    else
        out << MSerial_AsnText;
    out << *entry;

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CBamGraphTestApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBamGraphTestApp().AppMain(argc, argv);
}
