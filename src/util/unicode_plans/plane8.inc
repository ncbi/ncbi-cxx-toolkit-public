/*  $Id$
 * ==========================================================================
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
 * ==========================================================================
 *
 * Author: Aleksey Vinokurov
 *
 * File Description:
 *    Unicode transformation library
 *    Plan E8
 *
 */

static TUnicodePlan s_Plan_E8h = {
    { " such that", eString },  // old dictionary      U+E800
    { 0, eString },  //                             U+E801
    { 0, eString },  //                             U+E802
    { 0, eString },  //                             U+E803
    { 0, eString },  //                             U+E804
    { 0, eString },  //                             U+E805
    { 0, eString },  //                             U+E806
    { 0, eString },  //                             U+E807
    { 0, eString },  //                             U+E808
    { 0, eString },  //                             U+E809
    { 0, eString },  //                             U+E80A
    { " black lozenge", eString },  // old dictionary  U+E80B
    { 0, eString },  //                             U+E80C
    { 0, eString },  //                             U+E80D
    { 0, eString },  //                             U+E80E
    { 0, eString },  //                             U+E80F
    { 0, eString },  //                             U+E810
    { 0, eString },  //                             U+E811
    { 0, eString },  //                             U+E812
    { 0, eString },  //                             U+E813
    { 0, eString },  //                             U+E814
    { 0, eString },  //                             U+E815
    { 0, eString },  //                             U+E816
    { 0, eString },  //                             U+E817
    { 0, eString },  //                             U+E818
    { 0, eString },  //                             U+E819
    { 0, eString },  //                             U+E81A
    { 0, eString },  //                             U+E81B
    { 0, eString },  //                             U+E81C
    { 0, eString },  //                             U+E81D
    { 0, eString },  //                             U+E81E
    { 0, eString },  //                             U+E81F
    { 0, eString },  //                             U+E820
    { 0, eString },  //                             U+E821
    { 0, eString },  //                             U+E822
    { 0, eString },  //                             U+E823
    { 0, eString },  //                             U+E824
    { 0, eString },  //                             U+E825
    { 0, eString },  //                             U+E826
    { 0, eString },  //                             U+E827
    { 0, eString },  //                             U+E828
    { 0, eString },  //                             U+E829
    { 0, eString },  //                             U+E82A
    { 0, eString },  //                             U+E82B
    { 0, eString },  //                             U+E82C
    { 0, eString },  //                             U+E82D
    { 0, eString },  //                             U+E82E
    { 0, eString },  //                             U+E82F
    { 0, eString },  //                             U+E830
    { 0, eString },  //                             U+E831
    { 0, eString },  //                             U+E832
    { 0, eString },  //                             U+E833
    { 0, eString },  //                             U+E834
    { 0, eString },  //                             U+E835
    { 0, eString },  //                             U+E836
    { 0, eString },  //                             U+E837
    { "circle with vertical bar", eString },  //       U+E838
    { 0, eString },  //                             U+E839
    { 0, eString },  //                             U+E83A
    { 0, eString },  //                             U+E83B
    { 0, eString },  //                             U+E83C
    { 0, eString },  //                             U+E83D
    { 0, eString },  //                             U+E83E
    { 0, eString },  //                             U+E83F
    { 0, eString },  //                             U+E840
    { 0, eString },  //                             U+E841
    { 0, eString },  //                             U+E842
    { 0, eString },  //                             U+E843
    { "\\", eString },  // old dictionary              U+E844
    { 0, eString },  //                             U+E845
    { 0, eString },  //                             U+E846
    { 0, eString },  //                             U+E847
    { 0, eString },  //                             U+E848
    { 0, eString },  //                             U+E849
    { 0, eString },  //                             U+E84A
    { 0, eString },  //                             U+E84B
    { 0, eString },  //                             U+E84C
    { 0, eString },  //                             U+E84D
    { 0, eString },  //                             U+E84E
    { 0, eString },  //                             U+E84F
    { 0, eString },  //                             U+E850
    { 0, eString },  //                             U+E851
    { 0, eString },  //                             U+E852
    { 0, eString },  //                             U+E853
    { 0, eString },  //                             U+E854
    { 0, eString },  //                             U+E855
    { 0, eString },  //                             U+E856
    { 0, eString },  //                             U+E857
    { 0, eString },  //                             U+E858
    { 0, eString },  //                             U+E859
    { 0, eString },  //                             U+E85A
    { 0, eString },  //                             U+E85B
    { 0, eString },  //                             U+E85C
    { 0, eString },  //                             U+E85D
    { 0, eString },  //                             U+E85E
    { 0, eString },  //                             U+E85F
    { 0, eString },  //                             U+E860
    { 0, eString },  //                             U+E861
    { 0, eString },  //                             U+E862
    { 0, eString },  //                             U+E863
    { 0, eString },  //                             U+E864
    { 0, eString },  //                             U+E865
    { 0, eString },  //                             U+E866
    { 0, eString },  //                             U+E867
    { 0, eString },  //                             U+E868
    { 0, eString },  //                             U+E869
    { 0, eString },  //                             U+E86A
    { 0, eString },  //                             U+E86B
    { 0, eString },  //                             U+E86C
    { 0, eString },  //                             U+E86D
    { 0, eString },  //                             U+E86E
    { 0, eString },  //                             U+E86F
    { 0, eString },  //                             U+E870
    { 0, eString },  //                             U+E871
    { 0, eString },  //                             U+E872
    { 0, eString },  //                             U+E873
    { 0, eString },  //                             U+E874
    { 0, eString },  //                             U+E875
    { 0, eString },  //                             U+E876
    { 0, eString },  //                             U+E877
    { 0, eString },  //                             U+E878
    { 0, eString },  //                             U+E879
    { 0, eString },  //                             U+E87A
    { 0, eString },  //                             U+E87B
    { 0, eString },  //                             U+E87C
    { 0, eString },  //                             U+E87D
    { 0, eString },  //                             U+E87E
    { 0, eString },  //                             U+E87F
    { 0, eString },  //                             U+E880
    { 0, eString },  //                             U+E881
    { 0, eString },  //                             U+E882
    { 0, eString },  //                             U+E883
    { 0, eString },  //                             U+E884
    { 0, eString },  //                             U+E885
    { 0, eString },  //                             U+E886
    { 0, eString },  //                             U+E887
    { 0, eString },  //                             U+E888
    { 0, eString },  //                             U+E889
    { 0, eString },  //                             U+E88A
    { 0, eString },  //                             U+E88B
    { 0, eString },  //                             U+E88C
    { 0, eString },  //                             U+E88D
    { 0, eString },  //                             U+E88E
    { 0, eString },  //                             U+E88F
    { 0, eString },  //                             U+E890
    { 0, eString },  //                             U+E891
    { 0, eString },  //                             U+E892
    { 0, eString },  //                             U+E893
    { 0, eString },  //                             U+E894
    { 0, eString },  //                             U+E895
    { 0, eString },  //                             U+E896
    { 0, eString },  //                             U+E897
    { 0, eString },  //                             U+E898
    { 0, eString },  //                             U+E899
    { 0, eString },  //                             U+E89A
    { 0, eString },  //                             U+E89B
    { 0, eString },  //                             U+E89C
    { 0, eString },  //                             U+E89D
    { 0, eString },  //                             U+E89E
    { 0, eString },  //                             U+E89F
    { 0, eString },  //                             U+E8A0
    { 0, eString },  //                             U+E8A1
    { 0, eString },  //                             U+E8A2
    { 0, eString },  //                             U+E8A3
    { 0, eString },  //                             U+E8A4
    { 0, eString },  //                             U+E8A5
    { 0, eString },  //                             U+E8A6
    { 0, eString },  //                             U+E8A7
    { 0, eString },  //                             U+E8A8
    { 0, eString },  //                             U+E8A9
    { 0, eString },  //                             U+E8AA
    { 0, eString },  //                             U+E8AB
    { 0, eString },  //                             U+E8AC
    { 0, eString },  //                             U+E8AD
    { 0, eString },  //                             U+E8AE
    { 0, eString },  //                             U+E8AF
    { 0, eString },  //                             U+E8B0
    { 0, eString },  //                             U+E8B1
    { 0, eString },  //                             U+E8B2
    { 0, eString },  //                             U+E8B3
    { 0, eString },  //                             U+E8B4
    { 0, eString },  //                             U+E8B5
    { 0, eString },  //                             U+E8B6
    { 0, eString },  //                             U+E8B7
    { 0, eString },  //                             U+E8B8
    { 0, eString },  //                             U+E8B9
    { 0, eString },  //                             U+E8BA
    { 0, eString },  //                             U+E8BB
    { 0, eString },  //                             U+E8BC
    { 0, eString },  //                             U+E8BD
    { 0, eString },  //                             U+E8BE
    { 0, eString },  //                             U+E8BF
    { 0, eString },  //                             U+E8C0
    { 0, eString },  //                             U+E8C1
    { 0, eString },  //                             U+E8C2
    { 0, eString },  //                             U+E8C3
    { 0, eString },  //                             U+E8C4
    { 0, eString },  //                             U+E8C5
    { 0, eString },  //                             U+E8C6
    { 0, eString },  //                             U+E8C7
    { 0, eString },  //                             U+E8C8
    { 0, eString },  //                             U+E8C9
    { 0, eString },  //                             U+E8CA
    { 0, eString },  //                             U+E8CB
    { 0, eString },  //                             U+E8CC
    { 0, eString },  //                             U+E8CD
    { 0, eString },  //                             U+E8CE
    { 0, eString },  //                             U+E8CF
    { 0, eString },  //                             U+E8D0
    { 0, eString },  //                             U+E8D1
    { 0, eString },  //                             U+E8D2
    { 0, eString },  //                             U+E8D3
    { 0, eString },  //                             U+E8D4
    { 0, eString },  //                             U+E8D5
    { 0, eString },  //                             U+E8D6
    { 0, eString },  //                             U+E8D7
    { 0, eString },  //                             U+E8D8
    { 0, eString },  //                             U+E8D9
    { 0, eString },  //                             U+E8DA
    { 0, eString },  //                             U+E8DB
    { 0, eString },  //                             U+E8DC
    { 0, eString },  //                             U+E8DD
    { 0, eString },  //                             U+E8DE
    { 0, eString },  //                             U+E8DF
    { 0, eString },  //                             U+E8E0
    { 0, eString },  //                             U+E8E1
    { 0, eString },  //                             U+E8E2
    { 0, eString },  //                             U+E8E3
    { 0, eString },  //                             U+E8E4
    { 0, eString },  //                             U+E8E5
    { 0, eString },  //                             U+E8E6
    { 0, eString },  //                             U+E8E7
    { 0, eString },  //                             U+E8E8
    { 0, eString },  //                             U+E8E9
    { 0, eString },  //                             U+E8EA
    { 0, eString },  //                             U+E8EB
    { 0, eString },  //                             U+E8EC
    { 0, eString },  //                             U+E8ED
    { 0, eString },  //                             U+E8EE
    { 0, eString },  //                             U+E8EF
    { 0, eString },  //                             U+E8F0
    { 0, eString },  //                             U+E8F1
    { 0, eString },  //                             U+E8F2
    { 0, eString },  //                             U+E8F3
    { 0, eString },  //                             U+E8F4
    { 0, eString },  //                             U+E8F5
    { 0, eString },  //                             U+E8F6
    { 0, eString },  //                             U+E8F7
    { 0, eString },  //                             U+E8F8
    { 0, eString },  //                             U+E8F9
    { 0, eString },  //                             U+E8FA
    { 0, eString },  //                             U+E8FB
    { 0, eString },  //                             U+E8FC
    { 0, eString },  //                             U+E8FD
    { 0, eString },  //                             U+E8FE
    { 0, eString },  //                             U+E8FF
};

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:25  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
