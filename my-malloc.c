#include "my-malloc.h"
#include <unistd.h>
#include <stdio.h>


static int nb_alloc   = 0;              /* Nombre de fois où on alloué     */
static int nb_dealloc = 0;              /* Nombre de fois où on désalloué  */
static int nb_sbrk    = 0;              /* nombre de fois où a appelé sbrk */

#define MOST_RESTRICTING_TYPE double // Pour s’aligner sur des frontieres multiples
									// de la taille du type le plus contraignant
#define SIZE_SBRK 1024 

#define SIZE_HEADER sizeof(Header) 

typedef union header {
	// Header de bloc
	struct {
		unsigned int size;
		// Taille du bloc
		union header *ptr;
		// bloc libre suivant
	} info;
	MOST_RESTRICTING_TYPE dummy;
	// Ne sert qu’a provoquer un alignement
} Header;

static Header * freelist = NULL;

Header * addBlockInFreelist(Header * block){
	Header * tmp = freelist;

	if(tmp == NULL){
		freelist = block;
		return freelist;
	}

	while(tmp->info.ptr != NULL){//parcours de la freelist
		tmp = tmp->info.ptr;
	}
	tmp->info.ptr = block;
	return (mergeBlock(tmp) == 1) ? tmp : block;
}

Header * extendProgramSpace(){
	nb_sbrk += 1;
	Header * extendBlock = sbrk(0);
	if (sbrk(SIZE_SBRK) == ((void *) -1)){
			return NULL;
	}
	extendBlock->info.size = SIZE_SBRK - SIZE_HEADER;
	extendBlock->info.ptr = NULL;
	
	return addBlockInFreelist(extendBlock);
}

int getDebBlock(Header * block){
	int adresseDeb = block;
	adresseDeb += SIZE_HEADER;
	return adresseDeb;
}

Header * getHeaderBlock(void * ptr){
	char * adr = ptr;
	adr -= SIZE_HEADER;
	return (void *)adr;
}

Header * searchBlock(size_t size){
	Header * tmp = freelist;

	while(tmp && (tmp->info.size < size)){//parcours de la freelist
		tmp = tmp->info.ptr;
	}

	return tmp;
}

int mergeBlock(Header * block){
	Header * nextBlock = block->info.ptr;
	int adrNext = nextBlock;
	int adrEndCurrent = getDebBlock(block) + block->info.size;
	if(adrEndCurrent == adrNext){
		block->info.ptr = nextBlock->info.ptr;
		block->info.size += (SIZE_HEADER + nextBlock->info.size);  
		return 1;
	}
	return 0;
}

void splitBlock(Header * block, int size){
	Header * secondBlock = getDebBlock(block) + size;
	
	secondBlock->info.size = block->info.size - SIZE_HEADER - size;
    secondBlock->info.ptr = block->info.ptr;

    block->info.size = size;
    block->info.ptr = secondBlock;
}

void deleteBlockFromFreelist(Header * block){
	if(block == freelist){
		freelist = (block->info.ptr) ? block->info.ptr : NULL;
		return;
	}
	if(freelist){
		Header * tmp = freelist;
		while (tmp->info.ptr && tmp->info.ptr != block){
            tmp = tmp->info.ptr;
		}
		tmp->info.ptr = (tmp->info.ptr) ? tmp->info.ptr->info.ptr : NULL;
	}
}

void * mymalloc(size_t size){
	nb_alloc += 1;
	Header * allocatedBlock;
	//si on a plus d'espace réservé pour le programme
	if(freelist == NULL){
		extendProgramSpace();
	}

	//si il y a de l'espace libre réservé 
	if(freelist){
		allocatedBlock = searchBlock(size);	
		if(allocatedBlock == NULL){	
			allocatedBlock = extendProgramSpace();
			if(allocatedBlock == NULL) return NULL;
		} 
		if(allocatedBlock) {
			if(allocatedBlock->info.size >= SIZE_HEADER + size){
				splitBlock(allocatedBlock, size);
			}
		}
	}

	deleteBlockFromFreelist(allocatedBlock);
	printf("block alloc %d : size=%d / next=%d\n", allocatedBlock, allocatedBlock->info.size, allocatedBlock->info.ptr);
	return getDebBlock(allocatedBlock);
}

//ptr -> adresse deb du bloc 
void myfree(void *ptr) {
	nb_dealloc += 1;
	Header * blockToFree = getHeaderBlock(ptr);
	if(freelist == NULL){
		freelist = blockToFree;
	} else {
		if(blockToFree < freelist){
			blockToFree->info.ptr = freelist;
			freelist = blockToFree;
			mergeBlock(freelist);
		} else {
			Header * tmp = freelist;
			while (tmp->info.ptr && tmp->info.ptr < blockToFree){
            	tmp = tmp->info.ptr;
			}
			blockToFree->info.ptr = tmp->info.ptr;
			tmp->info.ptr = blockToFree;

			mergeBlock(blockToFree);
			mergeBlock(tmp);
		}
	}
	printf("block free %d : size=%d / next=%d\n", blockToFree, blockToFree->info.size, blockToFree->info.ptr);
}

void *mycalloc(size_t nmemb, size_t size) {
  
}

void *myrealloc(void *ptr, size_t size) {
  
}

void mymalloc_infos(char *msg) {
  if (msg) fprintf(stderr, "**********\n*** %s\n", msg);

  fprintf(stderr, "# allocs = %3d - # deallocs = %3d - # sbrk = %3d\n",
	  nb_alloc, nb_dealloc, nb_sbrk);
  /* Ca pourrait être pas mal d'afficher ici les blocs dans la liste libre */

  if(freelist){
  	Header * tmp = freelist;
  	while(tmp){
  		printf("Block %d (Taille : %d / ptr : %d)\n", tmp, tmp->info.size, tmp->info.ptr);
  		tmp = tmp->info.ptr;
  	}
  }

  if (msg) fprintf(stderr, "**********\n\n");
}