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
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

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
#define PRINT_ADDR(p)  printf("0x%0x\n",p);
#define PRETTY_PRINT_LIST(num, size, bp, hdrp, ftrp,prev_ptr, next_ptr, prev_free_blkp, next_free_blkp) \
printf("Element %d: size=%lu, addr(bp)=0x%lx, addr(HDPR)=0x%lx, addr(FTRP)=0x%lx, \
       addr(PREV_PTR)=0x%lx, addr(NEXT_PTR)=0x%lx, \
       addr(PREV_FREE_BLKP)=0x%lx, addr(NEXT_FREE_BLKP)=0x%lx\n", \
       num, size, bp, hdrp, ftrp, prev_ptr, \
       next_ptr, prev_free_blkp, next_free_blkp)

void* heap_listp = NULL;
//void *free_listp;  // head of the free list (explicit list method)

/* Cases in coalecsing
 * This enum is passed to add_to_free_list to handle the 
 * pointer reassignment of the free list in different cases */
/*
typedef enum
{
  CASE1 = 0,
  CASE2,
  CASE3,
  CASE4
} cases;
*/
size_t boundary[20]={32,64,128,256,512,1024,2048,4096,8192,10240,12288,14336,16384,20480,22528,24576,26624,30720,32768,40960};
void * free_list[20];
void *extend_heap(size_t words);

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
 int mm_init(void)
 {
 //dont use heap_listp any more
   if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
         return -1;
     PUT(heap_listp, 0);                         // alignment padding
     
     /*store eight pointer*/
     /*
     free_list[0]=heap_listp+1*WSIZE; //for size 32=<x<64
     free_list[1]=heap_listp+2*WSIZE; //for size 64=<x<128
     free_list[2]=heap_listp+3*WSIZE; //for size 128=<x<256
     free_list[3]=heap_listp+4*WSIZE; //for size 256=<x<512
     free_list[4]=heap_listp+5*WSIZE; //for size 512=<x<1024
     free_list[5]=heap_listp+6*WSIZE; //for size 1024=<x<2048
     free_list[6]=heap_listp+7*WSIZE; //for size 2048=<x<4096
     free_list[7]=heap_listp+8*WSIZE; //for size 4096=<x<inifinite
     */
     PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
     PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));   // prologue footer
     PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    // epilogue header
     heap_listp += DSIZE;
    // free_listp = NULL;
     int i;
     for(i=0;i<8;i++){
     free_list[i]=NULL;
     }
         
     return 0;
 }

/**********************************************************
 * print_free_list
 **********************************************************/
 /*
void print_free_list(void *free_listp)
{
    if (free_listp == NULL)
       return;
    void *bp = free_listp;
    int num = 0;
    printf("list content:\n");
    for (bp = free_listp; bp > 0; bp = (void *)NEXT_FREE_BLKP(bp), num++)
    {
       PRETTY_PRINT_LIST(num, (size_t)GET_SIZE(HDRP(bp)), (size_t)bp, (size_t)HDRP(bp), (size_t)FTRP(bp), (size_t)PREV_PTR(bp),
		       (size_t)NEXT_PTR(bp), (size_t)PREV_FREE_BLKP(bp), (size_t) NEXT_FREE_BLKP(bp));
    }
}
*/
/**********************************************************
 * add_to_free_list_helper
 * Add a free block into the free list using address-ordered
 * policy
 * Covers 3 cases:
 * - insert at the head of the list
 * - insert in the middle of the list
 * - insert at the tail of the list
 **********************************************************/
void add_to_free_list_helper(void *bp, int index)
{
  if (free_list[index] == NULL)
  {
    //printf("insert to the empty list\n");
    free_list[index]= bp;  //The free block becomes the first element in the free list
    PUT(NEXT_PTR(bp), 0); //zero out dangling pointer
    PUT(PREV_PTR(bp), 0); //zero out dangling pointer
  }
  else 
  {
    void *traverse = free_list[index];
    if ((size_t)bp < (size_t)(traverse))  // insert at head
    {
    // printf("insert to the head of list\n");
       PUT(PREV_PTR(bp), 0);
       PUT(NEXT_PTR(bp), (size_t)traverse);
       PUT(PREV_PTR(traverse), (size_t)bp);
       free_list[index]=bp;
       return;  //done with insertion
    }
#if 1
    while (NEXT_FREE_BLKP(traverse) > 0) 
    {
       if ((size_t)NEXT_FREE_BLKP(traverse) < (size_t)bp)
          traverse = (void *)NEXT_FREE_BLKP(traverse); //continue traversing
       else
       {
          //insert before NEXT_FREE_BLKP(traverse)
	  //printf("insert to the middle of list\n");
	  PUT(PREV_PTR(bp), (size_t)traverse);
	  PUT(NEXT_PTR(bp), (size_t)NEXT_FREE_BLKP(traverse));
	  PUT(PREV_PTR(NEXT_FREE_BLKP(traverse)), (size_t)bp);
	  PUT(NEXT_PTR(traverse), (size_t)bp);
	  //printf("finish insert to the middle of list\n");
	  return;  //done with insertion
       }
    }
    //At this point, we've reached the last element of the list
    //Therefore, bp must be the largest addr in the list
    //So we insert block(bp) at the tail of the list
    // printf("insert to the end of list\n");
    PUT(PREV_PTR(bp), (size_t)traverse);
    PUT(NEXT_PTR(bp), 0);
    PUT(NEXT_PTR(traverse), (size_t)bp);
  }
#endif
}


/**********************************************************
 * remove_from_free_list
 * Remove a block from the free list 
 * Covers 4 cases:
 * - Remove from the head of the list
 * - Remove from the middle of the list
 * - Remove from the tail of the list
 **********************************************************/
void remove_from_free_list(void *bp,  int index)
{
    //free_list[i];
    //printf("remove from list: bp is %lx flistp is %lx\n",bp, GET(flistp));
    if (free_list[index] == NULL)
       return;
    if (bp == free_list[index]) // head of the list
    //if (PREV_FREE_BLKP(bp) == 0)
    {
       if (NEXT_FREE_BLKP(bp) != 0)
       {
          //printf("remove from list: 1");
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
       	//printf("remove from list: 2");
       return;
    }
    if (PREV_FREE_BLKP(bp) != 0 && NEXT_FREE_BLKP(bp) == 0) // tail of the list (We have some elements before)
    {
       //printf("remove from list: 3");
       PUT(NEXT_PTR(PREV_FREE_BLKP(bp)), 0);
       PUT(PREV_PTR(bp), 0); //zero out dangling pointer
       return;
    }
    if (PREV_FREE_BLKP(bp) != 0 && NEXT_FREE_BLKP(bp) != 0) // middle of the list
    {
    	//printf("remove from list: 4");
       PUT(NEXT_PTR(PREV_FREE_BLKP(bp)), (size_t)NEXT_FREE_BLKP(bp));
       PUT(PREV_PTR(NEXT_FREE_BLKP(bp)), (size_t)PREV_FREE_BLKP(bp));
       PUT(PREV_PTR(bp), 0); //zero out dangling pointer
       PUT(NEXT_PTR(bp), 0); //zero out dangling pointer
       return;
    }
}


/*give a free block size, return the corresponding flist header*/
int flist_hdrp(size_t size){
int i;
//printf("flist_hdrp size is %d\n",(int) size);
for(i=0;i<7;i++){
if(boundary[i]>=size){
return i;
}
}

return 7;

#if 0
int i=0;
switch (szie)
{
    case >2048:
    		i=7;break;
    case >1024:
    		i=6;break;
    case >512:
    		i=5;break;
    case >256:
    		i=4;break;
    case >128:
    		i=3;break;
    case >64:
    		i=2;break;
    case >32:
    		i=1;break;
    case >0:
    		i=0;break;
    default:
    		i=7;break;

}
return i;

#endif
}

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 **********************************************************/
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    int tmp_p=0;
    int extra_p=0;
    if (prev_alloc && next_alloc) {       /* Case 1 */
        //printf("case 1\n");
        tmp_p= flist_hdrp(size);
	//printf("1.Adding %lx to %d\n",bp,tmp_p);
	add_to_free_list_helper(bp,tmp_p );
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
    	//printf("case 2\n");
	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        tmp_p= flist_hdrp(size);
	extra_p=flist_hdrp(GET_SIZE(HDRP(NEXT_BLKP(bp))));
	remove_from_free_list(NEXT_BLKP(bp),extra_p);
	
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
	//printf("2.Adding %lx to %d\n",bp,tmp_p);
	add_to_free_list_helper(bp, tmp_p);
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
    	//printf("case 3\n");
	size += GET_SIZE(HDRP(PREV_BLKP(bp)));
	tmp_p= flist_hdrp(size);
	extra_p=flist_hdrp(GET_SIZE(HDRP(PREV_BLKP(bp))));
	remove_from_free_list(PREV_BLKP(bp),extra_p);
//	add_to_free_list_helper(PREV_BLKP(bp), GET(tmp_p));
       	PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	//printf("3.Adding %lx to %d\n",PREV_BLKP(bp),tmp_p);
	add_to_free_list_helper(PREV_BLKP(bp), tmp_p);
	//printf("3.1Adding %lx to %lx\n",PREV_BLKP(bp),(void *)GET(tmp_p));
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
    	//printf("case 4\n");
	size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
	tmp_p= flist_hdrp(size);
	extra_p=flist_hdrp(GET_SIZE(HDRP(PREV_BLKP(bp))));
	remove_from_free_list(PREV_BLKP(bp),extra_p);
	extra_p=flist_hdrp(GET_SIZE(HDRP(NEXT_BLKP(bp))));
	remove_from_free_list(NEXT_BLKP(bp),extra_p);
	PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
	//printf("4.Adding %lx to %d\n",PREV_BLKP(bp),tmp_p);
	add_to_free_list_helper(PREV_BLKP(bp), tmp_p);
	//printf("4.Adding %lx to %lx\n",PREV_BLKP(bp),(void *)GET(tmp_p));
        return (PREV_BLKP(bp));
    }
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t words)
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
    //printf("extend heap\n");

    /* Coalesce if the previous block was free */
    //printf("extend call coalesce\n");
    return coalesce(bp);
}


/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * find_fit(size_t asize)
{

    int i = flist_hdrp(asize);
    void * flistp=free_list[i];
     //print_free_list(flistp);
    while(i<=7){
    if(flistp!=NULL){
    void *baseptr = flistp;
    //void * tmp_p=NULL;
    for (; baseptr > 0; baseptr = (void *)(NEXT_FREE_BLKP(baseptr))) 
    {
        if (asize <= GET_SIZE(HDRP(baseptr))) //found a suitable free block
        {/*
	   if(tmp_p==NULL)
		tmp_p=baseptr;
	   else if(GET_SIZE(HDRP(baseptr))<GET_SIZE(HDRP(tmp_p)))
	   	tmp_p=baseptr;
	   else ;
	*/
	//printf("find_fit find %lx\n",baseptr);
	return baseptr;
        }
	//return tmp_p;
    }//for 
    //printf("2. exiting find_fit\n");
    //if(tmp_p!=NULL)
    //return tmp_p;
    }//if(flistp!=NULL)
    i++;
    if(i<=7)
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

	coalesce(NEXT_BLKP(bp));
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
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    PUT(PREV_PTR(bp), 0);                  // free block prev_ptr points to 0
    PUT(NEXT_PTR(bp), 0);                  // free block next_ptr points to 0
    //printf("mm_free call coalesce\n");
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
    #if 0
	printf("pass find_fit\n");
#endif
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    //printf("malloc size =%d\n",extendsize);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
#if 0
	printf("before place\n");
#endif	
    place(bp, asize);
#if 0
	printf("pass place\n");
#endif
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
    if (size < copySize){
    //copySize = size;
    //splitting
    memcpy(oldptr, oldptr, size);
    if (copySize - size >= 2 * DSIZE) 
     {
        PUT(HDRP(oldptr), PACK(size, 1));
        PUT(FTRP(oldptr), PACK(size, 1));
        PUT(HDRP(NEXT_BLKP(oldptr)), PACK(copySize - size, 0)); //free block header
        PUT(FTRP(NEXT_BLKP(oldptr)), PACK(copySize - size, 0)); //free block footer
        PUT(PREV_PTR(NEXT_BLKP(oldptr)), 0);            //free block prev_ptr
        PUT(NEXT_PTR(NEXT_BLKP(oldptr)), 0);            //free block next_ptr

	coalesce(NEXT_BLKP(oldptr));

     }
     else
     {
        PUT(HDRP(oldptr), PACK(size, 1));
        PUT(FTRP(oldptr), PACK(size, 1));
     }
          return oldptr;
    }

#if 0
//realloc
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(oldptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(oldptr)));
    if(prev_alloc&&!next_alloc){//case 2
	size_t next=GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
	size_t itself= GET_SZIE(HDRP(oldptr));
	if(next+itself)

	}
    else if(!prev_alloc&&next_alloc){//case 3
	size_t next=GET(FTRP(PREV_BLKP(oldptr)));
	size_t itself= GET_SZIE(HDRP(oldptr));


	}
    else if(!prev_alloc&&!next_alloc){//case 4
	


	}
//reallo
    else;

#endif
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    /* Copy the old data. */
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
  return 1;
}
