# HTTP-thread-pool-server
HTTP thread pool server


=== Description ===

Description: 
The following program will create a HTTP Server that support various type of files,
also it can be multitasking, it means few costumers can use the server in same time because
we created threadpool.

Program files:
threadpool.c - creating a threadpool that can do multiply functions in same time
server.c - create a http server that support serveral functions and can be done in same time


Functions:
    
threadpool.c : 
threadpool* create_threadool(int num_threads_in_pool) - create a threadpool with size num of threads and init all the data around for mutex etc.
void dispatch(threadpool*,dispatch_fn,void *) -  this function enter a job to do into a queue
void* do_work(void*) - taking the first elemnt in the queue and doing the mutex job, also waiting if the queue is empty.
void* destroy_threadpool(threadpool*) - destroy all threads, he will wait untill they all will finish

server.c :
char *get_mime_type(char*) - return us the contect to the response
checkDigits(char*) - get a char and check if there are only digits
int ReadFromSock(char*,int) - get the socket and write the info into the char that we get to the func
parseHeaders(int*,char*,char*,char*) - check for valid headers and put them into the chars, also if there is an error we write the error code into the int.
parsePath(char*,int*) - parsing the path and in case of error write it into the int
char *Error2Sttring(int) - using the error we submit in other check and translate it into the right info
int dsipatchFn(void*) - the work that the server doing, we send it to the threadpool dispatch func
void sendErrorResp - create the right message to send error respond
int sendFileResp - check for permissions and create right message to send the file
int sendFolderResp - check for permissions and create right message to send the folder entity
