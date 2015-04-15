/**************************************************************
rdt-part3.h
Student name: Huang Yukun
Student No. : 3035030808
Date and version: 4/15/2015, v1.0
Development platform: Ubuntu 12.04
Development language: C
Compilation:
	Can be compiled with g++
*****************************************************************/

#ifndef RDT3_H
#define RDT3_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <vector>

#define PAYLOAD 1000		//size of data payload of the RDT layer
#define TIMEOUT 50000		//50 milliseconds
#define TWAIT 10*TIMEOUT	//Each peer keeps an eye on the receiving  
							//end for TWAIT time units before closing
							//For retransmission of missing last ACK
#define W 5					//For Extended S&W - define pipeline window size


//----- Type defines ----------------------------------------------------------
typedef unsigned char		u8b_t;    	// a char
typedef unsigned short		u16b_t;  	// 16-bit word
typedef unsigned int		u32b_t;		// 32-bit word 

extern float LOSS_RATE, ERR_RATE;


/* this function is for simulating packet loss or corruption in an unreliable channel */
/***
Assume we have registered the target peer address with the UDP socket by the connect()
function, udt_send() uses send() function (instead of sendto() function) to send 
a UDP datagram.
***/
int udt_send(int fd, void * pkt, int pktLen, unsigned int flags) {
	double randomNum = 0.0;

	/* simulate packet loss */
	//randomly generate a number between 0 and 1
	randomNum = (double)rand() / RAND_MAX;
	if (randomNum < LOSS_RATE){
		//simulate packet loss of unreliable send
		printf("WARNING: udt_send: Packet lost in unreliable layer!!!!!!\n");
		return pktLen;
	}

	/* simulate packet corruption */
	//randomly generate a number between 0 and 1
	randomNum = (double)rand() / RAND_MAX;
	if (randomNum < ERR_RATE){
		//clone the packet
		u8b_t errmsg[pktLen];
		memcpy(errmsg, pkt, pktLen);
		//change a char of the packet
		int position = rand() % pktLen;
		if (errmsg[position] > 1) errmsg[position] -= 2;
		else errmsg[position] = 254;
		printf("WARNING: udt_send: Packet corrupted in unreliable layer!!!!!!\n");
		return send(fd, errmsg, pktLen, 0);
	} else 	// transmit original packet
		return send(fd, pkt, pktLen, 0);
}

/* this function is for calculating the 16-bit checksum of a message */
/***
Source: UNIX Network Programming, Vol 1 (by W.R. Stevens et. al)
***/
u16b_t checksum(u8b_t *msg, u16b_t bytecount)
{
	u32b_t sum = 0;
	u16b_t * addr = (u16b_t *)msg;
	u16b_t word = 0;
	
	// add 16-bit by 16-bit
	while(bytecount > 1)
	{
		sum += *addr++;
		bytecount -= 2;
	}
	
	// Add left-over byte, if any
	if (bytecount > 0) {
		*(u8b_t *)(&word) = *(u8b_t *)addr;
		sum += word;
	}
	
	// Fold 32-bit sum to 16 bits
	while (sum>>16) 
		sum = (sum & 0xFFFF) + (sum >> 16);
	
	word = ~sum;
	
	return word;
}

//----- Type defines ----------------------------------------------------------

// define your data structures and global variables in here
typedef u8b_t Packet[PAYLOAD+4];
typedef u8b_t ACK[4];

unsigned char sequence_number_space = 16; // 2^k = 16, can guarantee to be bigger than W=9
//use int type seq num, need convertion when putting into packet
unsigned char expected_sequence_number_to_receive = 0;
unsigned char next_sequence_number_to_send = 0;

int client_sender_already_to_receiver = 0;
int client_sender_already_back_to_sender = 0;
int server_fd = 0;
int client_fd = 0;

int rdt_socket();
int rdt_bind(int fd, u16b_t port);
int rdt_target(int fd, char * peer_name, u16b_t peer_port);
int rdt_send(int fd, char * msg, int length);
int rdt_recv(int fd, char * msg, int length);
int rdt_close(int fd);

/* Application process calls this function to create the RDT socket.
   return	-> the socket descriptor on success, -1 on error 
*/
int rdt_socket() {
//same as part 1
	return socket(AF_INET, SOCK_DGRAM, 0);
}

/* Application process calls this function to specify the IP address
   and port number used by itself and assigns them to the RDT socket.
   return	-> 0 on success, -1 on error
*/
int rdt_bind(int fd, u16b_t port){
//same as part 1
   struct sockaddr_in myaddr;
   myaddr.sin_family = AF_INET;
   myaddr.sin_port = htons(port);
   myaddr.sin_addr.s_addr = INADDR_ANY;
   return bind(fd, (struct sockaddr*) &myaddr, sizeof myaddr);
}

/* Application process calls this function to specify the IP address
   and port number used by remote process and associates them to the 
   RDT socket.
   return	-> 0 on success, -1 on error
*/
int rdt_target(int fd, char * peer_name, u16b_t peer_port){
//same as part 1
   struct addrinfo hints, *res;

   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_DGRAM;

   char port[sizeof peer_port];
   sprintf(port,"%d" ,peer_port);
   getaddrinfo(peer_name, port, &hints, &res);

   return connect(fd, res->ai_addr, res->ai_addrlen);
}

/* Application process calls this function to transmit a message to
   target (rdt_target) remote process through RDT socket; this call will
   not return until the whole message has been successfully transmitted
   or when encountered errors.
   msg		-> pointer to the application's send buffer
   length	-> length of application message
   return	-> size of data sent on success, -1 on error
*/
int rdt_send(int fd, char * msg, int length){
//implement the Extended Stop-and-Wait ARQ logic

//must use the udt_send() function to send data via the unreliable layer
	if (server_fd == 0)
  		server_fd = fd; //set up only once
  	if (client_sender_already_to_receiver == 1)
  		client_sender_already_back_to_sender = 1;
	int number_of_packets_need_to_send = length / PAYLOAD;
	if (length % PAYLOAD != 0){
		number_of_packets_need_to_send++;
	}
	std::vector<int> required_ACKs;
	//buffer the packets in the sender side
	Packet pkt[number_of_packets_need_to_send];
	for (int i=0; i<number_of_packets_need_to_send; i++){
		//control info
		pkt[i][0] = '1'; // type of packet, 1 for data
		pkt[i][1] = next_sequence_number_to_send;
		//set checksum field to zero
		pkt[i][2] = '0';
		pkt[i][3] = '0';
		// copy application data to payload field
		if (i != number_of_packets_need_to_send-1){
			std::copy(msg + i * PAYLOAD, msg + ((i+1) * PAYLOAD), &pkt[i][4]);
			u16b_t ckm = checksum(pkt[i], PAYLOAD+4);
			//set checksum in the header
			memcpy(&pkt[i][2], (unsigned char*)&ckm, 2);
		}else{
			std::copy(msg + i * PAYLOAD, msg + length, &pkt[i][4]);
			u16b_t ckm = checksum(pkt[i], length - (number_of_packets_need_to_send-1)*PAYLOAD+4);
			memcpy(&pkt[i][2], (unsigned char*)&ckm, 2);
		}
		
		//calculate checksum for whole packet
		// printf("WHATS the LENGTH: %d\n", length);
		// u16b_t ckm = checksum(pkt[i], length+4);
		// //set checksum in the header
		// memcpy(&pkt[i][2], (unsigned char*)&ckm, 2);
		int send;
		if (i != number_of_packets_need_to_send-1)
		{
			if((send = udt_send(fd, pkt[i], PAYLOAD+4, 0)) == -1){
			 	perror("send");
			}
			printf("SEND out data = %d bytes, seq#=%d\n", send, next_sequence_number_to_send);
		}
		else{
			if((send = udt_send(fd, pkt[i], length - (number_of_packets_need_to_send-1)*PAYLOAD+4, 0)) == -1){
			 	perror("send");
			}
			printf("SEND out data = %d bytes, seq#=%d\n", send, next_sequence_number_to_send);
		}
		required_ACKs.push_back(next_sequence_number_to_send);
		if (next_sequence_number_to_send < sequence_number_space-1)
			next_sequence_number_to_send++;
		else
			next_sequence_number_to_send = 0;
	}

	int the_cACK_meaning_whole_packet_received = required_ACKs.back();

	struct timeval timer;
	//setting description set
	fd_set read_fds;
	FD_ZERO(&read_fds);

	u8b_t buf[4]; //header size
	FD_SET(fd, &read_fds);
	for(;;) {
	  //repeat until received expected ACK
  	  // FD_SET(fd, &read_fds);
	  //setting timeout
	  timer.tv_sec = 0;
	  // timer.tv_usec = 0;
	  timer.tv_usec = TIMEOUT;
	  int status;
	  status = select(fd+1, &read_fds, NULL, NULL, &timer);
	  if (status == -1){
	  	perror("select ");
	  	exit(4);
	  } else if(status == 0){
	  	//timeout happens
	  	//retransmit the unacknowledged packet
	  	for (std::vector<int>::iterator it = required_ACKs.begin() ; it != required_ACKs.end(); ++it){
	  		//it stores the seq number not getting ACK yet
	  		//the data can be calculated by the_cACK_meaning_whole_packet_received
	  		int send;
	  		if (*it > the_cACK_meaning_whole_packet_received){
	  			int buffered_pkt_sequence = number_of_packets_need_to_send - (the_cACK_meaning_whole_packet_received + sequence_number_space - *it);
	  			send = udt_send(fd, pkt[buffered_pkt_sequence-1], PAYLOAD+4, 0);
	  			printf("resend packet of size = %d\n", send);
	  		}
	  		else if (*it == the_cACK_meaning_whole_packet_received){
	  			send = udt_send(fd, pkt[number_of_packets_need_to_send-1], length - (number_of_packets_need_to_send-1)*PAYLOAD+4, 0);
	  			printf("resend packet of size = %d\n", send);
	  		}
	  		else{
	  			int buffered_pkt_sequence = number_of_packets_need_to_send - (the_cACK_meaning_whole_packet_received - *it);
	  			send = udt_send(fd, pkt[buffered_pkt_sequence-1], PAYLOAD+4, 0);
	  			printf("resend packet of size = %d\n", send);
	  		}
	  	}
	  	FD_SET(fd, &read_fds);
	  	continue;
	  }
		for(;;){
		  	if (FD_ISSET(fd, &read_fds)){
		  		int nbytes;
		  		nbytes = recv(fd, buf, sizeof buf, 0);
		  		printf("SEND receive msg of size %d\n", nbytes);
		  		u16b_t ACK_check = checksum(buf, 4);
		  		u8b_t checksum_in_char[2];
				memcpy(&checksum_in_char[0], (unsigned char*)&ACK_check, 2);
				// printf("checksum_in_char: %u, %u\n", checksum_in_char[0], checksum_in_char[1]);
		  		if (nbytes <= 0) {
					// got error or connection closed by client
					if (nbytes == 0) {
					// connection closed
						printf("selectserver: socket %d hung up\n", fd);
					} else {
						perror("recv");
					}
				}else {
						if (checksum_in_char[0]!='0' || checksum_in_char[1]!='0'){
						//not necessarily corrupted
						if (buf[0] == '0'){
							perror("corrupted");
							break;
						}
						else{
							//data
							printf("SENDER get data\n");
							ACK ack;
							ack[0] = '0'; //packet type, 0 is ACK
							ack[2] = '0';
							ack[3] = '0';
							// ack[1] = '0';//it can only be 0 based on the sequence
							int number = 0;
							ack[1] = (unsigned char) number;
							// //so resend the ACK if it's only if it's "old"
							// // printf("senderCOUNT = %d\n", sender_count);
							if (fd == client_fd && client_sender_already_back_to_sender != 1){
								//ignore the new data msg
								number = 1;
								ack[1] = (unsigned char) number;
								u16b_t ckm = checksum(ack, 4);
								memcpy(&ack[2], (unsigned char*)&ckm, 2);
								udt_send(fd, ack, 4, 0);
								printf("UNIQUE ACK1 sent\n");
								FD_SET(fd, &read_fds);
								break;
							}
							else if (fd == server_fd && client_sender_already_to_receiver == 1){
								u16b_t ckm = checksum(ack, 4);
								memcpy(&ack[2], (unsigned char*)&ckm, 2);
								if (udt_send(fd, ack, 4, 0) == -1){
									perror("send");
								}
								else{
									printf("UNIQUE ACK0 sent\n");
									FD_SET(fd, &read_fds);
									break;
								}
							}
							else{
								printf("strange thigns\n");
								printf("break\n");
								//ignore
								break;
							}
						}
					}
					else{
						//if is ACK
						if (buf[0] == '0'){
							// printf("ACK%u get, expecting = %d \n", the_cACK_meaning_whole_packet_received, the_cACK_meaning_whole_packet_received);
							// printf("buf[1] = %c, ACK get = %u\n", buf[1],(unsigned char)the_cACK_meaning_whole_packet_received);
							//you cannot imagine how much I suffer doing casting
							if (buf[1] == (unsigned char)the_cACK_meaning_whole_packet_received){
								//the whole msg is received by its peer
								printf("cACK%u get, whole msg successfully received!\n", buf[1]);
								return length; //not counting the size of headers
							}
							else{
								for (std::vector<int>::iterator it = required_ACKs.begin() ; it != required_ACKs.end(); ++it){
									if (buf[1] == (unsigned char)*it){
										printf("ACK%u received!\n", buf[1]);
										required_ACKs.erase(it);
										//restart timer
										break;
									}
								}
								FD_SET(fd, &read_fds);
								break;
							}
						}
						else {
							// //if is data
							// printf("SENDER receive Dataaaaaaaaaaaaaaaaaaaa\n");
							// //this is a sender, how can we receive it
							// //ignore it
						}
					}
				}
		  	}
		  }
	}
}

/* Application process calls this function to wait for a message of any
   length from the remote process; the caller will be blocked waiting for
   the arrival of the message. 
   msg		-> pointer to the receiving buffer
   length	-> length of receiving buffer
   return	-> size of data received on success, -1 on error
*/
int rdt_recv(int fd, char * msg, int length){
//implement the Extended Stop-and-Wait ARQ logic
	if (client_fd == 0)
		client_fd = fd;
	if (fd == server_fd)
		client_sender_already_to_receiver = 1;
	// printf("waiting for message\n");
	for(;;) {
		int receiveBytes = recv(fd, msg, length+4, 0);
		// printf("last byte = %c\n", msg[receiveBytes-1]);
		printf("RECV receive msg of size %d\n", receiveBytes);
		if (receiveBytes <= 0){
			if (receiveBytes == 0) {
			// connection closed
				printf("selectserver: socket %d hung up\n", fd);
			} else {
				perror("recv");
			}
		}
		u8b_t* checksum_msg = (u8b_t*)msg;
		u16b_t ckm = checksum(checksum_msg, receiveBytes);
		u8b_t checksum_in_char[2];
		memcpy(&checksum_in_char[0], (unsigned char*)&ckm, 2);
		//make ACK packet
		ACK ack;
		ack[0] = '0'; //packet type, 0 is ACK
		ack[2] = '0';
		ack[3] = '0';
		// printf("unsigned short ckm: %hu\n", ckm);
		// printf("checksum_in_char: %c, %c\n", checksum_in_char[0], checksum_in_char[1]);
		// if (false){
		if (checksum_in_char[0]!='0' || checksum_in_char[1]!='0'){
				//corrupted
				printf("message corrupted\n");
				//resend last ACK
				unsigned char last_sent_ACK;
				if (expected_sequence_number_to_receive - 1 <0){
					last_sent_ACK = sequence_number_space - 1;
				}
				else{
					last_sent_ACK = expected_sequence_number_to_receive - 1;
				}
				ack[1] = last_sent_ACK;
				u16b_t ckm = checksum(ack, 4);
				memcpy(&ack[2], (unsigned char*)&ckm, 2);
				if (udt_send(fd, ack, 4, 0) == -1){
					perror("send");
				}
				else{
					printf("resend ACK=%d, expecting = %d \n",last_sent_ACK, expected_sequence_number_to_receive);
				}
		}else{
			if (msg[0] == '1'){
				//is DATA
				//is also expected
				if (msg[1] == expected_sequence_number_to_receive){
					printf("receive expected data\n");
					ack[1] = msg[1];
					u16b_t ckm = checksum(ack, 4);
					memcpy(&ack[2], (unsigned char*)&ckm, 2);
					if (udt_send(fd, ack, 4, 0) == -1){
							perror("send");
					}
					else{
						printf("send ACK=%d \n",expected_sequence_number_to_receive);
						if (expected_sequence_number_to_receive < sequence_number_space - 1){
							expected_sequence_number_to_receive++;
						}
						else{
							expected_sequence_number_to_receive = 0;
						}
						// for (int i=0; i < receiveBytes; i++){
						// 	printf("msg[%d] = %c\n", i, msg[i]);
						// }
						for (int i=0; i < receiveBytes-4; i++){
							msg[i] = msg[i+4];
						}
						msg[receiveBytes-4] = '\0';

						return receiveBytes - 4;
					}
				}else{
					// not expected data, resend last ACK
					printf("not expected data, expecting %d\n", expected_sequence_number_to_receive);
					unsigned char last_sent_ACK;
					if (expected_sequence_number_to_receive - 1 <0){
						last_sent_ACK = sequence_number_space - 1;
					}
					else{
						last_sent_ACK = expected_sequence_number_to_receive - 1;
					}
					ack[1] = last_sent_ACK;
					u16b_t ckm = checksum(ack, 4);
					memcpy(&ack[2], (unsigned char*)&ckm, 2);
					if (udt_send(fd, ack, 4, 0) == -1){
						perror("send");
					}
					printf("resend ACK%d\n", last_sent_ACK);
				}
			}else{
				//is ACK
				ACK ack;
				ack[0] = '0'; //packet type, 0 is ACK
				ack[2] = '0';
				ack[3] = '0';
				ack[1] = '1';
				u16b_t ckm = checksum(ack, 4);
				// printf("unsigned short ckm: %hu\n", ckm);
				memcpy(&ack[2], (unsigned char*)&ckm, 2);
				if (udt_send(fd, ack, 4, 0) == -1){
					perror("send");
				}
				else{
					printf("ACK1 sent, for replying sender\n");
				}
			}
		}
	}
}

/* Application process calls this function to close the RDT socket.
*/
int rdt_close(int fd){
//implement the Extended Stop-and-Wait ARQ logic
	struct timeval timer;
	//setting description set
	fd_set read_fds;
	FD_ZERO(&read_fds);

	FD_SET(fd, &read_fds);
	int status;
	u8b_t buf[4];
	for (;;){
		//setting timeout
		timer.tv_sec = 0;
		timer.tv_usec = TWAIT;
		status = select(fd+1, &read_fds, NULL, NULL, &timer);
		if (status == -1){
		  	perror("select ");
		  	exit(4);
		} else if(status == 0){
		  	//timeout happens
		  	return close(fd);
		}
		else{
		  	if (FD_ISSET(fd, &read_fds)){
		  		recv(fd, buf, sizeof buf, 0);
		  		u16b_t ACK_check = checksum(buf, 4);
		  		u8b_t checksum_in_char[2];
				memcpy(&checksum_in_char[0], (unsigned char*)&ACK_check, 2);
				if (checksum_in_char[0] =='0' && checksum_in_char[1] =='0'){
					//ACK
					printf("get ACK, terminate\n");
				}
				else{
					//data
					ACK ack;
					ack[0] = '0';
					ack[2] = '0';
					ack[3] = '0';
					// int number = sequencenumbertore;
					// ack[1] =  (unsigned char) number;//of course it needs modification
					unsigned char last_sent_ACK;
					if (expected_sequence_number_to_receive - 1 <0){
						last_sent_ACK = sequence_number_space - 1;
					}
					else{
						last_sent_ACK = expected_sequence_number_to_receive - 1;
					}
					ack[1] =  last_sent_ACK;
					u16b_t ckm = checksum(ack, 4);
					memcpy(&ack[2], (char*)&ckm, 2);
					if (udt_send(fd, ack, 4, 0) == -1){
						perror("send");
					}
					else{
						printf("CLOSE resend ACK = %u\n", ack[1]);
					}
				}
			}
		}
	}
}

#endif
