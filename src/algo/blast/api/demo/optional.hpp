#ifndef ALGO_BLAST_API_DEMO___OPTIONAL__HPP
#define ALGO_BLAST_API_DEMO___OPTIONAL__HPP

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

/// @file optional.hpp
/// Optional value idiom for remote_blast.
///
/// Optional value concept (but not the actual code) is from the Boost
/// libraries.  Many times in programming, we have the concept of a
/// parameter which can either take a value or be omitted.  The
/// Optional<> template expresses this idea directly in code, wrapping
/// any value type.  Each optional object contains a boolean which is
/// set to true if there is a meaningful value present, or false
/// otherwise.  Attempts to read a false value will result in an
/// assertion.  The presence of the value can be checked.  Also
/// provided are functions which return an optional value from a CArgs
/// object based on name; CArgs (unlike the C toolkit counterpart)
/// preserves the information of whether an option was selected or
/// omitted.  This obviates the need for 'sentinel' values, special
/// "unreasonable" default values for each parameter, used to detect
/// the absence of the parameter.  Optional values semantics are
/// important to remote_blast because of the design of the blast4
/// (netblast) network protocols.  This design states that the blast4
/// server would provide default values for (most) options if a value
/// is not specified by the client.  It is therefore important to
/// track which optional values are present.

#include <corelib/ncbiargs.hpp>

BEGIN_NCBI_SCOPE

/// Optional template - wrap any value type as an optional value.
///
/// Optional template - any value type may be wrapped as an optional
/// value.  If no value has been set, the object is considered invalid
/// and will fail an assertion if an attempt is made to read the
/// value.  This class also provides a few basic 'logic' operations on
/// optional values.

template <typename T>
class COptional
{
public:
    /// Value constructor - produces a valid (present) object.
    ///
    /// Copy the value from the value type, set object to valid.
    //  @param value The value to set.
    COptional(T value)
        : m_Value(value),
          m_Exists(true)
    {
    }
    
    /// Copy constructor - copies an existing object.
    ///
    /// Copy from another optional object.
    /// @param other COptional object (valid or not) to copy.
    COptional(const COptional<T> & other)
        : m_Exists(other.m_Exists)
    {
        if (m_Exists) {
            m_Value = other.m_Value;
        }
    }
    
    /// Blank constructor - construct an invalid object.
    COptional(void)
        : m_Exists(false)
    {
    }
    
    /// Returns true if the value is present/exists.
    bool Exists(void) const
    {
        return m_Exists;
    }
    
    /// Returns the value, or throws an assertion (if not valid).
    const T & GetValue(void) const
    {
        _ASSERT(m_Exists);
        return m_Value;
    }
        
    /// Boolean max() operator - return the max of existing values.
    ///
    /// If both arguments exist: return the max.  If neither argument
    /// exists, returns an invalid object.  Otherwise, one argument
    /// exists, so return that object.
    ///
    /// @param lhs One argument.
    /// @param rhs The other argument.
    static COptional<T> Max(const COptional<T> & lhs,
                            const COptional<T> & rhs);
    
    /// Boolean min() operator - return the min of existing values.
    ///
    /// If both arguments exist: return the min.  If neither argument
    /// exists, returns an invalid object.  Otherwise, one argument
    /// exists, so return that object.
    ///
    /// @param lhs One argument.
    /// @param rhs The other argument.
    static COptional<T> Min(const COptional<T> & lhs,
                            const COptional<T> & rhs);
    
    /// Unary inversion operator - return the inverse of the value.
    ///
    /// If the argument exists: return ! value().  Otherwise, return
    /// an invalid object.
    ///
    /// @param v The (possibly unset) value.
    
    static COptional<T> Invert(const COptional<T> & v);
    
    /// Assignment operator.
    ///
    /// Copy the state of another object.  If it exists, copy
    /// the value as well.
    
    const COptional<T> & operator= (const COptional<T> & other)
    {
        if (this != &other) {
            m_Exists = other.m_Exists;
                        
            if (m_Exists) {
                m_Value = other.m_Value;
            }
        }
                
        return *this;
    }
        
private:
    /// The actual value (if m_Exists is true).
    T    m_Value;
    
    /// If true, m_Value is meaningful.
    bool m_Exists;
};

using std::string;

/// Optional integer.
typedef COptional<int>            TOptInteger;

/// Optional bool.
typedef COptional<bool>           TOptBool;

/// Optional double prec float.
typedef COptional<double>         TOptDouble;

/// Optional string (pointer) value - be sure to delete the actual object.
typedef COptional<string> TOptString;

/// Optional input stream pointer - be sure to delete the actual object.
typedef COptional<CNcbiIstream *> TOptInfile;

/// Get existence and value from a boolean program parameter.
inline TOptBool CheckArgsBool(const CArgValue & val)
{
    if (val.HasValue()) {
        return TOptBool(val.AsBoolean());
    } else {
        return TOptBool();
    }
}

/// Get existence and value from a double program parameter.
inline TOptDouble CheckArgsDouble(const CArgValue & val)
{
    if (val.HasValue()) {
        return TOptDouble(val.AsDouble());
    } else {
        return TOptDouble();
    }
}

/// Get existence and value from an integer program parameter.
inline TOptInteger CheckArgsInteger(const CArgValue & val)
{
    if (val.HasValue()) {
        return TOptInteger(val.AsInteger());
    } else {
        return TOptInteger();
    }
}

/// Get existence and value from a string program parameter.
inline TOptString CheckArgsString(const CArgValue & val)
{
    if (val.HasValue()) {
        return TOptString(val.AsString());
    } else {
        return TOptString();
    }
}

template <class T>
inline COptional<T> COptional<T>::Max(const COptional<T> & lhs,
                                      const COptional<T> & rhs)
{
    COptional<T> result;
                
    if (lhs.m_Exists) {
        if (rhs.m_Exists) {
            result.m_Exists = true;
            result.m_Value = ((lhs.m_Value > rhs.m_Value)
                              ? lhs.m_Value
                              : rhs.m_Value);
        } else {
            result = lhs;
        }
    } else {
        if (rhs.m_Exists) {
            result = rhs;
        }
    }
                
    return result;
}

template <class T>
inline COptional<T> COptional<T>::Min(const COptional<T> & lhs,
                                      const COptional<T> & rhs)
{
    COptional<T> result;
                
    if (lhs.m_Exists) {
        if (rhs.m_Exists) {
            result.m_Exists = true;
            result.m_Value = ((lhs.m_Value < rhs.m_Value)
                              ? lhs.m_Value
                              : rhs.m_Value);
        } else {
            result = lhs;
        }
    } else {
        if (rhs.m_Exists) {
            result = rhs;
        }
    }
                
    return result;
}

template <class T>
inline COptional<T> COptional<T>::Invert(const COptional<T> & v)
{
    COptional<T> result;
    
    if (v.m_Exists) {
        result.m_Exists = true;
        result.m_Value = ! v.m_Value;
    }
    
    return result;
}

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.2  2004/05/19 14:52:02  camacho
 * 1. Added doxygen tags to enable doxygen processing of algo/blast/core
 * 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
 *    location
 * 3. Added use of @todo doxygen keyword
 *
 * Revision 1.1  2004/02/18 17:04:41  bealer
 * - Adapt blast_client code for Remote Blast API, merging code into the
 *   remote_blast demo application.
 *
 * ===========================================================================
 */

END_NCBI_SCOPE

#endif // ALGO_BLAST_API_DEMO___OPTIONAL__HPP

