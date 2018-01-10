/***************************************************************
   Author - Virag Gada
   File - client.c
   Description - Source file for client
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
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <strings.h>
#include <openssl/md5.h>

/* Global Definitions */
#define NUM_OF_SERVERS (4)
int socket_fd[4];
/* Global arrays storing our mapped words and their count */

char temp[1]= {0};
uint32_t temp_count;
/* Store config file values into these arrays */
char userName[10] = {(int)'\0'};
char passWord[20] = {(int)'\0'};
char *serverNames[NUM_OF_SERVERS];
char serverIPs[NUM_OF_SERVERS][10]={(int)'\0'};
char serverPort[NUM_OF_SERVERS][6]={(int)'\0'};
struct sockaddr_in remoteAddr[4];
/* Struct to store the info for our file pieces */
struct {
  char fName[20];
  int piece[4];
}listInfo;

int filePiece[4][2] = {(int)'\0'};

/* Prompt for user on using the commands */
void promptUser(void){
  printf("\nCommands available to use:\n");
  printf("    get [file_name]\n");
  printf("    put [file_name]\n");
  printf("    list\n");
  printf("    mkdir\n");
}

char *command_list[4]= {"get","put","list","mkdir"};
int command_num = 0;

int readConfig(char * configName);
int setupConnectionToServer();
int createClientSocket();
int sendToServer(int sockfd, char * dataToSend, size_t len);
int calculateMD5(const char * buffer,long int fileSize, uint8_t *lastByte);
int calculateEncryptKey(char * param1, char * param2);

int main(int argc, char **argv) {
  /*Variables*/

  struct sockaddr_in fromAddr;
  char command[50];
  int byte_count;
  char *token[3];
  char *buffer = NULL;
  char *holder = NULL;
  char *piece[4] = {NULL};
  char header[256] = {(int)'\0'};
  int i,j,k;;
  struct stat file_info;
  FILE *fptr;
  size_t fileSize = 0;
  uint8_t md5LastByte;
  int encryptByte;
  int val;
  char nameOfPiece[20] = {(int)'\0'};
  int sizeForEachPiece[4];
  char storage;
  long int response[1] = {(int)'\0'};
  char *linePtr;
  if (argc < 2) {
    printf("USAGE: ./client dfc.conf\n");
    exit(1);
  }

  int status = readConfig(argv[1]);
  printf("Status - %d\n", status);
  if(status == 0)
    exit(0);

  //status = calculateMD5("Makefile",&md5LastByte);
  //if(status == 0)
  //  exit(0);
  /*Create Socket*/
  status = createClientSocket();
  if(status == 0){
    exit(0);
  }

  status = setupConnectionToServer();
  if(status == 0){
  printf("%s\n","All servers are not availablel");
  //  exit(0);
  }
  bzero(&fromAddr, sizeof(fromAddr));
  /* Ignore signal generatred due to failure of send */
  signal(SIGPIPE, SIG_IGN);

  while(1) {
    /* Prompt the user everytime on how to send the command */
    promptUser();
    memset(command,(int)'\0',strlen(command));
    fgets(command,50,stdin);
    status = setupConnectionToServer();
    if(status == 0){
    printf("%s\n","Retrying connection at each command");
    //  exit(0);
    }
    /* Add NULL terminator to user input*/
    if ((strlen(command)>0) && (command[strlen (command) - 1] == '\n'))
      command[strlen (command) - 1] = '\0';

    printf("Input from user %s\n",command);
    /* Seperate values from buffer with delimiter " "(space) */
    token[0] = strtok(command," ");
    // Get the file name
    token[1] = strtok(NULL," ");
    // Get the subfolder
    token[2] = strtok(NULL," ");
    /* Perform actions based on commands */
    for(int i = 0; i<4; i++) {
      if(strcmp(command_list[i],token[0]) == 0) {
        command_num = i+1;
        break;
      }
    }

    // Perform action based on input from user
    switch (command_num) {
    case 1: /* Get command from client */
            memset(&listInfo,(int)'\0',sizeof(listInfo));
            strcpy(listInfo.fName,token[1]);
            /* First get a list from the server */
            memset(header,(int)'\0',strlen(header));
            //token[1] = strtok(NULL," ");
            if(token[2] != NULL){
              sprintf(header,"%s %s %s %s",userName,passWord,"list",token[2]);
            }else{
              sprintf(header,"%s %s %s",userName,passWord,"list");
            }
            printf("Header- %s\n", header);
            //buffer = (char *)calloc(1,1024);
            for(i =0;i<NUM_OF_SERVERS;i++){
              status = sendToServer(socket_fd[i],header,strlen(header));
              //printf("%s\n","Sent header to server");
              if(status == 0){
                printf("Server %d not active\n",i+1);
              }else{
                if((val = recv(socket_fd[i],response,1,0))<=0){
                  printf("%s\n","Error receiving");
                }
                if(response[0]==0){
                  printf("%s\n","Invalid Username/Password. Please try again.");
                  //break;
                }else{
                  //printf("%s\n","User authenticated");

                  piece[i] = (char *)calloc(1,512);
                  memset(piece[i],(int)'\0',512);
                  if((val = recv(socket_fd[i],piece[i],512,0))<=0){
                    printf("%s\n","Error receiving");
                  }
                  else{
                    //printf("%s\n",piece[i]);
                    //strcat(buffer,piece[i]);
                  }
                }
              }
              listInfo.piece[i] = 99;
            }

            /* Find which piece is available on which server and store in the structure */
            for(i=0;i<NUM_OF_SERVERS;i++){
              if(piece[i]!=NULL){
                linePtr = piece[i];
                while((linePtr=strstr(linePtr,listInfo.fName))!=NULL){
                  linePtr = linePtr + strlen(listInfo.fName)+1;
                  storage = *linePtr;
                  if(listInfo.piece[atoi(&storage)-1] == 99){
                    listInfo.piece[atoi(&storage)-1] = i;
                    //printf("Pair - %c\n", storage);
                  }
                }
              }
            }

            if((listInfo.piece[0]==99)||(listInfo.piece[1]==99)||(listInfo.piece[2]==99)||(listInfo.piece[3]==99)){
                printf("%s\n","File cannot be completed");
            }else{
              /* Get files from specific servers */
              for(i=0;i<NUM_OF_SERVERS;i++){
                printf("Piece %d from server %d\n",i+1,listInfo.piece[i]+1);
                sprintf(nameOfPiece,".%s.%d",listInfo.fName,i+1);
                memset(header,(int)'\0',strlen(header));
                if(token[2] != NULL){
                  sprintf(header,"%s %s %s %s %s",userName,passWord,token[0],nameOfPiece,token[2]);
                }else{
                  sprintf(header,"%s %s %s %s",userName,passWord,token[0],nameOfPiece);
                }
                printf("%s\n", header);
                status = sendToServer(socket_fd[listInfo.piece[i]],header,strlen(header));
                if(status == 0){
                  break;
                }
                if((val = recv(socket_fd[listInfo.piece[i]],response,1,0))<=0){
                  printf("%s\n","Error receiving");
                }
                if(response[0]==0){
                  printf("%s\n","Invalid Username/Password. Please try again.");
                  break;
                }
                //printf("%s\n","User authenticated");
                if((val = recv(socket_fd[listInfo.piece[i]],response,8,0))<=0){
                  printf("%s\n","Error receiving");
                  break;
                }
                if(response[0]==0){
                  printf("%s\n","File not found");
                }else{
                  printf("Size - %ld\n", response[0]);
                  piece[i] = (char *)calloc(1,response[0]);
                  sizeForEachPiece[i] = response[0];
                  if((val = recv(socket_fd[listInfo.piece[i]],piece[i],response[0],0))<0){
                    printf("%s\n","Error receiving");
                  }
                  //printf("Size received - %ld\n",strlen(piece[i]));
                  //printf("Size read %d\n",val);
                }
              }
              /* Find the encryption byte */
              encryptByte = calculateEncryptKey(userName,passWord);
              //printf("Decrypt byte %x\n", encryptByte);
              holder = (char *)calloc(1,response[0]);
              sprintf(nameOfPiece,"%s/%s","path",listInfo.fName);
              fptr = fopen(nameOfPiece,"w+");

              for(i=0;i<NUM_OF_SERVERS;i++){
                //printf("sizeForEachPiece %d\n",sizeForEachPiece[i]);
                for(j=0;j<sizeForEachPiece[i];j++){
                  holder[j]=piece[i][j]^encryptByte;
                }

                val = fwrite(holder,sizeForEachPiece[i],1,fptr);
                memset(holder,(int)'\0',response[0]);
                free(piece[i]);
                piece[i]=NULL;
              }
              printf("%s\n","File received");
              free(holder);
              fclose(fptr);
            }
            break;

    case 2: /* Put command from client */
          if((fptr = fopen (token[1],"r")) == NULL){
            printf("%s\n","Error reading");
          }else{
            /* Get the file size */
            fstat(fileno(fptr),&file_info);
            fileSize = file_info.st_size;

            /* Store it in a buffer of the same size */
            buffer = (char *)calloc(1,fileSize);
            if(buffer == NULL){
              printf("%s\n","Error creating buffer");
            }
            fread(buffer,fileSize,1,fptr);
            //fclose(fptr);
            /* Calculate the MD5 sum and  get the last byte*/
            status = calculateMD5(buffer,fileSize,&md5LastByte);
            if(status == 0){
              printf("%s\n","Error calculating MD5");
              break;
            }
            free(buffer);
            /* Distribute the file pieces based on modulo division with 4 */
            if(md5LastByte%NUM_OF_SERVERS == 0){
              filePiece[0][0] = 1; filePiece[0][1] = 2;
              filePiece[1][0] = 2; filePiece[1][1] = 3;
              filePiece[2][0] = 3; filePiece[2][1] = 4;
              filePiece[3][0] = 4; filePiece[3][1] = 1;
            }else if(md5LastByte%NUM_OF_SERVERS == 1){
              filePiece[0][0] = 4; filePiece[0][1] = 1;
              filePiece[1][0] = 1; filePiece[1][1] = 2;
              filePiece[2][0] = 2; filePiece[2][1] = 3;
              filePiece[3][0] = 3; filePiece[3][1] = 4;
            }else if(md5LastByte%NUM_OF_SERVERS == 2){
              filePiece[0][0] = 3; filePiece[0][1] = 4;
              filePiece[1][0] = 4; filePiece[1][1] = 1;
              filePiece[2][0] = 1; filePiece[2][1] = 2;
              filePiece[3][0] = 3; filePiece[3][1] = 4;
            }else if(md5LastByte%NUM_OF_SERVERS == 3){
              filePiece[0][0] = 2; filePiece[0][1] = 3;
              filePiece[1][0] = 3; filePiece[1][1] = 4;
              filePiece[2][0] = 4; filePiece[2][1] = 1;
              filePiece[3][0] = 1; filePiece[3][1] = 2;
            }

            /* Find the encryption byte */
            encryptByte = calculateEncryptKey(userName,passWord);
            //printf("Encrypt byte %x\n", encryptByte);

            fseek(fptr, 0, SEEK_SET);
            piece[0] = (char *)calloc(1,fileSize/NUM_OF_SERVERS+1);
            memset(piece[0],(int)'\0',fileSize/NUM_OF_SERVERS+1);
            sizeForEachPiece[0]=fileSize/NUM_OF_SERVERS;

            piece[1] = (char *)calloc(1,fileSize/NUM_OF_SERVERS+1);
            memset(piece[1],(int)'\0',fileSize/NUM_OF_SERVERS+1);
            sizeForEachPiece[1]=fileSize/NUM_OF_SERVERS;

            piece[2] = (char *)calloc(1,fileSize/NUM_OF_SERVERS+1);
            memset(piece[2],(int)'\0',fileSize/NUM_OF_SERVERS+1);
            sizeForEachPiece[2]=fileSize/NUM_OF_SERVERS;

            piece[3] = (char *)calloc(1,(fileSize/NUM_OF_SERVERS)+(fileSize%NUM_OF_SERVERS)+1);
            memset(piece[3],(int)'\0',(fileSize/NUM_OF_SERVERS)+(fileSize%NUM_OF_SERVERS)+1);
            sizeForEachPiece[3]=(fileSize/NUM_OF_SERVERS)+(fileSize%NUM_OF_SERVERS);

            /* Have a buffer to store the encrypted piece before sending */
            holder = (char *)calloc(1,(fileSize/NUM_OF_SERVERS)+(fileSize%NUM_OF_SERVERS)+1);
            memset(holder,(int)'\0',(fileSize/NUM_OF_SERVERS)+(fileSize%NUM_OF_SERVERS)+1);
            printf("%s\n","Buffers created");
            for(i=0;i<NUM_OF_SERVERS;i++){
              if(piece[i]==NULL){
                printf("%s %d\n","Buffer not created", i+1);
              }
            }
            if(holder==NULL){
              printf("%s\n","Holder not created");
            }

            if((byte_count=fread(piece[0],1,fileSize/NUM_OF_SERVERS,fptr))<=0){
              printf("%s\n","Unable to copy piece 1");
              break;
            }

            //printf("%s %d %ld\n", "read once",byte_count,strlen(piece[0]));
            if((byte_count=fread(piece[1],1,fileSize/NUM_OF_SERVERS,fptr))<=0){
              printf("%s\n","Unable to copy piece 2");
              break;
            }

            //printf("%s %d %ld\n", "read twice",byte_count,strlen(piece[1]));

            if((byte_count=fread(piece[2],1,fileSize/NUM_OF_SERVERS,fptr))<=0){
              printf("%s\n","Unable to copy piece 3");
              break;
            }

            //printf("%s %d %ld\n","read thrice",byte_count,strlen(piece[2]));
            if((byte_count=fread(piece[3],1,(fileSize/NUM_OF_SERVERS)+(fileSize%NUM_OF_SERVERS),fptr))<=0){
              printf("%s\n","Unable to copy piece 4");
              break;
            }
            fclose(fptr);
            //printf("%s %d %ld\n","read four times",byte_count,strlen(piece[3]));

            printf("%s\n","Sending files to server");
            /* i counter for server */
            for(i=0; i<NUM_OF_SERVERS ; i++){
              /* j counter for the number of file pieces to be sent*/
              for(j=0; j<2 ; j++){
                /* Create header */
                memset(header,(int)'\0',strlen(header));
                sprintf(header,"%s %s %s %s %d %d %s",userName,passWord,token[0],token[1],sizeForEachPiece[filePiece[i][j]-1],filePiece[i][j],token[2]);
                printf("%s\n",header);
                status = sendToServer(socket_fd[i],header,strlen(header));
                //printf("%s\n","Sent header to server");
                if(status == 0){
                  break;
                }
                if((val = recv(socket_fd[i],response,1,0))<=0){
                  printf("%s\n","Error receiving");
                }
                if(response[0]==0){
                  printf("%s\n","Invalid Username/Password. Please try again.");
                  break;
                }
                printf("%s\n","User authenticated");
                printf("Sending piece %d to server %d\n",filePiece[i][j],i+1);
                for(k=0;k<sizeForEachPiece[filePiece[i][j]-1];k++){
                  holder[k] = piece[filePiece[i][j]-1][k] ^ encryptByte;
                }

                status = sendToServer(socket_fd[i],holder,sizeForEachPiece[filePiece[i][j]-1]);
                if(status == 0){
                  printf("%s %d\n","Error sending piece to server",i+1);
                  break;
                }
                memset(holder,(int)'\0',(fileSize/NUM_OF_SERVERS)+(fileSize%NUM_OF_SERVERS)+1);
              }
              if(j == 0){
                printf("%s %d\n","Server not available",i+1);
              }
            }
            for(i=0;i<NUM_OF_SERVERS;i++){
              free(piece[i]);
              piece[i]=NULL;
            }
            free(holder);
          }
          memset(filePiece,(int)'\0',sizeof(filePiece));
          memset(header,(int)'\0',strlen(header));

          break;

    case 3: /* List command from client */
          memset(header,(int)'\0',strlen(header));
          //token[1] = strtok(NULL," ");
          if(token[1] != NULL){
            sprintf(header,"%s %s %s %s",userName,passWord,token[0],token[1]);
          }else{
            sprintf(header,"%s %s %s",userName,passWord,token[0]);
          }

          printf("Header- %s\n", header);
          buffer = (char *)calloc(1,1024);
          for(i =0;i<NUM_OF_SERVERS;i++){
            status = sendToServer(socket_fd[i],header,strlen(header));
            //printf("%s\n","Sent header to server");
            if(status == 0){
              printf("Server %d not active\n",i+1);
            }else{
              if((val = recv(socket_fd[i],response,1,0))<=0){
                printf("%s\n","Error receiving");
              }
              if(response[0]==0){
                printf("%s\n","Invalid Username/Password. Please try again.");
                //break;
              }else{
                printf("%s\n","User authenticated");

                piece[i] = (char *)calloc(1,512);
                memset(piece[i],(int)'\0',512);
                if((val = recv(socket_fd[i],piece[i],512,0))<=0){
                  printf("%s\n","Error receiving");
                }
                else{
                  //printf("%s\n",piece[i]);
                  strcat(buffer,piece[i]);
                }
              }
            }
          }
          //printf("%s\n",buffer);
          if(strlen(buffer)>0){
            fptr = fopen("list","w+");
            fwrite(buffer,strlen(buffer),1,fptr);
            fclose(fptr);
            free(buffer);
            /* Sort the list and get all unique files */
            system("sort list | uniq > final_list");
            //printf("%s\n","Sorted");
            //system("cat final_list");
            fptr = fopen("final_list","r");
            char *fileString = NULL;
            char *fileCopy = NULL;
            if(fptr!=NULL){
              fstat(fileno(fptr),&file_info);
              fileSize = file_info.st_size;
              buffer = (char *)calloc(1,fileSize); /* Buffer to store the contents of each line */
              fileString = (char *)calloc(1,fileSize);
              fileCopy = (char *)calloc(1,fileSize);
              int piece_count = 0;
              //printf("Size of buffer %ld\n",sizeof(buffer) );
              while(getline(&buffer, &fileSize, fptr) != -1)
              {   //printf("New line - %s\n", buffer);
                  if(strncmp(buffer,".",strlen(".")) == 0)
                  {
                    //char *linePtr;/* Pointer to the start of each line */
                    linePtr = strstr((char *)buffer,".");
                    linePtr = linePtr + strlen(".");
                    if (piece_count == 0)
                    {
                        memset(fileString,(int)'\0',strlen(fileString));
                        memcpy(fileString,linePtr,strlen(linePtr)-3);
                    }
                    memset(fileCopy,(int)'\0',strlen(fileCopy));
                    memcpy(fileCopy,linePtr,strlen(linePtr)-3);
                    if(strcmp(fileString,fileCopy) == 0)
                    {
                        piece_count = piece_count +1;
                        if(piece_count == 4){
                            printf("%s [COMPLETE]\n",fileString);
                            //piece_count = 0;
                        }
                    }
                    else
                    {
                        if(piece_count != 4)
                            printf("%s [INCOMPLETE]\n",fileString);
                        piece_count = 1;
                        strcpy(fileString,fileCopy);

                    }
                  }
              }
              if(piece_count != 4)
                  printf("%s [INCOMPLETE]\n",fileString);
            }
            free(fileString);
            free(fileCopy);
            fclose(fptr);
          }
          free(buffer);

          for(i=0;i<NUM_OF_SERVERS;i++){
            free(piece[i]);
            piece[i]=NULL;
          }
          break;
    case 4:
          if(token[1] == NULL){
            printf("%s\n","Enter a directory name");
          }else{
            for(i=0;i<NUM_OF_SERVERS;i++){
              memset(header,(int)'\0',strlen(header));
              sprintf(header,"%s %s %s %s",userName,passWord,token[0],token[1]);
              printf("%s\n",header);
              status = sendToServer(socket_fd[i],header,strlen(header));
              if(status == 0){
                printf("Server %d not active\n",i+1);
              }else{
                if((val = recv(socket_fd[i],response,1,0))<=0){
                  printf("%s\n","Error receiving");
                }
                if(response[0]==0){
                  printf("%s\n","Invalid Username/Password. Please try again.");
                  //break;
                }else{
                  //printf("%s\n","User authenticated");
                  if((val = recv(socket_fd[i],response,1,0))<=0){
                    printf("%s\n","Error receiving");
                  }
                  else{
                    if(response[0]==0){
                      printf("%s %d\n","Directory not created on server",i+1);
                    }else{
                      printf("%s %d\n","Directory created on server",i+1);
                    }
                  }
                }
              }
            }
          }
          break;
    default: /* If wrong command sent */
          printf("%s\n","Wrong command entered");
          break;
    }
    //memset(command,(int)'\0',strlen(command));
    //memset(buffer,0,sizeof(buffer));
    //memset(holder,0,sizeof(holder));
  }

  /* Close the socket and return */
  close(socket_fd[0]);
  close(socket_fd[1]);
  close(socket_fd[2]);
  close(socket_fd[3]);
  return 0;
}


/* Function to read the server configuration file */
int readConfig(char * configName){
  FILE *cptr;
  char *src,*position;
  char configBuffer[512] = {(int)'\0'};
  struct stat file_info;
  /* Read the config file */
  if((cptr=fopen(configName,"r"))==NULL)
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

    src = strstr(configBuffer,"DFS1");
    src = src + strlen("DFS1")+1;
    position = src;
    while(*position!=':'){
      position++;
    }
    strncpy(serverIPs[0],src,position-src);
    src = position+1;
    while(*position!='\n'){
      position++;
    }
    strncpy(serverPort[0],src,position-src);

    src = strstr(configBuffer,"DFS2");
    src = src + strlen("DFS2")+1;
    position = src;
    while(*position!=':'){
      position++;
    }
    strncpy(serverIPs[1],src,position-src);
    src = position+1;
    while(*position!='\n'){
      position++;
    }
    strncpy(serverPort[1],src,position-src);

    src = strstr(configBuffer,"DFS3");
    src = src + strlen("DFS3")+1;
    position = src;
    while(*position!=':'){
      position++;
    }
    strncpy(serverIPs[2],src,position-src);
    src = position+1;
    while(*position!='\n'){
      position++;
    }
    strncpy(serverPort[2],src,position-src);

    src = strstr(configBuffer,"DFS4");
    src = src + strlen("DFS4")+1;
    position = src;
    while(*position!=':'){
      position++;
    }
    strncpy(serverIPs[3],src,position-src);
    src = position+1;
    while(*position!='\n'){
      position++;
    }
    strncpy(serverPort[3],src,position-src);

    src = strstr(configBuffer,"Username:");
    src = src + strlen("Username:")+1;
    if(sscanf(src,"%s",userName)!=1){
      return 0;
    }

    src = strstr(configBuffer,"Password:");
    src = src + strlen("Password:")+1;
    if(sscanf(src,"%s",passWord)!=1){
      return 0;
    }

    /*for (i = 0; i < NUM_OF_SERVERS; i++) {
      printf("IP - %s, Port - %s\n",serverIPs[i],serverPort[i]);
    }*/
    printf("User - %s\n",userName);
    printf("Password - %s\n", passWord);
  }
  fclose(cptr);
  return 1;
}

/* Function to setup connection to the distributed server */
int setupConnectionToServer(){
  int i;
  for(i = 0;i<NUM_OF_SERVERS; i++){

    memset(&remoteAddr[i],(int)'\0',sizeof(struct sockaddr_in));
    remoteAddr[i].sin_family = AF_INET;
    remoteAddr[i].sin_addr.s_addr = inet_addr(serverIPs[i]);
    remoteAddr[i].sin_port = htons(atoi(serverPort[i]));

    if(connect(socket_fd[i],(struct sockaddr*)&remoteAddr[i],sizeof(struct sockaddr))<0){
      if(errno != EISCONN){
        printf("Error connection to server %d\n", i+1);
        perror("Error Connection:");
        sleep(1);
        if(connect(socket_fd[i],(struct sockaddr*)&remoteAddr[i],sizeof(struct sockaddr))<0){
          printf("Timed out on %d\n", i+1);
        }
      }
    }else{
      printf("Connected to DFS%d\n",i+1);
    }
  }
  return 1;
}

/* Function to calculate MD5 sum of a buffer */
int calculateMD5(const char * buffer,long int fileSize, uint8_t *lastByte){
  uint8_t *md5_res = NULL;
  md5_res = (uint8_t *)calloc(1,MD5_DIGEST_LENGTH);
  MD5(buffer,fileSize,md5_res);
  *lastByte = md5_res[MD5_DIGEST_LENGTH-1];
  printf("Last byte %x\n",*lastByte);
  free(md5_res);
  return 1;
}

/* Function to get the encryption key based on 2 input strings */
int calculateEncryptKey(char * param1, char * param2){
  int encryptKey = 0;
  int i;
  for(i=0;i<strlen(param1);i++){
    encryptKey+=*param1;
    param1++;
  }
  for(i=0;i<strlen(param2);i++){
    encryptKey+=*param2;
    param2++;
  }
  return encryptKey;
}

/* Function to create client sockets depending on the number of servers */
int createClientSocket(){
  int i;
  for(i=0;i<NUM_OF_SERVERS;i++){
    if ((socket_fd[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      printf("Client failed to create socket %d\n",i+1);
      exit(1);
    }
  }
  return 1;
}

/* Function to send a string to server */
int sendToServer(int sockfd, char * dataToSend, size_t len){

  int n;
  /* send the message line to the server */
  n = write(sockfd, dataToSend, len);
  if (n < 0){
    if(errno != EPIPE){
      perror("ERROR writing to socket:");
    }
    return 0;
  }
  return 1;
}
