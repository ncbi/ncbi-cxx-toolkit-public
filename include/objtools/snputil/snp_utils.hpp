#ifndef SNP_OBJUTILS___SNP_UTILS__HPP
#define SNP_OBJUTILS___SNP_UTILS__HPP

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
 * Authors:  Melvin Quintos
 *
 * File Description:
 *   Declares Helper functions in NSnp class
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>

#include <objtools/snputil/snp_bitfield.hpp>

#include <objmgr/feat_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
///
/// NSnp --
///
/// Helper functions for SNP features
class NCBI_SNPUTIL_EXPORT NSnp
{
public:

    /// Determine if feature is a SNP
    ///
    /// @param mapped_feat
    ///   CMappedFeat object representing feature
    /// @return
    ///   - true if Subtype is variation
    ///   - false otherwise
    static bool             IsSnp(const CMappedFeat &mapped_feat);

    /// Get Create Time
    /// It will fetch the creation time based on the CAnnotDescr of the feature's
    ///   parent annotation object.
    ///
    /// @param mapped_feat
    ///   CMappedFeat object representing feature
    /// @return
    ///   - CTime object representing Creation time.  A default constructed CTime
    ///     object will be returned if no Create-time was found.
    static CTime            GetCreateTime(const CMappedFeat &mapped_feat);

    /// Return rsid of SNP
    ///
    /// @param mapped_feat
    ///   CMappedFeat object representing SNP feature
    /// @return
    ///   - rsid of SNP as set in its Tag data
    ///   - 0 if no rsid found
    static int              GetRsid(const CMappedFeat &mapped_feat);

    /// Return rsid of SNP
    ///
    /// @param mapped_feat
    ///   CSeq_feat object representing SNP feature
    /// @return
    ///   - rsid of SNP as set in its Tag data
    ///   - 0 if no rsid found
    static int              GetRsid(const CSeq_feat &feat);

    /// Return distance of neighbors in flanking sequence
    ///
    /// @param mapped_feat
    ///   CMappedFeat object representing feature
    /// @return
    ///   - length of neighbors on flanking sequencer
    ///   - 0 if no length information found
    static int              GetLength(const CMappedFeat &);

    /// Return distance of neighbors in flanking sequence
    ///
    /// @param mapped_feat
    ///   CSeq_feat object representing feature
    /// @return
    ///   - length of neighbors on flanking sequencer
    ///   - 0 if no length information found
    static int              GetLength(const CSeq_feat &);

    /// Return bitfield information stored as "QualityCodes"
    ///
    /// @param mapped_feat
    ///   CMappedFeat object representing snp feature
    /// @return
    ///   - bitfield created from octect sequence of QualityCodes
    ///   - CSnpBitfield is empty if no "QualityCodes" are found
    static CSnpBitfield     GetBitfield(const CMappedFeat &);

    /// Return bitfield information stored as "QualityCodes"
    ///
    /// @param mapped_feat
    ///   CSeq_feat object representing snp feature
    /// @return
    ///   - bitfield created from octect sequence of QualityCodes
    ///   - CSnpBitfield is empty if no "QualityCodes" are found
    static CSnpBitfield     GetBitfield(const CSeq_feat &feat);

    /// Check if SNP exists in GenBank database
    ///
    /// @param scope
    ///   CScope object representing scope of data
    /// @param mapped_feat
    ///   CMappedFeat object representing snp feature
    /// @param allele
    ///   string object representing allele of SNP (e.g. A or GG or -)
    /// @return
    ///   - true if SNP was found
    ///   - false otherwise
    static bool             IsSnpKnown( CScope &scope, const CMappedFeat &private_snp,  const string &allele=kEmptyStr);

    /// Check if SNP exists in GenBank database
    ///
    /// @param scope
    ///   CScope object representing scope of data
    /// @param loc
    ///   CSeq_loc representing location of SNP
    /// @param allele
    ///   string object representing allele of SNP (e.g. A or GG or -)
    /// @return
    ///   - true if SNP was found
    ///   - false otherwise
    static bool             IsSnpKnown( CScope &scope, const CSeq_loc& loc, const string &allele=kEmptyStr);

///////////////////////////////////////////////////////////////////////////////
// Private Methods
///////////////////////////////////////////////////////////////////////////////
private:

};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SNP_OBJUTILS___SNP_UTILS__HPP

