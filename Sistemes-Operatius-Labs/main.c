#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main (int argc, char* argv[]){

    
    int bytes = atoi(argv[3]);// convertir argv[3] a int ??
    int bytes_usables = bytes - (bytes%sizeof(int));
    
    int fd = open(argv[2], O_RDONLY, 0644);
    int sum = 0;

    if(strcmp(argv[1] , "text")== 0){
       
        //processar text
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