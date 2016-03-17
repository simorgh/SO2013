/*
 * usuario/dormilon.c
 *
 *  Minikernel. Versión 1.0
 *
 *  Vicent Roig Ripoll
 *
 */

#include "servicios.h"

int main()
{
	printf("-> DORMILON: Abans de dormir\n");
	dormir(1);
	printf("-> DORMILON: Despres de dormir\n");
	return 0;
} 
