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
 * Author:  Ning Ma
 *
 */

/// @file igblast.hpp
/// Declares CIgBlast, the C++ API for the IG-BLAST engine.

#ifndef ALGO_BLAST_API___IGBLAST__HPP
#define ALGO_BLAST_API___IGBLAST__HPP

#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <objmgr/scope.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(blast)

class IQueryFactory;

class NCBI_XBLAST_EXPORT CIgBlastOptions : public CObject
{
public:
    // the germline database search must be carried out locally
    CRef<CLocalDbAdapter> m_Db[3];   // germline database
    string m_Origin;                 // the origin of species
    string m_DomainSystem;           // domain system to do annotation
    bool m_IsProtein;                // search molecular type
    bool m_FocusV;                   // should alignment restrict to V
    bool m_Translate;                // should translation be displayed
};

class NCBI_XBLAST_EXPORT CIgBlastResults : public CSearchResults 
{
public:
    bool m_MinusStrand;              // hit is on minus strand of the query
    // TODO: this may move to igblastn_app.cpp...
    map<string, string> m_ChainType; // chain type of the subjects
    vector<string> m_TopVMatches;    // Top 3 V matches
    int m_GeneInfo[6];               // The start and end offset for VDJ
    int m_FrameInfo[2];              // Frame number for V end and J start
    int m_DomainInfo[12];            // The start and end offset for FWR1, 
                                     // CDR1, FWR2, CDR2, FWR3 domains
};

class NCBI_XBLAST_EXPORT CIgBlast : public CObject
{
public:
    /// Local Igblast search API
    /// @param query_factory  Concatenated query sequences [in]
    /// @param blastdb        Adapter to the BLAST database to search [in]
    /// @param options        Blast search options [in]
    /// @param ig_options     Additional Ig-BLAST specific options [in]
    CIgBlast(CRef<IQueryFactory> query_factory,
             CRef<CLocalDbAdapter> blastdb,
             CConstRef<CBlastOptionsHandle> options,
             CConstRef<CIgBlastOptions> ig_options);

    /// Remote Igblast search API
    /// @param query_factory  Concatenated query sequences [in]
    /// @param blastdb        Remote BLAST database to search [in]
    /// @param subjects       Subject sequences to search [in]
    /// @param options        Blast search options [in]
    /// @param ig_options     Additional Ig-BLAST specific options [in]
    CIgBlast(CRef<IQueryFactory> query_factory,
             CRef<CSearchDatabase> blastdb,
             CRef<IQueryFactory>   subjects,
             CConstRef<CBlastOptionsHandle> options,
             CConstRef<CIgBlastOptions> ig_options);

    /// Destructor
    ~CIgBlast() {};

    /// Run the Ig-BLAST engine
    CRef<CSearchResultSet> Run();

private:

    CRef<IQueryFactory> m_Query;
    CRef<IQueryFactory> m_Subject;
    CRef<CLocalDbAdapter> m_LocalDb;
    CRef<CSearchDatabase> m_RemoteDb;
    CConstRef<CBlastOptionsHandle> m_Options;
    CConstRef<CIgBlastOptions> m_IgOptions;

    /// Prohibit copy constructor
    CIgBlast(const CIgBlast& rhs);

    /// Prohibit assignment operator
    CIgBlast& operator=(const CIgBlast& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___IGBLAST__HPP */
