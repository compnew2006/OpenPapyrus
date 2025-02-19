/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005, 2011 Oracle and/or its affiliates.  All rights reserved.
 *
 * $Id$
 */
#include "db_config.h"
#include "db_int.h"
#pragma hdrstop
#ifndef HAVE_SIMPLE_THREAD_TYPE
 // @sobolev // @v9.5.5 #include "dbinc/db_page.h"
 // @sobolev // @v9.5.5 #include "dbinc/hash.h"                /* Needed for call to __ham_func5. */
#endif

static int __env_in_api(ENV *);
static void __env_clear_state(ENV *);
/*
 * __env_failchk_pp --
 *	ENV->failchk pre/post processing.
 *
 * PUBLIC: int __env_failchk_pp(DB_ENV *, uint32);
 */
int __env_failchk_pp(DB_ENV * dbenv, uint32 flags)
{
	DB_THREAD_INFO * ip;
	int ret;
	ENV * env = dbenv->env;
	ENV_ILLEGAL_BEFORE_OPEN(env, "DB_ENV->failchk");
	/*
	 * ENV->failchk requires self and is-alive functions.  We
	 * have a default self function, but no is-alive function.
	 */
	if(!ALIVE_ON(env)) {
		__db_errx(env, DB_STR("1503", "DB_ENV->failchk requires DB_ENV->is_alive be configured"));
		return EINVAL;
	}
	if(flags != 0)
		return __db_ferr(env, "DB_ENV->failchk", 0);
	ENV_ENTER(env, ip);
	FAILCHK_THREAD(env, ip); /* mark as failchk thread */
	ret = __env_failchk_int(dbenv);
	ENV_LEAVE(env, ip);
	return ret;
}
/*
 * __env_failchk_int --
 *	Process the subsystem failchk routines
 *
 * PUBLIC: int __env_failchk_int(DB_ENV *);
 */
int __env_failchk_int(DB_ENV * dbenv)
{
	int ret;
	ENV * env = dbenv->env;
	F_SET(dbenv, DB_ENV_FAILCHK);
	/*
	 * We check for dead threads in the API first as this would be likely
	 * to hang other things we try later, like locks and transactions.
	 */
	if((ret = __env_in_api(env)) != 0)
		goto err;
	if(LOCKING_ON(env) && (ret = __lock_failchk(env)) != 0)
		goto err;
	if(TXN_ON(env) && ((ret = __txn_failchk(env)) != 0 || (ret = __dbreg_failchk(env)) != 0))
		goto err;
#ifdef HAVE_REPLICATION_THREADS
	if(REP_ON(env) && (ret = __repmgr_failchk(env)) != 0)
		goto err;
#endif
	/* Mark any dead blocked threads as dead. */
	__env_clear_state(env);
#ifdef HAVE_MUTEX_SUPPORT
	ret = __mut_failchk(env);
#endif
err:
	F_CLR(dbenv, DB_ENV_FAILCHK);
	return ret;
}
/*
 * __env_thread_size --
 *	Initial amount of memory for thread info blocks.
 * PUBLIC: size_t __env_thread_size __P((ENV *, size_t));
 */
size_t __env_thread_size(ENV * env, size_t other_alloc)
{
	DB_ENV * dbenv = env->dbenv;
	size_t size = 0;
	uint32 max = dbenv->thr_max;
	if(dbenv->thr_init != 0) {
		size = dbenv->thr_init*__env_alloc_size(sizeof(DB_THREAD_INFO));
		SETMAX(max, dbenv->thr_init);
	}
	else if(max == 0 && ALIVE_ON(env)) {
		if((max = dbenv->tx_init) == 0) {
			/*
			 * They want thread tracking, but don't say how much.
			 * Arbitrarily assume 1/10 of the remaining memory
			 * or at least 100.  We just use this to size
			 * the hash table.
			 */
			if(dbenv->memory_max != 0)
				max = (uint32)(((dbenv->memory_max-other_alloc)/10)/sizeof(DB_THREAD_INFO));
			SETMAX(max, 100);
		}
	}
	/*
	 * Set the number of buckets to be 1/8th the number of
	 * thread control blocks.  This is rather arbitrary.
	 */
	dbenv->thr_max = max;
	if(max != 0)
		size += __env_alloc_size(sizeof(DB_HASHTAB)*__db_tablesize(max/8));
	return size;
}
/*
 * __env_thread_max --
 *	Return the amount of extra memory to hold thread information.
 * PUBLIC: size_t __env_thread_max(ENV *);
 */
size_t __env_thread_max(ENV * env)
{
	size_t size;
	DB_ENV * dbenv = env->dbenv;
	/*
	 * Allocate space for thread info blocks.  Max is only advisory,
	 * so we allocate 25% more.
	 */
	if(dbenv->thr_max > dbenv->thr_init) {
		size = dbenv->thr_max-dbenv->thr_init;
		size += size/4;
	}
	else {
		dbenv->thr_max = dbenv->thr_init;
		size = dbenv->thr_init/4;
	}
	size = size*__env_alloc_size(sizeof(DB_THREAD_INFO));
	return size;
}
/*
 * __env_thread_init --
 *	Initialize the thread control block table.
 *
 * PUBLIC: int __env_thread_init __P((ENV *, int));
 */
int __env_thread_init(ENV * env, int during_creation)
{
	DB_HASHTAB * htab;
	THREAD_INFO * thread;
	int ret;
	DB_ENV * dbenv = env->dbenv;
	REGINFO * infop = env->reginfo;
	REGENV * renv = (REGENV *)infop->primary;
	if(renv->thread_off == INVALID_ROFF) {
		if(dbenv->thr_max == 0) {
			env->thr_hashtab = NULL;
			if(ALIVE_ON(env)) {
				__db_errx(env, DB_STR("1504", "is_alive method specified but no thread region allocated"));
				return EINVAL;
			}
			return 0;
		}
		if(!during_creation) {
			__db_errx(env, DB_STR("1505", "thread table must be allocated when the database environment is created"));
			return EINVAL;
		}
		if((ret = __env_alloc(infop, sizeof(THREAD_INFO), &thread)) != 0) {
			__db_err(env, ret, DB_STR("1506", "unable to allocate a thread status block"));
			return ret;
		}
		memzero(thread, sizeof(*thread));
		renv->thread_off = R_OFFSET(infop, thread);
		thread->thr_nbucket = __db_tablesize(dbenv->thr_max/8);
		if((ret = __env_alloc(infop, thread->thr_nbucket*sizeof(DB_HASHTAB), &htab)) != 0)
			return ret;
		thread->thr_hashoff = R_OFFSET(infop, htab);
		__db_hashinit(htab, thread->thr_nbucket);
		thread->thr_max = dbenv->thr_max;
		thread->thr_init = dbenv->thr_init;
	}
	else {
		thread = (THREAD_INFO *)R_ADDR(infop, renv->thread_off);
		htab = (DB_HASHTAB *)R_ADDR(infop, thread->thr_hashoff);
	}
	env->thr_hashtab = htab;
	env->thr_nbucket = thread->thr_nbucket;
	dbenv->thr_max = thread->thr_max;
	dbenv->thr_init = thread->thr_init;
	return 0;
}
/*
 * __env_thread_destroy --
 *	Destroy the thread control block table.
 *
 * PUBLIC: void __env_thread_destroy(ENV *);
 */
void __env_thread_destroy(ENV * env)
{
	DB_HASHTAB * htab;
	REGINFO * infop = env->reginfo;
	REGENV * renv = (REGENV *)infop->primary;
	if(renv->thread_off != INVALID_ROFF) {
		THREAD_INFO * thread = (THREAD_INFO *)R_ADDR(infop, renv->thread_off);
		if((htab = env->thr_hashtab) != NULL) {
			for(uint32 i = 0; i < env->thr_nbucket; i++) {
				DB_THREAD_INFO * ip = SH_TAILQ_FIRST(&htab[i], __db_thread_info);
				DB_THREAD_INFO * np;
				for(; ip; ip = np) {
					np = SH_TAILQ_NEXT(ip, dbth_links, __db_thread_info);
					__env_alloc_free(infop, ip);
				}
			}
			__env_alloc_free(infop, htab);
		}
		__env_alloc_free(infop, thread);
	}
}
/*
 * __env_in_api --
 *	Look for threads which died in the api and complain.
 *	If no threads died but there are blocked threads unpin
 *	any buffers they may have locked.
 */
static int __env_in_api(ENV * env)
{
	DB_ENV * dbenv;
	DB_HASHTAB * htab;
	DB_THREAD_INFO * ip;
	REGENV * renv;
	REGINFO * infop;
	THREAD_INFO * thread;
	uint32 i;
	int unpin, ret;
	if((htab = env->thr_hashtab) == NULL)
		return EINVAL;
	dbenv = env->dbenv;
	infop = env->reginfo;
	renv = (REGENV *)infop->primary;
	thread = (THREAD_INFO *)R_ADDR(infop, renv->thread_off);
	unpin = 0;
	for(i = 0; i < env->thr_nbucket; i++)
		SH_TAILQ_FOREACH(ip, &htab[i], dbth_links, __db_thread_info) {
			if(ip->dbth_state == THREAD_SLOT_NOT_IN_USE || (ip->dbth_state == THREAD_OUT && thread->thr_count <  thread->thr_max))
				continue;
			if(dbenv->is_alive(dbenv, ip->dbth_pid, ip->dbth_tid, 0))
				continue;
			if(ip->dbth_state == THREAD_BLOCKED) {
				ip->dbth_state = THREAD_BLOCKED_DEAD;
				unpin = 1;
				continue;
			}
			if(ip->dbth_state == THREAD_OUT) {
				ip->dbth_state = THREAD_SLOT_NOT_IN_USE;
				continue;
			}
			return __db_failed(env, DB_STR("1507", "Thread died in Berkeley DB library"), ip->dbth_pid, ip->dbth_tid);
		}
		if(unpin == 0)
			return 0;
	for(i = 0; i < env->thr_nbucket; i++)
		SH_TAILQ_FOREACH(ip, &htab[i], dbth_links, __db_thread_info)
		if(ip->dbth_state == THREAD_BLOCKED_DEAD && (ret = __memp_unpin_buffers(env, ip)) != 0)
			return ret;
	return 0;
}
/*
 * __env_clear_state --
 *	Look for threads which died while blockedi and clear them..
 */
static void __env_clear_state(ENV * env)
{
	DB_THREAD_INFO * ip;
	uint32 i;
	DB_HASHTAB * htab = env->thr_hashtab;
	for(i = 0; i < env->thr_nbucket; i++)
		SH_TAILQ_FOREACH(ip, &htab[i], dbth_links, __db_thread_info)
		if(ip->dbth_state == THREAD_BLOCKED_DEAD)
			ip->dbth_state = THREAD_SLOT_NOT_IN_USE;
}

struct __db_threadid {
	pid_t pid;
	db_threadid_t tid;
};

int FASTCALL __env_set_state(ENV * env, DB_THREAD_INFO ** ipp, DB_THREAD_STATE state)
{
	struct __db_threadid id;
	DB_ENV * dbenv;
	DB_HASHTAB * htab;
	DB_THREAD_INFO * ip;
	REGENV * renv;
	REGINFO * infop;
	THREAD_INFO * thread;
	uint32 indx;
	int ret;
	dbenv = env->dbenv;
	htab = env->thr_hashtab;
	if(F_ISSET(dbenv, DB_ENV_NOLOCKING)) {
		*ipp = NULL;
		return 0;
	}
	dbenv->thread_id(dbenv, &id.pid, &id.tid);
	/*
	 * Hashing of thread ids.  This is simple but could be replaced with
	 * something more expensive if needed.
	 */
#ifdef HAVE_SIMPLE_THREAD_TYPE
	/*
	 * A thread ID may be a pointer, so explicitly cast to a pointer of
	 * the appropriate size before doing the bitwise XOR.
	 */
	indx = (uint32)((uintptr_t)id.pid^(uintptr_t)id.tid);
#else
	indx = __ham_func5(NULL, &id.tid, sizeof(id.tid));
#endif
	indx %= env->thr_nbucket;
	SH_TAILQ_FOREACH(ip, &htab[indx], dbth_links, __db_thread_info) {
#ifdef HAVE_SIMPLE_THREAD_TYPE
		if(id.pid == ip->dbth_pid && id.tid == ip->dbth_tid)
			break;
#else
		if(memcmp(&id.pid, &ip->dbth_pid, sizeof(id.pid)) != 0)
			continue;
		if(memcmp(&id.tid, &ip->dbth_tid, sizeof(id.tid)) != 0)
			continue;
		break;
#endif
	}
	/*
	 * If ipp is not null,  return the thread control block if found.
	 * Check to ensure the thread of control has been registered.
	 */
	if(state == THREAD_VERIFY) {
		DB_ASSERT(env, ip != NULL && ip->dbth_state != THREAD_OUT);
		if(ipp) {
			if(ip == NULL)  /* The control block wasnt found */
				return EINVAL;
			*ipp = ip;
		}
		return 0;
	}
	*ipp = NULL;
	ret = 0;
	if(ip == NULL) {
		infop = env->reginfo;
		renv = (REGENV *)infop->primary;
		thread = (THREAD_INFO *)R_ADDR(infop, renv->thread_off);
		MUTEX_LOCK(env, renv->mtx_regenv);
		/*
		 * If we are passed the specified max, try to reclaim one from
		 * our queue.  If failcheck has marked the slot not in use, we
		 * can take it, otherwise we must call is_alive before freeing
		 * it.
		 */
		if(thread->thr_count >= thread->thr_max) {
			SH_TAILQ_FOREACH(ip, &htab[indx], dbth_links, __db_thread_info)
			if(ip->dbth_state == THREAD_SLOT_NOT_IN_USE || (ip->dbth_state == THREAD_OUT && ALIVE_ON(env) && !dbenv->is_alive(dbenv, ip->dbth_pid, ip->dbth_tid, 0)))
				break;
			if(ip) {
				DB_ASSERT(env, ip->dbth_pincount == 0);
				goto init;
			}
		}
		thread->thr_count++;
		if((ret = __env_alloc(infop, sizeof(DB_THREAD_INFO), &ip)) == 0) {
			memzero(ip, sizeof(*ip));
			/*
			 * This assumes we can link atomically since we do
			 * no locking here.  We never use the backpointer
			 * so we only need to be able to write an offset
			 * atomically.
			 */
			SH_TAILQ_INSERT_HEAD(&htab[indx], ip, dbth_links, __db_thread_info);
			ip->dbth_pincount = 0;
			ip->dbth_pinmax = PINMAX;
			ip->dbth_pinlist = R_OFFSET(infop, ip->dbth_pinarray);

init:                   
			ip->dbth_pid = id.pid;
			ip->dbth_tid = id.tid;
			ip->dbth_state = state;
			SH_TAILQ_INIT(&ip->dbth_xatxn);
		}
		MUTEX_UNLOCK(env, renv->mtx_regenv);
	}
	else
		ip->dbth_state = state;
	*ipp = ip;
	DB_ASSERT(env, ret == 0);
	if(ret != 0)
		__db_errx(env, DB_STR("1508", "Unable to allocate thread control block"));
	return ret;
}
/*
 * __env_thread_id_string --
 *	Convert a thread id to a string.
 *
 * PUBLIC: char *__env_thread_id_string
 * PUBLIC:     __P((DB_ENV *, pid_t, db_threadid_t, char *));
 */
char * __env_thread_id_string(DB_ENV * dbenv, pid_t pid, db_threadid_t tid, char * buf)
{
#ifdef HAVE_SIMPLE_THREAD_TYPE
 #ifdef UINT64_FMT
	char fmt[20];
	snprintf(fmt, sizeof(fmt), "%s/%s", UINT64_FMT, UINT64_FMT);
	snprintf(buf, DB_THREADID_STRLEN, fmt, (uint64)pid, (uint64)(uintptr_t)tid);
 #else
	snprintf(buf, DB_THREADID_STRLEN, "%lu/%lu", (ulong)pid, (ulong)tid);
 #endif
#else
 #ifdef UINT64_FMT
	char fmt[20];
	snprintf(fmt, sizeof(fmt), "%s/TID", UINT64_FMT);
	snprintf(buf, DB_THREADID_STRLEN, fmt, (uint64)pid);
 #else
	snprintf(buf, DB_THREADID_STRLEN, "%lu/TID", (ulong)pid);
 #endif
#endif
	COMPQUIET(dbenv, 0);
	COMPQUIET(*(uint8 *)&tid, 0);
	return buf;
}
