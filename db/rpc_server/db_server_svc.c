#include "db_config.h"
#ifdef HAVE_RPC
/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "db_server.h"
#include <stdio.h>
#include <stdlib.h> /* getenv, exit */
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
extern void __dbsrv_timeout();

#ifdef DEBUG
#define	RPC_SVC_FG
#endif

static void
db_serverprog_1(rqstp, transp)
	struct svc_req *rqstp;
	register SVCXPRT *transp;
{
	union {
		__env_cachesize_msg __db_env_cachesize_1_arg;
		__env_close_msg __db_env_close_1_arg;
		__env_create_msg __db_env_create_1_arg;
		__env_flags_msg __db_env_flags_1_arg;
		__env_open_msg __db_env_open_1_arg;
		__env_remove_msg __db_env_remove_1_arg;
		__txn_abort_msg __db_txn_abort_1_arg;
		__txn_begin_msg __db_txn_begin_1_arg;
		__txn_commit_msg __db_txn_commit_1_arg;
		__db_bt_maxkey_msg __db_db_bt_maxkey_1_arg;
		__db_bt_minkey_msg __db_db_bt_minkey_1_arg;
		__db_close_msg __db_db_close_1_arg;
		__db_create_msg __db_db_create_1_arg;
		__db_del_msg __db_db_del_1_arg;
		__db_extentsize_msg __db_db_extentsize_1_arg;
		__db_flags_msg __db_db_flags_1_arg;
		__db_get_msg __db_db_get_1_arg;
		__db_h_ffactor_msg __db_db_h_ffactor_1_arg;
		__db_h_nelem_msg __db_db_h_nelem_1_arg;
		__db_key_range_msg __db_db_key_range_1_arg;
		__db_lorder_msg __db_db_lorder_1_arg;
		__db_open_msg __db_db_open_1_arg;
		__db_pagesize_msg __db_db_pagesize_1_arg;
		__db_put_msg __db_db_put_1_arg;
		__db_re_delim_msg __db_db_re_delim_1_arg;
		__db_re_len_msg __db_db_re_len_1_arg;
		__db_re_pad_msg __db_db_re_pad_1_arg;
		__db_remove_msg __db_db_remove_1_arg;
		__db_rename_msg __db_db_rename_1_arg;
		__db_stat_msg __db_db_stat_1_arg;
		__db_swapped_msg __db_db_swapped_1_arg;
		__db_sync_msg __db_db_sync_1_arg;
		__db_cursor_msg __db_db_cursor_1_arg;
		__db_join_msg __db_db_join_1_arg;
		__dbc_close_msg __db_dbc_close_1_arg;
		__dbc_count_msg __db_dbc_count_1_arg;
		__dbc_del_msg __db_dbc_del_1_arg;
		__dbc_dup_msg __db_dbc_dup_1_arg;
		__dbc_get_msg __db_dbc_get_1_arg;
		__dbc_put_msg __db_dbc_put_1_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply(transp, xdr_void,
			(char *)NULL);
		return;

	case __DB_env_cachesize:
		xdr_argument = xdr___env_cachesize_msg;
		xdr_result = xdr___env_cachesize_reply;
		local = (char *(*)()) __db_env_cachesize_1;
		break;

	case __DB_env_close:
		xdr_argument = xdr___env_close_msg;
		xdr_result = xdr___env_close_reply;
		local = (char *(*)()) __db_env_close_1;
		break;

	case __DB_env_create:
		xdr_argument = xdr___env_create_msg;
		xdr_result = xdr___env_create_reply;
		local = (char *(*)()) __db_env_create_1;
		break;

	case __DB_env_flags:
		xdr_argument = xdr___env_flags_msg;
		xdr_result = xdr___env_flags_reply;
		local = (char *(*)()) __db_env_flags_1;
		break;

	case __DB_env_open:
		xdr_argument = xdr___env_open_msg;
		xdr_result = xdr___env_open_reply;
		local = (char *(*)()) __db_env_open_1;
		break;

	case __DB_env_remove:
		xdr_argument = xdr___env_remove_msg;
		xdr_result = xdr___env_remove_reply;
		local = (char *(*)()) __db_env_remove_1;
		break;

	case __DB_txn_abort:
		xdr_argument = xdr___txn_abort_msg;
		xdr_result = xdr___txn_abort_reply;
		local = (char *(*)()) __db_txn_abort_1;
		break;

	case __DB_txn_begin:
		xdr_argument = xdr___txn_begin_msg;
		xdr_result = xdr___txn_begin_reply;
		local = (char *(*)()) __db_txn_begin_1;
		break;

	case __DB_txn_commit:
		xdr_argument = xdr___txn_commit_msg;
		xdr_result = xdr___txn_commit_reply;
		local = (char *(*)()) __db_txn_commit_1;
		break;

	case __DB_db_bt_maxkey:
		xdr_argument = xdr___db_bt_maxkey_msg;
		xdr_result = xdr___db_bt_maxkey_reply;
		local = (char *(*)()) __db_db_bt_maxkey_1;
		break;

	case __DB_db_bt_minkey:
		xdr_argument = xdr___db_bt_minkey_msg;
		xdr_result = xdr___db_bt_minkey_reply;
		local = (char *(*)()) __db_db_bt_minkey_1;
		break;

	case __DB_db_close:
		xdr_argument = xdr___db_close_msg;
		xdr_result = xdr___db_close_reply;
		local = (char *(*)()) __db_db_close_1;
		break;

	case __DB_db_create:
		xdr_argument = xdr___db_create_msg;
		xdr_result = xdr___db_create_reply;
		local = (char *(*)()) __db_db_create_1;
		break;

	case __DB_db_del:
		xdr_argument = xdr___db_del_msg;
		xdr_result = xdr___db_del_reply;
		local = (char *(*)()) __db_db_del_1;
		break;

	case __DB_db_extentsize:
		xdr_argument = xdr___db_extentsize_msg;
		xdr_result = xdr___db_extentsize_reply;
		local = (char *(*)()) __db_db_extentsize_1;
		break;

	case __DB_db_flags:
		xdr_argument = xdr___db_flags_msg;
		xdr_result = xdr___db_flags_reply;
		local = (char *(*)()) __db_db_flags_1;
		break;

	case __DB_db_get:
		xdr_argument = xdr___db_get_msg;
		xdr_result = xdr___db_get_reply;
		local = (char *(*)()) __db_db_get_1;
		break;

	case __DB_db_h_ffactor:
		xdr_argument = xdr___db_h_ffactor_msg;
		xdr_result = xdr___db_h_ffactor_reply;
		local = (char *(*)()) __db_db_h_ffactor_1;
		break;

	case __DB_db_h_nelem:
		xdr_argument = xdr___db_h_nelem_msg;
		xdr_result = xdr___db_h_nelem_reply;
		local = (char *(*)()) __db_db_h_nelem_1;
		break;

	case __DB_db_key_range:
		xdr_argument = xdr___db_key_range_msg;
		xdr_result = xdr___db_key_range_reply;
		local = (char *(*)()) __db_db_key_range_1;
		break;

	case __DB_db_lorder:
		xdr_argument = xdr___db_lorder_msg;
		xdr_result = xdr___db_lorder_reply;
		local = (char *(*)()) __db_db_lorder_1;
		break;

	case __DB_db_open:
		xdr_argument = xdr___db_open_msg;
		xdr_result = xdr___db_open_reply;
		local = (char *(*)()) __db_db_open_1;
		break;

	case __DB_db_pagesize:
		xdr_argument = xdr___db_pagesize_msg;
		xdr_result = xdr___db_pagesize_reply;
		local = (char *(*)()) __db_db_pagesize_1;
		break;

	case __DB_db_put:
		xdr_argument = xdr___db_put_msg;
		xdr_result = xdr___db_put_reply;
		local = (char *(*)()) __db_db_put_1;
		break;

	case __DB_db_re_delim:
		xdr_argument = xdr___db_re_delim_msg;
		xdr_result = xdr___db_re_delim_reply;
		local = (char *(*)()) __db_db_re_delim_1;
		break;

	case __DB_db_re_len:
		xdr_argument = xdr___db_re_len_msg;
		xdr_result = xdr___db_re_len_reply;
		local = (char *(*)()) __db_db_re_len_1;
		break;

	case __DB_db_re_pad:
		xdr_argument = xdr___db_re_pad_msg;
		xdr_result = xdr___db_re_pad_reply;
		local = (char *(*)()) __db_db_re_pad_1;
		break;

	case __DB_db_remove:
		xdr_argument = xdr___db_remove_msg;
		xdr_result = xdr___db_remove_reply;
		local = (char *(*)()) __db_db_remove_1;
		break;

	case __DB_db_rename:
		xdr_argument = xdr___db_rename_msg;
		xdr_result = xdr___db_rename_reply;
		local = (char *(*)()) __db_db_rename_1;
		break;

	case __DB_db_stat:
		xdr_argument = xdr___db_stat_msg;
		xdr_result = xdr___db_stat_reply;
		local = (char *(*)()) __db_db_stat_1;
		break;

	case __DB_db_swapped:
		xdr_argument = xdr___db_swapped_msg;
		xdr_result = xdr___db_swapped_reply;
		local = (char *(*)()) __db_db_swapped_1;
		break;

	case __DB_db_sync:
		xdr_argument = xdr___db_sync_msg;
		xdr_result = xdr___db_sync_reply;
		local = (char *(*)()) __db_db_sync_1;
		break;

	case __DB_db_cursor:
		xdr_argument = xdr___db_cursor_msg;
		xdr_result = xdr___db_cursor_reply;
		local = (char *(*)()) __db_db_cursor_1;
		break;

	case __DB_db_join:
		xdr_argument = xdr___db_join_msg;
		xdr_result = xdr___db_join_reply;
		local = (char *(*)()) __db_db_join_1;
		break;

	case __DB_dbc_close:
		xdr_argument = xdr___dbc_close_msg;
		xdr_result = xdr___dbc_close_reply;
		local = (char *(*)()) __db_dbc_close_1;
		break;

	case __DB_dbc_count:
		xdr_argument = xdr___dbc_count_msg;
		xdr_result = xdr___dbc_count_reply;
		local = (char *(*)()) __db_dbc_count_1;
		break;

	case __DB_dbc_del:
		xdr_argument = xdr___dbc_del_msg;
		xdr_result = xdr___dbc_del_reply;
		local = (char *(*)()) __db_dbc_del_1;
		break;

	case __DB_dbc_dup:
		xdr_argument = xdr___dbc_dup_msg;
		xdr_result = xdr___dbc_dup_reply;
		local = (char *(*)()) __db_dbc_dup_1;
		break;

	case __DB_dbc_get:
		xdr_argument = xdr___dbc_get_msg;
		xdr_result = xdr___dbc_get_reply;
		local = (char *(*)()) __db_dbc_get_1;
		break;

	case __DB_dbc_put:
		xdr_argument = xdr___dbc_put_msg;
		xdr_result = xdr___dbc_put_reply;
		local = (char *(*)()) __db_dbc_put_1;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	(void) memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, &argument)) {
		fprintf(stderr, "unable to free arguments");
		exit(1);
	}
	__dbsrv_timeout(0);
	return;
}

void __dbsrv_main()
{
	register SVCXPRT *transp;

	(void) pmap_unset(DB_SERVERPROG, DB_SERVERVERS);

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		fprintf(stderr, "cannot create tcp service.");
		exit(1);
	}
	if (!svc_register(transp, DB_SERVERPROG, DB_SERVERVERS, db_serverprog_1, IPPROTO_TCP)) {
		fprintf(stderr, "unable to register (DB_SERVERPROG, DB_SERVERVERS, tcp).");
		exit(1);
	}

	svc_run();
	fprintf(stderr, "svc_run returned");
	exit(1);
	/* NOTREACHED */
}
#endif /* HAVE_RPC */
