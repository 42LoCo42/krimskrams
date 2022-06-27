#include "coro.h"

size_t krk_coro_stack = 1 << 13;

int krk_coro_run(krk_coro_t* coro) {
	coro->state = RUNNING;
	coro->result = NULL;
	return swapcontext(&coro->back_ctx, &coro->coro_ctx);
}

static int krk_coro_back(krk_coro_t* coro) {
	return swapcontext(&coro->coro_ctx, &coro->back_ctx);
}

int krk_coro_yield(krk_coro_t* coro, void* result) {
	coro->state = PAUSED;
	coro->result = result;
	return krk_coro_back(coro);
}

int krk_coro_finish(krk_coro_t* coro, void* result) {
	coro->state = FINISHED;
	coro->result = result;
	return krk_coro_back(coro);
}

int krk_coro_error(krk_coro_t* coro) {
	coro->state = ERRORED;
	return krk_coro_back(coro);
}

int krk_coro_force(krk_coro_t* coro) {
	for(;;) {
		krk_coro_run(coro);
		if(coro->state == FINISHED) return 0;
		if(coro->state == ERRORED)  return -1;
	}
}

void krk_coro_free(krk_coro_t* coro) {
	free(coro->coro_ctx.uc_stack.ss_sp);
}
