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
    bool m_IsProtein;                // search molecular type
    string m_Origin;                 // the origin of species
    string m_DomainSystem;           // domain system for annotation
    CRef<CLocalDbAdapter> m_Db[4];   // user specified germline database
                                     // 0-2: - user specified V, D, J
                                     // 3:   - the default V gl db
    int  m_NumAlign[3];              // number of VDJ alignments to show
    bool m_FocusV;                   // should alignment restrict to V
    bool m_Translate;                // should translation be displayed
};

class NCBI_XBLAST_EXPORT CIgAnnotation : public CObject
{
public:
    bool m_MinusStrand;              // hit is on minus strand of the query
    vector<string> m_ChainType;      // chain type of the subjects
    int m_GeneInfo[6];               // The (start) and (end offset + 1) for VDJ
    int m_FrameInfo[2];              // Frame number for V end and J start
    int m_DomainInfo[13];            // The (start) and (end offset) for FWR1, 
                                     // CDR1, FWR2, CDR2, FWR3, CDR3 domains
                                     // plus the extension of the last annotated
                                     // domain
                                

    /// Constructor
    CIgAnnotation() 
        : m_MinusStrand (false) 
    {
        for (int i=0; i<6; i++) m_GeneInfo[i] = -1;
        for (int i=0; i<2; i++) m_FrameInfo[i] = -1;
        for (int i=0; i<13; i++) m_DomainInfo[i] = -1;
    }

};
    
class CIgAnnotationInfo
{
public:
    CIgAnnotationInfo(CConstRef<CIgBlastOptions> &ig_options);

    bool GetDomainInfo(const string sid, int * domain_info) {
        if (m_DomainIndex.find(sid) != m_DomainIndex.end()) {
            int index = m_DomainIndex[sid];
            for (int i=0; i<10; ++i) {
                domain_info[i] = m_DomainData[index + i];
            }
            return true;
        }
        return false;
    }

    const string GetChainType(const string sid) {
        if (m_ChainType.find(sid) != m_ChainType.end()) {
            return m_ChainType[sid];
        }
        return "N/A";
    }

    const int GetFrameOffset(const string sid) {
        if (m_FrameOffset.find(sid) != m_FrameOffset.end()) {
            return m_FrameOffset[sid];
        }
        return -1;
    }

private:
    map<string, int> m_DomainIndex;
    vector<int> m_DomainData;
    map<string, string> m_ChainType;
    map<string, int> m_FrameOffset;
};

class NCBI_XBLAST_EXPORT CIgBlastResults : public CSearchResults 
{
public:

    const CRef<CIgAnnotation> & GetIgAnnotation() const {
        return m_Annotation;
    }
  
    CRef<CIgAnnotation> & SetIgAnnotation() {
        return m_Annotation;
    }

    CRef<CSeq_align_set> & SetSeqAlign() {
        return m_Alignment;
    }
    
    /// Constructor
    /// @param query List of query identifiers [in]
    /// @param align alignments for a single query sequence [in]
    /// @param errs error messages for this query sequence [in]
    /// @param ancillary_data Miscellaneous output from the blast engine [in]
    /// @param query_masks Mask locations for this query [in]
    /// @param rid RID (if applicable, else empty string) [in]
    CIgBlastResults(CConstRef<objects::CSeq_id>   query,
                    CRef<objects::CSeq_align_set> align,
                    const TQueryMessages         &errs,
                    CRef<CBlastAncillaryData>     ancillary_data)
           : CSearchResults(query, align, errs, ancillary_data) { }

private:
    CRef<CIgAnnotation> m_Annotation;
};

class NCBI_XBLAST_EXPORT CIgBlast : public CObject
{
public:
    /// Local Igblast search API
    /// @param query_factory  Concatenated query sequences [in]
    /// @param blastdb        Adapter to the BLAST database to search [in]
    /// @param options        Blast search options [in]
    /// @param ig_options     Additional Ig-BLAST specific options [in]
    CIgBlast(CRef<CBlastQueryVector> query_factory,
             CRef<CLocalDbAdapter> blastdb,
             CRef<CBlastOptionsHandle> options,
             CConstRef<CIgBlastOptions> ig_options)
       : m_IsLocal(true),
         m_NumThreads(1),
         m_Query(query_factory),
         m_LocalDb(blastdb),
         m_Options(options),
         m_IgOptions(ig_options),
         m_AnnotationInfo(ig_options) { }

    /// Remote Igblast search API
    /// @param query_factory  Concatenated query sequences [in]
    /// @param blastdb        Remote BLAST database to search [in]
    /// @param subjects       Subject sequences to search [in]
    /// @param options        Blast search options [in]
    /// @param ig_options     Additional Ig-BLAST specific options [in]
    CIgBlast(CRef<CBlastQueryVector> query_factory,
             CRef<CSearchDatabase> blastdb,
             CRef<IQueryFactory>   subjects,
             CRef<CBlastOptionsHandle> options,
             CConstRef<CIgBlastOptions> ig_options)
       : m_IsLocal(false),
         m_NumThreads(1),
         m_Query(query_factory),
         m_Subject(subjects),
         m_RemoteDb(blastdb),
         m_Options(options),
         m_IgOptions(ig_options),
         m_AnnotationInfo(ig_options) { }

    /// Destructor
    ~CIgBlast() {};

    /// Run the Ig-BLAST engine
    CRef<CSearchResultSet> Run();

    /// Set MT mode
    void SetNumberOfThreads(size_t nthreads) {
        m_NumThreads = nthreads;
    }

private:

    bool m_IsLocal;
    size_t m_NumThreads;
    CRef<CBlastQueryVector> m_Query;
    CRef<IQueryFactory> m_Subject;
    CRef<CLocalDbAdapter> m_LocalDb;
    CRef<CSearchDatabase> m_RemoteDb;
    CRef<CBlastOptionsHandle> m_Options;
    CConstRef<CIgBlastOptions> m_IgOptions;
    CIgAnnotationInfo m_AnnotationInfo;

    /// Prohibit copy constructor
    CIgBlast(const CIgBlast& rhs);

    /// Prohibit assignment operator
    CIgBlast& operator=(const CIgBlast& rhs);

    /// Prepare blast option handle and query for V germline database search
    void x_SetupVSearch(CRef<IQueryFactory>           &qf,
                        CRef<CBlastOptionsHandle>     &opts_hndl);

    /// Prepare blast option handle and query for D, J germline database search
    void x_SetupDJSearch(vector<CRef <CIgAnnotation> > &annots,
                         CRef<IQueryFactory>           &qf,
                         CRef<CBlastOptionsHandle>     &opts_hndl);

    /// Prepare blast option handle and query for specified database search
    void x_SetupDbSearch(vector<CRef <CIgAnnotation> > &annot,
                         CRef<IQueryFactory>           &qf);

    /// Anntate the V gene based on blast results
    static void s_AnnotateV(CRef<CSearchResultSet>        &results_V, 
                            vector<CRef <CIgAnnotation> > &annot);

    /// Anntate the domains based on blast results
    void x_AnnotateDomain(CRef<CSearchResultSet>        &gl_results, 
                          CRef<CSearchResultSet>        &dm_results, 
                          vector<CRef <CIgAnnotation> > &annot);

    /// Anntate the D, J genes based on blast results
    void x_AnnotateDJ(CRef<CSearchResultSet>        &results_D, 
                      CRef<CSearchResultSet>        &results_J, 
                      vector<CRef <CIgAnnotation> > &annot);

    /// Set the chain type and frame info
    void x_SetChainType(CRef<CSearchResultSet>        &results, 
                        vector<CRef <CIgAnnotation> > &annot);

    /// Append blast results to the final results
    static void s_AppendResults(CRef<CSearchResultSet> &results,
                                int                     num_aligns,
                                CRef<CSearchResultSet> &final_results);

    
    /// Append annotation info to the final results
    static void s_SetAnnotation(vector<CRef <CIgAnnotation> > &annot,
                                CRef<CSearchResultSet> &final_results);

};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___IGBLAST__HPP */
