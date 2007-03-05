#ifndef APP_BLAST_CLIENT___SEARCH_OPTS__HPP
#define APP_BLAST_CLIENT___SEARCH_OPTS__HPP

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
 * Author:  Kevin Bealer
 *
 */

/// @file search_opts.hpp
/// Encapsulates optional netblast search parameters.
///
/// The Netblast protocol provides a long list of optional search
/// parameters.  Some of these are already supported here - more will
/// be supported in the future.  This code takes responsibility for
/// this aspect of the blast_client.  Adding a new search parameter
/// should require modifications to ONLY this file.  This works toward
/// the OOP goal of designing interfaces and encapsulation in such a
/// way as to isolate parts of the program that will change.

#include "optional.hpp"
#include <objects/blast/Blast4_cutoff.hpp>

BEGIN_NCBI_SCOPE

class CStringLit
{
public:
    explicit CStringLit(const char * x)
        : m_ptr(x)
    {
    }
    
    operator const char *(void)
    {
        return m_ptr;
    }
    
    operator string(void)
    {
        return string(m_ptr);
    }

private:
    const char * m_ptr;
};

class CUserOpt : public CStringLit
{
public:
    explicit CUserOpt(const char * x)
        : CStringLit(x)
    {
    }
};

class CNetName : public CStringLit
{
public:
    explicit CNetName(const char * x)
        : CStringLit(x)
    {
    }
};

class CArgKey : public CStringLit
{
public:
    explicit CArgKey(const char * x)
        : CStringLit(x)
    {
    }
};

class COptDesc : public CStringLit
{
public:
    explicit COptDesc(const char * x)
        : CStringLit(x)
    {
    }
};

enum EListPick {
    EListAlgo = 1,
    EListProg
};


/// This is base class for classes that operate on the option set.
///
/// Functors that operate on the set of options in NetblastSearchOpts
/// are derived from this class; it provides a set of handy methods
/// for reading options (from the CArgs object, to the field) and
/// adding options (to the CArgDescriptions, based on a field object
/// and key name).  Note that the AddOpt() code does not actually modify the
/// field -- it merely uses C++ overloading to select the correct
/// CArgDescription parse type based on each field type.  Because of
/// this mechanism, changing the type of a field (from OptInteger to
/// OptDouble for example), makes all the necessary network protocol
/// and CArgs interface adjustments.
/// @sa NetblastSearchOpts - see also.

class COptionWalker {
public:
    /// Destructor.
    virtual ~COptionWalker()
    {
    }
    
    /// Read a boolean field.
    void ReadOpt(const CArgs & args,
                 TOptBool    & field,
                 const char  * key);
    
    /// Read a double field.
    void ReadOpt(const CArgs & args,
                 TOptDouble  & field,
                 const char  * key);
    
    /// Read an integer field.
    void ReadOpt(const CArgs & args,
                 TOptInteger & field,
                 const char  * key);
    
    /// Read a string field.
    void ReadOpt(const CArgs & args,
                 TOptString  & field,
                 const char  * key);
    
    
    /// Build the CArgDescription for a boolean field.
    void AddOpt(CArgDescriptions & ui,
                TOptBool         & field,
                const char       * name,
                const char       * synop,
                const char       * comment);
    
    /// Build the CArgDescription for a double field.
    void AddOpt(CArgDescriptions & ui,
                TOptDouble       & field,
                const char       * name,
                const char       * synop,
                const char       * comment);
    
    /// Build the CArgDescription for an integer field.
    void AddOpt(CArgDescriptions & ui,
                TOptInteger      & field,
                const char       * name,
                const char       * synop,
                const char       * comment);
    
    /// Build the CArgDescription for a string field.
    void AddOpt(CArgDescriptions & ui,
                TOptString       & field,
                const char       * name,
                const char       * synop,
                const char       * comment);

    /// Require this boolean function
    virtual bool NeedRemote(void) = 0;
};


/// This class stores search options for blast_client.
///
/// This class stores optional search parameters for blast_client.
/// The heart of this class is the Apply() method, which takes an
/// object as an input parameter and applies that object to each
/// search option field.  There are three types of fields: Local,
/// Remote(), and Same().  The Local fields represent blastcl4
/// options.  The Remote() fields represent netblast (socket protocol)
/// options.  The Same() fields are fields which are both Remote() and
/// Local; the value is passed directly through as a netblast
/// parameter.  Most fields fall into the Same() category.  The
/// objects passed to Apply() are called with the appropriate one of
/// the Local(), Remote(), or Same() methods for each field.  To add a
/// new field, create a new TOpt* type object in the private section,
/// and call Same(), Local() or Remote() with the properties of that
/// field in the Apply() method.


class CNetblastSearchOpts
{
public:
    /// Default constructor - used by CreateInterface().
    CNetblastSearchOpts(void)
    {
    }
    
    /// CArgs constructor - reads the values from the provided CArgs object.
    CNetblastSearchOpts(const CArgs & a);
    
    /// Create an interface for the program based on parameters in Apply().
    static void CreateInterface(CArgDescriptions & ui);
    
    /// Apply the operation specified by "op" to each search option.
    ///

    /// This will apply the operation specified by "op" (which is
    /// probably derived from OptionWalker) to each search option.
    /// The object should have methods Local(), Remote(), and Same(),
    /// which take 4, 2, and 5 parameters) respectively.  To add a new
    /// option, you should another op.xxx() line here (or for remote
    /// options, calculate the field's value (possibly from local
    /// options) in the section marked "Computations & Remote values").
    /// @param op Object defining an operation over the search options.
    /// @sa OptionWalker, InterfaceBuilder, OptionReader, SearchParamBuilder.
    template <class T>
    void Apply(T & op)
    {
        // Local values
        
        op.Local(m_Evalue,
                 CUserOpt("E"),
                 CArgKey ("ExpectValue"),
                 COptDesc("Expect value (cutoff)."));
        
        op.Same (m_GapOpen,
                 CUserOpt("gap_open"),
                 CNetName("gap-open"),
                 CArgKey ("GapOpenCost"),
                 COptDesc("Gap-open cost."),
                 EListAlgo);
        
        op.Same (m_GapExtend,
                 CUserOpt("gap_extend"),
                 CNetName("gap-extend"),
                 CArgKey ("GapExtendCost"),
                 COptDesc("Gap-extend cost."),
                 EListAlgo);
        
        op.Same (m_WordSize,
                 CUserOpt("wordsize"),
                 CNetName("word-size"),
                 CArgKey ("WordSize"),
                 COptDesc("Word size."),
                 EListAlgo);
        
        op.Same (m_Matrix,
                 CUserOpt("matrix"),
                 CNetName("matrix"),
                 CArgKey ("Matrix"),
                 COptDesc("Search frequency matrix."),
                 EListAlgo);
        
        op.Same (m_NucPenalty,
                 CUserOpt("nucpenalty"),
                 CNetName("nucl-penalty"),
                 CArgKey ("NucPenalty"),
                 COptDesc("Penalty for a nucleotide mismatch (blastn only)."),
                 EListAlgo);
        
        op.Same (m_NucReward,
                 CUserOpt("nucreward"),
                 CNetName("nucl-reward"),
                 CArgKey ("NucReward"),
                 COptDesc("Reward for a nucleotide match (blastn only)."),
                 EListAlgo);
        
        op.Local(m_NumDesc,
                 CUserOpt("numdesc"),
                 CArgKey ("NumDesc"),
                 COptDesc("Number of one line database sequence descriptions to show."));
        
        op.Local(m_NumAlgn,
                 CUserOpt("numalign"),
                 CArgKey ("NumAligns"),
                 COptDesc("Number of database sequence alignments to show."));
        
        op.Local(m_Gapped,
                 CUserOpt("gapped"),
                 CArgKey ("GappedAlign"),   
                 COptDesc("Perform gapped alignment."));
        
        op.Same (m_QuGenCode,
                 CUserOpt("qugencode"),
                 CNetName("genetic-code"),    
                 CArgKey ("QuGenCode"),     
                 COptDesc("Query Genetic code to use."),
                 EListAlgo);
        
        op.Same (m_DbGenCode,
                 CUserOpt("dbgencode"),
                 CNetName("db-genetic-code"), 
                 CArgKey ("DbGenCode"),     
                 COptDesc("DB Genetic code to use."),
                 EListAlgo);
        
        op.Same (m_Searchspc,
                 CUserOpt("searchspc"),
                 CNetName("searchsp-eff"),    
                 CArgKey ("SearchSpc"),     
                 COptDesc("Effective length of the search space."),
                 EListProg);
        
        op.Same (m_PhiQuery,
                 CUserOpt("phi_query"),
                 CNetName("phi-pattern"),     
                 CArgKey ("PhiQuery"),      
                 COptDesc("Pattern Hit Initiated search expression."),
                 EListAlgo);
        
        op.Same (m_FilterString,
                 CUserOpt("filter_string"),
                 CNetName("filter"),
                 CArgKey ("FilterString"),
                 COptDesc("Specifies the types of filtering to do."),
                 EListAlgo);
        
        op.Same (m_EntrezQuery,
                 CUserOpt("entrez_query"),
                 CNetName("entrez-query"),
                 CArgKey ("EntrezQuery"),
                 COptDesc("Search only in entries matching this Entrez query."),
                 EListProg);
        
        // Computations & Remote values
        
        if (op.NeedRemote()) {
            // Gapped is the default
            TOptBool ungapped = TOptBool::Invert(m_Gapped);
            op.Remote(ungapped, CNetName("ungapped-alignment"), EListAlgo);
            
            // Network only needs max
            TOptInteger num_hits = TOptInteger::Max(m_NumAlgn, m_NumDesc);
            op.Remote(num_hits, CNetName("hitlist-size"), EListProg);
            
            if (m_Evalue.Exists()) {
                typedef objects::CBlast4_cutoff TCutoff;
                CRef<TCutoff> cutoff(new TCutoff);
                cutoff->SetE_value(m_Evalue.GetValue());
                
                COptional< CRef<TCutoff> > cutoff_opt(cutoff);
                
                op.Remote(cutoff_opt, CNetName("cutoff"), EListAlgo);
            }
        }
    }
    
    /// Get the number of alignments to display.
    TOptInteger NumAligns(void)
    {
        return m_NumAlgn;
    }
    
    /// Returns gapped alignment flag.
    TOptBool Gapped(void)
    {
        return m_Gapped;
    }
    
private:
    // Optional search parameters
    
    TOptDouble  m_Evalue;
    TOptInteger m_GapOpen;
    TOptInteger m_GapExtend;
    TOptInteger m_WordSize;
    TOptString  m_Matrix;
    TOptInteger m_NucPenalty;
    TOptInteger m_NucReward;
    TOptInteger m_NumDesc;
    TOptInteger m_NumAlgn;
    TOptInteger m_Thresh;
    TOptBool    m_Gapped;
    TOptInteger m_QuGenCode;
    TOptInteger m_DbGenCode;
    TOptBool    m_BelieveDef;
    TOptDouble  m_Searchspc;
    TOptString  m_PhiQuery;
    TOptString  m_FilterString;
    TOptString  m_EntrezQuery;
        
    /// Internal method used by CreateInterface.
    void x_CreateInterface2(CArgDescriptions & ui);
};

END_NCBI_SCOPE

#endif // APP_BLAST_CLIENT___SEARCH_OPTS__HPP
