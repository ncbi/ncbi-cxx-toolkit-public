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
#include <iostream>

#include <algo/align/prosplign/prosplign.hpp>

BEGIN_NCBI_SCOPE

class CProteinAlignText;

BEGIN_SCOPE(prosplign)

class CNPiece {//AKA 'good hit'
public:
    int beg, end;  //represents [beg, end) interval IN ALIGNMENT COORD.
    int posit, efflen;

    CNPiece(string::size_type obeg, string::size_type oend, int oposit, int oefflen);
};

/// Extended output filtering parameters
/// deprecated, used in older programs
class CProSplignOutputOptionsExt : public CProSplignOutputOptions {
public:
    CProSplignOutputOptionsExt(const CProSplignOutputOptions& options);

    int drop;
    int splice_cost;

    bool Dropof(int efflen, int posit, list<prosplign::CNPiece>::iterator it);
    void Join(list<prosplign::CNPiece>::iterator it, list<prosplign::CNPiece>::iterator last);
    bool Perc(list<prosplign::CNPiece>::iterator it, int efflen, int posit, list<prosplign::CNPiece>::iterator last);
    bool Bad(list<prosplign::CNPiece>::iterator it);
    bool ForwCheck(list<prosplign::CNPiece>::iterator it1, list<prosplign::CNPiece>::iterator it2);
    bool BackCheck(list<prosplign::CNPiece>::iterator it1, list<prosplign::CNPiece>::iterator it2);
};

class CProSplignScaledScoring;
class CSubstMatrix;

list<CNPiece> FindGoodParts(const CProteinAlignText& alignment_text, CProSplignOutputOptionsExt options, const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix);
list<CNPiece> ExcludeBadExons(const CNPiece pc, const string& match_all_pos, const string& protein, CProSplignOutputOptionsExt m_options);
list<CNPiece> FindGoodParts(const CNPiece pc, const string& match_all_pos, const string& protein, CProSplignOutputOptionsExt m_options);
//tries to trim negative score tail, returns true if trimmed, false otherwise
//stop at intron. Score frameshift as regular gap
bool TrimNegativeTail(CNPiece& pc, const CProteinAlignText& alignment_text, const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix);

void RefineAlignment(objects::CScope& scope, objects::CSeq_align& seq_align, const list<CNPiece>& good_parts);
void SetScores(objects::CSeq_align& seq_align, objects::CScope& scope, const string& matrix_name = "BLOSUM62");

// class CAli;
// class CSubstMatrix;

// class CInfo
// {
// public:
//     CInfo(const CAli& ali, const CSubstMatrix& matrix);
//     ~CInfo(void);

// //returns next nucleotide  after given alignment position or ncoor.back() if none
//     //or seqlen if none (NULL BASED), works for [-1:alig_len-1] range
//     int NextNucNullBased(int alipos) const { 
//         if(alipos<0) return 0;
//         return ncoor[alipos]; 
//     }

//     //'good' pieces. many methods imply that 
//     //the pieces begin and and on match (positive?)
//     //careful with type 'full'
//     list<CNPiece> m_AliPiece;
//     void Cut(CProSplignOutputOptionsExt output_options = CProSplignOutputOptions());

// private:

//     //alignment coords -> sequence coords.
//     //ONE BASED (designed specially for output), for initial gap value is 0
//     vector<int> ncoor;

//     string outn, outr, match, outp;

//     void InitAlign(const CAli& ali, const CSubstMatrix& matrix);
// };

END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif //PROSPLIGN_INFO_HPP
