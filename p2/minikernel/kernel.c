/*
 *  kernel/kernel.c
 *
 *  Minikernel. Versión 1.0
 *
 *  Fernando Pérez Costoya
 *  ______________________________________
 * 
 *  Alumne: Vicent Roig Ripoll
 */

/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */
#include "kernel.h"	/* Contiene defs. usadas por este modulo */


/* ======================================================
 *			PROTOTIPS
 * ======================================================
 */ 
static void ajustar_dormidos();
static void bloquear(lista_BCPs *lista );
static void desbloquear (BCP * proc, lista_BCPs * lista);
static BCP * maxima_prioridad (lista_BCPs *lista);
static void muestra_lista(lista_BCPs *);
static void reajustar_prioridades();


static boolean replanificacio_pendent; /* variable global, per indicar si hi ha un canvi de context involuntari pendent.*/

 
/*
 *
 * Funciones relacionadas con la tabla de procesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */

/*
 * Función que inicia la tabla de procesos
 */
static void iniciar_tabla_proc(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		tabla_procs[i].estado=NO_USADA;
}

/*
 * Función que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		if (tabla_procs[i].estado==NO_USADA)
			return i;
	return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP * proc){
	if (lista->primero==NULL)
		lista->primero= proc;
	else
		lista->ultimo->siguiente=proc;
	lista->ultimo= proc;
	proc->siguiente=NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista){
	if (lista->ultimo==lista->primero)
		lista->ultimo=NULL;
	lista->primero=lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP * proc){
	BCP *paux=lista->primero;

	if (paux==proc)
		eliminar_primero(lista);
	else {
		for ( ; ((paux) && (paux->siguiente!=proc));
			paux=paux->siguiente);
		if (paux) {
			if (lista->ultimo==paux->siguiente)
				lista->ultimo=paux;
			paux->siguiente=paux->siguiente->siguiente;
		}
	}
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int(){
	int nivel;

	printk("-> NO HAY LISTOS. ESPERA INT\n");

	/* Baja al mínimo el nivel de interrupción mientras espera */
	nivel=fijar_nivel_int(NIVEL_1);
	halt();
	fijar_nivel_int(nivel);
}

/*
 * Procediment que retorna el BCP del procés de màxima prioridad de la cua lista_listos.
 * Aquesta rutina es cridarà des del planificador. 
 */ 
static BCP * maxima_prioridad (lista_BCPs *lista){
	BCP* p;
	BCP *max;
	p = lista->primero;
	max = p;
	
	printk("\n-> Maxima Prioridad:");
	while(p != NULL){
	    if(p->prioridad_efectiva > max->prioridad_efectiva)max = p;
	    p = p->siguiente;
	}
    
	printk("\n=============\n ID:\t%d\n PR:\t%d\n PE:\t%d\n Estat:\t%d\n=============\n", max->id,max->prioridad,max->prioridad_efectiva, max->estado); 
	return max;
}

/*
 * Función de planificacion que implementa un algoritmo FIFO.
 */
static BCP * planificador(){
	BCP* p;
	
	while (lista_listos.primero==NULL) espera_int();	/* No hay nada que hacer */
	  
	printk("\n-> [ PLANIFICADOR ]\n");
	muestra_lista(&lista_listos);
	p = maxima_prioridad(&lista_listos); /* si el procés amb PE més alta es 0, a lista listos tenen tots PE=0, cal REAJUSTAR PRIORITATS!!
	
	/* sempre que sigui necessari reajustar prioritats...*/
	while(p->prioridad_efectiva==0){
		printk("*** TODOS LOS PROCESOS LISTOS TIENEN PRIORIDAD EFECTIVA 0 ***\n");
		reajustar_prioridades();
		p = maxima_prioridad(&lista_listos);
	}
	return p;
}



/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso(){
	BCP * p_proc_anterior;

	liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

	p_proc_actual->estado=TERMINADO;
	eliminar_elem(&lista_listos,p_proc_actual);  /* proc. fuera de listos */
	
	/* Realizar cambio de contexto */
	p_proc_anterior=p_proc_actual;
	p_proc_actual=planificador();

	printk("-> C.CONTEXTO POR FIN: de %d a %d\n",
			p_proc_anterior->id, p_proc_actual->id);

	liberar_pila(p_proc_anterior->pila);
	p_proc_actual->estado = EJECUCION; /*Recordamos asignar el estado al nuevo proceso seleccionado por el planificador*/
	replanificacio_pendent = FALSE;
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
        return; /* no debería llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit(){

	if (!viene_de_modo_usuario())
		panico("excepcion aritmetica cuando estaba dentro del kernel");

	printk("-> EXCEPCION ARITMETICA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no debería llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem(){
	if (!viene_de_modo_usuario())
		panico("excepcion de memoria cuando estaba dentro del kernel");


	printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no debería llegar aqui */
}


static void muestra_lista(lista_BCPs* lista){
	//BCP *actual = lista_dormidos.primero;
	BCP *actual = lista->primero;
	printk("**********************************************************************************\n");
	printk("\t\t\t\tMuestra Lista\n");
	printk("**********************************************************************************\n");
	/* recorregut de la llista... */
	while(actual){
		printk("______________\n ID:\t%d\n PR:\t%d\n PE:\t%d\n Estat:\t%d\n______________\n", actual->id,actual->prioridad,actual->prioridad_efectiva, actual->estado);
		actual = actual->siguiente;	/* passem al següent desat amb anterioritat */
	}
	printk("**********************************************************************************\n");
}


/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal(){
	printk("-> TRATANDO INT. DE TERMINAL %c\n", leer_puerto(DIR_TERMINAL));
	return;
}

/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj(){
	printk("-> TRATANDO INT. DE RELOJ\n");
	
	/* A cada interrupció de rellotge es decrementa en una unitat la prioritat efectiva del procés que usa la CPU en el moment */
	if(p_proc_actual->estado == EJECUCION) p_proc_actual->prioridad_efectiva--;
	
	/* Quan la prioritat efectiva arriba a 0, s'ha d'activar un canvi de context involuntari, ja que el procés ha d'abandonar el processador i s'ha de seleccionar el procés de prioritat efectiva més gran entre tots els processos preparats per executar-se.*/
	if(p_proc_actual->prioridad_efectiva == 0){
		/*Activem interrupció software*/
		replanificacio_pendent = TRUE; 
		activar_int_SW();
	}
	ajustar_dormidos();
}


/* =======================================================================
 * Funció de soport per el tractament d'interrupcions de rellotje.
 * Decrementa el temps d'espera dels processos adormits i quan algun arribin a zero, els desbloqueja.
 * =======================================================================*/
static void ajustar_dormidos(){
    BCP *p_aux = lista_dormidos.primero;
    BCP *p_sig; /* punter auxiliar per tractar tots els BCP a la mateixa crida i evitar l'error suggerit a classe, desarem el punter al seguent per evitar aquest error! */
    
    /* per tot proces adormit... */
    while(p_aux!=NULL){
		p_sig = p_aux->siguiente;	/* Abans d'un possible desbloqueig desam el punter al seguent element */
		p_aux->ticks--;
		if(p_aux->ticks==0)
			desbloquear(p_aux,&lista_dormidos);
		p_aux = p_sig;			/* passem al següent desat amb anterioritat */
    }  
}


/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis(){
	int nserv, res;

	nserv=leer_registro(0);
	if (nserv<NSERVICIOS)
		res=(tabla_servicios[nserv].fservicio)();
	else
		res=-1;		/* servicio no existente */
	escribir_registro(0,res);
	return;
}

/* =======================================================================
 * Rutina pel bloqueig d'un procés. Ha de posar el procés en estat bloquejat, reajustar
 * les llistes del BCP corresponents i realitzar el canvi de context.
 * ======================================================================= */
static void bloquear(lista_BCPs *lista ){
	BCP *p_proc_anterior;
	
	p_proc_actual->estado = BLOQUEADO;
	p_proc_anterior = p_proc_actual;
	
	/* [NOTA]: A tots els llocs on manipulem lista_listos tenim que deshabilitar les interrupcions de rellotje
	 * per tal d'assegurar l'integritat dels punters de la llista (no hagin set modificats per un altre interrupcio). */
	
	int n=fijar_nivel_int(NIVEL_3); /* deshabilita interrupcions i desa el nivell anterior a n */
	 /* [Important]: tal i com funciona el kernel cal ELIMINAR de lista_listos 
	  * abans de INSERIR a dormidos (no pot haver el mateix element a dos llocs alhora)*/
	eliminar_elem(&lista_listos, p_proc_actual);
	insertar_ultimo(lista,p_proc_actual); /* A la funció li passem actualment lista_dormidos, tq *lista ---> lista_dormidos */
	fijar_nivel_int(n); /* recuperem el nivell anterior, habilitem interrupcions */
	
	p_proc_actual=planificador(); /*obtenim el nou proces a executar mitjançant la seleccio del planificador*/
	p_proc_actual->estado = EJECUCION; /*Recordamos asignar el estado al nuevo proceso seleccionado por el planificador*/
	replanificacio_pendent = FALSE;
	cambio_contexto(&(p_proc_anterior->contexto_regs), &(p_proc_actual->contexto_regs));
}



/* =======================================================================
 * Rutina pel desbloqueig d'un procés.
 * Donada una llista BCB (lista), desbloqueja aquest proces.
 * ======================================================================= */
static void desbloquear (BCP * proc, lista_BCPs * lista){
	/* Reb lista dormidos, s'el·limina de lista (en aquest cas lista_dormidos) i la passam a lista_listos*/ 
	proc->estado=LISTO;
	
	/*En despertar un procés, si la seva prioritat és mes gran que l'actual --> CCI */
	if(proc->prioridad_efectiva > p_proc_actual->prioridad_efectiva){
		/*Activem interrupció software*/
		replanificacio_pendent = TRUE; 
		activar_int_SW();
	}
	
	/* inserció al final dels "listos" */
	int n = fijar_nivel_int(NIVEL_3);
	eliminar_elem(lista, proc);
	insertar_ultimo(&lista_listos, proc);
	fijar_nivel_int(n);
}



/*
 * Tratamiento de interrupciuones software
 * Aqui realitzarem el CCI (Canvi de context involuntari)
 */
static void int_sw(){
	printk("\n* * * * * * * * * * * *\n* TRATANDO INT. SW\n* * * * * * * * * * * *\n");
	if(replanificacio_pendent){  
		BCP *p_proc_anterior;
		p_proc_actual->estado = BLOQUEADO;
		p_proc_anterior = p_proc_actual;
		p_proc_actual = planificador(); /*obtenim el nou proces a executar mitjançant la seleccio del planificador*/
		
		p_proc_actual->estado = EJECUCION; /*Recordamos asignar el estado al nuevo proceso seleccionado por el planificador*/
		
		cambio_contexto(&(p_proc_anterior->contexto_regs), &(p_proc_actual->contexto_regs));
		replanificacio_pendent = FALSE;
	}
	printk("\n* * * * * * * * * * * *\n* INT. SW FINALITZA\n* * * * * * * * * * * *\n");
	return;
}

/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog){ /*MODIFICADA : deshabilitem interrupcions de rellotge */
	void *imagen, *pc_inicial;
	int error=0;
	int proc;
	BCP *p_proc;
	int n;

	proc=buscar_BCP_libre();
	if (proc==-1) return -1;	/* no hay entrada libre */

	/* A rellenar el BCP ... */
	p_proc=&(tabla_procs[proc]);

	/* crea la imagen de memoria leyendo ejecutable */
	imagen=crear_imagen(prog, &pc_inicial);
	if (imagen){
		p_proc->info_mem=imagen;
		p_proc->pila=crear_pila(TAM_PILA);
		fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,pc_inicial,&(p_proc->contexto_regs));
		p_proc->id=proc;
		p_proc->estado=LISTO;
		
		
		/* Comprovem si anem a crear el procés init (i per tant, si encara no hi ha procés actual)*/
		if(!p_proc_actual){
			/* al procés init que, al no tenir pare, PE=PB=MIN_PRO.*/
			p_proc->prioridad = MIN_PRIO;  /* El procés init té prioritat mínima, li assignem la seva prioritat per defecte.*/
			p_proc->prioridad_efectiva = MIN_PRIO;
			
		} else { /* Altrament No és el procés init.*/
			//Cada procés té una prioritat associada, que per defecte està heretada del pare
			p_proc->prioridad = p_proc_actual->prioridad;
			
			//Si PB=MIN_PRIO i PE<=MIN_PRIO MAI repartirà la seva PE amb els seus fills (que tindràn la mateixa PE)
			if((p_proc->prioridad == MIN_PRIO) && (p_proc->prioridad_efectiva <= MIN_PRIO)){
				p_proc->prioridad_efectiva = p_proc_actual->prioridad_efectiva;
			} else{
				/* En crear-se un nou procés la PE nova del pare i la del fill valdràn la meitat de la PE vella del pare */
				p_proc->prioridad_efectiva = p_proc_actual->prioridad_efectiva/2;  
				p_proc_actual->prioridad_efectiva = p_proc_actual->prioridad_efectiva/2;
			}
		}
		
		
		n=fijar_nivel_int(NIVEL_3); /* deshabilita interrupcions i desa el nivell anterior a n */
		insertar_ultimo(&lista_listos, p_proc); /* insertem al final de la cua de listos */
		fijar_nivel_int(n); /* recuperem el nivell anterior, habilitem interrupcions */
		
		error= 0;
	}
	else error= -1; /* fallo al crear imagen */
	
	
	return error;
}

/*
 * reajustar_prioridades
 * -----------------------------------------------------------------------------------------
 * Permet realitzar un reajust de les prioritats per a TOTS els processos actius del sistema
 * quan la prioritat efectiva de tots els processos de la llista lista_listos sigui igual a 0
 * 
 */ 
static void reajustar_prioridades(){
	int i;
	printk("-> REAJUSTE PRIORIDADES\n");
	
	/* TOTS els processos inclou tant els que tenim a lista_listos i lista_dormidos, podem usar un sol recurregut
	 * utilitzant tabla_procs, sempre i quan estiguin en ús! */
	for(i=0;i<MAX_PROC;i++){
		if(tabla_procs[i].estado!=NO_USADA && tabla_procs[i].estado!=TERMINADO){
			 tabla_procs[i].prioridad_efectiva =tabla_procs[i].prioridad_efectiva/2 + tabla_procs[i].prioridad;    
		}
	}
	return;
}



/* * * * * * * * * * * * * * * * * * * *  * * * * * * * *
 * Rutinas que llevan a cabo las llamadas al sistema
 *______________________________________________________
 *  -> sis_crear_proceso
 *  -> sis_escribir
 *  -> sis_terminar_proceso
 *  -> sis_get_pid
 *  -> sis_dormir
 * -> sis_fijar_prio
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


/*
 * Tratamiento de llamada al sistema crear_proceso.
 * Llama a la
 * funcion auxiliar crear_tarea sis_terminar_proceso
 */
int sis_crear_proceso(){
	char *prog;
	int res;

	printk("-> PROC %d: CREAR PROCESO\n", p_proc_actual->id);
	prog=(char *)leer_registro(1);
	res=crear_tarea(prog);
	return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir(){
	char *texto;
	unsigned int longi;

	texto=(char *)leer_registro(1);
	longi=(unsigned int)leer_registro(2);

	escribir_ker(texto, longi);
	return 0;
}


/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso
 */
int sis_terminar_proceso(){
	printk("-> FIN PROCESO %d\n", p_proc_actual->id);
	liberar_proceso();

	return 0; /* no debería llegar aqui */
}

/*
 * Tratamiento de llamada al sistema get_pid.
 */
int sis_get_pid(){
	printk("-> GET_PID: Proceso Actual %d\n", p_proc_actual->id);
	return p_proc_actual->id;
}


/*
 * Tratamiento de llamada al sistema dormir.
 */
int sis_dormir(){
    printk("-> SIS_DORMIR\n");
    int_reloj(); //activem les interrupcions software.
    
    p_proc_actual->ticks = leer_registro(1) * TICK; //Obtenim els ticks per posar-ho a dormir
    //Bloqueig voluntari:
    bloquear(&lista_dormidos);
    
    return 0; /* no debería llegar aqui */
}


int sis_fijar_prio(){
	printk("-> SIS_FIJAR_PRIO\n");
	unsigned int PBnova = leer_registro(1);
	unsigned int PBprevia = p_proc_actual->prioridad;
	
	/* el valor del paràmetre no està entre MIN_PRIO i MAX_PRIO */
	if(PBnova>MAX_PRIO && PBnova<MIN_PRIO ){
		return -1;
	}
	
	p_proc_actual->prioridad = PBnova;
	/* Controlem que la nova prioritat fixada es igual o superior a l'actual (el procés que s'executa té màxima prioritat).
	 * si la prioritat a assignar és inferior cal que el planificador revisi si tenim altres processos amb prioritat superior. (CCI) */
	if(PBnova < PBprevia){
		p_proc_actual->prioridad_efectiva = p_proc_actual->prioridad_efectiva * (PBnova/PBprevia);
		/*Activem interrupció software (Canvi de context involuntari) */
		replanificacio_pendent = TRUE; 
		activar_int_SW(); 
	}
	else{
		/* Altrament (PBnova>=PBprevia) */
		p_proc_actual->prioridad_efectiva = p_proc_actual->prioridad_efectiva * (PBnova+PBprevia)/(2*PBprevia);
	}
	return 0;		/*Retorna 0 si tot ha anat be*/
}



/*
 *
 * Rutina de inicialización invocada en arranque
 *
 */
int main(){
	/* se llega con las interrupciones prohibidas */
	iniciar_tabla_proc();

	instal_man_int(EXC_ARITM, exc_arit); 
	instal_man_int(EXC_MEM, exc_mem); 
	instal_man_int(INT_RELOJ, int_reloj); 
	instal_man_int(INT_TERMINAL, int_terminal); 
	instal_man_int(LLAM_SIS, tratar_llamsis); 
	instal_man_int(INT_SW, int_sw); 

	iniciar_cont_int();		/* inicia cont. interr. */
	iniciar_cont_reloj(TICK);	/* fija frecuencia del reloj */
	iniciar_cont_teclado();		/* inici cont. teclado */

	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	p_proc_actual->estado = EJECUCION; /*Recordamos asignar el estado al nuevo proceso seleccionado por el planificador*/
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	return 0;
}
