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



CNSTServiceProperties::CNSTServiceProperties()
{
    // m_TTL default value is infinity, i.e. NULL in the DB
    m_TTL.m_IsNull = true;

    // m_ProlongOnRead and m_ProlongOnWrite and m_ProlongOnRelocate default
    // values are 0 so a default constructor is fine.

    m_ProlongOnReadInTTLs = k_ProlongInTTLsNotConfigured;
    m_ProlongOnWriteInTTLs = k_ProlongInTTLsNotConfigured;
    m_ProlongOnRelocateInTTLs = k_ProlongInTTLsNotConfigured;
}


CJsonNode
CNSTServiceProperties::Serialize(void) const
{
    CJsonNode       service(CJsonNode::NewObjectNode());

    service.SetString("TTL", GetTTLAsString());

    service.SetString("ProlongOnRead",
                      x_GetProlongAsString(m_ProlongOnRead,
                                           m_ProlongOnReadInTTLs));
    service.SetString("ProlongOnWrite",
                      x_GetProlongAsString(m_ProlongOnWrite,
                                           m_ProlongOnWriteInTTLs));
    service.SetString("ProlongOnRelocate",
                      x_GetProlongAsString(m_ProlongOnRelocate,
                                           m_ProlongOnRelocateInTTLs));

    return service;
}


string
CNSTServiceProperties::GetTTLAsString(void) const
{
    if (m_TTL.m_IsNull)
        return "infinity";
    return m_TTL.m_Value.AsSmartString(kTimeSpanFlags);
}


string
CNSTServiceProperties::x_GetProlongAsString(const CTimeSpan &  as_time_span,
                                            double             in_ttls) const
{
    if (in_ttls == k_ProlongInTTLsNotConfigured)
        return as_time_span.AsSmartString(kTimeSpanFlags);
    return NStr::NumericToString(in_ttls) + " ttl";
}

CNSTServiceRegistry::CNSTServiceRegistry()
{
    // LBSMD test service propertiesare ttl = 1h, prolongs = 0s
    TNSTDBValue<CTimeSpan>      lbsmd_ttl;

    lbsmd_ttl.m_IsNull = false;
    lbsmd_ttl.m_Value = CTimeSpan(3600L);
    m_LBSMDTestServiceProperties.SetTTL(lbsmd_ttl);
    m_LBSMDTestServiceProperties.SetProlongOnRead(CTimeSpan(0L));
    m_LBSMDTestServiceProperties.SetProlongOnWrite(CTimeSpan(0L));
}


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
    list<string>            new_services = x_GetMetadataServices(reg);
    CNSTServiceProperties   new_default_service_props =
                              x_ReadServiceProperties(reg, "metadata_conf",
                                                      CNSTServiceProperties());
    TServiceProperties      new_service_conf;

    for (list<string>::const_iterator  k = new_services.begin();
            k != new_services.end(); ++k) {
        new_service_conf[ *k ] = x_ReadServiceProperties(reg, "service_" + *k,
                                                new_default_service_props);
    }

    // Here: we have new service defaults and new services. Calculate
    // the difference and save the new values.
    CMutexGuard         guard(m_Lock);
    vector<string>      added;
    vector<string>      deleted;
    vector<string>      modified;

    for (TServiceProperties::const_iterator  k = m_Services.begin();
            k != m_Services.end(); ++k) {
        if (new_service_conf.find(k->first) == new_service_conf.end()) {
            deleted.push_back(k->first);
        } else {
            if (m_Services[k->first] != new_service_conf[k->first])
                modified.push_back(k->first);
        }
    }

    for (TServiceProperties::const_iterator  k = new_service_conf.begin();
            k != new_service_conf.end(); ++k)
        if (m_Services.find(k->first) == m_Services.end())
            added.push_back(k->first);

    CJsonNode           diff = CJsonNode::NewObjectNode();

    if (m_DefaultProperties.GetTTLAsString() !=
            new_default_service_props.GetTTLAsString()) {
        CJsonNode   ttl_diff = CJsonNode::NewObjectNode();
        ttl_diff.SetByKey("Old",
                          CJsonNode::NewStringNode(
                                m_DefaultProperties.GetTTLAsString()));
        ttl_diff.SetByKey("New",
                          CJsonNode::NewStringNode(
                                new_default_service_props.GetTTLAsString()));
        diff.SetByKey("TTL", ttl_diff);
    }

    if (m_DefaultProperties.GetProlongOnReadAsString() !=
            new_default_service_props.GetProlongOnReadAsString()) {
        CJsonNode   prolong_diff = CJsonNode::NewObjectNode();
        prolong_diff.SetByKey(
            "Old", CJsonNode::NewStringNode(
                    m_DefaultProperties.GetProlongOnReadAsString()));
        prolong_diff.SetByKey(
            "New", CJsonNode::NewStringNode(
                    new_default_service_props.GetProlongOnReadAsString()));
        diff.SetByKey("ProlongOnRead", prolong_diff);
    }

    if (m_DefaultProperties.GetProlongOnWriteAsString() !=
            new_default_service_props.GetProlongOnWriteAsString()) {
        CJsonNode   prolong_diff = CJsonNode::NewObjectNode();
        prolong_diff.SetByKey(
            "Old", CJsonNode::NewStringNode(
                    m_DefaultProperties.GetProlongOnWriteAsString()));
        prolong_diff.SetByKey(
            "New", CJsonNode::NewStringNode(
                    new_default_service_props.GetProlongOnWriteAsString()));
        diff.SetByKey("ProlongOnWrite", prolong_diff);
    }

    if (m_DefaultProperties.GetProlongOnRelocateAsString() !=
            new_default_service_props.GetProlongOnRelocateAsString()) {
        CJsonNode   prolong_diff = CJsonNode::NewObjectNode();
        prolong_diff.SetByKey(
            "Old", CJsonNode::NewStringNode(
                    m_DefaultProperties.GetProlongOnRelocateAsString()));
        prolong_diff.SetByKey(
            "New", CJsonNode::NewStringNode(
                    new_default_service_props.GetProlongOnRelocateAsString()));
        diff.SetByKey("ProlongOnRelocate", prolong_diff);
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

                if (m_Services[*k].GetTTLAsString() !=
                        new_service_conf[*k].GetTTLAsString()) {
                    CJsonNode       val_diff = CJsonNode::NewObjectNode();
                    val_diff.SetByKey("Old",
                                      CJsonNode::NewStringNode(
                                      m_Services[*k].GetTTLAsString()));
                    val_diff.SetByKey("New",
                                      CJsonNode::NewStringNode(
                                      new_service_conf[*k].GetTTLAsString()));
                    mod_diff.SetByKey("TTL", val_diff);
                }

                if (m_Services[*k].GetProlongOnReadAsString() !=
                            new_service_conf[*k].GetProlongOnReadAsString()) {
                    CJsonNode       val_diff = CJsonNode::NewObjectNode();
                    val_diff.SetByKey("Old",
                                      CJsonNode::NewStringNode(
                        m_Services[*k].GetProlongOnReadAsString()));
                    val_diff.SetByKey("New",
                                      CJsonNode::NewStringNode(
                        new_service_conf[*k].GetProlongOnReadAsString()));
                    mod_diff.SetByKey("ProlongOnRead", val_diff);
                }

                if (m_Services[*k].GetProlongOnWriteAsString() !=
                            new_service_conf[*k].GetProlongOnWriteAsString()) {
                    CJsonNode       val_diff = CJsonNode::NewObjectNode();
                    val_diff.SetByKey("Old",
                                      CJsonNode::NewStringNode(
                        m_Services[*k].GetProlongOnWriteAsString()));
                    val_diff.SetByKey("New",
                                      CJsonNode::NewStringNode(
                        new_service_conf[*k].GetProlongOnWriteAsString()));
                    mod_diff.SetByKey("ProlongOnWrite", val_diff);
                }

                if (m_Services[*k].GetProlongOnRelocateAsString() !=
                        new_service_conf[*k].GetProlongOnRelocateAsString()) {
                    CJsonNode       val_diff = CJsonNode::NewObjectNode();
                    val_diff.SetByKey("Old",
                                      CJsonNode::NewStringNode(
                        m_Services[*k].GetProlongOnRelocateAsString()));
                    val_diff.SetByKey("New",
                                      CJsonNode::NewStringNode(
                        new_service_conf[*k].GetProlongOnRelocateAsString()));
                    mod_diff.SetByKey("ProlongOnRelocate", val_diff);
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

    for (TServiceProperties::const_iterator  k = m_Services.begin();
            k != m_Services.end(); ++k) {
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
    bool            found = m_Services.find(service) != m_Services.end();

    if (found)
        return true;

    // Hardcoded service for LBSMD health check
    return NStr::EqualNocase(service, k_LBSMDNSTTestService);
}


// true if the service is found
bool
CNSTServiceRegistry::GetTTL(const string &            service,
                            TNSTDBValue<CTimeSpan> &  ttl) const
{
    CMutexGuard                         guard(m_Lock);
    TServiceProperties::const_iterator  s = m_Services.find(service);

    if (s == m_Services.end()) {
        // It might be the LBSMD test service
        if (NStr::EqualNocase(service, k_LBSMDNSTTestService)) {
            ttl = m_LBSMDTestServiceProperties.GetTTL();
            return true;
        }
        return false;
    }

    ttl = s->second.GetTTL();
    return true;
}


// true if the service is found
bool
CNSTServiceRegistry::GetProlongOnRead(const string &  service,
                                      CTimeSpan &     prolong_on_read) const
{
    CMutexGuard                         guard(m_Lock);
    TServiceProperties::const_iterator  s = m_Services.find(service);

    if (s == m_Services.end()) {
        // It might be the LBSMD test service
        if (NStr::EqualNocase(service, k_LBSMDNSTTestService)) {
            prolong_on_read = m_LBSMDTestServiceProperties.GetProlongOnRead();
            return true;
        }
        return false;
    }

    prolong_on_read = s->second.GetProlongOnRead();
    return true;
}


// true if the service is found
bool
CNSTServiceRegistry::GetProlongOnWrite(const string &  service,
                                       CTimeSpan &     prolong_on_write) const
{
    CMutexGuard                         guard(m_Lock);
    TServiceProperties::const_iterator  s = m_Services.find(service);

    if (s == m_Services.end()) {
        // It might be the LBSMD test service
        if (NStr::EqualNocase(service, k_LBSMDNSTTestService)) {
            prolong_on_write = m_LBSMDTestServiceProperties.GetProlongOnWrite();
            return true;
        }
        return false;
    }

    prolong_on_write = s->second.GetProlongOnWrite();
    return true;
}


// true if the service is found
bool
CNSTServiceRegistry::GetProlongOnRelocate(
                                const string &  service,
                                CTimeSpan &     prolong_on_relocate) const
{
    CMutexGuard                         guard(m_Lock);
    TServiceProperties::const_iterator  s = m_Services.find(service);

    if (s == m_Services.end()) {
        // It might be the LBSMD test service
        if (NStr::EqualNocase(service, k_LBSMDNSTTestService)) {
            prolong_on_relocate =
                        m_LBSMDTestServiceProperties.GetProlongOnRelocate();
            return true;
        }
        return false;
    }

    prolong_on_relocate = s->second.GetProlongOnRelocate();
    return true;
}


// true if the service is found
bool
CNSTServiceRegistry::GetServiceProperties(const string &  service,
                                          CNSTServiceProperties &  props) const
{
    CMutexGuard                         guard(m_Lock);
    TServiceProperties::const_iterator  s = m_Services.find(service);

    if (s == m_Services.end()) {
        // It might be the LBSMD test service
        if (NStr::EqualNocase(service, k_LBSMDNSTTestService)) {
            props = m_LBSMDTestServiceProperties;
            return true;
        }
        return false;
    }

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
    string                      value;

    if (reg.HasEntry(section, "ttl")) {
        TNSTDBValue<CTimeSpan>      new_val;

        value = reg.GetString(section, "ttl", "");
        try {
            new_val = ReadTimeSpan(value, true); // allow infinity
        } catch (...) {
            new_val.m_IsNull = true;
        }
        result.SetTTL(new_val);
    }

    if (reg.HasEntry(section, "prolong_on_read")) {
        value = reg.GetString(section, "prolong_on_read", "");
        NStr::TruncateSpacesInPlace(value);

        if (NStr::EndsWith(value, "ttl", NStr::eNocase))
            result.SetProlongOnRead(x_ReadProlongInTTLs(value));
        else
            result.SetProlongOnRead(x_ReadProlongProperty(value));
    }
    if (reg.HasEntry(section, "prolong_on_write")) {
        value = reg.GetString(section, "prolong_on_write", "");
        NStr::TruncateSpacesInPlace(value);

        if (NStr::EndsWith(value, "ttl", NStr::eNocase))
            result.SetProlongOnWrite(x_ReadProlongInTTLs(value));
        else
            result.SetProlongOnWrite(x_ReadProlongProperty(value));
    }
    if (reg.HasEntry(section, "prolong_on_relocate")) {
        value = reg.GetString(section, "prolong_on_relocate", "");
        NStr::TruncateSpacesInPlace(value);

        if (NStr::EndsWith(value, "ttl", NStr::eNocase))
            result.SetProlongOnRelocate(x_ReadProlongInTTLs(value));
        else
            result.SetProlongOnRelocate(x_ReadProlongProperty(value));
    }

    return result;
}


double
CNSTServiceRegistry::x_ReadProlongInTTLs(string &  value) const
{
    return NStr::StringToDouble(
                value.substr(0, value.size() - 3),  // Strip the 'ttl' suffix
                NStr::fAllowTrailingSpaces);        // There could be spaces
                                                    // between the value and
                                                    // the suffix
}


CTimeSpan
CNSTServiceRegistry::x_ReadProlongProperty(const string &  value)
{
    CTimeSpan       new_val;    // default constructor sets it to 0 which
                                // is the default
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


list<string>
CNSTServiceRegistry::x_GetMetadataServices(const IRegistry &  reg)
{
    list<string>    metadata_services;
    list<string>    sections;
    const string    prefix = "service_";

    reg.EnumerateSections(&sections);
    for (list<string>::const_iterator  k = sections.begin();
            k != sections.end(); ++k) {
        if (NStr::StartsWith(*k, prefix, NStr::eNocase)) {
            if (reg.HasEntry(*k, "metadata")) {
                try {
                    if (reg.GetBool(*k, "metadata", false)) {
                        string      service_name = string(k->c_str() +
                                                          prefix.size());
                        if (!service_name.empty())
                            metadata_services.push_back(service_name);
                    }
                } catch (...) {
                    ;
                }
            }
        }
    }

    return metadata_services;
}

