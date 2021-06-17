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
 * Authors:  Melvin Quintos
 *
 * File Description:
 *  Provides implementation of CSnpBitfield class.  See snp_bitfield.hpp
 *  for class usage.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/snputil/snp_bitfield.hpp>

#include "snp_bitfield_factory.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

///////////////////////////////////////////////////////////////////////////////
// File Globals / typedefs, etc
///////////////////////////////////////////////////////////////////////////////

// Used for GetVariationClassString
static const char * g_VARIATION_NAMES[] =
{ "UNKNOWN",
  "SNV",
  "INDEL",
  "HET",
  "MSAT",
  "NAMED",
  "NOVAR",
  "MIXED",
  "MNV",
  "Idenity",
  "Inversion",
  "DEL",
  "INS"
};

static const char * g_FXN_NAMES[] =
{
    "Unknown",
    "intron_variant",
    "splice_donor_variant",
    "splice_acceptor_variant",
    "UTR",
    "synonymous_variant",
    "nonsense_variant",
    "missense_variant",
    "frameshift_variant",

    // dbSNP 2.0 additions
    "In Gene",      // In gene segment Defined as sequence intervals covered by a gene ID but not having an aligned transcript. FxnCode = 11
    "2KB_upstream_variant",   // In 5' gene region FxnCode = 15
    "500B_downstream_variant",    // In 3' gene region FxnCode = 13
    "5_prime_UTR_variant",    // In 5' UTR Location is in an untranslated region (UTR). FxnCode = 55
    "3_prime_UTR_variant",    // In 3' UTR Location is in an untranslated region (UTR). FxnCode = 53
    "Multiple",      // Has multiple gene functions (i.e. fwd strand 5'near gene, rev strand 3'near gene)

    "stop_gained",
    "stop_lost"
};


///////////////////////////////////////////////////////////////////////////////
// Public Static Methods
///////////////////////////////////////////////////////////////////////////////
const char * CSnpBitfield::GetString(EVariationClass e)
{
    return g_VARIATION_NAMES[e];
}

const char * CSnpBitfield::GetString(EFunctionClass e)
{
    return g_FXN_NAMES[e];
}


bool CSnpBitfield::IsCompatible(EFunctionClass e1, EFunctionClass e2)
{
    bool compatible = false;

    // make the value of e1 always lower than e2
    if (e1 > e2) {
        EFunctionClass tmp = e1;
        e1 = e2;
        e2 = tmp;
    }

    // Handle the case of the same function class (silly, but necessary)
    if (e1 == e2) {
        compatible = true;
    }
    // Handle eUTR case
    else if(e1 == eUTR) {
        if (e2 == eInUTR3 || e2 == eInUTR5) {
            compatible = true;
        }
    }

    return compatible;
}

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
CSnpBitfield::CSnpBitfield()
{
    m_bitfield.reset(new CSnpBitfieldNull());
}

CSnpBitfield::CSnpBitfield(const CSeq_feat& feat)
{
    *this = feat;  // reuse operator=
}

CSnpBitfield::CSnpBitfield(const CSnpBitfield &rhs)
{
    *this = rhs;  // reuse operator=
}

CSnpBitfield & CSnpBitfield::operator= ( const CSnpBitfield &rhs )
{
    if(this == &rhs)
        return *this;

    m_bitfield.reset(rhs.m_bitfield->Clone());

    return *this;
}

CSnpBitfield & CSnpBitfield::operator=(const CSeq_feat& feat)
{
    IEncoding * ptr = CSnpBitfieldFactory::CreateBitfield(feat);

    m_bitfield.reset(ptr);

    return *this;
}

const char * CSnpBitfield::GetVariationClassString() const
{
    int v = GetVariationClass();
    return g_VARIATION_NAMES[v];
}

#define ADD_FXN_CLASS(fxn_class)    if(IsTrue(fxn_class)) { FunctionClassesLst.push_back(g_FXN_NAMES[fxn_class]); }

string CSnpBitfield::GetGenePropertyString() const
{
    string sFunctionClasses;
    list<string> FunctionClassesLst;

    // a SNP record may have several function classes, so we need to try all of them and 
    // concatenate corresponding strings
    ADD_FXN_CLASS(eIntron);
    ADD_FXN_CLASS(eDonor);
    ADD_FXN_CLASS(eAcceptor);
    ADD_FXN_CLASS(eUTR);
    ADD_FXN_CLASS(eSynonymous);
    ADD_FXN_CLASS(eNonsense);
    ADD_FXN_CLASS(eMissense);
    ADD_FXN_CLASS(eFrameshift);
    ADD_FXN_CLASS(eInGene);
    ADD_FXN_CLASS(eInGene5);
    ADD_FXN_CLASS(eInGene3);
    ADD_FXN_CLASS(eInUTR5);
    ADD_FXN_CLASS(eInUTR3);
    ADD_FXN_CLASS(eStopGain);
    ADD_FXN_CLASS(eStopLoss);
    return NStr::Join(FunctionClassesLst, ",");
}

///////////////////////////////////////////////////////////////////////////////
// Private Methods
///////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE
