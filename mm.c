/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "team6",
    /* First member's full name */
    "Park sunghwan",
    /* First member's email address */
    "psung9510@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8 
#define CHUNKSIZE (1<<12)

#define MAX(x,y) ((x) > (y) ? (x): (y))

// 헤더와 푸터에 저장할 수 있는 값 리턴
#define PACK(size, alloc) ((size) | (alloc))

/* 크기와 할당 비트를 통합해서 헤더와 푸터에 저장할 수 있는 값을 리턴*/
#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* 주소 p의 헤더 또는 푸터의 SIZE와 할당 비트를 리턴한다.*/
#define GET_SIZE(p)   (GET(p) & ~0x7) // 뒤에 3비트를 제외하고 읽어옴
#define GET_ALLOC(p)  (GET(p) & 0x1) // 할당 가용 확인

/* 각각 블록 헤더와 풋터를 가리키는 포인터를 리턴한다.*/
#define HDRP(ptr)    ((char *)(ptr) - WSIZE)
#define FTRP(ptr)    ((char *)(ptr) + GET_SIZE(HDRP(ptr))- DSIZE)

/* 다음과 이전 블록 포인터를 각각 리턴한다.*/
#define NEXT_BLKP(ptr)   (((char *)(ptr) + GET_SIZE((char *)(ptr) - WSIZE))) 
#define PREV_BLKP(ptr)   (((char *)(ptr) - GET_SIZE((char *)(ptr) - DSIZE)))


static void *extend_heap(size_t);
static void *coalesce(void *ptr);
// static void *find_fit(size_t asize);
static void place(void *ptr, size_t asize);
static void* next_fit(size_t asize);

static char *heap_listp;
static char *last_ptr;
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   
    // mem_sbrk: 힙 영역을 incr(0이 아닌 양수) bytes 만큼 확장하고, 새로 할당된 힙 영역의 첫번째 byte를 가리키는 제네릭 포인터를 리턴함
    /* 비어있는 heap을 만든다.*/
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1){
        return -1;
    };
        
    PUT(heap_listp, 0);                             // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    // Prologue header 
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));     // Prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));          // Epilogue header
    heap_listp += (2*WSIZE); 

    // 두 가지 다른 경우에 호출된다.
    // (1) 힙이 초기화 될때 (2) mm_malloc이 적당한 맞춤fit을 찾지 못했을 때
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
        return -1;
    }
    last_ptr = heap_listp;

    return 0;
}

static void *extend_heap(size_t words)
{
    // 요청한 크기를 인접 2워드의 배수(8바이트)로 반올림하여, 그 후에 추가적인 힙 공간 요청
    char *ptr;
    size_t size;
    // 요청한 크기를 2워드의 배수로 반올림하고 추가 힙 공간을 요청함
    size = (words %2) ? (words+1)*WSIZE : words * WSIZE;
    if((long)(ptr = mem_sbrk(size)) == -1)
        return NULL;


    PUT(HDRP(ptr), PACK(size, 0));  //free 블록의 header
    PUT(FTRP(ptr), PACK(size, 0));  //free 블록의 footer
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1)); // new epilogue header

    return coalesce(ptr);
};

// 할당된 블록을 합칠 수 있는 경우 4가지에 따라 메모리 연결
static void *coalesce(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    if (prev_alloc && next_alloc){              // Case 1 : 현재만 반환시
        last_ptr = ptr;
        return ptr;
    }
    else if(prev_alloc && !next_alloc){         // Case 2 : 다음 블록과 병합
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    }
    else if(!prev_alloc && next_alloc){         // Case 3 : 이전 블록과 병합
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(FTRP(ptr), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size,0));
        ptr = PREV_BLKP(ptr);
    }
    else{                                       // Case 4 : 이전 블록과 다음 블록 병합
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(FTRP(NEXT_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }
    last_ptr = ptr;
    return ptr;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size){
    size_t asize;
    size_t extendsize;
    char *ptr;

    if (size == 0){
        return NULL;
    }
    // size를 바탕으로 헤더와 푸터의 공간 확보
    // 8바이트는 정렬조건을 만족하기 위해
    // 추가 8바이트는 헤더와 푸터 오버헤드를 위해서 확보
    if (size <= DSIZE){
        asize = 2*DSIZE;
    }else{
        asize = DSIZE*((size+(DSIZE) + (DSIZE-1)) / DSIZE);
    }

    // 가용 블록을 가용리스트에서 검색하고 할당기는 요청한 블록을 배치한다.
    if((ptr = next_fit(asize)) !=NULL){
        place(ptr, asize);
        last_ptr = ptr;
        return ptr;
    }

    //맞는 블록을 찾기 못한다면 새로운 가용 블록으로 확장하고 배치한다.
    extendsize = MAX(asize, CHUNKSIZE);
    if ((ptr = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(ptr, asize);
    last_ptr = ptr;

    return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
static void *find_fit(size_t asize){
    char *ptr = heap_listp;

    ptr = NEXT_BLKP(ptr);

    while ( GET_SIZE(HDRP(ptr)) < asize || GET_ALLOC(HDRP(ptr)) == 1 )
    {
        ptr = NEXT_BLKP(ptr);

        if (GET_SIZE(HDRP(ptr)) == 0){        //Epilogue를 만났을 때
            return NULL;
        }
    }
    return ptr;
}
*/

static void* next_fit(size_t asize)
{
    char* ptr = last_ptr;

    for (ptr = NEXT_BLKP(ptr); GET_SIZE(HDRP(ptr))!=0; ptr = NEXT_BLKP(ptr))
    {
        if (GET_ALLOC(HDRP(ptr)) == 0 && GET_SIZE(HDRP(ptr)) >= asize)
        {
            last_ptr = ptr;
            return ptr;
        }        
    }

    ptr = heap_listp;
    while (ptr < last_ptr)
    {
        ptr = NEXT_BLKP(ptr);

        if (GET_ALLOC(HDRP(ptr)) == 0 && GET_SIZE(HDRP(ptr)) >= asize)
        {
            last_ptr = ptr;
            return ptr;
        }
    }
    return NULL ;
}


// 
static void place(void *ptr, size_t asize){
    // 맞는 블록을 찾으면 요청한 블록을 배치하고 초과부분을 분할한다.
    size_t csize = GET_SIZE(HDRP(ptr));

    if((csize - asize) >= (2*DSIZE)){
        //가용 블록에 사이즈 - 요청한 블록의 사이즈 각 더블워드*2 크거나 같을때
        //요청 블록을 넣고 남은 사이즈는 가용 블록으로 분할
        PUT(HDRP(ptr), PACK(asize, 1));
        PUT(FTRP(ptr), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(csize - asize, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(csize - asize, 0));
    }else{
        //할당하고 남은 블록이 더블워드*2보다 작다며 나누지 않고 할당
        // 남은 블록이 더블워드*2보다 작은 경우는 데이터를 담을 수 없음
        PUT(HDRP(ptr), PACK(csize, 1));
        PUT(FTRP(ptr), PACK(csize, 1));
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);  // 메모리의 특정한 부분으로부터 얼마까지의 부분을 다른 메모리 영역으로 복사해주는 함수(old_bp로부터 copySize만큼의 문자를 new_bp로 복사해라)
    mm_free(oldptr);
    return newptr;
}
