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
 * Authors:  Josh Cherry
 *
 * File Description:
 *
 */


#include <ncbi_pch.hpp>
#include <algo/sequence/prot_prop.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// simply count the occurrences of the different aa (ncbistdaa) types
// puts counts in aacount, returns total count
TSeqPos CProtProp::AACount(CSeqVector& v, vector<TSeqPos>& aacount)
{
    v.SetCoding(CSeq_data::e_Ncbistdaa);
    
    TSeqPos size = v.size();
    aacount.resize(26);
    for (int i = 0;  i < 26;  i++) {
        aacount[i] = 0;
    }

    for (CSeqVector_CI vit(v);  vit.GetPos() < size;  ++vit) {
        CSeqVector::TResidue res = *vit;
        aacount[res]++;
    }
    return size;
}


double CProtProp::GetProteinPI(CSeqVector& v)
{

    // amino acid count
    vector<TSeqPos> aacount;
    CProtProp::AACount(v, aacount);

    // first and last residues
    CSeqVector::TResidue nter = v[0];
    CSeqVector::TResidue cter = v[v.size() - 1];

    // use these to get pI
    
#define PH_MAX 14
#define PH_MIN 0
#define MAXLOOP 2000  // max. number of iterations for root-finding
#define EPSI 0.0001   // epsilon (desired precision)
    
    double phMin = PH_MIN;
    double phMax = PH_MAX;
    double phMid = (phMin + phMax) / 2;  // init to avoid compiler warning

    for (int i = 0;  i < MAXLOOP && (phMax - phMin) > EPSI;  i++) {
        phMid = phMin + (phMax - phMin) / 2;
        double charge = GetProteinCharge(aacount, nter, cter, phMid);
        if (charge > 0) {
            phMin = phMid;
        } else {
            phMax = phMid;
        }
    }
    return phMid;
}


//  pKas according to Expasy, by NCBIstdaa:
//  A  B  C  D  E  F  G  H  I  K  L  M  N  P  Q  R  S  T  V  W  X  Y  Z  U  *
//  note: we don't have values for U; use most common for N and C, ignore sidechain
static const double pKaN[26] =
{0, 7.59, 7.5, 7.5, 7.5, 7.7, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7, 7.5, 8.36, 7.5, 7.5, 6.93, 6.82, 7.44, 7.5, 7.5, 7.5, 7.5, 7.5, 0};
static const double pKaC[26] =
{0, 3.55, 3.55, 3.55, 4.55, 4.75, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 3.55, 0};
static const double pKaSide[26] =
{0, 0, 0, 9, 4.05, 4.45, 0, 0, 5.98, 0, 10, 0, 0, 0, 0, 0, 12.0, 0, 0, 0, 0, 0, 10, 0, 0, 0};
static const size_t maxRes = sizeof(pKaN) / sizeof(*pKaN) - 1;

double CProtProp::GetProteinCharge(const vector<TSeqPos>& aacount,
                        CSeqVector::TResidue nter,
                        CSeqVector::TResidue cter,
                        double pH)
{

    // N- and C-termini
    double cnter = exp10(-pH) / (exp10(-pKaN[nter]) + exp10(-pH));
    double ccter = exp10(-pKaC[cter]) / (exp10(-pKaC[cter]) + exp10(-pH));
    // basic aas
    double carg = aacount[16] * exp10(-pH) / (exp10(-pKaSide[16]) + exp10(-pH));
    double clys = aacount[10] * exp10(-pH) / (exp10(-pKaSide[10]) + exp10(-pH));
    double chis = aacount[8] * exp10(-pH) / (exp10(-pKaSide[8]) + exp10(-pH));
    // classic acidic aas
    double casp = aacount[4] * exp10(-pKaSide[4]) / (exp10(-pKaSide[4]) + exp10(-pH));
    double cglu = aacount[5] * exp10(-pKaSide[5]) / (exp10(-pKaSide[5]) + exp10(-pH));
    // mildly acidic aas
    double ccys = aacount[3] * exp10(-pKaSide[3]) / (exp10(-pKaSide[3]) + exp10(-pH));
    double ctyr = aacount[22] * exp10(-pKaSide[22]) / (exp10(-pKaSide[22]) + exp10(-pH));

    return carg + clys + chis + cnter -
        (casp + cglu + ctyr + ccys + ccter);
}

END_SCOPE(objects)
END_NCBI_SCOPE
