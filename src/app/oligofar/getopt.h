#ifndef OLIGOFAR__GETOPT__H
#define OLIGOFAR__GETOPT__H

#ifdef __WIN32__

#ifdef __cplusplus 
extern "C" {
#endif

struct option {
	const char * name;
	int has_arg;
	int * flag;
	int val;
};

int getopt( int argc, char ** argv, const char * optlist );
int getopt_long( int argc, char ** argv, const char * optlist, const struct option * longoption, int * longindex );

extern int optind;
extern int optopt;
extern int opterr;
extern const char * optarg;


#ifdef __cplusplus
}
#endif

#endif

#endif
