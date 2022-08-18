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
* Authors:  Sergiy Gotvyanskyy
*
* File Description:
*   Unit test for asynchronous multi-source file writer
*
*/

#include <ncbi_pch.hpp>
#include "multi_source_file.hpp"

#include <sstream>
#include <chrono>
#include <thread>
#include <iostream>
#include <future>


void TestMTWriter()
{
    using namespace std::chrono_literals;
    {
        ncbi::objects::CMultiSourceWriter writer;
        writer.Open(std::cerr);
        {
            ncbi::objects::CMultiSourceOStream ostr1;
            auto ostr2 = writer.NewStream();
            auto ostr3 = writer.NewStream();
            ostr1 = std::move(ostr2);

            ostr2 << "Bad hello\n";
            auto task1 = std::async(std::launch::async, [](ncbi::objects::CMultiSourceOStream ostr)
                {
                    std::this_thread::sleep_for(5s);
                    for (size_t i=0; i<1000; ++i) {
                        ostr << "Hello " << i << "\n";
                    }
                    std::this_thread::sleep_for(5s);
                }, std::move(ostr1));

            std::this_thread::sleep_for(1s);

            auto task2 = std::async(std::launch::async, [](ncbi::objects::CMultiSourceOStream ostr)
                {
                    ostr << "Not hello\n";
                    //std::this_thread::sleep_for(5s);
                }, std::move(ostr3));

        }
        writer.Close();
    }
    std::cerr << std::endl;
};
