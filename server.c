#include "threadpool.h"
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <dirent.h>


//defines we used in code
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define DEFAULT_PROTOCOL "HTTP/1.0"
#define DEFAULT_PROTOCOL1 "HTTP/1.1"

//functions helped us on code
int checkDigits(char*); // help us to check if a string contain only numbers
char *Error2String(int);// make the error number to string code
int parseHeaders(char*,char*,char*,int*); // parse the eheader
int parsePath(char*,int*); // parse the path
int dispatchFn(void*); // function to dispatch
int readFromSoc(int,char*); // read from socket
int writeToSoc(int,char*,int);// write to socket
void sendErrorResp(int,char*,char*,char*,int);  // make error replay
int sendFileResp(int,char*, char*,char*,int*);// make file repaly
int sendFolderResp(int,char*,char*,char*,int*);//make folder replay
char *get_mime_type(char*);// get supported type




//this function will if a text contains only digits
int checkDigits(char* text)
{
	int i;
	for (i = 0; i < strlen(text); i++)
		if (text[i] < '0' || text[i] > '9')  
			return -1;
	return 0;
}

//the type of file to send in the headers, added a faw of my own
char *get_mime_type(char *name)
{
char *ext = strrchr(name, '.');
if (!ext) return NULL;
if (strcmp(ext,".txt") == 0||strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
if (strcmp(ext, ".gif") == 0) return "image/gif";
if (strcmp(ext, ".png") == 0) return "image/png";
if (strcmp(ext, ".css") == 0) return "text/css";
if (strcmp(ext, ".au") == 0) return "audio/basic";
if (strcmp(ext, ".wav") == 0) return "audio/wav";
if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
return NULL;
}

//function that we send to the threadpool
int dispatchFn(void *arg)
{
	// define var that we need to use here
	char *headerC = NULL;
	int checker;
	int socfd = *(int*)arg; 
	int	errType = 0; 
	char recMsg[4000];
	char path[4000];
	char protocol[9] = { DEFAULT_PROTOCOL };
	char timeBuffer[128];
	time_t now;
	
	//put curr time and read req from soc
	now = time(NULL);
	strftime(timeBuffer, sizeof(timeBuffer), RFC1123FMT, gmtime(&now));//setup the time
	checker = readFromSoc(socfd, recMsg);
	if (checker == -1)// if read failed
	{
		perror("dispatch_function : readFromSocket() Failure\n");
		return -1;
	}

	//in case rhe message is empty
	if (strlen(recMsg) == 0 )
		return -1;
	
	headerC = strstr(recMsg, "\r\n");
	//printf("%s",header_check);
	if (headerC == NULL) // no \r\n
	{
		sendErrorResp(socfd, NULL, protocol, timeBuffer, 400); // send error 400 because no \r\n
		return -1;
	}
	
	//seperate the header
	headerC[0] = '\0';
	checker = parseHeaders(recMsg, path, protocol, &errType);
	if (checker == -1)
	{
		sendErrorResp(socfd, path, protocol, timeBuffer, errType);
		return -1;
	}
	
	//we got path from headerparse and now we parse the path
	checker = parsePath(path, &errType); // checker save if it parsed well
	if (checker == -1)
	{
		sendErrorResp(socfd, path, protocol, timeBuffer, errType); // in case it no we send the rror type to create error message
		return -1;
	}
	
	// we check which error/accept and send the respond according to it
	if (errType == 201) // we are in case of file
	{
		checker =sendFileResp(socfd, path, protocol, timeBuffer, &errType);// we call the function that send file response
		if(checker == -1)// if it fail
		{
			if (errType != 0)// if the error type got value we send error response
				sendErrorResp(socfd, path, protocol, timeBuffer, errType);
			return -1;
		}
	}
	else if (errType == 202) // we re in cae of folder
	{	
		checker =sendFolderResp(socfd, path, protocol, timeBuffer, &errType);// check folder
		if(checker == -1)// if fail send correct error response
		{
			if (errType != 0)
				sendErrorResp(socfd, path, protocol, timeBuffer, errType);
			return -1;
		}
	}
	close(socfd);
	return 0;
}


//function to read from socket we get the socket fd and put the message into rec message
int readFromSoc(int sockFd, char *recMsg)
{
	int alreadyRead = 0;
	char temp[2000];
	while (1) 
	{
		alreadyRead = read(sockFd, temp, sizeof(temp)); // we add how much already read
		if (alreadyRead < 0) // in case that there is nothing to read anymore
			return -1;
		else if (alreadyRead > 0) // in case we can still read
		{
			sprintf(recMsg + strlen(recMsg), "%s", temp); // put the pointer into the right place in recmsg and add it tmp
			if (strstr(temp, "\r\n")) // if we found \r\n we should stop read
				break;
			bzero(temp, sizeof(temp));//init temp to be empty
		}
		else
			break;
	}
	return 0;
}

//make verification to the hader and also parse it into the right things
int parseHeaders(char *httpReq, char *path, char *protocol, int *errType)
{
	//find which protocol used + buffer for path
	char *tmp0 = strstr(httpReq, "HTTP/1.0");
	char *tmp1 = strstr(httpReq, "HTTP/1.1");
	char pathBuffer[4000];
	
	// if there is no HTTP/1.X protocol it means error will be 400
	if (tmp0 == NULL && tmp1 == NULL)
	{
		*errType = 400;
		return -1;
	}
	else if (tmp0 != NULL && tmp1 == NULL) //HTTP/1.0
	{
		strcpy(protocol, "HTTP/1.0");// set into our protocol which protocol used (1.0)
		if (tmp0[8] != '\0')// check also if its the last arguemnt or not last argument
		{
			*errType = 400;
			return -1;
		}
			
	}
	else if (tmp1 != NULL && tmp0 == NULL) // same here for protocol http1.1
	{
		strcpy(protocol, "HTTP/1.1");
		if (tmp1[8] != '\0') 
		{
			*errType = 400;
			return -1;
		}
	}

	if (strncmp(httpReq, "GET ", 4) != 0)// check if GET request if placed well from the beginning with space
	{
		*errType = 400;
		return -1;
	}

	httpReq += 4; // HTTP REQUEST WITHout GET_
	int n = strlen(httpReq) - (strlen(protocol) + 1); // the place between the protocol and path
	if (httpReq[n] != ' ')//check for space
	{
		*errType = 400;
		return -1;
	}

	//set up our path
	strncpy(path, httpReq, n); 
	sprintf(pathBuffer, ".%s", path);
	sprintf(path, "%s", pathBuffer);
	return 0;
}

//verify that our path parsed well and verificated
int parsePath(char *path, int *errType)
{
	
	struct stat fs;
	int pathSize = strlen(path);
	struct dirent *fileEntity = NULL;
	DIR *folder = NULL;
	
	//checking permissions
	if (lstat(path, &fs) <0)// check if there is permitions if no 404 error
	{ 
		*errType = 404;//if no return 404 error
		return -1;
	}


	//if ther is a path in folder
	if (S_ISDIR(fs.st_mode))//check the path of folder
 	{
		if(path[pathSize-1]!='/')//if the the path not ending with '/' for exmp balbla/tt
		{
			*errType = 302;// we set error 302 for this case
			return -1;
		}
		else 
		{
			folder = opendir(path);//open current folder path
			if (folder == NULL)//if unable to open the folder
			{	
				*errType = 500;//error case 500
				return -1;
			}
			fileEntity = readdir(folder);// point to the next folder
			while (fileEntity != NULL)// while there is still folders
			{
			
				if (!strcmp(fileEntity->d_name, "index.html"))// if we found in the file entity name index html
				{
					strcat(path, "index.html");//we add it to the path
					if (stat(path, &fs) == -1)// checking permissions of path
					{ 
						*errType = 500;
						return -1;
					}
					*errType = 201;
					closedir(folder);
					return 0;
				}
				fileEntity = readdir(folder);//go to the next folder
			}
			*errType = 202;//succeeded and its folder
			closedir(folder);
		}
	} 
	else if (S_ISREG(fs.st_mode))//if its path of file
	{
		*errType = 201;
	}
	return 0;
}

//create error with headers and html code and write it
void sendErrorResp(int sockFd, char *path, char *protocol, char *time, int err)
{
	char htmlCode[1000]={0};
	char headers[1000]={0};
	char response[1000]={0};
	char* errToString=Error2String(err);
	char *txtErr;
	
	if(err == 302)// if its error 302 and we need the path and create header
		sprintf(headers,"%s %s\r\nServer: webserver/1.0\r\nDate: %s\r\nLocation: %s\r\nContent-Type: %s\r\n",DEFAULT_PROTOCOL1,errToString,time,path+1,"text/html");
	else // in any other case of errors
		sprintf(headers,"%s %s\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: %s\r\n",DEFAULT_PROTOCOL1,errToString,time,"text/html");
	
	//check the right error type we mentioned before and gave to here and create the html code
	if(err == 302)
		txtErr = "Directories must end with a slash.";
	else if(err == 400)
		txtErr = "Bad Request.";
	else if(err == 403)
		txtErr ="Access denied.";
	else if(err == 404)
		txtErr ="File not found.";
	else if(err == 500)
		txtErr ="Some server side error.";
	else if(err == 501)
		txtErr ="Method is not supported";
	sprintf(htmlCode,"<HTML><HEAD><TITLE>%s</TITLE></HEAD>\r\n<BODY><H4>%s</H4>%s</BODY></HTML>\r\n\r\n",errToString,errToString,txtErr);//we add the correct error to the htmlcode
	sprintf(headers + strlen(headers),"Content-Length: %lu\r\nConnection: close\r\n\r\n", strlen(htmlCode));//add lest information
	sprintf(response, "%s%s", headers, htmlCode);//creating the response message
	writeToSoc(sockFd, response, strlen(response));//write it to the socket
}

//Sending file respond and also check features
int sendFileResp(int socketFd, char *path, char *protocol, char *time, int *errType)
{	
	char response[1000]={0};
	char fileData[8000]={0};
	char timeBuffer[32]={0};
	char *fileName = NULL;
	int fileFd;
	int readBytes;
	int place;
	struct stat fileStats;
	struct dirent *fileEntity = NULL;
	DIR *folder = NULL;

	
	fileFd = open(path, O_RDONLY, S_IRUSR);// only read permission
	if (fileFd < 0) //check for perm
	{
		if (access(path,R_OK) != 0) // in case no permission
			*errType = 403;
		return -1;
	}
	
	//if it fall in stat ( no valid path )
	if (lstat(path, &fileStats) < 0)
	{
		*errType = 404;
		return -1;
	}
	
	//get the file name
	for (place = strlen(path)-1; place>=0; place--)
	{ 
		if (path[place] == '/')//check if its / at i like to get the file name
		{
			fileName = &(path[place + 1]);
			break;
		}
	}
	
	//check mime type for the headers
	char *mime_type = get_mime_type(fileName);
	if (mime_type == NULL)// if there is no such a type
	{
		*errType = 403;
		return -1;
	}
	
	//cut the path and back the file again after
	path[place] = '\0';//put it in the end
	folder = opendir(path); 
	path[place] = '/';//add also / if there is more info
	
	//to get the dir of the file
	fileEntity = readdir(folder);//first folder
	while (fileEntity != NULL) // we run on all the dirnet untill its NULL
	{
		if (!strcmp(fileEntity->d_name, fileName))
		{
			//printf("WE ARE HEREEEEEEEEEEEEEEEEEEEEEEEEEEE");
			break;
		}
		fileEntity = readdir(folder);// go to the next folder
	}
	closedir(folder);
	
	//create a response message of the headers
	sprintf(response,"%s %s\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: ",protocol, Error2String(200),time);
	strftime(timeBuffer, sizeof(timeBuffer), RFC1123FMT, gmtime(&fileStats.st_mtime));// set up curr time
	sprintf(response + strlen(response),"%s\r\nContent-length: %lu\r\nLast-Modified: %s\r\nConnection: close\r\n\r\n",mime_type, fileStats.st_size, timeBuffer);

	if (writeToSoc(socketFd, response, strlen(response)) < 0)// write to the socket the response message we create
	{
		perror("sendFileResp : FAILURE WRITE TO SOCKET ()");
		*errType = 0;
		return -1; 
	}
	
	while (1)// read data from the files
	{
		readBytes = read(fileFd, fileData, sizeof(fileData));// how much we read already
		if(readBytes > 0)// all the time we still reading keep doing it
		{
			if (writeToSoc(socketFd, fileData, readBytes) < 0)
			{
				perror("sendFileResp : FAILURE WRITE TO SOCKET ()");
				*errType = 0;
				return -1; 
			}
		}
		else if(readBytes == 0)
			break;
		
		else//failed to read this case its not for this socket
		{
			perror("error on read from socket ()\n");
			*errType = 500; 
			close(fileFd); 
			return -1;
		}
	}
	write(socketFd,"\r\n\r\n",4);
	close(fileFd);
	return 0;
}

//send a response with the folder information in a table
int sendFolderResp(int socketFd, char *path, char *protocol, char *time, int *errType)
{
	//variables
	int counter = 0;
	char headers[1000]={0};
	char fileLastTime[32]={0};
	char folderLastTime[32]={0};
	struct stat fileInfo={0};
	struct dirent *currFileEntity=NULL;
	DIR *folder;
	
	folder = opendir(path); // open the path
	currFileEntity = readdir(folder);
	while (currFileEntity != NULL)  // here we run and count how many file in the folder
	{
		if (strcmp (currFileEntity->d_name, ".")) // we dont notice the . 
			counter++;
		currFileEntity = readdir(folder);
	}
	closedir(folder);

	char *htmlCode = (char*)calloc(counter*(512), sizeof(char)); // for each found upon we give 512 bytes	
	folder = opendir(path); //can't fail, path already validated
	if (lstat(path, &fileInfo) < 0) // we run through the folders, in case of any error the only error can be is intrenal
	{
		*errType = 500;
		closedir(folder);
		free(htmlCode);
		return -1;
	}
	
	// we build first header and than check the last modified time of the folder
	sprintf(headers,"%s %s\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: text/html\r\n",protocol, Error2String(200),time);
	strftime(folderLastTime, sizeof(folderLastTime), RFC1123FMT, gmtime(&fileInfo.st_mtime));

	//after headers we build the html message and the table
	sprintf(htmlCode,"<HTML>\r\n<HEAD><TITLE> Index of %s</TITLE></HEAD>\r\n<BODY>\r\n<H4>Index of %s</H4>\r\n",path + 1, path + 1);
	sprintf(htmlCode + strlen(htmlCode),"<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n");
	
	//check all files in folder
	currFileEntity = readdir(folder);
	while (currFileEntity) 
	{
		if (!strcmp (currFileEntity->d_name, ".")) //if we mention it it is DOUBLE our dir
		{
			currFileEntity = readdir(folder);
			continue;
		}
		
		strcat(path, currFileEntity->d_name); // cut the current file
		if (lstat(path, &fileInfo) < 0) // check file permissions
		{
			*errType = 403;
			return -1;
		}
		//set last time on the current file 
		strftime(fileLastTime, sizeof(fileLastTime), RFC1123FMT, gmtime(&fileInfo.st_mtime));
		
		//write the html code with the correct values
		sprintf(htmlCode + strlen(htmlCode),"<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td><td>",currFileEntity->d_name,currFileEntity->d_name, fileLastTime);
		
		//check if its a file and not folder and add the finish of html code
		if(S_ISREG(fileInfo.st_mode))sprintf(htmlCode + strlen(htmlCode), "%lu", fileInfo.st_size);
		sprintf(htmlCode + strlen(htmlCode), "</td></tr>\r\n");

		//skip the current file and go to the next	
		path[strlen(path) - strlen(currFileEntity->d_name)] = '\0';
		currFileEntity = readdir(folder);
	}	
	
	//add more information to html and to the headers
	sprintf(htmlCode + strlen(htmlCode),"</table>\r\n<HR>\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n</HR>\r\n</BODY></HTML>\r\n\r\n");
	sprintf(headers + strlen(headers),"Content-Length: %lu\r\nLast-Modified: %s\r\nConnection: close\r\n\r\n",strlen(htmlCode), folderLastTime);
	
	//calloc the response message and write the headers and html code into the response
	char *response = (char*)calloc(strlen(htmlCode) + (512), sizeof(char));
	sprintf(response, "%s%s", headers, htmlCode);
	
	//write to socket the response message
	if (writeToSoc(socketFd, response, strlen(response)) == -1)
	{
		*errType = 0;
		closedir(folder);
		free(htmlCode);
		free(response);
		return -1; 
	}

	// close connections and free callocs
	free(htmlCode);
	free(response);
	closedir(folder);
	return 0;
}

//write to socket help function
int writeToSoc(int socketFd, char *sendMsg, int sizeToWrite)
{
	int alreadyWritten = 1;//init data
	char *toSend = sendMsg; // message need to be sent

	while (sizeToWrite > 0)//while we got still to write
	{
		alreadyWritten = write(socketFd, toSend, sizeToWrite);
		if (alreadyWritten < 0) //in case nothing to read anymore
			return -1;
		sizeToWrite -= alreadyWritten;//reduce how much still to write
		toSend += alreadyWritten;//add how much we wrote
	}
	return 0;
}

//here we get the error number that we got from the functions and translate it into the correct string
char *Error2String(int errType)
{
	if (errType == 200) 
		return "200 OK";
	else if (errType == 302)
		return "302 Found";
	else if (errType == 400)
		return "400 Bad Request";
	else if (errType == 403)
		return "403 Forbidden";
	else if (errType == 404)
		return "404 Not Found";
	else if (errType == 500)
		return "500 Internal Server Error";
	else if	(errType == 501)
		return "501 Not supported";
	return "ERROR NO CODE FOUND";
}

int main(int argc, char *argv[])
{
	//variables
	int socketFd;
	int newSocket ;
	int *clientSocket;
	int counter = 0;
	struct sockaddr_in serverAdress;
	struct sockaddr_in clientAdress;
	socklen_t socklen = sizeof(struct sockaddr_in);
	int port;
	int poolSize;
	int maxRequests;

	if (argc != 4) // check that there is correct amount of digis
	{
		printf("Usage: server <port> <pool-size> <max-requests-number>\n");
		exit(EXIT_FAILURE);
	}
	
	if(checkDigits(argv[1]) == -1 || checkDigits(argv[2]) == -1 || checkDigits(argv[3]) == -1) // check if all the input are digits
	{
		perror("Usage: server <port> <pool-size> <max-requests-number>\n");
		exit(EXIT_FAILURE);
	}

	//parse header
	port = atoi(argv[1]);
	poolSize = atoi(argv[2]);
	maxRequests = atoi(argv[3]);

	// check for valid input in argv
	if(port < 0)
	{
		perror("Usage: server <port> <pool-size> <max-requests-number>\n");
		exit(EXIT_FAILURE);
	}
	else if( poolSize < 1 || poolSize > MAXT_IN_POOL)
	{
		perror("Usage: server <port> <pool-size> <max-requests-number>\n");
		exit(EXIT_FAILURE);
	}
	else if(maxRequests < 1)
	{
		perror("Usage: server <port> <pool-size> <max-requests-number>\n");
		exit(EXIT_FAILURE);
	}


	//create server TCP connection
	socketFd = socket(AF_INET,SOCK_STREAM,0);
	if(socketFd < 0)
	{
		perror("RROR on socketFd \n");
		exit(EXIT_FAILURE);
	}

	serverAdress.sin_family = AF_INET;
	serverAdress.sin_addr.s_addr = INADDR_ANY;
	serverAdress.sin_port = htons(port);

	if(bind(socketFd,(struct sockaddr *)&serverAdress,sizeof(serverAdress) ) < 0)
	{
		perror("FAILURE BINIDNG SOCKETFD ()\n");
		exit(EXIT_FAILURE);
	}

	// listen to soc fd ad create Threadpool
	listen(socketFd,5);
	threadpool *Tpool = create_threadpool(poolSize);
	
	//accept incoming requests
	while(counter < maxRequests)
	{
		newSocket = accept(socketFd, (struct sockaddr*)&clientAdress, &socklen);// create new soc
		clientSocket = (int*)calloc(1, sizeof(int));//the client soc
		*clientSocket = newSocket;
		dispatch(Tpool, dispatchFn, (void*)clientSocket);//sent to the func of Tpool
		counter++;
	}
	
	//close connection to server
	close(socketFd);
	destroy_threadpool(Tpool);
	return 0; 
}