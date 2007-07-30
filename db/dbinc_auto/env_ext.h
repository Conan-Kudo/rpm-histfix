/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_env_ext_h_
#define	_env_ext_h_

#if defined(__cplusplus)
extern "C" {
#endif

void __env_alloc_init __P((REGINFO *, size_t));
size_t __env_alloc_overhead __P((void));
size_t __env_alloc_size __P((size_t));
int __env_alloc __P((REGINFO *, size_t, void *));
void __env_alloc_free __P((REGINFO *, void *));
void __env_alloc_print __P((REGINFO *, u_int32_t));
int __env_read_db_config __P((DB_ENV *));
int __config_split __P((char *, char *[]));
int __env_failchk_pp __P((DB_ENV *, u_int32_t));
int __env_thread_init __P((DB_ENV *, int));
int __env_set_state __P((DB_ENV *, DB_THREAD_INFO **, DB_THREAD_STATE));
char *__env_thread_id_string __P((DB_ENV *, pid_t, db_threadid_t, char *));
int __db_file_extend __P((DB_ENV *, DB_FH *, size_t));
int __db_file_multi_write __P((DB_ENV *, const char *));
int __db_file_write __P((DB_ENV *, DB_FH *, u_int32_t, u_int32_t, int));
void __db_env_destroy __P((DB_ENV *));
int  __env_set_alloc __P((DB_ENV *, void *(*)(size_t), void *(*)(void *, size_t), void (*)(void *)));
int __env_get_encrypt_flags __P((DB_ENV *, u_int32_t *));
int __env_set_encrypt __P((DB_ENV *, const char *, u_int32_t));
int  __env_set_flags __P((DB_ENV *, u_int32_t, int));
int  __env_set_data_dir __P((DB_ENV *, const char *));
int  __env_set_intermediate_dir __P((DB_ENV *, int, u_int32_t));
void __env_set_errcall __P((DB_ENV *, void (*)(const DB_ENV *, const char *, const char *)));
void __env_get_errfile __P((DB_ENV *, FILE **));
void __env_set_errfile __P((DB_ENV *, FILE *));
void __env_get_errpfx __P((DB_ENV *, const char **));
void __env_set_errpfx __P((DB_ENV *, const char *));
void __env_set_msgcall __P((DB_ENV *, void (*)(const DB_ENV *, const char *)));
void __env_get_msgfile __P((DB_ENV *, FILE **));
void __env_set_msgfile __P((DB_ENV *, FILE *));
int  __env_set_paniccall __P((DB_ENV *, void (*)(DB_ENV *, int)));
int  __env_set_shm_key __P((DB_ENV *, long));
int  __env_set_tmp_dir __P((DB_ENV *, const char *));
int  __env_set_verbose __P((DB_ENV *, u_int32_t, int));
int __db_mi_env __P((DB_ENV *, const char *));
int __db_mi_open __P((DB_ENV *, const char *, int));
int __db_env_config __P((DB_ENV *, char *, u_int32_t));
int __db_appname __P((DB_ENV *, APPNAME, const char *, u_int32_t, DB_FH **, char **));
int __env_open_pp __P((DB_ENV *, const char *, u_int32_t, int));
int __env_open __P((DB_ENV *, const char *, u_int32_t, int));
int __env_remove __P((DB_ENV *, const char *, u_int32_t));
int __env_config __P((DB_ENV *, const char *, u_int32_t, int));
int __env_close_pp __P((DB_ENV *, u_int32_t));
int __env_close __P((DB_ENV *, int));
int __env_get_open_flags __P((DB_ENV *, u_int32_t *));
int __db_apprec __P((DB_ENV *, DB_LSN *, DB_LSN *, int, u_int32_t));
int    __log_backup __P((DB_ENV *, DB_LOGC *, DB_LSN *, DB_LSN *, u_int32_t));
int __env_openfiles __P((DB_ENV *, DB_LOGC *, void *, DBT *, DB_LSN *, DB_LSN *, double, int));
int __env_init_rec __P((DB_ENV *, u_int32_t));
int __env_attach __P((DB_ENV *, u_int32_t *, int, int));
int __env_turn_on __P((DB_ENV *));
int __env_turn_off __P((DB_ENV *, u_int32_t));
void __env_panic_set __P((DB_ENV *, int));
int __env_ref_increment __P((DB_ENV *));
int __env_ref_decrement __P((DB_ENV *));
int __env_detach __P((DB_ENV *, int));
int __env_remove_env __P((DB_ENV *));
int __env_region_attach __P((DB_ENV *, REGINFO *, size_t));
int __env_region_detach __P((DB_ENV *, REGINFO *, int));
int __envreg_register __P((DB_ENV *, int *));
int __envreg_unregister __P((DB_ENV *, int));
int __envreg_xunlock __P((DB_ENV *));
int __env_stat_print_pp __P((DB_ENV *, u_int32_t));
void __db_print_fh __P((DB_ENV *, const char *, DB_FH *, u_int32_t));
void __db_print_fileid __P((DB_ENV *, u_int8_t *, const char *));
void __db_dl __P((DB_ENV *, const char *, u_long));
void __db_dl_pct __P((DB_ENV *, const char *, u_long, int, const char *));
void __db_dlbytes __P((DB_ENV *, const char *, u_long, u_long, u_long));
void __db_print_reginfo __P((DB_ENV *, REGINFO *, const char *, u_int32_t));
int __db_stat_not_built __P((DB_ENV *));
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_close __P((DB_ENV *));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_add_remote_site __P((DB_ENV *, const char *, u_int, int *, u_int32_t));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_get_ack_policy __P((DB_ENV *, int *));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_set_ack_policy __P((DB_ENV *, int));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_set_local_site __P((DB_ENV *, const char *, u_int, u_int32_t));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_site_list __P((DB_ENV *, u_int *, DB_REPMGR_SITE **));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_start __P((DB_ENV *, int, u_int32_t));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_stat_pp __P((DB_ENV *, DB_REPMGR_STAT **, u_int32_t));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_stat_print_pp __P((DB_ENV *, u_int32_t));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_handle_event __P((DB_ENV *, u_int32_t, void *));
#endif

#if defined(__cplusplus)
}
#endif
#endif /* !_env_ext_h_ */
