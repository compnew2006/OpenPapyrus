/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 2011 Oracle and/or its affiliates.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Olson.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *  may be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

#include "db_config.h"
#include "db_int.h"
#pragma hdrstop

static int __bam_build(DBC*, uint32, DBT*, PAGE*, uint32, uint32);
static int __bam_dup_check(DBC*, uint32, PAGE*, uint32, uint32, db_indx_t *);
static int __bam_dup_convert(DBC*, PAGE*, uint32, uint32);
static int __bam_ovput(DBC*, uint32, db_pgno_t, PAGE*, uint32, DBT *);
static uint32 __bam_partsize(DB*, uint32, DBT*, PAGE*, uint32);
/*
 * __bam_iitem --
 *	Insert an item into the tree.
 *
 * PUBLIC: int __bam_iitem __P((DBC *, DBT *, DBT *, uint32, uint32));
 */
int __bam_iitem(DBC * dbc, DBT * key, DBT * data, uint32 op, uint32 flags)
{
	BKEYDATA * bk, bk_tmp;
	BTREE * t;
	BTREE_CURSOR * cp;
	DB * dbp;
	DBT bk_hdr, tdbt;
	DB_MPOOLFILE * mpf;
	ENV * env;
	PAGE * h;
	db_indx_t cnt, indx;
	uint32 data_size, have_bytes, need_bytes, needed, pages, pagespace;
	char tmp_ch;
	int cmp, bigkey, bigdata, del, dupadjust;
	int padrec, replace, ret, t_ret, was_deleted;
	COMPQUIET(cnt, 0);
	dbp = dbc->dbp;
	env = dbp->env;
	mpf = dbp->mpf;
	cp = reinterpret_cast<BTREE_CURSOR *>(dbc->internal);
	t = static_cast<BTREE *>(dbp->bt_internal);
	h = (PAGE *)cp->page;
	indx = cp->indx;
	del = dupadjust = replace = was_deleted = 0;
	/*
	 * Fixed-length records with partial puts: it's an error to specify
	 * anything other simple overwrite.
	 */
	if(F_ISSET(dbp, DB_AM_FIXEDLEN) && F_ISSET(data, DB_DBT_PARTIAL) && data->size != data->dlen)
		return __db_rec_repl(env, data->size, data->dlen);
	/*
	 * Figure out how much space the data will take, including if it's a
	 * partial record.
	 *
	 * Fixed-length records: it's an error to specify a record that's
	 * longer than the fixed-length, and we never require less than
	 * the fixed-length record size.
	 */
	data_size = F_ISSET(data, DB_DBT_PARTIAL) ? __bam_partsize(dbp, op, data, h, indx) : data->size;
	padrec = 0;
	if(F_ISSET(dbp, DB_AM_FIXEDLEN)) {
		if(data_size > t->re_len)
			return __db_rec_toobig(env, data_size, t->re_len);
		// Records that are deleted anyway needn't be padded out. 
		if(!LF_ISSET(BI_DELETED) && data_size < t->re_len) {
			padrec = 1;
			data_size = t->re_len;
		}
	}
	/*
	 * Handle partial puts or short fixed-length records: check whether we
	 * can just append the data or else build the real record.  We can't
	 * append if there are secondaries: we need the whole data item for the
	 * application's secondary callback.
	 */
	if(op == DB_CURRENT && dbp->dup_compare == NULL && F_ISSET(data, DB_DBT_PARTIAL) && !DB_IS_PRIMARY(dbp)) {
		bk = GET_BKEYDATA(dbp, h, indx+(TYPE(h) == P_LBTREE ? O_INDX : 0));
		/*
		 * If the item is an overflow type, and the input DBT is
		 * partial, and begins at the length of the current item then
		 * it is an append. Avoid deleting and re-creating the entire
		 * offpage item.
		 */
		if(B_TYPE(bk->type) == B_OVERFLOW && data->doff == ((BOVERFLOW *)bk)->tlen) {
			/*
			 * If the cursor has not already cached the last page
			 * in the offpage chain. We need to walk the chain
			 * to be sure that the page has been read.
			 */
			if(cp->stream_start_pgno != ((BOVERFLOW *)bk)->pgno || cp->stream_off > data->doff || data->doff > cp->stream_off+P_MAXSPACE(dbp, dbp->pgsize)) {
				memzero(&tdbt, sizeof(DBT));
				tdbt.doff = data->doff-1;
				// 
				// Set the length to 1, to force __db_goff to do the traversal.
				// 
				tdbt.dlen = tdbt.ulen = 1;
				tdbt.data = &tmp_ch;
				tdbt.flags = DB_DBT_PARTIAL|DB_DBT_USERMEM;
				// 
				// Read to the last page.  It will be cached in the cursor.
				// 
				if((ret = __db_goff(dbc, &tdbt, ((BOVERFLOW *)bk)->tlen, ((BOVERFLOW *)bk)->pgno, NULL, NULL)) != 0)
					return ret;
			}
			// 
			// Since this is an append, dlen is irrelevant (there are no bytes to overwrite). We need the caller's
			// DBT size to end up with the total size of the item. From now on, use dlen as the length of the user's
			// data that we are going to append. Don't futz with the caller's DBT any more than we
			// have to in order to send back the size.
			// 
			tdbt = *data;
			tdbt.dlen = data->size;
			tdbt.size = data_size;
			data = &tdbt;
			F_SET(data, DB_DBT_STREAMING);
		}
	}
	if(!F_ISSET(data, DB_DBT_STREAMING) && (padrec || F_ISSET(data, DB_DBT_PARTIAL))) {
		tdbt = *data;
		if((ret = __bam_build(dbc, op, &tdbt, h, indx, data_size)) != 0)
			return ret;
		data = &tdbt;
	}
	/*
	 * If the user has specified a duplicate comparison function, return
	 * an error if DB_CURRENT was specified and the replacement data
	 * doesn't compare equal to the current data.  This stops apps from
	 * screwing up the duplicate sort order.  We have to do this after
	 * we build the real record so that we're comparing the real items.
	 */
	if(op == DB_CURRENT && dbp->dup_compare) {
		if((ret = __bam_cmp(dbc, data, h, indx+(TYPE(h) == P_LBTREE ? O_INDX : 0), dbp->dup_compare, &cmp)) != 0)
			return ret;
		if(cmp != 0) {
			__db_errx(env, DB_STR("1004", "Existing data sorts differently from put data"));
			return EINVAL;
		}
	}
	// 
	// If the key or data item won't fit on a page, we'll have to store them on overflow pages.
	// 
	needed = 0;
	bigdata = data_size > cp->ovflsize;
	switch(op) {
	    case DB_KEYFIRST:
			// We're adding a new key and data pair. 
			bigkey = key->size > cp->ovflsize;
			needed += (bigkey ? BOVERFLOW_PSIZE : BKEYDATA_PSIZE(key->size));
			needed += (bigdata ? BOVERFLOW_PSIZE : BKEYDATA_PSIZE(data_size));
			break;
	    case DB_AFTER:
	    case DB_BEFORE:
	    case DB_CURRENT:
			/*
			 * We're either overwriting the data item of a key/data pair
			 * or we're creating a new on-page duplicate and only adding a data item.
			 *
			 * !!!
			 * We're not currently correcting for space reclaimed from
			 * already deleted items, but I don't think it's worth the complexity.
			 */
			bigkey = 0;
			if(op == DB_CURRENT) {
				bk = GET_BKEYDATA(dbp, h, indx+(TYPE(h) == P_LBTREE ? O_INDX : 0));
				have_bytes = (B_TYPE(bk->type) == B_KEYDATA) ? BKEYDATA_PSIZE(bk->len) : BOVERFLOW_PSIZE;
				need_bytes = 0;
			}
			else {
				have_bytes = 0;
				need_bytes = sizeof(db_indx_t);
			}
			need_bytes += (bigdata ? BOVERFLOW_PSIZE : BKEYDATA_PSIZE(data_size));
			if(have_bytes < need_bytes)
				needed += need_bytes-have_bytes;
			break;
	    default:
			return __db_unknown_flag(env, "DB->put", op);
	}
	/* Split the page if there's not enough room. */
	if(P_FREESPACE(dbp, h) < needed)
		return DB_NEEDSPLIT;
	/*
	 * Check to see if we will convert to off page duplicates -- if
	 * so, we'll need a page.
	 */
	if(F_ISSET(dbp, DB_AM_DUP) && TYPE(h) == P_LBTREE && op != DB_KEYFIRST && P_FREESPACE(dbp, h)-needed <= dbp->pgsize/2 && __bam_dup_check(dbc, op, h, indx, needed, &cnt)) {
		pages = 1;
		dupadjust = 1;
	}
	else
		pages = 0;
	/*
	 * If we are not using transactions and there is a page limit
	 * set on the file, then figure out if things will fit before
	 * taking action.
	 */
	if(dbc->txn == NULL && mpf->mfp->maxpgno != 0) {
		pagespace = P_MAXSPACE(dbp, dbp->pgsize);
		if(bigdata)
			pages += ((data_size-1)/pagespace)+1;
		if(bigkey)
			pages += ((key->size-1)/pagespace)+1;
		if(pages > (mpf->mfp->maxpgno-mpf->mfp->last_pgno))
			return __db_space_err(dbp);
	}
	if(F_ISSET(dbc, DBC_OPD))
		LOCK_CHECK_OFF(dbc->thread_info);
	ret = __memp_dirty(mpf, &h, dbc->thread_info, dbc->txn, dbc->priority, 0);
	if(cp->csp->page == cp->page)
		cp->csp->page = h;
	cp->page = h;
	if(F_ISSET(dbc, DBC_OPD))
		LOCK_CHECK_ON(dbc->thread_info);
	if(ret != 0)
		return ret;
	/*
	 * The code breaks it up into five cases:
	 *
	 * 1. Insert a new key/data pair.
	 * 2. Append a new data item (a new duplicate).
	 * 3. Insert a new data item (a new duplicate).
	 * 4. Delete and re-add the data item (overflow item).
	 * 5. Overwrite the data item.
	 */
	switch(op) {
	    case DB_KEYFIRST:           /* 1. Insert a new key/data pair. */
		if(bigkey) {
			if((ret = __bam_ovput(dbc, B_OVERFLOW, PGNO_INVALID, h, indx, key)) != 0)
				return ret;
		}
		else
		if((ret = __db_pitem(dbc, h, indx, BKEYDATA_SIZE(key->size), NULL, key)) != 0)
			return ret;
		if((ret = __bam_ca_di(dbc, PGNO(h), indx, 1)) != 0)
			return ret;
		++indx;
		break;
	    case DB_AFTER:              /* 2. Append a new data item. */
		if(TYPE(h) == P_LBTREE) {
			/* Copy the key for the duplicate and adjust cursors. */
			if((ret = __bam_adjindx(dbc, h, indx+P_INDX, indx, 1)) != 0)
				return ret;
			if((ret = __bam_ca_di(dbc, PGNO(h), indx+P_INDX, 1)) != 0)
				return ret;
			indx += 3;
			cp->indx += 2;
		}
		else {
			++indx;
			cp->indx += 1;
		}
		break;
	    case DB_BEFORE:             /* 3. Insert a new data item. */
		if(TYPE(h) == P_LBTREE) {
			/* Copy the key for the duplicate and adjust cursors. */
			if((ret = __bam_adjindx(dbc, h, indx, indx, 1)) != 0)
				return ret;
			if((ret = __bam_ca_di(dbc, PGNO(h), indx, 1)) != 0)
				return ret;
			++indx;
		}
		break;
	    case DB_CURRENT:
		/*
		 * Clear the cursor's deleted flag.  The problem is that if
		 * we deadlock or fail while deleting the overflow item or
		 * replacing the non-overflow item, a subsequent cursor close
		 * will try and remove the item because the cursor's delete
		 * flag is set.
		 */
		if((ret = __bam_ca_delete(dbp, PGNO(h), indx, 0, NULL)) != 0)
			return ret;
		if(TYPE(h) == P_LBTREE)
			++indx;
		bk = GET_BKEYDATA(dbp, h, indx);
		/*
		 * In a Btree deleted records aren't counted (deleted records
		 * are counted in a Recno because all accesses are based on
		 * record number).  If it's a Btree and it's a DB_CURRENT
		 * operation overwriting a previously deleted record, increment
		 * the record count.
		 */
		if(TYPE(h) == P_LBTREE || TYPE(h) == P_LDUP)
			was_deleted = B_DISSET(bk->type);
		/*
		 * 4. Delete and re-add the data item.
		 *
		 * If we're changing the type of the on-page structure, or we
		 * are referencing offpage items, we have to delete and then
		 * re-add the item.  We do not do any cursor adjustments here
		 * because we're going to immediately re-add the item into the
		 * same slot.
		 */
		if(bigdata || B_TYPE(bk->type) != B_KEYDATA) {
			/*
			 * If streaming, don't delete the overflow item,
			 * just delete the item pointing to the overflow item.
			 * It will be added back in later, with the new size.
			 * We can't simply adjust the size of the item on the
			 * page, because there is no easy way to log a
			 * modification.
			 */
			if(F_ISSET(data, DB_DBT_STREAMING)) {
				if((ret = __db_ditem(dbc, h, indx, BOVERFLOW_SIZE)) != 0)
					return ret;
			}
			else if((ret = __bam_ditem(dbc, h, indx)) != 0)
				return ret;
			del = 1;
			break;
		}
		/* 5. Overwrite the data item. */
		replace = 1;
		break;
	    default:
		return __db_unknown_flag(env, "DB->put", op);
	}
	/* Add the data. */
	if(bigdata) {
		/*
		 * We do not have to handle deleted (BI_DELETED) records
		 * in this case; the actual records should never be created.
		 */
		DB_ASSERT(env, !LF_ISSET(BI_DELETED));
		ret = __bam_ovput(dbc, B_OVERFLOW, PGNO_INVALID, h, indx, data);
	}
	else {
		if(LF_ISSET(BI_DELETED)) {
			B_TSET_DELETED(bk_tmp.type, B_KEYDATA);
			bk_tmp.len = data->size;
			bk_hdr.data = &bk_tmp;
			bk_hdr.size = SSZA(BKEYDATA, data);
			ret = __db_pitem(dbc, h, indx, BKEYDATA_SIZE(data->size), &bk_hdr, data);
		}
		else if(replace)
			ret = __bam_ritem(dbc, h, indx, data, 0);
		else
			ret = __db_pitem(dbc, h, indx, BKEYDATA_SIZE(data->size), NULL, data);
	}
	if(ret != 0) {
		if(del == 1 && (t_ret = __bam_ca_di(dbc, PGNO(h), indx+1, -1)) != 0) {
			__db_err(env, t_ret, DB_STR("1005", "cursor adjustment after delete failed"));
			return __env_panic(env, t_ret);
		}
		return ret;
	}
	/*
	 * Re-position the cursors if necessary and reset the current cursor
	 * to point to the new item.
	 */
	if(op != DB_CURRENT) {
		if((ret = __bam_ca_di(dbc, PGNO(h), indx, 1)) != 0)
			return ret;
		cp->indx = TYPE(h) == P_LBTREE ? indx-O_INDX : indx;
	}
	/*
	 * If we've changed the record count, update the tree.  There's no
	 * need to adjust the count if the operation not performed on the
	 * current record or when the current record was previously deleted.
	 */
	if(F_ISSET(cp, C_RECNUM) && (op != DB_CURRENT || was_deleted))
		if((ret = __bam_adjust(dbc, 1)) != 0)
			return ret;
	/*
	 * If a Btree leaf page is at least 50% full and we may have added or
	 * modified a duplicate data item, see if the set of duplicates takes
	 * up at least 25% of the space on the page.  If it does, move it onto
	 * its own page.
	 */
	if(dupadjust && (ret = __bam_dup_convert(dbc, h, indx-O_INDX, cnt)) != 0)
		return ret;
	/* If we've modified a recno file, set the flag. */
	if(dbc->dbtype == DB_RECNO)
		t->re_modified = 1;
	return ret;
}
/*
 * __bam_partsize --
 *	Figure out how much space a partial data item is in total.
 */
static uint32 __bam_partsize(DB * dbp, uint32 op, DBT * data, PAGE * h, uint32 indx)
{
	if(op != DB_CURRENT) {
		// If the record doesn't already exist, it's simply the data we're provided.
		return (data->doff + data->size);
	}
	else {
		// Otherwise, it's the data provided plus any already existing data that we're not replacing.
		BKEYDATA * bk = GET_BKEYDATA(dbp, h, indx+(TYPE(h) == P_LBTREE ? O_INDX : 0));
		uint32 nbytes = B_TYPE(bk->type) == B_OVERFLOW ? ((BOVERFLOW *)bk)->tlen : bk->len;
		return __db_partsize(nbytes, data);
	}
}
/*
 * __bam_build --
 *	Build the real record for a partial put, or short fixed-length record.
 */
static int __bam_build(DBC * dbc, uint32 op, DBT * dbt, PAGE * h, uint32 indx, uint32 nbytes)
{
	BKEYDATA * bk, tbk;
	BOVERFLOW * bo;
	BTREE * t;
	DB * dbp;
	DBT copy, * rdata;
	uint32 len, tlen;
	uint8 * p;
	int ret;
	COMPQUIET(bo, 0);
	dbp = dbc->dbp;
	t = static_cast<BTREE *>(dbp->bt_internal);
	/* We use the record data return memory, it's only a short-term use. */
	rdata = &dbc->my_rdata;
	if(rdata->ulen < nbytes) {
		if((ret = __os_realloc(dbp->env, nbytes, &rdata->data)) != 0) {
			rdata->ulen = 0;
			rdata->data = NULL;
			return ret;
		}
		rdata->ulen = nbytes;
	}
	/*
	 * We use nul or pad bytes for any part of the record that isn't
	 * specified; get it over with.
	 */
	memset(rdata->data, F_ISSET(dbp, DB_AM_FIXEDLEN) ? t->re_pad : 0, nbytes);
	/*
	 * In the next clauses, we need to do three things: a) set p to point
	 * to the place at which to copy the user's data, b) set tlen to the
	 * total length of the record, not including the bytes contributed by
	 * the user, and c) copy any valid data from an existing record.  If
	 * it's not a partial put (this code is called for both partial puts
	 * and fixed-length record padding) or it's a new key, we can cut to
	 * the chase.
	 */
	if(!F_ISSET(dbt, DB_DBT_PARTIAL) || op != DB_CURRENT) {
		p = (uint8 *)rdata->data+dbt->doff;
		tlen = dbt->doff;
		goto user_copy;
	}
	/* Find the current record. */
	if(indx < NUM_ENT(h)) {
		bk = GET_BKEYDATA(dbp, h, indx+(TYPE(h) == P_LBTREE ? O_INDX : 0));
		bo = (BOVERFLOW *)bk;
	}
	else {
		bk = &tbk;
		B_TSET(bk->type, B_KEYDATA);
		bk->len = 0;
	}
	if(B_TYPE(bk->type) == B_OVERFLOW) {
		/*
		 * In the case of an overflow record, we shift things around
		 * in the current record rather than allocate a separate copy.
		 */
		memzero(&copy, sizeof(copy));
		if((ret = __db_goff(dbc, &copy, bo->tlen, bo->pgno, &rdata->data, &rdata->ulen)) != 0)
			return ret;
		/* Skip any leading data from the original record. */
		tlen = dbt->doff;
		p = (uint8 *)rdata->data+dbt->doff;
		/*
		 * Copy in any trailing data from the original record.
		 *
		 * If the original record was larger than the original offset
		 * plus the bytes being deleted, there is trailing data in the
		 * original record we need to preserve.  If we aren't deleting
		 * the same number of bytes as we're inserting, copy it up or
		 * down, into place.
		 *
		 * Use memmove(), the regions may overlap.
		 */
		if(bo->tlen > dbt->doff+dbt->dlen) {
			len = bo->tlen-(dbt->doff+dbt->dlen);
			if(dbt->dlen != dbt->size)
				memmove(p+dbt->size, p+dbt->dlen, len);
			tlen += len;
		}
	}
	else {
		/* Copy in any leading data from the original record. */
		memcpy(rdata->data, bk->data, dbt->doff > bk->len ? bk->len : dbt->doff);
		tlen = dbt->doff;
		p = (uint8 *)rdata->data+dbt->doff;

		/* Copy in any trailing data from the original record. */
		len = dbt->doff+dbt->dlen;
		if(bk->len > len) {
			memcpy(p+dbt->size, bk->data+len, bk->len-len);
			tlen += bk->len-len;
		}
	}
user_copy:
	/*
	 * Copy in the application provided data -- p and tlen must have been
	 * initialized above.
	 */
	memcpy(p, dbt->data, dbt->size);
	tlen += dbt->size;
	/* Set the DBT to reference our new record. */
	rdata->size = F_ISSET(dbp, DB_AM_FIXEDLEN) ? t->re_len : tlen;
	rdata->dlen = 0;
	rdata->doff = 0;
	rdata->flags = 0;
	*dbt = *rdata;
	return 0;
}
/*
 * __bam_ritem --
 *	Replace an item on a page.
 */
int __bam_ritem(DBC * dbc, PAGE * h, uint32 indx, DBT * data, uint32 typeflag)
{
	BKEYDATA * bk;
	DB * dbp;
	DBT orig, repl;
	db_indx_t min, prefix, suffix;
	uint32 len;
	int ret;
	uint8 * dp, * p, * t, type;
	dbp = dbc->dbp;
	/*
	 * Replace a single item onto a page.  The logic figuring out where
	 * to insert and whether it fits is handled in the caller.  All we do
	 * here is manage the page shuffling.
	 */
	bk = GET_BKEYDATA(dbp, h, indx);
	len = bk->len;
	dp = bk->data;
	type = bk->type;
	typeflag = B_DISSET(type);
	/* Log the change. */
	if(DBC_LOGGING(dbc)) {
		/*
		 * We might as well check to see if the two data items share
		 * a common prefix and suffix -- it can save us a lot of log
		 * message if they're large.
		 */
		min = data->size < len ? data->size : len;
		for(prefix = 0, p = dp, t = (uint8 *)data->data; prefix < min && *p == *t; ++prefix, ++p, ++t)
			;
		min -= prefix;
		for(suffix = 0, p = (uint8 *)dp+len-1, t = (uint8 *)data->data+data->size-1; suffix < min && *p == *t; ++suffix, --p, --t)
			;
		/* We only log the parts of the keys that have changed. */
		orig.data = (uint8 *)dp+prefix;
		orig.size = len-(prefix+suffix);
		repl.data = (uint8 *)data->data+prefix;
		repl.size = data->size-(prefix+suffix);
		if((ret = __bam_repl_log(dbp, dbc->txn, &LSN(h), 0, PGNO(h), &LSN(h), (uint32)indx, typeflag, &orig, &repl, (uint32)prefix, (uint32)suffix)) != 0)
			return ret;
	}
	else
		LSN_NOT_LOGGED(LSN(h));
	return __bam_ritem_nolog(dbc, h, indx, NULL, data, type);
}
/*
 * __bam_ritem_nolog --
 *	Replace an item on a page.
 */
int __bam_ritem_nolog(DBC * dbc, PAGE * h, uint32 indx, DBT * hdr, DBT * data, uint32 type)
{
	BKEYDATA * bk;
	BINTERNAL * bi;
	db_indx_t cnt, off, lo, ln;
	int32 nbytes;
	uint8 * t;
	DB * dbp = dbc->dbp;
	/*
	 * Set references to the first in-use byte on the page and the
	 * first byte of the item being replaced.
	 */
	db_indx_t * inp = P_INP(dbp, h);
	uint8 * p = (uint8 *)h+HOFFSET(h);
	if(TYPE(h) == P_IBTREE) {
		bi = GET_BINTERNAL(dbp, h, indx);
		t = (uint8 *)bi;
		lo = (db_indx_t)BINTERNAL_SIZE(bi->len);
		if(data == NULL) {
			DB_ASSERT(dbp->env, hdr != NULL);
			bi = (BINTERNAL *)hdr->data;
			P_16_COPY(&bi->len, &cnt);
			ln = (db_indx_t)BINTERNAL_SIZE(cnt);
		}
		else
			ln = (db_indx_t)BINTERNAL_SIZE(data->size);
	}
	else {
		bk = GET_BKEYDATA(dbp, h, indx);
		t = (uint8 *)bk;
		lo = (db_indx_t)BKEYDATA_SIZE(bk->len);
		ln = (db_indx_t)BKEYDATA_SIZE(data->size);
	}
	/*
	 * If the entry is growing in size, shift the beginning of the data
	 * part of the page down.  If the entry is shrinking in size, shift
	 * the beginning of the data part of the page up.  Use memmove(3),
	 * the regions overlap.
	 */
	if(lo != ln) {
		nbytes = (int32)(lo-ln); /* Signed difference. */
		if(p == t)                      /* First index is fast. */
			inp[indx] += (uint32)nbytes;
		else {                          /* Else, shift the page. */
			memmove(p+nbytes, p, (size_t)(t-p));
			/* Adjust the indices' offsets. */
			off = (uint32)inp[indx];
			for(cnt = 0; cnt < NUM_ENT(h); ++cnt)
				if(inp[cnt] <= off)
					inp[cnt] += (uint32)nbytes;
		}
		/* Clean up the page and adjust the item's reference. */
		HOFFSET(h) += (uint32)nbytes;
		t += nbytes;
	}
	/* Copy the new item onto the page. */
	if(TYPE(h) == P_IBTREE) {
		DB_ASSERT(dbp->env, hdr != NULL);
		memcpy(t, hdr->data, hdr->size);
		bi = (BINTERNAL *)t;
		if(data && data->size != 0)
			memcpy(bi->data, data->data, data->size);
	}
	else {
		bk = (BKEYDATA *)t;
		bk->len = data->size;
		B_TSET(bk->type, type);
		memcpy(bk->data, data->data, bk->len);
	}
	return 0;
}
/*
 * __bam_irep --
 *	Replace an item on an internal page.
 *
 * PUBLIC: int __bam_irep __P((DBC *, PAGE *, uint32, DBT *, DBT *));
 */
int __bam_irep(DBC * dbc, PAGE * h, uint32 indx, DBT * hdr, DBT * data)
{
	DBT dbt;
	int ret;
	DB * dbp = dbc->dbp;
	BINTERNAL * bi = GET_BINTERNAL(dbp, h, indx);
	BINTERNAL * bn = (BINTERNAL *)hdr->data;
	if(B_TYPE(bi->type) == B_OVERFLOW && (ret = __db_doff(dbc, ((BOVERFLOW *)bi->data)->pgno)) != 0)
		return ret;
	if(DBC_LOGGING(dbc)) {
		dbt.data = bi;
		dbt.size = BINTERNAL_SIZE(bi->len);
		if((ret = __bam_irep_log(dbp, dbc->txn, &LSN(h), 0, PGNO(h), &LSN(h), (uint32)indx, TYPE(h), hdr, data, &dbt)) != 0)
			return ret;
	}
	else
		LSN_NOT_LOGGED(LSN(h));
	return __bam_ritem_nolog(dbc, h, indx, hdr, data, bn->type);
}
/*
 * __bam_dup_check --
 *	Check to see if the duplicate set at indx should have its own page.
 */
static int __bam_dup_check(DBC * dbc, uint32 op, PAGE * h, uint32 indx, uint32 sz, db_indx_t * cntp)
{
	BKEYDATA * bk;
	db_indx_t cnt, first;
	DB * dbp = dbc->dbp;
	db_indx_t * inp = P_INP(dbp, h);
	/*
	 * Count the duplicate records and calculate how much room they're
	 * using on the page.
	 */
	while(indx > 0 && inp[indx] == inp[indx-P_INDX])
		indx -= P_INDX;
	/* Count the key once. */
	bk = GET_BKEYDATA(dbp, h, indx);
	sz += B_TYPE(bk->type) == B_KEYDATA ? BKEYDATA_PSIZE(bk->len) : BOVERFLOW_PSIZE;
	/*
	 * Sum up all the data items.
	 * Account for the record being inserted.  If we are replacing it,
	 * don't count it twice.
	 *
	 * We execute the loop with first == indx to get the size of the
	 * first record.
	 */
	cnt = op == DB_CURRENT ? 0 : 1;
	for(first = indx; indx < NUM_ENT(h) && inp[first] == inp[indx]; ++cnt, indx += P_INDX) {
		bk = GET_BKEYDATA(dbp, h, indx+O_INDX);
		sz += B_TYPE(bk->type) == B_KEYDATA ? BKEYDATA_PSIZE(bk->len) : BOVERFLOW_PSIZE;
	}
	/*
	 * We have to do these checks when the user is replacing the cursor's
	 * data item -- if the application replaces a duplicate item with a
	 * larger data item, it can increase the amount of space used by the
	 * duplicates, requiring this check.  But that means we may have done
	 * this check when it wasn't a duplicate item after all.
	 */
	if(cnt == 1)
		return 0;
	/*
	 * If this set of duplicates is using more than 25% of the page, move
	 * them off.  The choice of 25% is a WAG, but the value must be small
	 * enough that we can always split a page without putting duplicates
	 * on two different pages.
	 */
	if(sz < dbp->pgsize/4)
		return 0;
	*cntp = cnt;
	return 1;
}
/*
 * __bam_dup_convert --
 *	Move a set of duplicates off-page and into their own tree.
 */
static int __bam_dup_convert(DBC * dbc, PAGE * h, uint32 indx, uint32 cnt)
{
	BKEYDATA * bk;
	DBT hdr;
	DB_LOCK lock;
	PAGE * dp;
	db_indx_t cpindx, dindx, first;
	int ret, t_ret;
	DB * dbp = dbc->dbp;
	DB_MPOOLFILE * mpf = dbp->mpf;
	db_indx_t * inp = P_INP(dbp, h);
	/* Move to the beginning of the dup set. */
	while(indx > 0 && inp[indx] == inp[indx-P_INDX])
		indx -= P_INDX;
	/* Get a new page. */
	if((ret = __db_new(dbc, dbp->dup_compare == NULL ? P_LRECNO : P_LDUP, &lock, &dp)) != 0)
		return ret;
	P_INIT(dp, dbp->pgsize, dp->pgno, PGNO_INVALID, PGNO_INVALID, LEAFLEVEL, TYPE(dp));

	/*
	 * Move this set of duplicates off the page.  First points to the first
	 * key of the first duplicate key/data pair, cnt is the number of pairs
	 * we're dealing with.
	 */
	memzero(&hdr, sizeof(hdr));
	first = indx;
	dindx = indx;
	cpindx = 0;
	do {
		/* Move cursors referencing the old entry to the new entry. */
		if((ret = __bam_ca_dup(dbc, first, PGNO(h), indx, PGNO(dp), cpindx)) != 0)
			goto err;
		/*
		 * Copy the entry to the new page.  If the off-duplicate page
		 * If the off-duplicate page is a Btree page (i.e. dup_compare
		 * will be non-NULL, we use Btree pages for sorted dups,
		 * and Recno pages for unsorted dups), move all entries
		 * normally, even deleted ones.  If it's a Recno page,
		 * deleted entries are discarded (if the deleted entry is
		 * overflow, then free up those pages).
		 */
		bk = GET_BKEYDATA(dbp, h, dindx+1);
		hdr.data = bk;
		hdr.size = B_TYPE(bk->type) == B_KEYDATA ? BKEYDATA_SIZE(bk->len) : BOVERFLOW_SIZE;
		if(dbp->dup_compare == NULL && B_DISSET(bk->type)) {
			/*
			 * Unsorted dups, i.e. recno page, and we have
			 * a deleted entry, don't move it, but if it was
			 * an overflow entry, we need to free those pages.
			 */
			if(B_TYPE(bk->type) == B_OVERFLOW && (ret = __db_doff(dbc, (GET_BOVERFLOW(dbp, h, dindx+1))->pgno)) != 0)
				goto err;
		}
		else {
			if((ret = __db_pitem(dbc, dp, cpindx, hdr.size, &hdr, NULL)) != 0)
				goto err;
			++cpindx;
		}
		/* Delete all but the last reference to the key. */
		if(cnt != 1) {
			if((ret = __bam_adjindx(dbc, h, dindx, first+1, 0)) != 0)
				goto err;
		}
		else
			dindx++;
		/* Delete the data item. */
		if((ret = __db_ditem(dbc, h, dindx, hdr.size)) != 0)
			goto err;
		indx += P_INDX;
	} while(--cnt);
	/* Put in a new data item that points to the duplicates page. */
	if((ret = __bam_ovput(dbc, B_DUPLICATE, dp->pgno, h, first+1, NULL)) != 0)
		goto err;
	/* Adjust cursors for all the above movements. */
	ret = __bam_ca_di(dbc, PGNO(h), first+P_INDX, (int)(first+P_INDX-indx));
err:
	if((t_ret = __memp_fput(mpf, dbc->thread_info, dp, dbc->priority)) != 0 && ret == 0)
		ret = t_ret;
	__TLPUT(dbc, lock);
	return ret;
}
/*
 * __bam_ovput --
 *	Build an item for an off-page duplicates page or overflow page and
 *	insert it on the page.
 */
static int __bam_ovput(DBC * dbc, uint32 type, db_pgno_t pgno, PAGE * h, uint32 indx, DBT * item)
{
	BOVERFLOW bo;
	DBT hdr;
	int ret;
	UMRW_SET(bo.unused1);
	B_TSET(bo.type, type);
	UMRW_SET(bo.unused2);
	/*
	 * If we're creating an overflow item, do so and acquire the page
	 * number for it.  If we're creating an off-page duplicates tree,
	 * we are giving the page number as an argument.
	 */
	if(type == B_OVERFLOW) {
		if((ret = __db_poff(dbc, item, &bo.pgno)) != 0)
			return ret;
		bo.tlen = item->size;
	}
	else {
		bo.pgno = pgno;
		bo.tlen = 0;
	}
	/* Store the new record on the page. */
	memzero(&hdr, sizeof(hdr));
	hdr.data = &bo;
	hdr.size = BOVERFLOW_SIZE;
	return __db_pitem(dbc, h, indx, BOVERFLOW_SIZE, &hdr, 0);
}
