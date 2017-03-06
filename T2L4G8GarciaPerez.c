/* 
 * 
 * Héctor García Cabezón 
 *          y
 * Alejandro Pérez Maldonado
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

/*      VARIABLES GLOBALES     */

#define NUMCONSINT 4 // Maximo numero de consumidores intermedios
int posicion_consint = 0; // Posiciones de los consumidores intermedios para el buffer1
int posicion_consint2 = 0; // Posiciones de los consumidores intermedios para el buffer2
int cantidad = 0; // La cantidad de numeros producidos hasta este
int *buffer1;  // Buffer circular 1
char **buffer2; //Buffer circular 2 

/*  Semaforos   */

sem_t numeros, huecos, intermedios; //  Para coordinar el hilo productor con los consumidores intermedios
sem_t cadenas, espacios, final;  //  Para coordinar los hilos consumidores con el consumidor final


/*      ESTRUCTURA      */

struct parametros{
    int nNumeros;
    int TamBuffer1;
    int TamBuffer2;
    int id;
};

/*      FUNCION PARA GENERAR NUMEROS ALEATORIOS     */

int aleatorio(void){ return random()%100000; }

/*      FUNCION PARA SABER SI ES PRIMO      */

bool es_primo(int numero){
    int divisor;
    for(divisor = 2; divisor <= sqrt(numero); divisor++){
        if(numero%divisor == 0){
            return false;
        }
    }
    return true;
}

/*      HILO  PRODUCTOR         */

void *productor(void *arg){
    
    struct parametros *p;
    p = (struct parametros *)arg;
    int i,dato, posicion = 0;
    
    for(i = 0; i < p -> nNumeros; i++){
        dato = aleatorio();
        
       /*   Cordinamos el hilo productor para que produzca en función de como consumen los datos los consumidores intermedios    */
        
        sem_wait(&huecos);
        buffer1[posicion] = dato;
        posicion = (posicion + 1) % p ->TamBuffer1;
        sem_post(&numeros);
    }
    pthread_exit(NULL);
}

/*      HILOS CONSUMIDORES INTERMEDIOS      */

void *consumidorIntermedio(void *arg){
    
    struct parametros *p;
    p = (struct parametros *)arg;
    int dato, idNum, contador, contador2;
    int id = p -> id ;    //  Obtenemos el identificador del hilo
    char respuesta[15];
    
    while(1){
            
            sem_wait(&intermedios);
            if(posicion_consint >= p ->nNumeros){
                  pthread_exit(NULL);    
            }
                  
                  /*    Evitamos que mas de un hilo consumidor intermedios entre en el mismo indice del buffer1  */
                  /*    Cordinamos el hilo productor con los consumidores intermedios   */
                  
                  sem_wait(&numeros);
                  contador = posicion_consint;
                  posicion_consint++;
                  dato = buffer1[contador % p->TamBuffer1];
                  cantidad = cantidad + 1;
                  idNum = cantidad;
                  sem_post(&huecos);
                  sem_wait(&final);
                  sem_post(&intermedios);
                  
                  /*    Coordinamos los hilos para que generen la salida por orden y coordinamos los hilos consumidores intermedios con el consumidor final   */

                  sem_wait(&espacios);
                  contador2 = posicion_consint2;
                  posicion_consint2++;
                  
                  /*****      Comprobar si es primo ******/
                  
                  if(es_primo(dato)){
                            sprintf(buffer2[contador2 % p->TamBuffer2], "Hilo: %d. Valor producido numero: %d. Cantidad: %d. Es primo.\n", id, dato, idNum);
                  }else{
                            sprintf(buffer2[contador2 % p->TamBuffer2], "Hilo: %d. Valor producido numero: %d. Cantidad: %d. No es primo.\n", id, dato, idNum);
                  }
                  
                  /*********************************************************/
                  
                  sem_post(&cadenas);   
                  sem_post(&final);
    } 
}

/*      CONSUMIDOR FINAL        */

void *consumidorFinal(void *arg){
    
    struct parametros *p;
    p = (struct parametros *)arg;
    FILE *salida;
    int i, cont, posicion = 0;
    salida = fopen("salida.txt","w+");   //  Abre el archivo, si no existe lo crea, y si exites resetea su contenido

    /*  Comprobamos que se pueda crear el fichero   */
    
    if(salida == NULL){
        fprintf(stderr, "El fichero no se puede abrir");
        return (EXIT_SUCCESS);
    }
    
    for(i = 0; i < p->nNumeros; i++){
        
        cont = posicion;
        posicion++;
        sem_wait(&cadenas);
        fputs(buffer2[cont % p->TamBuffer2], salida);
        sem_post(&espacios);
    }
    
     fclose(salida);
     pthread_exit(NULL);
}

int main(int argc, char  *argv[]) {
    
    /*      COMPROBACIONES DE PASO DE ARGUMENTOS        */
    
    int entero;  //  Para comprobaciones de tipo (int) en el paso de argumentos          
    
    if(argc != 4){
        fprintf(stderr, "Error en el numero de argumentos\n");
        return (EXIT_SUCCESS);
    }else{
        if(sscanf (argv[1],"%d",&entero) != 1){
            fprintf(stderr, "Primer argumento erroneo\n");
            return (EXIT_SUCCESS);
        }
        if((sscanf (argv[2],"%d",&entero) != 1) || atoi(argv[2]) > (atoi(argv[1])/2)){
            fprintf(stderr, "Segundo argumento erroneo\n");
            return (EXIT_SUCCESS);
        }
        if((sscanf (argv[3],"%d",&entero) != 1) || atoi(argv[3]) > (atoi(argv[1])/2)){
            fprintf(stderr, "Tercer argumento erroneo\n");
            return (EXIT_SUCCESS);
        }
    }
    
    /*      DECLARAMOS VARIABLES    */
    
    int i;
    struct parametros p;                 // Estructura para pasar parámetros a los hilos
    p.nNumeros = atoi(argv[1]);     //  Cantidad de numeros_aleatorios que tiene que generar el hilo productor
    p.TamBuffer1 = atoi(argv[2]);   //  Tamaño del buffer circular compartido entre Productor y Consumidores intermedios
    p.TamBuffer2 = atoi(argv[3]);   //  Tamaño del buffer circular compartido entre los Consumidores intermedios y el Consumidor final
    int tamCadena = 50;                 //  Máximo tamaño que podrá tener la cadena generada por los hilos consumidores intermedios
    pthread_t hiloProductor;             //  Hilo Productor
    pthread_t hiloConsumidorInt[NUMCONSINT];     //  Hilos Consumidores intermedios
    pthread_t hiloFinal;    // Hilo Consumidor final
    
   
    /*      MEMORIA DINAMICA        */
    
    buffer1 = malloc(p.TamBuffer1 * sizeof(int));   //  Asignamos memoria dinámica al primer buffer
    
    buffer2 = malloc (p.TamBuffer2 * sizeof (char*));   // Asignamos memoria dinámica a la parte que contendra las cadenas del segundo buffer
    
    for(i = 0; i<p.TamBuffer2; i++){
        buffer2[i] = malloc(tamCadena * sizeof(char));  //  Asignamos memoria dinámica a cada cadena del segundo buffer 
    }
    
    /*      COMPROBACIÓN DE QUE LA MEMORIA DINÁMICA SE RESERVA CORRECTAMENTE        */
    
    if(buffer1 == NULL){
         fprintf(stderr, "Error al reservar memoria dinámica");
        return (EXIT_SUCCESS);
    }
    if(buffer2 == NULL){
        fprintf(stderr, "Error al reservar memoria dinámica");
        return (EXIT_SUCCESS);
    }
     
    /*      INICIALIZAMOS LOS SEMAFOROS     */
    
    sem_init(&numeros, 0, 0);
    sem_init(&huecos, 0, p.TamBuffer1);
    sem_init(&cadenas, 0 , 0);
    sem_init(&espacios, 0, p.TamBuffer2);
    sem_init(&intermedios, 0, 1);
    sem_init(&final, 0, 1);
   
    /*      CREAMOS LOS HILOS       */
    
    pthread_create(&hiloProductor, NULL, productor, (void *)&p);  //  Creamos el hilo productor
    
    for(i=0; i<NUMCONSINT; i++){
        p.id = i + 1;   //  Creamos el identificador para cada hilo con ayuda de la estructua ya creada
        pthread_create(&hiloConsumidorInt[i], NULL, consumidorIntermedio, (void *)&p);    //  Creamos los hilos consumidores intermedios
    }
    
    pthread_create(&hiloFinal, NULL, consumidorFinal, (void *)&p);  //Creamos el hilo Consumidor final
    
    /*      ENTRAMOS EN LOS HILOS       */
    
    pthread_join(hiloProductor, NULL);  //  Accedemos al hilo Productor
    
    for(i=0; i<NUMCONSINT; i++){
        pthread_join(hiloConsumidorInt[i], NULL);   //  Accedemos a los hilos Consumidores intermedios
    }
    
   pthread_join(hiloFinal, NULL);  // Accedemos al hilo Consumidor final
    
    /*      LIBERAMOS LA MEMORIA DINAMICA ASIGNADA      */
    
    free(buffer1);
    free(buffer2);
    
    /*      LIBERAMOS LOS SEMAFOROS       */
    
    sem_destroy(&numeros);
    sem_destroy(&huecos);
    sem_destroy(&cadenas);
    sem_destroy(&espacios);
    sem_destroy(&intermedios);
    sem_destroy(&final);
    
    return (EXIT_SUCCESS);
}
