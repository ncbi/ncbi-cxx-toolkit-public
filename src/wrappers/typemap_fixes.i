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
 * Authors:  Josh Cherry
 *
 * File Description:  Some typemaps to override what SWIG provides
 *
 */


// Better Perl char * "in" and "check" typemaps for pre-UTL SWIG

#ifdef SWIGPERL

%typemap(in) char * {
  if (SWIG_ConvertPtr($input, (void **) &$1, $1_descriptor,0) < 0) {
    if (SvPOK($input)) {
      $1 = ($1_ltype) SvPV($input, PL_na);
    } else {
      SWIG_croak("Type error in argument $argnum of $symname. "
                 "Expected $1_mangle");
    }
  }
}

%typecheck(SWIG_TYPECHECK_STRING) char * {
  if (SWIG_ConvertPtr($input, (void **) &$1, $1_descriptor,0) < 0) {
    $1 = SvPOK($input) ? 1 : 0;
  } else {
    $1 = 1;
  }
}

#endif
