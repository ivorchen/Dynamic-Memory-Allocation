/*
 * This implementation replicates the implicit list implementation
 * provided in the textbook
 * "Computer Systems - A Programmer's Perspective"
 * Blocks are never coalesced or reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "DragonBall",
    /* First member's full name */
    "Chong Zhu",
    /* First member's email address */
    "chong.zhu@mail.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "Zhongxing Chen",
    /* Second member's email address (leave blank if none) */
    "zhongxing.chen@mail.utoronto.ca"
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            	/* word size (bytes) */
#define DSIZE       (2 * WSIZE)            	/* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      		/* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given a free block ptr bp, compute address of its prev and next ptr */
#define PREV_PTR(bp)    (FTRP(bp) - DSIZE) //place the prev_ptr 2-word size before the footer
#define NEXT_PTR(bp)    (FTRP(bp) - WSIZE) //place the next_ptr 1-word size before the footer

/* Given a free block ptr bp, compute address of the next and previous free blocks */
#define NEXT_FREE_BLKP(bp) (GET(NEXT_PTR(bp)))
#define PREV_FREE_BLKP(bp) (GET(PREV_PTR(bp)))

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Utilities */
//print the content of a node with a pointer given by bp
#define PRINT_ADDR(p)  printf("0x%0x\n",p);
#define PRETTY_PRINT_LIST(num, size, bp, hdrp, ftrp,prev_ptr, next_ptr, prev_free_blkp, next_free_blkp) \
printf("Element %d: size=%lu, addr(bp)=0x%lx, addr(HDPR)=0x%lx, addr(FTRP)=0x%lx, \
       addr(PREV_PTR)=0x%lx, addr(NEXT_PTR)=0x%lx, \
       addr(PREV_FREE_BLKP)=0x%lx, addr(NEXT_FREE_BLKP)=0x%lx\n", \
       num, size, bp, hdrp, ftrp, prev_ptr, \
       next_ptr, prev_free_blkp, next_free_blkp)

void* heap_listp = NULL;

//define the size for the array to store boundary and free list head pointer
#define SIZE 13

/* set the boundary for each free list
 * e.g.
 * 32 for the range which x is 32<=x<64 
 * 64 for the range which x is 64<=x<96
 */
size_t boundary[SIZE]={32,64,96,128,160,256,512,1024,2048,4096,4128,4160,4224};

/* free list array is used to store the head pointer for each size class 
 * e.g.
 * free[0] stores the head pointer which is pointing to the free list with range: 32<=x<64
 * free[1] stores the head pointer which is pointing to the free list with range: 64<=x<96
 */
void * free_list[SIZE];

void * ULIMIT=NULL;//store the upper limit of the heap, mm_check() use it
void * LLIMIT=NULL;//store the lower limit of the heap, mm_check() use it 


void * extend_heap(size_t words);

/**********************************************************
 * For this lab, the team choose Segregated free list to 
 * implment the functionality.LIFO is used for adding free 
 * block into the free list. For find out a fit free 
 * block to allocate, we select first fit scenario.  
 * Every time, mm_free gets called, coalescing is also run 
 *********************************************************/


/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
 int mm_init(void)
 {
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
         return -1;
     PUT(heap_listp, 0);                         	// alignment padding
     PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));	// prologue header
     PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));   	// prologue footer
     PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    	// epilogue header
     ULIMIT=heap_listp + (3 * WSIZE); //set the lower limit of the heap to LLMIT
     LLIMIT=heap_listp + (2 * WSIZE); //set initial value for the lower limit of the heap to LLMIT
     heap_listp += DSIZE;
     
     //initialize the free list head pointer array
     int i;
     for(i=0;i<SIZE;i++){
     free_list[i]=NULL;
     }
         
     return 0;
 }

/**********************************************************
 * print_free_list
 * given the free list head pointer "current_class_sizep",
 * print the content of each node in a size class.
 **********************************************************/
void print_free_list(void *current_class_sizep)
{
    if (current_class_sizep == NULL)
       return;
    void *bp = current_class_sizep;
    int num = 0;
    printf("list content:\n");
    for (bp = current_class_sizep; bp > 0; bp = (void *)NEXT_FREE_BLKP(bp), num++)
    {
       PRETTY_PRINT_LIST(num, (size_t)GET_SIZE(HDRP(bp)), (size_t)bp, (size_t)HDRP(bp), (size_t)FTRP(bp), (size_t)PREV_PTR(bp),
		       (size_t)NEXT_PTR(bp), (size_t)PREV_FREE_BLKP(bp), (size_t) NEXT_FREE_BLKP(bp));
    }
}


/**********************************************************
 * print_free_list_traverse_class_size
 * print the content of each node in the all the free list
 * call print_free_list to print the content of each node 
 * in a size class.
 **********************************************************/
void print_free_list_traverse_class_size()
{
    int index = 0;
    for (; index < 8; index++)
    {
       printf("List class %d\n",index);
       print_free_list(free_list[index]);
    }
}


/**********************************************************
 * add_to_free_list_helper
 * Add a free block into the free list using LIFO policy
 * Covers 3 cases:
 * - insert at the head of the list
 * - insert in the middle of the list
 * - insert at the tail of the list
 **********************************************************/
static inline void add_to_free_list_helper(void *bp, int index)
{
  if (free_list[index] == NULL)
  {
    free_list[index]= bp;  	//The free block becomes the first element in the free list
    PUT(NEXT_PTR(bp), 0); 	//zero out dangling pointer
    PUT(PREV_PTR(bp), 0); 	//zero out dangling pointer
  }
  else 
  {
    void *traverse = free_list[index];
       PUT(PREV_PTR(bp), 0);
       PUT(NEXT_PTR(bp), (size_t)traverse);
       PUT(PREV_PTR(traverse), (size_t)bp);
       free_list[index]=bp;
       return;  //done with insertion
  }
}


/**********************************************************
 * remove_from_free_list
 * Remove a block from the free list 
 * Covers 4 cases:
 * - Remove from the head of the list
 * - Remove from the middle of the list
 * - Remove from the tail of the list
 **********************************************************/
static inline void remove_from_free_list(void *bp,  int index)
{
    if (free_list[index] == NULL)
       return;
    if (bp == free_list[index]) // head of the list
    {
       if (NEXT_FREE_BLKP(bp) != 0)
       {
	  PUT(PREV_PTR(NEXT_FREE_BLKP(bp)), 0);
	  free_list[index]= (void *)NEXT_FREE_BLKP(bp);
	  PUT(NEXT_PTR(bp), 0); //zero out dangling pointer
       }
       else  // There is only one element in the list, namely, block(bp)
       {
       	  PUT(PREV_PTR(bp), 0); //zero out dangling pointer
          PUT(NEXT_PTR(bp), 0); //zero out dangling pointer
	  free_list[index]=NULL;
       }
       return;
    }
    if (PREV_FREE_BLKP(bp) != 0 && NEXT_FREE_BLKP(bp) == 0) // tail of the list (We have some elements before)
    {
       PUT(NEXT_PTR(PREV_FREE_BLKP(bp)), 0);
       PUT(PREV_PTR(bp), 0); //zero out dangling pointer
       return;
    }
    if (PREV_FREE_BLKP(bp) != 0 && NEXT_FREE_BLKP(bp) != 0) // middle of the list
    {
       PUT(NEXT_PTR(PREV_FREE_BLKP(bp)), (size_t)NEXT_FREE_BLKP(bp));
       PUT(PREV_PTR(NEXT_FREE_BLKP(bp)), (size_t)PREV_FREE_BLKP(bp));
       PUT(PREV_PTR(bp), 0); //zero out dangling pointer
       PUT(NEXT_PTR(bp), 0); //zero out dangling pointer
       return;
    }
}


/*give a free block size, return the corresponding flist header*/
static inline int flist_hdrp(size_t size){
int i;
//printf("flist_hdrp size is %d\n",(int) size);
for(i=0;i<SIZE;i+=2){

if(boundary[i]>=size){
return i;
}

if(boundary[i+1]>=size){
return i+1;
}

}

return SIZE-1;
}

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 **********************************************************/
static inline void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    int tmp_p=0;
    int extra_p=0;
    if (prev_alloc && next_alloc) {       /* Case 1 */
        tmp_p= flist_hdrp(size);
	add_to_free_list_helper(bp,tmp_p );
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        tmp_p= flist_hdrp(size);
	extra_p=flist_hdrp(GET_SIZE(HDRP(NEXT_BLKP(bp))));
	remove_from_free_list(NEXT_BLKP(bp),extra_p);
	
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
	add_to_free_list_helper(bp, tmp_p);
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
	size += GET_SIZE(HDRP(PREV_BLKP(bp)));
	tmp_p= flist_hdrp(size);
	extra_p=flist_hdrp(GET_SIZE(HDRP(PREV_BLKP(bp))));
	remove_from_free_list(PREV_BLKP(bp),extra_p);
       	PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	add_to_free_list_helper(PREV_BLKP(bp), tmp_p);
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
	size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
	tmp_p= flist_hdrp(size);
	extra_p=flist_hdrp(GET_SIZE(HDRP(PREV_BLKP(bp))));
	remove_from_free_list(PREV_BLKP(bp),extra_p);
	extra_p=flist_hdrp(GET_SIZE(HDRP(NEXT_BLKP(bp))));
	remove_from_free_list(NEXT_BLKP(bp),extra_p);
	PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
	add_to_free_list_helper(PREV_BLKP(bp), tmp_p);
        return (PREV_BLKP(bp));
    }
}



/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
inline void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return NULL;

    /* Initialize free block header/footer/prev_ptr/next_ptr and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(PREV_PTR(bp), 0);                  // free block prev_ptr points to 0
    PUT(NEXT_PTR(bp), 0);                  // free block next_ptr points to 0
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header
    ULIMIT=HDRP(NEXT_BLKP(bp));

    /* Coalesce if the previous block was free */
    return bp;
}


/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
inline void * find_fit(size_t asize)
{

    int i = flist_hdrp(asize);
    void * flistp=free_list[i];
    while(i<SIZE){
    if(flistp!=NULL){
    void *baseptr = flistp;
    for (; baseptr > 0; baseptr = (void *)(NEXT_FREE_BLKP(baseptr))) 
    {
        if (asize <= GET_SIZE(HDRP(baseptr))) //found a suitable free block
	return baseptr;
        
	
    } 
    }//if(flistp!=NULL)
    i++;
    if(i<SIZE)
    flistp = free_list[i];
    }//while

    //no  suitable free block found
    return NULL;


}

/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t asize)
{
  /* Get the current block size */
  size_t bsize = GET_SIZE(HDRP(bp));
  int index=flist_hdrp(bsize);
  //void * flistp=free_list[index];
  if (!GET_ALLOC(HDRP(bp)))
     remove_from_free_list(bp,index);
  if (NEXT_FREE_BLKP(bp) != 0 || PREV_FREE_BLKP(bp) !=0)
     printf("Remove_from_free_list didn't zero out dangling pointers.\n");


  /* At this point, we need to decide whether to coalesce the splitted 
   * block to coalesce_candidate.
   * If so, coalesce first and then call add_to_free_list().
   * If not, just call add_to_free_list(); 
   * Note that we need 4 * WSIZE of overhead for a free block. */
  if (asize < bsize) // we are using the smaller size, namely, asize
  {
     if (bsize - asize >= 2 * DSIZE) 
     {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(bsize - asize, 0)); //free block header
        PUT(FTRP(NEXT_BLKP(bp)), PACK(bsize - asize, 0)); //free block footer
        PUT(PREV_PTR(NEXT_BLKP(bp)), 0);            //free block prev_ptr
        PUT(NEXT_PTR(NEXT_BLKP(bp)), 0);            //free block next_ptr
 	int index=flist_hdrp(GET_SIZE(HDRP(NEXT_BLKP(bp))));
 	add_to_free_list_helper(NEXT_BLKP(bp),index );
     }
     else
     {
        PUT(HDRP(bp), PACK(bsize, 1));
        PUT(FTRP(bp), PACK(bsize, 1));
     }
  }
  else if (asize == bsize)
  {
     PUT(HDRP(bp), PACK(bsize, 1));
     PUT(FTRP(bp), PACK(bsize, 1));
  }
  else //asize > bsize
  { 
     printf("Find_fit size is incorrect.\n");
  }
 
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    if(bp == NULL){
      return;
    }
    if(!GET_ALLOC(HDRP(bp))) return;
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    PUT(PREV_PTR(bp), 0);                  // free block prev_ptr points to 0
    PUT(NEXT_PTR(bp), 0);                  // free block next_ptr points to 0
    coalesce(bp);
}


/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(..)
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {

        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
	
    place(bp, asize);
    return bp;

}



/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    copySize = GET_SIZE(HDRP(oldptr));

    //realloc
    if (size > copySize){
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(oldptr)));
    size_t next=GET_SIZE(HDRP(NEXT_BLKP(oldptr)));

      if(!next_alloc)
      {//case 2
	if(copySize+next>size)
	{//satisfied, coalesce them
	size_t asize;
	if (size <= DSIZE)
        asize = 2 * DSIZE;
        else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
	//remove it from list;
	int index=flist_hdrp(next);
	remove_from_free_list(NEXT_BLKP(oldptr),index);
	PUT(HDRP(oldptr), PACK(copySize+next, 1));
        PUT(FTRP(oldptr), PACK(copySize+next, 1));
	place(oldptr, asize);
	return oldptr;
	}
	else if (copySize+next<size && GET_SIZE(HDRP(NEXT_BLKP(NEXT_BLKP(oldptr))))==0)
	{
		size_t tmp_i=size-(copySize+next);
	size_t asize;
	if (tmp_i <= DSIZE)
        asize = 2 * DSIZE;
        else
        asize = DSIZE * ((tmp_i + (DSIZE) + (DSIZE-1))/ DSIZE);
        size_t extendsize=MAX(asize,CHUNKSIZE);
	void * bpp=extend_heap(extendsize/WSIZE);
	if(bpp==NULL)
	return NULL;
	 
	int index=flist_hdrp(next);
	remove_from_free_list(NEXT_BLKP(oldptr),index);
	if (size <= DSIZE)
        asize = 2 * DSIZE;
        else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
	PUT(HDRP(oldptr), PACK(copySize+next+GET_SIZE(HDRP(bpp)), 1));
        PUT(FTRP(oldptr), PACK(copySize+next+GET_SIZE(HDRP(bpp)), 1));
	
	return oldptr;

	}
      }
      else
      {
	if (GET_SIZE(HDRP(NEXT_BLKP(oldptr)))==0)
	{
	size_t tmp_i=size-copySize;
	size_t asize;
	if (tmp_i <= DSIZE)
        asize = 2 * DSIZE;
        else
        asize = DSIZE * ((tmp_i + (DSIZE) + (DSIZE-1))/ DSIZE);
        size_t extendsize=MAX(asize,CHUNKSIZE);
	void * bpp=extend_heap(extendsize/WSIZE);
	if(bpp==NULL)
	return NULL;
	 
	if (size <= DSIZE)
        asize = 2 * DSIZE;
        else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
	PUT(HDRP(oldptr), PACK(copySize+GET_SIZE(HDRP(bpp)), 1));
        PUT(FTRP(oldptr), PACK(copySize+GET_SIZE(HDRP(bpp)), 1));
	
	return oldptr;
	}
      }

   }
      newptr = mm_malloc(size);
      if (newptr == NULL)
      return NULL;

      /* Copy the old data. */
    
      if (size < copySize)
        copySize = size;
      memcpy(newptr, oldptr, copySize);
      mm_free(oldptr);
      return newptr;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){

    int index = 0;
    //traverse all the free list
    for (; index < SIZE; index++)
    {
      	void * current_class_sizep=free_list[index];
	if (current_class_sizep != NULL)
       	{
    	void *bp = current_class_sizep;
    	for (bp = current_class_sizep; bp > 0; bp = (void *)NEXT_FREE_BLKP(bp))
    	{	
		if(GET_SIZE(HDRP(bp))==0)return 0;					//check whether the size of a free block is 0
		/* check whether the the consistency of the 
		 * header anthe footer
		 */
		if(GET_SIZE(HDRP(bp))!=(GET_SIZE(FTRP(bp))))return 0;
		if(GET_SIZE(bp-DSIZE)!=(GET_SIZE(HDRP(PREV_BLKP(bp)))))return 0;

		//check a free block is allocated or not
		if(GET_ALLOC(HDRP(bp))||GET_ALLOC(FTRP(bp)))return 0;
		//check whether bp exceed the heap's upper limit
		if(bp>=ULIMIT)return 0;
		//check whether bp exceed the heap's lower limit
		if(bp<=LLIMIT)return 0;
    	}
	}

    }

  //traverse the whole heap
  void * traverse = heap_listp;
  while(GET_SIZE(HDRP(traverse))!=0){
  if(traverse>ULIMIT||traverse<LLIMIT)return 0;			//check whether exceed the heap range
  if(FTRP(traverse)-DSIZE!=NEXT_BLKP(traverse))return 0;	//check whether gaps between blocks
  traverse=NEXT_BLKP(traverse);
  }


  return 1;
}
