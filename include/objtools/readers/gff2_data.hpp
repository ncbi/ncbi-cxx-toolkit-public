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

#ifndef OBJTOOLS_READERS___GFF2DATA__HPP
#define OBJTOOLS_READERS___GFF2DATA__HPP

#include <objtools/readers/gff_base_columns.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
class CGff2Record:
//  ----------------------------------------------------------------------------
    public CGffBaseColumns
{
public:
    //typedef CCdregion::EFrame TFrame;
    typedef map<string, string> TAttributes;
    typedef TAttributes::iterator TAttrIt;
    typedef TAttributes::const_iterator TAttrCit;
    using SeqIdResolver = CRef<CSeq_id> (*)(const string&, unsigned int, bool);


public:
    CGff2Record(): CGffBaseColumns() {};
    CGff2Record(
            const CGff2Record& rhs):
        CGffBaseColumns(rhs)
    {
        m_Attributes.insert(rhs.m_Attributes.begin(), rhs.m_Attributes.end());
    };

    virtual ~CGff2Record() {};

    //
    //  Input/output:
    //
    virtual bool AssignFromGff(
        const string& );

    //
    // Accessors:
    //
    bool IsAlignmentRecord() const {
        if (NStr::StartsWith(Type(), "match") ||
            NStr::EndsWith(Type(), "_match")) {
            return true;
        }
        return false;
    };

    const TAttributes& Attributes() const {
        return m_Attributes;
    };

    bool GetAttribute(
        const string&,
        string& ) const;

    bool GetAttribute(
        const string&,
        list<string>& ) const;

    virtual bool InitializeFeature(
        int,
        CRef<CSeq_feat>,
        SeqIdResolver = nullptr ) const;

    virtual bool UpdateFeature(
        int,
        CRef<CSeq_feat>,
        SeqIdResolver = nullptr ) const;

    bool IsMultiParent() const;

    static void TokenizeGFF(
        vector<CTempStringEx>& columns,
        const CTempStringEx& line);

protected:
    virtual bool xAssignAttributesFromGff(
        const string&,
        const string& );

    bool xSplitGffAttributes(
        const string&,
        vector< string >& ) const;

    virtual bool xMigrateAttributes(
        int,
        CRef<CSeq_feat> ) const;

    virtual bool xInitFeatureData(
        int,
        CRef<CSeq_feat>) const;

    virtual bool xUpdateFeatureData(
        int,
        CRef<CSeq_feat>,
        SeqIdResolver = nullptr ) const;

    virtual bool xMigrateAttributesSubSource(
        int,
        CRef<CSeq_feat>,
        TAttributes& ) const;

    virtual bool xMigrateAttributesOrgName(
        int,
        CRef<CSeq_feat>,
        TAttributes& ) const;

    virtual bool xMigrateAttributesGo(
        int,
        CRef<CSeq_feat>,
        TAttributes& ) const;

    //utility helpers:
    //
    static string xNormalizedAttributeKey(
        const CTempString&);

    static string xNormalizedAttributeValue(
        const CTempString&);

    static bool xMigrateAttributeDefault(
        TAttributes&,
        const string&,
        CRef<CSeq_feat>,
        const string&,
        int);

    static bool xMigrateAttributeSingle(
        TAttributes&,
        const string&,
        CRef<CSeq_feat>,
        const string&,
        int);

    //
    // Data:
    //
    string m_strAttributes;
    TAttributes m_Attributes;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___GFF2DATA__HPP
