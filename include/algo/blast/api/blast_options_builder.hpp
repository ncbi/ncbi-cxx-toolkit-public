#ifndef ALGO_BLAST_API___BLAST_OPTIONS_BUILDER__HPP
#define ALGO_BLAST_API___BLAST_OPTIONS_BUILDER__HPP

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
 * Authors:  Kevin Bealer
 *
 */

/// @file blast_options_builder.hpp
/// Declares the CBlastOptionsBuilder class.

#include <algo/blast/api/blast_options_handle.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Class to build CBlastOptionsHandle from blast4 ASN objects.
///
/// This class takes a program, service, and lists of name/value
/// inputs in the form of the blast4 ASN objects, and builds a
/// CBlastOptionsHandle object.  Some fields expressed in blast4's
/// returned data are not part of the CBlastOptionsHandle; these are
/// returned via seperate getters.

class NCBI_XBLAST_EXPORT CBlastOptionsBuilder {
public:
    /// List of name/value pairs.
    typedef objects::CBlast4_parameters::Tdata TValueList;

    /// List of Blast4 masks.
    typedef list< CRef<objects::CBlast4_mask> > TMaskList;

    /// Constructor
    ///
    /// This takes the program and service strings, using them to
    /// determine the type of CBlastOptionsHandle to return.  Some of
    /// the name/value pairs also influence the type of blast options
    /// handle required.
    ///
    /// @param program Blast4 program string (e.g. blastn or blastp).
    /// @param service Blast4 service string (e.g. plain or rpsblast).
    /// @param locality Locality of the resulting object.
    CBlastOptionsBuilder(const string                & program,
                         const string                & service,
                         CBlastOptions::EAPILocality   locality
                         = CBlastOptions::eLocal,
                         bool ignore_unsupported_options = false);
    
    /// Build and return options as a CBlastOptionsHandle.
    ///
    /// A CBlastOptionsHandle is constructed and returned.
    ///
    /// @param aopts List of algorithm options [in].
    /// @param popts List of program options [in].
    /// @param fopts List of formatting options [in].
    /// @param task_name name of the task deduced from the arguments [in|out]
    CRef<CBlastOptionsHandle>
    GetSearchOptions(const objects::CBlast4_parameters * aopts,
                     const objects::CBlast4_parameters * popts,
                     const objects::CBlast4_parameters * fopts,
                     string *task_name=NULL);

    /// Check whether an Entrez query is specified.
    bool HaveEntrezQuery();
    
    /// Get the Entrez query.
    string GetEntrezQuery();
    
    /// Check whether an OID range start point is specified.
    bool HaveFirstDbSeq();
    
    /// Get the OID range start point.
    int GetFirstDbSeq();
    
    /// Check whether an OID range end point is specified.
    bool HaveFinalDbSeq();
    
    /// Get the OID range end point.
    int GetFinalDbSeq();
    
    /// Check whether a GI list is specified.
    bool HaveGiList();
    
    /// Get the GI list.
    list<int> GetGiList();
    
    /// Check whether a negative GI list is specified.
    bool HaveNegativeGiList();
    
    /// Get the negative GI list.
    list<int> GetNegativeGiList();

    /// Check whether a database filtering algorithm ID is specified
    bool HasDbFilteringAlgorithmId();
    /// Get the database filtering algorithm ID
    int GetDbFilteringAlgorithmId();

    /// Check whether a database filtering algorithm key is specified
    bool HasDbFilteringAlgorithmKey();
    /// Get the database filtering algorithm key
    string GetDbFilteringAlgorithmKey();

    /// Get Subject Maksing Type (soft/hard)
    bool HasSubjectMaskingType();
    ESubjectMaskingType GetSubjectMaskingType();

    /// Check whether query masks are specified.
    bool HaveQueryMasks();

    /// Get the query masks.
    TMaskList GetQueryMasks();

    /// Return the range that was used to restrict the query sequence(s)
    /// (returns TSeqRange::GetEmpty() if not applicable)
    TSeqRange GetRestrictedQueryRange() { return m_QueryRange; }

    /// Set the 'ignore unsupported options' flag.
    /// @param ignore True if the unsupported options should be ignored [in]
    void SetIgnoreUnsupportedOptions(bool ignore);

    /// Compute the EProgram value to use for this search.
    ///
    /// The blast4 protocol uses a notion of program and service to
    /// represent the type of search to do.  This method computes the
    /// EProgram value corresponding to these strings.  Sometimes this
    /// result should be modified based on the value of other options,
    /// specifically the existence of the PHI pattern indicates PHI
    /// blast and certain megablast template options that can indicate
    /// discontiguous megablast.
    /// @sa AdjustProgram.
    ///
    /// @param program The program string used by blast4.
    /// @param service The service string used by blast4.
    /// @return The EProgram value corresponding to these strings.
    static EProgram ComputeProgram(const string & program,
                                   const string & service);
    
    /// Adjust the EProgram based on option values.
    ///
    /// The blast4 protocol uses a notion of program and service to
    /// represent the type of search to do.  However, for some values
    /// of program and service, it is necessary to look at options
    /// values in order to determine the precise EProgram value.  This
    /// is particularly true when dealing with discontiguous megablast
    /// for example.  This method adjusts the program value based on
    /// the additional information found in these options.
    ///
    /// @param L The list of options used for this search.
    /// @param program The EProgram suggested by program+service.
    /// @param program_string The program as a string.
    /// @return The EProgram value as adjusted by options or the argument
    /// program if L is NULL
    static EProgram AdjustProgram(const TValueList * L,
                                  EProgram           program,
                                  const string     & program_string);
    
private:
    /// Optional-value idiom.
    ///
    /// This template defines both a value type and a flag to indicate
    /// whether that value type has been set.  Rather than require the
    /// designer to choose out-of-range values that indicate an unset
    /// condition for each field, this design encodes this information
    /// directly.  It is parameterized on the type of data to store.
    template<typename T>
    class SOptional {
    public:
        /// Constructor.
        SOptional()
            : m_IsSet(false), m_Value(T())
        {
        }
        
        /// Check whether the value has been set.
        bool Have()
        {
            return m_IsSet;
        }
        
        /// Get the value.
        T Get()
        {
            return m_Value;
        }

        /// Get the reference to the stored value.
        T& GetRef()
        {
            return m_Value;
        }
        
        /// Assign the field from another optional field.
        SOptional<T> & operator=(const T & x)
        {
            m_IsSet = true;
            m_Value = x;
            return *this;
        }
        
    private:
        /// True if the value has been specified.
        bool m_IsSet;
        
        /// The value itself, valid only if m_IsSet is true.
        T    m_Value;
    };
    
    /// Apply values directly to BlastOptions object.
    ///
    /// This function applies the values of certain blast4 parameter
    /// list options to the BlastOptions object.  It is called after
    /// all other options are set.  This design allows options which
    /// interact with each other to be handled as a group.
    ///
    /// @param boh Blast options handle.
    void x_ApplyInteractions(CBlastOptionsHandle & boh);
    
    /// Apply the value of one option to the CBlastOptionsHandle.
    /// @param opts The blast options handle.
    /// @param p The parameter to apply to the options handle.
    void x_ProcessOneOption(CBlastOptionsHandle        & opts,
                            objects::CBlast4_parameter & p);
    
    /// Apply the value of all options to the CBlastOptionsHandle.
    ///
    /// A list of blast4 parameters is used to configure the provided
    /// CBlastOptionsHandle object.
    ///
    /// @param opts The blast options handle.
    /// @param L The list of parameters to add to the options.
    void x_ProcessOptions(CBlastOptionsHandle & opts,
                          const TValueList    * L);
    
    /// Program value for blast4 protocol.
    string m_Program;
    
    /// Service value for blast4 protocol.
    string m_Service;
    
    /// Whether to perform culling.
    bool m_PerformCulling;
    
    /// How much culling to do.
    int m_HspRangeMax;
    
    /// The entreq query to use (or none).
    SOptional<string>      m_EntrezQuery;
    
    /// The first OID to process (or none).
    SOptional<int>         m_FirstDbSeq;
    
    /// The last OID to process (or none).
    SOptional<int>         m_FinalDbSeq;
    
    /// The GI list (or none).
    SOptional< list<int> > m_GiList;
    
    /// The negative GI list (or none).
    SOptional< list<int> > m_NegativeGiList;

    /// The GI list (or none).
    SOptional<int> m_DbFilteringAlgorithmId;
    SOptional<string> m_DbFilteringAlgorithmKey;
    SOptional<ESubjectMaskingType> m_SubjectMaskingType;

    /// The query masking locations
    SOptional< TMaskList > m_QueryMasks;

    /// Indicated that query masks have already been collected
    bool m_IgnoreQueryMasks;

    /// The range to restrict the query sequence(s)
    TSeqRange m_QueryRange;

    /// API Locality of resulting options.
    CBlastOptions::EAPILocality m_Locality;

    /// Should this class quietly ignore unsupported options
    bool m_IgnoreUnsupportedOptions;

    /// Should loading of the megablast BLAST DB index be required?
    bool m_ForceMbIndex;
    /// Which megablast BLAST DB index name to load
    string m_MbIndexName;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___BLAST_OPTIONS_BUILDER__HPP */
