/*  $Id$
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
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * =========================================================================
 *
 * Authors: Colleen Bollin, based on similar discrepancy tests
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include "discrepancy_core.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(biosource_tests);


// MAP_CHROMOSOME_CONFLICT

const string kMapChromosomeConflict = "[n] source[s] on eukaryotic sequence[s] [has] map but not chromosome";


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MAP_CHROMOSOME_CONFLICT, CSeqdesc, eDisc | eOncaller, "Eukaryotic sequences with a map source qualifier should also have a chromosome source qualifier")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSource() || !context.IsEukaryotic() || !obj.GetSource().IsSetSubtype()) {
        return;
    }

    bool has_map = false;
    bool has_chromosome = false;

    ITERATE(CBioSource::TSubtype, it, obj.GetSource().GetSubtype()) {
        if ((*it)->IsSetSubtype()) {
            if ((*it)->GetSubtype() == CSubSource::eSubtype_map) {
                has_map = true;
            } else if ((*it)->GetSubtype() == CSubSource::eSubtype_chromosome) {
                has_chromosome = true;
                break;
            }
        } 
    }

    if (has_map && !has_chromosome) {
        m_Objs[kMapChromosomeConflict].Add(*context.NewDiscObj(CConstRef<CSeqdesc>(&obj)),
            false).Fatal();
    }

}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MAP_CHROMOSOME_CONFLICT)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}



END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
