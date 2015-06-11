#include "my-malloc.h"
#include <unistd.h>
#include <stdio.h>
#include <strings.h>


static int nb_alloc   = 0;              /* Nombre de fois où on alloué     */
static int nb_dealloc = 0;              /* Nombre de fois où on désalloué  */
static int nb_sbrk    = 0;              /* nombre de fois où a appelé sbrk */

#define MOST_RESTRICTING_TYPE double // Pour s’aligner sur des frontieres multiples
									// de la taille du type le plus contraignant
#define SIZE_SBRK 1024 //Taille d'un sbrk

#define SIZE_HEADER sizeof(Header) //Taille d'un header

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

static Header * freelist = NULL; //liste chainée des blocks libres

//ajoute un bloc a la fin de la liste des blocks libres
Header * addBlockInFreelist(Header * block){
	Header * tmp = freelist;
	//si list vide on met directement le block
	if(tmp == NULL){
		freelist = block;
		return freelist;
	}

	//parcours de la freelist
	while(tmp->info.ptr != NULL)
		tmp = tmp->info.ptr;

	tmp->info.ptr = block;
	//on regarde si le dernier block de la freelist et block 
	//sont voisins pour les merges 
	return mergeBlock(tmp) ? tmp : block;
}

//ajoute de l'espace memoire au programme (sbrek)
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

//retourne l'adresse du block a partir du header
int getDebBlock(Header * block){
	int adresseDeb = block;
	adresseDeb += SIZE_HEADER;
	return adresseDeb;
}

//retourne le header a partir d'un block
Header * getHeaderBlock(void * ptr){
	char * adr = ptr;
	adr -= SIZE_HEADER;
	return (void *)adr;
}

//recherche si un bloc a une taille assez grande pour contenir size_t 
Header * searchBlock(size_t size){
	Header * tmp = freelist;

	//parcours de la freelist
	while(tmp && (tmp->info.size < size))
		tmp = tmp->info.ptr;

	return tmp;
}

//fusionne les block et le suivant si ils sont voisins
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

//Separe un block en deux partie avec la premiere de la size voulue
void splitBlock(Header * block, int size){
	Header * secondBlock = getDebBlock(block) + size;
	
	secondBlock->info.size = block->info.size - SIZE_HEADER - size;
    secondBlock->info.ptr = block->info.ptr;

    block->info.size = size;
    block->info.ptr = secondBlock;
}

//supprime un block de la liste chainée
void deleteBlockFromFreelist(Header * block){
	if(block == freelist){
		freelist = (block->info.ptr) ? block->info.ptr : NULL;
		return;
	}
	if(freelist){
		Header * tmp = freelist;
		while (tmp->info.ptr && tmp->info.ptr != block){//parcours de la freelist
            tmp = tmp->info.ptr;
		}
		//si on a trouvé le block a supprimé
		//on le supprime de la liste
		tmp->info.ptr = (tmp->info.ptr) ? tmp->info.ptr->info.ptr : NULL;
	}
}

void * mymalloc(size_t size){
	nb_alloc += 1;
	if(size == 0)
		return NULL;
	Header * allocatedBlock;
	//si on a plus d'espace réservé pour le programme
	if(freelist == NULL){
		extendProgramSpace();
	}

	//si il y a de l'espace libre réservé 
	if(freelist){
		//on cherche si un block libre est dispo
		allocatedBlock = searchBlock(size);	
		if(allocatedBlock == NULL){	// si y'en a pas on élargit la mémoire
			allocatedBlock = extendProgramSpace();
			if(allocatedBlock == NULL) return NULL;
		} 
		if(allocatedBlock) {//si on a un on le découpe a la taille souhaité
			if(allocatedBlock->info.size >= SIZE_HEADER + size)
				splitBlock(allocatedBlock, size);
		}
	}
	//on l'enleve de la freelist
	deleteBlockFromFreelist(allocatedBlock);
	return getDebBlock(allocatedBlock);//return le début de la zone mémoire libre
}

//ptr -> adresse deb du bloc 
void myfree(void * ptr) {
	nb_dealloc += 1;
	Header * blockToFree = getHeaderBlock(ptr);//on récupère le header de la zone a free
	if(freelist == NULL)// si la freelist est vide le block devient la freelist
		freelist = blockToFree; 
	else 
	{
		if(blockToFree < freelist){// si l'adr memoire du blockfree est avant, le block est mis au début
			blockToFree->info.ptr = freelist;
			freelist = blockToFree;
			mergeBlock(freelist);
		} else {
			Header * tmp = freelist;//  parcours de la freelist jusqu'a trouvé la place d'une blockfree
			while (tmp->info.ptr && tmp->info.ptr < blockToFree)
            	tmp = tmp->info.ptr;
			
			if(tmp->info.ptr != NULL && blockToFree->info.size != 0){
				blockToFree->info.ptr = tmp->info.ptr;// ajout de blockfree dans le list
				tmp->info.ptr = blockToFree;
			}
			
			//on merge si ils ont des voisins free
			mergeBlock(blockToFree);
			mergeBlock(tmp);
		}
	}
}

void *mycalloc(size_t nmemb, size_t size) {
  	if(size == 0 || nmemb == 0)
  		return NULL;
  	nb_alloc += 1;
  	int realSize = nmemb * size;
  	void * adrReserved = mymalloc(realSize);// allocation de la taille souhaitée
  	if(adrReserved != NULL)
  		bzero(adrReserved, realSize);//remplissage de l'espace mémoire avec des 0
  
  	return adrReserved;
}

void *myrealloc(void *ptr, size_t size) {
  	if(ptr == NULL)
  		return mymalloc(size);
  	if(size == 0 && ptr != NULL){
  		myfree(ptr);
  		return NULL;
 	}
 	nb_alloc += 1;
 	Header * currentBlock = getHeaderBlock(ptr);
 	int oldSize = currentBlock->info.size;
 	myfree(ptr);
  	if(size > oldSize){
	  	void * adrNewBlock = mymalloc(size);
	  	memcpy(adrNewBlock, ptr, oldSize);
		return adrNewBlock;
  	} else {
  		//si on a realloc avec une size plus petite, on free puis malloc classique
  		return mymalloc(size);
  	}
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

