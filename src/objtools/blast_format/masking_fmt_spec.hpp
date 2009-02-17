/* $Id$
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

/// @file masking_fmt_spec.hpp
/// Defines the CMaskingFmtSpecHelper class

#ifndef OBJTOOLS_BLAST_FORMAT__MASKING_FMT_SPEC__HPP
#define OBJTOOLS_BLAST_FORMAT__MASKING_FMT_SPEC__HPP

BEGIN_NCBI_SCOPE

/// Auxiliary class to help parse the masking algorithm specification to assist
/// the CSeqFormatter class
class CMaskingFmtSpecHelper 
{
    vector<int> m_FiltAlgorithms;
    string m_FilteringAlgoSpec;

    static const char kDelim;
    static const string kDelimStr;

public:
    CMaskingFmtSpecHelper(const string& fmt_spec, CSeqDB& seqdb)
    {
        x_ParseFilteringAlgorithmSpec(fmt_spec);
        x_ConvertFilteringAlgorithmSpec(seqdb);
        x_ValidateFilteringAlgorithms(seqdb);
    }

    vector<int> GetFilteringAlgorithms() const {
        return m_FiltAlgorithms;
    }

    string GetFiltAlgorithmSpecifiers() const {
        return m_FilteringAlgoSpec;
    }

private:
    /// Extracts the relevant portion of its argument that represents the
    /// filtering algorithm specification (could be empty)
    /// @post m_FilteringAlgoSpec has been assigned its proper value
    void x_ParseFilteringAlgorithmSpec(const string& fmt_spec)
    {
        static const string kToken("%m");
        SIZE_TYPE i = NStr::Find(fmt_spec, kToken);
        m_FilteringAlgoSpec.assign(kEmptyStr);
        if (i == NPOS) {
            return;
        }

        m_FilteringAlgoSpec.reserve(fmt_spec.size());
        for (i += kToken.size(); i < fmt_spec.size(); i++) {
            if (isdigit(fmt_spec[i]) || fmt_spec[i] == kDelim) {
                m_FilteringAlgoSpec.append(1, fmt_spec[i]);
            } else {
                break;
            }
        }
    }

    /// Converts the filtering algorithm spec from its string representation to
    /// one that can be understood by CSeqDB
    /// @param seqdb BLAST database object [in]
    /// @post m_FiltAlgorithms has been assigned (it could be empty!)
    void x_ConvertFilteringAlgorithmSpec(CSeqDB& seqdb)
    {
        if (m_FilteringAlgoSpec.empty()) {
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION > 550))  &&  \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
            vector<int> available_filt_algo;
            seqdb.GetAvailableMaskAlgorithms(available_filt_algo);
            m_FiltAlgorithms.swap(available_filt_algo);
#endif
        } else {
            vector<string> tokens;
            NStr::Tokenize(m_FilteringAlgoSpec, kDelimStr, tokens);
            m_FiltAlgorithms.reserve(tokens.size());
            ITERATE(vector<string>, itr, tokens) {
                int x = NStr::StringToInt(*itr, NStr::fConvErr_NoThrow);
                m_FiltAlgorithms.push_back(x);
            }
        }
    }

    /// Validates that the parsed filtering algorithms IDs indeed exist in the
    /// BLAST database. For those that don't, they are removed from the list of
    /// algorithm IDs and an error message is displayed
    /// @post no invalid algorithm IDs are present in m_FiltAlgorithms
    void x_ValidateFilteringAlgorithms(CSeqDB& seqdb) 
    {
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION > 550))  &&  \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
        vector<int> invalid_algo_ids = 
            seqdb.ValidateMaskAlgorithms(m_FiltAlgorithms);
        if ( !invalid_algo_ids.empty() ) {
            CNcbiOstrstream os;
            os << "Invalid filtering algorithm IDs: ";
            os << invalid_algo_ids.front();
            for (SIZE_TYPE i = 1; i < invalid_algo_ids.size(); i++) {
                os << ", " << invalid_algo_ids[i];
            }
            string msg = CNcbiOstrstreamToString(os);
            ERR_POST(Warning << msg);
        }
        ITERATE(vector<int>, itr, invalid_algo_ids) {
            vector<int>::iterator pos = remove(m_FiltAlgorithms.begin(),
                                               m_FiltAlgorithms.end(), *itr);
            m_FiltAlgorithms.erase(pos, m_FiltAlgorithms.end());
        }
#endif
    }
};

END_NCBI_SCOPE

#endif  /* OBJTOOLS_BLAST_FORMAT__MASKING_FMT_SPEC__HPP */
