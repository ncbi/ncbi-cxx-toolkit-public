#ifndef UTIL_MATH___PROMOTE__HPP
#define UTIL_MATH___PROMOTE__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

//
//
// Promotion classes
//
//

// the basic promotion type
// we then provide a *lot* of specialized classes to perform the
// correct promotions
template <typename T, typename U> struct SPromoteTraits
{
public:
    typedef T TPromote;
};


// these defines makes adding new types much easier to understand

//
// first, a define to promote identical types to themselves
#define NCBI_PROMOTE_TRAITS(type) \
template<> struct SPromoteTraits< type, type > { \
public: \
    typedef type    TPromote; \
}


//
// next, this type handles promotion of unlike types
#define NCBI_PROMOTE2_TRAITS(type1,type2,type3) \
template<> struct SPromoteTraits< type1, type2 > { \
public: \
    typedef type3   TPromote; \
}; \
template<> struct SPromoteTraits< type2, type1 > { \
public: \
    typedef type3   TPromote; \
}


//
// this macro makes the syntax a little easier to understand when declaring a
// promoted type
//
#if defined(NCBI_COMPILER_MSVC) && (_MSC_VER <= 1200)
#  define NCBI_PROMOTE(a,b) SPromoteTraits< a, b >::TPromote
#else
#  define NCBI_PROMOTE(a,b) typename SPromoteTraits< a, b >::TPromote
#endif

//
// comparisons for built-in types
// this is needed because we can define the correct sorts of type promotion
// for various template classes and global operators
//


//
// promotion of identical types
NCBI_PROMOTE_TRAITS(char);
NCBI_PROMOTE_TRAITS(unsigned char);
NCBI_PROMOTE_TRAITS(short);
NCBI_PROMOTE_TRAITS(unsigned short);
NCBI_PROMOTE_TRAITS(int);
NCBI_PROMOTE_TRAITS(unsigned int);
NCBI_PROMOTE_TRAITS(float);
NCBI_PROMOTE_TRAITS(double);

NCBI_PROMOTE2_TRAITS(char, unsigned char, unsigned char);
NCBI_PROMOTE2_TRAITS(char, short, short);
NCBI_PROMOTE2_TRAITS(char, unsigned short, unsigned short);
NCBI_PROMOTE2_TRAITS(char, int, int);
NCBI_PROMOTE2_TRAITS(char, unsigned int, unsigned int);
NCBI_PROMOTE2_TRAITS(char, float, float);
NCBI_PROMOTE2_TRAITS(char, double, double);
NCBI_PROMOTE2_TRAITS(unsigned char, short, short);
NCBI_PROMOTE2_TRAITS(unsigned char, unsigned short, unsigned short);
NCBI_PROMOTE2_TRAITS(unsigned char, int, int);
NCBI_PROMOTE2_TRAITS(unsigned char, unsigned int, unsigned int);
NCBI_PROMOTE2_TRAITS(unsigned char, float, float);
NCBI_PROMOTE2_TRAITS(unsigned char, double, double);
NCBI_PROMOTE2_TRAITS(short, unsigned short, unsigned short);
NCBI_PROMOTE2_TRAITS(short, int, int);
NCBI_PROMOTE2_TRAITS(short, unsigned int, unsigned int);
NCBI_PROMOTE2_TRAITS(short, float, float);
NCBI_PROMOTE2_TRAITS(short, double, double);
NCBI_PROMOTE2_TRAITS(unsigned short, int, int);
NCBI_PROMOTE2_TRAITS(unsigned short, unsigned int, unsigned int);
NCBI_PROMOTE2_TRAITS(unsigned short, float, float);
NCBI_PROMOTE2_TRAITS(unsigned short, double, double);
NCBI_PROMOTE2_TRAITS(int, unsigned int, unsigned int);
NCBI_PROMOTE2_TRAITS(int, float, float);
NCBI_PROMOTE2_TRAITS(int, double, double);
NCBI_PROMOTE2_TRAITS(unsigned int, float, float);
NCBI_PROMOTE2_TRAITS(unsigned int, double, double);
NCBI_PROMOTE2_TRAITS(float, double, double);


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/08/19 13:11:35  dicuccio
 * Added include for ncbistd.hpp
 *
 * Revision 1.5  2004/03/10 14:14:26  dicuccio
 * Changed include guard to be consistent.  Moved commented portion to
 * gui-specific directory.  Cleaned up error in template macros - add space around
 * types to avoid '>>' on brain-dead compilers
 *
 * Revision 1.4  2004/02/26 18:35:17  gorelenk
 * Added (_MSC_VER <= 1200) for definition of NCBI_PROMOTE - typename presents
 * causes a compilation error on MSVC 6.0.
 *
 * Revision 1.3  2004/02/26 17:44:29  dicuccio
 * Restore preprocessor guard for 'typename' in SPromote<>
 *
 * Revision 1.2  2004/02/26 16:52:34  gorelenk
 * Changed definition of NCBI_PROMOTE(a,b). MSVC compilers also demands
 * "typename" in this case.
 *
 * Revision 1.1  2004/02/09 17:34:12  dicuccio
 * Moved from gui/math
 *
 * Revision 1.4  2004/01/23 13:54:53  dicuccio
 * Lots of fixes.  Wrapped promote type designation in a macro.  Dropped use of
 * partial template specialization due to lack of support in MSVC
 *
 * Revision 1.3  2003/12/22 19:14:21  dicuccio
 * Fixed lots of bugs in referencing non-existent functions
 *
 * Revision 1.2  2003/06/09 19:45:19  dicuccio
 * Fixed promotion rules - defined TPromote in traits base class
 *
 * Revision 1.1  2003/06/09 19:30:50  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // UTIL_MATH___PROMOTE___HPP
