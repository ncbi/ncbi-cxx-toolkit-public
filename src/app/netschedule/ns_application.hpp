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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: Network scheduler daemon
 *
 */

#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_types.h>

USING_NCBI_SCOPE;


// NetSchedule daemon application
class CNetScheduleDApp : public CNcbiApplication
{
public:
    CNetScheduleDApp();
    void Init(void);
    int Run(void);
    bool GetEffectiveReinit(void) const { return m_EffectiveReinit; }
    map<string, string>
    GetOrigLogSection(void) const { return m_OrigLogSection; }
    map<string, string>
    GetOrigDiagSection(void) const { return m_OrigDiagSection; }
    map<string, string>
    GetOrigBDBSection(void) const { return m_OrigBDBSection; }

private:
    STimeout                m_ServerAcceptTimeout;
    bool                    m_EffectiveReinit;

    void x_SaveSection(const CNcbiRegistry &  reg,
                       const string &  section,
                       map<string, string> &  storage);
    bool x_WritePid(void) const;

    map<string, string>     m_OrigLogSection;
    map<string, string>     m_OrigDiagSection;
    map<string, string>     m_OrigBDBSection;
};

