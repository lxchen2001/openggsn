/* 
 * IP address pool functions.
 * Copyright (C) 2003 Mondru AB.
 * 
 * The contents of this file may be used under the terms of the GNU
 * General Public License Version 2, provided that the above copyright
 * notice and this permission notice is included in all copies or
 * substantial portions of the software.
 * 
 * The initial developer of the original code is
 * Jens Jakobsen <jj@openggsn.org>
 * 
 * Contributor(s):
 * 
 */

#include <netinet/in.h> /* in_addr */
#include <stdlib.h>     /* calloc */
#include <stdio.h>      /* sscanf */

#include "ippool.h"

/**
 * lookup()
 * Generates a 32 bit hash.
 * Based on public domain code by Bob Jenkins
 * It should be one of the best hash functions around in terms of both
 * statistical properties and speed. It is NOT recommended for cryptographic
 * purposes.
 **/
unsigned long int lookup( k, length, level)
register unsigned char *k;         /* the key */
register unsigned long int length; /* the length of the key */
register unsigned long int level; /* the previous hash, or an arbitrary value*/
{

#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

  typedef  unsigned long  int  ub4;   /* unsigned 4-byte quantities */
  typedef  unsigned       char ub1;   /* unsigned 1-byte quantities */
  register unsigned long int a,b,c,len;
  
  /* Set up the internal state */
  len = length;
  a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
  c = level;           /* the previous hash value */
  
  /*---------------------------------------- handle most of the key */
  while (len >= 12)
    {
      a += (k[0] +((ub4)k[1]<<8) +((ub4)k[2]<<16) +((ub4)k[3]<<24));
      b += (k[4] +((ub4)k[5]<<8) +((ub4)k[6]<<16) +((ub4)k[7]<<24));
      c += (k[8] +((ub4)k[9]<<8) +((ub4)k[10]<<16)+((ub4)k[11]<<24));
      mix(a,b,c);
      k += 12; len -= 12;
    }
  
  /*------------------------------------- handle the last 11 bytes */
  c += length;
  switch(len)              /* all the case statements fall through */
    {
    case 11: c+=((ub4)k[10]<<24);
    case 10: c+=((ub4)k[9]<<16);
    case 9 : c+=((ub4)k[8]<<8);
      /* the first byte of c is reserved for the length */
    case 8 : b+=((ub4)k[7]<<24);
    case 7 : b+=((ub4)k[6]<<16);
    case 6 : b+=((ub4)k[5]<<8);
    case 5 : b+=k[4];
    case 4 : a+=((ub4)k[3]<<24);
    case 3 : a+=((ub4)k[2]<<16);
    case 2 : a+=((ub4)k[1]<<8);
    case 1 : a+=k[0];
      /* case 0: nothing left to add */
    }
  mix(a,b,c);
  /*-------------------------------------------- report the result */
  return c;
}


int ippool_printaddr(struct ippool_t *this) {
  int n;
  printf("ippool_printaddr\n");
  printf("Firstdyn %d\n", this->firstdyn - this->member);
  printf("Lastdyn %d\n",  this->lastdyn - this->member);
  printf("Firststat %d\n", this->firststat - this->member);
  printf("Laststat %d\n",  this->laststat - this->member);
  printf("Listsize %d\n",  this->listsize);

  for (n=0; n<this->listsize; n++) {
    printf("Unit %d inuse %d prev %d next %d addr %x\n", 
	   n,
	   this->member[n].inuse,
	   this->member[n].prev - this->member,
	   this->member[n].next - this->member,
	   this->member[n].addr.s_addr
	   );
  }
  return 0;
}


int ippool_hashadd(struct ippool_t *this, struct ippoolm_t *member) {
  uint32_t hash;
  struct ippoolm_t *p;
  struct ippoolm_t *p_prev = NULL; 

  /* Insert into hash table */
  hash = ippool_hash4(&member->addr) & this->hashmask;
  for (p = this->hash[hash]; p; p = p->nexthash)
    p_prev = p;
  if (!p_prev)
    this->hash[hash] = member;
  else 
    p_prev->nexthash = member;
  return 0; /* Always OK to insert */
}

int ippool_hashdel(struct ippool_t *this, struct ippoolm_t *member) {
  uint32_t hash;
  struct ippoolm_t *p;
  struct ippoolm_t *p_prev = NULL; 

  /* Find in hash table */
  hash = ippool_hash4(&member->addr) & this->hashmask;
  for (p = this->hash[hash]; p; p = p->nexthash) {
    if (p == member) {
      break;
    }
    p_prev = p;
  }

  if (p!= member) {
    printf("ippool_hashdel: Tried to delete member not in hash table\n");
    return -1; /* Member was not in hash table !!! */
  }

  if (!p_prev)
    this->hash[hash] = 0;
  else
    p_prev->nexthash = 0;

  return 0;
}


unsigned long int ippool_hash4(struct in_addr *addr) {
  return lookup(&addr->s_addr, sizeof(addr->s_addr), 0);
}

#ifndef IPPOOL_NOIP6
unsigned long int ippool_hash6(struct in6_addr *addr) {
  return lookup(addr->u6_addr8, sizeof(addr->u6_addr8), 0);
}
#endif


/* Get IP address and mask */
int ippool_aton(struct in_addr *addr, struct in_addr *mask,
		char *pool, int number) {

  /* Parse only first instance of network for now */
  /* Eventually "number" will indicate the token which we want to parse */

  unsigned int a1, a2, a3, a4;
  unsigned int m1, m2, m3, m4;
  int c;
  unsigned int m;
  int masklog;

  c = sscanf(pool, "%u.%u.%u.%u/%u.%u.%u.%u",
	     &a1, &a2, &a3, &a4,
	     &m1, &m2, &m3, &m4);
  switch (c) {
  case 4:
    if (a1 == 0 && a2 == 0 && a3 == 0 && a4 == 0) /* Full Internet */
      mask->s_addr = 0x00000000;
    else if (a2 == 0 && a3 == 0 && a4 == 0)       /* class A */
      mask->s_addr = htonl(0xff000000);
    else if (a3 == 0 && a4 == 0)	          /* class B */
      mask->s_addr = htonl(0xffff0000);
    else if (a4 == 0)	                          /* class C */
      mask->s_addr = htonl(0xffffff00);
    else
      mask->s_addr = 0xffffffff;
    break;
  case 5:
    if (m1 < 0 || m1 > 32) {
      return -1; /* Invalid mask */
    }
    mask->s_addr = htonl(0xffffffff << (32 - m1));
    break;
  case 8:
    if (m1 >= 256 ||  m2 >= 256 || m3 >= 256 || m4 >= 256)
      return -1; /* Wrong mask format */
    m = m1 * 0x1000000 + m2 * 0x10000 + m3 * 0x100 + m4;
    for (masklog = 0; ((1 << masklog) < ((~m)+1)); masklog++);
    if (((~m)+1) != (1 << masklog))
      return -1; /* Wrong mask format (not all ones followed by all zeros)*/
    mask->s_addr = htonl(m);
    break;
  default:
    return -1; /* Invalid mask */
  }

  if (a1 >= 256 ||  a2 >= 256 || a3 >= 256 || a4 >= 256)
    return -1; /* Wrong IP address format */
  else
    addr->s_addr = htonl(a1 * 0x1000000 + a2 * 0x10000 + a3 * 0x100 + a4);

  return 0;
}

/* Create new address pool */
int ippool_new(struct ippool_t **this, char *dyn,  char *stat, 
	       int allowdyn, int allowstat, int flags) {

  /* Parse only first instance of pool for now */

  int i;
  struct in_addr addr;
  struct in_addr mask;
  struct in_addr stataddr;
  struct in_addr statmask;
  unsigned int m;
  unsigned int listsize;
  unsigned int dynsize;
  unsigned int statsize;

  if (!allowdyn) {
    dynsize = 0;
  }
  else {
    if (ippool_aton(&addr, &mask, dyn, 0))
      return -1; /* Failed to parse dynamic pool */
    
    m = ntohl(mask.s_addr);
    dynsize = ((~m)+1);
    if (flags & IPPOOL_NONETWORK)   /* Exclude network address from pool */
      dynsize--;
    if (flags & IPPOOL_NOBROADCAST) /* Exclude broadcast address from pool */
      dynsize--;
  }

  if (!allowstat) {
    statsize = 0;
    stataddr.s_addr = 0;
    statmask.s_addr = 0;
  }
  else {
    if (ippool_aton(&stataddr, &statmask, stat, 0))
      return -1; /* Failed to parse static range */
    
    m = ntohl(statmask.s_addr);
    statsize = ((~m)+1);
    if (statsize > IPPOOL_STATSIZE) statsize = IPPOOL_STATSIZE;
  }

  listsize = dynsize + statsize; /* Allocate space for static IP addresses */

  if (!(*this = calloc(sizeof(struct ippool_t), 1))) {
    /* Failed to allocate memory for ippool */
    return -1;
  }
  
  (*this)->allowdyn  = allowdyn;
  (*this)->allowstat = allowstat;
  (*this)->stataddr  = stataddr;
  (*this)->statmask  = statmask;

  (*this)->listsize += listsize;
  if (!((*this)->member = calloc(sizeof(struct ippoolm_t), listsize))){
    /* Failed to allocate memory for members in ippool */
    return -1;
  }
  
  for ((*this)->hashlog = 0; 
       ((1 << (*this)->hashlog) < listsize);
       (*this)->hashlog++);

  /*   printf ("Hashlog %d %d %d\n", (*this)->hashlog, listsize, (1 << (*this)->hashlog)); */

  /* Determine hashsize */
  (*this)->hashsize = 1 << (*this)->hashlog; /* Fails if mask=0: All Internet*/
  (*this)->hashmask = (*this)->hashsize -1;
  
  /* Allocate hash table */
  if (!((*this)->hash = calloc(sizeof(struct ippoolm_t), (*this)->hashsize))){
    /* Failed to allocate memory for hash members in ippool */
    return -1;
  }
  
  (*this)->firstdyn = NULL;
  (*this)->lastdyn = NULL;
  for (i = 0; i<dynsize; i++) {

    if (flags & IPPOOL_NONETWORK)
      (*this)->member[i].addr.s_addr = htonl(ntohl(addr.s_addr) + i + 1);
    else
      (*this)->member[i].addr.s_addr = htonl(ntohl(addr.s_addr) + i);

    (*this)->member[i].inuse = 0;

    /* Insert into list of unused */
    (*this)->member[i].prev = (*this)->lastdyn;
    if ((*this)->lastdyn) {
      (*this)->lastdyn->next = &((*this)->member[i]);
    }
    else {
      (*this)->firstdyn = &((*this)->member[i]);
    }
    (*this)->lastdyn = &((*this)->member[i]);
    (*this)->member[i].next = NULL; /* Redundant */

    ippool_hashadd(*this, &(*this)->member[i]);
  }

  (*this)->firststat = NULL;
  (*this)->laststat = NULL;
  for (i = dynsize; i<listsize; i++) {

    (*this)->member[i].addr.s_addr = 0;
    (*this)->member[i].inuse = 0;

    /* Insert into list of unused */
    (*this)->member[i].prev = (*this)->laststat;
    if ((*this)->laststat) {
      (*this)->laststat->next = &((*this)->member[i]);
    }
    else {
      (*this)->firststat = &((*this)->member[i]);
    }
    (*this)->laststat = &((*this)->member[i]);
    (*this)->member[i].next = NULL; /* Redundant */
  }

  if (0) ippool_printaddr(*this);
  return 0;
}

/* Delete existing address pool */
int ippool_free(struct ippool_t *this) {
  free(this->hash);
  free(this->member);
  free(this);
  return 0; /* Always OK */
}

/* Find an IP address in the pool */
int ippool_getip(struct ippool_t *this, struct ippoolm_t **member,
		 struct in_addr *addr) {
  struct ippoolm_t *p;
  uint32_t hash;

  /* Find in hash table */
  hash = ippool_hash4(addr) & this->hashmask;
  for (p = this->hash[hash]; p; p = p->nexthash) {
    if ((p->addr.s_addr == addr->s_addr) && (p->inuse)) {
      *member = p;
      return 0;
    }
  }
  *member = NULL;
  return -1; /* Address could not be found */
}

/**
 * ippool_newip
 * Get an IP address. If addr = 0.0.0.0 get a dynamic IP address. Otherwise
 * check to see if the given address is available. If available within
 * dynamic address space allocate it there, otherwise allocate within static
 * address space.
**/
int ippool_newip(struct ippool_t *this, struct ippoolm_t **member,
		 struct in_addr *addr) {
  struct ippoolm_t *p;
  struct ippoolm_t *p2 = NULL;
  uint32_t hash;

  /* If static:
   *   Look in dynaddr. 
   *     If found remove from firstdyn/lastdyn linked list.
   *   Else allocate from stataddr.
   *    Remove from firststat/laststat linked list.
   *    Insert into hash table.
   *
   * If dynamic
   *   Remove from firstdyn/lastdyn linked list.
   *
   */

  if (0) ippool_printaddr(this);

  /* First check to see if this type of address is allowed */
  if ((addr) && (addr->s_addr)) { /* IP address given */
    if (!this->allowstat) {
      return -1; /* Static not allowed */
    }
    if ((addr->s_addr & this->statmask.s_addr) != this->stataddr.s_addr) {
      return -1; /* Static out of range */
    }
  }
  else {
    if (!this->allowdyn) {
      return -1; /* Dynamic not allowed */
    }
  }

  if ((addr) && (addr->s_addr)) { /* IP address given */
    /* Find in hash table */
    hash = ippool_hash4(addr) & this->hashmask;
    for (p = this->hash[hash]; p; p = p->nexthash) {
      if ((p->addr.s_addr == addr->s_addr)) {
	p2 = p;
	break;
      }
    }
  }
  else { /* No ip address given */
    if (!this ->firstdyn)
      return -1; /* No more available */
    else
      p2 = this ->firstdyn;
  }

  if (p2) { /* Was allocated from dynamic address pool */
    if (p2->inuse) return -1; /* Allready in use / Should not happen */
  
    /* Remove from linked list of free dynamic addresses */
    if (p2->prev) 
      p2->prev->next = p2->next;
    else
      this->firstdyn = p2->next;
    if (p2->next) 
      p2->next->prev = p2->prev;
    else
      this->lastdyn = p2->prev;
    p2->next = NULL;
    p2->prev = NULL;
    p2->inuse = 1; /* Dynamic address in use */
    
    *member = p2;
    if (0) ippool_printaddr(this);
    return 0; /* Success */
  }

  /* It was not possible to allocate from dynamic address pool */
  /* Try to allocate from static address space */

  if ((addr) && (addr->s_addr)) { /* IP address given */
    if (this->firststat)
      return -1; /* No more available */
    else
      p2 = this ->firststat;

    /* Remove from linked list of free static addresses */
    if (p2->prev) 
      p2->prev->next = p2->next;
    else
      this->firststat = p2->next;
    if (p2->next) 
      p2->next->prev = p2->prev;
    else
      this->laststat = p2->prev;
    p2->next = NULL;
    p2->prev = NULL;
    p2->inuse = 1; /* Static address in use */

    *member = p2;
    if (0) ippool_printaddr(this);
    return 0; /* Success */
  }

  return -1; /* Should never get here. TODO: Bad code */
}


int ippool_freeip(struct ippool_t *this, struct ippoolm_t *member) {
  
  if (0) ippool_printaddr(this);

  if (!member->inuse) return -1; /* Not in use: Should not happen */

  switch (member->inuse) {
  case 0: /* Not in use: Should not happen */
    return -1;
  case 1: /* Allocated from dynamic address space */
    /* Insert into list of unused */
    member->prev = this->lastdyn;
    if (this->lastdyn) {
      this->lastdyn->next = member;
    }
    else {
      this->firstdyn = member;
    }
    this->lastdyn = member;
    
    member->inuse = 0;
    member->peer = NULL;
    if (0) ippool_printaddr(this);
    return 0;
  case 2: /* Allocated from static address space */
    if (ippool_hashdel(this, member))
      return -1;
    /* Insert into list of unused */
    member->prev = this->laststat;
    if (this->laststat) {
      this->laststat->next = member;
    }
    else {
      this->firststat = member;
    }
    this->laststat = member;
    
    member->inuse = 0;
    member->addr.s_addr = 0;
    member->peer = NULL;
    member->nexthash = NULL;
    if (0) ippool_printaddr(this);
    return 0;
  default: /* Should not happen */
    return -1;
  }
}


#ifndef IPPOOL_NOIP6
extern unsigned long int ippool_hash6(struct in6_addr *addr);
extern int ippool_getip6(struct ippool_t *this, struct in6_addr *addr);
extern int ippool_returnip6(struct ippool_t *this, struct in6_addr *addr);
#endif