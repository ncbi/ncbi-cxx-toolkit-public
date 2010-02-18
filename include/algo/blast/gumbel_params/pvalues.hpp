#ifndef ALGO_BLAST_GUMBEL_PARAMS__PVALUES___HPP
#define ALGO_BLAST_GUMBEL_PARAMS__PVALUES___HPP

/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: pvalues.hpp

Author: Greg Boratyn, Sergey Sheetlin

Contents: Interface for computation of P-values for given Gumbel parameters

******************************************************************************/

#include <corelib/ncbiobj.hpp>
#include <algo/blast/gumbel_params/gumbel_params.hpp>
#include <vector>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Input parameters for P-values calculation
///
class CScorePValuesOptions : public CObject
{
public:
    typedef Int4 TParam;     //Type for integer input parameters

   
public:

    /// Create options
    /// @param min_score Begining of the score range for p-values calculation
    /// [in]
    /// @param max_score End of the score range for p-values calculaton [in]
    /// @param seq1_len Length of the first sequence [in]
    /// @param seq2_len Length of the second sequence [in]
    /// @param gumbel_results Gumbel parameters along with error estimations
    /// [in]
    ///
    CScorePValuesOptions(TParam min_score, TParam max_score,
                         TParam seq1_len, TParam seq2_len,
                         const CConstRef<CGumbelParamsResult>& gumbel_results);

    
    //---- Getters and setters

    /// Set min score for which p-values will be computed
    /// @param score Min score [in]
    ///
    void SetMinScore(TParam score) {m_MinScore = score;}

    /// Get min score for which p-values will be computed
    /// @return Min score
    ///
    TParam GetMinScore(void) const {return m_MinScore;}

    /// Set max score for which p-values will be computed
    /// @param score Max score [in]
    ///
    void SetMaxScore(TParam score) {m_MaxScore = score;}

    /// Get max score for which p-values will be computed
    /// @return Max score
    ///
    TParam GetMaxScore(void) const {return m_MaxScore;}


    /// Set length of sequence 1
    /// @param len Sequence length [in]
    ///
    void SetSeq1Len(TParam len) {m_Seq1Len = len;}

    /// Get length of sequence 1
    /// @return Sequence length
    ///
    TParam GetSeq1Len(void) const {return m_Seq1Len;}

    /// Set length of sequence 2
    /// @param len Sequence length [in]
    ///
    void SetSeq2Len(TParam len) {m_Seq2Len = len;}

    /// Get length of sequence 2
    /// @return Sequence length
    ///
    TParam GetSeq2Len(void) const {return m_Seq2Len;}

    /// Set Gumbel parameters calculation results
    /// @param params Gumbel parameters calculation result [in]
    ///
    void SetGumbelResults(const CConstRef<CGumbelParamsResult>& params)
    {m_GumbelParams = params;}

    /// Get Gumbel parameters calculation results
    /// @return Gumbel parameters claculation results
    ///
    const CGumbelParamsResult& GetGumbelParams(void) const 
    {return *m_GumbelParams;}

    // ---- Validation ----

    /// Validates parameter values
    /// @return True if parameters pass validation, false otherwise
    ///
    bool Validate(void) const;

                                
protected:
    /// Forbid copy constructor
    CScorePValuesOptions(const CScorePValuesOptions&);
    /// Forbid assignment operator
    CScorePValuesOptions& operator=(const CScorePValuesOptions&);

protected:
    /// P-values will be computed for the range [m_MinScore1,m_MaxScore]
    TParam m_MinScore;

    /// P-values will be computed for the range [m_MinScore1,m_MaxScore]
    TParam m_MaxScore;

        //lengths of the sequences
    TParam m_Seq1Len;
    TParam m_Seq2Len;

    //Gumbel parameters calculation results
    CConstRef<CGumbelParamsResult> m_GumbelParams;
};

/// Results of score P-values calculation
class CScorePValues : public CObject
{
public:
    typedef double TPValue;

public:

    CScorePValues(void) {}

    /// Set p-values
    /// @return Vector of p-values
    ///
    vector<TPValue>& SetPValues(void) {return m_PValues;}

    /// Get p-values
    /// @return Vector of p-values
    ///
    const vector<TPValue>& GetPValues(void) const {return m_PValues;} 

    /// Set errors for p-values calculation
    /// @return Vector of errors
    ///
    vector<TPValue>& SetErrors(void) {return m_Errors;}

    /// Get errors for p-values calculation
    /// @return Vector of errors
    ///
    const vector<TPValue>& GetErrors(void) const {return m_Errors;} 

protected:
    /// Calculated p-values
    vector<TPValue> m_PValues;

    /// Errors for calculated p-values
    vector<TPValue> m_Errors; 
};



///Wrapper for P-values calculation
class CScorePValuesCalc
{
public:

    /// Constructor
    /// @param options Options [in]
    ///
    CScorePValuesCalc(const CConstRef<CScorePValuesOptions>& options)
        : m_Options(options) {}


    /// Calculate P-values
    ///
    CRef<CScorePValues> Run(void);

    /// Get results of P-values calculation
    /// @return Result of p-values calculation
    ///
    CRef<CScorePValues> GetResult(void);

protected:
    //Forbid copy constructor
    CScorePValuesCalc(const CScorePValuesCalc&);
    //Forbid assignement operator
    CScorePValuesCalc& operator=(CScorePValuesCalc&);

protected:
    CConstRef<CScorePValuesOptions> m_Options;
    CRef<CScorePValues> m_Result;
};


class CScorePValuesException : public CException
{
public:
    enum EErrCode {eInvalidOptions,
                   eResultNotSet, 
                   eGumbelParamsEmpty,
                   eMemoryAllocation,
                   eUnexpectedError
    };

    virtual const char* GetErrCodeString(void) const {
        return GetErrCode() == eResultNotSet 
            ? "eResultNotSet" : "eGumbelParamsEmpty";
    }

    NCBI_EXCEPTION_DEFAULT(CScorePValuesException, CException);
};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif // ALGO_BLAST_GUMBEL_PARAMS__PVALUES___HPP


