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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <algo/gnomon/gnomon.hpp>
#include <serial/serial.hpp>
#include <serial/objostr.hpp>


USING_SCOPE(ncbi);
USING_SCOPE(ncbi::objects);

class CLocalFinderApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CLocalFinderApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddKey("input", "InputFile",
                     "File containing FASTA-format sequence",
                     CArgDescriptions::eString);

    arg_desc->AddDefaultKey("from", "From",
                            "From",
                            CArgDescriptions::eInteger,
                            "0");

    arg_desc->AddDefaultKey("to", "To",
                            "To",
                            CArgDescriptions::eInteger,
                            "1000000000");

    arg_desc->AddKey("model", "ModelData",
                     "Model Data",
                     CArgDescriptions::eString);

    arg_desc->AddDefaultKey("ap", "AprioriInfo",
                            "A priori info",
                            CArgDescriptions::eString,
                            "");

    arg_desc->AddDefaultKey("fs", "FrameShifts",
                            "Frame Shifts",
                            CArgDescriptions::eString,
                            "");

    arg_desc->AddFlag("rep", "Repeats");


    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


int CLocalFinderApp::Run(void)
{
    CArgs myargs = GetArgs();

    string file         = myargs["input"].AsString();
    int left            = myargs["from"].AsInteger();
    int right           = myargs["to"].AsInteger();
    string modeldata    = myargs["model"].AsString();
    string apriorifile  = myargs["ap"].AsString();
    string shifts       = myargs["fs"].AsString();
    bool repeats        = myargs["rep"];


    CGnomon gnomon;

    // set our model data file
    gnomon.SetModelData(modeldata);

    //
    // read our sequence data
    //
    {{
         CNcbiIfstream from(file.c_str());
         vector<char> seq;
         string line;
         char c;
         getline(from,line);
         while(from >> c) seq.push_back(c);
         gnomon.SetSequence(seq);
     }}

    // set the a priori information
    gnomon.SetAprioriInfo(apriorifile);

    // set the frame shift information
    gnomon.SetFrameShiftInfo(shifts);

    // set the repeats flag
    gnomon.SetRepeats(repeats);

    // set the scan range
    gnomon.SetScanRange(CRange<TSeqPos>(left, right));

    // run!
    gnomon.Run();

    /**
      int cgcontent = 0;
      int len = seq.size();
      right = min(right,len-1);
      for(int i = left; i <= right; ++i)
      {
      int c = toupper(seq[i]);
      if(c == 'C' || c == 'G') ++cgcontent;
      }
      cgcontent = cgcontent*100./(right-left+1)+0.5;

      double e1 = sw.Elapsed();

      if(debug) cerr << "Input time: " << e1 << endl;

      sw.Start();
    //	MDD_Donor donor(modeldata,cgcontent);
    WAM_Donor<2> donor(modeldata,cgcontent);
    WAM_Acceptor<2> acceptor(modeldata,cgcontent);
    WMM_Start start(modeldata,cgcontent);
    WAM_Stop stop(modeldata,cgcontent);
    MC3_CodingRegion<5> cdr(modeldata,cgcontent);
    MC_NonCodingRegion<5> intron_reg(modeldata,cgcontent), intergenic_reg(modeldata,cgcontent);
    //	NullRegion intron_reg, intergenic_reg;
    Intron::Init(modeldata,cgcontent,right-left+1);
    Intergenic::Init(modeldata,cgcontent,right-left+1);
    Exon::Init(modeldata,cgcontent);
    double e2 = sw.Elapsed();
    if(debug) cerr << "Init time: " << e2 << endl;

    sw.Start();
    bool leftwall = true, rightwall = true;
    SeqScores ss(acceptor, donor, start, stop, cdr, intron_reg, 
    intergenic_reg, seq, left, right, cls, fshifts, repeats, 
    leftwall, rightwall, file);
    HMM_State::SetSeqScores(ss);
    double e3 = sw.Elapsed();
    if(debug) cerr << "Scoring time: " << e3 << endl;

    sw.Start();
    Parse parse(ss);
    double e4 = sw.Elapsed();
    if(debug) cerr << "Parse time: " << e4 << endl;

    sw.Start();
    parse.PrintGenes();
     **/

    // dump the annotation
    CRef<CSeq_annot> annot = gnomon.GetAnnot();
    auto_ptr<CObjectOStream> os(CObjectOStream::Open(eSerial_AsnText, cout));
    *os << *annot;

    return 0;

}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CLocalFinderApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.1  2003/10/24 15:07:25  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
