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

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include "vector_math.hpp"
#include "cn3d_tools.hpp"

BEGIN_SCOPE(Cn3D)

void SetTranslationMatrix(Matrix* m, const Vector& v, int n)
{
    m->m[0]=1;  m->m[1]=0;  m->m[2]=0;  m->m[3]=n*v.x;
    m->m[4]=0;  m->m[5]=1;  m->m[6]=0;  m->m[7]=n*v.y;
    m->m[8]=0;  m->m[9]=0;  m->m[10]=1; m->m[11]=n*v.z;
    m->m[12]=0; m->m[13]=0; m->m[14]=0; m->m[15]=1;
}

void SetScaleMatrix(Matrix* m, const Vector& v)
{
    m->m[0]=v.x; m->m[1]=0;   m->m[2]=0;    m->m[3]=0;
    m->m[4]=0;   m->m[5]=v.y; m->m[6]=0;    m->m[7]=0;
    m->m[8]=0;   m->m[9]=0;   m->m[10]=v.z; m->m[11]=0;
    m->m[12]=0;  m->m[13]=0;  m->m[14]=0;   m->m[15]=1;
    return;
}

void SetRotationMatrix(Matrix* m, const Vector& v, double rad, int n)
{
    double c, s, t;
    Vector u(v);
    u.normalize();
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

    t.x=m.m[0]*v->x + m.m[1]*v->y + m.m[2]*v->z  + m.m[3];
    t.y=m.m[4]*v->x + m.m[5]*v->y + m.m[6]*v->z  + m.m[7];
    t.z=m.m[8]*v->x + m.m[9]*v->y + m.m[10]*v->z + m.m[11];
    *v = t;
    return;
}

// C = A x B
void ComposeInto(Matrix* C, const Matrix& A, const Matrix& B)
{
    for (int i=0; i<4; ++i) {
        for (int j=0; j<4; ++j) {
            C->m[4*i+j]=0;
            for (int s=0; s<4; ++s) {
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


//---------------------------------------------------------------------
// copied from:  /PROGRAMS/SigmaX2.2/SIGLIB/RigidBody.c
//
//    S*I*G*M*A - 1996 program
// revised 1996        J. Hermans, Univ. N. Carolina
//
//     perform rigid body fit for two structures
//         (cf. Ferro&Hermans and McLachlan)
//
// inputs:
//    natx:          number of atoms
//    xref:          reference coordinates
//    xvar:          variable coordinates
//    Inverse_mass:  inverse weights (1 per atom)
//    do_select:     use the Selection array
//    selected:      selection array
//
// outputs:
//    cgref:     c.o.mass of xref
//    cgvar:     c.o.mass of xvar
//
//  NEW xvar = cgref + rot * ( OLD xvar - cgvar ).
//  The new xvar will be the best approximation to xref.
//---------------------------------------------------------------------

// Adapted for Cn3D++ with Vector, Matrix classes by Paul Thiessen.
// xvar Vectors are left unchanged; instead, the transformations necessary
// to superimpose the variable structure onto the reference structure
// are returned in cgref, cgvar, and rotMat. The transformation process is:
// 1) translate variable so its center of mass is at origin; 2) apply
// rotation in rotMat; 3) translate variable so its center of mass is
// at center of mass of reference.

void RigidBodyFit(
    int natx, const Vector * const *xref, const Vector * const *xvar, const double *weights,
    Vector& cgref, Vector& cgvar, Matrix& rotMat)
{
    Vector  t, phi, cosin, sine;
    double  rot[3][3], corlnmatrx[3][3];
    int     flag1, flag2, flag3;
    int     i, j, iatv, ix;
    double  an2, xx, f, fz, sgn, del, phix, phibes;
    double  tol = .0001;

    // compute centroids
    cgref *= 0;
    cgvar *= 0;
    an2 = 0.;
    for (iatv=0; iatv<natx; ++iatv) {
        if (weights[iatv] <= 0.) continue;
        an2 += weights[iatv];
        cgref += *(xref[iatv]) * weights[iatv];
        cgvar += *(xvar[iatv]) * weights[iatv];
    }
    cgref /= an2;
    cgvar /= an2;

    // compute correlation matrix
    for (i=0; i<3; ++i) {
        cosin[i] = 1.;
        for (j=0; j<3; ++j) {
            corlnmatrx[i][j] = 0.;
        }
    }
    for (iatv=0; iatv<natx; ++iatv) {
        for (i=0; i<3; ++i) {
            if (weights[iatv] <= 0.) continue;
            xx = ((*(xvar[iatv]))[i] - cgvar[i]) * weights[iatv];
            for (j=0; j<3; ++j) {
                corlnmatrx[i][j] += xx * ((*(xref[iatv]))[j] - cgref[j]);
            }
        }
    }

    // evaluate rotation matrix iteratively
    flag1 = flag2 = flag3 = false;
    ix = 0;
    del = .5;
    sgn = 1.;
    phibes = phix = 0.;

    while (true) {
        cosin[ix] = cos(phix);
        sine[ix] = sin(phix);
        rot[0][0] = cosin[1] * cosin[2];
        rot[1][0] = cosin[1] * sine[2];
        rot[0][1] = sine[0] * sine[1] * cosin[2] - cosin[0] * sine[2];
        rot[1][1] = sine[0] * sine[1] * sine[2] + cosin[0] * cosin[2];
        rot[2][0] = -sine[1];
        rot[2][1] = sine[0] * cosin[1];
        rot[2][2] = cosin[0] * cosin[1];
        rot[0][2] = cosin[0] * sine[1] * cosin[2] + sine[0] * sine[2];
        rot[1][2] = cosin[0] * sine[1] * sine[2] - sine[0] * cosin[2];

        // compute the trace of (rot x corlnmatrix)
        f = 0.;
        for (i=0; i<3; ++i) {
            for (j=0; j<3; ++j) {
                f += rot[i][j] * corlnmatrx[i][j];
            }
        }

        if (!flag3) {
            fz = f;
            flag3 = true;
            phix = phibes + sgn * del;
            phi[ix] = phix;
            continue;
        }
        if (f > fz) {
            // f went down, try again with same difference.
            fz = f;
            flag1 = true;
            phibes = phi[ix];
            flag2 = false;
            phix = phibes + sgn * del;
            phi[ix] = phix;
            continue;
        }
        if (!flag1) {
            // try in the opposite direction
            sgn = -sgn;
            flag1 = true;
            phix = phibes + sgn * del;
            phi[ix] = phix;
            continue;
        }
        phi[ix] = phibes;
        cosin[ix] = cos(phibes);
        sine[ix] = sin(phibes);
        flag1 = false;
        if (ix < 2) {
            // apply the same del to the next Euler angle
            ++ix;
            phibes = phi[ix];
            phi[ix] = phix = phibes + sgn * del;
            continue;
        }
        if (flag2) {
            // cut del in half
            flag2 = false;
            del *= .5;
            if (del < tol) break;
        } else {
            flag2 = true;
        }
        ix = 0;
        phibes = phi[ix];
        phix = phibes + sgn * del;
        phi[ix] = phix;
    }
    // end iterative evaluation of rotation matrix

    // store computed rotation matrix in rotMat
    rotMat.SetToIdentity();
    for (i=0; i<3; ++i) {
        for (j=0; j<3; ++j) {
            rotMat[4*i + j] = rot[j][i];
        }
    }
} // end RigidBodyFit()

double ComputeRMSD(int nCoords, const Vector * const *masterCoords,
    const Vector * const *slaveCoords, const Matrix *transformSlaveToMaster)
{
    Vector x;
    double rmsd = 0.0;
    int n = 0;
    for (int c=0; c<nCoords; ++c) {
        if (!slaveCoords[c] || !masterCoords[c]) continue;
        x = *(slaveCoords[c]);
        if (transformSlaveToMaster)
            ApplyTransformation(&x, *transformSlaveToMaster);
        rmsd += (*(masterCoords[c]) - x).lengthSquared();
        ++n;
    }
    if (n == 0) {
        WARNINGMSG("ComputeRMSD() - received no non-NULL coordinates");
        return 0.0;
    }
    rmsd = sqrt(rmsd / n);
    return rmsd;
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2005/03/23 18:39:37  thiessen
* split out RMSD calculator as function
*
* Revision 1.8  2004/05/21 21:41:40  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.7  2004/03/15 18:38:52  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.6  2004/02/19 17:05:22  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.5  2003/02/03 19:20:08  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.4  2001/02/09 20:17:32  thiessen
* ignore atoms w/o alpha when doing structure realignment
*
* Revision 1.3  2000/11/13 18:06:53  thiessen
* working structure re-superpositioning
*
* Revision 1.2  2000/07/17 22:37:18  thiessen
* fix vector_math typo; correctly set initial view
*
* Revision 1.1  2000/06/27 20:09:41  thiessen
* initial checkin
*
*/
