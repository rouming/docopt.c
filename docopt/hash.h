#ifndef _HASH_H_
#define _HASH_H_

#include <string.h>

#include "types.h"
#include "list.h"

/* Must be power of 2 */
#define MAX_BUCKETS 128

/*
 * This is the "One-at-a-Time" algorithm by Bob Jenkins
 * from requirements by Colin Plumb.
 * (http://burtleburtle.net/bob/hash/doobs.html)
 */
static inline unsigned int jhash(const char *key, size_t len)
{
	unsigned int hash, i;

	for (hash = 0, i = 0; i < len; ++i) {
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

struct hash_entry {
	struct list_head buckent;
	struct list_head listent;
	const void   *key;
	size_t        key_len;
};

struct hash_table {
	struct list_head buckets[MAX_BUCKETS];
	struct list_head list;
};

static inline void hash_init(struct hash_table *tbl)
{
	int i;

	INIT_LIST_HEAD(&tbl->list);
	for (i = 0; i < ARRAY_SIZE(tbl->buckets); i++)
		INIT_LIST_HEAD(&tbl->buckets[i]);
}

static inline void
hash_entry_init(struct hash_entry *e, const void *key,
		size_t key_len)
{
	e->key = key;
	e->key_len = key_len;
}

static inline struct hash_entry *
hash_lookup(struct hash_table *tbl, const void *key,
	    size_t key_len, unsigned int *hint)
{
	struct hash_entry *e;
	struct list_head *l;
	unsigned int hash, ind;

	hash = jhash((const char *)key, key_len);
	ind = hash & (MAX_BUCKETS - 1);
	l = &tbl->buckets[ind];
	if (hint)
		*hint = ind;

	list_for_each_entry(e, l, buckent) {
		if (e->key_len != key_len)
			continue;
		if (!memcmp(e->key, key, key_len))
			return e;
	}
	return NULL;
}

static inline void
hash_insert(struct hash_table *tbl, struct hash_entry *e,
	    unsigned int *hint)
{
	unsigned int hash, ind;

	if (hint && *hint < MAX_BUCKETS)
		ind = *hint;
	else {
		hash = jhash(e->key, e->key_len);
		ind = hash & (MAX_BUCKETS - 1);
	}
	list_add_tail(&e->buckent, &tbl->buckets[ind]);
	list_add_tail(&e->listent, &tbl->list);
}

static inline void
hash_remove(struct hash_entry *e)
{
	list_del(&e->buckent);
	list_del(&e->listent);
}

#define hash_for_each_entry(pos, hash, member)				\
	for (pos = container_of(list_first_entry(&(hash)->list,		\
						 struct hash_entry,	\
						 listent),		\
				typeof(*(pos)), member);		\
		     &pos->member.listent != (&(hash)->list);		\
	     pos = container_of(list_next_entry(&pos->member,		\
						listent),		\
				typeof(*pos), member))

#define hash_for_each_entry_safe(pos, n, hash, member)			\
	for (pos = container_of(list_first_entry(&(hash)->list,		\
						 struct hash_entry,	\
						 listent),		\
				typeof(*pos), member),			\
		     n = container_of(list_next_entry(&pos->member,	\
						      listent),		\
				      typeof(*pos), member);		\
	     &pos->member.listent != (&(hash)->list);			\
	     pos = n,							\
		     n = container_of(list_next_entry(&n->member,	\
						      listent),		\
				      typeof(*n), member))		\




#endif /* _HASH_H_ */
