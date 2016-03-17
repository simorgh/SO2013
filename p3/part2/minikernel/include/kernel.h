/*
 *  minikernel/include/kernel.h
 *
 *  Minikernel. Versión 1.0
 *
 *  Fernando Pérez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene definiciones usadas por kernel.c
 *
 *      SE DEBE MODIFICAR PARA INCLUIR NUEVA FUNCIONALIDAD
 *
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include "const.h"
#include "HAL.h"
#include "llamsis.h"

/*
 * La prioritat pren valors entre un mínim i un màxim
 */
#define MIN_PRIO 10
#define MAX_PRIO 50

typedef enum{ FALSE = 0, TRUE = 1} boolean ; /* definició de tipus Boolea per treballar comodament amb aquest tipus.*/


/*
 *
 * Definicion del tipo que corresponde con el BCP.
 * Se va a modificar al incluir la funcionalidad pedida.
 *
 */
typedef struct BCP_t *BCPptr;

typedef struct BCP_t {
	int id;				/* ident. del proceso */
	int p_id;			/* ident. del proceso padre */
	int estado;			/* TERMINADO|LISTO|EJECUCION|BLOQUEADO*/
	int fills;			/* número de fills que ha creat en un moment determinat*/
	contexto_t contexto_regs;	/* copia de regs. de UCP */
	void * pila;			/* dir. inicial de la pila */
	BCPptr siguiente;		/* puntero a otro BCP */
	void *info_mem;			/* descriptor del mapa de memoria */
	int ticks;			/* ticks que falten per despertar el proces adormit */
	unsigned int prioridad;		/* cada procés té una prioritat associada */
	unsigned int prioridad_efectiva;
} BCP;

/*
 *
 * Definicion del tipo que corresponde con la cabecera de una lista
 * de BCPs. Este tipo se puede usar para diversas listas (procesos listos,
 * procesos bloqueados en semáforo, etc.).
 *
 */

typedef struct{
	BCP *primero;
	BCP *ultimo;
} lista_BCPs;


/*
 * Variable global que identifica el proceso actual
 */

BCP * p_proc_actual=NULL;

/*
 * Variable global que representa la tabla de procesos
 */
BCP tabla_procs[MAX_PROC];

/*
 * Variable global que representa la cola de procesos listos
 */
lista_BCPs lista_listos= {NULL, NULL};


/*
 * Variable global que representa la cola de procesos dormidos
 */
lista_BCPs lista_dormidos= {NULL, NULL};


/*
 * Variable global que representa la lista donde estarán los procesos bloqueados que esperan la finalización de la ejecución de todos sus hijos.
 */
lista_BCPs lista_espera = {NULL, NULL};


/*
 *
 * Definición del tipo que corresponde con una entrada en la tabla de
 * llamadas al sistema.
 *
 */
typedef struct{
	int (*fservicio)();
} servicio;


/*
 * Prototipos de las rutinas que realizan cada llamada al sistema
 */
int sis_crear_proceso();
int sis_terminar_proceso();
int sis_escribir();
int sis_get_pid();
int sis_dormir();
int sis_fijar_prio();
int sis_get_ppid();
int sis_espera();

/*
 * Variable global que contiene las rutinas que realizan cada llamada
 */
servicio tabla_servicios[NSERVICIOS]={	{sis_crear_proceso},
					{sis_terminar_proceso},
					{sis_escribir},
					{sis_get_pid},
					{sis_dormir},
					{sis_fijar_prio},
					{sis_get_ppid},
					{sis_espera}
				      };
					

#endif /* _KERNEL_H */

