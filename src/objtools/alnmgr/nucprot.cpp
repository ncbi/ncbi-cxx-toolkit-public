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

#include <ncbi_pch.hpp>

#include <corelib/ncbistd.hpp>

#include <objtools/alnmgr/nucprot.hpp>

#include <util/tables/raw_scoremat.h>

BEGIN_NCBI_SCOPE

enum Nucleotides { nA, nC, nG, nT, nN };

CSubstMatrix::CSubstMatrix(const string& matrix_name, int scaling)
{
    const SNCBIPackedScoreMatrix* packed_mtx =
        NCBISM_GetStandardMatrix(matrix_name.c_str());
    if (packed_mtx == NULL)
        NCBI_THROW(CException, eUnknown, "unknown scoring matrix: "+matrix_name);

    m_alphabet = packed_mtx->symbols;
    m_alphabet = NStr::ToUpper(m_alphabet);

    if (string::npos == m_alphabet.find('X'))
        NCBI_THROW(CException, eUnknown, "unsupported scoring matrix: "+matrix_name);

    SNCBIFullScoreMatrix mtx;
    NCBISM_Unpack(packed_mtx, &mtx);

    for(int i = 0; i < 256;  ++i) {
        for(int j = 0; j < 256;  ++j) {
            scaled_subst_matrix[i][j] = packed_mtx->defscore*scaling;
        }
    }

    int score;
    for(const char* p1 = packed_mtx->symbols; *p1; ++p1) {
        int c = toupper(*p1);
        int lc = tolower(c);
        for(const char* p2 = packed_mtx->symbols; *p2; ++p2) {
            int d = toupper(*p2);
            int ld = tolower(d);
            score = mtx.s[(int)(*p1)][(int)(*p2)]*scaling;
            scaled_subst_matrix[c][d] = score;
            scaled_subst_matrix[lc][ld] = score;
            scaled_subst_matrix[c][ld] = score;
            scaled_subst_matrix[lc][d] = score;
    }}
}

void CSubstMatrix::SetTranslationTable(const CTranslationTable* trans_table)
{
    m_trans_table.Reset(trans_table);
}

CTranslationTable::CTranslationTable(int gcode) : m_trans_table(objects::CGen_code_table::GetTransTable(gcode))
{
    for(int i=0; i<5; ++i)
    for(int j=0; j<5; ++j)
    for(int k=0; k<5; ++k)
        aa_table[i*(8*8)+j*8+k] =
            TranslateTriplet(NucToChar(i),NucToChar(j),NucToChar(k));
}

int CTranslationTable::CharToNuc(char c) {
  if(c == 'A' || c == 'a') return nA;
  if(c == 'C' || c == 'c') return nC;
  if(c == 'G' || c == 'g') return nG;
  if(c == 'T' || c == 't') return nT;
  return nN;
}

char CTranslationTable::NucToChar(int n) {
    if(n == nA) return 'A';
    if(n == nT) return 'T';
    if(n == nG) return 'G';
    if(n == nC) return 'C';
    return 'N';
}

END_NCBI_SCOPE
