#ifndef BISON_NEWICK_TAB_HPP
# define BISON_NEWICK_TAB_HPP

#ifndef YYSTYPE
typedef union {
    char *strval;
    double dblval;
    TPhyTreeNode *nodeval;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	LABEL	257
# define	NUM	258


extern YYSTYPE newicklval;

#endif /* not BISON_NEWICK_TAB_HPP */
