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
* Authors:  Paul Thiessen
*
* File Description:
*      Vector and Matrix classes to simplify 3-d geometry calculations
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2000/06/29 19:18:19  thiessen
* improved atom map
*
* Revision 1.2  2000/06/29 16:46:16  thiessen
* use NCBI streams correctly
*
* Revision 1.1  2000/06/27 20:08:15  thiessen
* initial checkin
*
* ===========================================================================
*/

#ifndef CN3D_VECTORMATH__HPP
#define CN3D_VECTORMATH__HPP

#include <math.h>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbistl.hpp>

USING_NCBI_SCOPE;

BEGIN_SCOPE(Cn3D)

class Vector {
public:
    double x, y, z;

    Vector(double xi = 0, double yi = 0, double zi = 0)
    {
        x=xi; y=yi; z=zi;
    }
    friend Vector operator + (const Vector& a, const Vector& b)
    {
        return Vector(a.x+b.x, a.y+b.y, a.z+b.z);
    }
    Vector& operator += (const Vector& v)
    {
        x+=v.x; y+=v.y; z+=v.z;
        return *this;
    }
    friend Vector operator - (const Vector& a, const Vector& b)
    {
        return Vector(a.x-b.x, a.y-b.y, a.z-b.z);
    }
    Vector& operator -= (const Vector& v)
    {
        x-=v.x; y-=v.y; z-=v.z;
        return *this;
    }
    friend Vector operator * (const Vector& v, double f)
    {
        return Vector(v.x*f, v.y*f, v.z*f);
    }
    friend Vector operator * (double f, const Vector& v)
    {
        return Vector(v.x*f, v.y*f, v.z*f);
    }
    Vector& operator *= (double f)
    {
        x*=f; y*=f; z*=f;
        return *this;
    }
    friend Vector operator / (const Vector& v, double f)
    {
        return Vector(v.x/f, v.y/f, v.z/f);
    }
    Vector& operator /= (double f)
    {
        x/=f; y/=f; z/=f;
        return *this;
    }
    friend double length(const Vector& v)
    {
        return sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    }
    friend double dot(const Vector& a, const Vector& b)
    {
        return double (a.x*b.x + a.y*b.y + a.z*b.z);
    }
    friend Vector cross(const Vector& a, const Vector& b)
    {
        return Vector(
            a.y*b.z - a.z*b.y,
            a.z*b.x - a.x*b.z,
            a.x*b.y - a.y*b.x
        );
    }
};

inline CNcbiOstream& operator << (CNcbiOstream& s, const Vector& v)
{
    return s << '<' << v.x << ',' << v.y << ',' << v.z << '>';
}

class Matrix {
public:
    double m[16];
    Matrix& operator = (const Matrix& o) {
        for (int i=0; i<16; i++) m[i]=o.m[i];
        return *this;
    }
    Matrix(double m0 =1, double m1 =0, double m2 =0, double m3 =0,
           double m4 =0, double m5 =1, double m6 =0, double m7 =0,
           double m8 =0, double m9 =0, double m10 =1, double m11 =0,
           double m12 =0, double m13 =0, double m14 =0, double m15 =1) {
        m[0]=m0;  m[1]=m1;  m[2]=m2; m[3]=m3;
        m[4]=m4;  m[5]=m5;  m[6]=m6;  m[7]=m7;
        m[8]=m8;  m[9]=m9;  m[10]=m10; m[11]=m11;
        m[12]=m12; m[13]=m13; m[14]=m14; m[15]=m15;
    }
};

void SetTranslationMatrix(Matrix* m, const Vector& v, int n =1);
void SetScaleMatrix(Matrix* m, const Vector& v);
void SetRotationMatrix(Matrix* m, const Vector& v, double rad, int n =1);
void ApplyTransformation(Vector* v, const Matrix& m);
void ComposeInto(Matrix* C, const Matrix& A, const Matrix& B);
void InvertInto(Matrix* I, const Matrix& A);

END_SCOPE(Cn3D)

#endif // CN3D_VECTORMATH__HPP
