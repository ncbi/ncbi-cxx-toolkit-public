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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include "GeneFinder.hpp"


USING_SCOPE(ncbi);

class CGnomonApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CGnomonApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddKey("file", "File",
                     "File",
                     CArgDescriptions::eString);

    arg_desc->AddDefaultKey("from", "From",
                            "From",
                            CArgDescriptions::eInteger,
                            "0");

    arg_desc->AddDefaultKey("to", "To",
                            "To",
                            CArgDescriptions::eInteger,
                            "1000000000");

    arg_desc->AddDefaultKey("model", "ModelData",
                            "Model Data",
                            CArgDescriptions::eString,
                            "Human.inp");

    arg_desc->AddDefaultKey("ap", "AprioriInfo",
                            "A priori info",
                            CArgDescriptions::eString,
                            "");

    arg_desc->AddOptionalKey("fs", "FrameShifts",
                             "Frame Shifts",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("rep", "Repeats");
    arg_desc->AddFlag("debug", "Debug");


    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


int CGnomonApp::Run(void)
{
    CArgs myargs = GetArgs();


    string file         = myargs["file"].AsString();
    int left            = myargs["from"].AsInteger();
    int right           = myargs["to"].AsInteger();
    string modeldata    = myargs["model"].AsString();
    string apriorifile  = myargs["ap"].AsString();
    string shifts       = myargs["fs"].AsString();
    bool repeats        = myargs["rep"].AsBoolean();
    bool debug          = myargs["debug"].AsBoolean();

    CStopWatch sw;
    sw.Start();
    ClusterSet cls;
    if(!apriorifile.empty())
    {
        CNcbiIfstream from(apriorifile.c_str());
        if(!from)
        {
            cerr << "Can't find alignments " << apriorifile << '\n';
            exit(1);
        }
        from >> cls;
    }

    FrameShifts fshifts;
    if(!shifts.empty())
    {
        CNcbiIfstream from(shifts.c_str());
        if(!from)
        {
            cerr << "Can't find frameshifts " << shifts << '\n';
            exit(1);
        }
        int loc;
        while(from >> loc)
        {
            char is_i, c = 'N';
            from >> is_i;
            if(is_i == '+') from >> c;
            fshifts.push_back(FrameShiftInfo(loc,is_i == '+',c));
        }
    }

    CNcbiIfstream from(file.c_str());
    CVec seq;
    string line;
    char c;
    getline(from,line);
    while(from >> c) seq.push_back(c);

    int cgcontent = 0;
    int len = seq.size();
    right = min(right,len-1);
    for(int i = left; i <= right; ++i)
    {
        int c = toupper(seq[i]);
        if(c == 'C' || c == 'G') ++cgcontent;
    }
    cgcontent = cgcontent*100./(right-left+1)+0.5;

    cout << "CG-content: " << cgcontent << endl;

    double e1 = sw.Elapsed();
    if(debug) cerr << "Input time: " << e1 << '\n';

    sw.Start();

    Terminal* donorp;
    if(modeldata == "Human.inp")
    {
        donorp = new MDD_Donor(modeldata,cgcontent);
    }
    else
    {
        donorp = new WAM_Donor<2>(modeldata,cgcontent);
    }

    WAM_Acceptor<2> acceptor(modeldata,cgcontent);
    WMM_Start start(modeldata,cgcontent);
    WAM_Stop stop(modeldata,cgcontent);
    MC3_CodingRegion<5> cdr(modeldata,cgcontent);
    MC_NonCodingRegion<5> intron_reg(modeldata,cgcontent), intergenic_reg(modeldata,cgcontent);
    Intron::Init(modeldata,cgcontent,right-left+1);
    Intergenic::Init(modeldata,cgcontent,right-left+1);
    Exon::Init(modeldata,cgcontent);

    double e2 = sw.Elapsed();
    if(debug) cerr << "Init time: " << e2 << '\n';

    sw.Start();
    bool leftwall = true, rightwall = true;
    SeqScores ss(acceptor, *donorp, start, stop, cdr, intron_reg, 
                 intergenic_reg, seq, left, right, cls, fshifts, repeats, 
                 leftwall, rightwall, file);
    HMM_State::SetSeqScores(ss);

    double e3 = sw.Elapsed();
    if(debug) cerr << "Scoring time: " << e3 << '\n';

    sw.Start();
    Parse parse(ss);
    double e4 = sw.Elapsed();
    if(debug) cerr << "Parse time: " << e4 << '\n';

    sw.Start();
    parse.PrintGenes();
    delete donorp;
    return 0;

}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CGnomonApp().AppMain(argc, argv, 0, eDS_Default, 0);
}



#if 0
double diff(struct timeval t2, struct timeval t1)
{  double t;
    t = t2.tv_sec-t1.tv_sec+0.000001*(t2.tv_usec-t1.tv_usec);
    return t;
}     

Int2 Main()
{
    struct timeval t1,t2;

    Args myargs[] = 
    {
        {"File",NULL,NULL,NULL,FALSE,'i',ARG_STRING,0.0,0,NULL},
        {"From","0",NULL,NULL,TRUE,'f',ARG_INT,0.0,0,NULL},
        {"To","1000000000",NULL,NULL,TRUE,'t',ARG_INT,0.0,0,NULL},
        {"Model data","Human.inp",NULL,NULL,TRUE,'m',ARG_STRING,0.0,0,NULL},
        {"Apriori info","",NULL,NULL,TRUE,'a',ARG_STRING,0.0,0,NULL},
        {"Frame shifts","",NULL,NULL,TRUE,'s',ARG_STRING,0.0,0,NULL},
        {"Repeats","0",NULL,NULL,TRUE,'r',ARG_INT,0.0,0,NULL},
        {"Debug","0",NULL,NULL,TRUE,'d',ARG_INT,0.0,0,NULL},
    };
    if(!GetArgs("GeneFinder",sizeof(myargs)/sizeof(myargs[0]), myargs)) exit(1);

    enum {Input, From, To, Model, Apriori, Shifts, Repeats, Debug};

    string file(myargs[Input].strvalue);
    int left = myargs[From].intvalue, right = myargs[To].intvalue;
    string modeldata(myargs[Model].strvalue);
    string apriorifile(myargs[Apriori].strvalue);
    string shifts(myargs[Shifts].strvalue);
    bool repeats(myargs[Repeats].intvalue);
    bool debug(myargs[Debug].intvalue);

    gettimeofday(&t1, NULL);
    ClusterSet cls;
    if(!apriorifile.empty())
    {
        CNcbiIfstream from(apriorifile.c_str());
        if(!from)
        {
            cerr << "Can't find alignments " << apriorifile << '\n';
            exit(1);
        }
        from >> cls;
    }

    FrameShifts fshifts;
    if(!shifts.empty())
    {
        CNcbiIfstream from(shifts.c_str());
        if(!from)
        {
            cerr << "Can't find frameshifts " << shifts << '\n';
            exit(1);
        }
        int loc;
        while(from >> loc)
        {
            char is_i, c = 'N';
            from >> is_i;
            if(is_i == '+') from >> c;
            fshifts.push_back(FrameShiftInfo(loc,is_i == '+',c));
        }
    }

    CNcbiIfstream from(file.c_str());
    CVec seq;
    string line;
    char c;
    getline(from,line);
    while(from >> c) seq.push_back(c);

    int cgcontent = 0;
    int len = seq.size();
    right = min(right,len-1);
    for(int i = left; i <= right; ++i)
    {
        int c = toupper(seq[i]);
        if(c == 'C' || c == 'G') ++cgcontent;
    }
    cgcontent = cgcontent*100./(right-left+1)+0.5;

    cout << "CG-content: " << cgcontent << endl;

    gettimeofday(&t2, NULL);
    if(debug) cerr << "Input time: " << diff(t2,t1) << '\n';

    gettimeofday(&t1, NULL);

    Terminal* donorp;
    if(modeldata == "Human.inp")
    {
        donorp = new MDD_Donor(modeldata,cgcontent);
    }
    else
    {
        donorp = new WAM_Donor<2>(modeldata,cgcontent);
    }

    WAM_Acceptor<2> acceptor(modeldata,cgcontent);
    WMM_Start start(modeldata,cgcontent);
    WAM_Stop stop(modeldata,cgcontent);
    MC3_CodingRegion<5> cdr(modeldata,cgcontent);
    MC_NonCodingRegion<5> intron_reg(modeldata,cgcontent), intergenic_reg(modeldata,cgcontent);
    Intron::Init(modeldata,cgcontent,right-left+1);
    Intergenic::Init(modeldata,cgcontent,right-left+1);
    Exon::Init(modeldata,cgcontent);
    gettimeofday(&t2, NULL);
    if(debug) cerr << "Init time: " << diff(t2,t1) << '\n';

    gettimeofday(&t1, NULL);
    bool leftwall = true, rightwall = true;
    SeqScores ss(acceptor, *donorp, start, stop, cdr, intron_reg, 
                 intergenic_reg, seq, left, right, cls, fshifts, repeats, 
                 leftwall, rightwall, file);
    HMM_State::SetSeqScores(ss);
    gettimeofday(&t2, NULL);
    if(debug) cerr << "Scoring time: " << diff(t2,t1) << '\n';

    gettimeofday(&t1, NULL);
    Parse parse(ss);
    gettimeofday(&t2, NULL);
    if(debug) cerr << "Parse time: " << diff(t2,t1) << '\n';

    gettimeofday(&t1, NULL);
    parse.PrintGenes();

    /*
       cout << "\n\n";
       parse.PrintInfo();
       gettimeofday(&t2, NULL);
       if(debug) cerr << "Print time: " << diff(t2,t1) << '\n';

       list<Gene> genes = parse.GetGenes();
       for(list<Gene>::iterator it = genes.begin(); it != genes.end(); ++it)
       {
       const Gene& igene = *it;
       for(int j = 0; j < igene.size(); ++j)
       {
       int a = igene[j].Start();
       int b = igene[j].Stop();
       if(j == igene.size()-1 && igene.Strand() == Plus) b += 3;
       if(j == 0 && igene.Strand() == Minus) a -= 3;
       cout << a+1 << ' ' << b+1 << '\n';
       }
       }
       */
    delete donorp;
    return 0;

}

#endif

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
