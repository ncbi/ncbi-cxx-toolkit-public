#ifndef ALGO_BLAST_API__VERSION__HPP
#define ALGO_BLAST_API__VERSION__HPP

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

/** @file version.hpp
 * Declares singleton objects to store the version and reference for the BLAST 
 * engine.
 */
 
#include <corelib/version_api.hpp>
#include <algo/blast/core/blast_engine.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Keeps track of the version of the BLAST engine in the NCBI C++ toolkit.
/// Used to perform run-time version checks
///
/// For reference, please refer to http://apr.apache.org/versioning.html
class CBlastVersion : public CVersionInfo {
public:
    CBlastVersion()
        : CVersionInfo(kBlastMajorVersion, 
                       kBlastMinorVersion, 
                       kBlastPatchVersion) {}
    virtual string Print(void) const {
        return CVersionInfo::Print() + "+";
    }
};


/// Class to keep track of the various BLAST references
class NCBI_XBLAST_EXPORT CReference
{
public:
    /// Enumerates the various BLAST publications
    enum EPublication {
        eGappedBlast = 0,           ///< 1997 NAR paper
        ePhiBlast,                  ///< 1998 NAR paper
        eMegaBlast,                 ///< 2000 J Compt Biol paper
        eCompBasedStats,            ///< 2001 NAR paper
        eCompAdjustedMatrices,      ///< submitted for publication
        eIndexedMegablast,          ///< 2008 Bioinformatics on indexed megablast
        eDeltaBlast,          	    ///< 2012 Biology Direct on DeltaBLAST
        eMaxPublications            ///< Used as sentinel value
    };

    /// Reference for requested publication
    static string GetString(EPublication pub);
    /// Reference for requested publication without umlaut
    static string GetHTMLFreeString(EPublication pub);
    /// Get Pubmed url for requested publication
    static string GetPubmedUrl(EPublication pub);

private:
    /// Prohibit constructing this class
    CReference();
    /// Prohibit copy constructor
    CReference(const CReference& rhs);
    /// Prohibit assignment operator
    CReference& operator=(const CReference& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__VERSION__HPP */
