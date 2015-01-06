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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   NetStorage services and their properties
 *
 */


#include <ncbi_pch.hpp>

#include "nst_service_parameters.hpp"
#include "nst_config.hpp"


USING_NCBI_SCOPE;


CTimeSpan::TSmartStringFlags   kTimeSpanFlags = CTimeSpan::fSS_Nanosecond |
                                                CTimeSpan::fSS_SkipZero |
                                                CTimeSpan::fSS_Short;



CNSTServiceProperties::CNSTServiceProperties()
{
    // m_TTL default value is infinity, i.e. NULL in the DB
    m_TTL.m_IsNull = true;

    // m_ProlongOnRead and m_ProlongOnWrite default values are 0
    // so a default constructor is fine.
}


CJsonNode
CNSTServiceProperties::Serialize(void) const
{
    CJsonNode       service(CJsonNode::NewObjectNode());

    service.SetString("TTL", GetTTLAsString());
    service.SetString("ProlongOnRead",
                      m_ProlongOnRead.AsSmartString(kTimeSpanFlags));
    service.SetString("ProlongOnWrite",
                      m_ProlongOnWrite.AsSmartString(kTimeSpanFlags));

    return service;
}


string
CNSTServiceProperties::GetTTLAsString(void) const
{
    if (m_TTL.m_IsNull)
        return "infinity";
    return m_TTL.m_Value.AsSmartString(kTimeSpanFlags);
}


CNSTServiceRegistry::CNSTServiceRegistry()
{}


size_t
CNSTServiceRegistry::Size(void) const
{
    CMutexGuard     guard(m_Lock);
    return m_Services.size();
}


CJsonNode
CNSTServiceRegistry::ReadConfiguration(const IRegistry &  reg)
{
    // Read the new configuration first
    list<string>                    new_services;
    NStr::Split(reg.GetString("metadata_conf", "services", ""),
                " \t\r\n\v\f,", new_services);

    CNSTServiceProperties           new_default_service_props =
            x_ReadServiceProperties(reg, "metadata_conf",
                                    CNSTServiceProperties());

    map< string,
         CNSTServiceProperties >    new_service_conf;

    for (list<string>::const_iterator   k = new_services.begin();
         k != new_services.end(); ++k) {
        string      section = "service_" + *k;
        if (reg.HasEntry(section))
            new_service_conf[ *k ] = x_ReadServiceProperties(
                                                reg, section,
                                                new_default_service_props);
        else
            new_service_conf[ *k ] = new_default_service_props;
    }

    // Here: we have new service defaults and new services. Calculate
    // the difference and save the new values.
    CMutexGuard         guard(m_Lock);
    vector<string>      added;
    vector<string>      deleted;
    vector<string>      modified;

    for (map< string,
              CNSTServiceProperties >::const_iterator  k = m_Services.begin();
            k != m_Services.end(); ++k) {
        if (new_service_conf.find(k->first) == new_service_conf.end()) {
            deleted.push_back(k->first);
        } else {
            if (m_Services[k->first] != new_service_conf[k->first])
                modified.push_back(k->first);
        }
    }

    for (map< string,
              CNSTServiceProperties >::const_iterator
                k = new_service_conf.begin(); k != new_service_conf.end(); ++k)
        if (m_Services.find(k->first) == m_Services.end())
            added.push_back(k->first);

    CJsonNode           diff = CJsonNode::NewObjectNode();

    if (m_DefaultProperties.GetTTL() !=
            new_default_service_props.GetTTL()) {
        CJsonNode   ttl_diff = CJsonNode::NewObjectNode();
        ttl_diff.SetByKey("Old",
                          CJsonNode::NewStringNode(
                                m_DefaultProperties.GetTTLAsString()));
        ttl_diff.SetByKey("New",
                          CJsonNode::NewStringNode(
                                new_default_service_props.GetTTLAsString()));
        diff.SetByKey("TTL", ttl_diff);
    }

    if (m_DefaultProperties.GetProlongOnRead() !=
            new_default_service_props.GetProlongOnRead()) {
        CJsonNode   prolong_diff = CJsonNode::NewObjectNode();
        prolong_diff.SetByKey("Old",
                              CJsonNode::NewStringNode(
                                  m_DefaultProperties.
                                            GetProlongOnRead().
                                                AsSmartString(kTimeSpanFlags)));
        prolong_diff.SetByKey("New",
                              CJsonNode::NewStringNode(
                                  new_default_service_props.
                                            GetProlongOnRead().
                                                AsSmartString(kTimeSpanFlags)));
        diff.SetByKey("ProlongOnRead", prolong_diff);
    }

    if (m_DefaultProperties.GetProlongOnWrite() !=
            new_default_service_props.GetProlongOnWrite()) {
        CJsonNode   prolong_diff = CJsonNode::NewObjectNode();
        prolong_diff.SetByKey("Old",
                              CJsonNode::NewStringNode(
                                  m_DefaultProperties.
                                            GetProlongOnWrite().
                                                AsSmartString(kTimeSpanFlags)));
        prolong_diff.SetByKey("New",
                              CJsonNode::NewStringNode(
                                  new_default_service_props.
                                            GetProlongOnWrite().
                                                AsSmartString(kTimeSpanFlags)));
        diff.SetByKey("ProlongOnWrite", prolong_diff);
    }

    if (!added.empty() || !deleted.empty() || !modified.empty()) {
        CJsonNode   services_diff = CJsonNode::NewObjectNode();

        if (!added.empty()) {
            CJsonNode       add_node = CJsonNode::NewArrayNode();
            for (vector<string>::const_iterator  k = added.begin();
                    k != added.end(); ++k)
                add_node.AppendString(*k);
            services_diff.SetByKey("Added", add_node);
        }

        if (!deleted.empty()) {
            CJsonNode       del_node = CJsonNode::NewArrayNode();
            for (vector<string>::const_iterator  k = deleted.begin();
                    k != deleted.end(); ++k)
                del_node.AppendString(*k);
            services_diff.SetByKey("Deleted", del_node);
        }

        if (!modified.empty()) {
            CJsonNode   serv_diff = CJsonNode::NewObjectNode();
            for (vector<string>::const_iterator  k = modified.begin();
                    k != modified.end(); ++k) {
                CJsonNode       mod_diff = CJsonNode::NewObjectNode();

                if (m_Services[*k].GetTTL() != new_service_conf[*k].GetTTL()) {
                    CJsonNode       val_diff = CJsonNode::NewObjectNode();
                    val_diff.SetByKey("Old",
                                      CJsonNode::NewStringNode(
                                      m_Services[*k].GetTTLAsString()));
                    val_diff.SetByKey("New",
                                      CJsonNode::NewStringNode(
                                      new_service_conf[*k].GetTTLAsString()));
                    mod_diff.SetByKey("TTL", val_diff);
                }

                if (m_Services[*k].GetProlongOnRead() !=
                                    new_service_conf[*k].GetProlongOnRead()) {
                    CJsonNode       val_diff = CJsonNode::NewObjectNode();
                    val_diff.SetByKey("Old",
                                      CJsonNode::NewStringNode(
                        m_Services[*k].GetProlongOnRead().
                                                AsSmartString(kTimeSpanFlags)));
                    val_diff.SetByKey("New",
                                      CJsonNode::NewStringNode(
                        new_service_conf[*k].GetProlongOnRead().
                                                AsSmartString(kTimeSpanFlags)));
                    mod_diff.SetByKey("ProlongOnRead", val_diff);
                }

                if (m_Services[*k].GetProlongOnWrite() !=
                                    new_service_conf[*k].GetProlongOnWrite()) {
                    CJsonNode       val_diff = CJsonNode::NewObjectNode();
                    val_diff.SetByKey("Old",
                                      CJsonNode::NewStringNode(
                        m_Services[*k].GetProlongOnWrite().
                                                AsSmartString(kTimeSpanFlags)));
                    val_diff.SetByKey("New",
                                      CJsonNode::NewStringNode(
                        new_service_conf[*k].GetProlongOnWrite().
                                                AsSmartString(kTimeSpanFlags)));
                    mod_diff.SetByKey("ProlongOnWrite", val_diff);
                }

                serv_diff.SetByKey(*k, mod_diff);
            }
            services_diff.SetByKey("Modified", serv_diff);
        }
        diff.SetByKey("Services", services_diff);
    }

    m_Services = new_service_conf;
    m_DefaultProperties = new_default_service_props;
    return diff;
}


CJsonNode
CNSTServiceRegistry::Serialize(void) const
{
    CJsonNode       services(CJsonNode::NewArrayNode());
    CMutexGuard     guard(m_Lock);

    CJsonNode       default_service_properties = CJsonNode::NewObjectNode();
    default_service_properties.SetByKey("DefaultServiceProperties",
                                        m_DefaultProperties.Serialize());
    services.Append(default_service_properties);

    for (map<string, CNSTServiceProperties>::const_iterator
            k = m_Services.begin(); k != m_Services.end(); ++k) {
        CJsonNode   single_service(k->second.Serialize());

        single_service.SetString("Name", k->first);
        services.Append(single_service);
    }

    return services;
}


bool
CNSTServiceRegistry::IsKnown(const string &  service) const
{
    CMutexGuard     guard(m_Lock);
    return m_Services.find(service) != m_Services.end();
}


// true if the service is found
bool
CNSTServiceRegistry::GetTTL(const string &            service,
                            TNSTDBValue<CTimeSpan> &  ttl) const
{
    CMutexGuard                                   guard(m_Lock);
    map< string,
         CNSTServiceProperties >::const_iterator  s = m_Services.find(service);

    if (s == m_Services.end())
        return false;

    ttl = s->second.GetTTL();
    return true;
}


// true if the service is found
bool
CNSTServiceRegistry::GetProlongOnRead(const string &  service,
                                      CTimeSpan &     prolong_on_read) const
{
    CMutexGuard                                   guard(m_Lock);
    map< string,
         CNSTServiceProperties >::const_iterator  s = m_Services.find(service);

    if (s == m_Services.end())
        return false;

    prolong_on_read = s->second.GetProlongOnRead();
    return true;
}


// true if the service is found
bool
CNSTServiceRegistry::GetProlongOnWrite(const string &  service,
                                       CTimeSpan &     prolong_on_write) const
{
    CMutexGuard                                   guard(m_Lock);
    map< string,
         CNSTServiceProperties >::const_iterator  s = m_Services.find(service);

    if (s == m_Services.end())
        return false;

    prolong_on_write = s->second.GetProlongOnWrite();
    return true;
}


// true if the service is found
bool
CNSTServiceRegistry::GetServiceProperties(const string &  service,
                                          CNSTServiceProperties &  props) const
{
    CMutexGuard                                   guard(m_Lock);
    map< string,
         CNSTServiceProperties >::const_iterator  s = m_Services.find(service);

    if (s == m_Services.end())
        return false;

    props = s->second;
    return true;
}


CNSTServiceProperties
CNSTServiceRegistry::GetDefaultProperties(void) const
{
    CMutexGuard     guard(m_Lock);
    return m_DefaultProperties;
}


// Reads a service properties applying default values
CNSTServiceProperties
CNSTServiceRegistry::x_ReadServiceProperties(
                                const IRegistry &  reg,
                                const string &  section,
                                const CNSTServiceProperties &  defaults
                                            )
{
    CNSTServiceProperties       result = defaults;

    if (reg.HasEntry(section, "ttl")) {
        TNSTDBValue<CTimeSpan>      new_val;
        string                      value = reg.GetString(section, "ttl", "");
        try {
            new_val = ReadTimeSpan(value, true); // allow infinity
        } catch (...) {
            new_val.m_IsNull = true;
        }
        result.SetTTL(new_val);
    }

    if (reg.HasEntry(section, "prolong_on_read"))
        result.SetProlongOnRead(x_ReadProlongProperty(reg, section,
                                                      "prolong_on_read"));
    if (reg.HasEntry(section, "prolong_on_write"))
        result.SetProlongOnWrite(x_ReadProlongProperty(reg, section,
                                                       "prolong_on_write"));
    return result;
}


CTimeSpan
CNSTServiceRegistry::x_ReadProlongProperty(const IRegistry &  reg,
                                           const string &  section,
                                           const string &  entry)
{
    CTimeSpan       new_val;    // default constructor sets it to 0 which
                                // is the default
    string          value = reg.GetString(section, entry, "");
    if (!value.empty()) {
        try {
            TNSTDBValue<CTimeSpan>  val = ReadTimeSpan(value,
                                                       false); // no infinity
            new_val = val.m_Value;
        } catch (...) {
            ;
        }
    }
    return new_val;
}



