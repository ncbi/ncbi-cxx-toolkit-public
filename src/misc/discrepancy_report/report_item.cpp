/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Author:  Colleen Bollin
 *
 * File Description:
 *   class for storing results for Discrepancy Report
 *
 * $Log:$ 
 */

#include <ncbi_pch.hpp>

#include <misc/discrepancy_report/report_item.hpp>

BEGIN_NCBI_SCOPE;

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);


bool CReportOutputConfig::ShouldListObjects(const string& test_name)
{
    if (m_SuppressAllObjects) {
        return false;
    }
    string search = test_name;
    NStr::ToUpper(search);
    TSuppressObjectsList::iterator f = m_SuppressObjects.find(search);
    if (f == m_SuppressObjects.end()) {
        return true;
    } else {
        return !(f->second);
    }
}


string CReportItem::Format(CReportOutputConfig& config, CScope& scope, CReportObject& obj)
{
    string rval = "";

    switch (config.GetFormat()) {
        case CReportOutputConfig::eFormatText:
            if (config.ShouldUseFeatureTableFormat()) {
                obj.SetFeatureTable(scope);
                rval = obj.GetFeatureTable();
            } else {
                obj.SetText(scope);
                rval = obj.GetText();
            }
            break;
        case CReportOutputConfig::eFormatXML:
            obj.SetXML(scope);
            rval = obj.GetXML();
            break;
    }
    return rval;
}


string CReportItem::Format(CReportOutputConfig& config, const CReportObject& obj) const
{
    string rval = "";

    switch (config.GetFormat()) {
        case CReportOutputConfig::eFormatText:
            if (config.ShouldUseFeatureTableFormat()) {
                rval = obj.GetFeatureTable();
            } else {
                rval = obj.GetText();
            }
            break;
        case CReportOutputConfig::eFormatXML:
            rval = obj.GetXML();
            break;
    }
    return rval;
}


string CReportItem::Format(CReportOutputConfig& config, CScope& scope, bool is_sub)
{
    string rval = "";

    switch (config.GetFormat()) {
        case CReportOutputConfig::eFormatText:
            if (config.ShouldAddTag() && m_NeedsTag) {
                rval = "FATAL: ";
            }
            if (config.ShouldLabelSub()) {
                if (is_sub) {
                    rval += "DiscRep_SUB:";
                } else {
                    rval += "DiscRep_ALL:";
                }
            }

            rval += m_SettingName + "::" + m_Text + "\n";
            if (config.ShouldListObjects(m_SettingName)) {
                NON_CONST_ITERATE(TReportObjectList, it, m_Objects) {                
                    rval += Format(config, scope, **it) + "\n";
                }
            }
            break;
        case CReportOutputConfig::eFormatXML:
            break;
    }
    if (config.ShouldExpand(m_SettingName)) {
        NON_CONST_ITERATE(TReportItemList, it, m_Subitems) {
            rval += (*it)->Format(config, scope, true);
        }
    }
    return rval;
}


string CReportItem::Format(CReportOutputConfig& config, bool is_sub) const
{
    string rval = "";

    switch (config.GetFormat()) {
        case CReportOutputConfig::eFormatText:
            if (config.ShouldAddTag() && m_NeedsTag) {
                rval = "FATAL: ";
            }
            if (config.ShouldLabelSub()) {
                if (is_sub) {
                    rval += "DiscRep_SUB:";
                } else {
                    rval += "DiscRep_ALL:";
                }
            }
            rval += m_SettingName + "::" + m_Text + "\n";
            if (config.ShouldListObjects(m_SettingName)) {
                ITERATE(TReportObjectList, it, m_Objects) {                
                    rval += Format(config, **it) + "\n";
                }
            }
            break;
        case CReportOutputConfig::eFormatXML:
            break;
    }
    if (config.ShouldExpand(m_SettingName)) {
        ITERATE(TReportItemList, it, m_Subitems) {
            rval += (*it)->Format(config, true);
        }
    }
    return rval;
}


void CReportItem::FormatXML(CReportOutputConfig& config, xml::node& xml_root) const
{
    string severity = (config.ShouldAddTag() && m_NeedsTag) ? "FATAL" : "INFO";

    xml::node::iterator 
             xit = xml_root.insert(xml_root.end(), 
                                   xml::node("message", m_Text.c_str()));
    // set attributes:
    xit->get_attributes().insert("code", m_SettingName.c_str());
    xit->get_attributes().insert("severity", severity.c_str());

    if (config.ShouldExpand(m_SettingName)) {
        ITERATE(TReportItemList, it, m_Subitems) {    
            (*it)->FormatXML(config, *xit);
        }
    }
}


void CReportItem::SetTextWithHas(size_t num, const string& noun, const string& phrase)
{
    bool is_plural = (num != 1);
    m_Text = NStr::NumericToString(num) + " " + noun + 
             (is_plural ? "s have " : " has ") + phrase;             
}


void CReportItem::SetTextWithHas(const string& noun, const string& phrase)
{
    SetTextWithHas(m_Objects.size(), noun, phrase);
}


void CReportItem::SetTextWithIs(size_t num, const string& noun, const string& phrase)
{
    bool is_plural = (num != 1);
    m_Text = NStr::NumericToString(num) + " " + noun + 
             (is_plural ? "s are " : " is ") + phrase;             
}


void CReportItem::SetTextWithIs(const string& noun, const string& phrase)
{
    SetTextWithIs(m_Objects.size(), noun, phrase);
}


void CReportItem::SetTextWithSimpleVerb(size_t num, const string& noun, const string& verb, const string& phrase)
{
    bool is_plural = (num != 1);
    m_Text = NStr::NumericToString(num) + " " + noun + 
                  (is_plural ? "s " : " ") + verb +
                  (is_plural ? " " : "s ") + phrase;
}


void CReportItem::SetTextWithSimpleVerb(const string& noun, const string& verb, const string& phrase)
{
    SetTextWithSimpleVerb(m_Objects.size(), noun, verb, phrase);
}


void CReportItem::AddObjectsFromSubcategories()
{
    ITERATE(TReportItemList, it, m_Subitems) {
        const TReportObjectList& obj_list = (*it)->GetObjects();
        if (obj_list.size() > 0) {
            m_Objects.insert(m_Objects.end(), obj_list.begin(), obj_list.end());
        }
    }
}


void CReportItem::DropReferences()
{
    NON_CONST_ITERATE(TReportObjectList, obj, m_Objects) {
        (*obj)->DropReference();
    }
    NON_CONST_ITERATE(TReportItemList, item, m_Subitems) {
        (*item)->DropReferences();
    }
}


void CReportItem::DropReferences(CScope& scope)
{
    NON_CONST_ITERATE(TReportObjectList, obj, m_Objects) {
        (*obj)->DropReference(scope);
    }
    NON_CONST_ITERATE(TReportItemList, item, m_Subitems) {
        (*item)->DropReferences(scope);
    }
}


END_NCBI_SCOPE
