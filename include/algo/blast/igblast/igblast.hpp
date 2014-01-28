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

#ifndef ALGO_BLAST_IGBLAST___IGBLAST__HPP
#define ALGO_BLAST_IGBLAST___IGBLAST__HPP

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

/// Keeps track of the version of IgBLAST in the NCBI C++ toolkit.
/// Used to perform run-time version checks
///
/// For reference, please refer to http://apr.apache.org/versioning.html
class CIgBlastVersion : public CVersionInfo {
public:
    CIgBlastVersion()
        : CVersionInfo(1, 0, 0) {}
};

class IQueryFactory;

class CIgBlastOptions : public CObject
{
public:
    // the germline database search must be carried out locally
    bool m_IsProtein;                // search molecular type
    string m_Origin;                 // the origin of species
    string m_DomainSystem;           // domain system for annotation
    string m_SequenceType;           //ig or tcr?
    int m_Min_D_match;               //the word size for D gene search
    int m_D_penalty;                 //the mismatch penalty for D gene search
    string m_AuxFilename;            // auxulary file name
    string m_IgDataPath;             // internal data path
    CRef<CLocalDbAdapter> m_Db[4];   // user specified germline database
                                     // 0-2: - user specified V, D, J
                                     // 3:   - the default V gl db
    int  m_NumAlign[3];              // number of VDJ alignments to show
    bool m_FocusV;                   // should alignment restrict to V
    bool m_Translate;                // should translation be displayed
};

class CIgAnnotation : public CObject
{
public:
    bool m_MinusStrand;              // hit is on minus strand of the query
    vector<string> m_TopGeneIds;     // Top match germline gene ID
    vector<string> m_ChainType;      // chain types of the query ([0]) and subjects ([1:])
    string m_ChainTypeToShow;        // chain type to show to user.  Normally this is 
                                     //the same as m_ChainType[0] but could be different
                                     // in case o TCRA/D chains which can use both JA and JD
    int m_GeneInfo[6];               // The (start) and (end offset + 1) for VDJ
    int m_FrameInfo[3];              // Coding frame start offset for V start, V end,
                                     // and V start.
    int m_DomainInfo[12];            // The (start) and (end offset) for FWR1, 
                                     // CDR1, FWR2, CDR2, FWR3, CDR3 domains
                                     // note: the first and last domains are be extended
    int m_DomainInfo_S[10];          // The (start) and (end offset) for FWR1, 
                                     // CDR1, FWR2, CDR2, FWR3, CDR3 domains on topV sequence

    /// Constructor
    CIgAnnotation() 
        : m_MinusStrand (false) 
    {
        for (int i=0; i<3; i++) m_TopGeneIds.push_back("N/A");
        for (int i=0; i<6; i++) m_GeneInfo[i] = -1;
        for (int i=0; i<3; i++) m_FrameInfo[i] = -1;
        for (int i=0; i<12; i++) m_DomainInfo[i] = -1;
        for (int i=0; i<10; i++) m_DomainInfo_S[i] = -1;
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

    const string GetDomainChainType(const string sid) {
        if (m_DomainChainType.find(sid) != m_DomainChainType.end()) {
            return m_DomainChainType[sid];
        }
        return "N/A";
    }

    int GetFrameOffset(const string sid) {
        if (m_FrameOffset.find(sid) != m_FrameOffset.end()) {
            return m_FrameOffset[sid];
        }
        return -1;
    }

    const string GetDJChainType(const string sid) {
        if (m_DJChainType.find(sid) != m_DJChainType.end()) {
            return m_DJChainType[sid];
        }
        return "N/A";
    }

private:
    map<string, int> m_DomainIndex;
    vector<int> m_DomainData;
    map<string, string> m_DomainChainType;
    map<string, int> m_FrameOffset;
    map<string, string> m_DJChainType;
};

class CIgBlastResults : public CSearchResults 
{
public:

    int m_NumActualV;
    int m_NumActualD;
    int m_NumActualJ;

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
           : CSearchResults(query, align, errs, ancillary_data),
             m_NumActualV(0), m_NumActualD(0), m_NumActualJ(0) { }

private:
    CRef<CIgAnnotation> m_Annotation;
};

class CIgBlast : public CObject
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
             CConstRef<CIgBlastOptions> ig_options,
             string entrez_query)
       : m_IsLocal(false),
         m_NumThreads(1),
         m_Query(query_factory),
         m_Subject(subjects),
         m_RemoteDb(blastdb),
         m_Options(options),
         m_IgOptions(ig_options),
         m_AnnotationInfo(ig_options),
         m_EntrezQuery(entrez_query){ }

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
    string m_EntrezQuery;

    /// Prohibit copy constructor
    CIgBlast(const CIgBlast& rhs);

    /// Prohibit assignment operator
    CIgBlast& operator=(const CIgBlast& rhs);

    /// Prepare blast option handle and query for V germline database search
    void x_SetupVSearch(CRef<IQueryFactory>           &qf,
                        CRef<CBlastOptionsHandle>     &opts_hndl);

    /// Prepare blast option handle and query for D, J germline database search
    void x_SetupDJSearch(const vector<CRef <CIgAnnotation> > &annots,
                         CRef<IQueryFactory>           &qf,
                         CRef<CBlastOptionsHandle>     &opts_hndl,
                         int db_type);

    /// Prepare blast option handle and query for specified database search
    void x_SetupDbSearch(vector<CRef <CIgAnnotation> > &annot,
                         CRef<IQueryFactory>           &qf);

    /// Annotate the V gene based on blast results
    void x_AnnotateV(CRef<CSearchResultSet>        &results,
                     vector<CRef <CIgAnnotation> > &annot);

    /// Annotate the D and J genes based on blast results
    void x_AnnotateDJ(CRef<CSearchResultSet>        &results_D,
                      CRef<CSearchResultSet>        &results_J,
                      vector<CRef <CIgAnnotation> > &annot);

    /// Annotate the query chaintype and domains based on blast results
    void x_AnnotateDomain(CRef<CSearchResultSet>        &gl_results, 
                          CRef<CSearchResultSet>        &dm_results, 
                          vector<CRef <CIgAnnotation> > &annot);

    /// Set the subject chain type and frame info
    void x_SetChainType(CRef<CSearchResultSet>        &results, 
                        vector<CRef <CIgAnnotation> > &annot);

    /// Convert bl2seq result to database search mode
    void x_ConvertResultType(CRef<CSearchResultSet>  &results);

    /// Sort blast results according to evalue
    static void s_SortResultsByEvalue(CRef<CSearchResultSet> &results);

    /// Append blast results to the final results
    static void s_AppendResults(CRef<CSearchResultSet> &results,
                                int                     num_aligns,
                                int                     gene,
                                CRef<CSearchResultSet> &final_results);

    
    /// Append annotation info to the final results
    static void s_SetAnnotation(vector<CRef <CIgAnnotation> > &annot,
                                CRef<CSearchResultSet> &final_results);

    void x_FindDJ(CRef<CSearchResultSet>& results_D,
                  CRef<CSearchResultSet>& results_J,
                  CRef <CIgAnnotation> & annot,
                  CRef<CSeq_align_set>& align_D,
                  CRef<CSeq_align_set>& align_J,
                  string q_ct,
                  bool q_ms,
                  ENa_strand q_st,
                  int q_ve,
                  int iq);

    void x_FindDJAln(CRef<CSeq_align_set>& align_D,
                     CRef<CSeq_align_set>& align_J,
                     string q_ct,
                     bool q_ms,
                     ENa_strand q_st,
                     int q_ve,
                     int iq,
                     bool va_or_vd_as_heavy_chain);
    
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_IGBLAST___IGBLAST__HPP */
