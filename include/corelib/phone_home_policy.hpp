#ifndef CORELIB___PHONE_HOME_POLICY__HPP
#define CORELIB___PHONE_HOME_POLICY__HPP

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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 *
 */

/// @file phone_home_policy.hpp
/// Define IPhoneHomePolicy, interface class for Phone Home Policy.
/// https://confluence.ncbi.nlm.nih.gov/pages/viewpage.action?pageId=184682676
/// 
/// For the sample implementation see:
///     src/sample/app/connect/ncbi_usage_report_phonehome_sample.cpp


#include <corelib/version_api.hpp>
#include <common/ncbi_build_info.h>


BEGIN_NCBI_SCOPE

/** @addtogroup AppFramework
 *
 * @{
 */

class CNcbiApplicationAPI;


/// Interface class for Phone Home Policy.
/// 
/// Phone Home Policy is disabled by default. It should be explicitly enabled
/// in the Apply() method, based on passed to te application command line arguments
/// or previously saved configuration.
/// 
class IPhoneHomePolicy
{
public:
    /// Constructor
    IPhoneHomePolicy() : m_IsEnabled(false) {};
    /// Destructor
    virtual ~IPhoneHomePolicy() { Finish(); };

    /// Apply policy for an application.
    ///  
    /// For example, it can get and check program arguments, and call any other methods
    /// depending on it. Automatically called from CNcbiApplication::SetPhoneHomePolicy().
    /// @param app
    ///   Pointer to the CNcbiApplicationAPI instance.
    /// @sa CNcbiApplication::SetPhoneHomePolicy, CNcbiApplication::GetPhoneHomePolicy
    ///
    virtual void Apply(CNcbiApplicationAPI* app) = 0;

    /// Print a message about collecting data, disablig telemetry and privacy policies.
    virtual void Print() = 0;

    /// Save policy configuration.
    virtual void Save() = 0;

    /// Restore policy configuration.
    virtual void Restore() = 0;

    /// Initialize policy/reporting API.
    virtual void Init() {};

    /// Deinitialize policy/reporting API.
    /// Redefine if you want to gracefully terminate reporting API.
    /// Automatically called from the destructor.
    virtual void Finish() {};

    /// Return current policy status (enabbed/disabled).
    virtual bool IsEnabled() const { return m_IsEnabled; };

    /// Set policy status (enabled/disabled).
    virtual void SetEnabled(bool enabled = true) { m_IsEnabled = enabled; };

protected:
    bool m_IsEnabled;  ///< Enable/disable telemetry status
};


/* @} */


END_NCBI_SCOPE

#endif // CORELIB___PHONE_HOME_POLICY__HPP
