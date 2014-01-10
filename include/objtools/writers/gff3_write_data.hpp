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
 * Author: Frank Ludwig
 *
 * File Description:
 *   GFF3 transient data structures
 *
 */

#ifndef OBJTOOLS_WRITERS___GFF3DATA__HPP
#define OBJTOOLS_WRITERS___GFF3DATA__HPP

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/feature.hpp>
#include <objtools/writers/gff2_write_data.hpp>
#include <objects/seqfeat/Cdregion.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
class CGff3WriteRecordFeature
//  ----------------------------------------------------------------------------
    : public CGffWriteRecordFeature
{
public:
    typedef CCdregion::EFrame TFrame;

public:
    CGff3WriteRecordFeature(
        CGffFeatureContext&,
        const string& id=""
    );

    CGff3WriteRecordFeature(
        const CGff3WriteRecordFeature&
    );

    virtual ~CGff3WriteRecordFeature();

    virtual string StrAttributes() const;

    virtual bool AssignFromAsn(
        CMappedFeat,
        unsigned int =0);

    virtual bool AssignFromAsnLinear(
        CMappedFeat,
        unsigned int =0);

    bool AssignParent(
        const CGff3WriteRecordFeature& );

    void ForceAttributeID(
        const string& );

    CRef<CSeq_loc> GetCircularLocation() const { return m_pLoc; };

    virtual bool NeedsQuoting(
        const string& str) const {return false;};

protected:
    virtual bool x_AssignType(
        CMappedFeat,
        unsigned int =0 );
    virtual bool x_AssignStart(
        CMappedFeat );
    virtual bool x_AssignStop(
        CMappedFeat );

    virtual bool x_AssignAttributes(
        CMappedFeat,
        unsigned int =0);
    virtual bool x_AssignAttributesFromAsnCore(
        CMappedFeat );
    virtual bool x_AssignAttributesFromAsnExtended(
        CMappedFeat );
    virtual bool x_AssignAttributesExtraQuals(
        CMappedFeat );

    virtual bool x_AssignAttributesMrna(
        CMappedFeat );
    virtual bool x_AssignAttributesTrna(
        CMappedFeat );
    virtual bool x_AssignAttributesCds(
        CMappedFeat );
    virtual bool x_AssignAttributesMiscFeature(
        CMappedFeat );
    virtual bool x_AssignAttributesGene(
        CMappedFeat );
    virtual bool x_AssignAttributesNcrna(
        CMappedFeat );

    virtual bool x_AssignAttributeGene(
        CMappedFeat );
    virtual bool x_AssignAttributeGeneDesc(
        CMappedFeat );
    virtual bool x_AssignAttributeMapLoc(
        CMappedFeat );
    virtual bool x_AssignAttributeNote(
        CMappedFeat );
    virtual bool x_AssignAttributePartial(
        CMappedFeat );
    virtual bool x_AssignAttributePseudo(
        CMappedFeat );
    virtual bool x_AssignAttributeDbXref(
        CMappedFeat );
    virtual bool x_AssignAttributeGeneSynonym(
        CMappedFeat );
    virtual bool x_AssignAttributeLocusTag(
        CMappedFeat );
    virtual bool x_AssignAttributeProduct(
        CMappedFeat );
    virtual bool x_AssignAttributeCodonStart(
        CMappedFeat );
    virtual bool x_AssignAttributeEvidence(
        CMappedFeat );
    virtual bool x_AssignAttributeException(
        CMappedFeat );
    virtual bool x_AssignAttributeModelEvidence(
        CMappedFeat );
    virtual bool x_AssignAttributeGbKey(
        CMappedFeat );
    virtual bool x_AssignAttributeTranscriptId(
        CMappedFeat );
    virtual bool x_AssignAttributeProteinId(
        CMappedFeat );
    virtual bool x_AssignAttributeExonNumber(
        CMappedFeat );
    virtual bool x_AssignAttributeNcrnaClass(
        CMappedFeat );
    virtual bool x_AssignAttributeTranslationTable(
        CMappedFeat );
    virtual bool x_AssignAttributeCodeBreak(
        CMappedFeat );
    virtual bool x_AssignAttributeOldLocusTag(
        CMappedFeat );
    virtual bool x_AssignAttributeName(
        CMappedFeat );
    virtual bool x_AssignAttributeIsOrdered(
        CMappedFeat );

    virtual void x_StrAttributesAppendValueGff3(
        const string&,
        map<string, vector<string> >&,
        string& ) const;

protected:
    CRef<CSeq_loc> m_pLoc;

    static const char* ATTR_SEPARATOR;
    static const char* MULTIVALUE_SEPARATOR;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GFF3DATA__HPP
