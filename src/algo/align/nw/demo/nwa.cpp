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
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);
    argdescr->SetUsageContext(GetArguments().GetProgramName(),
                              "Global alignment algorithms demo application.\n"
                              "Build 1.00.09 - 01/24/03");

    argdescr->AddDefaultKey
        ("matrix", "matrix", "scoring matrix",
         CArgDescriptions::eString, "nucl");

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

    argdescr->AddFlag("mm",
                      "Limit memory use to linear (Myers and Miller method)");
    argdescr->AddFlag("mt", "Use multiple threads");

    // supported output formats
    argdescr->AddOptionalKey
        ("o1", "o1", "Filename for type 1 output", CArgDescriptions::eString);

    argdescr->AddOptionalKey
        ("o2", "o2", "Filename for type 2 output", CArgDescriptions::eString);

    argdescr->AddFlag("ofasta", 
        "Generate gapped FastA output for the aligner sequences");

    argdescr->AddOptionalKey
        ("oasn", "oasn", "ASN.1 output filename", CArgDescriptions::eString);
    
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


auto_ptr<ofstream> open_ofstream (const string& filename) {

    auto_ptr<ofstream> pofs0 ( new ofstream (filename.c_str()) );
    if(*pofs0) {
        return pofs0;
    }
    else {
        NCBI_THROW(CAppNWAException,
                   eCannotWriteFile,
                   "Cannot write to file" + filename);
    }
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
                   "Linear memory approach is not yet supported for the "
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

    bool   output_type1  ( args["o1"] );
    bool   output_type2  ( args["o2"] );
    bool   output_asn    ( args["oasn"] );
    bool   output_fasta  ( args["ofasta"] );
    
    auto_ptr<ofstream> pofs1 (0);
    auto_ptr<ofstream> pofs2 (0);
    auto_ptr<ofstream> pofsAsn (0);
    auto_ptr<ofstream> pofsFastA_seq1 (0);
    auto_ptr<ofstream> pofsFastA_seq2 (0);

    if(output_type1) {
        pofs1.reset(open_ofstream (args["o1"].AsString()).release());
    }

    if(output_type2) {
        pofs2.reset(open_ofstream (args["o2"].AsString()).release());
    }

    if(output_asn) {
        pofsAsn.reset(open_ofstream (args["asn"].AsString()).release());
    }

    if(output_fasta) {
        pofsFastA_seq1.reset(open_ofstream (args["seq1"].AsString() + ".gfa")
                             .release());
        pofsFastA_seq2.reset(open_ofstream (args["seq2"].AsString() + ".gfa")
                             .release());
    }

    int score = aligner->Run();
    cerr << "Score = " << score << endl;


    const size_t line_width = 50;
    if(pofs1.get()) {
        *pofs1 << aligner->Format(line_width, CNWAligner::eFormatType1);
    }

    if(pofs2.get()) {
        *pofs2 << aligner->Format(line_width, CNWAligner::eFormatType2);
    }

    if(pofsAsn.get()) {
        *pofsAsn << aligner->Format(line_width, CNWAligner::eFormatAsn);
    }

    if(pofsFastA_seq1.get()) {
        *pofsFastA_seq1 << '>' << seqname1 << endl;
        *pofsFastA_seq1 << aligner->Format(line_width,
                                           CNWAligner::eFormatFastA, 1);
    }

    if(pofsFastA_seq2.get()) {
        *pofsFastA_seq2 << '>' << seqname2 << endl;
        *pofsFastA_seq2 << aligner->Format(line_width, 
                                           CNWAligner::eFormatFastA, 2);
    }
    
    if(!output_type1 && !output_type2 && !output_asn && !output_fasta) {
        cout << aligner->Format(line_width, CNWAligner::eFormatType2);
    }
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
 * Revision 1.8  2003/01/24 19:43:03  ucko
 * Change auto_ptr assignment to use release and reset rather than =,
 * which not all compilers support.
 *
 * Revision 1.7  2003/01/24 16:49:59  kapustin
 * Support different output formats
 *
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
