/***************************************************************
   Author - Virag Gada
   File - server.c
   Description - Source file for web server
 ****************************************************************/

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <strings.h>
#include <memory.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>

#define BACKLOG (10)
#define MAX_CONNECTIONS (10)
#define CONFIG_FILE ("dfs.conf")
#define DELIMITER (" ")
#define COMMAND_SIZE (256)

/* Array's to store a variety of information */
char listenPort[5];
char fileSystemLocation[50]={(int)'\0'};
char defaultFiles[3][20]={(int)'\0'};
char contentExtension[50][10]={(int)'\0'};
char contentType[50][20]={(int)'\0'};
char errorTypes[6][50]={(int)'\0'};
//int keepAliveTime = 0;
//int doPersistent = 0;

char *command_list[4] = {"get","put","list","mkdir"};
int command_num = 0;

//char *token[2];

char valid_users[5][10]={(int)'\0'};
char valid_password[5][10]={(int)'\0'};

sig_atomic_t quit_request = 0;
uint16_t active_count = 0;
/* Use for receive timeout */
struct timeval tv={0,0};
char serverDirectory[50] = {(int)'\0'};
/* Array of child threads and function descriptiors */
pthread_t child_threads[MAX_CONNECTIONS];
int client_fd[MAX_CONNECTIONS];

/* Variable for our socket fd*/
int sockfd;

/* Function prototypes */
int readConfig(void);
void *respondClient(void * num);
void closingHandler(int mySignal);
int checkAndCreateDirectory(char * path);

//int delete(char * file);
int main(int argc, char *argv[])
{
  /*Variables*/
  long availableSlot = 0;
  struct sockaddr_in *selfAddr;
  struct sockaddr_in fromAddr;
  socklen_t fromAddrSize;
  //char buffer[MAX_BUFF_SIZE];

  pthread_attr_t attr;

  if (argc != 3) {
    printf ("USAGE: ./server </DFS#> <port no>\n");
    exit(1);
  }

  char serverPort[5] = {(int)'\0'};
  strcpy(serverPort,argv[2]);
  strcpy(serverDirectory,argv[1]);

  int status = readConfig();

  if(status == 0)
    exit(1);

  status = checkAndCreateDirectory(serverDirectory);
  if(status == 0)
    exit(1);

  selfAddr = (struct sockaddr_in *)calloc(1,sizeof(struct sockaddr_in));

  (*selfAddr).sin_family = AF_INET;           //address family
  (*selfAddr).sin_port = htons(atoi(serverPort)); //sets port to network byte order
  (*selfAddr).sin_addr.s_addr = htonl(INADDR_ANY); //sets local address

  /*Create Socket*/
  if((sockfd = socket((*selfAddr).sin_family,SOCK_STREAM,0))< 0) {
    printf("Unable to create socket\n");
    exit(1);
  }
  //printf("Socket Created\n");

  /*Call Bind*/
  if(bind(sockfd,(struct sockaddr *)selfAddr,sizeof(*selfAddr))<0) {
    printf("Unable to bind\n");
    exit(1);
  }
  //printf("Socket Binded\n");

  /* Listen for incomming connections */
  if(listen(sockfd,BACKLOG)!=0){
    printf("%s\n","Listen Error");
    exit(1);
  }

  /* Add a signal handler for our SIGINT signal ( Ctrl + C )*/
  signal(SIGINT,closingHandler);

  memset(&fromAddr,0,sizeof(fromAddr));
  //memset(buffer,0,sizeof(buffer));
  fromAddrSize = sizeof(fromAddr);
  memset(client_fd,-1,sizeof(client_fd));
  pthread_attr_init(&attr);
  //pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  /*listen*/
  while(1) {
    printf("waiting for connections..\n");
    /*Accept*/
    if((client_fd[availableSlot] = accept(sockfd,(struct sockaddr *)&fromAddr,&fromAddrSize)) < 0)
    {
      printf("Failed to accept connection\n");
      break;
    }else{
      /* Create new thread to handle the client */
      pthread_create(&child_threads[availableSlot],&attr,respondClient,(void *)availableSlot);
    }
    /* Add client fd to a position in the array which is empty */
    while (client_fd[availableSlot]!=-1) {
      availableSlot = (availableSlot+1)%MAX_CONNECTIONS;
    }
    printf("Slot number - %ld\n", availableSlot);
  }

  /*Close*/
  close(sockfd);
  return 0;
}

/* Handler function for SIGINT (Ctrl+C) signal*/
void closingHandler(int mySignal){
  /* Set the global quit flag */
  quit_request = 1;
  int i = 0;

  /* Wait for all active threads to close */
  while(active_count>0){
    for(i = 0; i<MAX_CONNECTIONS ; i++){
      /* If thread is running wait to exit and join */
      if(client_fd[i]!=-1){
        pthread_join(child_threads[i],NULL);
      }
    }
  }
  /* Close the server sock fd and exit */
  close(sockfd);
  exit(0);
}

/* Thread function to respond to client request */
void *respondClient(void * num){

  int client_no = (int )(long) num;
  //write(client_fd[client_no],"1",1);
  //printf("Num - %d\n",client_no);
  FILE *fptr;
  char *dataBuffer = NULL;
  long int size_to_send;
  char clientCommand[COMMAND_SIZE] = {(int)'\0'};
  struct stat file_info;
  char nameOfPiece[20] = {(int)'\0'};
  char nameOfPath[30] = {(int)'\0'};
  int status=0;
  long int file_size;
  long int response[1] = {(int)'\0'};
  int i;

  /* Structure request */
  struct {
    char user[10];
    char pass[10];
    char cmd[5];
    char file[20];
    char fileSize[11];
    char piece[2];
    char subfolder[10];
  }clientRequest;

  int val;
  void * check;

  //tv.tv_sec = keepAliveTime;
  active_count++;
  do{
    status = 0;
    printf("%s\n","Waiting for new request");
    /* Monitor the file descriptor */

    printf("Child Number : %d\n", client_no);
    memset(&clientRequest,(int)'\0',sizeof(clientRequest));
    memset(clientCommand,(int)'\0',COMMAND_SIZE);
    /* If any action on given file descriptor then proceed */
    if((val = recv(client_fd[client_no],clientCommand,COMMAND_SIZE,0))<=0){
      /* Exit on error */
      printf("Exiting Child Number : %d\n", client_no);
      break;
    }else if(strlen(clientCommand) == 0){
      /* Handling NULL data from client */
      printf("%s\n","NULL data");
    }else {
      printf("\n%s\n","Received from client:");
      printf("%s\n",clientCommand);
      memset(&clientRequest,(int)'\0',sizeof(clientRequest));
      /*Seperate the user, password, command, filename, piece, file size and subfolder*/
      strcpy(clientRequest.user,strtok(clientCommand," "));
      strcpy(clientRequest.pass,strtok(NULL," "));
      strcpy(clientRequest.cmd,strtok(NULL," "));

      //printf("Size of struct %ld\n",sizeof(clientRequest));
      for(i=0;i<2;i++){
        if((strcmp(clientRequest.user,valid_users[i])==0)&&(strcmp(clientRequest.pass,valid_password[i])==0)) {
          status = 1;
          break;
        }
      }
      if(status == 0){
        write(client_fd[client_no],&status,1);
        printf("%s\n","User not found");
      }else{
        printf("%s\n","Valid user");
        /* Perform actions based on commands */
        for(int i = 0; i<4; i++) {
          if(strcmp(command_list[i],clientRequest.cmd) == 0){
            command_num = i+1;
            break;
          }
        }
            // Perform action based on input from user
        switch (command_num) {
        case 1: /* Get command from client */
              response[0] = status;
              write(client_fd[client_no],response,1);
              strcpy(clientRequest.file,strtok(NULL," "));
              //strcpy(clientRequest.subfolder,strtok(NULL," "));
              check = strtok(NULL," ");
              if(check != NULL){
                strcpy(clientRequest.subfolder,check);
                //printf("%s\n", clientRequest.subfolder);
              }

              if(check != NULL){
                sprintf(nameOfPath,"%s/%s/%s/%s",serverDirectory,clientRequest.user,clientRequest.subfolder,clientRequest.file);
              }else{
                sprintf(nameOfPath,"%s/%s/%s",serverDirectory,clientRequest.user,clientRequest.file);
              }
              //printf("%s\n",nameOfPath);
              if((fptr = fopen (nameOfPath,"r")) == NULL){
                printf("%s\n","Error reading");
                response[0] = 0;
                write(client_fd[client_no],response,1);
              }else{
                /* Get the file size */
                fstat(fileno(fptr),&file_info);

                file_size = file_info.st_size;
                response[0] = file_size;
                //printf("Size of file %ld\n", response[0]);
                /* Send acknowledgement and file size to client */
                val = write(client_fd[client_no],response,8);
                //printf("Size sent %d\n",val);
                /* Store it in a buffer of the same size */
                dataBuffer = (char *)calloc(1,file_size);
                if(dataBuffer == NULL){
                  printf("%s\n","Error creating buffer");
                  write(client_fd[client_no],"0",1);
                }else{
                  fread(dataBuffer,file_size,1,fptr);

                  if((val = write(client_fd[client_no],dataBuffer,file_size))<0){
                    printf("%s\n","Error sending file");
                  }
                  free(dataBuffer);
                }
                fclose(fptr);
              }
              break;

        case 2: /* Put command from client */
              write(client_fd[client_no],&status,1);
              strcpy(clientRequest.file,strtok(NULL," "));
              strcpy(clientRequest.fileSize,strtok(NULL," "));
              strcpy(clientRequest.piece,strtok(NULL," "));
              strcpy(clientRequest.subfolder,strtok(NULL," "));
              //printf("Subfolder - %s\n",clientRequest.subfolder);

              sprintf(nameOfPath,"%s/%s",serverDirectory,clientRequest.user);
              printf("%s\n",nameOfPath);
              status = checkAndCreateDirectory(nameOfPath);
              if(strcmp(clientRequest.subfolder,"(null)")!=0){
                sprintf(nameOfPath,"%s/%s/%s",serverDirectory,clientRequest.user,clientRequest.subfolder);
                printf("%s\n",nameOfPath);
                status = checkAndCreateDirectory(nameOfPath);
              }
              //printf("Directory status %d\n",status);
              dataBuffer = (char *)calloc(1,atoi(clientRequest.fileSize));
              memset(dataBuffer,(int)'\0',atoi(clientRequest.fileSize));
              printf("%s\n","Waiting for data from client");
              if((val = recv(client_fd[client_no],dataBuffer,atoi(clientRequest.fileSize),0))<=0){
                printf("Value is %d\n", val);
                /* Exit on error */
                printf("Exiting Child Number : %d\n", client_no);
                printf("Error on put recv %s\n", strerror(errno));
              }else{
                printf("%s\n","Received data from client");
                sprintf(nameOfPiece,"%s/.%s.%s",nameOfPath,clientRequest.file,clientRequest.piece);
                printf("%s\n",nameOfPiece);
                fptr = fopen(nameOfPiece,"w+");
                if(fptr == NULL){
                  printf("%s %s\n","Error creating file",nameOfPiece);
                  break;
                }
                //printf("size - %d dataBuffer %s\n",atoi(clientRequest.fileSize),dataBuffer);
                if((val = fwrite(dataBuffer,atoi(clientRequest.fileSize),1,fptr)<0)) {
                  printf("%s\n","Error writing to file");
                  fclose(fptr);
                  break;
                }
                fclose(fptr);
              }
              free(dataBuffer);

              memset(nameOfPiece,(int)'\0',strlen(nameOfPiece));
              memset(nameOfPath,(int)'\0',strlen(nameOfPath));
              break;

        case 3: /* List command from client */
              write(client_fd[client_no],&status,1);
              check = strtok(NULL," ");
              if(check != NULL){
                strcpy(clientRequest.subfolder,check);
                printf("%s\n", clientRequest.subfolder);
              }
              //printf("Folder %s\n",clientRequest.subfolder);
              printf("Sending list of files in this directory\n");
              DIR *d;
              struct dirent *dir;
              dataBuffer = (char *)calloc(1,512);
              memset(dataBuffer,(int)'\0',strlen(dataBuffer));
              /* Get files in the user directory */
              sprintf(nameOfPath,"%s/%s",serverDirectory,clientRequest.user);
              if(check != NULL){
                sprintf(nameOfPath,"%s/%s/%s",serverDirectory,clientRequest.user,clientRequest.subfolder);
              }
              printf("%s\n",nameOfPath);
              d = opendir(nameOfPath);
              if (d)
              {
                while ((dir = readdir(d)) != NULL)
                {
                  if(dir->d_type == DT_REG){
                    if((strcmp(dir->d_name,".")!=0)&&(strcmp(dir->d_name,".."))){
                      strcat(dataBuffer,dir->d_name);
                      strcat(dataBuffer,"\n");
                    }
                  }
                }
                closedir(d);
              }
              /* Now reply the client with the same data */
              size_to_send = strlen(dataBuffer);
              //printf("List - %s\n",dataBuffer);

              if ((val = write(client_fd[client_no],dataBuffer,size_to_send)) < 0) {
                printf("List send failed\n");
              }
              printf("Size sent %d\n",val);
              free(dataBuffer);
              break;

        case 4:
              write(client_fd[client_no],&status,1);
              printf("%s\n","Valid user");

              check = strtok(NULL," ");
              if(check != NULL){
                strcpy(clientRequest.subfolder,check);
                printf("%s\n", clientRequest.subfolder);
              }else{
                /* No subdirectory specified */
                write(client_fd[client_no],"0",1);
                break;
              }

              sprintf(nameOfPath,"%s/%s",serverDirectory,clientRequest.user);
              printf("%s\n",nameOfPath);
              status = checkAndCreateDirectory(nameOfPath);
              if(check != NULL){
                sprintf(nameOfPath,"%s/%s/%s",serverDirectory,clientRequest.user,clientRequest.subfolder);
                printf("%s\n",nameOfPath);
                status = checkAndCreateDirectory(nameOfPath);
              }
              write(client_fd[client_no],&status,1);
              break;
        default: /* If wrong command sent */
              printf("%s\n","Wrong command entered");
              break;

        }

      }
    memset(&clientRequest,(int)'\0',sizeof(clientRequest));
    memset(clientCommand,(int)'\0',COMMAND_SIZE);
    }
      /* Loop if pipelining is required */
  }while(1);
  active_count--;
  printf("%s\n","Disconnected");
  /* Close the thread socket */
  close(client_fd[client_no]);

  /* Set its value to -1 to use this position for some other thread */
  client_fd[client_no]=-1;
  pthread_exit(NULL);
}


/* Function to read the server configuration file */
int readConfig(void){
  FILE *cptr;
  char *src;
  char configBuffer[512] = {(int)'\0'};
  int i;
  struct stat file_info;
  /* Read the config file */
  if((cptr=fopen(CONFIG_FILE,"r"))==NULL)
  {
    return 0;
  }
  else{
    /* Get the file information */
    fstat(fileno(cptr),&file_info);
    /* Read the file size */
    long int file_size=file_info.st_size;
    if(fread(configBuffer,file_size,1,cptr)<0){
      fclose(cptr);
      return 0;
    }
    src = strstr(configBuffer,"#Users");
    src = src + strlen("#Users")+1;
    i=0;
    while(sscanf(src,"%s %s",valid_users[i],valid_password[i])==2){
      src = src + strlen(valid_users[i])+strlen(valid_password[i])+2;
      i++;
      if(i==2){
        break;
      }
    }
  }
  /*for(i=0;i<2;i++){
    printf("User - %s, Pass %s\n",valid_users[i],valid_password[i]);
  }*/

  fclose(cptr);
  return 1;
}

/* Function to create directory */
int checkAndCreateDirectory(char * path){
  struct stat sd;
  int ret = stat(path,&sd);

  if(ret == 0){
    if(sd.st_mode & S_IFDIR){
      printf("%s directory is present\n",path);
    }
    return 1;
  }else{
    if(errno == ENOENT){
      printf("%s\n","Directory does not exist. Creating new directory...");
      ret = mkdir(path,S_IRWXU);
      if(ret != 0){
        printf("%s\n","mkdir failed");
        perror("Failed due to ");
      }
      else {
        printf("Created directory - %s\n",path);
      }
      return 1;
    }
  }
  return 0;
}
