#ifndef UTIL_MATH___PROMOTE___HPP
#define UTIL_MATH___PROMOTE___HPP

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

// we don't explicitly include <corelib/ncbistd.hpp> because this file is not
// indented to be used directly
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
#define PROMOTE(type) \
template<> struct SPromoteTraits<type, type> { \
public: \
    typedef type    TPromote; \
}


//
// next, this type handles promotion of unlike types
#define PROMOTE2(type1,type2,type3) \
template<> struct SPromoteTraits<type1, type2> { \
public: \
    typedef type3   TPromote; \
}; \
template<> struct SPromoteTraits<type2, type1> { \
public: \
    typedef type3   TPromote; \
}


//
// this macro makes the syntax a little easier to understand when declaring a
// promoted type
//
#define NCBI_PROMOTE(a,b) typename SPromoteTraits<a,b>::TPromote

//
// comparisons for built-in types
// this is needed because we can define the correct sorts of type promotion
// for various template classes and global operators
//


//
// promotion of identical types
PROMOTE(char);
PROMOTE(unsigned char);
PROMOTE(short);
PROMOTE(unsigned short);
PROMOTE(int);
PROMOTE(unsigned int);
PROMOTE(float);
PROMOTE(double);

PROMOTE2(char, unsigned char, unsigned char);
PROMOTE2(char, short, short);
PROMOTE2(char, unsigned short, unsigned short);
PROMOTE2(char, int, int);
PROMOTE2(char, unsigned int, unsigned int);
PROMOTE2(char, float, float);
PROMOTE2(char, double, double);
PROMOTE2(unsigned char, short, short);
PROMOTE2(unsigned char, unsigned short, unsigned short);
PROMOTE2(unsigned char, int, int);
PROMOTE2(unsigned char, unsigned int, unsigned int);
PROMOTE2(unsigned char, float, float);
PROMOTE2(unsigned char, double, double);
PROMOTE2(short, unsigned short, unsigned short);
PROMOTE2(short, int, int);
PROMOTE2(short, unsigned int, unsigned int);
PROMOTE2(short, float, float);
PROMOTE2(short, double, double);
PROMOTE2(unsigned short, int, int);
PROMOTE2(unsigned short, unsigned int, unsigned int);
PROMOTE2(unsigned short, float, float);
PROMOTE2(unsigned short, double, double);
PROMOTE2(int, unsigned int, unsigned int);
PROMOTE2(int, float, float);
PROMOTE2(int, double, double);
PROMOTE2(unsigned int, float, float);
PROMOTE2(unsigned int, double, double);
PROMOTE2(float, double, double);


/**
//
// prmotion rules for non-builtin types
// we add these as we need them
//

//
// promote: int + CVect/CMatrix<int> --> ???
PROMOTE2(int, CVect2<int>, CVect2<int>);
PROMOTE2(int, CVect3<int>, CVect3<int>);
PROMOTE2(int, CVect4<int>, CVect4<int>);
PROMOTE2(int, CMatrix3<int>, CMatrix3<int>);
PROMOTE2(int, CMatrix4<int>, CMatrix4<int>);

//
// promote: int + CVect/CMatrix<float> --> ???
PROMOTE2(int, CVect2<float>, CVect2<float>);
PROMOTE2(int, CVect3<float>, CVect3<float>);
PROMOTE2(int, CVect4<float>, CVect4<float>);
PROMOTE2(int, CMatrix3<float>, CMatrix3<float>);
PROMOTE2(int, CMatrix4<float>, CMatrix4<float>);

//
// promote: float + CVect/CMatrix<int> --> ???
PROMOTE2(float, CVect2<int>, CVect2<float>);
PROMOTE2(float, CVect3<int>, CVect3<float>);
PROMOTE2(float, CVect4<int>, CVect4<float>);
PROMOTE2(float, CMatrix3<int>, CMatrix3<float>);
PROMOTE2(float, CMatrix4<int>, CMatrix4<float>);

//
// promote: float + CVect/CMatrix<float> --> ???
PROMOTE2(float, CVect2<float>, CVect2<float>);
PROMOTE2(float, CVect3<float>, CVect3<float>);
PROMOTE2(float, CVect4<float>, CVect4<float>);
PROMOTE2(float, CMatrix3<float>, CMatrix3<float>);
PROMOTE2(float, CMatrix4<float>, CMatrix4<float>);

//
// promote: double + CVect/CMatrix<double> --> ???
PROMOTE2(double, CVect2<double>, CVect2<double>);
PROMOTE2(double, CVect3<double>, CVect3<double>);
PROMOTE2(double, CVect4<double>, CVect4<double>);
PROMOTE2(double, CMatrix3<double>, CMatrix3<double>);
PROMOTE2(double, CMatrix4<double>, CMatrix4<double>);

//
// promote: CVect/CMatrix<float> + CVect/CMatrix<float> --> ???
//PROMOTE2(CVect3<float>, CVect4<float>, CVect4<float>);
PROMOTE2(CVect3<float>, CMatrix3<float>, CMatrix3<float>);
PROMOTE2(CVect3<float>, CMatrix4<float>, CMatrix4<float>);
PROMOTE2(CVect4<float>, CMatrix4<float>, CMatrix4<float>);
PROMOTE2(float, CVect4< CVect3<float> >, CVect4< CVect3<float> >);
PROMOTE2(float, CMatrix4< CVect3<float> >, CMatrix4< CVect3<float> >);

**/

#undef PROMOTE
#undef PROMOTE2

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
