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

#ifndef GFF3_ANNOT_ASSEMBLER__HPP
#define GFF3_ANNOT_ASSEMBLER__HPP

#include <corelib/ncbifile.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include "feat_annot_assembler.hpp"
#include "featid_generator.hpp"
#include "gff3_import_data.hpp"

class CFeatureMap;
class CFeatureIdGenerator;

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class CGff3FeatureMap
//  ============================================================================
{
public:
    CGff3FeatureMap() {};

    ~CGff3FeatureMap() {};

    void
    AddFeature(
        const std::string& id,
        CRef<CSeq_feat> pFeature) 
    {
        auto itExisting = mMap.find(id);
        if (itExisting != mMap.end()) {
            return;
        }
        mMap[id] = pFeature;
    };


    CRef<CSeq_feat>
    FindFeature(
        const std::string& id)
    {
        auto it = mMap.find(id);
        if (it == mMap.end()) {
            return CRef<CSeq_feat>();
        }
        return it->second;
    };

private:
    std::map<std::string, CRef<CSeq_feat>> mMap;
};

//  ============================================================================
class CGff3PendingFeatureList
//  ============================================================================
{
public:
    CGff3PendingFeatureList() {};

    ~CGff3PendingFeatureList() {};

    void 
    AddFeature(
        const std::string& id,
        CRef<CSeq_feat> pFeature)
    {
        auto itFeatureVec = mPendingMap.find(id);
        if (itFeatureVec == mPendingMap.end()) {
            mPendingMap[id] = vector<CRef<CSeq_feat>>();
        }
        auto& feats = mPendingMap[id];
        auto itFeature = find(feats.begin(), feats.end(), pFeature);
        if (itFeature == feats.end()) {
            feats.push_back(pFeature);
        }
    };

    bool
    FindPendingFeatures(
        const std::string& id,
        std::vector<CRef<CSeq_feat>>& features)
    {
        auto itPending = mPendingMap.find(id);
        if (itPending == mPendingMap.end()) {
            return false;
        }
        features = itPending->second;
        return true;
    };

    void
    MarkFeaturesDone(
        const std::string& id)
    {
        mPendingMap.erase(id);
    };

private:
    std::map<std::string, std::vector<CRef<CSeq_feat>>> mPendingMap;
};


//  ============================================================================
class CGff3AnnotAssembler:
    public CFeatAnnotAssembler
//  ============================================================================
{
public:
    CGff3AnnotAssembler(
        CImportMessageHandler&);

    virtual ~CGff3AnnotAssembler();

    void
    ProcessRecord(
        const CFeatImportData&,
        CSeq_annot&) override;

    virtual void
    FinalizeAnnot(
        const CAnnotImportData&,
        CSeq_annot&) override;

private:
    void
    xProcessFeatureDefault(
        const std::string&,
        const std::string&,
        CRef<CSeq_feat>,
        CSeq_annot&);

    void
    xProcessFeatureExon(
        const std::string&,
        const std::string&,
        CRef<CSeq_feat>,
        CSeq_annot&);

    void
    xProcessFeatureRna(
        const std::string&,
        const std::string&,
        CRef<CSeq_feat>,
        CSeq_annot&);

    void
    xProcessFeatureCds(
        const std::string&,
        const std::string&,
        CRef<CSeq_feat>,
        CSeq_annot&);

    void xAnnotGenerateXrefs(
        CSeq_annot&);

    static void xMarkLocationPending(
        CSeq_feat&);
    static bool xIsLocationPending(
        const CSeq_feat&);
    static void xUnmarkLocationPending(
        CSeq_feat&);

    CGff3FeatureMap mFeatureMap;
    CGff3PendingFeatureList mPendingFeatures;
    map<string,string> mXrefMap;
    CFeatureIdGenerator mIdGenerator;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif
