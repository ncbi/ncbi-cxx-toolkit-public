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

#include "nwa.hpp"
#include <algo/nw_aligner.hpp>

BEGIN_NCBI_SCOPE


void CAppNWA::Init()
{
    auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);
    argdescr->SetUsageContext(GetArguments().GetProgramName(),
                              "Needleman-Wunsch alignment test application.\n"
                              "Build 1.00.03 - 12/03/02");

    argdescr->AddDefaultKey
        ("matrix", "matrix", "scoring matrix",
         CArgDescriptions::eString, "nucl");

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


void CAppNWA::x_RunOnPair() const throw(CAppNWAException)
{
    const CArgs& args = GetArgs();

    // read input sequences
    vector<char> v1, v2;
    string seqname1, seqname2;
    int offset1 = 0, offset2 = 0;

    if ( !x_ReadFastaFile(args["seq1"].AsString(), &seqname1, &v1, &offset1)) {
        NCBI_THROW(CAppNWAException,
                   eCannotReadFile,
                   "Cannot read file " + args["seq1"].AsString());
    }
    if ( !x_ReadFastaFile(args["seq2"].AsString(), &seqname2, &v2, &offset2)) {
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

    CNWAligner aligner(&v1[0], v1.size(), &v2[0], v2.size(), smt);

    aligner.SetWm  (args["Wm"]. AsInteger());
    aligner.SetWms (args["Wms"].AsInteger());
    aligner.SetWg  (args["Wg"]. AsInteger());
    aligner.SetWs  (args["Ws"]. AsInteger());

    int score = aligner.Run();
    cerr << "Score = " << score << endl;
    cout << aligner.Format();
}


void CAppNWA::Exit()
{
    return;
}


bool CAppNWA::x_ReadFastaFile
(const string& filename,
 string*       seqname,
 vector<char>* sequence,
 int*          offset)
    const
{
    vector<char>& vOut = *sequence;
    vOut.clear();

    ifstream ifs(filename.c_str());

    // read sequence's name and offset
    ifs >> *seqname >> *offset;
    if ( !ifs )
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
 * Revision 1.1  2002/12/06 17:44:25  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
