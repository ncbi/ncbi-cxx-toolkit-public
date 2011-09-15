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

#ifndef OBJTOOLS_NUCPROT_H
#define OBJTOOLS_NUCPROT_H

#include <corelib/ncbi_limits.hpp>

#include <algorithm>
#include <sstream>

#include <objects/seqfeat/Genetic_code_table.hpp>

BEGIN_NCBI_SCOPE

class NCBI_XALNMGR_EXPORT CTranslationTable : public CObject {
public:
    CTranslationTable(int gcode);

    static int CharToNuc(char c);
    static char NucToChar(int n);

    inline char nuc2a(int nuc1, int nuc2, int nuc3) const
    {
        return aa_table[nuc1*(8*8)+nuc2*8+nuc3]; // need 5, use 8 for speed
    }

    char TranslateTriplet(char n1, char n2, char n3) const
    {
        return m_trans_table.GetCodonResidue(m_trans_table.SetCodonState(n1, n2, n3));
    }

    char TranslateTriplet(const string& triplet) const
    {
        return TranslateTriplet(triplet[0],triplet[1],triplet[2]);
    }

private:
    const objects::CTrans_table& m_trans_table;
    char aa_table[8*8*8];
};

/// Substitution Matrix for Scoring Amino-Acid Alignments
class NCBI_XALNMGR_EXPORT CSubstMatrix {
public:
    CSubstMatrix(const string& name, int scaling);

    void SetTranslationTable(const CTranslationTable* trans_table);

    inline int MultScore(int nuc1, int nuc2, int nuc3, char amin) const { return scaled_subst_matrix[int(amin)][int(m_trans_table->nuc2a(nuc1, nuc2, nuc3))]; }

    inline int ScaledScore(char amin1, char amin2) const { return scaled_subst_matrix[int(amin1)][int(amin2)]; }

    string m_alphabet;
private:
    int scaled_subst_matrix[256][256];
    CConstRef<CTranslationTable> m_trans_table;
};

END_NCBI_SCOPE

#endif //OBJTOOLS_NUCPROT_H
