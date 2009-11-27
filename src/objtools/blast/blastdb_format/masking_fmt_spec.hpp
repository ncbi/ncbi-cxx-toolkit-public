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

#ifndef OBJTOOLS_BLASTDB_FORMAT__MASKING_FMT_SPEC__HPP
#define OBJTOOLS_BLASTDB_FORMAT__MASKING_FMT_SPEC__HPP

#include <objtools/blast/blastdb_format/invalid_data_exception.hpp>

BEGIN_NCBI_SCOPE

/// Auxiliary class to help parse the masking algorithm specification to assist
/// the CSeqFormatter class
class CMaskingFmtSpecHelper 
{
    int m_FiltAlgorithm;
    string m_FilteringAlgoSpec;

    static const char kDelim;
    static const string kDelimStr;

public:
    CMaskingFmtSpecHelper(const string& fmt_spec, CSeqDB& seqdb)
    {
        x_ParseFilteringAlgorithmSpec(fmt_spec);
        x_ValidateFilteringAlgorithm(seqdb, m_FiltAlgorithm);
    }

    int GetFilteringAlgorithm() const {
        return m_FiltAlgorithm;
    }

    string GetFiltAlgorithmSpecifier() const {
        return m_FilteringAlgoSpec;
    }

    /// Checks that the filtering algorithm is present in the BLAST database.
    /// @throws exception if any of the filtering algorithms is invalid
    static void ValidateFilteringAlgorithm(CSeqDB& seqdb,
                                           int filt_algo_id) {
        CMaskingFmtSpecHelper tmp(kEmptyStr, seqdb);
        tmp.x_ValidateFilteringAlgorithm(seqdb, filt_algo_id);
    }

private:
    /// Extracts the relevant portion of its argument that represents the
    /// filtering algorithm specification (could be empty)
    void x_ParseFilteringAlgorithmSpec(const string& fmt_spec)
    {
        static const string kToken("%m");
        SIZE_TYPE i = NStr::Find(fmt_spec, kToken);
        m_FilteringAlgoSpec.assign(kEmptyStr);
        m_FiltAlgorithm = -1;
        if (i == NPOS) {
            return;
        }

        if (fmt_spec == kToken) {
            NCBI_THROW(CInvalidDataException, eInvalidInput, 
                "Algorithm ID is missing for %m outfmt option.");
        }

        m_FilteringAlgoSpec.reserve(fmt_spec.size());
        for (i += kToken.size(); i < fmt_spec.size(); i++) {
            if (isdigit(fmt_spec[i])) {
                m_FilteringAlgoSpec.append(1, fmt_spec[i]);
            } else if (isspace(fmt_spec[i])) {   // we are done with %m
                break;
            } else {
                CNcbiOstrstream os;
                i = NStr::Find(fmt_spec, kToken);
                os << "Invalid filtering algorithm ID specification ";
                os << "starting at '" << fmt_spec.substr(i) << "'";
                string msg = CNcbiOstrstreamToString(os);
                NCBI_THROW(CInvalidDataException, eInvalidInput, msg);
            }
        }
        m_FiltAlgorithm = NStr::StringToInt(m_FilteringAlgoSpec, 
                                            NStr::fConvErr_NoThrow);
    }

    /// Validates that the parsed filtering algorithms ID indeed exists in the
    /// BLAST database. 
    void x_ValidateFilteringAlgorithm(CSeqDB& seqdb, 
                                      int filt_algorithm) 
    {
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION > 550))  &&  \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
        if (filt_algorithm == -1) {
            return;
        }

        vector<int> algo_ids (1, filt_algorithm);
        vector<int> invalid_algo_ids = 
            seqdb.ValidateMaskAlgorithms(algo_ids);
        if ( !invalid_algo_ids.empty() ) {
            NCBI_THROW(CInvalidDataException, eInvalidInput, 
                "Invalid filtering algorithm ID.");
        }
#endif
    }
};

END_NCBI_SCOPE

#endif  /* OBJTOOLS_BLASTDB_FORMAT__MASKING_FMT_SPEC__HPP */
