/* $Id$
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
 * Author:  Yuri Kapustin
 *
 * File Description:  NWA application
 *                   
*/

#include <algo/mm_aligner.hpp>
#include <algo/nw_aligner_mrna2dna.hpp>
#include "nwa.hpp"

BEGIN_NCBI_SCOPE


void CAppNWA::Init()
{
    auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);
    argdescr->SetUsageContext(GetArguments().GetProgramName(),
                              "Global alignment algorithms demo application.\n"
                              "Build 1.00.08 - 01/22/03");

    argdescr->AddDefaultKey
        ("matrix", "matrix", "scoring matrix",
         CArgDescriptions::eString, "nucl");

    argdescr->AddFlag("mm",
                      "Limit memory use to linear (Myers and Miller method)");

    argdescr->AddFlag("mt", "Use multiple threads");

    argdescr->AddFlag("mrna2dna", "mRna vs. Dna alignment");

    argdescr->AddKey
        ("seq1", "seq1",
         "the first input sequence in fasta file",
         CArgDescriptions::eString);
    argdescr->AddKey
        ("seq2", "seq2",
         "the second input sequence in fasta file",
         CArgDescriptions::eString);

    argdescr->AddDefaultKey
        ("Wm", "match", "match bonus (nucleotide sequences)",
         CArgDescriptions::eInteger,
         NStr::IntToString(CNWAligner::GetDefaultWm()).c_str());

    argdescr->AddDefaultKey
        ("Wms", "mismatch", "mismatch penalty (nucleotide sequences)",
         CArgDescriptions::eInteger,
         NStr::IntToString(CNWAligner::GetDefaultWms()).c_str());

    argdescr->AddDefaultKey
        ("Wg", "gap", "gap opening penalty",
         CArgDescriptions::eInteger,
         NStr::IntToString(CNWAligner::GetDefaultWg()).c_str());

    argdescr->AddDefaultKey
        ("Ws", "space", "gap extension (space) penalty",
         CArgDescriptions::eInteger,
         NStr::IntToString(CNWAligner::GetDefaultWs()).c_str());

    argdescr->AddDefaultKey
        ("Wi", "intron", "intron weight",
         CArgDescriptions::eInteger,
         NStr::IntToString(CNWAlignerMrna2Dna::GetDefaultWi()).c_str());

    int intron_min_size = CNWAlignerMrna2Dna::GetDefaultIntronMinSize();
    argdescr->AddDefaultKey
        ("IntronMinSize", "IntronMinSize", "intron minimum size",
         CArgDescriptions::eInteger,
         NStr::IntToString(intron_min_size).c_str());
    
    CArgAllow_Strings* paa_st = new CArgAllow_Strings;
    paa_st->Allow("nucl")->Allow("blosum62");
    argdescr->SetConstraint("matrix", paa_st);

    SetupArgDescriptions(argdescr.release());
}


int CAppNWA::Run()
{ 
    x_RunOnPair();
    return 0;
}


void CAppNWA::x_RunOnPair() const
    throw(CAppNWAException, CNWAlignerException)
{
    const CArgs& args = GetArgs();

    // analyze parameters
    const bool bMM = args["mm"];
    const bool bMT = args["mt"];
    const bool bMrna2Dna = args["mrna2dna"];

    if(bMrna2Dna && args["matrix"].AsString() != "nucl") {
        NCBI_THROW(CAppNWAException,
                   eInconsistentParameters,
                   "Wrong matrix specified");
    }
     
    if(bMrna2Dna && bMM) {
        NCBI_THROW(CAppNWAException,
                   eInconsistentParameters,
                   "Linear memory approach is not yet supported by the "
                   "spliced alignment algorithm");
    }
     
    if(bMT && !bMM) {
        NCBI_THROW(CAppNWAException,
                   eInconsistentParameters,
                   "Mutliple thread mode is currently supported "
                   "for Myers-Miller method only (-mm flag)");
    }

    // read input sequences
    vector<char> v1, v2;
    string seqname1, seqname2;

    if ( !x_ReadFastaFile(args["seq1"].AsString(), &seqname1, &v1)) {
        NCBI_THROW(CAppNWAException,
                   eCannotReadFile,
                   "Cannot read file " + args["seq1"].AsString());
    }
    if ( !x_ReadFastaFile(args["seq2"].AsString(), &seqname2, &v2)) {
        NCBI_THROW(CAppNWAException,
                   eCannotReadFile,
                   "Cannot read file" + args["seq2"].AsString());
    }

    // determine seq type
    CNWAligner::EScoringMatrixType smt;
    if (args["matrix"].AsString() == "blosum62")
        smt = CNWAligner::eBlosum62;
    else
        smt = CNWAligner::eNucl;

    auto_ptr<CNWAligner> aligner (
        bMrna2Dna? 
        new CNWAlignerMrna2Dna (&v1[0], v1.size(), &v2[0], v2.size()):
        (bMM?
         new CMMAligner (&v1[0], v1.size(), &v2[0], v2.size(), smt):
         new CNWAligner (&v1[0], v1.size(), &v2[0], v2.size(), smt))
        );

    aligner->SetWm  (args["Wm"]. AsInteger());
    aligner->SetWms (args["Wms"].AsInteger());
    aligner->SetWg  (args["Wg"]. AsInteger());
    aligner->SetWs  (args["Ws"]. AsInteger());

    if( bMrna2Dna ) {
        CNWAlignerMrna2Dna *aligner_mrna2dna = 
            static_cast<CNWAlignerMrna2Dna *> (aligner.get());
        
        aligner_mrna2dna->SetWi (args["Wi"]. AsInteger());
        aligner_mrna2dna->SetIntronMinSize(args["IntronMinSize"]. AsInteger());
    }

    if( bMT && bMM) {
        CMMAligner* pmma = static_cast<CMMAligner*> (aligner.get());
        pmma -> EnableMultipleThreads();
    }

    int score = aligner->Run();
    cerr << "Score = " << score << endl;
    cout << aligner->Format();
}


void CAppNWA::Exit()
{
    return;
}


bool CAppNWA::x_ReadFastaFile
(const string& filename,
 string*       seqname,
 vector<char>* sequence)
    const
{
    vector<char>& vOut = *sequence;
    vOut.clear();

    ifstream ifs(filename.c_str());

    // read sequence's name
    string str;
    getline(ifs, str);
    if(!ifs)
        return false;
    istrstream iss (str.c_str());

    char c;
    iss >> c >> *seqname;
    if(!iss)
        return false;

    // read the sequence
    while ( ifs ) {
        string s;
        ifs >> s;
        NStr::ToLower(s);
        copy(s.begin(), s.end(), back_inserter(vOut));
    }

    return true;
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2003/01/21 16:34:22  kapustin
 * mm
 *
 * Revision 1.5  2003/01/21 12:42:02  kapustin
 * Add mm parameter
 *
 * Revision 1.4  2003/01/08 15:58:32  kapustin
 * Read offset parameter from fasta reading routine
 *
 * Revision 1.3  2002/12/17 21:50:24  kapustin
 * Remove unnecesary seq type parameter from the mrna2dna constructor
 *
 * Revision 1.2  2002/12/12 17:59:30  kapustin
 * Enable spliced alignments
 *
 * Revision 1.1  2002/12/06 17:44:25  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
