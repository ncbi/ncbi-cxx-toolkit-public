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
 * Author:  Greg Boratyn
 *
 * File Description:
 *   Unit tests for the Phylogenetic tree computation API
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>

#include <serial/serial.hpp>    
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include "../guide_tree_render.hpp"

#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(objects);


// Check is node is leaf (by checking if node has label)
static bool s_IsLeaf(const CNode& node);


BOOST_AUTO_TEST_SUITE(guide_tree_render)

BOOST_AUTO_TEST_CASE(TestGetTreeInfo)
{
    CNcbiIfstream istr("data/tree.asn");
    CBioTreeContainer tree;
    istr >> MSerial_AsnText >> tree;

    // find number of leaves
    int num = 0;
    ITERATE (CNodeSet::Tdata, it, tree.GetNodes().Get()) {
        // if root node
        if (!(*it)->IsSetParent()) {
            continue;
        }

        if (s_IsLeaf(**it)) {
            num++;
        }
    }

    CGuideTreeRenderer renderer(tree);

    auto_ptr<CGuideTreeRenderer::STreeInfo> leaves = renderer.GetTreeInfo(0);

    // number of elements reported must the be the same as number of leaves
    BOOST_REQUIRE_EQUAL((int)leaves->seq_ids.size(), num);
    BOOST_REQUIRE_EQUAL(leaves->seq_ids.size(), leaves->accessions.size());
}


BOOST_AUTO_TEST_CASE(TestRenderTreeRect)
{
    CNcbiIfstream istr("data/tree.asn");
    CBioTreeContainer tree;
    istr >> MSerial_AsnText >> tree;

    CGuideTreeRenderer renderer(tree);
    renderer.SetRenderFormat(CGuideTreeRenderer::eRect);

    CTmpFile tmp_file;
    renderer.WriteImage(tmp_file.AsOutputFile(CTmpFile::eIfExists_Throw));

    CFile out_file(tmp_file.GetFileName());

    BOOST_REQUIRE(out_file.Exists() && out_file.GetLength() > 10);
}


BOOST_AUTO_TEST_CASE(TestRenderTreeSlanted)
{
    CNcbiIfstream istr("data/tree.asn");
    CBioTreeContainer tree;
    istr >> MSerial_AsnText >> tree;

    CGuideTreeRenderer renderer(tree);
    renderer.SetRenderFormat(CGuideTreeRenderer::eSlanted);

    CTmpFile tmp_file;
    renderer.WriteImage(tmp_file.AsOutputFile(CTmpFile::eIfExists_Throw));

    CFile out_file(tmp_file.GetFileName());

    BOOST_REQUIRE(out_file.Exists() && out_file.GetLength() > 10);
}


BOOST_AUTO_TEST_CASE(TestRenderTreeRadial)
{
    CNcbiIfstream istr("data/tree.asn");
    CBioTreeContainer tree;
    istr >> MSerial_AsnText >> tree;

    CGuideTreeRenderer renderer(tree);
    renderer.SetRenderFormat(CGuideTreeRenderer::eRadial);

    CTmpFile tmp_file;
    renderer.WriteImage(tmp_file.AsOutputFile(CTmpFile::eIfExists_Throw));

    CFile out_file(tmp_file.GetFileName());

    BOOST_REQUIRE(out_file.Exists() && out_file.GetLength() > 10);
}


BOOST_AUTO_TEST_CASE(TestRenderForce)
{
    CNcbiIfstream istr("data/tree.asn");
    CBioTreeContainer tree;
    istr >> MSerial_AsnText >> tree;

    CGuideTreeRenderer renderer(tree);
    renderer.SetRenderFormat(CGuideTreeRenderer::eForce);

    CTmpFile tmp_file;
    renderer.WriteImage(tmp_file.AsOutputFile(CTmpFile::eIfExists_Throw));

    CFile out_file(tmp_file.GetFileName());

    BOOST_REQUIRE(out_file.Exists() && out_file.GetLength() > 10);
}

BOOST_AUTO_TEST_SUITE_END()


static bool s_IsLeaf(const CNode& node)
{
    ITERATE (CNodeFeatureSet::Tdata, it, node.GetFeatures().Get()) {
        if ((*it)->GetFeatureid() == CGuideTree::eLabelId) {
            return true;
        }
    }
    return false;
}


#endif /* SKIP_DOXYGEN_PROCESSING */

