/* 20210741 김소현 */
/* sooohyun@postech.ac.kr*/


#include <stdio.h>
#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>



typedef struct
{
    int valid;
    unsigned tag;
    int count_LRU;
} line;



typedef struct
{
    line* lines;
} set;



typedef struct
{

    set* sets;
    int s; //set num
    int E; // line num
    int b;

} Cache;


int check_hit(set* cache_set, int n, int tag, int* hit);
int check_miss(set* cache_set, int n, int tag, int* miss);
int check_eviction(set* cache_set, int E, int tag, int* eviction);
void update(Cache* cache, int verbose, int address, int* hit, int* miss, int* eviction);


int main(int argc, char* argv[])

{

    Cache cache = {};

    int hit = 0;
    int miss = 0;
    int eviction = 0;

    FILE* file = 0;
    int verbose = 0;
    int opt;

    while ((opt = getopt(argc, argv, "s:E:b:t:hv")) != -1)
    {

        switch (opt)
        {
        case 's':
            cache.s = atoi(optarg);
            break;
        case 'E':
            cache.E = atoi(optarg);
            break;
        case 'b':
            cache.b = atoi(optarg);
            break;
        case 't':
            if (!(file = fopen(optarg, "r"))) 
                return 1;
            break;
        case 'h':
            printf("Usage: ./csim [-hv] -s <s> -E <E> -b <b> -t <tracefile>");
            return 0;
        case 'v':
            verbose = 1;
            break;
        default:
            return 1;
        }

    }

    if (!file)

    {
        return 1;
    }

    else if (!cache.s || !cache.b || !cache.E)

    {
        return 1;
    }

    cache.sets = malloc(sizeof(set) * (1 << cache.s));

    for (int i = 0; i < (1 << cache.s); i++)

    {
        cache.sets[i].lines = malloc(sizeof(line) * cache.E);
    }

    char op;
    unsigned long long address;

    int size = 0;

    while (fscanf(file, " %c %llx,%d", &op, &address, &size) != EOF)

    {
        switch (op)
        {
        case 'L':
        case 'S':
            if (verbose)
            {
                printf("%c %llx, %d ", op, address, size);
            }
            update(&cache, verbose, address, &hit, &miss, &eviction);
            if (verbose) printf("\n");
            break;

        case 'M':
            if (verbose)

            {
                printf("%c %llx, %d ", op, address, size);
            }
            update(&cache, verbose, address, &hit, &miss, &eviction);
            update(&cache, verbose, address, &hit, &miss, &eviction);

            if (verbose) printf("\n");
            break;
        }
    }

    printSummary(hit, miss, eviction);

    //free

    fclose(file);
    for (int i = 0; i << (1 << cache.s); i++)

    {
        free(cache.sets[i].lines);
    }

    free(cache.sets);
    
    return 0;

}



int check_hit(set* cache_set, int n, int tag, int* hit)

{

    for (int i = 0; i < n; i++)
    {

        line* cache_line = &cache_set->lines[i];

        if ((cache_line->valid) && (cache_line->tag == tag))

        {
            (*hit)++;
            cache_line->count_LRU = 0;
            return 1;
        }
    }
    return 0;

}



int check_miss(set* cache_set, int n, int tag, int* miss)

{

    (*miss)++;

    for (int i = 0; i < n; i++)

    {
        line* cache_line = &cache_set->lines[i];
        if (!cache_line->valid)

        {
            cache_line->valid = 1;
            cache_line->tag = tag;
            cache_line->count_LRU = 0;
            return 1;
        }
    }
    return 0;
}



int check_eviction(set* cache_set, int E, int tag, int* eviction)

{

    (*eviction)++;

    int max_index = 0;
    int max_LRU = cache_set->lines[0].count_LRU;


    for (int i = 1; i < E; i++)
    {
        line* cache_line = &cache_set->lines[i];
        if (cache_line->count_LRU > max_LRU)
        {
            max_LRU = cache_line->count_LRU;
            max_index = i;
        }
    }

    line* max_line = &cache_set->lines[max_index];
    
    max_line->tag = tag;
    max_line->count_LRU = 0;

    return 0;
}



void update(Cache* cache, int verbose, int address, int* hit, int* miss, int* eviction)

{

    int set_index = (address >> cache->b) & (~(~0 << cache->s));
    int tag = (~0) & (address >> (cache->b + cache->s));

    set* cache_set = &cache->sets[set_index];


    for (int i = 0; i < cache->E; i++)

    {

        line* cache_line = &cache_set->lines[i];
        if (cache_line->valid)
        {
            (cache_line->count_LRU)++;
        }
        else cache_line->count_LRU = 0;

    }

    if (check_hit(cache_set, cache->E, tag, hit))
    {
        if (verbose) printf("hit ");
        return;
    }
    else if (check_miss(cache_set, cache->E, tag, miss))
    {
        if (verbose) printf("miss ");
        return;
    }
    check_eviction(cache_set, cache->E, tag, eviction);
     if (verbose) printf("eviction ");
        return;
}



