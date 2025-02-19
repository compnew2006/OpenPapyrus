/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef _db_ext_h_
#define _db_ext_h_

#if defined(__cplusplus)
extern "C" {
#endif

int __crdel_init_recover(ENV*, DB_DISTAB *);
int __crdel_metasub_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __crdel_inmem_create_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __crdel_inmem_rename_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __crdel_inmem_remove_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __crdel_init_print(ENV*, DB_DISTAB *);
int __crdel_metasub_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __crdel_inmem_create_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __crdel_inmem_rename_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __crdel_inmem_remove_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_master_open(DB*, DB_THREAD_INFO*, DB_TXN*, const char *, uint32, int, DB**);
int __db_master_update(DB*, DB*, DB_THREAD_INFO*, DB_TXN*, const char *, DBTYPE, mu_action, const char *, uint32);
int __env_dbreg_setup(DB*, DB_TXN*, const char *, const char *, uint32);
int __env_setup(DB*, DB_TXN*, const char *, const char *, uint32, uint32);
int __env_mpool(DB*, const char *, uint32);
int __db_close(DB*, DB_TXN*, uint32);
int __db_refresh(DB*, DB_TXN*, uint32, int *, int);
int __db_log_page(DB*, DB_TXN*, DB_LSN*, db_pgno_t, PAGE *);
int __db_walk_cursors(DB*, DBC*, int (*)__P((DBC*, DBC*, uint32*, db_pgno_t, uint32, void *)), uint32*, db_pgno_t, uint32, void *);
int __db_backup_name(ENV*, const char *, const DB_TXN*, char **);
#ifdef CONFIG_TEST
int __db_testcopy(ENV*, DB*, const char *);
#endif
int __db_testdocopy(ENV*, const char *);
int __db_cursor_int(DB*, DB_THREAD_INFO*, DB_TXN*, DBTYPE, db_pgno_t, int, DB_LOCKER*, DBC**);
int __db_put(DB*, DB_THREAD_INFO*, DB_TXN*, DBT*, DBT*, uint32);
int __db_del(DB*, DB_THREAD_INFO*, DB_TXN*, DBT*, uint32);
int __db_sync(DB *);
int __db_associate(DB*, DB_THREAD_INFO*, DB_TXN*, DB*, int (*)(DB *, const DBT *, const DBT *, DBT *), uint32);
int __db_secondary_close(DB*, uint32);
int __db_associate_foreign(DB*, DB*, int (*)(DB *, const DBT *, DBT *, const DBT *, int *), uint32);
int __db_init_recover(ENV*, DB_DISTAB *);
int __db_addrem_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_addrem_42_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_big_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_big_42_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_ovref_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_relink_42_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_debug_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_noop_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_alloc_42_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_alloc_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_free_42_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_free_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_cksum_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_freedata_42_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_freedata_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_init_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_sort_44_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_trunc_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_realloc_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_relink_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_merge_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pgno_print(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_init_print(ENV*, DB_DISTAB *);
int FASTCALL __dbc_close(DBC *);
int __dbc_destroy(DBC *);
int __dbc_cmp(DBC*, DBC*, int *);
int __dbc_count(DBC*, db_recno_t *);
int __dbc_del (DBC*, uint32);
int __dbc_idel (DBC*, uint32);
#ifdef HAVE_COMPRESSION
int __dbc_bulk_del(DBC*, DBT*, uint32);
#endif
int __dbc_dup(DBC*, DBC**, uint32);
int __dbc_idup(DBC*, DBC**, uint32);
int __dbc_newopd(DBC*, db_pgno_t, DBC*, DBC**);
int __dbc_get(DBC*, DBT*, DBT*, uint32);
int FASTCALL __dbc_iget(DBC*, DBT*, DBT*, uint32);
int __dbc_put(DBC*, DBT*, DBT*, uint32);
int __dbc_iput(DBC*, DBT*, DBT*, uint32);
int __db_duperr(const DB *, uint32);
int __dbc_cleanup(DBC*, DBC*, int);
int __dbc_secondary_get_pp(DBC*, DBT*, DBT*, uint32);
int __dbc_pget(DBC*, DBT*, DBT*, DBT*, uint32);
int __dbc_del_primary(DBC *);
int FASTCALL __db_s_first(DB *, DB **);
int FASTCALL __db_s_next(DB **, DB_TXN *);
int __db_s_done(DB*, DB_TXN *);
int __db_buildpartial(DB*, DBT*, DBT*, DBT *);
uint32 __db_partsize(uint32, DBT *);
#ifdef DIAGNOSTIC
void __db_check_skeyset(DB*, DBT *);
#endif
int __cdsgroup_begin(ENV*, DB_TXN**);
int __cdsgroup_begin_pp(DB_ENV*, DB_TXN**);
int __db_compact_int(DB*, DB_THREAD_INFO*, DB_TXN*, DBT*, DBT*, DB_COMPACT*, uint32, DBT *);
int __db_exchange_page(DBC*, PAGE**, PAGE*, db_pgno_t, int);
int __db_truncate_overflow(DBC*, db_pgno_t, PAGE**, DB_COMPACT *);
int __db_truncate_root(DBC*, PAGE*, uint32, db_pgno_t*, uint32);
int __db_find_free(DBC*, uint32, uint32, db_pgno_t, db_pgno_t *);
int __db_relink(DBC*, PAGE*, PAGE*, db_pgno_t);
int __db_move_metadata(DBC*, DBMETA**, DB_COMPACT *);
int __db_pgin(DB_ENV*, db_pgno_t, void *, DBT *);
int __db_pgout(DB_ENV*, db_pgno_t, void *, DBT *);
int __db_decrypt_pg(ENV*, const DB *, PAGE *);
int __db_encrypt_and_checksum_pg(ENV *, const DB *, PAGE *);
void __db_metaswap(PAGE *);
int __db_byteswap(DB*, db_pgno_t, PAGE*, size_t, int);
int __db_pageswap(ENV*, DB*, void *, size_t, DBT*, int);
void __db_recordswap(uint32, uint32, void *, void *, uint32);
int __db_dispatch(ENV*, DB_DISTAB*, DBT*, DB_LSN*, db_recops, void *);
int __db_add_recovery(DB_ENV*, DB_DISTAB*, int (*)(DB_ENV *, DBT *, DB_LSN *, db_recops), uint32);
int __db_add_recovery_int(ENV*, DB_DISTAB*, int (*)(ENV *, DBT *, DB_LSN *, db_recops, void *), uint32);
int __db_txnlist_init(ENV*, DB_THREAD_INFO*, uint32, uint32, DB_LSN*, DB_TXNHEAD**);
int __db_txnlist_add(ENV*, DB_TXNHEAD*, uint32, uint32, DB_LSN *);
int __db_txnlist_remove(ENV*, DB_TXNHEAD*, uint32);
void __db_txnlist_ckp(ENV*, DB_TXNHEAD*, DB_LSN *);
void __db_txnlist_end(ENV*, DB_TXNHEAD *);
int __db_txnlist_find(ENV*, DB_TXNHEAD*, uint32, uint32 *);
int __db_txnlist_update(ENV*, DB_TXNHEAD*, uint32, uint32, DB_LSN*, uint32*, int);
int __db_txnlist_gen(ENV*, DB_TXNHEAD*, int, uint32, uint32);
int __db_txnlist_lsnadd(ENV*, DB_TXNHEAD*, DB_LSN *);
int __db_txnlist_lsnget(ENV*, DB_TXNHEAD*, DB_LSN*, uint32);
int __db_txnlist_lsninit(ENV*, DB_TXNHEAD*, DB_LSN *);
void __db_txnlist_print(DB_TXNHEAD *);
int __db_ditem_nolog(DBC*, PAGE*, uint32, uint32);
int __db_ditem(DBC*, PAGE*, uint32, uint32);
int __db_pitem_nolog(DBC*, PAGE*, uint32, uint32, DBT*, DBT *);
int __db_pitem(DBC*, PAGE*, uint32, uint32, DBT*, DBT *);
int __db_associate_pp(DB*, DB_TXN*, DB*, int (*)(DB *, const DBT *, const DBT *, DBT *), uint32);
int __db_close_pp(DB*, uint32);
int __db_cursor_pp(DB*, DB_TXN*, DBC**, uint32);
int FASTCALL __db_cursor(DB*, DB_THREAD_INFO*, DB_TXN*, DBC**, uint32);
int __db_del_pp(DB*, DB_TXN*, DBT*, uint32);
int __db_exists(DB*, DB_TXN*, DBT*, uint32);
int __db_fd_pp(DB*, int *);
int __db_get_pp(DB*, DB_TXN*, DBT*, DBT*, uint32);
int __db_get(DB*, DB_THREAD_INFO*, DB_TXN*, DBT*, DBT*, uint32);
int __db_join_pp(DB*, DBC**, DBC**, uint32);
int __db_key_range_pp(DB*, DB_TXN*, DBT*, DB_KEY_RANGE*, uint32);
int __db_open_pp(DB*, DB_TXN*, const char *, const char *, DBTYPE, uint32, int);
int __db_pget_pp(DB*, DB_TXN*, DBT*, DBT*, DBT*, uint32);
int __db_pget(DB*, DB_THREAD_INFO*, DB_TXN*, DBT*, DBT*, DBT*, uint32);
int __db_put_pp(DB*, DB_TXN*, DBT*, DBT*, uint32);
int __db_compact_pp(DB*, DB_TXN*, DBT*, DBT*, DB_COMPACT*, uint32, DBT *);
int __db_associate_foreign_pp(DB*, DB*, int (*)(DB *, const DBT *, DBT *, const DBT *, int *), uint32);
int __db_sync_pp(DB*, uint32);
int __dbc_close_pp(DBC *);
int __dbc_cmp_pp(DBC*, DBC*, int *, uint32);
int __dbc_count_pp(DBC*, db_recno_t*, uint32);
int __dbc_del_pp (DBC*, uint32);
int __dbc_dup_pp(DBC*, DBC**, uint32);
int __dbc_get_pp(DBC*, DBT*, DBT*, uint32);
int __dbc_get_arg(DBC*, DBT*, DBT*, uint32);
int __db_secondary_close_pp(DB*, uint32);
int __dbc_pget_pp(DBC*, DBT*, DBT*, DBT*, uint32);
int __dbc_put_pp(DBC*, DBT*, DBT*, uint32);
int __db_txn_auto_init(ENV*, DB_THREAD_INFO*, DB_TXN**);
int __db_txn_auto_resolve(ENV*, DB_TXN*, int, int);
int __db_join(DB*, DBC**, DBC**, uint32);
int __db_join_close(DBC *);
int __db_secondary_corrupt(DB *);
int __db_new(DBC*, uint32, DB_LOCK*, PAGE**);
int __db_free(DBC*, PAGE*, uint32);
#ifdef HAVE_FTRUNCATE
void __db_freelist_pos(db_pgno_t, db_pgno_t*, uint32, uint32 *);
#endif
void __db_freelist_sort(db_pglist_t*, uint32);
#ifdef HAVE_FTRUNCATE
int __db_pg_truncate(DBC*, DB_TXN*, db_pglist_t*, DB_COMPACT*, uint32*, db_pgno_t, db_pgno_t*, DB_LSN*, int);
#endif
#ifdef HAVE_FTRUNCATE
int __db_free_truncate(DB*, DB_THREAD_INFO*, DB_TXN*, uint32, DB_COMPACT*, db_pglist_t**, uint32*, db_pgno_t *);
#endif
int __db_lprint(DBC *);
int __db_lget(DBC*, int, db_pgno_t, db_lockmode_t, uint32, DB_LOCK *);
#ifdef DIAGNOSTIC
int __db_haslock(ENV*, DB_LOCKER*, DB_MPOOLFILE*, db_pgno_t, db_lockmode_t, uint32);
#endif
#ifdef DIAGNOSTIC
int __db_has_pagelock(ENV*, DB_LOCKER*, DB_MPOOLFILE*, PAGE*, db_lockmode_t);
#endif
int FASTCALL __db_lput(DBC *, DB_LOCK *);
int __db_create_internal(DB **, ENV *, uint32);
int FASTCALL __dbh_am_chk(DB *, uint32);
int __db_get_flags(DB*, uint32 *);
int __db_set_flags(DB*, uint32);
int __db_get_lorder(DB*, int *);
int __db_set_lorder(DB*, int);
int __db_set_pagesize(DB*, uint32);
int __db_open(DB*, DB_THREAD_INFO*, DB_TXN*, const char *, const char *, DBTYPE, uint32, int, db_pgno_t);
int __db_get_open_flags(DB*, uint32 *);
int __db_new_file(DB*, DB_THREAD_INFO*, DB_TXN*, DB_FH*, const char *);
int __db_init_subdb(DB*, DB*, const char *, DB_THREAD_INFO*, DB_TXN *);
int __db_chk_meta(ENV*, DB*, DBMETA*, uint32);
int __db_meta_setup(ENV*, DB*, const char *, DBMETA*, uint32, uint32);
int __db_reopen(DBC *);
int __db_goff(DBC*, DBT*, uint32, db_pgno_t, void **, uint32 *);
int __db_poff(DBC*, const DBT*, db_pgno_t *);
int __db_ovref(DBC*, db_pgno_t);
int __db_doff(DBC*, db_pgno_t);
int __db_moff(DBC*, const DBT*, db_pgno_t, uint32, int (*)(DB *, const DBT *, const DBT *), int *);
int __db_coff(DBC*, const DBT*, const DBT*, int (*)(DB *, const DBT *, const DBT *), int *);
int __db_vrfy_overflow(DB*, VRFY_DBINFO*, PAGE*, db_pgno_t, uint32);
int __db_vrfy_ovfl_structure(DB*, VRFY_DBINFO*, db_pgno_t, uint32, uint32);
int __db_safe_goff(DB*, VRFY_DBINFO*, db_pgno_t, DBT*, void *, uint32*, uint32);
void __db_loadme();
int __db_dumptree(DB*, DB_TXN*, char *, char *, db_pgno_t, db_pgno_t);
const FN * __db_get_flags_fn();
int __db_prnpage(DB*, DB_TXN*, db_pgno_t);
int __db_prpage(DB*, PAGE*, uint32);
const char * __db_lockmode_to_string(db_lockmode_t);
int __db_dumptree(DB*, DB_TXN*, char *, char *, db_pgno_t, db_pgno_t);
const FN * __db_get_flags_fn();
int __db_prpage_int(ENV*, DB_MSGBUF*, DB*, const char *, PAGE*, uint32, uint8 *, uint32);
void __db_prbytes(ENV*, DB_MSGBUF*, uint8 *, uint32);
void __db_prflags(ENV*, DB_MSGBUF*, uint32, const FN*, const char *, const char *);
const char * __db_pagetype_to_string(uint32);
int __db_dump_pp(DB*, const char *, int (*)(void *, const void *), void *, int, int);
int __db_dump(DB*, const char *, int (*)(void *, const void *), void *, int, int);
int __db_prdbt(DBT*, int, const char *, void *, int (*)(void *, const void *), int, int);
int __db_prheader(DB*, const char *, int, int, void *, int (*)(void *, const void *), VRFY_DBINFO*, db_pgno_t);
int __db_prfooter(void *, int (*)(void *, const void *));
int __db_pr_callback(void *, const void *);
const char * __db_dbtype_to_string(DBTYPE);
int __db_addrem_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_addrem_42_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_big_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_big_42_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_ovref_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_debug_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_noop_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_alloc_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_free_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_freedata_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_cksum_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_init_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_trunc_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_realloc_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_sort_44_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_alloc_42_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_free_42_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pg_freedata_42_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_relink_42_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_relink_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_merge_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
int __db_pgno_recover(ENV*, DBT*, DB_LSN*, db_recops, void *);
void __db_pglist_swap(uint32, void *);
void __db_pglist_print(ENV*, DB_MSGBUF*, DBT *);
int __db_traverse_big(DBC*, db_pgno_t, int (*)(DBC *, PAGE *, void *, int *), void *);
int __db_reclaim_callback(DBC*, PAGE*, void *, int *);
int __db_truncate_callback(DBC*, PAGE*, void *, int *);
int __env_dbremove_pp(DB_ENV*, DB_TXN*, const char *, const char *, uint32);
int __db_remove_pp(DB*, const char *, const char *, uint32);
int __db_remove(DB*, DB_THREAD_INFO*, DB_TXN*, const char *, const char *, uint32);
int __db_remove_int(DB*, DB_THREAD_INFO*, DB_TXN*, const char *, const char *, uint32);
int __db_inmem_remove(DB*, DB_TXN*, const char *);
int __env_dbrename_pp(DB_ENV*, DB_TXN*, const char *, const char *, const char *, uint32);
int __db_rename_pp(DB*, const char *, const char *, const char *, uint32);
int __db_rename_int(DB*, DB_THREAD_INFO*, DB_TXN*, const char *, const char *, const char *, uint32);
int __db_ret(DBC*, PAGE*, uint32, DBT*, void **, uint32 *);
int __db_retcopy(ENV*, DBT*, void *, uint32, void **, uint32 *);
int __env_fileid_reset_pp(DB_ENV*, const char *, uint32);
int __env_fileid_reset(ENV*, DB_THREAD_INFO*, const char *, int);
int __env_lsn_reset_pp(DB_ENV*, const char *, uint32);
int FASTCALL __db_lsn_reset(DB_MPOOLFILE *, DB_THREAD_INFO *);
int __db_compare_both(DB*, const DBT*, const DBT*, const DBT*, const DBT *);
int __db_sort_multiple(DB*, DBT*, DBT*, uint32);
int __db_stat_pp(DB*, DB_TXN*, void *, uint32);
int __db_stat_print_pp(DB*, uint32);
int __db_stat_print(DB*, DB_THREAD_INFO*, uint32);
int __db_truncate_pp(DB*, DB_TXN*, uint32*, uint32);
int __db_truncate(DB*, DB_THREAD_INFO*, DB_TXN*, uint32 *);
int __db_upgrade_pp(DB*, const char *, uint32);
int __db_upgrade(DB*, const char *, uint32);
int __db_lastpgno(DB*, char *, DB_FH*, db_pgno_t *);
int __db_31_offdup(DB*, char *, DB_FH*, int, db_pgno_t *);
int __db_verify_pp(DB*, const char *, const char *, FILE*, uint32);
int __db_verify_internal(DB*, const char *, const char *, void *, int (*)(void *, const void *), uint32);
int __db_verify(DB*, DB_THREAD_INFO*, const char *, const char *, void *, int (*)(void *, const void *), void *, void *, uint32);
int __db_vrfy_common(DB*, VRFY_DBINFO*, PAGE*, db_pgno_t, uint32);
int __db_vrfy_datapage(DB*, VRFY_DBINFO*, PAGE*, db_pgno_t, uint32);
int __db_vrfy_meta(DB*, VRFY_DBINFO*, DBMETA*, db_pgno_t, uint32);
void __db_vrfy_struct_feedback(DB*, VRFY_DBINFO *);
int __db_salvage_pg(DB*, VRFY_DBINFO*, db_pgno_t, PAGE*, void *, int (*)(void *, const void *), uint32);
int __db_salvage_leaf(DB*, VRFY_DBINFO*, db_pgno_t, PAGE*, void *, int (*)(void *, const void *), uint32);
int __db_vrfy_inpitem(DB*, PAGE*, db_pgno_t, uint32, int, uint32, uint32*, uint32 *);
int __db_vrfy_duptype(DB*, VRFY_DBINFO*, db_pgno_t, uint32);
int __db_salvage_duptree(DB*, VRFY_DBINFO*, db_pgno_t, DBT*, void *, int (*)(void *, const void *), uint32);
int __db_vrfy_dbinfo_create(ENV*, DB_THREAD_INFO*, uint32, VRFY_DBINFO**);
int __db_vrfy_dbinfo_destroy(ENV*, VRFY_DBINFO *);
int __db_vrfy_getpageinfo(VRFY_DBINFO*, db_pgno_t, VRFY_PAGEINFO**);
int __db_vrfy_putpageinfo(ENV*, VRFY_DBINFO*, VRFY_PAGEINFO *);
int __db_vrfy_pgset(ENV*, DB_THREAD_INFO*, uint32, DB**);
int __db_vrfy_pgset_get(DB*, DB_THREAD_INFO*, DB_TXN*, db_pgno_t, int *);
int __db_vrfy_pgset_inc(DB*, DB_THREAD_INFO*, DB_TXN*, db_pgno_t);
int __db_vrfy_pgset_next(DBC*, db_pgno_t *);
int __db_vrfy_childcursor(VRFY_DBINFO*, DBC**);
int __db_vrfy_childput(VRFY_DBINFO*, db_pgno_t, VRFY_CHILDINFO *);
int __db_vrfy_ccset(DBC*, db_pgno_t, VRFY_CHILDINFO**);
int __db_vrfy_ccnext(DBC*, VRFY_CHILDINFO**);
int __db_vrfy_ccclose(DBC *);
int __db_salvage_init(VRFY_DBINFO *);
int __db_salvage_destroy(VRFY_DBINFO *);
int __db_salvage_getnext(VRFY_DBINFO*, DBC**, db_pgno_t*, uint32*, int);
int __db_salvage_isdone(VRFY_DBINFO*, db_pgno_t);
int __db_salvage_markdone(VRFY_DBINFO*, db_pgno_t);
int __db_salvage_markneeded(VRFY_DBINFO*, db_pgno_t, uint32);
int __db_vrfy_prdbt(DBT*, int, const char *, void *, int (*)(void *, const void *), int, int, VRFY_DBINFO *);
int __partition_init(DB*, uint32);
int __partition_set(DB*, uint32, DBT*, uint32 (* callback)(DB *, DBT * key));
int __partition_set_dirs(DB*, const char **);
int __partition_open(DB*, DB_THREAD_INFO*, DB_TXN*, const char *, DBTYPE, uint32, int, int);
int __partition_get_callback(DB*, uint32*, uint32(**callback) (DB*, DBT*key));
int __partition_get_keys(DB*, uint32*, DBT**);
int __partition_get_dirs(DB*, const char ***);
int __partc_init(DBC *);
int __partc_get(DBC*, DBT*, DBT*, uint32);
int __partition_close(DB*, DB_TXN*, uint32);
int __partition_sync(DB *);
int __partition_stat(DBC*, void *, uint32);
int __part_truncate(DBC*, uint32 *);
int __part_compact(DB*, DB_THREAD_INFO*, DB_TXN*, DBT*, DBT*, DB_COMPACT*, uint32, DBT *);
int __part_lsn_reset(DB*, DB_THREAD_INFO *);
int __part_fileid_reset(ENV*, DB_THREAD_INFO*, const char *, uint32, int);
int __part_key_range(DBC*, DBT*, DB_KEY_RANGE*, uint32);
int __part_remove(DB*, DB_THREAD_INFO*, DB_TXN*, const char *, const char *, uint32);
int __part_rename(DB*, DB_THREAD_INFO*, DB_TXN*, const char *, const char *, const char *);
int __part_verify(DB*, VRFY_DBINFO*, const char *, void *, int (*)(void *, const void *), uint32);
int __part_testdocopy(DB*, const char *);
int __db_no_partition(ENV *);
int __partition_set(DB*, uint32, DBT*, uint32 (* callback)(DB *, DBT * key));
int __partition_get_callback(DB*, uint32*, uint32(**callback) (DB*, DBT*key));
int __partition_get_dirs(DB*, const char ***);
int __partition_get_keys(DB*, uint32*, DBT**);
int __partition_init(DB*, uint32);
int __part_fileid_reset(ENV*, DB_THREAD_INFO*, const char *, uint32, int);
int __partition_set_dirs(DB*, const char **);

#if defined(__cplusplus)
}
#endif
#endif /* !_db_ext_h_ */
