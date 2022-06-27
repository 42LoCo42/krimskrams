#ifndef KRK_CORO_H
#define KRK_CORO_H

#include <stdlib.h>
#include <ucontext.h>

#ifndef KRK_CORO_STACK
#define KRK_CORO_STACK (1 << 13)
#endif

#define krk_coro_mk(coro, func, argc, ...) ({                  \
	memset(coro, 0, sizeof(krk_coro_t));                       \
	void* stack = malloc(KRK_CORO_STACK);                      \
	stack == NULL || getcontext(&coro->coro_ctx) < 0 ? -1 : ({ \
		coro->coro_ctx.uc_stack.ss_sp = stack;                 \
		coro->coro_ctx.uc_stack.ss_size = KRK_CORO_STACK;      \
		makecontext(                                           \
			&coro->coro_ctx,                                   \
			(void(*)()) func,                                  \
			argc + 1,                                          \
			coro,                                              \
			__VA_ARGS__                                        \
		);                                                     \
		0;                                                     \
	});                                                        \
})

typedef enum {
	INVALID = 0,
	RUNNING,
	PAUSED,
	FINISHED,
	ERRORED,
} krk_coro_state_t;

typedef struct {
	ucontext_t       back_ctx;
	ucontext_t       coro_ctx;
	krk_coro_state_t state;
	void*            extra;
	void*            result;
} krk_coro_t;

int  krk_coro_run   (krk_coro_t* coro);
int  krk_coro_yield (krk_coro_t* coro, void* result);
int  krk_coro_finish(krk_coro_t* coro, void* result);
int  krk_coro_error (krk_coro_t* coro);
int  krk_coro_force (krk_coro_t* coro);
void krk_coro_free  (krk_coro_t* coro);

#endif
