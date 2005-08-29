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

/// Import definitions from the ncbi::blast scope.

USING_SCOPE(ncbi::blast);

/// Wrapped String Literal
///
/// This class is a wrapper for a string literal.  By wrapping the
/// string, type information is attached to the data, and C++ static
/// typing prevents one class from being assigned to a field which is
/// of the wrong type.  The methods in the CNetblastSearchOpts::Apply
/// take varying several sets of arguments, which could easily be
/// confused.  This technique makes it easy to see errors in local
/// contexts (such as within the Apply method definition), because the
/// class name matches the type of data.  If the wrong set of classes
/// is passed to one of the methods named Same(), Remote(), and
/// Local(), it is detected as a type error.

class CStringLit {
public:
    /// Constructor
    ///
    /// The data is stored in the object by copying the pointer, which
    /// means that the provided string should endure for the lifetime
    /// of this object (such as a string literal).
    /// 
    /// @param x
    ///   The data this class will store.
    explicit CStringLit(const char * x)
        : m_ptr(x)
    {
    }
    
    /// An implicit conversion to const char *.
    ///
    /// This object can be automatically converted to a const char
    /// pointer.
    operator const char *()
    {
        return m_ptr;
    }
    
    /// An implicit conversion to string.
    ///
    /// This object can be automatically converted to a string object.
    operator string()
    {
        return string(m_ptr);
    }

private:
    /// The string this object manages.
    const char * m_ptr;
};

/// User Option
///
/// This class contains the command line argument the user will
/// provide to set this option's value.

class CUserOpt : public CStringLit {
public:
    /// Constructor
    ///
    /// The user option is stored in the object by passing it to the
    /// base class.  This constructor is explicit, to prevent implicit
    /// conversions from strings or character pointers.
    /// 
    /// @param x
    ///   The user option as a string literal.
    
    explicit CUserOpt(const char * x)
        : CStringLit(x)
    {
    }
};

/// Blast4 Network API Option Name
///
/// This class contains the blast4 api option name.  The blast4 API
/// communicates options using a key-value pair idiom.

class CNetName : public CStringLit {
public:
    /// Constructor
    ///
    /// The option's network name is stored in the object by passing
    /// it to the base class.  This constructor is explicit, to
    /// prevent implicit conversions from strings or character
    /// pointers.
    /// 
    /// @param x
    ///   The blast4 network name as a string literal.
    
    explicit CNetName(const char * x)
        : CStringLit(x)
    {
    }
};

/// Argument Key
///
/// This is just a unique string that acts as a handle to access the
/// option from the NCBI command line processing classes.

class CArgKey : public CStringLit {
public:
    /// Constructor
    ///
    /// The CArgDesc argument key is stored in the object by passing
    /// it to the base class.  This constructor is explicit, to
    /// prevent implicit conversions from strings or character
    /// pointers.
    /// 
    /// @param x
    ///   The argument key as a string literal.
    
    explicit CArgKey(const char * x)
        : CStringLit(x)
    {
    }
};

/// Command Line Option Description
///
/// This class contains a description of the option which will be
/// provided in the program's usage documentation.  This documentation
/// is shown when a user runs the program with the command line option
/// "-" or enters incorrect command line options.

class COptDesc : public CStringLit {
public:
    /// Constructor
    ///
    /// The option description is stored in the object by passing it
    /// to the base class.  This constructor is explicit, to prevent
    /// implicit conversions from strings or character pointers.
    /// 
    /// @param x
    ///   The option description as a string literal.
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

class COptionWalker {
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

    /// Require this boolean function.
    virtual bool NeedRemote() = 0;
    
    /// Avoid compiler warning.
    virtual ~COptionWalker()
    {
    }
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

/// This macro produces stderr output if trace_blast_api is set.

#define WRITE_API_IDENT(NAME)                                \
    if (trace_blast_api) {                                   \
        cerr << "BlastAPI call: Set" # NAME "(...)" << endl; \
    }


/// This provides the start of a class definition, including the
/// SetValue() method and an error version of the Set() method.  The
/// first argument is a name of a blast option field, and the second
/// is an expression to compute the value of the option.

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
        throw runtime_error                    \
            ("program / parm mismatch");       \
    }

/// This macro continues the option handler definition by providing a
/// Set() method that will apply the method to a given blast options
/// class.  Each field can be applied to a number of options classes,
/// where each class corresponds to blastn, blastp, or similar.

#define OPT_HANDLER_SUPPORT(OPTIONTYPE) \
    template<class ValT>                \
    void Set(CRef<OPTIONTYPE>& opts,    \
             ValT & V)                  \
    {                                   \
        SetValue(opts, V);              \
    }

/// This ends the class by providing the terminating brace -- in a way
/// that will hopefully not confuse automatic indentation programs.

#define OPT_HANDLER_END() }


// Helper and Shortcut Handler Sets

/// This macro builds an option handler that applies one search option
/// to all available blast options classes.

#define OPT_HANDLER_SUPPORT_ALL(NAME)               \
OPT_HANDLER_START(NAME, V)                          \
OPT_HANDLER_SUPPORT(CBlastxOptionsHandle)           \
OPT_HANDLER_SUPPORT(CTBlastnOptionsHandle)          \
OPT_HANDLER_SUPPORT(CTBlastxOptionsHandle)          \
OPT_HANDLER_SUPPORT(CBlastProteinOptionsHandle)     \
OPT_HANDLER_SUPPORT(CBlastNucleotideOptionsHandle)  \
OPT_HANDLER_END()

/// This macro builds an option handler that applies one search option
/// to all available blast options classes, using an expression to
/// compute the search option value.

#define OPT_HANDLER_EXPR_ALL(NAME, EXPR)            \
OPT_HANDLER_START(NAME, EXPR)                       \
OPT_HANDLER_SUPPORT(CBlastxOptionsHandle)           \
OPT_HANDLER_SUPPORT(CTBlastnOptionsHandle)          \
OPT_HANDLER_SUPPORT(CTBlastxOptionsHandle)          \
OPT_HANDLER_SUPPORT(CBlastProteinOptionsHandle)     \
OPT_HANDLER_SUPPORT(CBlastNucleotideOptionsHandle)  \
OPT_HANDLER_END()

/// This macro applies the given search option to all nucleotide
/// classes (currently this only includes one class).

#define OPT_HANDLER_SUPPORT_NUCL(NAME)              \
OPT_HANDLER_START(NAME, V)                          \
OPT_HANDLER_SUPPORT(CBlastNucleotideOptionsHandle)  \
OPT_HANDLER_END()

// Translated Query (blastx, tblastx)

/// This macro applies the given search option to all types of
/// searches which utilize query translation.

#define OPT_HANDLER_SUPPORT_TRQ(NAME)       \
OPT_HANDLER_START(NAME, V)                  \
OPT_HANDLER_SUPPORT(CBlastxOptionsHandle)   \
OPT_HANDLER_SUPPORT(CTBlastxOptionsHandle)  \
OPT_HANDLER_END()

// Translated DB (tblastx, tblastn)

/// This macro applies a given search option to all types of searches
/// which utilize database translation.

#define OPT_HANDLER_SUPPORT_TRD(NAME)       \
OPT_HANDLER_START(NAME, V)                  \
OPT_HANDLER_SUPPORT(CTBlastnOptionsHandle)  \
OPT_HANDLER_SUPPORT(CTBlastxOptionsHandle)  \
OPT_HANDLER_END()

/// This macro applies the option to the CRemoteBlast class.  The
/// option is considered a program option rather than an algorithm
/// option.  This macro uses EXPR to compute the option value.

#define OPT_HANDLER_RB_EXPR(NAME,EXPR) \
OPT_HANDLER_START(NAME, EXPR)          \
OPT_HANDLER_SUPPORT(CRemoteBlast)      \
OPT_HANDLER_END()


// CBlastOptions based handlers

/// All search types can specify GapOpeningCost.
OPT_HANDLER_SUPPORT_ALL(GapOpeningCost);

/// All search types can specify GapExtensionCost.
OPT_HANDLER_SUPPORT_ALL(GapExtensionCost);

/// All search types can specify the WordSize.
OPT_HANDLER_SUPPORT_ALL(WordSize);

/// All search types can specify the matrix name.
OPT_HANDLER_EXPR_ALL   (MatrixName, V.c_str());

/// Searches with query translation can specify the query genetic code.
OPT_HANDLER_SUPPORT_TRQ(QueryGeneticCode);

/// Searches with database translation can specify the database genetic code.
OPT_HANDLER_SUPPORT_TRD(DbGeneticCode);

/// All search types can specify the size of the effective search space.
OPT_HANDLER_EXPR_ALL   (EffectiveSearchSpace, (long long) V);

/// All search types can specify filtering options.
OPT_HANDLER_EXPR_ALL   (FilterString, V.c_str());

/// All search types can specify whether a search is gapped.
OPT_HANDLER_SUPPORT_ALL(GappedMode);

/// All search types can specify the size of the hit list.
OPT_HANDLER_SUPPORT_ALL(HitlistSize);

/// All search types can specify an E-value threshold.
OPT_HANDLER_SUPPORT_ALL(EvalueThreshold);

// Nucleotide only

/// Nucleotide searches can specify a mismatch penalty.
OPT_HANDLER_SUPPORT_NUCL(MismatchPenalty);

/// Nucleotide searches can specify a match reward.
OPT_HANDLER_SUPPORT_NUCL(MatchReward);


// CBlast4Options based handlers

/// The Entrez Query is applied to the Remote Blast API object.
OPT_HANDLER_RB_EXPR(EntrezQuery, V.c_str());


/// Search Options for the Remote Blast Application
///
/// This class acts as the primary interface for the set of search
/// options available to the remote_blast demo application.  It
/// contains the relevant data in COptional<> template objects, and
/// defines the properties of the fields using the code in the Apply()
/// template method.

class CNetblastSearchOpts {
public:
    /// Default Constructor
    ///
    /// This constructor is used by the CreateInterface() function,
    /// which needs to iterate over the fields before any option
    /// values are available, to build the list of input fields.
    CNetblastSearchOpts()
    {
    }
    
    /// CArgs Based Constructor
    ///
    /// Construct the object, filling in the fields from the provided
    /// CArgs object.
    CNetblastSearchOpts(const CArgs & a);
    
    /// Create an interface for the application.
    ///
    /// This method creates an interface for the program based on
    /// the option behaviour defined in the Apply() method.
    static void CreateInterface(CArgDescriptions & ui);
    
    /// Apply the operation specified by "op" to each search option.
    ///
    /// This will apply the operation specified by "op" (which is
    /// probably derived from OptionWalker) to each search option.
    /// This method does not take a remote blast object or options
    /// handle object, and is meant for tasks where those objects are
    /// irrelevant.  The OptionWalker derived class defines all
    /// relevant behavior here - this class just provides a way to
    /// apply that code to all of the available options at once.
    /// 
    /// @param op
    ///   Object defining an operation over the search options.
    /// @sa OptionWalker, InterfaceBuilder, OptionReader, SearchParamBuilder.
    
    template <class OpWlkTp>
    void Apply(OpWlkTp & op)
    {
        // Non-remote versions don't need to know about the class
        // objects, so we send a null CRef<> here.
        
        CRef<CRemoteBlast> cb4o;
        Apply(op, cb4o, cb4o);
    }
    
    /// Apply the operation specified by "op" to each search option.
    ///
    /// This will apply the operation specified by "op" (which is
    /// probably derived from OptionWalker) to each search option.
    /// The object should have methods Local(), Remote(), and Same(),
    /// which take 4, 2, and 5 parameters) respectively.  To add a new
    /// option, one should provde an op.xxx() line here (or, for remote
    /// options, calculate the field's value (possibly from local
    /// options) in the section of this method marked "Computations &
    /// Remote values").
    /// 
    /// @param op
    ///   Object defining an operation over the search options.
    /// @param cboh
    ///   Blast options object to pass to the Same() or Remote() methods.
    /// @param cb4o
    ///   Remote Blast API object to pass to the Same() or Remote() methods.
    /// @sa OptionWalker, InterfaceBuilder, OptionReader, SearchParamBuilder.
    
    template <class OpWlkTp, class BlOptTp>
    void Apply(OpWlkTp            & op,
               CRef<BlOptTp>        cboh,
               CRef<CRemoteBlast>   cb4o)
    {
        // This option is not really a "Same()" value in terms of the
        // blast4 protocol.  But it is set in the same format between
        // local and remote because the blast options code hides the
        // difference.
        
        op.Same(m_Evalue,
                CUserOpt("E"),
                COptHandler_EvalueThreshold<BlOptTp>(),
                CArgKey ("ExpectValue"),
                COptDesc("Expect value (cutoff)."),
                cboh);
        
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
        }
    }
    
    /// Get the number of alignments to display.
    TOptInteger NumAligns()
    {
        return m_NumAlgn;
    }
    
    /// Returns the gapped alignment flag.
    TOptBool Gapped()
    {
        return m_Gapped;
    }
    
private:
    // Optional search parameters
    
    /// The evalue cutoff.
    TOptDouble  m_Evalue;
    
    /// The gap opening cost.
    TOptInteger m_GapOpen;
    
    /// The gap extension cost.
    TOptInteger m_GapExtend;
    
    /// The word size for this search.
    TOptInteger m_WordSize;
    
    /// The name of a standard blast matrix.
    TOptString  m_MatrixName;
    
    /// The penalty for a mismatch in a nucleotide search.
    TOptInteger m_NucPenalty;

    /// The reward for a match in a nucleotide search.
    TOptInteger m_NucReward;
    
    /// The number of descriptions lines to display.
    TOptInteger m_NumDesc;
    
    /// The number of pairwise alignments to show.
    TOptInteger m_NumAlgn;
    
    /// The threshold for inclusion of hits in a PSI blast search.
    TOptInteger m_Thresh;
    
    /// A flag indicated whether the alignment should be gapped.
    TOptBool    m_Gapped;
    
    /// The genetic code of the input query.
    TOptInteger m_QuGenCode;
    
    /// The genetic code used by the specified database.
    TOptInteger m_DbGenCode;
    
    /// A flag indicating whether to assume that the input query's
    /// defline is accurate.
    TOptBool    m_BelieveDef;
    
    /// The size of the search space.
    TOptDouble  m_Searchspc;
    
    /// A PHI blast query pattern.
    TOptString  m_PhiQuery;
    
    /// The types of filtering the search will use.
    TOptString  m_FilterString;
    
    /// An Entrez query limiting the parts of the database to include.
    TOptString  m_EntrezQuery;
    
    /// A helper method for the CreateInterface static method.
    void x_CreateInterface2(CArgDescriptions & ui);
};

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.11  2005/08/29 20:32:17  bealer
 * - Avoid compiler warning.
 *
 * Revision 1.10  2005/01/12 16:28:07  bealer
 * - Remove incorrect code and fold into other case.
 *
 * Revision 1.9  2005/01/12 15:07:44  vasilche
 * Commented out incorrect code.
 *
 * Revision 1.8  2004/11/30 22:08:30  ucko
 * Tweak OPT_HANDLER_SUPPORT to unconfuse GCC 2.95.
 *
 * Revision 1.7  2004/11/01 19:39:51  bealer
 * - More doxygen.
 *
 * Revision 1.6  2004/11/01 19:22:26  bealer
 * - Doxyg. docs for search_opts.hpp.
 *
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

