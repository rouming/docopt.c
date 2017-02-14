#ifndef _TYPES_H_
#define _TYPES_H_

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))

#ifndef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
#endif

#define container_of(ptr, type, member) ({                      \
		const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
		(type *)( (char *)__mptr - offsetof(type,member) );})

typedef _Bool bool;
enum {
	false	= 0,
	true	= 1
};

#endif /* _TYPES_H_ */
