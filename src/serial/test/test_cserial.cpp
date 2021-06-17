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
 * Author:  Eugene Vasilchenko
 *
 * File Description:
 *   .......
 *
 */

#include <ncbi_pch.hpp>
#include "test_serial.hpp"

#ifdef HAVE_NCBI_C

/////////////////////////////////////////////////////////////////////////////
// Test ASN serialization

BOOST_AUTO_TEST_CASE(s_TestCAsnSerialization)
{
    string text_in("webenv.ent"), text_out("webenv.ento");
    string  bin_in("webenv.bin"),  bin_out("webenv.bino");
    {
        WebEnv* env = 0;
        {
            // read ASN text
            // specify input as a file name, use Read method
            unique_ptr<CObjectIStream> in(
                CObjectIStream::Open(text_in, eSerial_AsnText));
            in->Read(&env, GetSequenceTypeRef(&env).Get());
        }
        {
            // write ASN binary
            // specify output as a file name, use Write method
            unique_ptr<CObjectOStream> out(
                CObjectOStream::Open(bin_out,eSerial_AsnBinary));
            out->Write(&env, GetSequenceTypeRef(&env).Get());
        }
        WebEnvFree(env);
        BOOST_CHECK( CFile( bin_in).Compare( bin_out) );
        {
            // C-style Object must be clean before loading: using new WebEnv instance
            WebEnv* env2 = 0;
            // read ASN binary
            unique_ptr<CObjectIStream> in(
                CObjectIStream::Open(bin_in,eSerial_AsnBinary));
            in->Read(&env2, GetSequenceTypeRef(&env2).Get());

            // write ASN text
            unique_ptr<CObjectOStream> out(
                CObjectOStream::Open(text_out,eSerial_AsnText));
            out->Write(&env2, GetSequenceTypeRef(&env2).Get());
            WebEnvFree(env2);
        }
        BOOST_CHECK( CFile(text_in).CompareTextContents(text_out, CFile::eIgnoreEol) );
    }
}

/////////////////////////////////////////////////////////////////////////////
// TestPrintAsn

BOOST_AUTO_TEST_CASE(s_TestCPrintAsn)
{
    string text_in("webenv.ent"), text_out("webenv.ento");
    {
        WebEnv* env = 0;
        {
            // read ASN text
            unique_ptr<CObjectIStream> in(
                CObjectIStream::Open(text_in, eSerial_AsnText));
            in->Read(&env, GetSequenceTypeRef(&env).Get());
        }
        {
            CNcbiOfstream out(text_out.c_str());
            PrintAsn(out, CConstObjectInfo(&env,GetSequenceTypeRef(&env).Get()));
        }
        WebEnvFree(env);
        BOOST_CHECK( CFile(text_in).CompareTextContents(text_out, CFile::eIgnoreEol) );
    }
}

#endif
