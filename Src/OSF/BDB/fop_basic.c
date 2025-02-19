/*-
 * See the file LICENSE for redistribution information.
 * Copyright (c) 2001, 2011 Oracle and/or its affiliates.  All rights reserved.
 * $Id$
 */
#include "db_config.h"
#include "db_int.h"
#pragma hdrstop
/*
 * The transactional guarantees Berkeley DB provides for file
 * system level operations (database physical file create, delete,
 * rename) are based on our understanding of current file system
 * semantics; a system that does not provide these semantics and
 * guarantees could be in danger.
 *
 * First, as in standard database changes, fsync and fdatasync must
 * work: when applied to the log file, the records written into the
 * log must be transferred to stable storage.
 *
 * Second, it must not be possible for the log file to be removed
 * without previous file system level operations being flushed to
 * stable storage.  Berkeley DB applications write log records
 * describing file system operations into the log, then perform the
 * file system operation, then commit the enclosing transaction
 * (which flushes the log file to stable storage).  Subsequently,
 * a database environment checkpoint may make it possible for the
 * application to remove the log file containing the record of the
 * file system operation.  DB's transactional guarantees for file
 * system operations require the log file removal not succeed until
 * all previous filesystem operations have been flushed to stable
 * storage.  In other words, the flush of the log file, or the
 * removal of the log file, must block until all previous
 * filesystem operations have been flushed to stable storage.  This
 * semantic is not, as far as we know, required by any existing
 * standards document, but we have never seen a filesystem where
 * it does not apply.
 */

/*
 * __fop_create --
 * Create a (transactionally protected) file system object.  This is used
 * to create DB files now, potentially blobs, queue extents and anything
 * else you wish to store in a file system object.
 *
 * PUBLIC: int __fop_create __P((ENV *, DB_TXN *,
 * PUBLIC:     DB_FH **, const char *, const char **, APPNAME, int, uint32));
 */
int __fop_create(ENV * env, DB_TXN * txn, DB_FH ** fhpp, const char * name, const char ** dirp, APPNAME appname, int mode, uint32 flags)
{
	DBT data, dirdata;
	DB_LSN lsn;
	int ret;
	char * real_name = NULL;
	DB_FH * fhp = NULL;
	if((ret = __db_appname(env, appname, name, dirp, &real_name)) != 0)
		return ret;
	if(mode == 0)
		mode = DB_MODE_600;
	if(DBENV_LOGGING(env)
#if !defined(DEBUG_WOP)
	   && txn != NULL
#endif
	   ) {
		DB_INIT_DBT(data, name, sstrlen(name)+1);
		if(dirp && *dirp)
			DB_INIT_DBT(dirdata, *dirp, sstrlen(*dirp)+1);
		else
			memzero(&dirdata, sizeof(dirdata));
		if((ret = __fop_create_log(env, txn, &lsn, flags|DB_FLUSH, &data, &dirdata, (uint32)appname, (uint32)mode)) != 0)
			goto err;
	}
	DB_ENV_TEST_RECOVERY(env, DB_TEST_POSTLOG, ret, name);
	if(fhpp == NULL)
		fhpp = &fhp;
	ret = __os_open(env, real_name, 0, DB_OSO_CREATE|DB_OSO_EXCL, mode, fhpp);
err:
	DB_TEST_RECOVERY_LABEL
	if(fhpp == &fhp && fhp)
		__os_closehandle(env, fhp);
	__os_free(env, real_name);
	return ret;
}
/*
 * __fop_remove --
 *	Remove a file system object.
 *
 * PUBLIC: int __fop_remove __P((ENV *, DB_TXN *,
 * PUBLIC:     uint8 *, const char *, const char **, APPNAME, uint32));
 */
int __fop_remove(ENV * env, DB_TXN * txn, uint8 * fileid, const char * name, const char ** dirp, APPNAME appname, uint32 flags)
{
	DBT fdbt, ndbt;
	DB_LSN lsn;
	int ret;
	char * real_name = NULL;
	if((ret = __db_appname(env, appname, name, dirp, &real_name)) != 0)
		goto err;
	if(!IS_REAL_TXN(txn)) {
		if(fileid && (ret = __memp_nameop(env, fileid, NULL, real_name, NULL, 0)) != 0)
			goto err;
	}
	else {
		if(DBENV_LOGGING(env)
#if !defined(DEBUG_WOP)
		   && txn != NULL
#endif
		   ) {
			memzero(&fdbt, sizeof(ndbt));
			fdbt.data = fileid;
			fdbt.size = fileid == NULL ? 0 : DB_FILE_ID_LEN;
			DB_INIT_DBT(ndbt, name, sstrlen(name)+1);
			if((ret = __fop_remove_log(env, txn, &lsn, flags, &ndbt, &fdbt, (uint32)appname)) != 0)
				goto err;
		}
		ret = __txn_remevent(env, txn, real_name, fileid, 0);
	}
err:
	__os_free(env, real_name);
	return ret;
}

/*
 * __fop_write
 *
 * Write "size" bytes from "buf" to file "name" beginning at offset "off."
 * If the file is open, supply a handle in fhp.  Istmp indicate if this is
 * an operation that needs to be undone in the face of failure (i.e., if
 * this is a write to a temporary file, we're simply going to remove the
 * file, so don't worry about undoing the write).
 *
 * Currently, we *only* use this with istmp true.  If we need more general
 * handling, then we'll have to zero out regions on abort (and possibly
 * log the before image of the data in the log record).
 *
 * PUBLIC: int __fop_write __P((ENV *, DB_TXN *,
 * PUBLIC:     const char *, const char *, APPNAME, DB_FH *, uint32,
 * PUBLIC:     db_pgno_t, uint32, void *, uint32, uint32, uint32));
 */
int __fop_write(ENV * env, DB_TXN * txn, const char * name, const char * dirname, APPNAME appname, DB_FH * fhp,
	uint32 pgsize, db_pgno_t pageno, uint32 off, void * buf, uint32 size, uint32 istmp, uint32 flags)
{
	DBT data, namedbt, dirdbt;
	DB_LSN lsn;
	size_t nbytes;
	int local_open = 0;
	int ret = 0;
	int t_ret;
	char * real_name = 0;
	DB_ASSERT(env, istmp != 0);
	if(DBENV_LOGGING(env)
#if !defined(DEBUG_WOP)
	   && txn != NULL
#endif
	   ) {
		memzero(&data, sizeof(data));
		data.data = buf;
		data.size = size;
		DB_INIT_DBT(namedbt, name, sstrlen(name)+1);
		if(dirname)
			DB_INIT_DBT(dirdbt, dirname, sstrlen(dirname)+1);
		else
			memzero(&dirdbt, sizeof(dirdbt));
		if((ret = __fop_write_log(env, txn, &lsn, flags, &namedbt, &dirdbt, (uint32)appname, pgsize, pageno, off, &data, istmp)) != 0)
			goto err;
	}
	if(fhp == NULL) {
		// File isn't open; we need to reopen it
		if((ret = __db_appname(env, appname, name, &dirname, &real_name)) != 0)
			return ret;
		if((ret = __os_open(env, real_name, 0, 0, 0, &fhp)) != 0)
			goto err;
		local_open = 1;
	}
	// Seek to offset
	if((ret = __os_seek(env, fhp, pageno, pgsize, off)) != 0)
		goto err;
	// Now do the write
	if((ret = __os_write(env, fhp, buf, size, &nbytes)) != 0)
		goto err;
err:
	if(local_open && (t_ret = __os_closehandle(env, fhp)) != 0 && ret == 0)
		ret = t_ret;
	__os_free(env, real_name);
	return ret;
}
/*
 * __fop_rename --
 *	Change a file's name.
 *
 * PUBLIC: int __fop_rename __P((ENV *, DB_TXN *, const char *, const char *,
 * PUBLIC:      const char **, uint8 *, APPNAME, int, uint32));
 */
int __fop_rename(ENV * env, DB_TXN * txn, const char * oldname, const char * newname, const char ** dirp, uint8 * fid, APPNAME appname, int with_undo, uint32 flags)
{
	DBT fiddbt, dir, new_dbt, old;
	DB_LSN lsn;
	int ret;
	char * n, * o;
	o = n = NULL;
	if((ret = __db_appname(env, appname, oldname, dirp, &o)) != 0)
		goto err;
	if((ret = __db_appname(env, appname, newname, dirp, &n)) != 0)
		goto err;
	if(DBENV_LOGGING(env)
#if !defined(DEBUG_WOP)
	   && txn != NULL
#endif
	   ) {
		DB_INIT_DBT(old, oldname, sstrlen(oldname)+1);
		DB_INIT_DBT(new_dbt, newname, sstrlen(newname)+1);
		if(dirp && *dirp)
			DB_INIT_DBT(dir, *dirp, sstrlen(*dirp)+1);
		else
			memzero(&dir, sizeof(dir));
		memzero(&fiddbt, sizeof(fiddbt));
		fiddbt.data = fid;
		fiddbt.size = DB_FILE_ID_LEN;
		if(with_undo)
			ret = __fop_rename_log(env, txn, &lsn, flags|DB_FLUSH, &old, &new_dbt, &dir, &fiddbt, (uint32)appname);
		else
			ret = __fop_rename_noundo_log(env, txn, &lsn, flags|DB_FLUSH, &old, &new_dbt, &dir, &fiddbt, (uint32)appname);
		if(ret != 0)
			goto err;
	}
	ret = __memp_nameop(env, fid, newname, o, n, 0);
err:
	__os_free(env, o);
	__os_free(env, n);
	return ret;
}
