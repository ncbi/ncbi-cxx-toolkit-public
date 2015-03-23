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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Colleen Bollin
 * Created:  17/09/2013 15:38:53
 */


#ifndef _REPORT_OBJECT_H_
#define _REPORT_OBJECT_H_

#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);
BEGIN_SCOPE(DiscRepNmSpc)


class CReportObject;

typedef vector<CRef< CReportObject> > TReportObjectList;


class NCBI_DISCREPANCY_EXPORT CReportObject : public CObject
{
public:
    CReportObject() : m_Object(NULL) {}
    CReportObject(CConstRef<CObject> obj) : m_Object(obj) {}
    ~CReportObject() {}

    const string& GetText() const { return m_Text; }
    const string& GetFeatureTable() const { return m_FeatureTable; }
    const string& GetXML() const {return m_XML; }

    void SetText(CScope& scope);
    void SetFeatureTable(CScope& scope);
    void SetXML(CScope& scope);
    void Format(CScope& scope);

    CConstRef<CObject> GetObject() const { return m_Object; }
    void SetObject(CConstRef<CObject> obj) { m_Object = obj; }

    // if we have read in Seq-entries from multiple files, the 
    // report should include the filename that the object was
    // originally found in
    string GetFilename() const { return m_Filename; }
    void SetFilename(const string& filename) { m_Filename = filename; }

    // the DropReferences methods save text representations of the objects
    // and reset references to underlying objects.
    // This makes it possible to allow the Seq-entry (and all the objects 
    // used to create the text representations) to go out of scope and not be
    // stored in memory
    void DropReference();
    void DropReference(CScope& scope);

    static string GetTextObjectDescription(CConstRef<CObject> obj, CScope& scope);
    static string GetFeatureTableObjectDescription(CConstRef<CObject> obj, CScope& scope);
    static string GetXMLObjectDescription(CConstRef<CObject> obj, CScope& scope);

    static string GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope);
    static string GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, const string& product);  
    static string GetTextObjectDescription(const CSeqdesc& sd, CScope& scope);
    static string GetTextObjectDescription(const CSeqdesc& sd);
    static string GetTextObjectDescription(CBioseq_Handle bsh);

    static bool AlreadyInList(const TReportObjectList& list, const CReportObject& new_obj);

    /// Do CReportObjects represent the same object?
    bool Equal(const CReportObject& other) const;

protected:
    string                m_Text;
    string                m_FeatureTable;
    string                m_XML;
    CConstRef<CObject>    m_Object;
    string                m_Filename;

};


END_SCOPE(DiscRepNmSpc)

END_NCBI_SCOPE

#endif 
