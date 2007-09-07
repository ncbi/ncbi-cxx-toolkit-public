/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Boris Kiryutin
*
* =========================================================================
*/


#ifndef PROSPLIGN_INFO_HPP
#define PROSPLIGN_INFO_HPP

#include <corelib/ncbistl.hpp>
#include <string>
#include <list>
#include <vector>
#include <iostream>

#include <algo/align/prosplign/prosplign.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

class CNSeq;
class CAli;
class CSubstMatrix;

class CInfo
{
public:
    CInfo(const CAli& ali, CProSplignOutputOptionsExt output_options = CProSplignOutputOptions());
    ~CInfo(void);

    const CAli& m_Ali; //the alignment CInfo object is created for
    //    ostream& m_InfoOut;
    //alignment coords -> sequence coords.
    //ONE BASED (designed specially for output), for initial gap value is 0
    vector<int> ncoor;
//    vector<int> pcoor;
    string match, outp, outr, outn;
    const CNSeq& nseq;
    CProSplignOutputOptionsExt m_options;

    //'good' pieces. many methods imply that 
    //the pieces begin and and on match (positive?)
    //careful with type 'full'
    list<CNPiece> m_AliPiece;

    //protein gap structure in alignment
    //gpseq[alipos] = i (intron), g, t or T (gap), l or u (not gap)
    vector<char> m_Gpseq;
    //shows gap structure in alignment. m_Gnseq[alipos] = false if gap, true if not
    vector<bool> m_Gnseq;
    //protein location in alignment coord [ProtBegOnAliCoord(),ProtEndOnAliCoord())
    int ProtBegOnAliCoord(void) const;//returns first alignment coord where protein is represented
    bool ProtBegOnAli(void) const;//Check if alignment to show (m_AliPiece) starts with true protein start
    int ProtEndOnAliCoord(void) const;//returns alignment coord right after end of protein

//following group of methods currently works only after PrintAlign is called
    bool ReportStart(void) const;
    bool ReportStop(void) const;    
    inline int GetFullAliLen(void) const { return outn.size(); }
//all counting methods operate only with respect of 'm_AliPiece'
    int GetAliLen(void) const;//EXCLUDING introns
    double GetTotalPosPerc(void) const;//returns number of positives*100% (after postprocessing!) divided by 3*prot_length
    int GetIdentNum(void) const;//returns number of alignment positions marked as ident
    int GetPositNum(void) const;//returns number of alignment positions marked as positive (including ident)
    int GetNegatNum(void) const;//returns number of alignment positions marked as negatives
    int GetNucGapLen(void) const;//total nuc gap length (that is gap in nuc seq, aka delition)
    int GetProtGapLen(void) const;//total prot gap length (that is gap in nuc prot, aka insertion)

    void Cut(void);
//     void StatOut(ostream& stat_out) const;//output of statistics, call only after PrintAlign
// private:
//     void Out(ostream& out);//new style, call only after PrintAlign
public:
    void InitAlign(const CSubstMatrix& matrix);
    //perform initialisation of the members above before output.
    //    void PrintAlign(ostream& out, int width = 100, bool info_only = false);	
	
	bool IsFV(int n);
    bool IsFH(int n);
//returns previous nucleotide  before given alignment position
    //or -1 if none (NULL BASED), works for [0:alig_len] range
    int PrevNucNullBased(int alipos) { 
        if(alipos) return ncoor[alipos-1] - 1;
        return -1;
    }
//returns next nucleotide  after given alignment position or ncoor.back() if none
    //or seqlen if none (NULL BASED), works for [-1:alig_len-1] range
    int NextNucNullBased(int alipos) const { 
        if(alipos<0) return 0;
        return ncoor[alipos]; 
    }
    
    bool StopInside(int beg, int end)//has stop in range [beg,end) on alignment   
    {
        for(int n=beg; n < end; ++n) {
            if(outr[n] == '*') return true;
        }
        return false;
    }

    bool full;//do not search for 'good pieces', output full alignment in info file
//    static bool info_only;   //true - info only, false - alignment output
    bool eat_gaps;//do not show regular gaps in short output (info)
    //deletions at the beginning/end for 'full' output
    int beg_del, end_del;
};

class CExonPiece
{
public:
    CExonPiece(void);
    virtual ~CExonPiece(void);
//    virtual bool IsFr(void) = 0; //frameshift?
    // length on alignment
    virtual int GetLen(void) { return m_Len; }
//     virtual void Out(ostream& out) = 0;

    int m_Len;//on alignment!
};

class CExonDel : public CExonPiece
{
public:
//     virtual void Out(ostream& out) { out<<"\tD"<<GetLen(); }
    /*
    virtual bool IsFr(void) {
        if(m_Len%3) return true;
        return false;
    }
    */
};

class CExonNonDel : public CExonPiece
{
public:
    int m_From, m_To;//from and to on NUCLEOTIDE!
};

class CExonIns : public CExonNonDel
{
public:
//     virtual void Out(ostream& out); //old: { out<<"\tI("<<GetLen(); }
    /*
    virtual bool IsFr(void) {
        if(m_Len%3) return true;
        return false;
    }
    */
};

class CExonMM : public CExonNonDel//any piece without framshift, may contain gaps
{
public:
//     virtual void Out(ostream& out);
//    virtual bool IsFr(void) { return false; }
};


class CExonStruct//exon
{
public:
    CExonStruct(void);
    ~CExonStruct(void);
//     void StructOut(ostream& out);

    vector<CExonPiece *> m_pc;
    string m_B;//nucleotides before exon
    string m_A;//nucleotides after exon
    int m_AliBeg; // first coor ON ALIGNMENT
    int m_AliEnd; // first coor ON ALIGNMENT AFTER END of exon
};

class CProtPiece //good piece of alignment
{
    public:
    CProtPiece(const CAli& cali, CNPiece& np, CInfo& info, int num);
    ~CProtPiece() { Clear(); }//takes care of 'CExonPiece *' in exons
    void Clear(void);//takes care of 'CExonPiece *' in exons
//     void Out(ostream& out);
    void EatGaps(void);//leaves frameshifts
    bool HasFr(void);//has frameshift?
    bool StopInside(void);//has stop inside?
    //checks if ExonPiece it is a frameshift in context of whole protein piece, i.e.
    // somewhere on right of 'it' must be a match (mismatch), same on left.
    //takes care if over-intron frameshifts and gaps
    bool IsFr(vector<CExonStruct>::iterator et, vector<CExonPiece *>::iterator it);
    int GetFrame(void);//frame at the beginning of the first exon. exon must be at least length three

    vector<CExonStruct> m_Exons;
    int m_Num;//order number of piece
private:
    CProtPiece(const CProtPiece&);
    CProtPiece& operator=(const CProtPiece& );
    CInfo& m_Info;
};


END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif //PROSPLIGN_INFO_HPP
