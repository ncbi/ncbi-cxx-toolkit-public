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
 * Authors:  Frank Ludwig
 *
 * File Description:  
 *
 */

#ifndef OBJTOOLS_WRITERS___FEATURE_CONTEXT__HPP
#define OBJTOOLS_WRITERS___FEATURE_CONTEXT__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/feat_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ----------------------------------------------------------------------------
class NCBI_XOBJWRITE_EXPORT CGffFeatureContext
//  ----------------------------------------------------------------------------
{
public:
    CGffFeatureContext()
    {};

    CGffFeatureContext(
        const CFeat_CI& feat_iter,
        CBioseq_Handle bsh=CBioseq_Handle(), 
        CSeq_annot_Handle sah=CSeq_annot_Handle()) :
        m_ft(feat_iter), m_bsh(bsh), m_sah(sah),
        m_bSequenceIsGenomicRecord(false)
    {
        xAssignSequenceIsGenomicRecord();
    };

    feature::CFeatTree& FeatTree() { return m_ft; };
    CBioseq_Handle BioseqHandle() const { return m_bsh; };
    CSeq_annot_Handle AnnotHandle() const { return m_sah; };

    CMappedFeat FindBestGeneParent(const CMappedFeat& mf);
    bool IsSequenceGenomicRecord() const { return m_bSequenceIsGenomicRecord; };

protected:
    feature::CFeatTree m_ft;
    CMappedFeat m_mfLastIn, m_mfLastOut;
    CBioseq_Handle m_bsh;
    CSeq_annot_Handle m_sah;
    bool m_bSequenceIsGenomicRecord;

    void xAssignSequenceIsGenomicRecord();

private:
    CGffFeatureContext(const CGffFeatureContext&);
    CGffFeatureContext& operator=(const CGffFeatureContext&);
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___FEATURE_CONTEXT__HPP
