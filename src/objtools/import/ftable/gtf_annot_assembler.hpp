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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   Test application for the CFormatGuess component
*
* ===========================================================================
*/

#ifndef GTF_ANNOT_ASSEMBLER__HPP
#define GTF_ANNOT_ASSEMBLER__HPP

#include <corelib/ncbifile.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include "featid_generator.hpp"
#include "feat_annot_assembler.hpp"
#include "gtf_import_data.hpp"

class CFeatureMap;
class CFeatureIdGenerator;

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class CGtfAnnotAssembler:
    public CFeatAnnotAssembler
//  ============================================================================
{
public:
    CGtfAnnotAssembler(
        CImportMessageHandler&);

    virtual ~CGtfAnnotAssembler();

    void
    ProcessRecord(
        const CFeatImportData&,
        CSeq_annot&) override;

    virtual void
    FinalizeAnnot(
        const CAnnotImportData&,
        CSeq_annot&) override;

private:
    void xProcessRecordGene(
        const CGtfImportData&,
        CSeq_annot&);
    void xProcessRecordMrna(
        const CGtfImportData&,
        CSeq_annot&);
    void xProcessRecordCds(
        const CGtfImportData&,
        CSeq_annot&);

    void xCreateGene(
        const CGtfImportData&,
        CRef<CSeq_feat>&,
        CSeq_annot&);
    void xCreateMrna(
        const CGtfImportData&,
        CRef<CSeq_feat>&,
        CSeq_annot&);
    void xCreateCds(
        const CGtfImportData&,
        CRef<CSeq_feat>&,
        CSeq_annot&);

    void xUpdateGene(
        const CGtfImportData&,
        CRef<CSeq_feat>&,
        CSeq_annot&);
    void xUpdateMrna(
        const CGtfImportData&,
        CRef<CSeq_feat>&,
        CSeq_annot&);
    void xUpdateCds(
        const CGtfImportData&,
        CRef<CSeq_feat>&,
        CSeq_annot&);

    void xFeatureSetGene(
        const CGtfImportData&,
        CRef<CSeq_feat>&);
    void xFeatureSetMrna(
        const CGtfImportData&,
        CRef<CSeq_feat>&);
    void xFeatureSetCds(
        const CGtfImportData&,
        CRef<CSeq_feat>&);

    void xFeatureSetLocation(
        const CGtfImportData&,
        CRef<CSeq_feat>&);

    void xFeatureSetFeatId(
        const CGtfImportData&,
        CRef<CSeq_feat>&);

    void xFeatureUpdateLocation(
        const CGtfImportData&,
        CRef<CSeq_feat>&);

    void xFeatureSetQualifiers(
        const CGtfImportData&,
        CRef<CSeq_feat>&);

    void xAnnotAddFeature(
        const CGtfImportData&,
        CRef<CSeq_feat>&,
        CSeq_annot&);

    void xAnnotGenerateXrefs(
        CSeq_annot&);

    unique_ptr<CFeatureMap> mpFeatureMap;
    unique_ptr<CFeatureIdGenerator> mpIdGenerator;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif
