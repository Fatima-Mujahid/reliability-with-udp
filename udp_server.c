#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>

#define MY_PORT "4950" // the port users will be connecting to
#define BUFFER_SIZE 500 // BUFFER_SIZE as per required for each segment (e.g: 500)


//struct for data packets
struct packet {
	int sequence_no;
	int packet_size;
	char data[BUFFER_SIZE];
};


// variables for socket
int socket_fd; // listen on socket_fd
struct addrinfo serv_addr, *serv_info, *ptr; // server's address information
struct sockaddr_storage cli_addr; // client's address information
socklen_t cli_addr_len = sizeof (struct sockaddr_storage);
char ip_addr[INET6_ADDRSTRLEN];
int rv;

// variables for video file
int no_of_bytes = 0;
int out;
int file_size;
int remaining = 0;
int received = 0;

// variables for transferring data paackets and acks
int no_of_packets = 5; // window size is 5
struct packet temp_packet;
struct packet packets[5];
int no_of_acks;
int acks[5];
int temp_ack;


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


// thread to receive packets (runs parallel with the main program)
void* receivePackets(void *vargp) {

	// receive 5 packets
	for (int i = 0; i < no_of_packets; i++) {
        	RECEIVE:
		if((no_of_bytes = recvfrom(socket_fd, &temp_packet, sizeof(struct packet), 0, (struct sockaddr *)&cli_addr, &cli_addr_len)) < 0) {
			perror("UDP Server: recvfrom");
			exit(1);
		}

		// in case of duplicate packet
        	if (packets[temp_packet.sequence_no].packet_size != 0) { 
            	// reallocating the array
            		packets[temp_packet.sequence_no] = temp_packet;
			// create an ack
			temp_ack = temp_packet.sequence_no;
			acks[temp_ack] = 1;
			// send the ack
			if(sendto(socket_fd, &temp_ack, sizeof(int), 0, (struct sockaddr *)&cli_addr, cli_addr_len) < 0){
				perror("UDP Server: sendto");
				exit(1);
			}
			printf("Duplicate Ack Sent:%d\n",temp_ack);

			// receive packet again until a unique packet is sent
			goto RECEIVE;
		}

		// in case of last packet
		if (temp_packet.packet_size == -1) {
			printf("last packet found\n");
			// decrementing the counter of the remaining loops
			no_of_packets = temp_packet.sequence_no + 1;
		}

		// in case of unique packet 
		if (no_of_bytes > 0) {
			printf("Packet Received:%d\n", temp_packet.sequence_no);
			// Keep the correct order of packets by index of the array
			packets[temp_packet.sequence_no] = temp_packet;
		}
              
	}
	return NULL;
}


// main function
int main(void) {

	memset(&serv_addr, 0, sizeof serv_addr); // ensure the struct is empty
	serv_addr.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
	serv_addr.ai_socktype = SOCK_DGRAM; //UDP socket datagrams
	serv_addr.ai_flags = AI_PASSIVE; // fill in my IP

	if ((rv = getaddrinfo(NULL, MY_PORT, &serv_addr, &serv_info)) != 0) {
		fprintf(stderr, "UDP Server: getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results in the linked list and bind to the first we can
	for(ptr = serv_info; ptr != NULL; ptr = ptr->ai_next) {
		if ((socket_fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) == -1) {
			perror("UDP Server: socket");
			continue;
		}

		// bind socket
		if (bind(socket_fd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
			close(socket_fd);
			perror("UDP Server: bind");
			continue;
		}

		break;
	}

	if (ptr == NULL) {
		fprintf(stderr, "UDP Server: Failed to bind socket\n");
		return 2;
	}

	freeaddrinfo(serv_info); // all done with this structure

	printf("UDP Server: Waiting to recieve datagrams...\n");
    
	pthread_t thread_id; // create thread ID
	
	// time delay variables
	struct timespec time1, time2;
	time1.tv_sec = 0;
	time1.tv_nsec = 30000000L;  // 0.03 seconds

	FILE * out = fopen("output_video.mp4","wb"); // open the video file in write mode		
	
	// receiving the size of the video file from the client
	if ((no_of_bytes = recvfrom(socket_fd, &file_size, sizeof(off_t), 0, (struct sockaddr *)&cli_addr, &cli_addr_len)) < 0) {
		perror("UDP Server: recvfrom");
		exit(1);
	}
	printf("Size of Video File to be received: %d bytes\n", file_size);

	no_of_bytes = 1;
	remaining = file_size;

	while (remaining > 0 || (no_of_packets == 5)) {

		// reinitialize the arrays
		
		memset(packets, 0, sizeof(packets));
        	for (int i = 0; i < 5; i++) {
        		packets[i].packet_size = 0; 
        	}

        	for (int i = 0; i < 5; i++) {
        		acks[i] = 0; 
        	}
               
        	// server starts receiving packets i.e thread execution starts
		pthread_create(&thread_id, NULL, receivePackets, NULL);

        	// wait for packets to be received i.e the code sleeps for 0.03 seconds
        	nanosleep(&time1, &time2);

		no_of_acks = 0;

		// send acks for the packets received only
		RESEND_ACK:
		for (int i = 0; i < no_of_packets; i++) {
			temp_ack = packets[i].sequence_no;
			// if the ack has not been sent before
			if (acks[temp_ack] != 1) {
				// create acks for the packets received ONLY
				if (packets[i].packet_size != 0) {
					acks[temp_ack] = 1;

					// send acks to the client
					if(sendto(socket_fd, &temp_ack, sizeof(int), 0, (struct sockaddr *)&cli_addr, cli_addr_len) > 0) {
						no_of_acks++;
						printf("Ack sent: %d\n", temp_ack);
					}
				}
			}
		}

		// stop n wait
		// wait for acks to be sent and processed by the client
		nanosleep(&time1, &time2);
		nanosleep(&time1, &time2);

		// if all packets were not received
		if (no_of_acks < no_of_packets) {
			goto RESEND_ACK;
		}
                
		// 5 packets have been received i.e. the thread executes successfully
		pthread_join(thread_id, NULL);
                 
		// write packets to output file
		for (int i = 0; i < no_of_packets; i++) {
			// data is present in the packets and its not the last packet
			if (packets[i].packet_size != 0 && packets[i].packet_size != -1) {
				printf("Writing packet: %d\n", packets[i].sequence_no);
				fwrite(packets[i].data, 1, packets[i].packet_size, out);
				remaining = remaining - packets[i].packet_size;
				received = received + packets[i].packet_size;
			}
		}

		printf("Received data: %d bytes\nRemaining data: %d bytes\n", received, remaining);
		
		// repeat process for the next 5 packets
	}
	
	printf("\nUDP Server: Recieved video file from client %s\n", inet_ntop(cli_addr.ss_family, get_in_addr((struct sockaddr *)&cli_addr), ip_addr, sizeof ip_addr));
	printf("File transfer completed successfully!\n");
    	close(socket_fd); // close server socket
    	return 0;
}
