# Reliable Video File Transfer with UDP
This code is for .mp4 video files.
The input video file should have name input_video.mp4
The output file will be generated as output_video.mp4

### TO RUN UDP_SERVER:
 - gcc udp_server.c -lpthread -o udp_serv
 - ./udp_serv

### TO RUN UDP_CLIENT:
 - gcc udp_client.c -lpthread -o udp_cli
 - ./udp_cli 127.0.0.1

## Behavior

### General:
This project requires implementation of code for a sender and receivers that implements video file transfer over UDP protocol using Linux/GNU C sockets. Sender opens a video file, reads data chunks from the file, and writes UDP segments, it then sends these segments on UDP. Receiver is able to receive, reorder and write data to a file at the receiving end. Some further improvements are made to the traditional UDP flow for increased reliability.
### Sender:
The sender reads the file specified by the filename and transfers it using UDP sockets. The sender sends over packets of 500 bytes, and with retransmission for packets that might not have reached to the receiver end. Additionally the packets are sent in a window size of 5 UDP segments each of which have a wait time of 0.03 seconds waiting time in-between. On completing the transfer, the sender terminates and exits. The sender binds to the listen port to receive acknowledgments and other signaling from the receiver. A single UDP socket is used for both sending and receiving data.
### Receiver:
The receiver binds to the UDP port specified on the command line and receives a file from the sender sent over the port. The file is received in packets of 500 bytes each; furthermore each packet is serialized so that the file can be reordered exactly on the receiver side. The packets are received in window size of 5 UDP segments each with a delay of 0.03 seconds between them. The file is then saved to a different filename. The receiver exits once the transfer is complete.

## Design

### UDP Connection Unreliability:
A User Datagram Protocol (UDP) connection between a client and a server provides high speed communication. As UDP is not a connection-oriented communications protocol, it does not ensure the availability of a connection between the client and server before transferring data. This allows fast transfer of data but also some packets i.e. bits of data, might get lost in transit. This in turns leads to a generally more unreliable connection as compared to another popular transport protocol, Transmission Control Protocol (TCP).
### Implementing Reliability in UDP:
For most of its use cases the unreliability of UDP is not a hindrance rather a design choice; as in streaming media, real-time multiplayer games and voice over IP (VoIP) loss of packets is not a fatal problem. But if we were to use a UDP connection for downloading content or sending over a video file then this loss of packets and disorderly transmission is unwanted. To overcome this hurdle we have implemented the following in our construction of this solution:
1) Sequence numbers: All the packets being sent from the sender side are numbered this helps in maintaining order of packets intact.
2) Retransmission: Packets for which the sender has received no acknowledgment for are selectively repeated. This ensures that no packets are lost while transmitting.
3) Window size of 5 UDP segments: Stop and wait is incorporated with sending at most 5 packets at a time and then waiting for their acknowledgment of being received. This wait time allows for a retransmission period. 
4) Reordering on receiver side: Using the sequence numbers from step 1 packets being received on the receiver end is recombined to form their initial form.
All these steps make UDP much more reliable.
### Sender:
The sender is defined in the file udp_client.c. This file sends the video file as datagrams. The client starts by creating a UDP socket, and then continues to open the input video file in read mode and concurrently reads the size of the file. Then the client firstly sends over the file size over to the receiver. 
After this the client goes into the main execution loop which runs until all the data has been successfully transferred over the receiver. In this loop firstly packets of 500 bytes are made from the remaining data, after this those 5 packets (equal to the window size) are sent over to the receiver. In the next step the client gets ready to receive acknowledgment from the receiver about successful packets transmission, for this reason the code waits (sleeps) for 0.03 seconds. The packets for which no acknowledgment is received after this wait interval are then selectively retransmitted. Again the client wait and makes certain that the resent packets are properly received on the other end if not the previous step reiterates until acknowledgment for all the sent packets is ascertained. This main loop ends here.
Eventually when the UDP client is done transmitting the datagrams it closes down the socket and exits.

![Client](https://github.com/Fatima-Mujahid/reliability-with-udp/blob/main/Resources/Client.png)
### Receiver:
The receiver is defined in the file udp_server.c. This file receives the video file as datagrams.
The server starts by creating a UDP socket and binds the socket to the servers address. The server opens a video file in write mode and proceeds to wait until it receives some datagrams from the sender. The first thing that the server receives is the file size of the video file being sent over.
After this the server goes into the main execution loop which runs until all the data (5 packets each of 500 bytes) has been successfully received from the sender. Firstly the server sets itself up for receiving those five packets during this execution loop iteration. The server then continues onto receiving those packets and sending back acknowledgment for those packets that have been received in parallel. Now by predefining the window size the server knows exactly how many packets to expect and after a wait period of 0.03 seconds if it has not received the correct number of packets, it returns to receiving state once again in the same iteration of the execution loop. After the entirety of the packets and their contents have been received and acknowledged back to the sender the servers starts writing the packets to the output file. It is here that the sequential number of packets that was done by the sender comes to use as the server outputs to the write file accordingly resulting in no rearranging of data in the video file, which would render the video unwatchable. The main loop ends here.
Eventually when the UDP server is done receiving the datagrams it closes down the socket and exits.

![Server](https://github.com/Fatima-Mujahid/reliability-with-udp/blob/main/Resources/Server.png)

## Output

### Client:
![Client Output 1](https://github.com/Fatima-Mujahid/reliability-with-udp/blob/main/Resources/client1.png)
![Client Output 2](https://github.com/Fatima-Mujahid/reliability-with-udp/blob/main/Resources/client2.png)
### Server:
![Server Output 1](https://github.com/Fatima-Mujahid/reliability-with-udp/blob/main/Resources/server1.png)
![Server Output 2](https://github.com/Fatima-Mujahid/reliability-with-udp/blob/main/Resources/server2.png)
