#ifndef ALGO_BLAST_API_DEMO___SEARCH_OPTS__HPP
#define ALGO_BLAST_API_DEMO___SEARCH_OPTS__HPP

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
/// this aspect of the remote_blast program.  Adding a new search
/// parameter should require modifications to ONLY this file.  This
/// works toward the OOP goal of designing interfaces and
/// encapsulation in such a way as to isolate parts of the program
/// that will change.

#include "optional.hpp"
#include <corelib/ncbiargs.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <objects/blast/Blast4_cutoff.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(ncbi::blast);

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

class COptionWalker
{
public:
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


/// This class stores search options for remote_blast.
///
/// This class stores optional search parameters for remote_blast.
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


/*
Solution 1: Scrap all this, writing seperate code bases for N, P, TX,
   etc.  Problem: this was time consuming to write, even in the
   current, "good programmers try not to do something twice, insane
   programmers try not to do something once" code configuration.
   

Solution 2: Create a traitsy solution.  The Set(...) methods will use
   nucleotide, protein, and so on specialized types.  Each COptHandler
   will be derived from whatever types are appropriate for its pieces.

   Thus, there is a generic Set() for throwing, and a SetValue() for
   setting.  The Set() is overridden to call SetValue for the right
   cases.
*/

extern bool trace_blast_api;

#define WRITE_API_IDENT(NAME)                                \
    if (trace_blast_api) {                                   \
        cerr << "BlastAPI call: Set" # NAME "(...)" << endl; \
    }

#define OPT_HANDLER_START(NAMESEG, EXPR)       \
template <class BlOptTp>                       \
class COptHandler_ ## NAMESEG {                \
public:                                        \
    template<class ValT, class OptsT>          \
    void SetValue(OptsT & opts, ValT & V)      \
    {                                          \
        WRITE_API_IDENT(NAMESEG);              \
        opts->Set ## NAMESEG (EXPR);           \
    }                                          \
                                               \
    template<class ValT, class OptsT>          \
    void Set(OptsT &, ValT &)             \
    {                                          \
        /*cerr << "In ["              */       \
        /*     << __PRETTY_FUNCTION__ */       \
        /*     << "]:\n";             */       \
        throw runtime_error                    \
            ("program / parm mismatch");       \
    }

#define OPT_HANDLER_SUPPORT(OPTIONTYPE) \
    template<class ValT>                \
    void Set(CRef<OPTIONTYPE> opts,     \
             ValT & V)                  \
    {                                   \
        SetValue(opts, V);              \
    }

#define OPT_HANDLER_END() }

// Helper and Shortcut Handler Sets

#define OPT_HANDLER_SUPPORT_ALL(NAME)               \
OPT_HANDLER_START(NAME, V)                          \
OPT_HANDLER_SUPPORT(CBlastxOptionsHandle)           \
OPT_HANDLER_SUPPORT(CTBlastnOptionsHandle)          \
OPT_HANDLER_SUPPORT(CTBlastxOptionsHandle)          \
OPT_HANDLER_SUPPORT(CBlastProteinOptionsHandle)     \
OPT_HANDLER_SUPPORT(CBlastNucleotideOptionsHandle)  \
OPT_HANDLER_END()

#define OPT_HANDLER_EXPR_ALL(NAME, EXPR)            \
OPT_HANDLER_START(NAME, EXPR)                       \
OPT_HANDLER_SUPPORT(CBlastxOptionsHandle)           \
OPT_HANDLER_SUPPORT(CTBlastnOptionsHandle)          \
OPT_HANDLER_SUPPORT(CTBlastxOptionsHandle)          \
OPT_HANDLER_SUPPORT(CBlastProteinOptionsHandle)     \
OPT_HANDLER_SUPPORT(CBlastNucleotideOptionsHandle)  \
OPT_HANDLER_END()

#define OPT_HANDLER_SUPPORT_NUCL(NAME)              \
OPT_HANDLER_START(NAME, V)                          \
OPT_HANDLER_SUPPORT(CBlastNucleotideOptionsHandle)  \
OPT_HANDLER_END()

// Translated Query (blastx, tblastx)

#define OPT_HANDLER_SUPPORT_TRQ(NAME)       \
OPT_HANDLER_START(NAME, V)                  \
OPT_HANDLER_SUPPORT(CBlastxOptionsHandle)   \
OPT_HANDLER_SUPPORT(CTBlastxOptionsHandle)  \
OPT_HANDLER_END()

// Translated DB (tblastx, tblastn)

#define OPT_HANDLER_SUPPORT_TRD(NAME)       \
OPT_HANDLER_START(NAME, V)                  \
OPT_HANDLER_SUPPORT(CTBlastnOptionsHandle)  \
OPT_HANDLER_SUPPORT(CTBlastxOptionsHandle)  \
OPT_HANDLER_END()

// CRemoteBlast option .. i.e. a program option rather than an
// algorithmic option.  Because it is sent to CRemoteBlast, it does
// not need to deal with Protein vs. Nucleot; rather, it needs to
// support only the CRemoteBlast type.  Only EXPR type is defined.

#define OPT_HANDLER_RB_EXPR(NAME,EXPR) \
OPT_HANDLER_START(NAME, EXPR)          \
OPT_HANDLER_SUPPORT(CRemoteBlast)      \
OPT_HANDLER_END()


// CBlastOptions based handlers

OPT_HANDLER_SUPPORT_ALL(GapOpeningCost);
OPT_HANDLER_SUPPORT_ALL(GapExtensionCost);
OPT_HANDLER_SUPPORT_ALL(WordSize);
OPT_HANDLER_EXPR_ALL   (MatrixName, V.c_str());
OPT_HANDLER_SUPPORT_TRQ(QueryGeneticCode);
OPT_HANDLER_SUPPORT_TRD(DbGeneticCode);
OPT_HANDLER_EXPR_ALL   (EffectiveSearchSpace, (long long) V);
OPT_HANDLER_EXPR_ALL   (FilterString, V.c_str());
OPT_HANDLER_SUPPORT_ALL(GappedMode);
OPT_HANDLER_SUPPORT_ALL(HitlistSize);
OPT_HANDLER_SUPPORT_ALL(EvalueThreshold);

// Nucleotide only
OPT_HANDLER_SUPPORT_NUCL(MismatchPenalty);
OPT_HANDLER_SUPPORT_NUCL(MatchReward);


// CBlast4Options based handlers

OPT_HANDLER_RB_EXPR(EntrezQuery, V.c_str());


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
    
    // Non-remote versions don't need to know about the algo/blast/api
    // class objects, so we send null CRef<>s here.
    
    template <class OpWlkTp>
    void Apply(OpWlkTp & op)
    {
        // This could perhaps be done better..
        CRef<CRemoteBlast> cb4o;
        Apply(op, cb4o, cb4o);
    }
    
    template <class OpWlkTp, class BlOptTp>
    void Apply(OpWlkTp            & op,
               CRef<BlOptTp>        cboh,
               CRef<CRemoteBlast>   cb4o)
    {
        // Local values
        
        op.Local(m_Evalue,
                 CUserOpt("E"),
                 CArgKey ("ExpectValue"),
                 COptDesc("Expect value (cutoff)."));
        
        op.Same(m_GapOpen,
                CUserOpt("gap_open"),
                COptHandler_GapOpeningCost<BlOptTp>(),
                CArgKey ("GapOpenCost"),
                COptDesc("Gap-open cost."),
                cboh);
        
        op.Same(m_GapExtend,
                CUserOpt("gap_extend"),
                COptHandler_GapExtensionCost<BlOptTp>(),
                CArgKey ("GapExtendCost"),
                COptDesc("Gap-extend cost."),
                cboh);
        
        op.Same(m_WordSize,
                CUserOpt("wordsize"),
                COptHandler_WordSize<BlOptTp>(),
                CArgKey ("WordSize"),
                COptDesc("Word size."),
                cboh);
        
        op.Same(m_MatrixName,
                CUserOpt("matrixname"),
                COptHandler_MatrixName<BlOptTp>(),
                CArgKey ("MatrixName"),
                COptDesc("Search frequency matrix (name of matrix)."),
                cboh);
        
        op.Same(m_NucPenalty,
                CUserOpt("nucpenalty"),
                COptHandler_MismatchPenalty<BlOptTp>(),
                CArgKey ("NucPenalty"),
                COptDesc("Penalty for a nucleotide mismatch (blastn only)."),
                cboh);
        
        op.Same(m_NucReward,
                CUserOpt("nucreward"),
                COptHandler_MatchReward<BlOptTp>(),
                CArgKey ("NucReward"),
                COptDesc("Reward for a nucleotide match (blastn only)."),
                cboh);
        
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
        
        op.Same(m_QuGenCode,
                CUserOpt("qugencode"),
                COptHandler_QueryGeneticCode<BlOptTp>(),    
                CArgKey ("QuGenCode"),     
                COptDesc("Query Genetic code to use."),
                cboh);
        
        op.Same(m_DbGenCode,
                CUserOpt("dbgencode"),
                COptHandler_DbGeneticCode<BlOptTp>(), 
                CArgKey ("DbGenCode"),     
                COptDesc("DB Genetic code to use."),
                cboh);
        
        op.Same(m_Searchspc,
                CUserOpt("searchspc"),
                COptHandler_EffectiveSearchSpace<BlOptTp>(),
                CArgKey ("SearchSpc"),
                COptDesc("Effective length of the search space."),
                cboh);
        
#if 0
        op.Same(m_PhiQuery,
                CUserOpt("phi_query"),
                COptHandler_PHIPattern<BlOptTp>(),
                CArgKey ("PhiQuery"),
                COptDesc("Pattern Hit Initiated search expression."),
                cboh);
#endif
        
        op.Same(m_FilterString,
                CUserOpt("filter_string"),
                COptHandler_FilterString<BlOptTp>(),
                CArgKey ("FilterString"),
                COptDesc("Specifies the types of filtering to do."),
                cboh);
        
        op.Same(m_EntrezQuery,
                CUserOpt("entrez_query"),
                COptHandler_EntrezQuery<BlOptTp>(),
                CArgKey ("EntrezQuery"),
                COptDesc("Search only in entries matching this Entrez query."),
                cb4o);
        
        // Computations & Remote values
        
        if (op.NeedRemote()) {
            // Gapped is the default ---- Note that BlastAPI and netblast
            // disagree on this point.. may one day erupt in violence.
            
            op.Remote(m_Gapped,
                      COptHandler_GappedMode<BlOptTp>(),
                      cboh);
            
            // Network only needs max
            TOptInteger num_hits = TOptInteger::Max(m_NumAlgn, m_NumDesc);
            op.Remote(num_hits,
                      COptHandler_HitlistSize<BlOptTp>(),
                      cboh);
            
            if (m_Evalue.Exists()) {
                typedef objects::CBlast4_cutoff TCutoff;
                CRef<TCutoff> cutoff(new TCutoff);
                cutoff->SetE_value(m_Evalue.GetValue());
                
                COptional< CRef<TCutoff> > cutoff_opt(cutoff);
                
                op.Remote(cutoff_opt,
                          COptHandler_EvalueThreshold<BlOptTp>(),
                          cboh);
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
    TOptString  m_MatrixName;
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

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.5  2004/04/16 14:30:03  bealer
 * - Fix compiler warnings.
 *
 * Revision 1.4  2004/04/15 21:18:56  bealer
 * - Remove semi-colons so that solaris compiler will not choke.
 *
 * Revision 1.3  2004/03/16 19:41:56  vasilche
 * Namespace qualifier is invalid in extern declaration. Removed extra semicolons
 *
 * Revision 1.2  2004/02/18 18:29:59  bealer
 * - Fix entrez query and add support (to Apply) for Remote Blast program
 *   options.
 *
 * Revision 1.1  2004/02/18 17:04:43  bealer
 * - Adapt blast_client code for Remote Blast API, merging code into the
 *   remote_blast demo application.
 *
 * ===========================================================================
 */

END_NCBI_SCOPE

#endif // ALGO_BLAST_API_DEMO___SEARCH_OPTS__HPP

