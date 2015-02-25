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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   Definition for CSeqMaskerIstat class.
 *
 */

#ifndef C_SEQ_MASKER_ISTAT_H
#define C_SEQ_MASKER_ISTAT_H

#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/version.hpp>

#include <algo/winmask/seq_masker_window.hpp>

#include <memory>

BEGIN_NCBI_SCOPE

/**
 **\brief Defines an interface for accessing the unit counts information.
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerIstat : public CObject
{
public:

    /**
        \brief Structure containing information about optimization
               parameters used.
     */
    struct optimization_data
    {
        /**
            \brief Object constructor.
            \param divisor initial value of the divisor_
            \param cba initial value of cba_
         */
        optimization_data( Uint4 divisor, Uint4 * cba )
            : divisor_( divisor/(8*sizeof( Uint4 )) ), cba_( cba ) 
        {}

        Uint4 divisor_;     /**< How many units are represented by one
                                 4-byte word in cba_ array. */
        Uint4 * cba_;       /**< Bit array with zeroes where all corresponding
                                 units have counts below t_extend. */
    };

    /**
        **\brief Object constructor.
        **\param arg_threshold the value of t_threshold to use instead of
        **                     the one supplied in the unit counts file
        **\param arg_textend the value of t_textend to use instead of
        **                     the one supplied in the unit counts file
        **\param arg_max_count the value of t_high to use instead of
        **                     the one supplied in the unit counts file
        **\param arg_use_max_count the count to use if the unit count is
        **                         greater than t_high
        **\param arg_min_count the value of t_low to use instead of
        **                     the one supplied in the unit counts file
        **\param arg_use_min_count the count to use if the unit count is
        **                         less than t_low
        **/
    explicit CSeqMaskerIstat(   Uint4 arg_threshold,
                                Uint4 arg_textend,
                                Uint4 arg_max_count,
                                Uint4 arg_use_max_count,
                                Uint4 arg_min_count,
                                Uint4 arg_use_min_count )
        :   threshold( arg_threshold ),
            textend( arg_textend ),
            max_count( arg_max_count ),
            use_max_count( arg_use_max_count ),
            min_count( arg_min_count ),
            use_min_count( arg_use_min_count ),
            ambig_unit( 0 ),
            opt_data_( 0, 0 )
    { total_ = 0; }

    /**
        **\brief Object destructor.
        **/
    virtual ~CSeqMaskerIstat() { if( opt_data_.cba_ ) delete[] opt_data_.cba_; }

    /**
        **\brief Look up the count value of a given unit.
        **\param unit the target unit
        **\return the count of the unit
        **/
    Uint4 operator[]( Uint4 unit ) const
    { 
        ++total_;
        return at( unit ); 
    }

    /**
        **\brief Get the unit size.
        **\return the unit size
        **/
    virtual Uint1 UnitSize() const = 0;

    /**
        **\brief Get the value of the unit used to represent an ambuguity.
        **\return ambiguity unit value
        **/
    const CSeqMaskerWindow::TUnit AmbigUnit() const
    { return ambig_unit; }

    /**
        **\brief Get the value of T_threshold.
        **\return T_threshold value
        **/
    Uint4 get_threshold() const { return threshold; }

    /**
        **\brief Get the value of T_extend.
        **\return T_extend value
        **/
    Uint4 get_textend() const { return textend; }

    /**
        \brief Get the data structure optimization parameters.
        \return pointer to optimization structure, if it is
                initialized, NULL otherwise
     */
    const optimization_data * get_optimization_data() const
    { return opt_data_.cba_ == 0 ? 0 : &opt_data_; }

    mutable Uint8 total_;

protected:

    /**
        **\brief Get the unit count of a given unit.
        **
        ** Derived classes should override this function
        ** to provide access to the unit counts.
        **
        **\param unit the unit value being looked up
        **\return count corrseponding to unit
        **/
    virtual Uint4 at( Uint4 unit ) const = 0;

public:

    /**
         \brief Get the true count for an n-mer.

         \param unit the n-mer value

         \return n-mer count not corrected for t_low
                 and t_high values
     **/
    virtual Uint4 trueat( Uint4 unit ) const = 0;

protected:

    /**
        **\brief Set the value of T_threshold.
        **\param arg_threshold new T_threshold value
        **/
    void set_threshold( Uint4 arg_threshold )
    { threshold = arg_threshold; }

    /**
        **\brief Set the value of T_extend.
        **\param arg_textend new T_extend value
        **/
    void set_textend( Uint4 arg_textend )
    { textend = arg_textend; }

public:

    /**
        **\brief Get the current value of T_high.
        **\return current T_high value
        **/
    Uint4 get_max_count() const { return max_count; }

protected:

    /**
        **\brief Set the value of T_high.
        **\param arg_max_count new T_high value
        **/
    void set_max_count( Uint4 arg_max_count )
    { max_count = arg_max_count; }

public:
    /**
        **\brief Get the count value for units with actual counts 
        **       above T_high.
        **\return value to use for units with count > T_high
        **/
    Uint4 get_use_max_count() const { return use_max_count; }

protected:
    /**
        **\brief Set the count value for units with actual counts
        **       above T_high.
        **\param arg_use_max_count new value to use for units with 
        **                         counts > T_high
        **/
    void set_use_max_count( Uint4 arg_use_max_count )
    { use_max_count = arg_use_max_count; }

public:

    /**
        **\brief Get the value of T_low.
        **\return current T_low value
        **/
    Uint4 get_min_count() const { return min_count; }

protected:

    /**
        **\brief Set the value of T_low.
        **\param arg_min_count new T_low value
        **/
    void set_min_count( Uint4 arg_min_count )
    { 
        if( min_count != 0 && min_count < arg_min_count ) {
            ERR_POST( Warning << "Requested value of t_low (" 
                              << min_count 
                              << ") is less than the one stored with the "
                              << "N-mer counts (" << arg_min_count << ")."
                              << "The value " << arg_min_count 
                              << " will be used." );
            min_count = arg_min_count;
        }
        else if( min_count == 0 ) {
            min_count = arg_min_count;
        }
    }

public:
    /**
        **\brief Get the count value for units with actual counts
        **       below T_low.
        **\return value to use for units with counts < T_low
        **/
    Uint4 get_use_min_count() const { return use_min_count; }

    /**\brief Return the metadata string. */
    string const & GetMetaData() const { return metadata; }

    /**\brief Return the encoding of the source statistics file. */
    string const & GetFmtEncoding() const { return fmt_encoding; }

    /**\brief Return the format version of the source statistics file. */
    CComponentVersionInfo const * GetFmtVersion() const {
        return fmt_version.get();
    }

protected:
    /**
        **\brief Set the count value for units with actual counts
        **       below T_low.
        **\param arg_use_min_count new value to use for units with
        **                         counts < T_low
        **/
    void set_use_min_count( Uint4 arg_use_min_count )
    { use_min_count = arg_use_min_count; }

    /**
        **\brief Set the unit size.
        **\param arg_unit_size new unit size value
        **/
    void set_unit_size( Uint1 arg_unit_size )
    { unit_size = arg_unit_size; }

    /**
        **\brief Set the ambiguity unit value
        **\param arg_ambig_unit new ambiguity unit
        **/
    void set_ambig_unit( 
        const CSeqMaskerWindow::TUnit & arg_ambig_unit )
    { ambig_unit = arg_ambig_unit; }

    /** 
        \brief Set optimization parameters.

        Constructor of the derived class is responsible for this.

        \param opt_data new optimization parameters
     */
    void set_optimization_data( const optimization_data & opt_data )
    { opt_data_ = opt_data; }

    /** Parse metadata string and set format version, encoding, and
        metadata.
    */
    void ParseMetaDataString( string const & md );

    /** Parse a given string as a format version specification. */
    bool ParseVersionString( string const & vs );

    /** Set metadata string. */
    void SetMetaData( string const & md ) { metadata = md; }

    /** Set the statistics file format encoding. */
    void SetFmtEncoding( string const & e ) { fmt_encoding = e; }

    /** Set the statistics file format version. */
    void SetFmtVersion( string const & name,
                           int major, int minor, int patch ) {
        fmt_version.reset( 
                new CComponentVersionInfo( name, major, minor, patch ) );
    }

private:

    /**\name Provide reference semantics for CSeqMaskerOstat. */
    /**@{*/
    CSeqMaskerIstat( const CSeqMaskerIstat & );
    CSeqMaskerIstat & operator=( const CSeqMaskerIstat & );
    /**@}*/

    Uint4 threshold;        /**<\internal T_threshold */
    Uint4 textend;          /**<\internal T_extend */
    Uint4 max_count;        /**<\internal T_high */
    Uint4 use_max_count;    /**<\internal Count to use for units with actual count > T_high. */
    Uint4 min_count;        /**<\internal T_low */
    Uint4 use_min_count;    /**<\internal Count to use for units with actual count < T_low. */
    Uint1 unit_size;        /**<\internal The unit size. */

    string metadata;        /**<\internal Metadata string. */
    string fmt_encoding;    /**<\internal Encoding of the stats file from which the data was read. */

    /** Format version of the statistics file from which the data was read. */
    std::auto_ptr< CComponentVersionInfo > fmt_version;

    CSeqMaskerWindow::TUnit ambig_unit; /**<\internal Unit value to represent ambiguities. */

    optimization_data opt_data_; /**<\internal Optimization parameters. */
};

END_NCBI_SCOPE

#endif
