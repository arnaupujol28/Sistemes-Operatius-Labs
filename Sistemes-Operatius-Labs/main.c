// compilar aixi: gcc main.c circularBuffer.c -o main
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "circularBuffer.h"

int main (int argc, char* argv[]){

    //comprovar error
    if (argc != 4) {
        write(2, "Us: ./myprogram binary|text path buffersize\n", 40);
        return 1;
    }

    
    int bytes = atoi(argv[3]);// convertir argv[3] a int ??
    int bytes_usables = bytes - (bytes%sizeof(int));
    
    int fd = open(argv[2], O_RDONLY, 0644);
    if (fd<0){//error
        write(2, "Error en obrir l'arxiu", strlen("Error en obrir l'arxiu"));
        return 1;
    }
    int sum = 0;

    if(strcmp(argv[1] , "text")== 0){
       
        //crear buffer circular
        CircularBuffer buffer_circular;
        buffer_init(&buffer_circular, bytes); 
        
        char tmp[bytes];
        int n;
        int eof = 0;

        while(1){

            //llegir buffer normal
            n = read(fd, tmp, bytes);
            if (n == 0) eof = 1;
            if (n<0){//error
                write(2, "Error en llegir l'arxiu", strlen("Error en llegir l'arxiu"));
                return 1;
            }

            // push de cada char llegit al circular buffer
            for(int i = 0; i<n;++i){

                while(buffer_free_bytes(&buffer_circular) == 0){ // buffer ple, processar elements 

                    int k = buffer_size_next_element(&buffer_circular, ',',eof); // recorre el buffer des de start fins trobar ',' i retorna quans bytes hi ha
                    
                    if(k == -1){// error o buffer massa petit
                        buffer_deallocate(&buffer_circular);
                        return -1;
                    }

                    // extreure k bytes del buffer circular i acumular-los
                    char num[k+1]; // el +1 és \0 ? 
                    for (int j = 0; j<k; ++j){

                        num[j] = (char)buffer_pop(&buffer_circular);
                    }
                    num[k] = '\0';

                    // Treure la coma final si hi és
                    if (k > 0 && num[k - 1] == ',') {
                        num[k - 1] = '\0';
                    }
                    // convertir els char a int
                    if (num[0] != '\0') {
                        sum += atoi(num);
                    }

                }
                    // omplir el buffer circular un altre cop
                    buffer_push(&buffer_circular, (char)tmp[i]);
            }

            while (1) {
                int k = buffer_size_next_element(&buffer_circular, ',', eof);
                if (k == -1) break;
    
                char num[k + 1];
    
                for (int j = 0; j < k; j++) {

                    num[j] = (char)buffer_pop(&buffer_circular);
                }
                num[k] = '\0';
    
                if (k > 0 && num[k - 1] == ',') {
                    num[k - 1] = '\0';
                }
    
                if (num[0] != '\0') {
                    sum += atoi(num);
                }
            }
    
            // 4) Si hem arribat a EOF i ja no queda res a processar, sortim
            if (eof) break;
        }

        char suma[100];
        sprintf(suma, "the sum is %d\n", sum);
        write(1,suma, strlen(suma));
        return 0;    

        
    }

    else{

        char buffer[bytes_usables]; // de quina mida s ha de crear?
        int n;
        while((n = read(fd,buffer, bytes_usables)) >0){

            int m = n / sizeof(int); // m = numero de ints que hi ha al buffer
            int* ints = (int*) buffer; // tracta l'array de caracters com a ints

            for(int i = 0; i<m; i++){

                sum = sum + ints[i];// aqui mirara els bytes de 4 en 4, es a dir recorrera els ints
            }
            
        }
        char suma[100];
        sprintf(suma, "the sum is %d\n", sum);
        write(1,suma, strlen(suma));
        return 0;


    }
}