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


#ifndef PROSPLIGN_ALI_HPP
#define PROSPLIGN_ALI_HPP

#include <algorithm>
#include <objects/seqalign/seqalign__.hpp>
#include "Info.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)
using namespace objects;

enum EAliPieceType {
  eMP, //match/mismatch
  eVP,//vertical gap/frameshift (i.e. in nuc, prot is presented)
  eHP,//horisontal gap/frameshift (i.e. in prot, nuc is presented)
  eSP//intron
};

class CAliPiece {
public:
    EAliPieceType m_type;
    int m_len;
    inline void Init(EAliPieceType t, int len = 0) { 
        m_type = t; 
        m_len = len; 
    }
};

class CNSeq;
class CPSeq;

class CAli {
public:
    vector<CAliPiece> m_ps;
    CNSeq *cnseq;
    CPSeq *cpseq;
    //finds piece (pit) and shift from the beginning of piece by alignment position
    void FindPos(vector<CAliPiece>::const_iterator& pit, int& shift, int alipos) const; 

	//finds piece (pit) and shift from the beginning of piece by alignment position
	//AND initial (0-based) protein(multiplied by 3) and nucleotide positions
    void FindPos(vector<CAliPiece>::const_iterator& pit, int& shift, int& nulpos, int& nultripos, int alipos) const; 

    //finds protein frame by alignment position (0, 1 or 2)
    int FindFrame(int alipos) const;

    //checks if first protein residue matches ATG (and is NOT spliced)
    //the protein residue may have any value (may be not M)
    bool HasStartOnNuc(void) const;

    double score;
  CAli(CNSeq& nseq, CPSeq& pseq, const vector<pair<int, int> >& igi, bool lgap, bool rgap, const CAli& frali);//adds introns/end gaps
  CAli(CNSeq& nseq, CPSeq& pseq);
};

class CSeq_alignHandle;
class CNPiece;
class CSubstMatrix;

//interface to convert alignment into CSeq-align
class CPosAli : public CAli {
public:
	list<CNPiece> m_pcs;
	bool m_IsFull;
    const CSeq_loc& m_genomic;
    const CSeq_id& m_protein;
    CProSplignOutputOptionsExt m_output_options;
    bool m_has_stop_on_nuc; // after the last good piece

    //FULL
	CPosAli(const CAli& ali, const CSeq_id& protein, const CSeq_loc& genomic, const CProSplignOutputOptions& output_options, const CSubstMatrix& matrix);
	//Pieces
	//CPosAli(const CAli& ali, const string& nuc_id, const string& prot_id, const list<CNPiece>& pcs);
	//make Pieces
	void AddPostProcInfo(const list<CNPiece>& pcs);
    // gets alignment fron CSeq_align
	// initiate param and nseq. nseq1 and prot shoud be initialiated before the constructor call
    //   CPosAli(CSeq_alignHandle& hali, const CSeq_id& protein, const CSeq_loc& genomic, const CProSplignOutputOptions& output_options, CNSeq& nseq, CPSeq& pseq, CNSeq& nseq1);

	CRef<CSeq_align> ToSeq_align(int comp_id = -1);// keeps comp_id as a score. Doesn't keep if '-1', 
	void PopulateDense_seg(CDense_seg& ds, vector<CAliPiece>::const_iterator& spit, int sshift, int nulpos, int nultripos, vector<CAliPiece>::const_iterator& epit, int eshift, ENa_strand nstrand);
    int NucPosOut(int pos) const;
    //checks if three (nucleotide) basis right after last protein residue equal to (TGA or TAA or TAG)
    bool HasStopOnNuc(void) const;
    void SetHasStopOnNuc(const CInfo& info);
};

class CAliCreator
{
public:
    CAli& m_ali;
    EAliPieceType m_CurType;
    int m_CurLen;

    CAliCreator(CAli& ali);
    ~CAliCreator(void);
    inline void Fini(void)
    {
        Clear();
        reverse(m_ali.m_ps.begin(), m_ali.m_ps.end());  
    }
    inline void Add(EAliPieceType type, int len)
    {
        if(m_CurType != type) {
            Clear();
            m_CurType = type;
        }
        m_CurLen+=len;
    }
private:
    inline void Clear(void) 
    { 
        if(m_CurLen) {
            CAliPiece pc;
            pc.Init(m_CurType);
            pc.m_len = m_CurLen;
            m_ali.m_ps.push_back(pc);
        }
        m_CurLen = 0; 
    }
};

class CSeq_alignHandle 
{
private:
    CRef<CSeq_align> m_sali;
public:
    CSeq_alignHandle(CRef<CSeq_align> sa) { m_sali = sa; }
    int GetCompNum(void); // returns -1 if not found
    string GetProtId(void);
    string GetNucId(void);
    bool GetStrand(void);
    //    friend CPosAli::CPosAli(CSeq_alignHandle& hali, const CSeq_id& protein,  const CSeq_loc& genomic, const CProSplignOutputOptions& output_options, CNSeq& nseq, CPSeq& pseq, CNSeq& nseq1);
};

END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif //PROSPLIGN_ALI_HPP
