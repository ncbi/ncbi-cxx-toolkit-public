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
 *   Sample test application for BAM reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <common/test_data_path.h>
#include <sra/readers/ncbi_traces_path.hpp>
#include "bam_test_common.hpp"


USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
//  CBAMTestApp::


#define BAM_DIR1 "/1000genomes/ftp/data"
#define BAM_DIR2 "/1000genomes/ftp/phase1/data"
#define BAM_DIR3 "/1kg_pilot_data/data"
#define BAM_DIR4 "/1000genomes2/ftp/data"
#define BAM_DIR5 "/1000genomes3/ftp/data"
#define BAM_DIR6 "/1000genomes4/ftp/data"
    
#define BAM_FILE1 "NA19240.mapped.SOLID.bfast.YRI.exome.20111114.bam"
#define BAM_FILE2 "NA19240.chromMT.ILLUMINA.bwa.YRI.exon_targetted.20100311.bam"
#define BAM_FILE3 "NA19240.chromMT.SLX.SRP000032.2009_04.bam"
#define BAM_FILE4 "NA19240.chrom3.SLX.SRP000032.2009_04.bam"


void CBAMTestCommon::InitCommonArgs(CArgDescriptions& args,
                                    const char* bam_name)
{
    args.AddOptionalKey("dir", "Dir",
                        "BAM files directory",
                        CArgDescriptions::eString);
    if ( !bam_name ) {
        bam_name = BAM_FILE1;
    }
    args.AddDefaultKey("file", "File",
                       "BAM file name",
                       CArgDescriptions::eString,
                       bam_name);
    args.AddOptionalKey("index", "IndexFile",
                        "Explicit path to BAM index file",
                        CArgDescriptions::eString);
    args.AddFlag("no_index", "Access BAM file without index");
    args.AddFlag("refseq_table", "Dump RefSeq table");
            
    args.AddOptionalKey("mapfile", "MapFile",
                        "IdMapper config filename",
                        CArgDescriptions::eInputFile);
    args.AddDefaultKey("genome", "Genome",
                       "UCSC build number",
                       CArgDescriptions::eString, "");
            
    args.AddOptionalKey("refseq", "RefSeqId",
                        "RefSeq id",
                        CArgDescriptions::eString);
    args.AddDefaultKey("refpos", "RefSeqPos",
                       "RefSeq position",
                       CArgDescriptions::eInteger,
                       "0");
    args.AddDefaultKey("refwindow", "RefSeqWindow",
                       "RefSeq window",
                       CArgDescriptions::eInteger,
                       "0");
    args.AddOptionalKey("refend", "RefSeqEnd",
                        "RefSeq last position",
                        CArgDescriptions::eInteger);
    args.AddOptionalKey("q", "Queries",
                        "Query variants: chr1, chr1:123-432, separate queries by commas",
                        CArgDescriptions::eString);
    args.AddFlag("by-start", "Search by alignment start position only");
    args.AddFlag("verbose", "Verbose program run");
            
    args.AddDefaultKey("o", "OutputFile",
                       "Output file of ASN.1",
                       CArgDescriptions::eOutputFile,
                       "-");
}


bool CBAMTestCommon::ParseCommonArgs(const CArgs& args)
{
    verbose = args["verbose"];
            
    string file = args["file"].AsString();
    
    if ( NStr::StartsWith(file, "http://") ||
         NStr::StartsWith(file, "https://") ||
         NStr::StartsWith(file, "ftp://") ||
         CFile(file).Exists() ) {
        path = file;
    }
    else {
        vector<string> dirs;
        if ( args["dir"] ) {
            dirs.push_back(args["dir"].AsString());
        }
        else {
            vector<string> reps;
            reps.push_back(string(NCBI_GetTestDataPath()));
            reps.push_back(string(NCBI_GetTestDataPath())+"/traces02");
            reps.push_back(string(NCBI_GetTestDataPath())+"/traces04");
            reps.push_back(NCBI_TRACES02_PATH);
            reps.push_back(NCBI_TRACES04_PATH);
            vector<string> subdirs;
            subdirs.push_back("");
            subdirs.push_back(BAM_DIR1);
            subdirs.push_back(BAM_DIR2);
            subdirs.push_back(BAM_DIR3);
            subdirs.push_back(BAM_DIR4);
            subdirs.push_back(BAM_DIR5);
            subdirs.push_back(BAM_DIR6);
            for ( auto& rep : reps ) {
                for ( auto& subdir : subdirs ) {
                    string dir = CFile::MakePath(rep, subdir);
                    if ( CDirEntry(dir).Exists() ) {
                        dirs.push_back(dir);
                    }
                }
            }
        }

        for ( auto& dir : dirs ) {
            path = CFile::MakePath(dir, file);
            if ( CFile(path).Exists() ) {
                break;
            }
            string name = CFile(file).GetName();
            SIZE_TYPE p = name.find('.');
            if ( p != NPOS ) {
                string s = name.substr(0, p);
                path = CFile::MakePath(CFile::MakePath(CFile::MakePath(dir, s), "alignment"), file);
                if ( CFile(path).Exists() ) {
                    break;
                }
                path = CFile::MakePath(CFile::MakePath(CFile::MakePath(dir, s), "exome_alignment"), file);
                if ( CFile(path).Exists() ) {
                    break;
                }
            }
            path.clear();
        }
        if ( path.empty() ) {
            path = file;
        }
        if ( !CFile(path).Exists() ) {
            ERR_POST("Data file "<<args["file"].AsString()<<" not found.");
            return false;
        }
        if ( verbose ) {
            LOG_POST("Testing file: "<<path);
        }
    }

    if ( !args["no_index"] ) {
        if ( args["index"] ) {
            index_path = args["index"].AsString();
        }
        else {
            index_path = path+".bai";
        }
    }
    
    if ( args["mapfile"] ) {
        id_mapper.reset(new CIdMapperConfig(args["mapfile"].AsInputFile(),
                                               args["genome"].AsString(),
                                               false));
    }
    else if ( !args["genome"].AsString().empty() ) {
        LOG_POST("Genome: "<<args["genome"].AsString());
        id_mapper.reset(new CIdMapperBuiltin(args["genome"].AsString(),
                                                false));
    }
    
    refseq_table = args["refseq_table"];

    out.out = &args["o"].AsOutputFile();

    if ( args["refseq"] ) {
        vector<string> refseqs;
        NStr::Split(args["refseq"].AsString(), ",", refseqs);
        for ( auto& refseq : refseqs ) {
            SQuery q;
            q.refseq_id = refseq;
            q.refseq_range.SetFrom(TSeqPos(args["refpos"].AsInteger()));
            if ( args["refwindow"] ) {
                q.refseq_range.SetLength(TSeqPos(args["refwindow"].AsInteger()));
            }
            if ( args["refend"] ) {
                q.refseq_range.SetTo(TSeqPos(args["refend"].AsInteger()));
            }
            if ( q.refseq_range.Empty() ) {
                q.refseq_range.SetTo(kInvalidSeqPos);
            }
            queries.push_back(q);
        }
    }
    else if ( args["q"] ) {
        vector<string> ss;
        NStr::Split(args["q"].AsString(), ",", ss);
        for ( auto s : ss ) {
            SIZE_TYPE colon = s.find(':');
            SQuery q;
            if ( colon == NPOS ) {
                q.refseq_id = s;
                q.refseq_range = CRange<TSeqPos>::GetWhole();
            }
            else {
                q.refseq_id = s.substr(0, colon);
                s = s.substr(colon+1);
                SIZE_TYPE dash = s.find('-');
                if ( dash == NPOS ) {
                    ERR_POST(Fatal<<"Bad query format");
                }
                q.refseq_range.SetFrom(NStr::StringToNumeric<TSeqPos>(s.substr(0, dash)));
                q.refseq_range.SetTo(NStr::StringToNumeric<TSeqPos>(s.substr(dash+1)));
            }
            queries.push_back(q);
        }
    }
    
    by_start = args["by-start"];
    return true;
}
