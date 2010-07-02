#ifndef WEIGHT__HPP
#define WEIGHT__HPP

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
* Author:  Aaron Ucko
*
* File Description:
*   Molecular weights for protein sequences
*/

#include <map>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioseq_Handle;
class CSeq_loc;
class CSeqVector;


/** @addtogroup ObjUtilWeight
 *
 * @{
 */


/// Handles the standard 20 amino acids and Sec; treats Asx as Asp and
/// Glx as Glu; throws CObjmgrUtilException on anything else.
///
/// NOTE: The molecular weight excludes a leading 'M' on proteins with a known
/// valid start, provided that the location is omitted or the location begins
/// at residue 0
NCBI_XOBJUTIL_EXPORT
double GetProteinWeight(const CBioseq_Handle& handle,
                        const CSeq_loc* location = 0);

NCBI_XOBJUTIL_EXPORT
double GetProteinWeight(const CSeq_feat& prot_feat,
                        CScope& scope,
                        const CSeq_loc* location = 0);

NCBI_XOBJUTIL_EXPORT
double GetProteinWeight(const string& iupac_aa_sequence);

typedef map<CConstRef<CSeq_loc>, double> TWeights;

/// Automatically picks reasonable ranges: in decreasing priority order,
/// - Annotated cleavage products (mature peptides)
/// - What's left after removing the first signal peptide found
/// - The entire sequence (skipping a leading methionine if present)
NCBI_XOBJUTIL_EXPORT
void GetProteinWeights(const CBioseq_Handle& handle, TWeights& weights);

/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* WEIGHT__HPP */
