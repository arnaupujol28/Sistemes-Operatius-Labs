#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main (int argc, char* argv[]){

    
    int bytes = atoi(argv[3]);// convertir argv[3] a int ??
    
    int fd = open(argv[2], O_RDONLY, 0644);
    int sum = 0;

    if(strcmp(argv[1] , "text")== 0){
       
        //processar text
    }

    else{

        char buffer[bytes + 20]; // de quina mida s ha de crear?
        /* read(n,buffer, bytes); // va argv[3] ?

        for(int i = 0; i<bytes; i++){

            sum = sum + buffer[i];
        }*/

        int n;
        while((n = read(fd,buffer, 4) != 0)){

            //processar cada byte
        }
    }
}