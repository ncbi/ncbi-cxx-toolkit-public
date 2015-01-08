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


#ifndef _REPORT_ITEM_H_
#define _REPORT_ITEM_H_

#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objmgr/scope.hpp>
#include <misc/discrepancy_report/report_object.hpp>

#include <misc/xmlwrapp/xmlwrapp.hpp>


BEGIN_NCBI_SCOPE

USING_SCOPE(objects);
BEGIN_SCOPE(DiscRepNmSpc)


class NCBI_DISCREPANCY_REPORT_EXPORT CReportOutputConfig : public CObject
{
public:
    typedef enum {
        eFormatText = 0,
        eFormatXML
    } EFormat;
  
    CReportOutputConfig () : m_UseFeatureTableFormat(false),
                             m_AddTag(false),
                             m_LabelSub(false),
                             m_SuppressAllObjects(false),
                             m_Format(eFormatText) {}

    ~CReportOutputConfig() {}

    bool ShouldUseFeatureTableFormat() const { return m_UseFeatureTableFormat; }
    void SetUseFeatureTableFormat(bool val = true) { m_UseFeatureTableFormat = val; }
    bool ShouldAddTag() const { return m_AddTag; }
    void SetAddTag(bool val = true) { m_AddTag = val; }
    bool ShouldLabelSub() const { return m_LabelSub; }
    void SetLabelSub(bool val = true) { m_LabelSub = val; }
    bool ShouldListObjects(const string& test_name);
    void SetListObjects(string test_name, bool val = true) { m_SuppressObjects[NStr::ToUpper(test_name)] = !val; }
    void ClearListObjects() { m_SuppressObjects.clear(); }
    void SetSuppressObjectList(bool val = true) { m_SuppressAllObjects = val; }
    EFormat GetFormat() const { return m_Format; }
    void SetFormat(EFormat format) { m_Format = format; }
    bool ShouldExpand(string test_name) { return m_TestList[NStr::ToUpper(test_name)]; }
    void SetExpand(string test_name, bool val = true) { m_TestList[NStr::ToUpper(test_name)] = val; }
    void ClearExpanded() { m_TestList.clear(); }

protected:
    bool m_UseFeatureTableFormat;  // indicates what format to use when 
                                   // formatting related objects
    bool m_AddTag;                 // indicates whether tags are requested in output
    bool m_LabelSub;               // indicates whether text should include label for "top level" vs. sub
    bool m_ListObjects;            // indicates whether to list objects in report
    EFormat m_Format;              // format to use for output (text or XML)

    typedef map <string, bool> TExpandedTestList;
    TExpandedTestList m_TestList;

    bool m_SuppressAllObjects;
    typedef map <string, bool> TSuppressObjectsList;
    TSuppressObjectsList m_SuppressObjects;
};


class CReportItem;

typedef vector<CRef< CReportItem> > TReportItemList;

class NCBI_DISCREPANCY_REPORT_EXPORT CReportItem : public CObject
{
public:
    CReportItem() : m_Text(""), 
                          m_IsSelected(false), m_IsChecked(false), m_IsExpanded(false), 
                          m_Objects(), m_Subitems(), m_NeedsTag(false) {}
    ~CReportItem() {}

    const string& GetText() const { return m_Text; }
    void SetText(const string& text) { m_Text = text; }
    void SetTextWithHas(size_t num, const string& noun, const string& phrase);
    void SetTextWithHas(const string& noun, const string& phrase);
    void SetTextWithIs(size_t num, const string& noun, const string& phrase);
    void SetTextWithIs(const string& noun, const string& phrase);
    void SetTextWithSimpleVerb(size_t num, const string& noun, const string& verb, const string& phrase);
    void SetTextWithSimpleVerb(const string& noun, const string& verb, const string& phrase);

    bool IsSelected() const { return m_IsSelected; }
    void SetSelected(bool selected = true);

    bool IsChecked() const { return m_IsChecked; }
    void SetChecked(bool checked = true) { m_IsChecked = checked;} ;

    bool IsExpanded() const { return m_IsExpanded; }
    void SetExpanded(bool expanded = true, bool recurse = false); 

    bool CanGetSubitems() const { return (!m_Subitems.empty()); };

    const TReportItemList& GetSubitems() const { return m_Subitems; }
    TReportItemList& SetSubitems() { return m_Subitems; }

    const TReportObjectList& GetObjects() const { return m_Objects; }
    TReportObjectList& SetObjects() { return m_Objects; }
    void AddObjectsFromSubcategories();
    
    void SetSettingName(const string& str) { m_SettingName = str; }
    const string& GetSettingName() const { return m_SettingName; }

    void SetNeedsTag(bool val) { m_NeedsTag = val; }
    bool NeedsTag() const { return m_NeedsTag; }

    string Format(CReportOutputConfig& config, CScope& scope, CReportObject& obj);
    string Format(CReportOutputConfig& config, const CReportObject& obj) const;
    string Format(CReportOutputConfig& config, CScope& scope, bool is_sub = false);
    string Format(CReportOutputConfig& config, bool is_sub = false) const;

    void FormatXML(CReportOutputConfig& config, xml::node& xml_root) const;

    // the DropReferences methods save text representations of the objects
    // and reset references to underlying objects.
    // This makes it possible to allow the Seq-entry (and all the objects 
    // used to create the text representations) to go out of scope and not be
    // stored in memory
    void DropReferences();
    void DropReferences(CScope& scope);

protected:
    string                        m_Text;
    bool                          m_IsSelected;
    bool                          m_IsExpanded;
    bool                          m_IsChecked;
    TReportItemList               m_Subitems;
    TReportObjectList             m_Objects;
    string                        m_SettingName;
    bool                          m_NeedsTag;
};
END_SCOPE(DiscRepNmSpc)

END_NCBI_SCOPE

#endif 
