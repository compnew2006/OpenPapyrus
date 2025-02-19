// GNUPLOT - gpexecute.c 
//
/*
 * AUTHORS
 *   Original Software (October 1999 - January 2000):
 *     Pieter-Tjerk de Boer <ptdeboer@cs.utwente.nl>
 *     Petr Mikulik <mikulik@physics.muni.cz>
 *     Johannes Zellner <johannes@zellner.org>
 */
#include <gnuplot.h>
#pragma hdrstop
#ifdef PIPE_IPC
	#include <unistd.h>     /* open(), write() */
	int pipe_died = 0;
#endif /* PIPE_IPC */
#if defined(PIPE_IPC) /* || defined(WIN_IPC) */
	static gpe_fifo_t * gpe_init();
	static void gpe_push(gpe_fifo_t ** base, GpEvent * ge);
	static GpEvent * gpe_front(gpe_fifo_t ** base);
	static int gpe_pop(gpe_fifo_t ** base);
#endif /* PIPE_IPC || WIN_IPC */
/*
 * gp_execute functions
 */
#if defined(PIPE_IPC) /* || defined(WIN_IPC) */

int buffered_output_pending = 0;

static gpe_fifo_t * gpe_init()
{
	gpe_fifo_t * base = SAlloc::M(sizeof(gpe_fifo_t));
	/* fprintf(stderr, "(gpe_init) \n"); */
	assert(base);
	base->next = (gpe_fifo_t*)0;
	base->prev = (gpe_fifo_t*)0;
	return base;
}

static void gpe_push(gpe_fifo_t ** base, GpEvent * ge)
{
	buffered_output_pending++;
	if((*base)->prev) {
		gpe_fifo_t * new = SAlloc::M(sizeof(gpe_fifo_t));
		/* fprintf(stderr, "(gpe_push) \n"); */
		assert(new);
		(*base)->prev->next = new;
		new->prev = (*base)->prev;
		(*base)->prev = new;
		new->next = (gpe_fifo_t*)0;
	}
	else {
		/* first element, this is the case, if the pipe isn't clogged */
		(*base)->next = (gpe_fifo_t*)0; /* tail */
		(*base)->prev = (*base); /* points to itself */
	}
	(*base)->prev->ge = *ge;
}

static GpEvent * gpe_front(gpe_fifo_t ** base)                           
{
	return &((*base)->ge);
}

static int gpe_pop(gpe_fifo_t ** base)
{
	buffered_output_pending--;
	if((*base)->prev == (*base)) {
		(*base)->prev = (gpe_fifo_t*)0;
		return 0;
	}
	else {
		gpe_fifo_t * save = *base;
		/* fprintf(stderr, "(gpe_pop) \n"); */
		(*base)->next->prev = (*base)->prev;
		(*base) = (*base)->next;
		SAlloc::F(save);
		return 1;
	}
}
#endif /* PIPE_IPC || WIN_IPC */

#ifdef PIPE_IPC
	RETSIGTYPE pipe_died_handler(int signum)
	{
		(void)signum; /* avoid -Wunused warning. */
		/* fprintf(stderr, "\n*******(pipe_died_handler)*******\n"); */
		close(1);
		pipe_died = 1;
	}
#endif /* PIPE_IPC */

void gp_exec_event(char type, int mx, int my, int par1, int par2, int winid)
{
	GpEvent ge;
#if defined(PIPE_IPC) /* || defined(WIN_IPC) */
	static struct gpe_fifo_t * base = (gpe_fifo_t*)0;
#endif
	ge.type = type;
	ge.mx = mx;
	ge.my = my;
	ge.par1 = par1;
	ge.par2 = par2;
	ge.winid = winid;
#ifdef PIPE_IPC
	if(pipe_died)
		return;
#endif
	// HBB 20010218: commented this out for WIN_IPC. We don't actually use the stack, there 
#if defined(PIPE_IPC) /* || defined(WIN_IPC) */
	if(!base) {
		base = gpe_init();
	}
	if(GE_pending != type) {
		gpe_push(&base, &ge);
	}
	else if(!buffered_output_pending) {
		return;
	}
#endif
#ifdef WIN_IPC
	GPO.DoEvent(GPT.P_Term, &ge);
	return;
#endif
#ifdef PIPE_IPC
	do {
		int status = write(1, gpe_front(&base), sizeof(ge));
		if(-1 == status) {
			switch(errno) {
				case EAGAIN:
				    /* do nothing */
				    FPRINTF((stderr, "(gp_exec_event) EAGAIN\n"));
				    break;
				default:
				    FPRINTF((stderr, "(gp_exec_event) errno = %d\n", errno));
				    break;
			}
			break;
		}
	} while(gpe_pop(&base));
#endif /* PIPE_IPC */
}
