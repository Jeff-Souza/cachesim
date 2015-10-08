/* Jeffrey Souza
 * u0402450
 * 
 * loginID: souza
 */
#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>

// keep tags consistant size
typedef unsigned long tag;

// struct to hold all our cache info
typedef struct
{
    tag** tags;

    int lines;
    int sets;

    int block;
    int blockOffset;

    int indexBits;

}Cache;

// forward declare functions
void makeCache(Cache* c, int s, int E, int b);
void clearCache(Cache* c);
void simulate(Cache* c, FILE* trace, int* hits, int* misses, int* evicitons);

int verbose;

int main(int argc, char **argv)
{
    verbose = 0;

    int s = 0; 		// Number of set index bits (S = 2^s is the number of sets)
    int E = 0;  	// Associativity (number of lines per set)
    int b = 0;   	// Number of block bits (B = 2^b is the block size)
    char *t = NULL;	// Name of the valgrind trace to replay

    // Parse command line args

    int a;
    while((a = getopt (argc, argv, "hvs:E:b:t:")) != -1)
    {
	switch(a)
	{
	    case 'h':
		printf("Heeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeellllpppp\n");
		return 0;
	    case 'v':
		verbose = 1;
		break;
	    case 's':
		s = atoi(optarg);
		break;
	    case 'E':
		E = atoi(optarg);
		break;
	    case 'b':
		b = atoi(optarg);
		break;
	    case 't':
		t = optarg;
		break;
	    default:
		printf("Invalid argument(s)\n");
		return 1;
	}
    }
    
    // if any command line args werent given abort
    if (s <= 0 || E <= 0 || b <= 0 || t == NULL)
    {
        printf("Invalid command line arguments\n");
        return 1;
    }
    
    // initialize our cache
    Cache c;
    makeCache(&c, s, E, b);

    int hit_count = 0, miss_count = 0, eviction_count = 0;

    // open trace file
    FILE *trace;
    trace = fopen(t, "r");

    // if trace file is valid simulate cache
    if(trace != NULL)
    {
	simulate(&c, trace, &hit_count, &miss_count, &eviction_count);
    }

    printSummary(hit_count, miss_count, eviction_count);

    // cleanup
    fclose(trace);
    clearCache(&c);

    return 0;
}


void makeCache(Cache* c, int s, int E, int b)
{
    c->lines = E;	     // lines per set
    c->sets = pow(2, s);     // s set bits gives us 2^s sets
    c->indexBits = s;	     // stroe set bits

    c->block = pow(2, b);    // b block bits gives us 2^b blocks
    c->blockOffset = b;      // store block bits

    
    /* tags is of type tag** which resembles a 2d array
     / the first layer of pointers is made of tags that represent sets
     / the second layer of pointers represents the lines in each set
    */

    // 1 tag per set
    // c->sets is sets per cache
    c->tags = malloc(sizeof(tag*) * c->sets);

    //  
    for(int i = 0; i < c->sets; i++)
    {
	// 1 tag per line
	// c->lines is lines per set
    	c->tags[i] = malloc(c->lines * sizeof(tag));	// each set has lines * tag size
    	memset(c->tags[i], -1, c->lines * sizeof(tag)); // clear new memory 
    }
}

void clearCache(Cache* c)
{
    // need to free anything we malloc
    for(int i = 0; i < c->sets; i++)
    {
	free(c->tags[i]);       // free the memory
    }
    free(c->tags);		// free memory
}

void simulate(Cache* c, FILE* trace, int* hits, int* misses, int* evictions)
{
    // start reading trace file
    int bufferSize = 128;
    char buff[bufferSize];

    while(fgets(buff, bufferSize, trace))
    {
	// ignore all instruction cache accesses
	if(buff[0] == 'I')
	{
	    continue;
	}
	
	// read in data from line
	char op;
	unsigned long addr = 0;
	int size = 0; 

	// %lx will interpret addr in hex :)
	sscanf(buff, " %c %lx,%d", &op, &addr, &size);

	// data modify guarantees hit
	if(op == 'M')
	{
	    (*hits)++;
	    if(verbose) printf("hit\n");
	}

        // bit math to grab bits we want-----------------------------------------------------

	int offset = c->blockOffset + c->indexBits;

        /* the bits that represent the set are found by 
	 / removing whats infront of the block and index bits 
	 / and then removing the index bits 
	*/

	unsigned long setBits = 0;

	setBits = (addr << (64 - offset)) >> (64 - c->indexBits);

	/*
	 / we are left with only left with the bits representing the set
	 /
	 / the bits that represent the tag are found by
	 / removing all the block and index bits
        */

	unsigned long tagBits = 0;

	tagBits = addr >> offset; 

	// confusing bit math done----------------------------------------------------------
	

	if(verbose == 1) printf("%c %lx %d\n", op, addr, size);
	
	int i;		 // holds which tag was the hit if we find it

	int found = 0;   // bool

	tag* set = c->tags[setBits]; // grab the set to search through

	// loop over all lines in the set
	for(i = 0; i < c->lines; i++)
	{
	    tag line = set[i];

	    if(line == tagBits)
	    {
		found = 1;
		break;
	    }
	    // line will be -1 if not set and thus never be a hit
	}

	if(found == 1) // HIT!
	{
	    tag old = set[0];

	    tag hit = set[i];

	    // move all lines before hit down 1 spot in the set
	    for (int a = 1; a <= i; a++)
	    {
	        tag temp = old; 
       		old = set[a];
          	set[a] = temp;
	    }
	    set[0] = hit; // set hit tag as first line

	    if(verbose) printf("hit\n");

	    ++*hits;
	}

	else // MISS!
	{
	    /* check if the last line in the set is empty
	     / if it is not empty we have an eviction
	    */
	    if (set[c->lines - 1] != -1)
	    {
	        ++*evictions;
		printf("eviction\n");
	    }

	    // shift all lines down 1 spot in the set
	    tag old = set[0];
	    for(int a = 1; a < c->lines; a++)
	    {
	        tag temp = old;
	        old = set[a];
	        set[a] = temp;
    	    }

	    // set new tag as the first line
	    set[0] = tagBits;

	    ++*misses;

	    if(verbose) printf("miss\n");
        }

	if(feof(trace)) break; // end when file is done
    }
    return;
}
