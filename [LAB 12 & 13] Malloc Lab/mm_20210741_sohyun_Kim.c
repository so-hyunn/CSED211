/*
 * 
 * 20210741 
 * kimsohyun 김소현
 * dynamic malloc allocator을 구현하기 위해 explicit free list를 사용했다. 
 * 그리고 동적 할당을 위해 free block을 찾기 위한 검색 방법으로 first-fit 방식을 선택했다.
 * implicit free list로 구현한 교과서 및 수업 ppt 코드에서 출발하여 계속해서 성능을 개선하였다. 
 * 그 결과 테스트 점수 86점을 받을 수 있었다. 
 * 
 * free block들은 free list에 LIFO policy대로 관리된다. 
 * free block은 | padding | prev free block | next free block | header | footer | epilogue header |의 구조로 구성되며
 * 최소 block size 는 따라서 24B다.
 * allocated block도 동일한 사이즈다. 
 * 모든 block을 8byte 단위로 정렬하여 header와 footer의 마지막 3개의 bit는 항상 0인데,
 * 이 중 가장 오른쪽 비트를 allocated bit으로 설정했는데 1이면 해당 block이 현재 할당되었다는 뜻이고, 0이면 free 상태를 나타낸다. 
 * 자세한 사항은 보고서에 작성하였다. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4      
#define DSIZE 8    
#define CHUNKSIZE  (1<<12) 

#define MAX(x, y) ((x) > (y)? (x) : (y))  

#define PACK(size, alloc)  ((size) | (alloc)) 

#define GET(ptr)       (*(unsigned int *)(ptr))           
#define PUT(ptr, val)  (*(unsigned int *)(ptr) = (val))   

#define GET_SIZE(ptr)  (GET(ptr) & ~0x7)                   
#define GET_ALLOC(ptr) (GET(ptr) & 0x1)                 

#define HDRP(bp) ((char *)(bp) - WSIZE)                     
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 

#define PREV_PTR(ptr) ((void*)(ptr))
#define NEXT_PTR(ptr) ((void*)(ptr) + WSIZE)

#define PREV(ptr) (*(void**)(ptr))
#define NEXT(ptr) (*(void**)(NEXT(ptr)))

// function declarations 
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_next_fit(size_t asize);
static void *find_first_fit(size_t asize);
static void insert_freelist(void* bp);
static void delete_freelist(void* bp);

// for heap consistency checker
static int mm_check(void);
static void check_everyblock_marked_free();
static void check_contiguous_freeblock_coalescing();
static void check_everyblock_in_freelist();
static void check_pointers_valid_freeblocks();
static void check_allocated_blocks_overlap();
static void check_pointers_valid_heap_address();

static void *root_ptr; 
static void *heap_list;  
//static void *prev_list; //next fit


/*
 * mm_init 
 * mm_init 함수에서는 필요한 초기화를 진행해준다.
 * 초기 힙 공간을 할당해주는 역할을 한다. 
 * | padding | prev free block | next free block | header | footer | epilogue header |
 * 24byte의 공간을 할당하고, 각각의 공간의 기능에 따라 PUT함수를 사용하여 값을 지정해준다.
 * 문제가 있으면 -1을 리턴한다. 
 */
int mm_init(void) 
{
    if ((heap_list = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1; // 초기화에 문제 있으면 -1 리턴한다
    PUT(heap_list, 0); // alignment padding  
    PUT(heap_list + (1*WSIZE), 0);  // prev free block
    PUT(heap_list + (2*WSIZE), 0);  // next free block
    PUT(heap_list + (3*WSIZE), PACK(DSIZE, 1)); // prologue header
    PUT(heap_list + (4*WSIZE), PACK(DSIZE, 1)); // prologue footer
    PUT(heap_list + (5*WSIZE), PACK(0, 1)); //epilogue header
    root_ptr = heap_list + WSIZE; //update root pointer which has free block list start address
    heap_list += (4*WSIZE);                     
    //prev_list = heap_list;
  
    // 만약 비어있다면
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) 
        return -1; // 초기화에 문제 있으면 -1 리턴한다
    return 0;
}


/* 
 * mm_malloc
 * 인자로 넘겨받는 크기의 block을 할당하고 할당된 block의 pointer을 리턴하는 함수이다. 
 * 예외처리 : 만약 heap_list가 Null이면 초기화가 필요하므로 초기화 해주고, 사이즈가 0이면 옳지 않은 값이므로 무시해준다. 
 * size를 alignment를 해주는 코드를 거친 후에 이 크기에 맞는 free block이 list에 있는지 find_first_fit 함수를 통해 검색한다. 
 * 검색 결과가 null이 아니고 있다면 place 함수를 통해 할당한다. 만약 맞는 block이 없다면 매크로로 구현한 MAX를 이용하여 할당 크기를 정한다. 
 * 적절한 블럭이 없다면, 메모리에 힙 공간을 더 요구하는 함수인 extend_heap을 사용하여 더 할당받은 후에 블럭을 할당해준다. 그리고 최종적으로 할당한 블록을 가리키는 포인터 bp를 리턴한다. 
 */
void *mm_malloc(size_t size) 
{
    size_t asize;      
    size_t extendsize; // 만약 적합한 블럭이 없을 때 늘려야 하는 사이즈를 저장하기 위한 변수
    char *bp;      

    if (heap_list == 0) //에러처리
    {
        mm_init();
    }
  
    if (size == 0) //에러처리 : 사이즈가 0일때 
        return NULL;

    // 할당하라고 한 사이즈를 alignment에 맞춰준다
    if (size <= DSIZE)                                         
        asize = 2*DSIZE;                                     
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    
    // 해당 사이즈에 적합한 블럭이 있는지 find_first_fit 함수를 이용해 찾는다.
    if ((bp = find_first_fit(asize)) != NULL) {  
        place(bp, asize);  // 있다면 배치한다         
        return bp;
    }

    // 만약 없다면 메모리로부터 힙 공간을 할당받아야 하기 때문에 사이즈를 체크하고 공간을 확보한다.
    extendsize = MAX(asize,CHUNKSIZE);                
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)  
        return NULL;                                  
    place(bp, asize); 
                          
    return bp;
} 


/*
 * mm_free
 * free block을 explicit free list에 추가해준다.
 * 이때 free list에서의 정보들을 업데이트 해준다. 
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp)); //GET_SIZE 매크로를 통해 size를 받은 후에 PUT 매크로로 정보를 넣어준다.
    PUT(NEXT_PTR(bp), 0);
    PUT(PREV_PTR(bp), 0);
    //next, prev block을 가리키는 포인터 값을 0으로 만들어서 연결을 제거
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    //해당 블럭의 할당 상태를 나타내는 비트를 0으로 넣어 free 상태임을 명시한다. 
    coalesce(bp);
}


/*
 * mm_realloc
 * 특정 조건을 만족하는 최소 사이즈의 할당된 블럭을 가리키는 포인터를 리넡하는 함수이다.
 * 먼저 ptr이 NULL이면 mm_malloc(size)를 호출한 것과 동일하게 처리한다. 
 * 이후 size이 0이라면 mm_free(ptr)을 호출한 것과 동일하게 처리한다.
 * ptr이 NULL이 아니라면, 다시 할당해주는 과정을 거친다. 
 * 새로 할당하는 사이즈와 현재 사이즈를 비교하여 늘어나야 할지를 결정한다.
 * 이를 조건문을 이용하여 처리해주고, 새로 할당하게 되면 free list에 추가하고, 원래의 block은 free list에서 삭제해준다. 
 */
 
void *mm_realloc(void *ptr, size_t size)
{
    
    if (ptr == NULL) //if ptr is NULL
        return mm_malloc(size); // mm_malloc(size) 호출과 동일 처리
    if (size == 0)
    {
        mm_free(ptr); // mm_free(ptr) 호출과 동일 처리
        return NULL;
    }
    else 
    {
        size_t old_size, new_size, if_next_alloc, sum;
        void* old_ptr = ptr;
        void* new_ptr;

        old_size = GET_SIZE(HDRP(ptr)); //이전 블럭 사이즈
        new_size = size + 2*WSIZE; // header, footer 공간 확보
        
        if (new_size > old_size) //만약 할당할 사이즈가 이전 블럭 사이즈보다 크다면
        {
            sum = old_size + GET_SIZE(HDRP(NEXT_BLKP(ptr))); //다음 블럭과 합친 사이즈를 계산해본다
            int addto_nextblock = !GET_ALLOC(HDRP(NEXT_BLKP(ptr))) && (sum >= new_size);
            //다음 block이 할당되어 있지 않고, 합친 사이즈가 새로 할당하고자 하는 사이즈보다 큰지 확인한다. 
            if (addto_nextblock) //만약 그렇다면 다음 블럭과 합쳐서 공간을 확보하면 된다
            {
                delete_freelist(NEXT_BLKP(ptr)); //그러면 다음 블럭을 free list에서 삭제한다. 
                //이때 이미 ptr은 free list 안에 있으므로 딱히 추가해줄 필요가 없다. 
                //그리고 새로 합친 사이즈와 할당되었다는 정보를 반영해준다. 
                PUT(HDRP(ptr), PACK(sum, 1));
                PUT(FTRP(ptr), PACK(sum, 1));
                return ptr;
            }
            else //만약 다음 block과 합칠 수 없는 상황이라면 아예 새로 할당해야한다.
            {
                new_ptr = mm_malloc(new_size);
                place(new_ptr, new_size);
                memcpy(new_ptr, ptr, new_size); //이때 원래 block에 있던 정보를 가져오고, 나머지 공간은 초기화하지 않고 놔둔다. 
                mm_free(ptr); //새로 pointer을 할당했으므로 new_ptr만 남기고 삭제한다. 
                return new_ptr;
            }
        }
    return ptr;
 }
 return NULL;     
}

 
/*
 * coalesce 함수는 블록 free에서 네 가지 케이스로 나누어 인접한 블록과의 연결을 수행하도록 한다. 
 * 네 가지 케이스를 나누어서 처리하고, 이때 새로 연결한 포인터를 free list에 추가한다. 
 * 이외에 합칠 때 block의 정보들을 수정한다. 
 */
static void *coalesce(void *bp) 
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); //이전 블럭 footer 포인터
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); //다음 블럭 header 포인터
    size_t size = GET_SIZE(HDRP(bp)); // 블럭 사이즈

    // 케이스는 보고서에 첨부한 그림에 해당하는 케이스 번호다
    /* Case 2 */
    // 다음 블럭이 free인 경우 합친다
    if (prev_alloc && !next_alloc) 
    {     
        delete_freelist(NEXT_BLKP(bp)); //다음 블럭을 합치기 때문에 다음 블럭을 지정한 포인터는 free list에서 삭제해야함
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); //사이즈는 다음 블럭 사이즈를 더한 크기다
        PUT(HDRP(bp), PACK(size, 0)); 
        PUT(FTRP(bp), PACK(size,0));
        // free하다는 정보와 새로 업데이트된 사이즈 정보 넣음
    }
    /* Case 3 */
    // 앞 블럭이 free한 경우 앞과 합친다
    else if (!prev_alloc && next_alloc) 
    {      
        delete_freelist(PREV_BLKP(bp)); // 앞의 블럭 지정 포인터를 리스트에서 삭제, 앞과 같은 방식임
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp); // 이 경우 포인터가 앞의 블럭 지정 포인터로 업데이트 되어야 함
    }
    /* Case 4 */
    // 만약 앞, 뒤 둘다 free라면 다 합친다
    else if (!prev_alloc && !next_alloc)
    {                         
        delete_freelist(PREV_BLKP(bp)); //따라서 앞, 뒤 블럭 모두 리스트에서 삭제
        delete_freelist(NEXT_BLKP(bp));     
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); //사이즈는 둘의 사이즈를 더한다

        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));

        bp = PREV_BLKP(bp); // 이 경우에도 앞의 블럭 지정하는 포인터로 업데이트
    }
    //prev_list = bp;

    /* Case 1 */ 
    insert_freelist(bp); //이제 새로 할당한 블럭을 free list에 넣는다
    return bp;
}


/* 
 * extend_heap 함수는 메모리에 추가로 공간을 요구하는 함수이다.
 * 정렬을 유지하고 요구하는 힙 크기를 짝수로 만들어준다. 
 * 그리고 mem_sbrk 함수를 이용하여 해당 사이즈만큼 메모리에 힙 공간을 요구한다. 
 * 할당한 후 coalesce 함수를 리턴한다. 
 * 이에 따라서 새롭게 할당한 공간 주위에 free block이 있는지 확인하고 연결 작업을 하고, free list에 추가하게 된다. 
 */
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; //짝수로 align
    
    if ((long)(bp = mem_sbrk(size)) == -1)  // mem_sbrk 함수를 이용하여 힙 공간을 메모리에서 더 가져온다
        return NULL;  // 에러나면 리턴                                        

    // free block의 정보 초기화해준다.
    PUT(HDRP(bp), PACK(size, 0)); //free block header   
    PUT(FTRP(bp), PACK(size, 0)); // free block footer
    PUT(NEXT_PTR(bp), 0); // set next block ptr
    PUT(PREV_PTR(bp), 0);  // set prev block ptr
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); //새로운 free 블럭의 에필로그를 정해준다. 

    // Coalesce 함수 불러서 연결한다.
    return coalesce(bp);                                          
}


/*
* place 함수는 할당 크기와 위치를 인자로 받는다. 
* 넣을 위치의 블록 사이즈를 계산하고, 할당할 크기보다 블록 사이즈가 크면 나머지를 splitting한다. 
* 아니면 블록 할당을 하면 된다. 그리고 할당 위치의 free block을 list에서 삭제해준다. 
*/
static void place(void *bp, size_t asize)
{
    size_t size = GET_SIZE(HDRP(bp));  //블럭 사이즈 가져온다
    size_t over_size = size - asize; //할당할 사이즈와의 차이를 계산한다
    delete_freelist(bp); //free list에서 삭제한다
    if ((over_size) >= (2*DSIZE)) 
    //splitting block 과정
    // 할당하고자 하는 사이즈가 공간보다 더 작게 할당되면 공간을 쪼개는 것이 단편화에 좋다.
    // 남은 공간을 free라고 지정해주지 않으면 남아있는 공간이 할당하기에 충분함에도 불구하고
    // 할당되어 있다고 인식되기 때문에 할당할 수 없는 문제가 발생하기 때문이다.
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        // 따라서 여기서 할당을 사이즈만큼 해준 후
        bp = NEXT_BLKP(bp); // 포인터를 다음 블럭으로 업데이트 한 후
        // 나머지 사이즈만큼을 free 블럭으로 명시해준다
        PUT(HDRP(bp), PACK(over_size, 0));
        PUT(FTRP(bp), PACK(over_size, 0));
        PUT(NEXT_PTR(bp), 0);
        PUT(PREV_PTR(bp), 0);
        // free 블럭이므로 나머지 정보 0으로
        coalesce(bp); // 추가 연결 작업을 수행해준다
    }
    
    else 
    { 
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
    }
    //prev_list = bp;      
}


/* Next Fit function */
// 보고서에 작성하였는데, 과정을 표현하기 위해 주석처리 해놓았다. 
/*
static void *find_next_fit(size_t asize)
{
    void* find_ptr;

    for (find_ptr = prev_list; GET_SIZE(HDRP(find_ptr))!=0; find_ptr = NEXT_BLKP(find_ptr))
    {
        if (GET_ALLOC(HDRP(find_ptr)) == 0 && GET_SIZE(HDRP(find_ptr)) >= asize)
        {
            prev_list = find_ptr;
            return find_ptr;
        }        
    }

    find_ptr = heap_list;
    while (find_ptr < prev_list)
    {
        find_ptr = NEXT_BLKP(find_ptr);

        if (GET_ALLOC(HDRP(find_ptr)) == 0 && GET_SIZE(HDRP(find_ptr)) >= asize)
        {
            prev_list = find_ptr;
            return find_ptr;
        }
        return NULL;
    }
}
*/


/*
 * find_first_fit 함수는 블록 할당할 위치를 찾는다. 
 * 처음부터 검색하기 위해 bp 포인터 위치를 root_ptr로 받아온다. 
 * 그리고 GET(NEXT_PTR(bp))를 통해 다음 블록으로 이동하여 for 문을 null을 가리킬때까지 수행한다. 
 * 이때 헤더 정보에 접근하여 할당되어 있지 않고, 사이즈가 요구하는 것보다 크면 원하는 블록을 찾은 것이므로 검색을 종료한다. 
 * 찾지 못하면 NULL을 리턴한다. 
*/
static void *find_first_fit(size_t asize)
{
    void *bp;
    
    for (bp = GET(root_ptr); bp != 0; bp = GET(NEXT_PTR(bp))) //처음 위치를 GET(root_ptr)로 받아와서 for문을 돌면서 전체 블럭을 확인한다
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) //조건에 맞는 블록을 찾으면
        {
            return bp; //리턴한다.
      }
    }
    return NULL;   
}


/*
 * insert_freelist 함수는 free list의 시작에 새로운 free block을 추가한다.
 * LIFO 방법을 선택했기 때문에 이것은 연결 리스트의 시작에 새로운 노드를 넣는 방식과 동일해 간단하다. 
 * free list의 시작 앞에 연결해주고 해당하는 포인터 값을 업데이트한다.
*/
static void insert_freelist(void* bp)
{
    void* prev_head= GET(root_ptr); //free list의 시작 포인터를 받아온다
    if (prev_head != NULL) //null이 아니면
    {
        PUT(PREV_PTR(prev_head), bp); // 그 이전을 가리키는 포인터에 넣고자 하는 포인터를 저장하고
    }
    PUT(NEXT_PTR(bp), prev_head); // 원래 시작과 연결해주어서 링크드 리스트에서 (새로운 포인터) -> (기존 시작 포인터) 가 되게끔 한다
    PUT(root_ptr, bp); // free list 시작을 업데이트 해준다
}


/*
 * delete_freelist 함수는 free list에서 인자로 받은 포인터에 해당하는 블록을 삭제한다.
 * 이것은 링크드 리스트 안에서 삭제하는 것과 방식이 똑같은데, 따라서 세부 경우를 나눠주어 처리해야한다. 
 * 앞과 뒤에 어떻게 연결되어 있는지 판단하는 것이 필요하다. 
*/
static void delete_freelist(void* bp)
{
    void* prev_ptr = GET(PREV_PTR(bp)); // 앞 블럭 가리키는 포인터
    void* next_ptr = GET(NEXT_PTR(bp)); // 뒤 블럭 가리키는 포인터

    if (prev_ptr && next_ptr) // 리스트의 중간에 있다면
    {
        PUT(PREV_PTR(next_ptr), prev_ptr);
        PUT(NEXT_PTR(prev_ptr), next_ptr);
    }
    else if (prev_ptr && !next_ptr) // 리스트의 뒤에 있다면 
    {
        PUT(NEXT_PTR(prev_ptr), next_ptr);
    }
    else if (!prev_ptr && next_ptr) // 리스트의 앞에 있다면
    {
        PUT(PREV_PTR(next_ptr), 0);
        PUT(root_ptr, next_ptr);
    }
    else // 리스트 앞뒤에 블럭이 없다면
    {
        PUT(root_ptr, next_ptr);
    }
    PUT(NEXT_PTR(bp), 0);
    PUT(PREV_PTR(bp), 0);
}



/*
 * 다음부터는 heap consistency checker를 구현하기 위해 만든 함수들이다. 
 * pdf에 명시된 총 6개의 조건을 확인하기 위해 사용자 정의 함수 6개를 만든 후,
 * mm_check에 넣음으로써 구현해주었다.
 */


// 1. Is every block in the free list marked as free?
static void check_everyblock_marked_free()
{
    void* check_free;
    for (check_free = root_ptr; check_free!= NULL;check_free =  GET(NEXT_PTR(check_free)))
    {
        if (GET_ALLOC(HDRP(check_free))) // 반복문 순회하면서 할당된 블럭 있는지 확인
        {
            printf("ERROR : exist block that is not marked allocated in free list");
        }
    }
    printf("1번 확인 완료\n");
}

/*
// 2. Are there any contiguous free blocks that somehow escaped coalescing?
static void check_contiguous_freeblock_coalescing()
{
    void* check_free;
    for (check_free = root_ptr; check_free!= NULL;check_free =  GET(NEXT_BLKP(check_free)))
    {
        if (!GET_ALLOC(HDRP(check_free)) && !GET_ALLOC(HDRP(NEXT_BLKP(check_free))))
        {
            printf("ERROR : exist contiguous free blocks in free list");
        }
    }
    printf("2번 확인 완료\n");
}


// 3. Is every free block actually in the free list?
static void check_everyblock_in_freelist()
{

}

// 4. Do the pointers in the free list point to valid free blocks?
static void check_pointers_valid_freeblocks()
{

}

// 5. Do any allocated blocks overlap?
static void check_allocated_blocks_overlap()
{

}

// 6. Do the pointers in a heap block point to valid heap address?
static void check_pointers_valid_heap_address()
{

}

*/

static int mm_check(void)
{
    printf("mm_check start\n");
    check_everyblock_marked_free();
    //heck_contiguous_freeblock_coalescing();
    //check_everyblock_in_freelist();
    //check_pointers_valid_freeblocks();
    //check_allocated_blocks_overlap();
    //check_pointers_valid_heap_address();
    printf("mm_check successfuly finished\n");
}



