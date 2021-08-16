#include <stdio.h>
#include <string.h>
#include "threadpool.h"
#include <unistd.h>
#include <stdlib.h>

int counter = 0;

int f(void * a){
    int index = *(int*)a;
     //print 0 to 3
    for (int i = 0; i < 3; i++)
    {
        printf("thread %d + i: %d\n",index , i);
        //paush for a sec
        sleep(1);
        counter++;
    }
    printf("-----\n");
    return 0;
}

/*yisrael bar 2/11/20*/
int main(int argc, char* argv[]) {
    threadpool* t0 = create_threadpool(atoi(argv[1]));
    printf("size of pool: %d\n", atoi(argv[1]));
    int num = atoi(argv[2]);
    printf("num of assiegnment: %d\n", num);
    int *array =  (int *)malloc(sizeof(int)*num);
    for (int i = 0; i < num; i++)
    {
        array[i] = i;
    }
    
    for (int i = 0; i < num; i++)
    {
            dispatch(t0,f,&array[i]);
    }

    // int num = 6;
    // ////test
    // int array[num];
    // for (int i = 0; i < num; i++)
    // {
    //     array[i] = i;
    // }
    
    // threadpool* t0 = create_threadpool(200);
    // for (int i = 0; i < num; i++)
    // {
    //         dispatch(t0,f, &array[i]);
    // }

    destroy_threadpool(t0);
    sleep(5);   
    free(array);
    printf("counter: %d\n",counter);
    return 0;
}

