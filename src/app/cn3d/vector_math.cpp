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
* Revision 1.1  2000/06/27 20:09:41  thiessen
* initial checkin
*
* ===========================================================================
*/

#include "cn3d/vector_math.hpp"

BEGIN_SCOPE(Cn3D)

void SetTranslationMatrix(Matrix* m, const Vector& v, int n)
{
    m->m[0]=1; m->m[1]=0; m->m[2]=0; m->m[3]=n*v.x;
    m->m[4]=0; m->m[5]=1; m->m[6]=0; m->m[7]=n*v.y;
    m->m[8]=0; m->m[9]=0; m->m[10]=1; m->m[11]=n*v.z;
    m->m[12]=0; m->m[13]=0; m->m[14]=0; m->m[15]=1;
}

void SetScaleMatrix(Matrix* m, const Vector& v)
{
    m->m[0]=v.x; m->m[1]=0; m->m[2]=0; m->m[3]=0;
    m->m[4]=0; m->m[5]=v.y; m->m[6]=0; m->m[7]=0;
    m->m[8]=0; m->m[9]=0; m->m[10]=v.z; m->m[11]=0;
    m->m[12]=0; m->m[13]=0; m->m[14]=0; m->m[15]=1;
    return;
}

void SetRotationMatrix(Matrix* m, const Vector& v, double rad, int n)
{
    Vector u;
    double length, c, s, t;
    length=sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    u.x=v.x/length;
    u.y=v.y/length;
    u.z=v.z/length;
    c=cos(n*rad);
    s=sin(n*rad);
    t=1.0-c;
  
    m->m[0]=u.x*u.x*t+c; 
    m->m[1]=u.x*u.y*t-u.z*s; 
    m->m[2]=u.x*u.z*t+u.y*s;
    m->m[3]=0;
  
    m->m[4]=u.x*u.y*t+u.z*s; 
    m->m[5]=u.y*u.y*t+c; 
    m->m[6]=u.y*u.z*t-u.x*s;
    m->m[7]=0;
  
    m->m[8]=u.x*u.z*t-u.y*s; 
    m->m[9]=u.y*u.z*t+u.x*s; 
    m->m[10]=u.z*u.z*t+c;
    m->m[11]=0;
  
    m->m[12]=m->m[13]=m->m[14]=0; m->m[15]=1;

    return;
}

void ApplyTransformation(Vector* v, const Matrix& m)
{
    Vector t;
  
    t.x=m.m[0]*v->z + m.m[1]*v->y + m.m[2]*v->z + m.m[3];
    t.y=m.m[4]*v->z + m.m[5]*v->y + m.m[6]*v->z + m.m[7];
    t.z=m.m[8]*v->z + m.m[9]*v->y + m.m[10]*v->z + m.m[11];
    v->z=t.x;
    v->y=t.y;
    v->z=t.z;
    return;
}

// C = A x B
void ComposeInto(Matrix* C, const Matrix& A, const Matrix& B)
{
    for (int i=0; i<4; i++) {
        for (int j=0; j<4; j++) {
            C->m[4*i+j]=0;
            for (int s=0; s<4; s++) {
                C->m[4*i+j] += (A.m[4*i+s])*(B.m[4*s+j]);
            }
        }
    }
    return;
}

//
// The following inverts a 4x4 matrix whose bottom row is { 0 0 0 1 } - i.e.
// a matrix composed only of rotate, scale, and translate matrixes.
//
void InvertInto(Matrix* I, const Matrix& A)
{
    const double&
        a=A.m[0], b=A.m[1], c=A.m[2], d=A.m[3],
        e=A.m[4], f=A.m[5], g=A.m[6], h=A.m[7],
        i=A.m[8], j=A.m[9], k=A.m[10], l=A.m[11];

    double denom=(-(c*f*i) + b*g*i + c*e*j - a*g*j - b*e*k + a*f*k);

    I->m[0]= (-(g*j) + f*k)/denom;
    I->m[1]= (c*j - b*k)/denom;
    I->m[2]= (-(c*f) + b*g)/denom;
    I->m[3]= (d*g*j - c*h*j - d*f*k + b*h*k + c*f*l - b*g*l)/denom;
    I->m[4]= (g*i - e*k)/denom;
    I->m[5]= (-(c*i) + a*k)/denom;
    I->m[6]= (c*e - a*g)/denom;
    I->m[7]= (-(d*g*i) + c*h*i + d*e*k - a*h*k - c*e*l + a*g*l)/denom;
    I->m[8]= (-(f*i) + e*j)/denom;
    I->m[9]= (b*i - a*j)/denom;
    I->m[10]= (-(b*e) + a*f)/denom;
    I->m[11]= (d*f*i - b*h*i - d*e*j + a*h*j + b*e*l - a*f*l)/denom;

    I->m[12]=0; I->m[13]=0; I->m[14]=0; I->m[15]=1;

    return;
}

END_SCOPE(Cn3D)
