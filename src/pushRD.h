#ifndef KRK_PUSHRD_H
#define KRK_PUSHRD_H

#include <stdlib.h>
#include <string.h>

#define concat(a, b) a ## b

#define krk_pushrd_t_INSTANCE(type) \
	typedef struct {                \
		type*  buf;                 \
		size_t len;                 \
	} concat(krk_pushrd_t$, type);

#define krk_pushrd_add_INSTANCE(type)          \
	int concat(krk_pushrd_add$, type) (        \
			concat(krk_pushrd_t$, type)* this, \
			type val                           \
	) {                                        \
		type* new = reallocarray(              \
			this->buf,                         \
			this->len + 1,                     \
			sizeof(type)                       \
		);                                     \
		if(new == NULL) return -1;             \
		new[this->len] = val;                  \
		this->buf = new;                       \
		this->len++;                           \
		return 0;                              \
	}

#define krk_pushrd_del_INSTANCE(type)            \
	void concat(krk_pushrd_del$, type) (         \
		concat(krk_pushrd_t$, type)* this,       \
		size_t pos                               \
	) {                                          \
		memmove(                                 \
			this->buf + pos,                     \
			this->buf + pos + 1,                 \
			(this->len - pos - 1) * sizeof(type) \
		);                                       \
		this->buf = reallocarray(                \
			this->buf,                           \
			this->len - 1,                       \
			sizeof(type)                         \
		);                                       \
		this->len--;                             \
	}

#define krk_pushrd_INSTANCE(type)  \
	krk_pushrd_t_INSTANCE(type);   \
	krk_pushrd_add_INSTANCE(type); \
	krk_pushrd_del_INSTANCE(type); \

#endif
