# Note: valgrind will detect a memory leak with not closed BDB files at
# SHUTDOWN
# This is a valid case and done on purpose:
# - not closing BDB files reduces the SHUTDOWN time dramatically
# - BDB files are not needed after SHUTDOWN anyway
valgrind --leak-check=full --track-origins=yes  --log-file="valgrind.4.21.0.o2g" /export/home/satskyse/ns_testsuite/netscheduled -nodaemon
