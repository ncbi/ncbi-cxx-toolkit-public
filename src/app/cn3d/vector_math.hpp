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
* ===========================================================================
*/

#ifndef CN3D_VECTORMATH__HPP
#define CN3D_VECTORMATH__HPP

#include <math.h>
#include <stddef.h>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbidiag.hpp>

BEGIN_SCOPE(Cn3D)

class Vector {
public:
    double x, y, z;

    Vector(double xi = 0.0, double yi = 0.0, double zi = 0.0)
    {
        x=xi; y=yi; z=zi;
    }
    Vector(const Vector& v)
    {
        x=v.x; y=v.y; z=v.z;
    }
    Vector& operator = (const Vector& v)
    {
		x=v.x; y=v.y; z=v.z;
        return *this;
    }
    void Set(double xs, double ys, double zs)
    {
        x=xs; y=ys; z=zs;
    }
    bool operator == (const Vector& other)
    {
        return (x == other.x && y == other.y && z == other.z);
    }
    bool operator != (const Vector& other)
    {
        return !(*this == other);
    }
    double& operator [] (size_t i)
    {
		static double err = 0.0;
        if (i == 0) return x;
        else if (i == 1) return y;
        else if (i == 2) return z;
        else ERR_POST(ncbi::Error << "Vector operator [] access out of range : " << i);
        return err;
    }
    double operator [] (size_t i) const
    {
        if (i == 0) return x;
        else if (i == 1) return y;
        else if (i == 2) return z;
        else ERR_POST(ncbi::Error << "Vector operator [] access out of range : " << i);
        return 0.0;
    }
    friend Vector operator - (const Vector& a)
    {
        return Vector(-a.x, -a.y, -a.z);
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
    double length(void) const
    {
        return sqrt(x*x + y*y + z*z);
    }
    double lengthSquared(void) const
    {
        return (x*x + y*y + z*z);
    }
    void normalize(void)
    {
        *this /= length();
    }
    friend double vector_dot(const Vector& a, const Vector& b)
    {
        return (a.x*b.x + a.y*b.y + a.z*b.z);
    }
    friend Vector vector_cross(const Vector& a, const Vector& b)
    {
        return Vector(
            a.y*b.z - a.z*b.y,
            a.z*b.x - a.x*b.z,
            a.x*b.y - a.y*b.x
        );
    }
};

inline ncbi::CNcbiOstream& operator << (ncbi::CNcbiOstream& s, const Vector& v)
{
    return s << '<' << v.x << ',' << v.y << ',' << v.z << '>';
}

class Matrix {
public:
    double m[16];
    Matrix(double m0 =1, double m1 =0, double m2 =0, double m3 =0,
           double m4 =0, double m5 =1, double m6 =0, double m7 =0,
           double m8 =0, double m9 =0, double m10 =1, double m11 =0,
           double m12 =0, double m13 =0, double m14 =0, double m15 =1) {
        m[0]=m0;  m[1]=m1;  m[2]=m2; m[3]=m3;
        m[4]=m4;  m[5]=m5;  m[6]=m6;  m[7]=m7;
        m[8]=m8;  m[9]=m9;  m[10]=m10; m[11]=m11;
        m[12]=m12; m[13]=m13; m[14]=m14; m[15]=m15;
    }
    Matrix(const Matrix& o) {
        for (int i=0; i<16; ++i) m[i]=o.m[i];
    }
    void SetToIdentity(void) {
        m[0] = m[5] = m[10] = m[15] = 1;
        m[1] = m[2] = m[3] = m[4] = m[6] = m[7] =
        m[8] = m[9] = m[11] = m[12] = m[13] = m[14] = 0;
    }
    Matrix& operator = (const Matrix& o) {
        for (int i=0; i<16; ++i) m[i]=o.m[i];
        return *this;
    }
    double& operator [] (size_t i)
    {
		static double err = 0.0;
        if (i > 15 || i < 0) {
            ERR_POST(ncbi::Error << "Matrix operator [] access out of range : " << i);
            return err;
        }
        return m[i];
    }
    double operator [] (size_t i) const
    {
        if (i > 15 || i < 0) {
            ERR_POST(ncbi::Error << "Matrix operator [] access out of range : " << i);
            return 0.0;
        }
        return m[i];
    }
};

void SetTranslationMatrix(Matrix* m, const Vector& v, int n =1);
void SetScaleMatrix(Matrix* m, const Vector& v);
void SetRotationMatrix(Matrix* m, const Vector& v, double rad, int n =1);
void ApplyTransformation(Vector* v, const Matrix& m);
void ComposeInto(Matrix* C, const Matrix& A, const Matrix& B);
void InvertInto(Matrix* I, const Matrix& A);

// Rigid body fit algorithm
void RigidBodyFit(
    int natx, const Vector * const *xref, const Vector * const *xvar, const double *weights,  // inputs
    Vector& cgref, Vector& cgvar, Matrix& rotMat);                                            // outputs

// RMSD calculator
double ComputeRMSD(int nCoords, const Vector * const *masterCoords,
    const Vector * const *slaveCoords, const Matrix *transformSlaveToMaster);

END_SCOPE(Cn3D)

#endif // CN3D_VECTORMATH__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.16  2005/03/23 18:39:37  thiessen
* split out RMSD calculator as function
*
* Revision 1.15  2004/03/15 17:17:56  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.14  2003/02/03 19:20:08  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.13  2001/08/15 20:50:31  juran
* Include <stddef.h> for size_t.
* "Alienate" friend operators.
*
* Revision 1.12  2001/05/22 22:35:36  thiessen
* remove SIZE_TYPE
*
* Revision 1.11  2000/11/13 18:05:58  thiessen
* working structure re-superpositioning
*
* Revision 1.10  2000/09/08 20:16:10  thiessen
* working dynamic alignment views
*
* Revision 1.9  2000/08/18 18:57:44  thiessen
* added transparent spheres
*
* Revision 1.8  2000/08/13 02:42:14  thiessen
* added helix and strand objects
*
* Revision 1.7  2000/07/27 13:30:10  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.6  2000/07/18 16:49:44  thiessen
* more friendly rotation center setting
*
* Revision 1.5  2000/07/17 22:36:46  thiessen
* fix vector_math typo; correctly set initial view
*
* Revision 1.4  2000/07/12 23:28:28  thiessen
* now draws basic CPK model
*
* Revision 1.3  2000/06/29 19:18:19  thiessen
* improved atom map
*
* Revision 1.2  2000/06/29 16:46:16  thiessen
* use NCBI streams correctly
*
* Revision 1.1  2000/06/27 20:08:15  thiessen
* initial checkin
*
*/
