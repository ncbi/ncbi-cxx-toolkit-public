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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   GFF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/so_map.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Variation_ref.hpp>

#include <objtools/readers/gff_base_columns.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
CGffBaseColumns::CGffBaseColumns():
//  ============================================================================
    m_strId(""),
    m_uSeqStart(0),
    m_uSeqStop(0),
    m_strSource(""),
    m_strType(""),
    m_pdScore(nullptr),
    m_peStrand(nullptr),
    m_pePhase(nullptr)
{
};

//  ============================================================================
CGffBaseColumns::CGffBaseColumns(
//  ============================================================================
    const CGffBaseColumns& rhs):
    m_strId(rhs.m_strId),
    m_uSeqStart(rhs.m_uSeqStart),
    m_uSeqStop(rhs.m_uSeqStop),
    m_strSource(rhs.m_strSource),
    m_strType(rhs.m_strType),
    m_pdScore(nullptr),
    m_peStrand(nullptr),
    m_pePhase(nullptr)
{
    if (rhs.m_pdScore) {
        m_pdScore = new double(rhs.Score());
    }
    if (rhs.m_peStrand) {
        m_peStrand = new ENa_strand(rhs.Strand());
    }
    if (rhs.m_pePhase) {
        m_pePhase = new TFrame(rhs.Phase());
    }
};

//  ============================================================================
CGffBaseColumns::~CGffBaseColumns()
//  ============================================================================
{
    delete m_pdScore;
    delete m_peStrand;
    delete m_pePhase; 
};


END_objects_SCOPE

END_NCBI_SCOPE
