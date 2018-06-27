/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2017 

                Authors: Sarah Alyousefi 
				         Ryan Bomalaski
                Florida Tech, Computer Science
   
       This program is free software; you can redistribute it and/or modify
       it under the terms of the GNU Affero General Public License as published by
       the Free Software Foundation; either the current version of the License, or
       (at your option) any later version.
   
      This program is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
      GNU General Public License for more details.
  
      You should have received a copy of the GNU Affero General Public License
      along with this program; if not, write to the Free Software
      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.              */
/* ------------------------------------------------------------------------- */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <sqlite3.h>

// the below includes may not be necessary, test to resolve
//#include <strings.h>
//#include <errno.h>
//#include <netinet/in.h>

#define MAX_CALLS 5
#define MAXLINE 1024

void runTCP(int portNum, char dbfilepath[]);
int runUDP(int portNum, char dbfilepath[]);
char * messageProcessor(char message[], char dbfilepath[]);
char * add_peer(char message[], char dbfilepath[]);
char * get_peers(char dbfilepath[]);
char * add_gossip(char message[], char dbfilepath[]);
void broadcastTCP(char message[], char dbfilepath[]);
void broadcastUDP(char message[], char dbfilepath[]);
void send_gossip(int port, char* address, char* gossip_message);


char* concat( const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

static int callback(void *count, int argc, char **argv, char **azColName){
   int i;
   int j= 0;
   char string[256],c;
   for(i=0; i<argc; i++){
     printf("arv[%d] = %s\n",i,argv[i]);
     concat(string,("%s",argv[i]));
   }
   printf("%s\n",string);
   return 0;
}

int
main (int argc, char **argv)
{
  char *pvalue = NULL;
  char *dvalue = NULL;
  int index;
  int portNum;
  int c, rc;
  pid_t childPID;
  sqlite3 *db;
  char *sql_create_tables;
  char *zErrMsge = 0;
  
  opterr = 0;

  //getopt checks for options -p and -d
  //-p [port number] allows the user to specify a port to listen to
  //-d [path to db] allows the user to specifcy where the server database is
  while ((c = getopt (argc, argv, "p:d:")) != -1)
    switch (c)
      {
      case 'p':
        pvalue = optarg;
        break;
      case 'd':
        dvalue = optarg;
        break;
      case '?':
        if (optopt == 'p')
          fprintf (stderr, "-%c requires an argument.\n", optopt);
	else if (optopt == 'd')
	  fprintf (stderr, "-%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr,
                   "Unknown option character `\\x%x'.\n",
                   optopt);
        return 1;
      default:
        abort ();
      }
  for (index = optind; index < argc; index++)
    printf ("Non-option argument %s\n", argv[index]);
  
  //If the user didn't specify values, use these as defaults
  //remember not to hard code in paths, points off for that
  //these are for testing, program won't run without these.
  if (pvalue == NULL){
    fprintf(stderr, "Arguments incorrect. Exiting.\n");
    exit(1);
  }
  if (dvalue == NULL){
    fprintf(stderr, "Arguments incorrect. Exiting.\n");
    exit(1);
  }
  
  //set the port number to an int
  portNum = atoi(pvalue);
  
  //check to see that portNum is a valid port number
  if (portNum>65535 || portNum<1){
    printf ("%d is not a valid port. Please choose a valid port.\n", portNum);
    exit(1);
  }
  

  rc = sqlite3_open(dvalue, &db);
  sql_create_tables = "CREATE TABLE PEERS("  \
         "NAME TEXT PRIMARY KEY    NOT NULL," \
         "IP            TEXT     NOT NULL," \
         "PORT        TEXT NOT NULL );" \
         "CREATE TABLE GOSSIP(" \
         "SHA256 TEXT PRIMARY KEY NOT NULL," \
         "DATE_TIME TEXT, " \
         "MESSAGE TEXT );";
  
  rc = sqlite3_exec(db,sql_create_tables,callback, 0, &zErrMsge);
  
  rc = sqlite3_close(db);	      
  
  //debug to make sure I'm getting in the correct values, remove from final
  //printf ("Port to Listen on: %d\n", portNum);
  //printf ("Database located at: %s \n", dvalue);
  
  //We want both TCP and UDP to run concurently, so we gots to fork!
  childPID = fork();
  if (childPID<0){
    fprintf (stderr, "Failed to fork to child");
    return -1;
  }
  else if (childPID==0){
    //RUN UDP STUFF HERE
    printf ("UDP Ported Opened: %d\n", portNum);
    //fork on receicing connection
    runUDP(portNum,dvalue);
  }
  else if (childPID >0){
    //RUN TCP STUFF HERE
    printf ("TCP Port Opened: %d\n", portNum); 
    //fork on receiviing connection
    runTCP(portNum,dvalue);
  }
  return 0;
}

//runTCP is called to start up the tcp listening server.
//the server is iterative, that's why it does not fork when it receives a message
//others can still connect, but they will not be prompted to enter their message until
//availablility opens up.
void runTCP(int portNum, char dbfilepath[]){
  struct sockaddr_in cli_addr, serv_addr;
  int serv_socket, cli_socket, clilen, iobuffer,msglen;
  char buffer[MAXLINE];
  char *returnMessage;
  
  if ((serv_socket = socket(AF_INET, SOCK_STREAM, 0))<0){
    perror("server can't open stream socket - tcp\n");
    exit(1);
  }
  
  serv_addr.sin_family        = AF_INET;
  serv_addr.sin_port          = htons(portNum);
  serv_addr.sin_addr.s_addr   = htonl(INADDR_ANY);
  
  socklen_t serv_addr_size = sizeof(serv_addr);
  
  if(bind(serv_socket, (struct sockaddr*) &serv_addr, sizeof(serv_addr))<0){
    perror("error binding - tcp\n");
    exit(1);
  }
  
  if(listen(serv_socket, MAX_CALLS)<0){
    perror("server failed to listen - tcp\n");
    exit(1);
  }
  
  socklen_t cli_addr_size = sizeof(cli_addr);
  while(1){
    cli_socket = accept (serv_socket, ( struct sockaddr*) &cli_addr, &cli_addr_size);
    if(fork()==0){
      //write (cli_socket, "Please enter your command\n", strlen("Please enter your command\n"));
  
      while( (msglen= recv(cli_socket, buffer, MAXLINE,0))>0){
	printf("Received Message: %s.\n",buffer);
	
	returnMessage = messageProcessor(buffer,dbfilepath);
	write(cli_socket, returnMessage, sizeof(returnMessage));
	memset(&buffer[0],0,MAXLINE);
      }
  
      if (msglen == 0){
	printf("Client diconnected - TCP\n");
	fflush(stdout);
      }
  
      if (msglen<0){
	printf("Something happened!\n");
	exit(1);
      }
    }
    else{
      
      close(cli_socket);
    }
  }
}

// UDP for child fork
int runUDP(int portNum, char dbfilepath[]){
  int sockfd, optval, msgbytesize, client_addr_len;
  struct sockaddr_in serv_addr, cli_addr;
  struct hostent *hostp;
  char buffer[MAXLINE];
  char *hostaddrp, *returnMessage;
  pid_t pid;
  
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    perror("server can't open datagram socket - udp\n");
  
  //build server internet address
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons((unsigned short)portNum);
  
  //bind
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    perror("error binding - udp\n");
  client_addr_len = sizeof(cli_addr);
  while(1){

    //recvfrom - UDP equivalent of recv
    bzero(buffer, MAXLINE);
    
    msgbytesize = recvfrom(sockfd, buffer, MAXLINE, 0,
		 (struct sockaddr *) &cli_addr, &client_addr_len);
    
    if (msgbytesize < 0)
      perror("ERROR in recvfrom");
    
    if(msgbytesize>0){
      pid = fork();
      if(pid==0){
      //allows me to see who sent the datagram, not really necessary, but could be useful
      hostp = gethostbyaddr((const char *)&cli_addr.sin_addr.s_addr, 
			  sizeof(cli_addr.sin_addr.s_addr), AF_INET);
      if (hostp == NULL)
	perror("ERROR on gethostbyaddr");
      hostaddrp = inet_ntoa(cli_addr.sin_addr);
      if (hostaddrp == NULL)
	perror("ERROR on inet_ntoa\n");
      printf("server received datagram from %s (%s)\n", 
	    hostp->h_name, hostaddrp);
    
    //echo input back to client, currently used for debugging
    
      returnMessage = messageProcessor(buffer,dbfilepath);
      msgbytesize = sendto(sockfd, returnMessage, strlen(returnMessage), 0, 
		(struct sockaddr *) &cli_addr, client_addr_len);
      if (msgbytesize < 0) 
	perror("ERROR in sendto");
    }
    }
  }
  return portNum;
}

//this will chunk through the messages and the do the sql stuff
char * messageProcessor(char message[], char dbfilepath[]){
  int msglength;
  msglength = strlen(message);
  char firstLetter = message[0];
  char delimiter = ':';
  
  // check message commands
  // PLACEHOLDER METHOD, JUST CHECKING FIRST LETTERS!!!
  // THIS IS BAD PRACTICE, but is being used until I get everythign working
  // This will be corrected last, as we get to assume correct input for the actual assignment :)
  if(firstLetter=='G'){
    //DO GOSSIP
    return add_gossip(message,dbfilepath);
  }
  // peer or peers?
  else if(firstLetter =='P'){
    if (message[4]=='S'){
      //DO PEERS
      return get_peers(dbfilepath);
    }
    else{
      // DO PEER
      return add_peer(message,dbfilepath);
    }
  }
  // not a understandable command?
  else{
    return("Not a useable command.\n");
  }
}



//add peers
char * add_peer(char message[], char dbfilepath[]){
  char *name, *ip, *port;
  sqlite3 *db;
  char * i_hate, *how_c, *works_with, *strangs, *it_sucks;
  char *update, *update2, *update3, *update4, *update5;
  int rc;
  char *zErrMsge = 0;
  
  //parse out the bits we need from the message
  strtok(message,":=%");
  name = strtok(NULL,":=%");
  strtok(NULL,":=%");
  port = strtok(NULL,":=%");
  strtok(NULL,":=%");
  ip = strtok(NULL,":=%");
  
  //concatenate sql command
  i_hate = concat("INSERT INTO PEERS VALUES('",name);
  how_c = concat(i_hate,"','");
  works_with = concat(how_c,ip);
  strangs = concat(works_with,"',");
  it_sucks = concat(strangs, port);
  char *sql_insert;
  sql_insert = concat (it_sucks,");");
  
  //concatenate update command
  update = concat("UPDATE PEERS SET IP ='",ip);
  update2 = concat(update,"' ,PORT = ");
  update3 = concat(update2,port);
  update4 = concat(update3," WHERE NAME = '");
  update5 = concat(update4,name);
  char *sql_update;
  sql_update = concat(update5,"';");
  
  /*
   * ,port = ");
  update3 = concat(update2,port);
  update4 = concat(update3,"
   */
  
  //debug prints
  printf("This is your sequel command\n%s\n",sql_insert);
  printf("%s\n",name);
  printf("%s\n",port);
  printf("%s\n",ip);
  
  //open db
  rc = sqlite3_open(dbfilepath,&db);

  rc = sqlite3_exec(db,sql_insert,callback, 0, &zErrMsge);
  if( rc!=SQLITE_OK ){
    rc = sqlite3_exec(db,sql_update,callback, 0, &zErrMsge);
    
    //printf("This is your sequel command\n%s\n",sql_update);
    
    fprintf(stderr, "Updated.\n");
    sqlite3_free(zErrMsge);
    return ("Updated.\n");
  }

  
  //close db
  rc = sqlite3_close(db);
  return("Added\n");
}


//get peers
char * get_peers(char dbfilepath[]){
  char sql[200] = "SELECT * FROM PEERS";
  sqlite3 *db;
  int rc;
  char *zErrMsge = 0;
  
  rc = sqlite3_open(dbfilepath,&db);
  
  rc = sqlite3_exec(db,sql,callback, 0, &zErrMsge);
  if( rc!=SQLITE_OK ){
    fprintf(stderr, "SQL error: %s\n", zErrMsge);
    sqlite3_free(zErrMsge);
    }
  rc = sqlite3_close(db);
  
  return ("Got to get_peers\n");
}


// add gossip
char * add_gossip(char message[], char dbfilepath[]){
  char *sha256, *date_time, *text_mes;
  sqlite3 *db;
  char * i_hate, *how_c, *works_with, *strangs, *it_sucks;
  int rc;
  char *zErrMsge = 0;
  strtok(message,":");
  sha256 = strtok(NULL,":");
  date_time = strtok(NULL,":");
  text_mes = strtok(NULL,":%");
  
    //concatenate sql command
  i_hate = concat("INSERT INTO GOSSIP VALUES('",sha256);
  how_c = concat(i_hate,"','");
  works_with = concat(how_c,date_time);
  strangs = concat(works_with,"','");
  it_sucks = concat(strangs, text_mes);
  char *sql_insert;
  sql_insert = concat (it_sucks,"');");
  printf("got here3\n");
  
  //debug prints, remove later
  printf("This is your sequel command\n%s\n",sql_insert);
  printf("%s\n",sha256);
  printf("%s\n",date_time);
  printf("%s\n", text_mes);
  
   //open db
  rc = sqlite3_open(dbfilepath,&db);

  rc = sqlite3_exec(db,sql_insert,callback, 0, &zErrMsge);
  if( rc!=SQLITE_OK ){
    fprintf(stderr, "Discarded.\n");
    sqlite3_free(zErrMsge);
    return("Discarded.\n");
    }
  
  broadcastUDP(text_mes, dbfilepath);
  //close db
  rc = sqlite3_close(db);
  
  return ("Message Added!\n");
}


// Only broadcasting on UDP because of issues with lack of listeners on broadcastTCP
// If a client is not actively listening, then TCP will hang and be bad
// UDP doesn't care about listeners and will simply place a datagram to broadcast
// This means that a user could potentially miss a datagram if they aren't actively
// listening to a UDP port, but that's better than not functioning if they're
// not actively connected to a port
void broadcastUDP(char message[], char dbfilepath[]){
  int port;
  char* address, *peers;
//broadcast place filler
printf("Broad on UDP: %s\n",message);
// get peers
peers = get_peers(dbfilepath);
//for each peer
    //get port
    //get address
    //broadcasts on UDP 55440 for now, as a debug option
    send_gossip(55440,"127.0.0.1",message);
  
}

void send_gossip(int port, char* address, char* gossip_message){
	struct hostent *server;
	struct sockaddr_in serverAddr;
	char hostname[MAXLINE]; 
	int clientSocket, nBytes,msglen;
	socklen_t addr_size;

  /*Create UDP socket*/
  clientSocket = socket(PF_INET, SOCK_DGRAM, 0);

  /*Configure settings in address struct*/
  strcpy(hostname, address);
  server = (struct hostent *) gethostbyname(hostname);
  bzero((char *) &serverAddr, sizeof(serverAddr));
  bcopy((char *)server->h_addr, (char *)&serverAddr.sin_addr.s_addr, server->h_length);
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);
  memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  
  addr_size = sizeof serverAddr;
  
  nBytes = strlen(gossip_message) + 1;
  //Send message to server
  msglen = sendto(clientSocket,gossip_message,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);
  if(msglen<0){
    printf("Issue\n");
  }

  
}

