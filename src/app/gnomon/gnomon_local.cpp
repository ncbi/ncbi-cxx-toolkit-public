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
#include <time.h>
#include <sys/time.h>

USING_NCBI_SCOPE;
USING_SCOPE(gnomon);

double diff(struct timeval t2, struct timeval t1)
{  double t;
   t = t2.tv_sec-t1.tv_sec+0.000001*(t2.tv_usec-t1.tv_usec);
   return t;
}


class CGnomon_localApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


void CGnomon_localApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),"gnomon_template");

    arg_desc->AddKey("inp","inp","Genomic sequence for prediction",CArgDescriptions::eInputFile);
	arg_desc->AddKey("param","param","Organism specific parameters",CArgDescriptions::eString);
	arg_desc->AddKey("mpp","mpp","Multiprotein penalty",CArgDescriptions::eDouble);
	arg_desc->AddDefaultKey("from", "from","Start point",CArgDescriptions::eInteger,"0");
	arg_desc->AddDefaultKey("to", "to","End point",CArgDescriptions::eInteger,"1000000000");
	arg_desc->AddOptionalKey("align","align","Alignments for model hints",CArgDescriptions::eInputFile);
    arg_desc->AddFlag("norep","DO NOT mask lower case letters");
    arg_desc->AddFlag("debug","Run in debug mode");
    arg_desc->AddFlag("open","Allow partial predictions");
	
	SetupArgDescriptions(arg_desc.release());
}

int CGnomon_localApplication::Run(void)
{
    CArgs args = GetArgs();
    
    CNcbiIstream& from = args["inp"].AsInputFile();
    int left = args["from"].AsInteger();
    int right = args["to"].AsInteger();
    string modeldata(args["param"].AsString());
    bool repeats = !args["norep"];
    bool debug = args["debug"];
    double mpp = args["mpp"].AsDouble();
    bool leftwall = !args["open"];
    bool rightwall = !args["open"];

    struct timeval t1,t2;
    
	gettimeofday(&t1, NULL);

    TAlignList cls;
    if(args["align"])
    {
        CNcbiIstream& apriorifile = args["align"].AsInputFile();
        CAlignVec algn;
        
        while(apriorifile >> algn) 
        {
            cls.push_back(algn);
            cerr << "Align " << algn;
        }
    }

    CResidueVec seq;
    string line;
    char c;
    getline(from,line);
    while(from >> c) seq.push_back(c);
    
    int len = seq.size();
    right = min(right,len-1);
    gettimeofday(&t2, NULL);
    if(debug) cerr << "Input time: " << diff(t2,t1) << '\n';

    gettimeofday(&t1, NULL);
    
    CGnomonEngine gnomon(modeldata, line.substr(1), seq, TSignedSeqRange(left, right));
        
    gettimeofday(&t2, NULL);
    if(debug) cerr << "Init time: " << diff(t2,t1) << '\n';

    gettimeofday(&t1, NULL);

    gnomon.Run(cls, repeats, leftwall, rightwall, mpp);

    gettimeofday(&t2, NULL);
    if(debug) cerr << "Scoring time: " << diff(t2,t1) << '\n';

    gettimeofday(&t1, NULL);
    gettimeofday(&t2, NULL);
    if(debug) cerr << "Parse time: " << diff(t2,t1) << '\n';

    gettimeofday(&t1, NULL);
    CUniqNumber unumber;
    PrintGenes(gnomon.GetGenes(),unumber);
    cout << "\n\n";
//	parse.PrintInfo();
    gettimeofday(&t2, NULL);
    if(debug) cerr << "Print time: " << diff(t2,t1) << '\n';

    return 0;

}

void CGnomon_localApplication::Exit(void)
{
    SetDiagStream(0);
}


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CGnomon_localApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
