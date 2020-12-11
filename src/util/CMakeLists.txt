# $Id$

NCBI_add_library(util)
NCBI_add_subdirectory(
    bitset 
    regexp 
    xregexp 
    compress 
    diff 
    image 
    tables 
    sequtil 
    qparse 
    lmdb 
    lmdbxx 
    test 
    demo
)

