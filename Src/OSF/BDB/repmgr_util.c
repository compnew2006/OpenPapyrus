/*-
 * See the file LICENSE for redistribution information.
 * Copyright (c) 2005, 2011 Oracle and/or its affiliates.  All rights reserved.
 * $Id$
 */
#include "db_config.h"
#include "db_int.h"
#pragma hdrstop

#define INITIAL_SITES_ALLOCATION        3            /* Arbitrary guess. */
#define RETRY_TIME_ADJUST               200000       /* Arbitrary estimate. */

static int get_eid(ENV*, const char *, uint, int *);
static int __repmgr_addrcmp(repmgr_netaddr_t*, repmgr_netaddr_t *);
static int read_gmdb(ENV*, DB_THREAD_INFO*, uint8 **, size_t *);
/*
 * Schedules a future attempt to re-establish a connection with the given site.
 * Usually, we wait the configured retry_wait period.  But if the "immediate"
 * parameter is given as TRUE, we'll make the wait time 0, and put the request
 * at the _beginning_ of the retry queue.
 *
 * PUBLIC: int __repmgr_schedule_connection_attempt __P((ENV *, uint, int));
 *
 * !!!
 * Caller should hold mutex.
 *
 * Unless an error occurs, we always attempt to wake the main thread;
 * __repmgr_bust_connection relies on this behavior.
 */
int __repmgr_schedule_connection_attempt(ENV * env, uint eid, int immediate)
{
	DB_REP * db_rep;
	REP * rep;
	REPMGR_RETRY * retry, * target;
	REPMGR_SITE * site;
	db_timespec t;
	int cmp, ret;
	db_rep = env->rep_handle;
	rep = db_rep->region;
	if((ret = __os_malloc(env, sizeof(*retry), &retry)) != 0)
		return ret;
	site = SITE_FROM_EID(eid);
	__os_gettime(env, &t, 1);
	if(immediate)
		TAILQ_INSERT_HEAD(&db_rep->retries, retry, entries);
	else {
		TIMESPEC_ADD_DB_TIMEOUT(&t, rep->connection_retry_wait);
		/*
		 * Although it's extremely rare, two sites could be trying to
		 * connect to each other simultaneously, and each could kill its
		 * own connection when it received the other's.  And this could
		 * continue, in sync, since configured retry times are usually
		 * the same.  So, perturb one site's retry time by a small
		 * amount to break the cycle.  Since each site has its own
		 * address, it's always possible to decide which is "greater
		 * than".
		 *   (The mnemonic is that a server conventionally has a
		 * small well-known port number.  And clients have the right to
		 * connect to servers, not the other way around.)
		 */
		cmp = __repmgr_addrcmp(&site->net_addr, &SITE_FROM_EID(db_rep->self_eid)->net_addr);
		DB_ASSERT(env, cmp != 0);
		if(cmp == 1)
			TIMESPEC_ADD_DB_TIMEOUT(&t, RETRY_TIME_ADJUST);
		/*
		 * Insert the new "retry" on the (time-ordered) list in its
		 * proper position.  To do so, find the list entry ("target")
		 * with a later time; insert the new entry just before that.
		 */
		TAILQ_FOREACH(target, &db_rep->retries, entries) {
			if(timespeccmp(&target->time, &t, >))
				break;
		}
		if(target == NULL)
			TAILQ_INSERT_TAIL(&db_rep->retries, retry, entries);
		else
			TAILQ_INSERT_BEFORE(target, retry, entries);
	}
	retry->eid = eid;
	retry->time = t;
	site->state = SITE_PAUSING;
	site->ref.retry = retry;
	return __repmgr_wake_main_thread(env);
}
/*
 * Compare two network addresses (lexicographically), and return -1, 0, or 1, as
 * the first is less than, equal to, or greater than the second.
 */
static int __repmgr_addrcmp(repmgr_netaddr_t * addr1, repmgr_netaddr_t * addr2)
{
	int cmp = strcmp(addr1->host, addr2->host);
	if(cmp != 0)
		return cmp;
	if(addr1->port < addr2->port)
		return -1;
	else if(addr1->port > addr2->port)
		return 1;
	return 0;
}
/*
 * Initialize the necessary control structures to begin reading a new input
 * message.
 *
 * PUBLIC: void __repmgr_reset_for_reading __P((REPMGR_CONNECTION *));
 */
void __repmgr_reset_for_reading(REPMGR_CONNECTION * con)
{
	con->reading_phase = SIZES_PHASE;
	__repmgr_iovec_init(&con->iovecs);
	__repmgr_add_buffer(&con->iovecs, con->msg_hdr_buf, __REPMGR_MSG_HDR_SIZE);
}
/*
 * Constructs a DB_REPMGR_CONNECTION structure.
 *
 * PUBLIC: int __repmgr_new_connection __P((ENV *,
 * PUBLIC:     REPMGR_CONNECTION **, socket_t, int));
 */
int __repmgr_new_connection(ENV * env, REPMGR_CONNECTION ** connp, socket_t s, int state)
{
	REPMGR_CONNECTION * c;
	int ret;
	if((ret = __os_calloc(env, 1, sizeof(REPMGR_CONNECTION), &c)) != 0)
		return ret;
	if((ret = __repmgr_alloc_cond(&c->drained)) != 0) {
		__os_free(env, c);
		return ret;
	}
	if((ret = __repmgr_init_waiters(env, &c->response_waiters)) != 0) {
		__repmgr_free_cond(&c->drained);
		__os_free(env, c);
		return ret;
	}
	c->fd = s;
	c->state = state;
	c->type = UNKNOWN_CONN_TYPE;
#ifdef DB_WIN32
	c->event_object = WSA_INVALID_EVENT;
#endif
	STAILQ_INIT(&c->outbound_queue);
	c->out_queue_length = 0;
	__repmgr_reset_for_reading(c);
	*connp = c;
	return 0;
}
/*
 * PUBLIC: int __repmgr_new_site __P((ENV *, REPMGR_SITE**,
 * PUBLIC:     const char *, uint));
 *
 * Manipulates the process-local copy of the sites list.  So, callers should
 * hold the db_rep->mutex (except for single-threaded, pre-open configuration).
 */
int __repmgr_new_site(ENV * env, REPMGR_SITE ** sitep, const char * host, uint port)
{
	REPMGR_CONNECTION * conn;
	REPMGR_SITE * site, * sites;
	char * p;
	uint i, new_site_max;
	int ret;
	DB_REP * db_rep = env->rep_handle;
	if(db_rep->site_cnt >= db_rep->site_max) {
		new_site_max = db_rep->site_max == 0 ? INITIAL_SITES_ALLOCATION : db_rep->site_max*2;
		if((ret = __os_malloc(env, sizeof(REPMGR_SITE)*new_site_max, &sites)) != 0)
			return ret;
		if(db_rep->site_max > 0) {
			/*
			 * For each site in the array, copy the old struct to
			 * the space allocated for the new struct.  But the
			 * sub_conns list header (and one of the conn structs on
			 * the list, if any) contain pointers to the address of
			 * the old list header; so we have to move them
			 * explicitly.  If not for that, we could use a simple
			 * __os_realloc() call.
			 */
			for(i = 0; i < db_rep->site_cnt; i++) {
				sites[i] = db_rep->sites[i];
				TAILQ_INIT(&sites[i].sub_conns);
				while(!TAILQ_EMPTY(&db_rep->sites[i].sub_conns)) {
					conn = TAILQ_FIRST(&db_rep->sites[i].sub_conns);
					TAILQ_REMOVE(&db_rep->sites[i].sub_conns, conn, entries);
					TAILQ_INSERT_TAIL(&sites[i].sub_conns, conn, entries);
				}
			}
			__os_free(env, db_rep->sites);
		}
		db_rep->sites = sites;
		db_rep->site_max = new_site_max;
	}
	if((ret = __os_strdup(env, host, &p)) != 0) {
		/* No harm in leaving the increased site_max intact. */
		return ret;
	}
	site = &db_rep->sites[db_rep->site_cnt++];
	site->net_addr.host = p;
	site->net_addr.port = (uint16)port;
	ZERO_LSN(site->max_ack);
	site->ack_policy = 0;
	site->alignment = 0;
	site->flags = 0;
	timespecclear(&site->last_rcvd_timestamp);
	TAILQ_INIT(&site->sub_conns);
	site->connector = NULL;
	site->state = SITE_IDLE;
	site->membership = 0;
	site->config = 0;
	*sitep = site;
	return 0;
}
/*
 * PUBLIC: int __repmgr_create_mutex __P((ENV *, mgr_mutex_t **));
 */
int __repmgr_create_mutex(ENV * env, mgr_mutex_t ** mtxp)
{
	mgr_mutex_t * mtx;
	int ret;
	if((ret = __os_malloc(env, sizeof(mgr_mutex_t), &mtx)) == 0 && (ret = __repmgr_create_mutex_pf(mtx)) != 0) {
		__os_free(env, mtx);
	}
	if(!ret)
		*mtxp = mtx;
	return ret;
}
/*
 * PUBLIC: int __repmgr_destroy_mutex __P((ENV *, mgr_mutex_t *));
 */
int __repmgr_destroy_mutex(ENV * env, mgr_mutex_t * mtx)
{
	int ret = __repmgr_destroy_mutex_pf(mtx);
	__os_free(env, mtx);
	return ret;
}
/*
 * Kind of like a destructor for a repmgr_netaddr_t: cleans up any subordinate
 * allocated memory pointed to by the addr, though it does not free the struct
 * itself.
 *
 * PUBLIC: void __repmgr_cleanup_netaddr __P((ENV *, repmgr_netaddr_t *));
 */
void __repmgr_cleanup_netaddr(ENV * env, repmgr_netaddr_t * addr)
{
	if(addr->host != NULL) {
		__os_free(env, addr->host);
		addr->host = NULL;
	}
}
/*
 * PUBLIC: void __repmgr_iovec_init __P((REPMGR_IOVECS *));
 */
void __repmgr_iovec_init(REPMGR_IOVECS * v)
{
	v->offset = v->count = 0;
	v->total_bytes = 0;
}
/*
 * PUBLIC: void __repmgr_add_buffer __P((REPMGR_IOVECS *, void *, size_t));
 *
 * !!!
 * There is no checking for overflow of the vectors[5] array.
 */
void __repmgr_add_buffer(REPMGR_IOVECS * v, void * address, size_t length)
{
	if(length > 0) {
		v->vectors[v->count].iov_base = (char *)address;
		v->vectors[v->count++].iov_len = (ulong)length;
		v->total_bytes += length;
	}
}
/*
 * PUBLIC: void __repmgr_add_dbt __P((REPMGR_IOVECS *, const DBT *));
 */
void __repmgr_add_dbt(REPMGR_IOVECS * v, const DBT * dbt)
{
	if(dbt->size > 0) {
		v->vectors[v->count].iov_base = (char *)dbt->data;
		v->vectors[v->count++].iov_len = dbt->size;
		v->total_bytes += dbt->size;
	}
}
/*
 * Update a set of iovecs to reflect the number of bytes transferred in an I/O
 * operation, so that the iovecs can be used to continue transferring where we
 * left off.
 *   Returns TRUE if the set of buffers is now fully consumed, FALSE if more
 * remains.
 *
 * PUBLIC: int __repmgr_update_consumed __P((REPMGR_IOVECS *, size_t));
 */
int __repmgr_update_consumed(REPMGR_IOVECS * v, size_t byte_count)
{
	db_iovec_t * iov;
	int i;
	for(i = v->offset;; i++) {
		DB_ASSERT(NULL, i < v->count && byte_count > 0);
		iov = &v->vectors[i];
		if(byte_count > iov->iov_len) {
			/*
			 * We've consumed (more than) this vector's worth.
			 * Adjust count and continue.
			 */
			byte_count -= iov->iov_len;
		}
		else {
			/*
			 * Adjust length of remaining portion of vector.
			 * byte_count can never be greater than iov_len, or we
			 * would not be in this section of the if clause.
			 */
			iov->iov_len -= (uint32)byte_count;
			if(iov->iov_len > 0) {
				/*
				 * Still some left in this vector.  Adjust base
				 * address too, and leave offset pointing here.
				 */
				iov->iov_base = (char *)(void *)((uint8 *)iov->iov_base+byte_count);
				v->offset = i;
			}
			else {
				/*
				 * Consumed exactly to a vector boundary.
				 * Advance to next vector for next time.
				 */
				v->offset = i+1;
			}
			/*
			 * If offset has reached count, the entire thing is
			 * consumed.
			 */
			return v->offset >= v->count;
		}
	}
}
/*
 * Builds a buffer containing our network address information, suitable for
 * publishing as cdata via a call to rep_start, and sets up the given DBT to
 * point to it.  The buffer is dynamically allocated memory, and the caller must
 * assume responsibility for it.
 *
 * PUBLIC: int __repmgr_prepare_my_addr(ENV *, DBT *);
 */
int __repmgr_prepare_my_addr(ENV * env, DBT * dbt)
{
	DB_REP * db_rep;
	repmgr_netaddr_t addr;
	size_t size, hlen;
	uint16 port_buffer;
	uint8 * ptr;
	int ret;
	db_rep = env->rep_handle;
	LOCK_MUTEX(db_rep->mutex);
	addr = SITE_FROM_EID(db_rep->self_eid)->net_addr;
	UNLOCK_MUTEX(db_rep->mutex);
	/*
	 * The cdata message consists of the 2-byte port number, in network byte
	 * order, followed by the null-terminated host name string.
	 */
	port_buffer = htons(addr.port);
	size = sizeof(port_buffer)+(hlen = sstrlen(addr.host)+1);
	if((ret = __os_malloc(env, size, &ptr)) != 0)
		return ret;
	DB_INIT_DBT(*dbt, ptr, size);
	memcpy(ptr, &port_buffer, sizeof(port_buffer));
	ptr = &ptr[sizeof(port_buffer)];
	memcpy(ptr, addr.host, hlen);
	return 0;
}
/*
 * !!!
 * This may only be called after threads have been started, because we don't
 * know the answer until we have established group membership (e.g., reading the
 * membership database).  That should be OK, because we only need this
 * for starting an election, or counting acks after sending a PERM message.
 *
 * PUBLIC: int __repmgr_get_nsites __P((ENV *, uint32 *));
 */
int __repmgr_get_nsites(ENV*env, uint32 * nsitesp)
{
	uint32 nsites;
	DB_REP * db_rep = env->rep_handle;
	if((nsites = db_rep->region->config_nsites) == 0) {
		__db_errx(env, DB_STR("3672", "Nsites unknown before repmgr_start()"));
		return EINVAL;
	}
	*nsitesp = nsites;
	return 0;
}
/*
 * PUBLIC: int __repmgr_thread_failure __P((ENV *, int));
 */
int __repmgr_thread_failure(ENV * env, int why)
{
	DB_REP * db_rep = env->rep_handle;
	LOCK_MUTEX(db_rep->mutex);
	__repmgr_stop_threads(env);
	UNLOCK_MUTEX(db_rep->mutex);
	return __env_panic(env, why);
}
/*
 * Format a printable representation of a site location, suitable for inclusion
 * in an error message.  The buffer must be at least as big as
 * MAX_SITE_LOC_STRING.
 *
 * PUBLIC: char *__repmgr_format_eid_loc __P((DB_REP *,
 * PUBLIC:    REPMGR_CONNECTION *, char *));
 *
 * Caller must hold mutex.
 */
char * __repmgr_format_eid_loc(DB_REP * db_rep, REPMGR_CONNECTION * conn, char * buffer)
{
	int eid;
	if(conn->type == APP_CONNECTION)
		snprintf(buffer, MAX_SITE_LOC_STRING, "(application channel)");
	else if(conn->type == REP_CONNECTION && IS_VALID_EID(eid = conn->eid))
		__repmgr_format_site_loc(SITE_FROM_EID(eid), buffer);
	else
		snprintf(buffer, MAX_SITE_LOC_STRING, "(unidentified site)");
	return buffer;
}
/*
 * PUBLIC: char *__repmgr_format_site_loc __P((REPMGR_SITE *, char *));
 */
char * __repmgr_format_site_loc(REPMGR_SITE * site, char * buffer)
{
	return __repmgr_format_addr_loc(&site->net_addr, buffer);
}
/*
 * PUBLIC: char *__repmgr_format_addr_loc __P((repmgr_netaddr_t *, char *));
 */
char * __repmgr_format_addr_loc(repmgr_netaddr_t * addr, char * buffer)
{
	snprintf(buffer, MAX_SITE_LOC_STRING, "site %s:%lu", addr->host, (ulong)addr->port);
	return buffer;
}
/*
 * PUBLIC: int __repmgr_repstart __P((ENV *, uint32));
 */
int __repmgr_repstart(ENV * env, uint32 flags)
{
	DBT my_addr;
	int ret;
	/* Include "cdata" in case sending to old-version site. */
	if((ret = __repmgr_prepare_my_addr(env, &my_addr)) != 0)
		return ret;
	ret = __rep_start_int(env, &my_addr, flags);
	__os_free(env, my_addr.data);
	if(ret != 0)
		__db_err(env, ret, DB_STR("3673", "rep_start"));
	return ret;
}
/*
 * PUBLIC: int __repmgr_become_master(ENV *);
 */
int __repmgr_become_master(ENV * env)
{
	DB_REP * db_rep;
	DB_THREAD_INFO * ip;
	DB * dbp;
	DB_TXN * txn;
	REPMGR_SITE * site;
	DBT key_dbt, data_dbt;
	__repmgr_membership_key_args key;
	__repmgr_membership_data_args member_status;
	repmgr_netaddr_t addr;
	uint32 status;
	uint8 data_buf[__REPMGR_MEMBERSHIP_DATA_SIZE];
	uint8 key_buf[MAX_MSG_BUF];
	size_t len;
	uint i;
	int ret, t_ret;
	db_rep = env->rep_handle;
	dbp = NULL;
	txn = NULL;
	/* Examine membership list to see if we have a victim in limbo. */
	LOCK_MUTEX(db_rep->mutex);
	ZERO_LSN(db_rep->limbo_failure);
	ZERO_LSN(db_rep->durable_lsn);
	db_rep->limbo_victim = DB_EID_INVALID;
	db_rep->limbo_resolution_needed = FALSE;
	FOR_EACH_REMOTE_SITE_INDEX(i) {
		site = SITE_FROM_EID(i);
		if(site->membership == SITE_ADDING || site->membership == SITE_DELETING) {
			db_rep->limbo_victim = static_cast<int>(i);
			db_rep->limbo_resolution_needed = TRUE;
			/*
			 * Since there can never be more than one limbo victim,
			 * when we find one we don't have to continue looking
			 * for others.
			 */
			break;
		}
	}
	db_rep->client_intent = FALSE;
	UNLOCK_MUTEX(db_rep->mutex);
	if((ret = __repmgr_repstart(env, DB_REP_MASTER)) != 0)
		return ret;
	if(db_rep->have_gmdb)
		return 0;
	db_rep->member_version_gen = db_rep->region->gen;
	ENV_ENTER(env, ip);
	if((ret = __repmgr_hold_master_role(env, NULL)) != 0)
		goto leave;
retry:
	if((ret = __repmgr_setup_gmdb_op(env, ip, &txn, DB_CREATE)) != 0)
		goto err;
	DB_ASSERT(env, txn != NULL);
	dbp = db_rep->gmdb;
	DB_ASSERT(env, dbp != NULL);
	/* Write the meta-data record. */
	if((ret = __repmgr_set_gm_version(env, ip, txn, 1)) != 0)
		goto err;
	/* Write a record representing each site in the group. */
	for(i = 0; i < db_rep->site_cnt; i++) {
		LOCK_MUTEX(db_rep->mutex);
		site = SITE_FROM_EID(i);
		addr = site->net_addr;
		status = site->membership;
		UNLOCK_MUTEX(db_rep->mutex);
		if(status == 0)
			continue;
		DB_INIT_DBT(key.host, addr.host, sstrlen(addr.host)+1);
		key.port = addr.port;
		ret = __repmgr_membership_key_marshal(env, &key, key_buf, sizeof(key_buf), &len);
		DB_ASSERT(env, ret == 0);
		DB_INIT_DBT(key_dbt, key_buf, len);
		member_status.flags = status;
		__repmgr_membership_data_marshal(env, &member_status, data_buf);
		DB_INIT_DBT(data_dbt, data_buf, __REPMGR_MEMBERSHIP_DATA_SIZE);
		if((ret = __db_put(dbp, ip, txn, &key_dbt, &data_dbt, 0)) != 0)
			goto err;
	}
err:
	if(txn != NULL) {
		if((t_ret = __db_txn_auto_resolve(env, txn, 0, ret)) != 0 && ret == 0)
			ret = t_ret;
		if((t_ret = __repmgr_cleanup_gmdb_op(env, TRUE)) != 0 && ret == 0)
			ret = t_ret;
	}
	if(ret == DB_LOCK_DEADLOCK || ret == DB_LOCK_NOTGRANTED)
		goto retry;
	if((t_ret = __repmgr_rlse_master_role(env)) != 0 && ret == 0)
		ret = t_ret;
leave:
	ENV_LEAVE(env, ip);
	return ret;
}
/*
 * Visits all the connections we know about, performing the desired action.
 * "err_quit" determines whether we give up, or soldier on, in case of an
 * error.
 *
 * PUBLIC: int __repmgr_each_connection __P((ENV *,
 * PUBLIC:     CONNECTION_ACTION, void *, int));
 *
 * !!!
 * Caller must hold mutex.
 */
int __repmgr_each_connection(ENV * env, CONNECTION_ACTION callback, void * info, int err_quit)
{
	DB_REP * db_rep;
	REPMGR_CONNECTION * conn, * next;
	REPMGR_SITE * site;
	uint eid;
	int ret, t_ret;

#define HANDLE_ERROR do { if(err_quit) return (t_ret); if(!ret) ret = t_ret; } while(0)

	db_rep = env->rep_handle;
	ret = 0;

	/*
	 * We might have used TAILQ_FOREACH here, except that in some cases we
	 * need to unlink an element along the way.
	 */
	for(conn = TAILQ_FIRST(&db_rep->connections); conn != NULL; conn = next) {
		next = TAILQ_NEXT(conn, entries);
		if((t_ret = (*callback)(env, conn, info)) != 0)
			HANDLE_ERROR;
	}
	FOR_EACH_REMOTE_SITE_INDEX(eid) {
		site = SITE_FROM_EID(eid);
		if(site->state == SITE_CONNECTED) {
			conn = site->ref.conn;
			if((t_ret = (*callback)(env, conn, info)) != 0)
				HANDLE_ERROR;
		}
		for(conn = TAILQ_FIRST(&site->sub_conns); conn != NULL; conn = next) {
			next = TAILQ_NEXT(conn, entries);
			if((t_ret = (*callback)(env, conn, info)) != 0)
				HANDLE_ERROR;
		}
	}
	return 0;
}
/*
 * Initialize repmgr's portion of the shared region area.  Note that we can't
 * simply get the REP* address from the env as we usually do, because at the
 * time of this call it hasn't been linked into there yet.
 *
 * This function is only called during creation of the region.  If anything
 * fails, our caller will panic and remove the region.  So, if we have any
 * failure, we don't have to clean up any partial allocation.
 *
 * PUBLIC: int __repmgr_open __P((ENV *, void *));
 */
int __repmgr_open(ENV * env, void * rep_)
{
	int ret;
	DB_REP * db_rep = env->rep_handle;
	REP * rep = (REP *)rep_;
	if((ret = __mutex_alloc(env, MTX_REPMGR, 0, &rep->mtx_repmgr)) != 0)
		return ret;
	DB_ASSERT(env, rep->siteinfo_seq == 0 && db_rep->siteinfo_seq == 0);
	rep->siteinfo_off = INVALID_ROFF;
	rep->siteinfo_seq = 0;
	if((ret = __repmgr_share_netaddrs(env, rep, 0, db_rep->site_cnt)) != 0)
		return ret;
	rep->self_eid = db_rep->self_eid;
	rep->perm_policy = db_rep->perm_policy;
	rep->ack_timeout = db_rep->ack_timeout;
	rep->connection_retry_wait = db_rep->connection_retry_wait;
	rep->election_retry_wait = db_rep->election_retry_wait;
	rep->heartbeat_monitor_timeout = db_rep->heartbeat_monitor_timeout;
	rep->heartbeat_frequency = db_rep->heartbeat_frequency;
	return ret;
}
/*
 * Join an existing environment, by setting up our local site info structures
 * from shared network address configuration in the region.
 *
 * As __repmgr_open(), note that we can't simply get the REP* address from the
 * env as we usually do, because at the time of this call it hasn't been linked
 * into there yet.
 *
 * PUBLIC: int __repmgr_join __P((ENV *, void *));
 */
int __repmgr_join(ENV * env, void * rep_)
{
	DB_REP * db_rep;
	REGINFO * infop;
	REP * rep;
	SITEINFO * p;
	REPMGR_SITE * site, temp;
	repmgr_netaddr_t * addrp;
	char * host;
	uint i, j;
	int ret;
	db_rep = env->rep_handle;
	infop = env->reginfo;
	rep = (REP *)rep_;
	ret = 0;
	MUTEX_LOCK(env, rep->mtx_repmgr);
	/*
	 * Merge local and shared lists of remote sites.  Note that the
	 * placement of entries in the shared array must not change.  To
	 * accomplish the merge, pull in entries from the shared list, into the
	 * proper position, shuffling not-yet-resolved local entries if
	 * necessary.  Then add any remaining locally known entries to the
	 * shared list.
	 */
	i = 0;
	if(rep->siteinfo_off != INVALID_ROFF) {
		p = (SITEINFO *)R_ADDR(infop, rep->siteinfo_off);
		/* For each address in the shared list ... */
		for(; i < rep->site_cnt; i++) {
			host = (char *)R_ADDR(infop, p[i].addr.host);
			RPRINT(env, (env, DB_VERB_REPMGR_MISC, "Site %s:%lu found at EID %u", host, (ulong)p[i].addr.port, i));
			/*
			 * Find it in the local list.  Everything before 'i'
			 * already matches the shared list, and is therefore in
			 * the right place.  So we only need to search starting
			 * from 'i'.  When found, local config values will be
			 * used because they are assumed to be "fresher".  But
			 * membership status is not, since this process hasn't
			 * been active (running) yet.
			 */
			for(j = i; j < db_rep->site_cnt; j++) {
				site = &db_rep->sites[j];
				addrp = &site->net_addr;
				if(strcmp(host, addrp->host) == 0 && p[i].addr.port == addrp->port) {
					p[i].config = site->config;
					site->membership = p[i].status;
					break;
				}
			}
			/*
			 * When not found in local list, copy peer values
			 * from shared list.
			 */
			if(j == db_rep->site_cnt) {
				if((ret = __repmgr_new_site(env, &site, host, p[i].addr.port)) != 0)
					goto unlock;
				site->config = p[i].config;
				site->membership = p[i].status;
			}
			DB_ASSERT(env, j < db_rep->site_cnt);
			/* Found or added at 'j', but belongs at 'i': swap. */
			if(i != j) {
				temp = db_rep->sites[j];
				db_rep->sites[j] = db_rep->sites[i];
				db_rep->sites[i] = temp;
				/*
				 * If we're moving the entry that self_eid
				 * points to, then adjust self_eid to match.
				 * For now this is still merely our original,
				 * in-process pointer; we have yet to make sure
				 * it matches the one from shared memory.
				 */
				if(db_rep->self_eid == (int)j)
					db_rep->self_eid = static_cast<int>(i);
			}
		}
	}
	if((ret = __repmgr_share_netaddrs(env, rep, i, db_rep->site_cnt)) != 0)
		goto unlock;
	if(db_rep->self_eid == DB_EID_INVALID)
		db_rep->self_eid = rep->self_eid;
	else if(rep->self_eid == DB_EID_INVALID)
		rep->self_eid = db_rep->self_eid;
	else if(db_rep->self_eid != rep->self_eid) {
		__db_errx(env, DB_STR("3674", "A mismatching local site address has been set in the environment"));
		ret = EINVAL;
		goto unlock;
	}
	db_rep->siteinfo_seq = rep->siteinfo_seq;
unlock:
	MUTEX_UNLOCK(env, rep->mtx_repmgr);
	return ret;
}
/*
 * PUBLIC: int __repmgr_env_refresh __P((ENV *env));
 */
int __repmgr_env_refresh(ENV * env)
{
	int ret = F_ISSET(env, ENV_PRIVATE) ? __mutex_free(env, &env->rep_handle->region->mtx_repmgr) : 0;
	return ret;
}
/*
 * Copies new remote site information from the indicated private array slots
 * into the shared region.  The corresponding shared array slots do not exist
 * yet; they must be allocated.
 *
 * PUBLIC: int __repmgr_share_netaddrs __P((ENV *, void *, uint, uint));
 *
 * !!! The rep pointer is passed, because it may not yet have been installed
 * into the env handle.
 *
 * !!! Assumes caller holds mtx_repmgr lock.
 */
int __repmgr_share_netaddrs(ENV * env, void * rep_, uint start, uint limit)
{
	DB_REP * db_rep;
	REP * rep;
	REGINFO * infop;
	REGENV * renv;
	SITEINFO * orig, * shared_array;
	char * host, * hostbuf;
	size_t sz;
	uint i, n;
	int eid, ret, touched;

	db_rep = env->rep_handle;
	infop = env->reginfo;
	renv = (REGENV *)infop->primary;
	rep = (REP *)rep_;
	ret = 0;
	touched = FALSE;

	MUTEX_LOCK(env, renv->mtx_regenv);

	for(i = start; i < limit; i++) {
		if(rep->site_cnt >= rep->site_max) {
			/* Table is full, we need more space. */
			if(rep->siteinfo_off == INVALID_ROFF) {
				n = INITIAL_SITES_ALLOCATION;
				sz = n*sizeof(SITEINFO);
				if((ret = __env_alloc(infop, sz, &shared_array)) != 0)
					goto out;
			}
			else {
				n = 2*rep->site_max;
				sz = n*sizeof(SITEINFO);
				if((ret = __env_alloc(infop, sz, &shared_array)) != 0)
					goto out;
				orig = (SITEINFO *)R_ADDR(infop, rep->siteinfo_off);
				memcpy(shared_array, orig, sizeof(SITEINFO)*rep->site_cnt);
				__env_alloc_free(infop, orig);
			}
			rep->siteinfo_off = R_OFFSET(infop, shared_array);
			rep->site_max = n;
		}
		else
			shared_array = (SITEINFO *)R_ADDR(infop, rep->siteinfo_off);
		DB_ASSERT(env, rep->site_cnt < rep->site_max && rep->siteinfo_off != INVALID_ROFF);
		host = db_rep->sites[i].net_addr.host;
		sz = sstrlen(host)+1;
		if((ret = __env_alloc(infop, sz, &hostbuf)) != 0)
			goto out;
		eid = (int)rep->site_cnt++;
		strcpy(hostbuf, host);
		shared_array[eid].addr.host = R_OFFSET(infop, hostbuf);
		shared_array[eid].addr.port = db_rep->sites[i].net_addr.port;
		shared_array[eid].config = db_rep->sites[i].config;
		shared_array[eid].status = db_rep->sites[i].membership;
		RPRINT(env, (env, DB_VERB_REPMGR_MISC, "EID %d is assigned for site %s:%lu", eid, host, (ulong)shared_array[eid].addr.port));
		touched = TRUE;
	}
out:
	if(touched)
		db_rep->siteinfo_seq = ++rep->siteinfo_seq;
	MUTEX_UNLOCK(env, renv->mtx_regenv);
	return ret;
}
/*
 * Copy into our local list any newly added/changed remote site
 * configuration information.
 *
 * !!! Caller must hold db_rep->mutex and mtx_repmgr locks.
 *
 * PUBLIC: int __repmgr_copy_in_added_sites(ENV *);
 */
int __repmgr_copy_in_added_sites(ENV * env)
{
	DB_REP * db_rep;
	REP * rep;
	REGINFO * infop;
	SITEINFO * base, * p;
	REPMGR_SITE * site;
	char * host;
	int ret;
	uint i;
	db_rep = env->rep_handle;
	rep = db_rep->region;
	if(rep->siteinfo_off == INVALID_ROFF)
		goto out;
	infop = env->reginfo;
	base = (SITEINFO *)R_ADDR(infop, rep->siteinfo_off);
	/* Create private array slots for new sites. */
	for(i = db_rep->site_cnt; i < rep->site_cnt; i++) {
		p = &base[i];
		host = (char *)R_ADDR(infop, p->addr.host);
		if((ret = __repmgr_new_site(env, &site, host, p->addr.port)) != 0)
			return ret;
		RPRINT(env, (env, DB_VERB_REPMGR_MISC, "Site %s:%lu found at EID %u", host, (ulong)p->addr.port, i));
	}
	/* Make sure info is up to date for all sites, old and new. */
	for(i = 0; i < db_rep->site_cnt; i++) {
		p = &base[i];
		site = SITE_FROM_EID(i);
		site->config = p->config;
		site->membership = p->status;
	}
out:
	/*
	 * We always make sure our local list has been brought up to date with
	 * the shared list before adding to the local list (except before env
	 * open of course).  So here there should be nothing on our local list
	 * not yet in shared memory.
	 */
	DB_ASSERT(env, db_rep->site_cnt == rep->site_cnt);
	db_rep->siteinfo_seq = rep->siteinfo_seq;
	return 0;
}

/*
 * Initialize a range of sites newly added to our site list array.  Process each
 * array entry in the range from <= x < limit.  Passing from >= limit is
 * allowed, and is effectively a no-op.
 *
 * PUBLIC: int __repmgr_init_new_sites __P((ENV *, uint, uint));
 *
 * !!! Assumes caller holds db_rep->mutex.
 */
int __repmgr_init_new_sites(ENV * env, uint from, uint limit)
{
	REPMGR_SITE * site;
	uint i;
	int ret;
	DB_REP * db_rep = env->rep_handle;
	if(db_rep->selector == NULL)
		return 0;
	for(i = from; i < limit; i++) {
		site = SITE_FROM_EID(i);
		if(site->membership == SITE_PRESENT && (ret = __repmgr_schedule_connection_attempt(env, i, TRUE)) != 0)
			return ret;
	}
	return 0;
}
/*
 * PUBLIC: int __repmgr_failchk(ENV *);
 */
int __repmgr_failchk(ENV * env)
{
	DB_ENV * dbenv;
	DB_REP * db_rep;
	REP * rep;
	db_threadid_t unused;
	dbenv = env->dbenv;
	db_rep = env->rep_handle;
	rep = db_rep->region;
	COMPQUIET(unused, 0);
	MUTEX_LOCK(env, rep->mtx_repmgr);
	/*
	 * Check to see if the main (listener) replication process may have died
	 * without cleaning up the flag.  If so, we only have to clear it, and
	 * another process should then be able to come along and become the
	 * listener.  So in either case we can return success.
	 */
	if(rep->listener != 0 && !dbenv->is_alive(dbenv, rep->listener, unused, DB_MUTEX_PROCESS_ONLY))
		rep->listener = 0;
	MUTEX_UNLOCK(env, rep->mtx_repmgr);
	return 0;
}
/*
 * PUBLIC: int __repmgr_master_is_known(ENV *);
 */
int __repmgr_master_is_known(ENV * env)
{
	DB_REP * db_rep = env->rep_handle;
	REP * rep = db_rep->region;
	int master = rep->master_id;
	/*
	 * We are the master, or we know of a master and have a healthy
	 * connection to it.
	 */
	return master == db_rep->self_eid || __repmgr_master_connection(env) != NULL;
}
/*
 * PUBLIC: int __repmgr_stable_lsn(ENV *, DB_LSN *);
 *
 * This function may be called before any of repmgr's threads have
 * been started.  This code must not be called before env open.
 * Currently that is impossible since its only caller is log_archive
 * which itself cannot be called before env_open.
 */
int __repmgr_stable_lsn(ENV * env, DB_LSN * stable_lsn)
{
	DB_LSN min_lsn;
	DB_REP * db_rep;
	REP * rep;
	REPMGR_SITE * site;
	uint eid;
	db_rep = env->rep_handle;
	rep = db_rep->region;
	ZERO_LSN(min_lsn);
	LOCK_MUTEX(db_rep->mutex);
	FOR_EACH_REMOTE_SITE_INDEX(eid) {
		site = SITE_FROM_EID(eid);
		/*
		 * Record the smallest ack'ed LSN from all connected sites.
		 * If we're a client, ignore the master because the master
		 * does not maintain nor send out its repmgr perm LSN in
		 * this way.
		 */
		if((int)eid == rep->master_id)
			continue;
		if(IS_SITE_AVAILABLE(site) && !IS_ZERO_LSN(site->max_ack) && (IS_ZERO_LSN(min_lsn) || LOG_COMPARE(&site->max_ack, &min_lsn) < 0))
			min_lsn = site->max_ack;
	}
	UNLOCK_MUTEX(db_rep->mutex);
	if(!IS_ZERO_LSN(min_lsn) && LOG_COMPARE(&min_lsn, stable_lsn) < 0)
		*stable_lsn = min_lsn;
	RPRINT(env, (env, DB_VERB_REPMGR_MISC, "Repmgr_stable_lsn: Returning stable_lsn[%lu][%lu]", (ulong)stable_lsn->file, (ulong)stable_lsn->Offset_));
	return 0;
}
/*
 * PUBLIC: int __repmgr_send_sync_msg __P((ENV *, REPMGR_CONNECTION *,
 * PUBLIC:     uint32, uint8 *, uint32));
 */
int __repmgr_send_sync_msg(ENV * env, REPMGR_CONNECTION * conn, uint32 type, uint8 * buf, uint32 len)
{
	REPMGR_IOVECS iovecs;
	__repmgr_msg_hdr_args msg_hdr;
	uint8 hdr_buf[__REPMGR_MSG_HDR_SIZE];
	size_t unused;
	msg_hdr.type = REPMGR_OWN_MSG;
	REPMGR_OWN_BUF_SIZE(msg_hdr) = len;
	REPMGR_OWN_MSG_TYPE(msg_hdr) = type;
	__repmgr_msg_hdr_marshal(env, &msg_hdr, hdr_buf);
	__repmgr_iovec_init(&iovecs);
	__repmgr_add_buffer(&iovecs, hdr_buf, __REPMGR_MSG_HDR_SIZE);
	if(len > 0)
		__repmgr_add_buffer(&iovecs, buf, len);
	return __repmgr_write_iovecs(env, conn, &iovecs, &unused);
}
/*
 * Produce a membership list from the known info currently in memory.
 *
 * PUBLIC: int __repmgr_marshal_member_list __P((ENV *, uint8 **, size_t *));
 *
 * Caller must hold mutex.
 */
int __repmgr_marshal_member_list(ENV * env, uint8 ** bufp, size_t * lenp)
{
	DB_REP * db_rep;
	REP * rep;
	REPMGR_SITE * site;
	__repmgr_membr_vers_args membr_vers;
	__repmgr_site_info_args site_info;
	uint8 * buf, * p;
	size_t bufsize, len;
	uint i;
	int ret;

	db_rep = env->rep_handle;
	rep = db_rep->region;

	/* Compute a (generous) upper bound on needed buffer size. */
	bufsize = __REPMGR_MEMBR_VERS_SIZE+db_rep->site_cnt*(__REPMGR_SITE_INFO_SIZE+MAXHOSTNAMELEN+1);
	if((ret = __os_malloc(env, bufsize, &buf)) != 0)
		return ret;
	p = buf;
	membr_vers.version = db_rep->membership_version;
	membr_vers.gen = rep->gen;
	__repmgr_membr_vers_marshal(env, &membr_vers, p);
	p += __REPMGR_MEMBR_VERS_SIZE;
	for(i = 0; i < db_rep->site_cnt; i++) {
		site = SITE_FROM_EID(i);
		if(site->membership == 0)
			continue;
		site_info.host.data = site->net_addr.host;
		site_info.host.size = (uint32)sstrlen(site->net_addr.host)+1;
		site_info.port = site->net_addr.port;
		site_info.flags = site->membership;

		ret = __repmgr_site_info_marshal(env, &site_info, p, (size_t)(&buf[bufsize]-p), &len);
		DB_ASSERT(env, ret == 0);
		p += len;
	}
	len = (size_t)(p-buf);
	*bufp = buf;
	*lenp = len;
	DB_ASSERT(env, ret == 0);
	return 0;
}
/*
 * Produce a membership list by reading the database.
 */
static int read_gmdb(ENV * env, DB_THREAD_INFO * ip, uint8 ** bufp, size_t * lenp)
{
	DB_TXN * txn;
	DB * dbp;
	DBC * dbc;
	DBT key_dbt, data_dbt;
	__repmgr_membership_key_args key;
	__repmgr_membership_data_args member_status;
	__repmgr_member_metadata_args metadata;
	__repmgr_membr_vers_args membr_vers;
	__repmgr_site_info_args site_info;
	uint8 data_buf[__REPMGR_MEMBERSHIP_DATA_SIZE];
	uint8 key_buf[MAX_MSG_BUF];
	uint8 metadata_buf[__REPMGR_MEMBER_METADATA_SIZE];
	char * host;
	size_t bufsize, len;
	uint8 * buf, * p;
	uint32 gen;
	int ret, t_ret;

	txn = NULL;
	dbp = NULL;
	dbc = NULL;
	buf = NULL;
	COMPQUIET(len, 0);
	if((ret = __rep_get_datagen(env, &gen)) != 0)
		return ret;
	if((ret = __txn_begin(env, ip, NULL, &txn, DB_IGNORE_LEASE)) != 0)
		goto err;
	if((ret = __rep_open_sysdb(env, ip, txn, REPMEMBERSHIP, 0, &dbp)) != 0)
		goto err;
	if((ret = __db_cursor(dbp, ip, txn, &dbc, 0)) != 0)
		goto err;
	memzero(&key_dbt, sizeof(key_dbt));
	key_dbt.data = key_buf;
	key_dbt.ulen = sizeof(key_buf);
	F_SET(&key_dbt, DB_DBT_USERMEM);
	memzero(&data_dbt, sizeof(data_dbt));
	data_dbt.data = metadata_buf;
	data_dbt.ulen = sizeof(metadata_buf);
	F_SET(&data_dbt, DB_DBT_USERMEM);
	/* Get metadata record, make sure key looks right. */
	if((ret = __dbc_get(dbc, &key_dbt, &data_dbt, DB_NEXT)) != 0)
		goto err;
	ret = __repmgr_membership_key_unmarshal(env, &key, key_buf, key_dbt.size, 0);
	DB_ASSERT(env, ret == 0);
	DB_ASSERT(env, key.host.size == 0);
	DB_ASSERT(env, key.port == 0);
	ret = __repmgr_member_metadata_unmarshal(env, &metadata, metadata_buf, data_dbt.size, 0);
	DB_ASSERT(env, ret == 0);
	DB_ASSERT(env, metadata.format == REPMGR_GMDB_FMT_VERSION);
	DB_ASSERT(env, metadata.version > 0);

	bufsize = 1000; /* Initial guess. */
	if((ret = __os_malloc(env, bufsize, &buf)) != 0)
		goto err;
	membr_vers.version = metadata.version;
	membr_vers.gen = gen;
	__repmgr_membr_vers_marshal(env, &membr_vers, buf);
	p = &buf[__REPMGR_MEMBR_VERS_SIZE];

	data_dbt.data = data_buf;
	data_dbt.ulen = sizeof(data_buf);
	while((ret = __dbc_get(dbc, &key_dbt, &data_dbt, DB_NEXT)) == 0) {
		ret = __repmgr_membership_key_unmarshal(env, &key, key_buf, key_dbt.size, 0);
		DB_ASSERT(env, ret == 0);
		DB_ASSERT(env, key.host.size <= MAXHOSTNAMELEN+1 && key.host.size > 1);
		host = (char *)key.host.data;
		DB_ASSERT(env, host[key.host.size-1] == '\0');
		DB_ASSERT(env, key.port > 0);
		ret = __repmgr_membership_data_unmarshal(env, &member_status, data_buf, data_dbt.size, 0);
		DB_ASSERT(env, ret == 0);
		DB_ASSERT(env, member_status.flags != 0);
		site_info.host = key.host;
		site_info.port = key.port;
		site_info.flags = member_status.flags;
		if((ret = __repmgr_site_info_marshal(env, &site_info, p, (size_t)(&buf[bufsize]-p), &len)) == ENOMEM) {
			bufsize *= 2;
			len = (size_t)(p-buf);
			if((ret = __os_realloc(env, bufsize, &buf)) != 0)
				goto err;
			p = &buf[len];
			ret = __repmgr_site_info_marshal(env, &site_info, p, (size_t)(&buf[bufsize]-p), &len);
			DB_ASSERT(env, ret == 0);
		}
		p += len;
	}
	len = (size_t)(p-buf);
	if(ret == DB_NOTFOUND)
		ret = 0;
err:
	if(dbc && (t_ret = __dbc_close(dbc)) != 0 && ret == 0)
		ret = t_ret;
	if(dbp && (t_ret = __db_close(dbp, txn, DB_NOSYNC)) != 0 && ret == 0)
		ret = t_ret;
	if(txn && (t_ret = __db_txn_auto_resolve(env, txn, 0, ret)) != 0 && ret == 0)
		ret = t_ret;
	if(!ret) {
		*bufp = buf;
		*lenp = len;
	}
	else
		__os_free(env, buf);
	return ret;
}
/*
 * Refresh our sites array from the given membership list.
 *
 * PUBLIC: int __repmgr_refresh_membership __P((ENV *,
 * PUBLIC:     uint8 *, size_t));
 */
int __repmgr_refresh_membership(ENV * env, uint8 * buf, size_t len)
{
	DB_REP * db_rep;
	REPMGR_SITE * site;
	__repmgr_membr_vers_args membr_vers;
	__repmgr_site_info_args site_info;
	char * host;
	uint8 * p;
	uint16 port;
	uint32 i, n;
	int eid, ret;
	db_rep = env->rep_handle;
	/*
	 * Membership list consists of membr_vers followed by a number of
	 * site_info structs.
	 */
	ret = __repmgr_membr_vers_unmarshal(env, &membr_vers, buf, len, &p);
	DB_ASSERT(env, ret == 0);
	if(db_rep->finished)
		return 0;
	/* Ignore obsolete versions. */
	if(__repmgr_gmdb_version_cmp(env, membr_vers.gen, membr_vers.version) <= 0)
		return 0;
	LOCK_MUTEX(db_rep->mutex);

	db_rep->membership_version = membr_vers.version;
	db_rep->member_version_gen = membr_vers.gen;

	for(i = 0; i < db_rep->site_cnt; i++)
		F_CLR(SITE_FROM_EID(i), SITE_TOUCHED);
	for(n = 0; p < &buf[len]; ++n) {
		ret = __repmgr_site_info_unmarshal(env, &site_info, p, (size_t)(&buf[len]-p), &p);
		DB_ASSERT(env, ret == 0);
		host = (char *)site_info.host.data;
		DB_ASSERT(env, (uint8 *)site_info.host.data+site_info.host.size <= p);
		host[site_info.host.size-1] = '\0';
		port = site_info.port;
		if((ret = __repmgr_set_membership(env, host, port, site_info.flags)) != 0)
			goto err;
		if((ret = __repmgr_find_site(env, host, port, &eid)) != 0)
			goto err;
		F_SET(SITE_FROM_EID(eid), SITE_TOUCHED);
	}
	ret = __rep_set_nsites_int(env, n);
	DB_ASSERT(env, ret == 0);

	/* Scan "touched" flags so as to notice sites that have been removed. */
	for(i = 0; i < db_rep->site_cnt; i++) {
		site = SITE_FROM_EID(i);
		if(F_ISSET(site, SITE_TOUCHED))
			continue;
		host = site->net_addr.host;
		port = site->net_addr.port;
		if((ret = __repmgr_set_membership(env, host, port, 0)) != 0)
			goto err;
	}
err:
	UNLOCK_MUTEX(db_rep->mutex);
	return ret;
}
/*
 * PUBLIC: int __repmgr_reload_gmdb(ENV *);
 */
int __repmgr_reload_gmdb(ENV * env)
{
	DB_THREAD_INFO * ip;
	uint8 * buf;
	size_t len;
	int ret;
	ENV_ENTER(env, ip);
	if((ret = read_gmdb(env, ip, &buf, &len)) == 0) {
		env->rep_handle->have_gmdb = TRUE;
		ret = __repmgr_refresh_membership(env, buf, len);
		__os_free(env, buf);
	}
	ENV_LEAVE(env, ip);
	return ret;
}
/*
 * Return 1, 0, or -1, as the given gen/version combination is >, =, or < our
 * currently known version.
 *
 * PUBLIC: int __repmgr_gmdb_version_cmp __P((ENV *, uint32, uint32));
 */
int __repmgr_gmdb_version_cmp(ENV * env, uint32 gen, uint32 version)
{
	uint32 g, v;
	DB_REP * db_rep = env->rep_handle;
	g = db_rep->member_version_gen;
	v = db_rep->membership_version;
	if(gen == g)
		return version == v ? 0 : (version < v ? -1 : 1);
	return gen < g ? -1 : 1;
}
/*
 * PUBLIC: int __repmgr_init_save(ENV *, DBT *);
 */
int __repmgr_init_save(ENV * env, DBT * dbt)
{
	DB_REP * db_rep;
	uint8 * buf;
	size_t len;
	int ret;
	db_rep = env->rep_handle;
	LOCK_MUTEX(db_rep->mutex);
	if(db_rep->site_cnt == 0) {
		dbt->data = NULL;
		dbt->size = 0;
		ret = 0;
	}
	else if((ret = __repmgr_marshal_member_list(env, &buf, &len)) == 0) {
		dbt->data = buf;
		dbt->size = (uint32)len;
	}
	UNLOCK_MUTEX(db_rep->mutex);
	return ret;
}
/*
 * PUBLIC: int __repmgr_init_restore(ENV *, DBT *);
 */
int __repmgr_init_restore(ENV * env, DBT * dbt)
{
	DB_REP * db_rep = env->rep_handle;
	db_rep->restored_list = (uint8 *)dbt->data;
	db_rep->restored_list_length = dbt->size;
	return 0;
}

/*
 * Generates an internal request for a deferred operation, to be performed on a
 * separate thread (conveniently, a message-processing thread).
 *
 * PUBLIC: int __repmgr_defer_op __P((ENV *, uint32));
 *
 * Caller should hold mutex.
 */
int __repmgr_defer_op(ENV * env, uint32 op)
{
	REPMGR_MESSAGE * msg;
	int ret;
	/*
	 * Overload REPMGR_MESSAGE to convey the type of operation being
	 * requested.  For now "op" is all we need; plenty of room for expansion
	 * if needed in the future.
	 *
	 * Leave msg->v.gmdb_msg.conn NULL to show no conn to be cleaned up.
	 */
	if((ret = __os_calloc(env, 1, sizeof(*msg), &msg)) != 0)
		return ret;
	msg->msg_hdr.type = REPMGR_OWN_MSG;
	REPMGR_OWN_MSG_TYPE(msg->msg_hdr) = op;
	ret = __repmgr_queue_put(env, msg);
	return ret;
}
/*
 * PUBLIC: void __repmgr_fire_conn_err_event __P((ENV *,
 * PUBLIC:     REPMGR_CONNECTION *, int));
 */
void __repmgr_fire_conn_err_event(ENV * env, REPMGR_CONNECTION * conn, int err)
{
	DB_REPMGR_CONN_ERR info;
	DB_REP * db_rep = env->rep_handle;
	if(conn->type == REP_CONNECTION && IS_VALID_EID(conn->eid)) {
		__repmgr_print_conn_err(env, &SITE_FROM_EID(conn->eid)->net_addr, err);
		info.eid = conn->eid;
		info.error = err;
		DB_EVENT(env, DB_EVENT_REP_CONNECT_BROKEN, &info);
	}
}
/*
 * PUBLIC: void __repmgr_print_conn_err __P((ENV *, repmgr_netaddr_t *, int));
 */
void __repmgr_print_conn_err(ENV * env, repmgr_netaddr_t * netaddr, int err)
{
	SITE_STRING_BUFFER site_loc_buf;
	char msgbuf[200]; /* Arbitrary size. */
	__repmgr_format_addr_loc(netaddr, site_loc_buf);
	/* TCP/IP sockets API convention: 0 indicates "end-of-file". */
	if(err == 0)
		RPRINT(env, (env, DB_VERB_REPMGR_MISC, "EOF on connection to %s", site_loc_buf));
	else
		RPRINT(env, (env, DB_VERB_REPMGR_MISC, "`%s' (%d) on connection to %s", __os_strerror(err, msgbuf, sizeof(msgbuf)), err, site_loc_buf));
}

/*
 * Change role from master to client, but if a GMDB operation is in progress,
 * wait for it to finish first.
 *
 * PUBLIC: int __repmgr_become_client(ENV *);
 */
int __repmgr_become_client(ENV * env)
{
	int ret;
	DB_REP * db_rep = env->rep_handle;
	LOCK_MUTEX(db_rep->mutex);
	if((ret = __repmgr_await_gmdbop(env)) == 0)
		db_rep->client_intent = TRUE;
	UNLOCK_MUTEX(db_rep->mutex);
	return ret == 0 ? __repmgr_repstart(env, DB_REP_CLIENT) : ret;
}
/*
 * Looks up a site from our local (in-process) list, or returns NULL if not
 * found.
 *
 * PUBLIC: REPMGR_SITE *__repmgr_lookup_site __P((ENV *, const char *, uint));
 */
REPMGR_SITE * __repmgr_lookup_site(ENV * env, const char * host, uint port)
{
	REPMGR_SITE * site;
	uint i;
	DB_REP * db_rep = env->rep_handle;
	for(i = 0; i < db_rep->site_cnt; i++) {
		site = &db_rep->sites[i];
		if(strcmp(site->net_addr.host, host) == 0 && site->net_addr.port == port)
			return site;
	}
	return NULL;
}

/*
 * Look up a site, or add it if it doesn't already exist.
 *
 * Caller must hold db_rep mutex and be within ENV_ENTER context, unless this is
 * a pre-open call.
 *
 * PUBLIC: int __repmgr_find_site __P((ENV *, const char *, uint, int *));
 */
int __repmgr_find_site(ENV * env, const char * host, uint port, int * eidp)
{
	REP * rep;
	REPMGR_SITE * site;
	int eid;
	DB_REP * db_rep = env->rep_handle;
	int ret = 0;
	if(REP_ON(env)) {
		rep = db_rep->region;
		MUTEX_LOCK(env, rep->mtx_repmgr);
		ret = get_eid(env, host, port, &eid);
		MUTEX_UNLOCK(env, rep->mtx_repmgr);
	}
	else {
		if((site = __repmgr_lookup_site(env, host, port)) == NULL && (ret = __repmgr_new_site(env, &site, host, port)) != 0)
			return ret;
		eid = EID_FROM_SITE(site);
	}
	if(!ret)
		*eidp = eid;
	return ret;
}
/*
 * Get the EID of the named remote site, even if it means creating a new entry
 * in our table if it doesn't already exist.
 *
 * Caller must hold both db_rep mutex and mtx_repmgr.
 */
static int get_eid(ENV * env, const char * host, uint port, int * eidp)
{
	REPMGR_SITE * site;
	int eid, ret;
	DB_REP * db_rep = env->rep_handle;
	REP * rep = db_rep->region;
	if((ret = __repmgr_copy_in_added_sites(env)) != 0)
		return ret;
	if((site = __repmgr_lookup_site(env, host, port)) == NULL) {
		/*
		 * Store both locally and in shared region.
		 */
		if((ret = __repmgr_new_site(env, &site, host, port)) != 0)
			return ret;
		eid = EID_FROM_SITE(site);
		DB_ASSERT(env, (uint)eid == db_rep->site_cnt-1);
		if((ret = __repmgr_share_netaddrs(env, rep, (uint)eid, db_rep->site_cnt)) == 0) {
			/* Show that a change was made. */
			db_rep->siteinfo_seq = ++rep->siteinfo_seq;
		}
		else {
			/*
			 * Rescind the local slot we just added, so that we at
			 * least keep the two lists in sync.
			 */
			db_rep->site_cnt--;
			__repmgr_cleanup_netaddr(env, &site->net_addr);
		}
	}
	else
		eid = EID_FROM_SITE(site);
	if(!ret)
		*eidp = eid;
	return ret;
}
/*
 * Sets the named remote site's group membership status to the given value,
 * creating it first if it doesn't already exist.  Adjusts connections
 * accordingly.
 *
 * PUBLIC: int __repmgr_set_membership __P((ENV *,
 * PUBLIC:     const char *, uint, uint32));
 *
 * Caller must host db_rep mutex, and be in ENV_ENTER context.
 */
int __repmgr_set_membership(ENV * env, const char * host, uint port, uint32 status)
{
	DB_REP * db_rep;
	REP * rep;
	REGINFO * infop;
	REPMGR_SITE * site;
	SITEINFO * sites;
	uint32 orig;
	int eid, ret;

	db_rep = env->rep_handle;
	rep = db_rep->region;
	infop = env->reginfo;

	COMPQUIET(orig, 0);
	COMPQUIET(site, 0);
	DB_ASSERT(env, REP_ON(env));

	MUTEX_LOCK(env, rep->mtx_repmgr);
	if((ret = get_eid(env, host, port, &eid)) == 0) {
		site = SITE_FROM_EID(eid);
		orig = site->membership;
		sites = (SITEINFO *)R_ADDR(infop, rep->siteinfo_off);
		RPRINT(env, (env, DB_VERB_REPMGR_MISC, "set membership for %s:%lu %lu (was %lu)", host, (ulong)port, (ulong)status, (ulong)orig));
		if(status != sites[eid].status) {
			/*
			 * Show that a change is occurring.
			 *
			 * The call to get_eid() might have also bumped the
			 * sequence number, and since this is all happening
			 * within a single critical section it would be possible
			 * to avoid "wasting" a sequence number.  But it's
			 * hardly worth the trouble and mental complexity: the
			 * sequence number counts changes that occur within an
			 * env region lifetime, so there should be plenty.
			 * We'll run out of membership DB version numbers long
			 * before this becomes a problem.
			 */
			db_rep->siteinfo_seq = ++rep->siteinfo_seq;
		}
		/* Set both private and shared copies of the info. */
		site->membership = status;
		sites[eid].status = status;
	}
	MUTEX_UNLOCK(env, rep->mtx_repmgr);
	/*
	 * If our notion of the site's membership changed, we may need to create
	 * or kill a connection.
	 */
	if(ret == 0 && !db_rep->finished && SELECTOR_RUNNING(db_rep)) {
		if(eid == db_rep->self_eid && status != SITE_PRESENT)
			ret = DB_DELETED;
		else if(orig != SITE_PRESENT && status == SITE_PRESENT && site->state == SITE_IDLE) {
			/*
			 * Here we might have just joined a group, or we might
			 * be an existing site and we've just learned of another
			 * site joining the group.  In the former case, we
			 * certainly want to connect right away; in the later
			 * case it might be better to wait, because the new site
			 * probably isn't quite ready to accept our connection.
			 * But deciding which case we're in here would be messy,
			 * so for now we just keep it simple and always try
			 * connecting immediately.  The resulting connection
			 * failure shouldn't hurt anything, because we'll just
			 * naturally try again later.
			 */
			ret = __repmgr_schedule_connection_attempt(env, (uint)eid, TRUE);
			if(eid != db_rep->self_eid)
				DB_EVENT(env, DB_EVENT_REP_SITE_ADDED, &eid);
		}
		else if(orig != 0 && status == 0)
			DB_EVENT(env, DB_EVENT_REP_SITE_REMOVED, &eid);
		/*
		 * Callers are responsible for adjusting nsites, even though in
		 * a way it would make sense to do it here.  It's awkward to do
		 * it here at start-up/join time, when we load up starting from
		 * an empty array.  Then we would get rep_set_nsites()
		 * repeatedly, and when leases were in use that would thrash the
		 * lease table adjustment.
		 */
	}
	return ret;
}
/*
 * PUBLIC: int __repmgr_bcast_parm_refresh(ENV *);
 */
int __repmgr_bcast_parm_refresh(ENV * env)
{
	DB_REP * db_rep;
	REP * rep;
	__repmgr_parm_refresh_args parms;
	uint8 buf[__REPMGR_PARM_REFRESH_SIZE];
	int ret;
	DB_ASSERT(env, REP_ON(env));
	db_rep = env->rep_handle;
	rep = db_rep->region;
	LOCK_MUTEX(db_rep->mutex);
	parms.ack_policy = (uint32)rep->perm_policy;
	if(rep->priority == 0)
		parms.flags = 0;
	else
		parms.flags = SITE_ELECTABLE;
	__repmgr_parm_refresh_marshal(env, &parms, buf);
	ret = __repmgr_bcast_own_msg(env,
		REPMGR_PARM_REFRESH, buf, __REPMGR_PARM_REFRESH_SIZE);
	UNLOCK_MUTEX(db_rep->mutex);
	return ret;
}
/*
 * PUBLIC: int __repmgr_chg_prio __P((ENV *, uint32, uint32));
 */
int __repmgr_chg_prio(ENV * env, uint32 prev, uint32 cur)
{
	if((prev == 0 && cur != 0) || (prev != 0 && cur == 0))
		return __repmgr_bcast_parm_refresh(env);
	return 0;
}

/*
 * PUBLIC: int __repmgr_bcast_own_msg __P((ENV *,
 * PUBLIC:     uint32, uint8 *, size_t));
 *
 * Caller must hold mutex.
 */
int __repmgr_bcast_own_msg(ENV * env, uint32 type, uint8 * buf, size_t len)
{
	REPMGR_CONNECTION * conn;
	REPMGR_SITE * site;
	int ret;
	uint i;
	DB_REP * db_rep = env->rep_handle;
	if(!SELECTOR_RUNNING(db_rep))
		return 0;
	FOR_EACH_REMOTE_SITE_INDEX(i) {
		site = SITE_FROM_EID(i);
		if(site->state != SITE_CONNECTED)
			continue;
		conn = site->ref.conn;
		if(conn->state != CONN_READY)
			continue;
		if((ret = __repmgr_send_own_msg(env, conn, type, buf, (uint32)len)) != 0 && (ret = __repmgr_bust_connection(env, conn)) != 0)
			return ret;
	}
	return 0;
}
