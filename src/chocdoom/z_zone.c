//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Zone Memory Allocation. Neat.
//


#include "z_zone.h"
#include "i_system.h"
#include "doomtype.h"


//
// ZONE MEMORY ALLOCATION
//
// There is never any space between memblocks,
//  and there will never be two contiguous free memblocks.
// The rover can be left pointing at a non-empty block.
//
// It is of no value to free a cachable block,
//  because it will get overwritten automatically if needed.
// 
 
#define MEM_ALIGN sizeof(void *)
#define ZONEID	0x1d4a1100

typedef struct memblock_s
{
    int			size;	// including the header and possibly tiny fragments
    void**		user;
    //int			tag;	// PU_FREE if this is free
    //int			id;	// should be ZONEID
    int idtag;
    struct memblock_s*	next;
    struct memblock_s*	prev;
} memblock_t;


typedef struct
{
    // total bytes malloced, including header
    int		size;

    // start / end cap for linked list
    memblock_t	blocklist;
    
    memblock_t*	rover;
    
} memzone_t;



memzone_t*	mainzone;

byte data_mem[64 * 1024] = {1}; // chunk of .data / DTCMRAM
byte d3_mem[62 * 1024] __attribute__((section(".dma_data")));

#ifdef TARGET_32BLIT_HW
#define NUM_MEMZONES 4
#else
#define NUM_MEMZONES 3
#endif
memzone_t* memzones[NUM_MEMZONES];

//
// Z_ClearZone
//
void Z_ClearZone (memzone_t* zone)
{
    memblock_t*		block;
	
    // set the entire zone to one free block
    zone->blocklist.next =
	zone->blocklist.prev =
	block = (memblock_t *)( (byte *)zone + sizeof(memzone_t) );
    
    zone->blocklist.user = (void *)zone;
    zone->blocklist.idtag = PU_STATIC | ZONEID;
    zone->rover = block;
	
    block->prev = block->next = &zone->blocklist;
    
    // a free block.
    block->idtag = PU_FREE;

    block->size = zone->size - sizeof(memzone_t);
}

static void InitZone(byte *base, int size, memzone_t **zoneptr)
{
    memblock_t*	block;

    memzone_t *zone = (memzone_t *)base;
    zone->size = size;

    // set the entire zone to one free block
    zone->blocklist.next =
	zone->blocklist.prev =
	block = (memblock_t *)( (byte *)zone + sizeof(memzone_t) );

    zone->blocklist.user = (void *)zone;
    zone->blocklist.idtag = PU_STATIC | ZONEID;
    zone->rover = block;

    block->prev = block->next = &zone->blocklist;

    // free block
    block->idtag = PU_FREE;
    
    block->size = zone->size - sizeof(memzone_t);

    *zoneptr = zone;
}

//
// Z_Init
//
void Z_Init (void)
{
    memblock_t*	block;
    int		size;

    byte *base = I_ZoneBase (&size);
    InitZone(base, size, &mainzone);

    memzones[1] = mainzone;

    // more zones
    InitZone(d3_mem, sizeof(d3_mem), &memzones[0]);
#ifdef TARGET_32BLIT_HW
    InitZone((byte *)0x30000000, 63 * 1024, &memzones[2]); // steal all of RAM_D2
    InitZone(data_mem, sizeof(data_mem), &memzones[3]);
#else
    InitZone(data_mem, sizeof(data_mem), &memzones[2]);
#endif
}


//
// Z_Free
//
void Z_Free (void* ptr)
{
    memblock_t*		block;
    memblock_t*		other;
	
    block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));

    if ((block->idtag & 0xFFFFFF00) != ZONEID)
	I_Error ("Z_Free: freed a pointer without ZONEID");
		
    if ((block->idtag & 0xFF) != PU_FREE && block->user != NULL)
    {
    	// clear the user's mark
	    *block->user = 0;
    }

    // mark as free
    block->idtag = PU_FREE;
    block->user = NULL;
	
    other = block->prev;

    if (other->idtag == PU_FREE)
    {
        // merge with previous free block
        other->size += block->size;
        other->next = block->next;
        other->next->prev = other;

        for(int i = 0; i < NUM_MEMZONES; i++)
            if (block == memzones[i]->rover)
                memzones[i]->rover = other;

        block = other;
    }
	
    other = block->next;
    if (other->idtag == PU_FREE)
    {
        // merge the next free block onto the end
        block->size += other->size;
        block->next = other->next;
        block->next->prev = block;

        for(int i = 0; i < NUM_MEMZONES; i++)
            if (other == memzones[i]->rover)
                memzones[i]->rover = block;
    }
}



//
// Z_Malloc
// You can pass a NULL user if the tag is < PU_PURGELEVEL.
//
#define MINFRAGMENT		64


void*
Z_Malloc
( int		size,
  int		tag,
  void*		user )
{
    int		extra;
    memblock_t*	start;
    memblock_t* rover;
    memblock_t* newblock;
    memblock_t*	base;
    void *result;
    int i;

    size = (size + MEM_ALIGN - 1) & ~(MEM_ALIGN - 1);

    // scan through the block list,
    // looking for the first free block
    // of sufficient size,
    // throwing out any purgable blocks along the way.

    // account for size of block header
    size += sizeof(memblock_t);
    

    for(i = 0; i < NUM_MEMZONES; i++)
    {
        // if there is a free block behind the rover,
        //  back up over them
        base = memzones[i]->rover;
        
        if (base->prev->idtag == PU_FREE)
            base = base->prev;
        
        rover = base;
        start = base->prev;
        
        do
        {
            // scanned all the way around the list
            // ... but continue if it's purgable
            if (rover == start && (start->idtag & 0xFF) < PU_PURGELEVEL)
                break;
        
            if (rover->idtag != PU_FREE)
            {
                if ((rover->idtag & 0xFF) < PU_PURGELEVEL)
                {
                    // hit a block that can't be purged,
                    // so move base past it
                    base = rover = rover->next;
                }
                else
                {
                    // free the rover block (adding the size to base)

                    // the rover can be the base block
                    base = base->prev;
                    Z_Free ((byte *)rover+sizeof(memblock_t));
                    base = base->next;
                    rover = base->next;
                }
            }
            else
            {
                rover = rover->next;
            }

        } while (base->idtag != PU_FREE || base->size < size);

        if(base->idtag == PU_FREE && base->size >= size)
            break;

        base = NULL;
    }

    if(!base)
        I_Error ("Z_Malloc: failed on allocation of %i bytes", size);
    
    // found a block big enough
    extra = base->size - size;
    
    if (extra >  MINFRAGMENT)
    {
        // there will be a free fragment after the allocated block
        newblock = (memblock_t *) ((byte *)base + size );
        newblock->size = extra;
	
        newblock->idtag = PU_FREE;
        newblock->user = NULL;	
        newblock->prev = base;
        newblock->next = base->next;
        newblock->next->prev = newblock;

        base->next = newblock;
        base->size = size;
    }
	
	if (user == NULL && tag >= PU_PURGELEVEL)
	    I_Error ("Z_Malloc: an owner is required for purgable blocks");

    base->user = user;
    base->idtag = tag | ZONEID;

    result  = (void *) ((byte *)base + sizeof(memblock_t));

    if (base->user)
    {
        *base->user = result;
    }

    // next allocation will start looking here
    memzones[i]->rover = base->next;	
    
    return result;
}



//
// Z_FreeTags
//
void
Z_FreeTags
( int		lowtag,
  int		hightag )
{
    memblock_t*	block;
    memblock_t*	next;
	for(int i = 0; i < NUM_MEMZONES; i++)
    {
        for (block = memzones[i]->blocklist.next ;
        block != &memzones[i]->blocklist ;
        block = next)
        {
        // get link before freeing
        next = block->next;

        // free block?
        if (block->idtag == PU_FREE)
            continue;
        
        if ((block->idtag & 0xFF) >= lowtag && (block->idtag & 0xFF) <= hightag)
            Z_Free ( (byte *)block+sizeof(memblock_t));
        }
    }
}

// TODO: everything below only looks at mainzone

//
// Z_DumpHeap
// Note: TFileDumpHeap( stdout ) ?
//
void
Z_DumpHeap
( int		lowtag,
  int		hightag )
{
    memblock_t*	block;
	
    printf ("zone size: %i  location: %p\n",
	    mainzone->size,mainzone);
    
    printf ("tag range: %i to %i\n",
	    lowtag, hightag);
	
    for (block = mainzone->blocklist.next ; ; block = block->next)
    {
	if ((block->idtag & 0xFF) >= lowtag && (block->idtag & 0xFF) <= hightag)
	    printf ("block:%p    size:%7i    user:%p    tag:%3i\n",
		    block, block->size, block->user, block->idtag & 0xFF);
		
	if (block->next == &mainzone->blocklist)
	{
	    // all blocks have been hit
	    break;
	}
	
	if ( (byte *)block + block->size != (byte *)block->next)
	    printf ("ERROR: block size does not touch the next block\n");

	if ( block->next->prev != block)
	    printf ("ERROR: next block doesn't have proper back link\n");

	if (block->idtag == PU_FREE && block->next->idtag == PU_FREE)
	    printf ("ERROR: two consecutive free blocks\n");
    }
}


//
// Z_FileDumpHeap
//
void Z_FileDumpHeap (FILE* f)
{
    memblock_t*	block;
 
    fprintf (f,"zone size: %i  location: %p\n",mainzone->size,mainzone);
 
    for (block = mainzone->blocklist.next ; ; block = block->next)
    {
	fprintf (f,"block:%p    size:%7i    user:%p    tag:%3i\n",
	     block, block->size, block->user, block->idtag & 0xFF);
		
	if (block->next == &mainzone->blocklist)
	{
	    // all blocks have been hit
	    break;
	}

	if ( (byte *)block + block->size != (byte *)block->next)
	    fprintf (f,"ERROR: block size does not touch the next block\n");

	if ( block->next->prev != block)
	    fprintf (f,"ERROR: next block doesn't have proper back link\n");

	if (block->idtag == PU_FREE && block->next->idtag == PU_FREE)
	    fprintf (f,"ERROR: two consecutive free blocks\n");
    }
}



//
// Z_CheckHeap
//
void Z_CheckHeap (void)
{
    memblock_t*	block;
	
    for (block = mainzone->blocklist.next ; ; block = block->next)
    {
	if (block->next == &mainzone->blocklist)
	{
	    // all blocks have been hit
	    break;
	}
	
	if ( (byte *)block + block->size != (byte *)block->next)
	    I_Error ("Z_CheckHeap: block size does not touch the next block\n");

	if ( block->next->prev != block)
	    I_Error ("Z_CheckHeap: next block doesn't have proper back link\n");

	if (block->idtag == PU_FREE && block->next->idtag == PU_FREE)
	    I_Error ("Z_CheckHeap: two consecutive free blocks\n");
    }
}




//
// Z_ChangeTag
//
void Z_ChangeTag2(void *ptr, int tag, char *file, int line)
{
    memblock_t*	block;
	
    block = (memblock_t *) ((byte *)ptr - sizeof(memblock_t));

    if ((block->idtag & 0xFFFFFF00) != ZONEID)
        I_Error("%s:%i: Z_ChangeTag: block without a ZONEID!",
                file, line);

    if (tag >= PU_PURGELEVEL && block->user == NULL)
        I_Error("%s:%i: Z_ChangeTag: an owner is required "
                "for purgable blocks", file, line);

    block->idtag = tag | ZONEID;
}

void Z_ChangeUser(void *ptr, void **user)
{
    memblock_t*	block;

    block = (memblock_t *) ((byte *)ptr - sizeof(memblock_t));

    if ((block->idtag & 0xFFFFFF00) != ZONEID)
    {
        I_Error("Z_ChangeUser: Tried to change user for invalid block!");
    }

    block->user = user;
    *user = ptr;
}



//
// Z_FreeMemory
//
int Z_FreeMemory (void)
{
    memblock_t*		block;
    int			free;
	
    free = 0;
    
    for (block = mainzone->blocklist.next ;
         block != &mainzone->blocklist;
         block = block->next)
    {
        if (block->idtag == PU_FREE || (block->idtag & 0xFF) >= PU_PURGELEVEL)
            free += block->size;
    }

    return free;
}

unsigned int Z_ZoneSize(void)
{
    return mainzone->size;
}

