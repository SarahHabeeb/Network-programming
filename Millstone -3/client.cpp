/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2017
                Author:  Sarah Alyousefi 
                         Ryan Bomalaski
                Florida Tech, Human Decision Support Systems Laboratory

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
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>
#include <sys/types.h>
#include <netdb.h> 
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <time.h> ///* time_t, struct tm, difftime, time, mktime */
#include <errno.h>
#include "sha256.h"//hashing function.h
#include <arpa/inet.h>

 
#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

using namespace std;
const int  buff = 1024;


///////////////////////////this is the current time function//////////////////

  // Due to how the server parses incoming commands, ':' cannot be in the command.  Likewise, spaces are a pain for time
  //  This also for some reason appends a /n to the time stamp, so I need to get rid of that too
string getTime(){
  time_t timestmp;
  timestmp=time(NULL);
  //get time
  char* CurrentTime =ctime(&timestmp);

  //get rid of invalid characters
  int i;
    for (i=0; CurrentTime[i]!= '\0'; i++)
    {
        if (CurrentTime[i] == ' ')
        {
            CurrentTime[i]='-';
        }
        if(CurrentTime[i]==':'){
	  CurrentTime[i] = '-';
	}
	if(CurrentTime[i] =='\n'){
	  CurrentTime[i] = '\0';
	}
    }
    //debug stuff
    //printf("Current Time is: %s\n", CurrentTime);
    //printf("Current time length  is: %lu\n", strlen(CurrentTime));
  return CurrentTime;
}



//////////////////////////////////   TCP client  //////////////////////////////////////////


void TCP_client(string theMessage, string HostName, int portNumber){
	string  timestp,hashcmd;
	struct hostent *server;
	struct sockaddr_in serverAddr;
	int sock,sockfd, msglen;
	char* msg[buff];
	socklen_t leg;
	char hostname[buff]; 
	// create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1)
	{
	 cout<<" ERROR: unable to create the socket"<<"\n";
	 exit(0);
  
	}
    
       // get the host by name
    	strcpy(hostname, HostName.c_str());
    	server = (struct hostent *) gethostbyname(hostname);
    	if (server == NULL) {
        	fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        	exit(0);
    	}
    	
	//connect to the server
    
	
    	bzero((char *) &serverAddr, sizeof(serverAddr));
        bcopy((char *)server->h_addr, (char *)&serverAddr.sin_addr.s_addr, server->h_length);

     	serverAddr.sin_family = AF_INET;
    	serverAddr.sin_port = htons(portNumber);
    	
    	//connect 
	if(connect(sock,(struct sockaddr*)&serverAddr, sizeof(serverAddr))==-1){
		fprintf(stderr, "\n ERROR can not connect to the server ..\n");
	exit(0);
	}

	// If the user didn't start with a message at command line, say hello and prompt for message
	if (theMessage.compare("")==0){
		
		printf("Hello, Welcome to my peers client..\n");
	
	
	}
	// This will listen for messages 1 at a time
	// There are two invalide messages: "quit" and "help" which are reserved for client funcationality
	// quit will quit the program
	// help will display a help dialog
	// All other input is used as a message and sent to the server declared when launching.
	leg = sizeof(serverAddr);
	while(1){
	   if (theMessage.compare("")!=0){
	     if(theMessage.compare("quit")==0){
	      
		exit(0);
			
	      }else if(theMessage.compare("help")==0){
	      
		printf("Enter a message to send a message.\n");
		printf("Type quit to quit.\n");
		theMessage = "";
		continue;
			
	      }
		//hash the message with SHA256///
	      string hashed =sha256(theMessage);
	      //get the current time ///
	      timestp = getTime();
	      //concat the whole message
	      hashcmd="GOSSIP:"+hashed+":"+timestp+":"+theMessage+"%\n";
	      //debug for displaying message
	      //printf("TCP - Message is: %s\n",hashcmd.c_str());
	      //printf("Size is %lu\n",strlen(hashcmd.c_str()));
			
	      //send the message
	      write(sock,hashcmd.c_str(),strlen(hashcmd.c_str()));
			
	      // get the server reply
	      msglen= read(sock,&msg, buff);
	      if (msglen>0){
		printf("command sent \n"); 
	      }
	      //set message and hashcmd to null, so that we don't continually send them
	      theMessage = "";
	      hashcmd = "";
			
	    }else{
			printf("TCP:Please Enter a message or help: \n ");
			getline(cin,theMessage);
		}

	}
}

//////////////////////////////////   UDP client  //////////////////////////////////////////



void UDP_client(string theMessage, string HostName, int portNumber){
	string  timestp,hashcmd;
	struct hostent *server;
	struct sockaddr_in serverAddr;
	char* msg[buff];
	char hostname[buff]; 
	int clientSocket, nBytes,msglen;
	socklen_t addr_size;

  /*Create UDP socket*/
  clientSocket = socket(PF_INET, SOCK_DGRAM, 0);

  /*Configure settings in address struct*/
  strcpy(hostname, HostName.c_str());
  server = (struct hostent *) gethostbyname(hostname);
  bzero((char *) &serverAddr, sizeof(serverAddr));
  bcopy((char *)server->h_addr, (char *)&serverAddr.sin_addr.s_addr, server->h_length);
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(portNumber);
  memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  

  /*Initialize size variable to be used later on*/
  addr_size = sizeof serverAddr;

  while(1){
    if(theMessage.compare("")!=0){
      if(theMessage.compare("quit")==0){
	exit(1);
      }else if (theMessage.compare("help")==0){
	//help
	printf("Enter a message to send a message.\n");
	printf("Type quit to quit.\n");
	theMessage ="";
	hashcmd = "";
      }else{
	string hashed = sha256(theMessage);
	timestp = getTime();
	hashcmd="GOSSIP:"+hashed+":"+timestp+":"+theMessage+"%\n";
	//printf("UDP - Message is: %s\n",hashcmd.c_str());
	nBytes = strlen(hashcmd.c_str()) + 1;
	//Send message to server
	msglen = sendto(clientSocket,hashcmd.c_str(),nBytes,0,(struct sockaddr *)&serverAddr,addr_size);


	//Receive message from server
	nBytes = recvfrom(clientSocket,&msg,buff,0,NULL, NULL);
	if(nBytes>0){
	  printf("Message sent\n");
	}
	theMessage = "";
	hashed = "";
      }
    }else{
      //get message
      //prompt for message
	    printf("UDP:Please Enter a command or help: \n ");
	    getline(cin,theMessage);
    }
  }
}
   

//////////////////////////// this my main function //////////////////////////


int main(int argc,char *argv[]){

	string strcmd1;
	int portNumber = 12345;
	
	string strcmd2 = "2017-01-09-16-18-20-001Z";
	string HostName = "183.116.10.43";
	int Uflag=0;
	int Tflag=0;
        int option;

	while ((option = getopt(argc, argv, "p:h: m:: t:: UT")) != -1) {
		switch (option) {
		case 'p':
			portNumber = atoi(optarg);
			break;
		case 'h':
			HostName = optarg;
			break;
		case 'm':
			strcmd1 = optarg;
			break;
		case 't':
			strcmd2 = optarg;
			break;
		case'U':
			Uflag=1;
			printf("-U: for UDP instead of default TCP..\n");
			break;
		case'T':
			Tflag=1;
			printf("-T: for TCP instead of default UDP..\n");
			break;	
		default: 
		    cerr << "Usage: eClient [-p xxxx] [-h HostName] [-m message] [-t messageTime]" << endl;
		    cerr << "   -p xxxx                Assignsfor port number :2334" << endl;
		    cerr << "   -m <Message>           Assigns for messages:Tom eats Jerry  " << endl;
		    cerr << "   -h HostName            The host name (like olin.fit.edu).." << endl;
		    cerr << "   -t<MassgesTime>        Assigns for the time:2017-01-09-16-18-20-001Z." << endl;
		    cerr << "   -s<Address>            Assigns for the Address:183.116.10.43" << endl;
		    cerr << "   -U                     work with UDP  instead of the TCP."<<endl; 
		    cerr << "   -T                     work with TCP  instead of the UDP."<<endl; 
		    cerr << "                          Default is 'localhost'" << endl;
		    exit(EXIT_FAILURE);
		}
	}
	string theMessage = strcmd1;
	string timestp = strcmd2;
	
	if (Uflag==1){
	      printf(" run the UDP client with the  server :) ...\n");
	      UDP_client(theMessage,HostName,portNumber);
		 }
		 

	else if (Tflag==1){
		printf(" run the TCP client with the  server :) ...\n");
		TCP_client(theMessage,HostName,portNumber);
				  
	   }else {
	      printf("Didn't specify TCP or UDP, defaulting to TCP\n");
	      TCP_client(theMessage,HostName,portNumber);
	   }
  
}
	
  // SHA256 fuctions
 
 
/*
 *
 * Copyright (C) 2005, 2007 Olivier Gay <olivier.gay@a3.epfl.ch>
 * All rights reserved. */
 
 
 /* This is the hash function code I test it several time on different text and its work correctly
 *Please do not delete this part. 
 * Its impotant for the UDP and TCP client classes
 */
const unsigned int SHA256::sha256_k[64] = //UL = uint32
            {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
             0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
             0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
             0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
             0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
             0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
             0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
             0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
             0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
             0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
             0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
             0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
             0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
             0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
             0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
             0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
 
void SHA256::transform(const unsigned char *message, unsigned int block_nb)
{
    uint32 w[64];
    uint32 wv[8];
    uint32 t1, t2;
    const unsigned char *sub_block;
    int i;
    int j;
    for (i = 0; i < (int) block_nb; i++) {
        sub_block = message + (i << 6);
        for (j = 0; j < 16; j++) {
            SHA2_PACK32(&sub_block[j << 2], &w[j]);
        }
        for (j = 16; j < 64; j++) {
            w[j] =  SHA256_F4(w[j -  2]) + w[j -  7] + SHA256_F3(w[j - 15]) + w[j - 16];
        }
        for (j = 0; j < 8; j++) {
            wv[j] = m_h[j];
        }
        for (j = 0; j < 64; j++) {
            t1 = wv[7] + SHA256_F2(wv[4]) + SHA2_CH(wv[4], wv[5], wv[6])
                + sha256_k[j] + w[j];
            t2 = SHA256_F1(wv[0]) + SHA2_MAJ(wv[0], wv[1], wv[2]);
            wv[7] = wv[6];
            wv[6] = wv[5];
            wv[5] = wv[4];
            wv[4] = wv[3] + t1;
            wv[3] = wv[2];
            wv[2] = wv[1];
            wv[1] = wv[0];
            wv[0] = t1 + t2;
        }
        for (j = 0; j < 8; j++) {
            m_h[j] += wv[j];
        }
    }
}
 
void SHA256::init(){
    m_h[0] = 0x6a09e667;
    m_h[1] = 0xbb67ae85;
    m_h[2] = 0x3c6ef372;
    m_h[3] = 0xa54ff53a;
    m_h[4] = 0x510e527f;
    m_h[5] = 0x9b05688c;
    m_h[6] = 0x1f83d9ab;
    m_h[7] = 0x5be0cd19;
    m_len = 0;
    m_tot_len = 0;
}
 
void SHA256::update(const unsigned char *message, unsigned int len)
{
    unsigned int block_nb;
    unsigned int new_len, rem_len, tmp_len;
    const unsigned char *shifted_message;
    tmp_len = SHA224_256_BLOCK_SIZE - m_len;
    rem_len = len < tmp_len ? len : tmp_len;
    memcpy(&m_block[m_len], message, rem_len);
    if (m_len + len < SHA224_256_BLOCK_SIZE) {
        m_len += len;
        return;
    }
    new_len = len - rem_len;
    block_nb = new_len / SHA224_256_BLOCK_SIZE;
    shifted_message = message + rem_len;
    transform(m_block, 1);
    transform(shifted_message, block_nb);
    rem_len = new_len % SHA224_256_BLOCK_SIZE;
    memcpy(m_block, &shifted_message[block_nb << 6], rem_len);
    m_len = rem_len;
    m_tot_len += (block_nb + 1) << 6;
}
 
void SHA256::final(unsigned char *digest)
{
    unsigned int block_nb;
    unsigned int pm_len;
    unsigned int len_b;
    int i;
    block_nb = (1 + ((SHA224_256_BLOCK_SIZE - 9)
                     < (m_len % SHA224_256_BLOCK_SIZE)));
    len_b = (m_tot_len + m_len) << 3;
    pm_len = block_nb << 6;
    memset(m_block + m_len, 0, pm_len - m_len);
    m_block[m_len] = 0;
    SHA2_UNPACK32(len_b, m_block + pm_len - 4);
    transform(m_block, block_nb);
    for (i = 0 ; i < 8; i++) {
        SHA2_UNPACK32(m_h[i], &digest[i << 2]);
    }
}
 
std::string sha256(std::string input)
{
    unsigned char digest[SHA256::DIGEST_SIZE];
    memset(digest,0,SHA256::DIGEST_SIZE);
 
    SHA256 ctx = SHA256();
    ctx.init();
    ctx.update( (unsigned char*)input.c_str(), input.length());
    ctx.final(digest);
 
    char buf[2*SHA256::DIGEST_SIZE+1];
    buf[2*SHA256::DIGEST_SIZE] = 0;
    for (int i = 0; i < SHA256::DIGEST_SIZE; i++)
        sprintf(buf+i*2, "%02x", digest[i]);
    return std::string(buf);
}
  





  
  






