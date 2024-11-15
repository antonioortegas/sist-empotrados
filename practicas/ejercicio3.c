//https://informatica.cv.uma.es/pluginfile.php/646484/mod_resource/content/15/sedmp_posix_tr_nb.pdf

/*
una tarea del estilo

void tarea(){
	while(1){
		tarea();
		sleep(1);
	}
}

NO ES UNA TAREA PERIODICA, es una tarea que se ejecuta de forma recurrente, pero no periodica
porque si queremos que la tarea se ejecute cada segundo, pero la tarea tarda 0.5 segundos en ejecutarse, 
la tarea se ejecutara cada 1.5 segundos, no cada segundo, y cada vez se ira retrasando mas y mas

Para "arreglar" esto con relojes, la idea es contar el tiempo que tarda en ejecutarse la tarea, y restarle ese tiempo al periodo de la tarea

MIRAR AHORA LOS COMENTARIOS DEL MAIN
*/

/*------------------------------------------------------------------------*/
/* C o n t r o l de Temperatura y P r e s i ó n ( p e r i o d i c i d a d mediante r e l o j e s )  */
/*------------------------------------------------------------------------*/
# include <stdlib.h>
# include <stddef.h>
# include <assert.h>
# include <string.h>
# include <stdio.h>
# include <unistd.h>
# include <time.h>
# include <errno.h>
# include <sched.h>
# include <pthread.h>
# include <sys/mman.h>

# define INACTIVO 0
# define ACTIVO 1

# define PERIODO_TMP_SEC 0
# define PERIODO_TMP_NSEC 500000000
# define PRIORIDAD_TMP 22
# define TMP_UMBRAL_SUP 100
# define TMP_UMBRAL_INF 90

# define PERIODO_STMP_SEC 0
# define PERIODO_STMP_NSEC 400000000
# define PRIORIDAD_STMP 24
# define STMP_VALOR_INI 80
# define STMP_INC 1
# define STMP_DEC 2

# define PERIODO_PRS_SEC 0
# define PERIODO_PRS_NSEC 350000000
# define PRIORIDAD_PRS 26
# define PRS_UMBRAL_SUP 1000
# define PRS_UMBRAL_INF 900

# define PERIODO_SPRS_SEC 0
# define PERIODO_SPRS_NSEC 350000000
# define PRIORIDAD_SPRS 28
# define SPRS_VALOR_INI 800
# define SPRS_INC 10
# define SPRS_DEC 20

# define PERIODO_MTR_SEC 1
# define PERIODO_MTR_NSEC 0
# define PRIORIDAD_MTR 20

struct Data_Tmp {
pthread_mutex_t mutex;
int estado;
int val;
};
struct Data_Prs {
pthread_mutex_t mutex;
int estado;
int val;
};
struct Data_Mtr {
struct Data_Tmp tmp;
struct Data_Prs prs;
};
/*------------------------------------------------------------------------*/
/*
* Nota: los "printf" están con el objetivo de depuración, para que el
* alumno pueda analizar el comportamiento de las tareas
*/
/*------------------------------------------------------------------------*/
/*-- Soporte -------------------------------------------------------------*/
/*------------------------------------------------------------------------*/
#define CHKN(syscall)
do {
    int err = syscall;
    if (err != 0) {
        fprintf(stderr, "%s:%d: SysCall Error: %s\n",
        __FILE__, __LINE__, strerror(errno));
        exit(EXIT_FAILURE);
    }
} while (0)
/*------------------------------------------------------------------------*/
#define CHKE(syscall)
do {
    int err = syscall;
    if (err != 0) {
        fprintf(stderr, "%s:%d: SysCall Error: %s\n",
        __FILE__, __LINE__, strerror(err));
        exit(EXIT_FAILURE);
    }
} while (0)
/*------------------------------------------------------------------------*/
const char* get_time(char *buf)
{
	time_t t = time(0);
	char* f = ctime_r(&t, buf); /* buf de longitud mínima 26 */
	f[strlen(f)-1] = '\0';
	return f;
}
/*------------------------------------------------------------------------*/
void addtime(struct timespec* tm, const struct timespec* val)
{
	tm->tv_sec += val->tv_sec;
	tm->tv_nsec += val->tv_nsec;
	if (tm->tv_nsec >= 1000000000L) {
		tm->tv_sec += (tm->tv_nsec / 1000000000L);
		tm->tv_nsec = (tm->tv_nsec % 1000000000L);
	}
}
/*------------------------------------------------------------------------*/
int clock_nanosleep_intr(clockid_t clk_id, int flags, const struct timespec *request,
struct timespec *remain)
{
	int err;
	while ((err = clock_nanosleep(clk_id, flags, request, remain)) == EINTR) {
		/* repetir si ha sido interrumpido */
	}
	return err;
}
/*------------------------------------------------------------------------*/
/*-- Tareas ------------------------------------------------------------*/
/*------------------------------------------------------------------------*/
void ini_tmp(struct Data_Tmp* data)
{
	data->estado = INACTIVO;
	data->val = STMP_VALOR_INI;
	CHKE(pthread_mutex_init(&data->mutex, NULL));
}
/*------------------------------------------------------------------------*/
void* tarea_stmp(void* arg)
{
	const struct timespec periodo = { PERIODO_STMP_SEC, PERIODO_STMP_NSEC };
	struct timespec next;
	struct Data_Tmp* data = arg;
	struct sched_param param;
	const char* pol;
	int policy;
	CHKE(pthread_getschedparam(pthread_self(), &policy, &param));
	pol = (policy == SCHED_FIFO) ? "FF" : (policy == SCHED_RR) ? "RR" : "--";
	printf("# Tarea Sensor de Temperatura [%s:%d]\n", pol, param.sched_priority);
	CHKN(clock_gettime(CLOCK_MONOTONIC, &next));
	while (1) {
		CHKE(clock_nanosleep_intr(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL));
		addtime(&next, &periodo);
		CHKE(pthread_mutex_lock(&data->mutex));
		switch (data->estado) {
			case INACTIVO:
				data->val += STMP_INC;
				break;
			case ACTIVO:
				data->val -= STMP_DEC;
				break;
		}
		CHKE(pthread_mutex_unlock(&data->mutex));
	}
	return NULL;
}
/*------------------------------------------------------------------------*/
void* tarea_tmp(void* arg)
{
	const struct timespec periodo = { PERIODO_TMP_SEC, PERIODO_TMP_NSEC };
	struct timespec next;
	char buf[30];
	struct Data_Tmp* data = arg;
	struct sched_param param;
	const char* pol;
	int policy;
	CHKE(pthread_getschedparam(pthread_self(), &policy, &param));
	pol = (policy == SCHED_FIFO) ? "FF" : (policy == SCHED_RR) ? "RR" : "--";
	printf("# Tarea Control de Temperatura [%s:%d]\n", pol, param.sched_priority);
	CHKN(clock_gettime(CLOCK_MONOTONIC, &next));
	while (1) {
		CHKE(clock_nanosleep_intr(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL));
		addtime(&next, &periodo);
		CHKE(pthread_mutex_lock(&data->mutex));
		switch (data->estado) {
			case INACTIVO:
				if (data->val > TMP_UMBRAL_SUP) {
					data->estado = ACTIVO;
					printf("# [%s] Activar inyección de aire frío\n",
					get_time(buf));
				}
				break;
			case ACTIVO:
				if (data->val < TMP_UMBRAL_INF) {
					data->estado = INACTIVO;
					printf("# [%s] Desactivar inyección de aire frío\n",
					get_time(buf));
				}
				break;
		}
		CHKE(pthread_mutex_unlock(&data->mutex));
	}
	return NULL;
}
/*------------------------------------------------------------------------*/
void ini_prs(struct Data_Prs* data)
{
	data->estado = INACTIVO;
	data->val = SPRS_VALOR_INI;
	CHKE(pthread_mutex_init(&data->mutex, NULL));
}
/*------------------------------------------------------------------------*/
void* tarea_sprs(void* arg)
{
	const struct timespec periodo = { PERIODO_SPRS_SEC, PERIODO_SPRS_NSEC };
	struct timespec next;
	struct Data_Prs* data = arg;
	struct sched_param param;
	const char* pol;
	int policy;
	CHKE(pthread_getschedparam(pthread_self(), &policy, &param));
	pol = (policy == SCHED_FIFO) ? "FF" : (policy == SCHED_RR) ? "RR" : "--";
	printf("# Tarea Sensor de Presión [%s:%d]\n", pol, param.sched_priority);
	CHKN(clock_gettime(CLOCK_MONOTONIC, &next));
	while (1) {
	CHKE(clock_nanosleep_intr(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL));
	addtime(&next, &periodo);
	CHKE(pthread_mutex_lock(&data->mutex));
	switch (data->estado) {
	case INACTIVO:
	data->val += SPRS_INC;
	break;
	case ACTIVO:
	data->val -= SPRS_DEC;
	break;
	}
	CHKE(pthread_mutex_unlock(&data->mutex));
	}
	return NULL;
}
/*------------------------------------------------------------------------*/
void* tarea_prs(void* arg)
{
	const struct timespec periodo = { PERIODO_PRS_SEC, PERIODO_PRS_NSEC };
	struct timespec next;
	char buf[30];
	struct Data_Prs* data = arg;
	struct sched_param param;
	const char* pol;
	int policy;
	CHKE(pthread_getschedparam(pthread_self(), &policy, &param));
	pol = (policy == SCHED_FIFO) ? "FF" : (policy == SCHED_RR) ? "RR" : "--";
	printf("# Tarea Control de Presión [%s:%d]\n", pol, param.sched_priority);
	CHKN(clock_gettime(CLOCK_MONOTONIC, &next));
	while (1) {
	CHKE(clock_nanosleep_intr(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL));
	addtime(&next, &periodo);
	CHKE(pthread_mutex_lock(&data->mutex));
	switch (data->estado) {
	case INACTIVO:
	if (data->val > PRS_UMBRAL_SUP) {
	data->estado = ACTIVO;
	printf("# [%s] Abrir válvula de presión\n", get_time(buf));
	}
	break;
	case ACTIVO:
	if (data->val < PRS_UMBRAL_INF) {
	data->estado = INACTIVO;
	printf("# [%s] Cerrar válvula de presión\n", get_time(buf));
	}
	break;
	}
	CHKE(pthread_mutex_unlock(&data->mutex));
	}
	return NULL;
}
/*------------------------------------------------------------------------*/
void* tarea_mtr(void* arg)
{
	/*
	La idea es:
	miro en que instante de tiempo estoy
	programar la siguiente ejecucion de la tarea
	*/
	const struct timespec periodo = { PERIODO_MTR_SEC, PERIODO_MTR_NSEC };
	struct timespec next;
	char buf[30];
	struct Data_Mtr* data = arg;
	struct sched_param param;
	const char* pol;
	int policy;
	CHKE(pthread_getschedparam(pthread_self(), &policy, &param));
	pol = (policy == SCHED_FIFO) ? "FF" : (policy == SCHED_RR) ? "RR" : "--";
	printf("# Tarea monitorización [%s:%d]\n", pol, param.sched_priority);
	CHKN(clock_gettime(CLOCK_MONOTONIC, &next));
	while (1) {
	CHKE(clock_nanosleep_intr(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL));
	addtime(&next, &periodo);
	/*--------------------------*/
	CHKE(pthread_mutex_lock(&data->tmp.mutex));
	CHKE(pthread_mutex_lock(&data->prs.mutex));
	printf("# [%s] Temperatura: %d %s Presión: %d %s\n",
	get_time(buf),
	data->tmp.val, (data->tmp.estado == INACTIVO ? "++" : "--"),
	data->prs.val, (data->prs.estado == INACTIVO ? "++" : "--"));
	CHKE(pthread_mutex_unlock(&data->prs.mutex));
	CHKE(pthread_mutex_unlock(&data->tmp.mutex));
	/*--------------------------*/
	}
	return NULL;
}
/*------------------------------------------------------------------------*/
/*-- Programa Principal --------------------------------------------------*/
/*------------------------------------------------------------------------*/
int maximo(int a, int b, int c, int d, int e)
{
	int m = (a > b ? a : b);
	m = (c > m ? c : m);
	m = (d > m ? d : m);
	m = (e > m ? e : m);
	return m;
}
void usage(const char* nm)
{
	fprintf(stderr, "usage: %s [-h] [-ff] [-rr]\n", nm);
	exit(EXIT_FAILURE);
}
void get_args(int argc, const char* argv[], int* policy)
{
	int i;
	for (i = 1; i < argc; ++i) {
	if (strcmp(argv[i], "-h")==0) {
	usage(argv[0]);
	} else if (strcmp(argv[i], "-ff")==0) {
	*policy = SCHED_FIFO;
	} else if (strcmp(argv[i], "-rr")==0) {
	*policy = SCHED_RR;
	} else {
	usage(argv[0]);
	}
	}
}
int main(int argc, const char* argv[])
{
	struct Data_Mtr data;
	pthread_attr_t attr;
	struct sched_param param;
	const char* pol;
	int pcy, policy = SCHED_FIFO;
	int prio0 = 1;
	int prio1 = PRIORIDAD_TMP;
	int prio2 = PRIORIDAD_PRS;
	int prio3 = PRIORIDAD_MTR;
	int prio4 = PRIORIDAD_STMP;
	int prio5 = PRIORIDAD_SPRS;
	pthread_t t1, t2, t3, t4, t5;
	/*
	* La tarea principal debe tener la mayor prioridad, para poder
	* crear todas las tareas necesarias.
	*/
	get_args(argc, argv, &policy);
	// doy la prioridad maxima a la tarea principal, la que se encarga de crear las demas tareas
	// para eso, obtengo la prioridad maxima de las tareas que se van a crear, y le sumo 1, para que la tarea principal tenga mas prioridad
	prio0 = maximo(prio1, prio2, prio3, prio4, prio5) + 1;
	CHKN(mlockall(MCL_CURRENT | MCL_FUTURE));
	// me asigno la prioridad a mi mismo (por eso uso pthread_self())
	CHKE(pthread_getschedparam(pthread_self(), &pcy, &param));
	param.sched_priority = prio0;
	CHKE(pthread_setschedparam(pthread_self(), policy, &param));
	pol = (policy == SCHED_FIFO) ? "FF" : (policy == SCHED_RR) ? "RR" : "--";
	ini_tmp(&data.tmp);
	ini_prs(&data.prs);
	// para crear una hebra con una prioridad especifica, hay que crear un atributo de hebra, y asignarle la prioridad
	CHKE(pthread_attr_init(&attr));
	// con esta funcion, le digo al atributo que la hebra que se cree con este atributo, va a tener la prioridad que le he asignado
	CHKE(pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED));
	// la politica sirve para modelar el comportamiento de la hebra cuando hay otra con la misma prioridad
	// fifo: la hebra que llega primero, es la que se ejecuta primero
	// rr: se turnan
	CHKE(pthread_attr_setschedpolicy(&attr, policy));
	param.sched_priority = prio1;
	CHKE(pthread_attr_setschedparam(&attr, &param));
	CHKE(pthread_create(&t1, &attr, tarea_tmp, &data.tmp));
	param.sched_priority = prio2;
	CHKE(pthread_attr_setschedparam(&attr, &param));
	CHKE(pthread_create(&t2, &attr, tarea_prs, &data.prs));
	param.sched_priority = prio3;
	CHKE(pthread_attr_setschedparam(&attr, &param));
	CHKE(pthread_create(&t3, &attr, tarea_mtr, &data));
	param.sched_priority = prio4;
	CHKE(pthread_attr_setschedparam(&attr, &param));
	CHKE(pthread_create(&t4, &attr, tarea_stmp, &data.tmp));
	param.sched_priority = prio5;
	CHKE(pthread_attr_setschedparam(&attr, &param));
	CHKE(pthread_create(&t5, &attr, tarea_sprs, &data.prs));
	CHKE(pthread_attr_destroy(&attr));
	printf("# Tarea principal [%s:%d]\n", pol, prio0);
	CHKE(pthread_join(t1, NULL));
	CHKE(pthread_join(t2, NULL));
	CHKE(pthread_join(t3, NULL));
	CHKE(pthread_join(t4, NULL));
	CHKE(pthread_join(t5, NULL));
	CHKE(pthread_mutex_destroy(&data.tmp.mutex));
	CHKE(pthread_mutex_destroy(&data.prs.mutex));
	return 0;
}
/*------------------------------------------------------------------------*/
