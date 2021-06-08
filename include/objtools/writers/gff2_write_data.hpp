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

#ifndef OBJTOOLS_WRITERS___GFF2DATA__HPP
#define OBJTOOLS_WRITERS___GFF2DATA__HPP

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/writers/write_util.hpp>
#include <objtools/writers/feature_context.hpp>
#include <objtools/writers/gff_feature_record.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGffWriteRecord
//  ============================================================================
    : public CGffFeatureRecord
{
public:
    typedef map<string, vector<string> > TAttributes;
    typedef TAttributes::iterator TAttrIt;
    typedef TAttributes::const_iterator TAttrCit;

public:
    CGffWriteRecord(
        CGffFeatureContext& fc,
        const string& id="" );
    CGffWriteRecord(
        const CGffWriteRecord& );
    virtual ~CGffWriteRecord();

    //
    //  Input/output:
    //
    bool CorrectType(
        const string& strType ) {
        mType = strType;
        return true;
    };

    virtual bool AssignFromAsn(
        const CMappedFeat&,
        unsigned int =0);

    virtual string StrStructibutes() const { return ""; };

protected:
    virtual bool x_AssignAttributes(
        const CMappedFeat&,
        unsigned int =0);

    virtual void x_StrAttributesAppendValue(
        const string&,
        const string&,
        const string&,
        map<string, vector<string> >&,
        string& ) const;

    CGffFeatureContext& m_fc;

    static const char* ATTR_SEPARATOR;
};

using CGffWriteRecordFeature = CGffWriteRecord;
using CGtfFeatureRecord = CGffWriteRecord;

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GFF2DATA__HPP
