BDB ICache class factory parameters:
(INI file configuration)





=========================================================================

timeout   : Timeout in seconds. 
Every BLOB in cache receives a time marker, and expires according to the 
timeout.

Mandatory.
=========================================================================

ttl_prolong : Maximum number of timeouts BLOB removal(expiration) delayed

This parameter regulates BLOB expiration. If client constantly reads
the BLOB and you do not want it to stuck in the database forever
(timestamp=onread), set this parameter.
If timeout is 3600 and ttl_prolong is 2, maximum possible timeout for
the BLOB becomes 3600 * 2 = 7200 seconds.

Individual timeouts can also be regulated with this parameter 
(see max_timeout)

Optional.
=========================================================================


max_timeout   : Maximum possible timeout in seconds. 

Individual timeouts cannot be more than this value.
If value is not set maximum is calculated as timeout *  ttl_prolong

Optional.
=========================================================================

timestamp : Arbitrary combination of:
            [onread] [subkey] [expire_not_used] [purge_on_startup]
            [check_expiration]

[onread] - Update timestamp when BLOB is requested. 
(By deafult timestamp reflects time when it was created)

[subkey] - Update timestamp for key-subkey. (By default key only).

[expire_not_used] - Expire objects older than a certain time frame

[purge_on_startup] - Remove expired BLOBs when cache is first open

[check_expiration] - Expiration timeout is always checked on any access to 
cache element


Mandatory.

=========================================================================

keep_versions : [all]  | [drop_old] | [drop_all]

[all] - When new version arrives, old versions are not deleted

[drop_old] - When a new version arrives, old versions are deleted

[drop_all] - When any version arrives all other versions are deleted


Optional.

=========================================================================


path : path to cache database. 

Mandatory.

=========================================================================

name : database name (default "lcache").

Optional.

=========================================================================

lock : [no_lock] | [pid_lock]

[no_lock]  - No locking agains use by more than one program. (Default.)
[pid_lock] - Cache can be used by only one application

Optional.

=========================================================================

[mem_size] - Size of the Berkeley DB cache. 
(Example: 10MB, 512KB)

Optional.

=========================================================================


read_only : [true] | [false]
Open cache as a read only database

Optional.
=========================================================================


write_sync :  [true] | [false]
Use syncronous or asyncronous transactions. (Asyncromous by default).
Async. transactions are faster but not durable in case of failure.

Optional
=========================================================================

use_transactions : [true] | [false]
Flag to use a transaction protected database. (Default: true)

Optional
=========================================================================


purge_batch_size : Integer
Number of records processed by Purge in one batch. While records are
fetched or deleted cache remains locked and cannot process new requests.

Optional
=========================================================================

purge_batch_sleep : Integer
Delay in milliseconds after batch processing. Introduced to let other
waiting requests go. Larger delay results in slower Purge but more favorable
priority for other cache requests

Optional
=========================================================================

purge_clean_log : Integer
Non zero value results in log cleaning every "purge_clean_log" time.

Optional
=========================================================================

purge_thread : [true] | [false]
Run purge in a background thread. (Default false)
See also purge_batch_sleep, purge_batch_size to adjust thread priority

Optional
=========================================================================

purge_thread_delay : Integer
Delay in seconds between consequtive Purge runs.

Optional
=========================================================================


checkpoint_bytes : Integer
Checkpoint the database at least as often as every bytes of log file 
are written. 

Optional
=========================================================================


overflow_limit : Integer
Watermark value. All BLOBs with size lower than this stored as database
records, higher than that as OS files

Optional
=========================================================================


log_file_max : Integer
Maximum size of transaction log file

Optional
=========================================================================

owner_stat : [true] | [false]
Statistics collection by BLOB owners. OFF by default.
Statistics is accumulated in memory, can be a problem for some applications.



Ini file example:

[bdb]
path=e:/netcached_data
name=nccache
write_sync=false
log_file_max=100M

timeout=3600
timestamp=subkey purge_on_startup check_expiration
keep_versions=drop_all








