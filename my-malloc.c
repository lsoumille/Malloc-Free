#include "my-malloc.h"


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

static int octAvailForMalloc = 0;

static void * locProgBreak = NULL;

void extendProgramSpace(){
	if (sbrk(SIZE_SBRK) == ((void *) -1)){
			fprintf(stderr, "Plus de memoire\n");
	}
	nb_sbrk += 1;
	octAvailForMalloc = SIZE_SBRK;
} 

void *mymalloc(size_t size){
	nb_alloc += 1;
	//si on a plus d'espace réservé pour le programme
	if(locProgBreak == NULL){
		locProgBreak = sbrk(0);
		extendProgramSpace();
	}

	//si il y a de l'espace libre réservé 
	if(freelist != NULL){
		void * adrHeaderFree = searchInFreelist(size);
		
		if(adrHeaderFree){
			return adrHeaderFree + SIZE_HEADER;
		}
	}
}