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
* Author:  Christiam Camacho
*
*/

/// @file blast_input_unit_test_aux.hpp
/// Declares the CAutoNcbiConfigFile class 

#ifndef ALGO_BLAST_BLASTINPUT_UNITTEST__BLAST_INPUT_UNIT_TEST_AUX__HPP
#define ALGO_BLAST_BLASTINPUT_UNITTEST__BLAST_INPUT_UNIT_TEST_AUX__HPP

#include <corelib/metareg.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

USING_NCBI_SCOPE;

/// Auxiliary class to write temporary NCBI configuration files in the local
/// directory for the purpose of testing the CBlastScopeSource configuration
/// class via this file (and override any other installed NCBI configuration
/// files)
class CAutoNcbiConfigFile {
public:
    static const char* kSection;
    static const char* kDataLoaders;
    static const char* kProtBlastDbDataLoader;
    static const char* kNuclBlastDbDataLoader;

    typedef blast::SDataLoaderConfig::EConfigOpts EConfigOpts;

    CAutoNcbiConfigFile(EConfigOpts opts = blast::SDataLoaderConfig::eDefault) {
        m_Sentry = CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);

        string value;
        if (opts & blast::SDataLoaderConfig::eUseBlastDbDataLoader) {
            value += "blastdb ";
        }
        if (opts & blast::SDataLoaderConfig::eUseGenbankDataLoader) {
            value += "genbank";
        }
        if (opts & blast::SDataLoaderConfig::eUseNoDataLoaders) {
            value += "none";
        }
        m_Sentry.registry->Set(kSection, kDataLoaders, value);
    }

    void SetProteinBlastDbDataLoader(const string& prot_db_name) {
        m_Sentry.registry->Set(kSection, kProtBlastDbDataLoader, prot_db_name);
    }

    void SetNucleotideBlastDbDataLoader(const string& nucl_db_name) {
        m_Sentry.registry->Set(kSection, kNuclBlastDbDataLoader, nucl_db_name);
    }

    void RemoveBLASTDBEnvVar() {
        m_BlastDb = m_Sentry.registry->Get(kSection, "BLASTDB");
        m_Sentry.registry->Set(kSection, "BLASTDB", "/dev/null");
    }

    ~CAutoNcbiConfigFile() {
        m_Sentry.registry->Set(kSection, kDataLoaders, kEmptyStr);
        m_Sentry.registry->Set(kSection, kProtBlastDbDataLoader, kEmptyStr);
        m_Sentry.registry->Set(kSection, kNuclBlastDbDataLoader, kEmptyStr);
        if ( !m_BlastDb.empty() ) {
            m_Sentry.registry->Set(kSection, "BLASTDB", m_BlastDb);
        }
    }
private:
    CMetaRegistry::SEntry m_Sentry;
    string m_BlastDb;
};

/// RAII class for the CBlastScopeSource. It revokes the BLAST database data
/// loader upon destruction to reset the environment for other unit tests
class CBlastScopeSourceWrapper {
public:
    CBlastScopeSourceWrapper() {
        m_ScopeSrc.Reset(new blast::CBlastScopeSource);
    }
    CBlastScopeSourceWrapper(blast::SDataLoaderConfig dlconfig) {
        m_ScopeSrc.Reset(new blast::CBlastScopeSource(dlconfig));
    }
    CRef<CScope> NewScope() { return m_ScopeSrc->NewScope(); }
    string GetBlastDbLoaderName() const { 
        return m_ScopeSrc->GetBlastDbLoaderName(); 
    }
    ~CBlastScopeSourceWrapper() {
        m_ScopeSrc->RevokeBlastDbDataLoader();
        m_ScopeSrc.Reset();
    }

private:
    CRef<blast::CBlastScopeSource> m_ScopeSrc;
};
/* @} */

#endif  /* ALGO_BLAST_BLASTINPUT_UNITTEST__BLAST_INPUT_UNIT_TEST_AUX__HPP */
