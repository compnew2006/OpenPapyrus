/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2011 Oracle and/or its affiliates.  All rights reserved.
 *
 * $Id$
 */
#include "db_config.h"
#include "db_int.h"
#pragma hdrstop

#define QAM_EXNAME(Q, I, B, L)  snprintf((B), (L), QUEUE_EXTENT, (Q)->dir, PATH_SEPARATOR[0], (Q)->name, (I))
/*
 * __qam_fprobe -- calculate and open extent
 *
 * Calculate which extent the page is in, open and create if necessary.
 *
 * PUBLIC: int __qam_fprobe __P((DBC *, db_pgno_t,
 * PUBLIC:     void *, qam_probe_mode, DB_CACHE_PRIORITY, uint32));
 */
int __qam_fprobe(DBC * dbc, db_pgno_t pgno, void * addrp, qam_probe_mode mode, DB_CACHE_PRIORITY priority, uint32 flags)
{
	DB_MPOOLFILE * mpf;
	MPFARRAY * array;
	uint8 fid[DB_FILE_ID_LEN];
	uint32 i, extid, maxext, numext, lflags, offset, oldext, openflags;
	char buf[DB_MAXPATHLEN];
	int ftype, less, t_ret;
	DB * dbp = dbc->dbp;
	ENV * env = dbp->env;
	QUEUE * qp = (QUEUE *)dbp->q_internal;
	int ret = 0;
	if(qp->page_ext == 0) {
		mpf = dbp->mpf;
		switch(mode) {
		    case QAM_PROBE_GET: return __memp_fget(mpf, &pgno, dbc->thread_info, dbc->txn, flags, addrp);
		    case QAM_PROBE_PUT: return __memp_fput(mpf, dbc->thread_info, addrp, priority);
		    case QAM_PROBE_DIRTY: return __memp_dirty(mpf, addrp, dbc->thread_info, dbc->txn, priority, flags);
		    case QAM_PROBE_MPF: *(DB_MPOOLFILE **)addrp = mpf; return 0;
		}
	}
	mpf = NULL;
	/*
	 * Need to lock long enough to find the mpf or create the file.
	 * The file cannot go away because we must have a record locked in that file.
	 */
	MUTEX_LOCK(env, dbp->mutex);
	extid = QAM_PAGE_EXTENT(dbp, pgno);
	/* Array1 will always be in use if array2 is in use. */
	array = &qp->array1;
	if(array->n_extent == 0) {
		/* Start with 4 extents */
		array->n_extent = 4;
		array->low_extent = extid;
		numext = offset = oldext = 0;
		less = 0;
		goto alloc;
	}
retry:
	if(extid < array->low_extent) {
		less = 1;
		offset = array->low_extent-extid;
	}
	else {
		less = 0;
		offset = extid-array->low_extent;
	}
	if(qp->array2.n_extent != 0 && (extid >= qp->array2.low_extent ? offset > extid-qp->array2.low_extent : offset > qp->array2.low_extent-extid)) {
		array = &qp->array2;
		if(extid < array->low_extent) {
			less = 1;
			offset = array->low_extent-extid;
		}
		else {
			less = 0;
			offset = extid-array->low_extent;
		}
	}
	/*
	 * Check to see if the requested extent is outside the range of
	 * extents in the array.  This is true by default if there are
	 * no extents here yet.
	 */
	if(less == 1 || offset >= array->n_extent) {
		oldext = array->n_extent;
		numext = (array->hi_extent-array->low_extent)+1;
		if(less == 1 && offset+numext <= array->n_extent) {
			/*
			 * If we can fit this one into the existing array by
			 * shifting the existing entries then we do not have to allocate.
			 */
			memmove(&array->mpfarray[offset], array->mpfarray, numext * sizeof(array->mpfarray[0]));
			memzero(array->mpfarray, offset*sizeof(array->mpfarray[0]));
			offset = 0;
		}
		else if(less == 0 && offset == array->n_extent && oneof2(mode, QAM_PROBE_GET, QAM_PROBE_PUT) && array->mpfarray[0].pinref == 0) {
			/*
			 * If this is at the end of the array and the file at
			 * the beginning has a zero pin count we can close
			 * the bottom extent and put this one at the end.
			 */
			mpf = array->mpfarray[0].mpf;
			if(mpf && (ret = __memp_fclose(mpf, 0)) != 0)
				goto err;
			memmove(&array->mpfarray[0], &array->mpfarray[1], (array->n_extent-1)*sizeof(array->mpfarray[0]));
			array->low_extent++;
			array->hi_extent++;
			offset--;
			array->mpfarray[offset].mpf = NULL;
			array->mpfarray[offset].pinref = 0;
		}
		else {
			/*
			 * See if we have wrapped around the queue.
			 * If it has then allocate the second array.
			 * Otherwise just expand the one we are using.
			 */
			maxext = (uint32)UINT32_MAX/(qp->page_ext*qp->rec_page);
			if(offset >= maxext/2) {
				array = &qp->array2;
				DB_ASSERT(env, array->n_extent == 0);
				oldext = 0;
				array->n_extent = 4;
				array->low_extent = extid;
				offset = 0;
				numext = 0;
			}
			else if(array->mpfarray[0].pinref == 0) {
				// 
				// Check to see if there are extents marked
				// for deletion at the beginning of the cache.
				// If so close them so they will go away.
				// 
				for(i = 0; i < array->n_extent; i++) {
					if(array->mpfarray[i].pinref != 0)
						break;
					else {
						mpf = array->mpfarray[i].mpf;
						if(mpf) {
							__memp_get_flags(mpf, &lflags);
							if(!FLD_ISSET(lflags, DB_MPOOL_UNLINK))
								break;
							else {
								array->mpfarray[i].mpf = NULL;
								if((ret = __memp_fclose(mpf, 0)) != 0)
									goto err;
							}
						}
					}
				}
				if(i == 0)
					goto increase;
				memmove(&array->mpfarray[0], &array->mpfarray[i], (array->n_extent-i)*sizeof(array->mpfarray[0]));
				memzero(&array->mpfarray[array->n_extent-i], i*sizeof(array->mpfarray[0]));
				array->low_extent += i;
				array->hi_extent += i;
				goto retry;
			}
			else {
				// Increase the size to at least include the new one and double it.
increase:
				array->n_extent += offset;
				array->n_extent <<= 2;
			}
alloc:
			if((ret = __os_realloc(env, array->n_extent*sizeof(struct __qmpf), &array->mpfarray)) != 0)
				goto err;
			if(less == 1) {
				// Move the array up and put the new one in the first slot.
				memmove(&array->mpfarray[offset], array->mpfarray, numext*sizeof(array->mpfarray[0]));
				memzero(array->mpfarray, offset*sizeof(array->mpfarray[0]));
				memzero(&array->mpfarray[numext+offset], (array->n_extent-(numext+offset))*sizeof(array->mpfarray[0]));
				offset = 0;
			}
			else // Clear the new part of the array
				memzero(&array->mpfarray[oldext], (array->n_extent-oldext)*sizeof(array->mpfarray[0]));
		}
	}
	// Update the low and hi range of saved extents
	if(extid < array->low_extent)
		array->low_extent = extid;
	if(extid > array->hi_extent)
		array->hi_extent = extid;
	// If the extent file is not yet open, open it
	if(array->mpfarray[offset].mpf == NULL) {
		QAM_EXNAME(qp, extid, buf, sizeof(buf));
		if((ret = __memp_fcreate(env, &array->mpfarray[offset].mpf)) != 0)
			goto err;
		mpf = array->mpfarray[offset].mpf;
		__memp_set_lsn_offset(mpf, 0);
		__memp_set_pgcookie(mpf, &qp->pgcookie);
		__memp_get_ftype(dbp->mpf, &ftype);
		__memp_set_ftype(mpf, ftype);
		__memp_set_clear_len(mpf, dbp->pgsize);
		/* Set up the fileid for this extent. */
		__qam_exid(dbp, fid, extid);
		__memp_set_fileid(mpf, fid);
		openflags = DB_EXTENT;
		if(LF_ISSET(DB_MPOOL_CREATE))
			openflags |= DB_CREATE;
		if(F_ISSET(dbp, DB_AM_RDONLY))
			openflags |= DB_RDONLY;
		if(F_ISSET(env->dbenv, DB_ENV_DIRECT_DB))
			openflags |= DB_DIRECT;
		if((ret = __memp_fopen(mpf, NULL, buf, NULL, openflags, qp->mode, dbp->pgsize)) != 0) {
			array->mpfarray[offset].mpf = NULL;
			__memp_fclose(mpf, 0);
			goto err;
		}
	}
	/*
	 * We have found the right file.  Update its ref count
	 * before dropping the dbp mutex so it does not go away.
	 */
	mpf = array->mpfarray[offset].mpf;
	if(mode == QAM_PROBE_GET)
		array->mpfarray[offset].pinref++;
	/*
	 * If we may create the page, then we are writing,
	 * the file may nolonger be empty after this operation
	 * so we clear the UNLINK flag.
	 */
	if(LF_ISSET(DB_MPOOL_CREATE))
		__memp_set_flags(mpf, DB_MPOOL_UNLINK, 0);
err:
	MUTEX_UNLOCK(env, dbp->mutex);
	if(!ret) {
		pgno--;
		pgno %= qp->page_ext;
		switch(mode) {
		    case QAM_PROBE_GET:
				ret = __memp_fget(mpf, &pgno, dbc->thread_info, dbc->txn, flags, addrp);
				if(!ret)
					return 0;
				break;
		    case QAM_PROBE_PUT:
				ret = __memp_fput(mpf, dbc->thread_info, addrp, dbp->priority);
				break;
		    case QAM_PROBE_DIRTY:
				return __memp_dirty(mpf, addrp, dbc->thread_info, dbc->txn, dbp->priority, flags);
		    case QAM_PROBE_MPF:
				*(DB_MPOOLFILE **)addrp = mpf;
				return 0;
		}
		MUTEX_LOCK(env, dbp->mutex);
		/* Recalculate because we dropped the lock. */
		offset = extid-array->low_extent;
		DB_ASSERT(env, array->mpfarray[offset].pinref > 0);
		if(--array->mpfarray[offset].pinref == 0 && (mode == QAM_PROBE_GET || ret == 0)) {
			/* Check to see if this file will be unlinked. */
			__memp_get_flags(mpf, &flags);
			if(LF_ISSET(DB_MPOOL_UNLINK)) {
				array->mpfarray[offset].mpf = NULL;
				if((t_ret = __memp_fclose(mpf, 0)) != 0 && ret == 0)
					ret = t_ret;
			}
		}
		MUTEX_UNLOCK(env, dbp->mutex);
	}
	return ret;
}
/*
 * __qam_fclose -- close an extent.
 *
 * Calculate which extent the page is in and close it.
 * We assume the mpf entry is present.
 *
 * PUBLIC: int __qam_fclose __P((DB *, db_pgno_t));
 */
int __qam_fclose(DB * dbp, db_pgno_t pgnoaddr)
{
	DB_MPOOLFILE * mpf;
	MPFARRAY * array;
	uint32 extid, offset;
	int ret = 0;
	ENV * env = dbp->env;
	QUEUE * qp = (QUEUE *)dbp->q_internal;
	MUTEX_LOCK(env, dbp->mutex);
	extid = QAM_PAGE_EXTENT(dbp, pgnoaddr);
	array = &qp->array1;
	if(array->low_extent > extid || array->hi_extent < extid)
		array = &qp->array2;
	offset = extid-array->low_extent;
	DB_ASSERT(env, extid >= array->low_extent && offset < array->n_extent);
	/* If other threads are still using this file, leave it. */
	if(array->mpfarray[offset].pinref != 0)
		goto done;
	mpf = array->mpfarray[offset].mpf;
	array->mpfarray[offset].mpf = NULL;
	ret = __memp_fclose(mpf, 0);
done:
	MUTEX_UNLOCK(env, dbp->mutex);
	return ret;
}
/*
 * __qam_fremove -- remove an extent.
 *
 * Calculate which extent the page is in and remove it.  There is no way
 * to remove an extent without probing it first and seeing that is is empty
 * so we assume the mpf entry is present.
 *
 * PUBLIC: int __qam_fremove __P((DB *, db_pgno_t));
 */
int __qam_fremove(DB*dbp, db_pgno_t pgnoaddr)
{
	DB_MPOOLFILE * mpf;
	MPFARRAY * array;
	uint32 extid, offset;
	QUEUE * qp = (QUEUE *)dbp->q_internal;
	ENV * env = dbp->env;
	int ret = 0;
	MUTEX_LOCK(env, dbp->mutex);
	extid = QAM_PAGE_EXTENT(dbp, pgnoaddr);
	array = &qp->array1;
	if(array->low_extent > extid || array->hi_extent < extid)
		array = &qp->array2;
	offset = extid-array->low_extent;
	DB_ASSERT(env, extid >= array->low_extent && offset < array->n_extent);
	mpf = array->mpfarray[offset].mpf;
	/* This extent my already be marked for delete and closed. */
	if(mpf == NULL)
		goto err;
	/*
	 * The log must be flushed before the file is deleted.  We depend on
	 * the log record of the last delete to recreate the file if we crash.
	 */
	if(LOGGING_ON(env) && (ret = __log_flush(env, NULL)) != 0)
		goto err;
	__memp_set_flags(mpf, DB_MPOOL_UNLINK, 1);
	/* Someone could be real slow, let them close it down. */
	if(array->mpfarray[offset].pinref != 0)
		goto err;
	array->mpfarray[offset].mpf = NULL;
	if((ret = __memp_fclose(mpf, 0)) != 0)
		goto err;
	/*
	 * If the file is at the bottom of the array
	 * shift things down and adjust the end points.
	 */
	if(offset == 0) {
		memmove(array->mpfarray, &array->mpfarray[1], (array->hi_extent-array->low_extent)*sizeof(array->mpfarray[0]));
		array->mpfarray[array->hi_extent-array->low_extent].mpf = NULL;
		if(array->low_extent != array->hi_extent)
			array->low_extent++;
	}
	else {
		if(extid == array->hi_extent)
			array->hi_extent--;
	}
err:
	MUTEX_UNLOCK(env, dbp->mutex);
	return ret;
}
/*
 * __qam_sync --
 *	Flush the database cache.
 *
 * PUBLIC: int __qam_sync __P((DB *));
 */
int __qam_sync(DB*dbp)
{
	int ret;
	// 
	// We can't easily identify the extent files associated with a specific
	// Queue file, so flush all Queue extent files.
	// 
	if((ret = __memp_fsync(dbp->mpf)) != 0)
		return ret;
	if(((QUEUE *)dbp->q_internal)->page_ext != 0)
		return __memp_sync_int(dbp->env, NULL, 0, DB_SYNC_QUEUE_EXTENT, 0, 0);
	return 0;
}
/*
 * __qam_gen_filelist -- generate a list of extent files.
 *	Another thread may close the handle so this should only
 *	be used single threaded or with care.
 *
 * PUBLIC: int __qam_gen_filelist __P((DB *,
 * PUBLIC:      DB_THREAD_INFO *, QUEUE_FILELIST **));
 */
int __qam_gen_filelist(DB*dbp, DB_THREAD_INFO * ip, QUEUE_FILELIST ** filelistp)
{
	DBC * dbc;
	QMETA * meta;
	size_t extent_cnt;
	db_recno_t i, current, first, stop, rec_extent;
	QUEUE_FILELIST * fp;
	int ret;
	ENV * env = dbp->env;
	DB_MPOOLFILE * mpf = dbp->mpf;
	QUEUE * qp = (QUEUE *)dbp->q_internal;
	*filelistp = NULL;
	if(qp->page_ext == 0)
		return 0;
	// This may happen during metapage recovery
	if(qp->name == NULL)
		return 0;
	// Find out the first and last record numbers in the database
	i = PGNO_BASE_MD;
	if((ret = __memp_fget(mpf, &i, ip, NULL, 0, &meta)) != 0)
		return ret;
	current = meta->cur_recno;
	first = meta->first_recno;
	if((ret = __memp_fput(mpf, ip, meta, dbp->priority)) != 0)
		return ret;
	/*
	 * Allocate the extent array.  Calculate the worst case number of
	 * pages and convert that to a count of extents.   The count of
	 * extents has 3 or 4 extra slots:
	 * roundoff at first (e.g., current record in extent);
	 * roundoff at current (e.g., first record in extent);
	 * NULL termination; and
	 * UINT32_MAX wraparound (the last extent can be small).
	 */
	rec_extent = qp->rec_page*qp->page_ext;
	extent_cnt = (current >= first) ? ((current-first)/rec_extent+3) : ((current+(UINT32_MAX-first))/rec_extent+4);
	if(extent_cnt == 0)
		return 0;
	if((ret = __os_calloc(env, extent_cnt, sizeof(QUEUE_FILELIST), filelistp)) != 0)
		return ret;
	fp = *filelistp;
	if((ret = __db_cursor(dbp, ip, NULL, &dbc, 0)) != 0)
		return ret;
again:
	stop = (current >= first) ? current : UINT32_MAX;
	/*
	 * Make sure that first is at the same offset in the extent as stop.
	 * This guarantees that the stop will be reached in the loop below,
	 * even if it is the only record in its extent.  This calculation is
	 * safe because first won't move out of its extent.
	 */
	first -= first%rec_extent;
	first += stop%rec_extent;
	for(i = first; i >= first && i <= stop; i += rec_extent) {
		if((ret = __qam_fprobe(dbc, QAM_RECNO_PAGE(dbp, i), &fp->mpf, QAM_PROBE_MPF, dbp->priority, 0)) != 0) {
			if(ret == ENOENT)
				continue;
			goto err;
		}
		fp->id = QAM_RECNO_EXTENT(dbp, i);
		fp++;
		DB_ASSERT(env, (size_t)(fp-*filelistp) < extent_cnt);
	}
	if(current < first) {
		first = 1;
		goto again;
	}
err:
	__dbc_close(dbc);
	return ret;
}
/*
 * __qam_extent_names -- generate a list of extent files names.
 *
 * PUBLIC: int __qam_extent_names __P((ENV *, char *, char ***));
 */
int __qam_extent_names(ENV * env, char * name, char *** namelistp)
{
	DB * dbp;
	DB_THREAD_INFO * ip;
	QUEUE * qp;
	QUEUE_FILELIST * filelist, * fp;
	size_t len;
	int cnt, ret, t_ret;
	char buf[DB_MAXPATHLEN], ** cp, * freep;
	*namelistp = NULL;
	filelist = NULL;
	ENV_GET_THREAD_INFO(env, ip);
	if((ret = __db_create_internal(&dbp, env, 0)) != 0)
		return ret;
	if((ret = __db_open(dbp, ip, NULL, name, NULL, DB_QUEUE, DB_RDONLY, 0, PGNO_BASE_MD)) != 0)
		goto done;
	qp = (QUEUE *)dbp->q_internal;
	if(qp->page_ext == 0)
		goto done;
	if((ret = __qam_gen_filelist(dbp, ip, &filelist)) != 0)
		goto done;
	if(filelist == NULL)
		goto done;
	cnt = 0;
	for(fp = filelist; fp->mpf != NULL; fp++)
		cnt++;
	// QUEUE_EXTENT contains extra chars, but add 6 anyway for the int
	len = (size_t)cnt*(sizeof(**namelistp)+sstrlen(QUEUE_EXTENT)+sstrlen(qp->dir)+sstrlen(qp->name)+6);
	if((ret = __os_malloc(dbp->env, len, namelistp)) != 0)
		goto done;
	cp = *namelistp;
	freep = (char *)(cp+cnt+1);
	for(fp = filelist; fp->mpf != NULL; fp++) {
		QAM_EXNAME(qp, fp->id, buf, sizeof(buf));
		len = sstrlen(buf);
		*cp++ = freep;
		strcpy(freep, buf);
		freep += len+1;
	}
	*cp = NULL;
done:
	__os_free(dbp->env, filelist);
	if((t_ret = __db_close(dbp, NULL, DB_NOSYNC)) != 0 && ret == 0)
		ret = t_ret;
	return ret;
}
/*
 * __qam_exid --
 *	Generate a fileid for an extent based on the fileid of the main
 * file.  Since we do not log schema creates/deletes explicitly, the log
 * never captures the fileid of an extent file.  In order that masters and
 * replicas have the same fileids (so they can explicitly delete them), we
 * use computed fileids for the extent files of Queue files.
 *
 * An extent file id retains the low order 12 bytes of the file id and
 * overwrites the dev/inode fields, placing a 0 in the inode field, and
 * the extent number in the dev field.
 *
 * PUBLIC: void __qam_exid __P((DB *, uint8 *, uint32));
 */
void __qam_exid(DB*dbp, uint8 * fidp, uint32 exnum)
{
	int i;
	uint8 * p;
	// Copy the fileid from the master
	memcpy(fidp, dbp->fileid, DB_FILE_ID_LEN);
	// The first four bytes are the inode or the FileIndexLow; 0 it
	for(i = sizeof(uint32); i > 0; --i)
		*fidp++ = 0;
	// The next four bytes are the dev/FileIndexHigh; insert the exnum 
	for(p = (uint8 *)&exnum, i = sizeof(uint32); i > 0; --i)
		*fidp++ = *p++;
}
/*
 * __qam_nameop --
 *	Remove or rename  extent files associated with a particular file.
 * This is to remove or rename (both in mpool and the file system) any
 * extent files associated with the given dbp.
 * This is either called from the QUEUE remove or rename methods or
 * when undoing a transaction that created the database.
 *
 * PUBLIC: int __qam_nameop __P((DB *, DB_TXN *, const char *, qam_name_op));
 */
int __qam_nameop(DB*dbp, DB_TXN * txn, const char * newname, qam_name_op op)
{
	size_t exlen, fulllen, len;
	uint8 fid[DB_FILE_ID_LEN];
	uint32 exid;
	int cnt, i, ret, t_ret;
	char buf[DB_MAXPATHLEN], nbuf[DB_MAXPATHLEN], sepsave;
	char * endname, * endpath, * exname, * fullname, ** names;
	const char * ndir;
	char * namep, * p_new_, * cp;
	ENV * env = dbp->env;
	QUEUE * qp = (QUEUE *)dbp->q_internal;
	cnt = ret = t_ret = 0;
	namep = exname = fullname = NULL;
	names = NULL;
	/* If this isn't a queue with extents, we're done. */
	if(qp->page_ext == 0)
		return 0;
	/*
	 * Generate the list of all queue extents for this file (from the
	 * file system) and then cycle through removing them and evicting
	 * from mpool.  We have two modes of operation here.  If we are
	 * undoing log operations, then do not write log records and try
	 * to keep going even if we encounter failures in nameop.  If we
	 * are in mainline code, then return as soon as we have a problem.
	 * Memory allocation errors (__db_appname, __os_malloc) are always
	 * considered failure.
	 *
	 * Set buf to : dir/__dbq.NAME.0 and fullname to HOME/dir/__dbq.NAME.0
	 * or, in the case of an absolute path: /dir/__dbq.NAME.0
	 */
	QAM_EXNAME(qp, 0, buf, sizeof(buf));
	if((ret = __db_appname(env, DB_APP_DATA, buf, &dbp->dirname, &fullname)) != 0)
		return ret;
	/* We should always have a path separator here. */
	if((endpath = __db_rpath(fullname)) == NULL) {
		ret = EINVAL;
		goto err;
	}
	sepsave = *endpath;
	*endpath = '\0';
	/*
	 * Get the list of all names in the directory and restore the
	 * path separator.
	 */
	if((ret = __os_dirlist(env, fullname, 0, &names, &cnt)) != 0)
		goto err;
	*endpath = sepsave;
	/* If there aren't any names, don't allocate any space. */
	if(cnt == 0)
		goto err;
	/*
	 * Now, make endpath reference the queue extent names upon which
	 * we can match.  Then we set the end of the path to be the
	 * beginning of the extent number, and we can compare the bytes
	 * between endpath and endname (__dbq.NAME.).
	 */
	endpath++;
	endname = strrchr(endpath, '.');
	if(endname == NULL) {
		ret = EINVAL;
		goto err;
	}
	++endname;
	*endname = '\0';
	len = sstrlen(endpath);
	fulllen = sstrlen(fullname);

	/* Allocate space for a full extent name.  */
	exlen = fulllen+20;
	if((ret = __os_malloc(env, exlen, &exname)) != 0)
		goto err;
	ndir = 0;
	p_new_ = 0;
	if(newname != NULL) {
		if((ret = __os_strdup(env, newname, &namep)) != 0)
			goto err;
		ndir = namep;
		if((p_new_ = __db_rpath(namep)) != NULL)
			*p_new_++ = '\0';
		else {
			p_new_ = namep;
			ndir = PATH_DOT;
		}
	}
	for(i = 0; i < cnt; i++) {
		/* Check if this is a queue extent file. */
		if(strncmp(names[i], endpath, len) != 0)
			continue;
		/* Make sure we have all numbers. foo.db vs. foo.db.0. */
		for(cp = &names[i][len]; *cp != '\0'; cp++)
			if(!isdigit((int)*cp))
				break;
		if(*cp != '\0')
			continue;
		/*
		 * We have a queue extent file.  We need to generate its
		 * name and its fileid.
		 */
		exid = (uint32)strtoul(names[i]+len, NULL, 10);
		__qam_exid(dbp, fid, exid);

		switch(op) {
		    case QAM_NAME_DISCARD:
			snprintf(exname, exlen, "%s%s", fullname, names[i]+len);
			if((t_ret = __memp_nameop(dbp->env, fid, NULL, exname, NULL, F_ISSET(dbp, DB_AM_INMEM))) != 0 && ret == 0)
				ret = t_ret;
			break;

		    case QAM_NAME_RENAME:
			snprintf(nbuf, sizeof(nbuf), QUEUE_EXTENT, ndir, PATH_SEPARATOR[0], p_new_, exid);
			QAM_EXNAME(qp, exid, buf, sizeof(buf));
			if((ret = __fop_rename(env, txn, buf, nbuf, &dbp->dirname, fid, DB_APP_DATA, 1, F_ISSET(dbp, DB_AM_NOT_DURABLE) ? DB_LOG_NOT_DURABLE : 0)) != 0)
				goto err;
			break;

		    case QAM_NAME_REMOVE:
			QAM_EXNAME(qp, exid, buf, sizeof(buf));
			if((ret = __fop_remove(env, txn, fid, buf, &dbp->dirname, DB_APP_DATA, F_ISSET(dbp, DB_AM_NOT_DURABLE) ? DB_LOG_NOT_DURABLE : 0)) != 0)
				goto err;
			break;
		}
	}
err:
	__os_free(env, fullname);
	__os_free(env, exname);
	__os_free(env, namep);
	__os_dirfree(env, names, cnt);
	return ret;
}
/*
 * __qam_lsn_reset -- reset the lsns for extents.
 *
 * PUBLIC: int __qam_lsn_reset __P((DB *, DB_THREAD_INFO *));
 */
int __qam_lsn_reset(DB*dbp, DB_THREAD_INFO * ip)
{
	QUEUE_FILELIST * filelist, * fp;
	int ret;
	QUEUE * qp = (QUEUE *)dbp->q_internal;
	if(qp->page_ext == 0)
		return 0;
	if((ret = __qam_gen_filelist(dbp, ip, &filelist)) != 0)
		return ret;
	if(filelist == NULL)
		return ret;
	for(fp = filelist; fp->mpf != NULL; fp++)
		if((ret = __db_lsn_reset(fp->mpf, ip)) != 0)
			break;
	__os_free(dbp->env, filelist);
	return ret;
}
