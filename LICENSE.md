CONTENTS

  Public Domain Notice
  Exceptions (for bundled 3rd-party code)
  Copyright F.A.Q.



==============================================================
                          PUBLIC DOMAIN NOTICE
             National Center for Biotechnology Information

With the exception of certain third-party files summarized below, this
software is a "United States Government Work" under the terms of the
United States Copyright Act.  It was written as part of the authors'
official duties as United States Government employees and thus cannot
be copyrighted.  This software is freely available to the public for
use. The National Library of Medicine and the U.S. Government have not
placed any restriction on its use or reproduction.

Although all reasonable efforts have been taken to ensure the accuracy
and reliability of the software and data, the NLM and the U.S.
Government do not and cannot warrant the performance or results that
may be obtained by using this software or data. The NLM and the U.S.
Government disclaim all warranties, express or implied, including
warranties of performance, merchantability or fitness for any
particular purpose.

Please cite the authors in any work or product based on this material.



==============================================================
EXCEPTIONS (in all cases excluding NCBI-written makefiles):


Location: src/algo/blast/core/ncbi_erf.c, src/util/ncbi_erf.c
Authors:  Sun Microsystems, Inc.
License:  Permissive [at top of file]

Location: include/algo/gnomon/debruijn (excepting DBGraph.hpp)
Authors:  INRIA
License:  GNU Affero GPL [agpl.txt]

Location: src/algo/phy_tree/newick.tab.*, src/db/bdb/bdb_query_bison.tab.c,
          src/util/qparse/query_parser_bison.tab.c
Authors:  Free Software Foundation, Inc.
License:  Unrestricted [at top of file]

Location: much of src/build-system/cmake/unused/FindSqlite3.cmake
Author:   Martin Dobias
License:  BSD [as in HunterGate.cmake]

Location: src/build-system/cmake/unused/HunterGate.cmake
Author:   Ruslan Baratov
License:  BSD [at top of file]

Location: src/build-system/configure
Authors:  Free Software Foundation, Inc.
License:  Unrestricted [at top of file]

Location: src/build-system/config.{guess,sub}
Authors:  FSF
License:  Unrestricted when distributed with the Toolkit;
          standalone, GNU General Public License [gpl.txt]

Location: src/build-system/m4/ax_check_gnu_make.m4
Authors:  John Darrington and Enrico M. Crisostomo
License:  Permissive [at top of file]

Location: src/build-system/m4/ax_jni_include_dir.m4
Author:   Don Anderson
License:  Permissive [at top of file]

Location: src/build-system/m4/ax_prog_cc_for_build.m4
Author:   Paolo Bonzini
License:  Permissive [at top of file]

Location: src/connect/mbedtls
Authors:  ARM Ltd.
License:  Apache 2.0 or GNU GPL 2.0 [src/connect/mbedtls/*-2.0.txt]

Location: src/connect/parson.{c,h}
Author:   Krzysztof Gabis
License:  MIT [at top of files]

Location: include/corelib/hash_impl
Authors:  HP, SGI, Moscow Center for SPARC Technology, and Boris Fomitchev
License:  MITish [as in include/corelib/hash_impl/_hashtable.h]

Location: part of src/corelib/ncbiexec.cpp
Authors:  Daniel Colascione, Microsoft Corporation
License:  Microsoft Limited Public License [ms-lpl.txt]

Location: {include,src}/dbapi/driver/ftds*/freetds
Authors:  See src/dbapi/driver/ftds*/freetds/AUTHORS
License:  GNU Library/Lesser General Public License
          [src/dbapi/driver/ftds*/freetds/COPYING.LIB]

Location: include/dbapi/driver/odbc/unix_odbc
Authors:  Peter Harvey and Nick Gorham
License:  GNU LGPL [src/dbapi/driver/ftds*/freetds/COPYING.LIB]

Location: src/gui/opengl/MTL*
Authors:  Sam Hocevar, Henry Maddocks, and Sean Morrison
License:  MIT [at top of files]

Location: src/gui/opengl/glutbitmap.h
Author:   Mark J. Kilgard
License:  Permissive [at top of file]

Location: {include,src}/gui/utils/muparser
Author:   Ingo Berg
License:  BSDish [include/gui/utils/muparser/muParser.h]

Location: {include,src}/gui/utils/splines
Author:   Enrico Bertolazzi
License:  BSD [at top of files]

Location: include/html/ncbi_menu*.js
Authors:  Netscape Communications Corp.
License:  Permissive [at top of files]

Location: include/misc/jsonwrapp/rapidjson11
Author:   Milo Yip
License:  MIT [include/misc/jsonwrapp/rapidjson/license.txt]

Location: include/misc/jsonwrapp/rapidjson11/msinttypes
Author:   Alexander Chemeris
License:  BSD [include/misc/jsonwrapp/rapidjson11/msinttypes/inttypes.h]

Location: {include,src}/misc/xmlwrapp
Author:   Peter J Jones at al. [src/misc/xmlwrapp/AUTHORS]
License:  BSDish [src/misc/xmlwrapp/LICENSE]

Location: {include,src}/util/bitset
Author:   Anatoliy Kuznetsov
License:  Apache License v2.0 [include/util/bitset/license.txt]

Location: src/util/checksum/cityhash
Author:   Geoff Pike and Jyrki Alakuijala, Google Inc.
License:  BSDish [src/util/checksum/cityhash/COPYING]

Location: src/util/checksum/farmhash
Author:   Geoff Pike, Google Inc.
License:  BSDish [src/util/checksum/farmhash/COPYING]

Location: src/util/checksum/murmurhash
Author:   Austin Appleby
License:  Unrestricted; at top of file

Location: src/util/compress/api/miniz
Authors:  RAD Game Tools and Valve Software, Rich Geldreich and
          Tenacious Software LLC
License:  MIT [src/util/compress/api/miniz/LICENSE]

Location: {include,src}/util/compress/bzip2
Author:   Julian R Seward
License:  BSDish [src/util/compress/bzip2/LICENSE]

Location: {include,src}/util/compress/zlib
Authors:  Jean-loup Gailly and Mark Adler
License:  BSDish [include/util/compress/zlib/zlib.h]

Location: {include,src}/util/compress/zlib_cloudflare
Authors:  Jean-loup Gailly and Mark Adler
License:  BSDish [src/util/compress/zlib_cloudflare/LICENSE]

Location: src/util/diff/diff_match_patch
Authors:  Neil Fraser
License:  Apache License v2.0 [src/util/diff/diff_match_patch/COPYING]

Location: src/util/diff/dtl
Authors:  Tatsuhiko Kubo at al. [src/util/diff/dtl/CONTRIBUTORS]
License:  BSDish [src/util/diff/dtl/COPYING]

Location: {include,src}/util/lmdb
Author:   Howard Chu, Symas Corp. [src/util/lmdb/COPYRIGHT]
License:  BSDish (OpenLDAP Public License) [src/util/lmdb/LICENSE]

Location: {include,src}/util/lmdbxx,
          include/objtools/blast/seqdb_reader/impl/lmdb++.h
Author:   Arto Bendiken [src/util/lmdbxx/AUTHORS]
License:  Unrestricted [src/util/lmdbxx/UNLICENSE]

Location: {include,src}/util/regexp
Author:   Philip Hazel
License:  BSDish [src/util/regexp/LICENCE]

Location: include/util/regexp/ctre/
Authors:  Hana Dusikovaá
Licnse:   Apache 2.0 [include/util/regexp/ctre/LICENSE]


==============================================================
Copyright F.A.Q.


--------------------------------------------------------------
Q. Our product makes use of the NCBI source code, and we did changes
   and additions to that version of the NCBI code to better fit it to
   our needs. Can we copyright the code, and how?

A. You can copyright only the *changes* or the *additions* you made to the
   NCBI source code. You should identify unambiguously those sections of
   the code that were modified, e.g. by commenting any changes you made
   in the code you distribute. Therefore, your license has to make clear
   to users that your product is a combination of code that is public domain
   within the U.S. (but may be subject to copyright by the U.S. in foreign
   countries) and code that has been created or modified by you.


--------------------------------------------------------------
Q. Can we (re)license all or part of the NCBI source code?

A. No, you cannot license or relicense the source code written by NCBI
   since you cannot claim any copyright in the software that was developed
   at NCBI as a 'government work' and consequently is in the public domain
   within the U.S.


--------------------------------------------------------------
Q. What if these copyright guidelines are not clear enough or are not
   applicable to my particular case?

A. Contact us. Send your questions to 'toolbox@ncbi.nlm.nih.gov'.


