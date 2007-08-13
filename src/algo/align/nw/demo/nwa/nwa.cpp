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
 * File Description:  xalgoalign application
 *                   
*/

#include <ncbi_pch.hpp>

#include "nwa.hpp"

#include <algo/align/nw/nw_band_aligner.hpp>
#include <algo/align/nw/mm_aligner.hpp>
#include <algo/align/nw/nw_spliced_aligner16.hpp>
#include <algo/align/nw/nw_spliced_aligner32.hpp>
#include <algo/align/nw/nw_formatter.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>

#define SPLALIGNER CSplicedAligner32


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

void CAppNWA::Init()
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);
    argdescr->SetUsageContext(GetArguments().GetProgramName(),
                              "Demo application using xalgoalign library");

    argdescr->AddDefaultKey
        ("matrix", "matrix", "scoring matrix",
         CArgDescriptions::eString, "nucl");

    argdescr->AddFlag("spliced",
                      "Spliced mRna/EST-to-Genomic alignment "
                      "(consider specifying -esf zzxx)" );

    argdescr->AddOptionalKey("pattern", "pattern",
                             "Use HSPs to guide spliced alignment",
                             CArgDescriptions::eInteger);
    argdescr->AddKey
        ("seq1", "seq1",
         "the first input sequence in fasta file",
         CArgDescriptions::eString);
    argdescr->AddKey
        ("seq2", "seq2",
         "the second input sequence in fasta file",
         CArgDescriptions::eString);

    argdescr->AddDefaultKey
        ("esf", "esf",
         "End-space free alignment. Format: lrLR where each character "
         "can be z (free end) or x (regular end) representing "
         "left and right ends. First sequence's ends are specified first.", 
         CArgDescriptions::eString,
         "xxxx");

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
        ("band", "band", "Band width in banded alignment",
         CArgDescriptions::eInteger, "-1");

    argdescr->AddDefaultKey
        ("shift", "shift", 
         "Band shift in banded alignment "
         "(specify negative value to indicate second sequence)",
         CArgDescriptions::eInteger, "0");

    argdescr->AddDefaultKey
        ("Wi0", "intron0", "type 0 (GT/AG) intron weight",
         CArgDescriptions::eInteger,
         NStr::IntToString(SPLALIGNER::GetDefaultWi(0)).c_str());

    argdescr->AddDefaultKey
        ("Wi1", "intron1", "type 1 (GC/AG) intron weight",
         CArgDescriptions::eInteger,
         NStr::IntToString(SPLALIGNER::GetDefaultWi(1)).c_str());

    argdescr->AddDefaultKey
        ("Wi2", "intron2", "type 2 (AT/AC) intron weight",
         CArgDescriptions::eInteger,
         NStr::IntToString(SPLALIGNER::GetDefaultWi(2)).c_str());

    int intron_min_size = SPLALIGNER::GetDefaultIntronMinSize();
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

    argdescr->AddOptionalKey
        ("ofasta", "ofasta",
         "Generate gapped FastA output for the aligner sequences",
         CArgDescriptions::eString);

    argdescr->AddOptionalKey
        ("oasn", "oasn", "ASN.1 output filename", CArgDescriptions::eString);

    argdescr->AddOptionalKey
        ("oexons", "exons",
         "Exon table output filename (spliced alignments only)",
         CArgDescriptions::eString);
    
    CArgAllow_Strings* paa_st = new CArgAllow_Strings;
    paa_st->Allow("nucl")->Allow("blosum62");
    argdescr->SetConstraint("matrix", paa_st);

    CArgAllow_Strings* paa_esf = new CArgAllow_Strings;
    paa_esf->Allow("xxxx")->Allow("xxxz")->Allow("xxzx")->Allow("xxzz");
    paa_esf->Allow("xzxx")->Allow("xzxz")->Allow("xzzx")->Allow("xzzz");
    paa_esf->Allow("zxxx")->Allow("zxxz")->Allow("zxzx")->Allow("zxzz");
    paa_esf->Allow("zzxx")->Allow("zzxz")->Allow("zzzx")->Allow("zzzz");
    argdescr->SetConstraint("esf", paa_esf);

    CArgAllow* paa0 = new CArgAllow_Integers(5,1000);
    argdescr->SetConstraint("pattern", paa0);

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
    THROWS((CAppNWAException, CAlgoAlignException))
{
    const CArgs& args = GetArgs();

    // analyze parameters
    const bool bMM = args["mm"];
    const bool bMT = args["mt"];
    const bool bMrna2Dna = args["spliced"];
    const bool bGuides = args["pattern"];

    bool   output_type1  ( args["o1"] );
    bool   output_type2  ( args["o2"] );
    bool   output_asn    ( args["oasn"] );
    bool   output_fasta  ( args["ofasta"] );
    bool   output_exons  ( args["oexons"] );

    int    band (args["band"].AsInteger());
    int    shift(args["shift"].AsInteger());

    if(bMrna2Dna && args["matrix"].AsString() != "nucl") {
        NCBI_THROW(CAppNWAException,
                   eInconsistentParameters,
                   "Spliced alignment assumes nucleotide sequences "
                   "(matrix = nucl)");
    }

    if(output_exons && !bMrna2Dna) {
        NCBI_THROW(CAppNWAException,
                   eInconsistentParameters,
                   "Exon output can only be requested in mRna2Dna mode");
    }
     
    if(bMrna2Dna && bMM) {
        NCBI_THROW(CAppNWAException,
                   eInconsistentParameters,
                   "Linear memory approach is not yet supported for the "
                   "spliced alignment algorithm");
    }
     
    if(!bMrna2Dna && bGuides) {
        NCBI_THROW(CAppNWAException,
                   eInconsistentParameters,
                   "Guides are only supported in spliced mode" );
    }
     
    if(bMT && !bMM) {
        NCBI_THROW(CAppNWAException,
                   eInconsistentParameters,
                   "Mutliple thread mode is currently supported "
                   "for Myers-Miller method only (-mm flag)");
    }

     
    if(bMM && band >= 0) {
        NCBI_THROW(CAppNWAException,
                   eInconsistentParameters,
                   "-mm and -band are inconsistent with each other");
    }

#ifndef NCBI_THREADS
    if(bMT) {
        NCBI_THROW(CAppNWAException,
            eNotSupported,
            "This application was built without multithreading support. "
            "To run in multiple threads, please re-configure and rebuild"
	    " with proper option.");
    }
    
#endif

    // read input sequences
    vector<char> v1, v2;

    CRef<CSeq_id> seqid1 = x_ReadFastaFile(args["seq1"].AsString(), &v1);
    CRef<CSeq_id> seqid2 = x_ReadFastaFile(args["seq2"].AsString(), &v2);

    // determine sequence/score matrix type
    const SNCBIPackedScoreMatrix* psm = 
      (args["matrix"].AsString() == "blosum62")? &NCBISM_Blosum62: 0;

    CNWAligner* pnwaligner = 0;
    if(bMrna2Dna) {
        pnwaligner = new SPLALIGNER(&v1[0], v1.size(), &v2[0], v2.size());
    }
    else if(bMM) {
        pnwaligner = new CMMAligner(&v1[0], v1.size(), &v2[0], v2.size(), psm);
    }
    else if (band < 0) {
        pnwaligner = new CNWAligner(&v1[0], v1.size(), &v2[0], v2.size(), psm);
    }
    else {
        CBandAligner * ba = new CBandAligner(&v1[0], v1.size(), &v2[0], v2.size(),
                                      psm, band);
        Uint1 where = shift >= 0? 0: 1;
        ba->SetShift(where,abs(shift));
        pnwaligner = ba;
    }


    auto_ptr<CNWAligner> aligner (pnwaligner);

    if(psm == NULL) {
        aligner->SetWm  (args["Wm"]. AsInteger());
        aligner->SetWms (args["Wms"].AsInteger());
        aligner->SetScoreMatrix(NULL);
    }
    aligner->SetWg  (args["Wg"]. AsInteger());
    aligner->SetWs  (args["Ws"]. AsInteger());

    aligner->SetScoreMatrix(psm); // re-set score matrix to handle 
                                  // possible ambiguity chars

    if( bMrna2Dna ) {
        SPLALIGNER *aligner_mrna2dna = 
            static_cast<SPLALIGNER*> (aligner.get());

        aligner_mrna2dna->SetWi (0, args["Wi0"]. AsInteger());
        aligner_mrna2dna->SetWi (1, args["Wi1"]. AsInteger());
        aligner_mrna2dna->SetWi (2, args["Wi2"]. AsInteger());

        aligner_mrna2dna->SetIntronMinSize(args["IntronMinSize"]. AsInteger());

        if(bGuides) {
            aligner_mrna2dna->MakePattern(args["pattern"].AsInteger());
        }
    }

    if(bMT && bMM) {
        CMMAligner* pmma = static_cast<CMMAligner*> (aligner.get());
        pmma -> EnableMultipleThreads();
    }
    
    auto_ptr<ofstream> pofs1 (0);
    auto_ptr<ofstream> pofs2 (0);
    auto_ptr<ofstream> pofsAsn (0);
    auto_ptr<ofstream> pofsFastA (0);
    auto_ptr<ofstream> pofsExons (0);

    if(output_type1) {
        pofs1.reset(open_ofstream (args["o1"].AsString()).release());
    }

    if(output_type2) {
        pofs2.reset(open_ofstream (args["o2"].AsString()).release());
    }

    if(output_asn) {
        pofsAsn.reset(open_ofstream (args["oasn"].AsString()).release());
    }

    if(output_fasta) {
        pofsFastA.reset(open_ofstream (args["ofasta"].AsString()).release());
    }

    if(output_exons) {
        pofsExons.reset(open_ofstream (args["oexons"].AsString()).release());
    }

    {{  // setup end penalties
        string ends = args["esf"].AsString();
        bool L1 = ends[0] == 'z';
        bool R1 = ends[1] == 'z';
        bool L2 = ends[2] == 'z';
        bool R2 = ends[3] == 'z';
        aligner->SetEndSpaceFree(L1, R1, L2, R2);
    }}

    int score = aligner->Run();
    cerr << "Score = " << score << endl;

    CNWFormatter formatter (*aligner);
    formatter.SetSeqIds(seqid1, seqid2);

    const size_t line_width = 100;
    string s;
    if(pofs1.get()) {
        formatter.AsText(&s, CNWFormatter::eFormatType1, line_width);
        *pofs1 << s;
    }

    if(pofs2.get()) {
        formatter.AsText(&s, CNWFormatter::eFormatType2, line_width);
        *pofs2 << s;
    }

    if(pofsAsn.get()) {
        formatter.AsText(&s, CNWFormatter::eFormatAsn, line_width);
        *pofsAsn << s;
    }

    if(pofsFastA.get()) {
        formatter.AsText(&s, CNWFormatter::eFormatFastA, line_width);
        *pofsFastA << s;
    }

    if(pofsExons.get()) {
        formatter.AsText(&s, CNWFormatter::eFormatExonTableEx, line_width);
        *pofsExons << s;
    }
    
    if(!output_type1 && !output_type2
       && !output_asn && !output_fasta
       && !output_exons) {
        formatter.AsText(&s, CNWFormatter::eFormatType2, line_width);
        cout << s;
    }
}


void CAppNWA::Exit()
{
    return;
}


CRef<CSeq_id> CAppNWA::x_ReadFastaFile (const string& filename,
                                        vector<char>* sequence) const
{
    vector<char>& vOut = *sequence;
    vOut.clear();

    ifstream ifs(filename.c_str());

    // read the defline
    string str;
    getline(ifs, str);

    CRef<CSeq_id> seqid;
    try {
        seqid.Reset(new CSeq_id(str));
    } catch (CSeqIdException&) {
        seqid.Reset(new CSeq_id(CSeq_id::e_Local, str));
    }

    // read the sequence
    while ( ifs ) {
        string s;
        ifs >> s;
        NStr::ToUpper(s);
        copy(s.begin(), s.end(), back_inserter(vOut));
    }

    return seqid;
}


END_NCBI_SCOPE


USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
    return CAppNWA().AppMain(argc, argv, 0, eDS_Default, 0);
}
