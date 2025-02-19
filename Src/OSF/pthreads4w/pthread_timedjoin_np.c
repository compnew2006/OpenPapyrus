/*
 * pthread_timedjoin_np.c
 *
 * Description:
 * This translation unit implements functions related to thread
 * synchronisation.
 *
 * --------------------------------------------------------------------------
 *
 *   Pthreads4w - POSIX Threads for Windows
 *   Copyright 1998 John E. Bossom
 *   Copyright 1999-2018, Pthreads4w contributors
 *
 *   Homepage: https://sourceforge.net/projects/pthreads4w/
 *
 *   The current list of contributors is contained
 *   in the file CONTRIBUTORS included with the source
 *   code distribution. The list can also be seen at the
 *   following World Wide Web location: https://sourceforge.net/p/pthreads4w/wiki/Contributors/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <sl_pthreads4w.h>
#pragma hdrstop
// Not needed yet, but defining it should indicate clashes with build target environment that should be fixed.
//#if !defined(WINCE)
	//#include <signal.h>
//#endif
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *   This function waits for 'thread' to terminate and
 *   returns the thread's exit value if 'value_ptr' is not
 *   NULL or until 'abstime' passes and returns an
 *   error. If 'abstime' is NULL then the function waits
 *   forever, i.e. reverts to pthread_join behaviour.
 *   This function detaches the thread on successful
 *   completion.
 *
 * PARAMETERS
 *   thread
 *     an instance of pthread_t
 *
 *   value_ptr
 *     pointer to an instance of pointer to void
 *
 *   abstime
 *     pointer to an instance of struct timespec
 *     representing an absolute time value
 *
 *
 * DESCRIPTION
 *   This function waits for 'thread' to terminate and
 *   returns the thread's exit value if 'value_ptr' is not
 *   NULL or until 'abstime' passes and returns an
 *   error. If 'abstime' is NULL then the function waits
 *   forever, i.e. reverts to pthread_join behaviour.
 *   This function detaches the thread on successful
 *   completion.
 *   NOTE:   Detached threads cannot be joined or canceled.
 *     In this implementation 'abstime' will be
 *     resolved to the nearest millisecond.
 *
 * RESULTS
 *     0               'thread' has completed
 *     ETIMEDOUT       abstime passed
 *     EINVAL          thread is not a joinable thread,
 *     ESRCH           no thread could be found with ID 'thread',
 *     ENOENT          thread couldn't find it's own valid handle,
 *     EDEADLK         attempt to join thread with self
 *
 * ------------------------------------------------------
 */
int pthread_timedjoin_np(pthread_t thread, void ** value_ptr, const struct timespec * abstime)
{
	int result;
	pthread_t self;
	DWORD milliseconds;
	__ptw32_thread_t * tp = static_cast<__ptw32_thread_t *>(thread.p);
	__ptw32_mcs_local_node_t node;
	if(!abstime) {
		milliseconds = INFINITE;
	}
	else {
		// Calculate timeout as milliseconds from current system time.
		milliseconds = __ptw32_relmillisecs(abstime);
	}
	__ptw32_mcs_lock_acquire(&__ptw32_thread_reuse_lock, &node);
	if(!tp || thread.x != tp->ptHandle.x) {
		result = ESRCH;
	}
	else if(PTHREAD_CREATE_DETACHED == tp->detachState) {
		result = EINVAL;
	}
	else {
		result = 0;
	}
	__ptw32_mcs_lock_release(&node);
	if(result == 0) {
		/*
		 * The target thread is joinable and can't be reused before we join it.
		 */
		self = pthread_self();
		if(!self.p)
			result = ENOENT;
		else if(pthread_equal(self, thread))
			result = EDEADLK;
		else {
			/*
			 * Pthread_join is a cancellation point.
			 * If we are canceled then our target thread must not be
			 * detached (destroyed). This is guaranteed because
			 * pthreadCancelableTimedWait will not return if we
			 * are canceled.
			 */
			result = pthreadCancelableTimedWait(tp->threadH, milliseconds);
			if(!result) {
				ASSIGN_PTR(value_ptr, tp->exitStatus);
				/*
				 * The result of making multiple simultaneous calls to
				 * pthread_join() or pthread_timedjoin_np() or pthread_detach()
				 * specifying the same target is undefined.
				 */
				result = pthread_detach(thread);
			}
			else if(ETIMEDOUT != result)
				result = ESRCH;
		}
	}
	return result;
}
