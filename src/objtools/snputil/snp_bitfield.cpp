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
  "SNP",
  "INDEL",
  "HET",
  "MSAT",
  "NAMED",
  "NOVAR",
  "MIXED",
  "MNP",
  "Idenity",
  "Inversion",
  "Deletion",
  "Insertion"
};

static const char * g_FXN_NAMES[] =
{
    "Unknown",
    "Intron",
    "Donor",
    "Acceptor",
    "UTR",
    "Synonymous",
    "Nonsense",
    "Missense",
    "Frameshift",

    // Version 2 additions
    "In Gene",      // In gene segment Defined as sequence intervals covered by a gene ID but not having an aligned transcript. FxnCode = 11
    "In 5' Gene",   // In 5' gene region FxnCode = 15
    "In 3' Gene",    // In 3' gene region FxnCode = 13
    "In 5' UTR",    // In 5' UTR Location is in an untranslated region (UTR). FxnCode = 55
    "In 3' UTR",    // In 3' UTR Location is in an untranslated region (UTR). FxnCode = 53
    "Multiple",      // Has multiple gene functions (i.e. fwd strand 5'near gene, rev strand 3'near gene)

    "STOP-Gain",
    "STOP-Loss"
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

const char * CSnpBitfield::GetGenePropertyString() const
{
    int v = GetFunctionClass();
    return g_FXN_NAMES[v];
}

///////////////////////////////////////////////////////////////////////////////
// Private Methods
///////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE
