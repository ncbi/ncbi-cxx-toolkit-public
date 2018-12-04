#ifndef VALIDATOR___DUP_FEATS__HPP
#define VALIDATOR___DUP_FEATS__HPP

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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   for removing duplicate features
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <serial/objectinfo.hpp>
#include <serial/serialbase.hpp>
#include <objmgr/scope.hpp>

#include <map>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioseq_Handle;
class CSeq_feat_Handle;
class CSeq_entry_Handle;
class CScope;

BEGIN_SCOPE(validator)

void NCBI_VALIDATOR_EXPORT GetProductToCDSMap(CScope &scope, map<CBioseq_Handle, set<CSeq_feat_Handle> > &product_to_cds);
set< CSeq_feat_Handle > NCBI_VALIDATOR_EXPORT GetDuplicateFeaturesForRemoval(CSeq_entry_Handle seh);
set< CBioseq_Handle > NCBI_VALIDATOR_EXPORT ListOrphanProteins(CSeq_entry_Handle seh, bool force_refseq = false);
bool AllowOrphanedProtein(const CBioseq& seq, bool force_refseq = false);



END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___DUP_FEATS__HPP */
