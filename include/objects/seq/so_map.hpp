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
 * File Description:  Sequence Ontology Type Mapping
 *
 */

#ifndef OBJECTS___SOMAP__HPP
#define OBJECTS___SOMAP__HPP

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CSeq_feat;

//  ============================================================================
class CompareNoCase
//  ============================================================================
{
public:
    bool operator()(const string& x, const string& y) const;
};
    
//  ============================================================================
class NCBI_SEQ_EXPORT CSoMap
//  ============================================================================
{
public:
    static std::string SoTypeToId(
        const std::string&);
    static std::string SoIdToType(
        const std::string&);

    static bool SoTypeToFeature(
        const string&,
        CSeq_feat&,
        bool =false);

    static bool FeatureToSoType(
        const CSeq_feat&,
        string&);

    static bool GetSupportedSoTerms(
        vector<string>&);

    static string ResolveSoAlias(
        const string&);

protected:
    static bool xCompareNoCase(const string&, const string&);

    static bool xFeatureMakeGene(const string&, CSeq_feat&);
    static bool xFeatureMakeCds(const string&, CSeq_feat&);
    static bool xFeatureMakeRna(const string&, CSeq_feat&);
    static bool xFeatureMakeMiscFeature(const string&, CSeq_feat&);
    static bool xFeatureMakeMiscRecomb(const string&, CSeq_feat&);
    static bool xFeatureMakeMiscRna(const string&, CSeq_feat&);
    static bool xFeatureMakeNcRna(const string&, CSeq_feat&);
    static bool xFeatureMakeImp(const string&, CSeq_feat&);
    static bool xFeatureMakeProt(const string&, CSeq_feat&);
    static bool xFeatureMakeRegion(const string&, CSeq_feat&);
    static bool xFeatureMakeRegulatory(const string&, CSeq_feat&);
    static bool xFeatureMakeRepeatRegion(const string&, CSeq_feat&);

    static bool xMapGeneric(const CSeq_feat&, string&);
    static bool xMapGene(const CSeq_feat&, string&);
    static bool xMapCds(const CSeq_feat&, string&);
    static bool xMapMiscFeature(const CSeq_feat&, string&);
    static bool xMapMiscRecomb(const CSeq_feat&, string&);
    static bool xMapRna(const CSeq_feat&, string&);
    static bool xMapNcRna(const CSeq_feat&, string&);
    static bool xMapOtherRna(const CSeq_feat&, string&);
    static bool xMapRegion(const CSeq_feat&, string&);
    static bool xMapRegulatory(const CSeq_feat&, string&);
    static bool xMapRepeatRegion(const CSeq_feat&, string&);
    static bool xMapBond(const CSeq_feat&, string&);


    using TYPEMAP = map<string, string, CompareNoCase>;
    using TYPEENTRY = TYPEMAP::const_iterator;
    static TYPEMAP mMapSoTypeToId;
    static TYPEMAP mMapSoIdToType;

    using FEATFUNC = bool(*)(const string&, CSeq_feat&);
    using FEATFUNCMAP = map<string, FEATFUNC, CompareNoCase>;
    using FEATFUNCENTRY = FEATFUNCMAP::const_iterator;
    static FEATFUNCMAP mMapFeatFunc;

    using TYPEFUNC = bool(*)(const CSeq_feat&, string&);
    using TYPEFUNCMAP = map<CSeqFeatData::ESubtype, TYPEFUNC>;
    using TYPEFUNCENTRY = TYPEFUNCMAP::const_iterator;
    static TYPEFUNCMAP mMapTypeFunc;

    using SOALIASMAP = map<string, string, CompareNoCase>;
    using ALIASENTRY = SOALIASMAP::const_iterator;
    static SOALIASMAP mMapSoAliases;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJECTS___SOMAP__HPP
