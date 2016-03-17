/*
 * usuario/simplon.c
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 * Programa de usuario que simplemente imprime un valor entero
 */

#include "servicios.h"
#define TOT_ITER 200 /* ponga las que considere oportuno */

int main()
{
	int i;
	printf("simplon: comienza\n");
	for (i=0; i<TOT_ITER; i++)
		printf("simplon: i %d\n", i);
	fijar_prio(30); /* Augmentam la prioritat */
	
	/* Prova de prioritats, deixarem que el simplon2 heredi del pare*/
	if (crear_proceso("dormilon")<0){
                printf("Error creando dormilon\n");
	}
	fijar_prio(10); //Aqui deber�a activarse la interrupci�n de Software.
	
	printf("simplon: termina\n");
	return 0;
}
