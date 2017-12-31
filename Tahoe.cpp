#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <iostream>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
using namespace std;

#define SERVER 0
#define CLIENT 1
#define RTT 200
#define MSS 512
#define BUFFERSIZE 10240
#define HEADERSIZE 20

int THRESHOLD = 4096;
int socket_fd;
int selfPort;
pthread_t listen_t,scan_t;
struct sockaddr_in myaddr;   /* address that client uses */
struct tcpHeader
{
	int16_t sourcePort;
	int16_t destPort;
	int32_t seq_num;
	int32_t ack_num;
	int16_t flag;
	int16_t recvWindow;
	int16_t checksum;
	int16_t urgDataPointer;
};
struct segment
{
	struct tcpHeader hdr;
	char *buffer;
};


void serverListen()
{
    int length; /* length of address structure      */
    struct sockaddr_in client_addr; /* address of client    */
	struct hostent *hp;
	int seq_num=0,ack_num=0,cwnd=1,rwnd;
	char fileData[BUFFERSIZE];
	long fileSize;
	int dataSent;
	int datatransmit_ack_count;
	int bytes_notyet_sent;
	int congestion_avoid = 0;
	int ack_num_readytorecv;
	struct segment segmentRecv;
	struct segment segmentSend;
	FILE * f = fopen("leagues.txt","rb");
	
	fseek (f, 0, SEEK_END);
	fileSize = ftell (f);
	fseek (f, 0, SEEK_SET);
	fread (fileData, 1, length, f);
	fclose(f);
	
	
    length = sizeof(client_addr);
	printf("====Parameter=====\n");
	printf("The RTT delay = %d ms\n",RTT);
	printf("The threshold = %d bytes\n",THRESHOLD);
	printf("The MSS = %d bytes\n",MSS);
	printf("The buffer size = %d bytes\n",BUFFERSIZE);
	printf("Server's IP is %s\n","127.0.0.1");
	printf("Server is listening on port %d\n",selfPort);
	printf("==================\n");
	
	
	startToBeginServer:;

	while(1)
	{
	    printf("Listening for Node...\n");
	    printf("(ClientFunction)Please Input Node [IP] [Port] you want to connect.\n");
		
		recvfrom(socket_fd, &segmentRecv, MSS, 0, (struct sockaddr*)&client_addr, (socklen_t *)&length);
		pthread_cancel(scan_t);
		printf("%d\n",segmentRecv.hdr.flag>>1 );
		if((segmentRecv.hdr.flag>>1) % 2 == 1)
		{
			//================= three way handshake =====================
			printf("=====Start the three-way handshake=====\n");
	        printf("Received a packet (SYN) from %s : %d\n", inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port));
			printf("	Received a packet(seq_num = %d ,ack_num = %d)\n",segmentRecv.hdr.seq_num,segmentRecv.hdr.ack_num);
			sleep(1);
			srand(time(NULL));
			seq_num = rand()%9000+1000;
			ack_num = segmentRecv.hdr.seq_num+1;
			segmentSend.hdr.sourcePort = selfPort;
			segmentSend.hdr.destPort = htons(client_addr.sin_port);
			segmentSend.hdr.seq_num = seq_num;
			segmentSend.hdr.ack_num = ack_num;
			segmentSend.hdr.flag = 0x0012;	//ACK=1,SYN=1
			sendto(socket_fd, &segmentSend, sizeof(segmentSend), 0, (struct sockaddr*)&client_addr, length);
			printf("Send a packet (SYN/ACK) to %s : %d\n", inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port));
			recvfrom(socket_fd, &segmentRecv, MSS, 0, (struct sockaddr*)&client_addr, (socklen_t *)&length);
			if((segmentRecv.hdr.flag >> 4) % 2 == 1)
			{
				printf("Received a packet (ACK) from %s : %d\n", inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port));
				printf("	Received a packet(seq_num = %d ,ack_num = %d)\n",segmentRecv.hdr.seq_num,segmentRecv.hdr.ack_num);
				printf("=====Complete the three-way handshake=====\n");
			}
			else
			{
				printf("Three-way handshake fail.\n");
				exit(0);
			}
		}
		else
		{
			printf("The segment is not SYN segment.Three-way handshake fail.\n");
			seq_num = rand()%9000+1000;
			segmentSend.hdr.sourcePort = selfPort;
			segmentSend.hdr.destPort = htons(client_addr.sin_port);
			segmentSend.hdr.seq_num = seq_num;
			segmentSend.hdr.ack_num = 0;
			segmentSend.hdr.flag = 0x0004;	//RST=1
			sendto(socket_fd, &segmentSend, sizeof(segmentSend), 0, (struct sockaddr*)&client_addr, length);
			continue;
		}
		//============== end of three way handshake =================
		
	
		//===========  data transmisson ============
		
		seq_num = 1;
		dataSent = 0;
		printf("Start to send the file, the file size is 10240 bytes.\n");
		
		startToFastTransmit:;
		printf("*****Slow start*****\n ");
		cwnd = 0;
		congestion_avoid = 0;
		int dup_ack_num = 0;
		int prev_ack_num;
		while(dataSent < 10240)
		{
			if(cwnd==0)
			{
				cwnd = 1;
			}
			else
			{
				if(congestion_avoid==0)
					cwnd = cwnd * 2;
				else if(congestion_avoid==1)
					cwnd = cwnd + MSS;
				if(cwnd == THRESHOLD)
				{
					congestion_avoid = 1;
					printf("*****Congestion Avoidance*****\n");
				}
			}
			rwnd = BUFFERSIZE - cwnd + 1;
			printf("cwnd = %d, rwnd = %d, threshold = %d\n",cwnd,rwnd,THRESHOLD);
			bytes_notyet_sent = cwnd;
			ack_num_readytorecv = 0;
			while(bytes_notyet_sent>0)
			{
				if(cwnd > MSS)
				{
					ack_num++;
					segmentSend.hdr.seq_num = seq_num;
					segmentSend.hdr.ack_num = ack_num;
					segmentSend.hdr.flag = 0x0000;
					if(seq_num != 2048)
						sendto(socket_fd,&segmentSend, sizeof(segmentSend), 0,(struct sockaddr*)&client_addr, length);
					else
						printf("*****Data loss at byte : 2048\n");
					ack_num_readytorecv++;
					printf("	Send a packet at %d bytes\n",dataSent+1);
					dataSent = dataSent + MSS;
					bytes_notyet_sent = bytes_notyet_sent - MSS;
					seq_num = seq_num + MSS;
				}
				else
				{
					ack_num++;
					segmentSend.hdr.seq_num = seq_num;
					segmentSend.hdr.ack_num = ack_num;
					segmentSend.hdr.flag = 0x0000;
					sendto(socket_fd,&segmentSend, sizeof(segmentSend), 0,(struct sockaddr*)&client_addr, length);
					ack_num_readytorecv++;
					printf("	Send a packet at %d bytes\n",dataSent+1);
					dataSent = dataSent + cwnd;
					bytes_notyet_sent = bytes_notyet_sent - cwnd;
					seq_num = seq_num + cwnd;
				}
			}
			while(ack_num_readytorecv>0)
			{
				recvfrom(socket_fd, &segmentRecv, MSS, 0, (struct sockaddr*)&client_addr, (socklen_t *)&length);
				if((segmentRecv.hdr.flag>>4)%2==1)
				{
					printf("	Receive a packet (seq_num = %d ,ack_num = %d)\n",segmentRecv.hdr.seq_num,segmentRecv.hdr.ack_num);
					ack_num_readytorecv = ack_num_readytorecv - 1;
					if(segmentRecv.hdr.ack_num == prev_ack_num)
					{
						dup_ack_num++;
						if(dup_ack_num == 3)
						{
							printf("Receive three duplicate ACKs.\n");
							printf("*****Fast transmission*****\n");
							dataSent = prev_ack_num - 1;
							seq_num = prev_ack_num;
							ack_num = segmentRecv.hdr.seq_num;
							THRESHOLD = cwnd / 2;
							goto startToFastTransmit;
						}
					}
				}
				else
				{
					printf("ACK segment lost\n");
					goto startToBeginServer;
				}
				prev_ack_num = segmentRecv.hdr.ack_num;
			}
		}
		printf("*****The file transmission finished.*****\n");
		//======== end of data transmission ===================
	
		printf("=====Start the four-way handshake=====\n");
		seq_num = rand()%9000+1000;
		ack_num = ack_num +1;
		segmentSend.hdr.seq_num = seq_num;
		segmentSend.hdr.ack_num = ack_num;
		segmentSend.hdr.flag = 0x0001;
		sendto(socket_fd, &segmentSend, sizeof(segmentSend), 0,(struct sockaddr*)&client_addr, length);
		printf("Send a packet (FIN) to %s : %d\n", inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port));
		recvfrom(socket_fd, &segmentRecv, MSS, 0, (struct sockaddr*)&client_addr, (socklen_t *)&length);
		if(segmentRecv.hdr.flag == 0x0010 )
		{
			printf("Received a packet (ACK) from %s : %d\n", inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port));
			printf("	Received a packet(seq_num = %d ,ack_num = %d)\n",segmentRecv.hdr.seq_num,segmentRecv.hdr.ack_num);
			recvfrom(socket_fd, &segmentRecv, MSS, 0, (struct sockaddr*)&client_addr, (socklen_t *)&length);
		}
		else
		{
			printf("Four-way handshake fail.Segment format error.\n");
			goto startToBeginServer;
		}
		if(segmentRecv.hdr.flag == 0x0001)
		{
			printf("Received a packet (FIN) from %s : %d\n", inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port));
			printf("	Received a packet(seq_num = %d ,ack_num = %d)\n",segmentRecv.hdr.seq_num,segmentRecv.hdr.ack_num);
		}
		else
		{
			printf("Four-way handshake fail.Segment format error.\n");
			goto startToBeginServer;
		}
		
		seq_num++;
		ack_num = segmentRecv.hdr.seq_num+1;
		segmentSend.hdr.seq_num = seq_num;
		segmentSend.hdr.ack_num = ack_num;
		segmentSend.hdr.flag = 0x0010;
		sendto(socket_fd, &segmentSend, sizeof(segmentSend), 0,(struct sockaddr*)&client_addr, length);
		printf("Send a packet (ACK) to %s : %d\n", inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port));
		printf("=====Complete the four-way handshake=====\n");
	}
}
void clientSend()
{
        int i;     /* loops through user name */
        int length;    /* length of message */
        unsigned int size;    /* the length of servaddr */
        struct hostent *hp;   /* holds IP address of server */
        struct sockaddr_in servaddr; /* the server's full addr */
		char IP[40],portS[40];
		int seq_num=0,ack_num=0;
		int port,startRecv=0,cwnd=0;
		int datatransmit_ack_count;
		int congestion_avoid = 0;
		int data_transmit_fin = 0;
		int bytes_notyet_recv;
		struct segment segmentRecv;
		struct segment segmentSend;
		cin >> IP;
		cin >> portS;
		port = atoi(portS);
		pthread_cancel(listen_t);

        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(port);
        hp = gethostbyname(IP);
        bcopy(hp->h_addr_list[0], (caddr_t)&servaddr.sin_addr,hp->h_length);
        size = sizeof(servaddr);
		
		//========================== three way handshake ============================
		srand(time(NULL));
		seq_num = rand()%9000+1000;
		ack_num = 0;
		segmentSend.hdr.sourcePort = (int16_t)selfPort;
		segmentSend.hdr.destPort = (int16_t)port;
		segmentSend.hdr.seq_num = seq_num;
		segmentSend.hdr.ack_num = ack_num;
		segmentSend.hdr.flag = 0x0002;	//SYN=1
		sendto(socket_fd, &segmentSend, sizeof(segmentSend), 0, (struct sockaddr*)&servaddr, size);
		printf("=====Start the three-way handshake=====\n");
		printf("Send a packet(SYN) to %s : %d\n",inet_ntoa(servaddr.sin_addr), htons(servaddr.sin_port));
        recvfrom(socket_fd, &segmentRecv, MSS, 0, (struct sockaddr*)&servaddr, &size);
		if(((segmentRecv.hdr.flag>>1) % 2==1) && ((segmentRecv.hdr.flag>>4) % 2==1))
		{
			printf("Receive a packet (SYN/ACK) from %s : %d\n",inet_ntoa(servaddr.sin_addr), htons(servaddr.sin_port));
			printf("	Received a packet(seq_num = %d ,ack_num = %d)\n",segmentRecv.hdr.seq_num,segmentRecv.hdr.ack_num);
			sleep(1);
			seq_num++;
			ack_num = segmentRecv.hdr.seq_num + 1;
			segmentSend.hdr.seq_num = seq_num;
			segmentSend.hdr.ack_num = ack_num;
			segmentSend.hdr.flag = 0x0010;
	        sendto(socket_fd, &segmentSend, sizeof(segmentSend), 0, (struct sockaddr*)&servaddr, size);
			printf("Send a packet(ACK) to %s : %d\n",inet_ntoa(servaddr.sin_addr), htons(servaddr.sin_port));
			printf("=====Complete the three-way handshake=====\n");
		}
		else if(segmentRecv.hdr.flag>>2 %2==1)
		{
			printf("Three-way hankshake fail.\n");
			exit(0);
		}
		//==================== end of three way handshake ===========================

		//=================== data transmission =================
		
		data_transmit_fin = 0;
		ack_num = 1;
		startToFastTransmit:;
		cwnd = 0;
		congestion_avoid = 0;
		while(1)
		{
			if(cwnd==0)
				cwnd = 1;
			else
			{
				if(congestion_avoid==0)
					cwnd = cwnd *2;
				else if(congestion_avoid==1)
					cwnd = cwnd + MSS;
				if(cwnd==THRESHOLD)
				{
					congestion_avoid = 1;
				}
				
			}
			bytes_notyet_recv = cwnd;
			while(bytes_notyet_recv > 0)
			{
				if(congestion_avoid == 1)
				{
					recvfrom(socket_fd, &segmentRecv, MSS, 0, (struct sockaddr*)&servaddr, &size);
					if(segmentRecv.hdr.seq_num > ack_num)
					{
						printf("	Receive a packet (seq_num = %d , ack_num = %d)\n",segmentRecv.hdr.seq_num,segmentRecv.hdr.ack_num);
						seq_num++;
						segmentSend.hdr.seq_num = seq_num;
						segmentSend.hdr.ack_num = ack_num;
						segmentSend.hdr.flag = 0x0010;	//ACK=1
						sendto(socket_fd, &segmentSend, sizeof(segmentSend), 0, (struct sockaddr*)&servaddr, size);
						goto startToFastTransmit;
					}
					else
					{
						bytes_notyet_recv = bytes_notyet_recv - MSS;
						datatransmit_ack_count++;
						if(segmentRecv.hdr.flag == 0x0001)
						{
							data_transmit_fin = 1;
							break;
						}
						if(startRecv==0)
						{
							printf("Receive a file from %s : %d\n",inet_ntoa(servaddr.sin_addr), htons(servaddr.sin_port));
							startRecv = 1;
						}
						printf("	Receive a packet (seq_num = %d , ack_num = %d)\n",segmentRecv.hdr.seq_num,segmentRecv.hdr.ack_num);
						seq_num++;
						if(cwnd > MSS)
							ack_num = ack_num + MSS;
						else
							ack_num = ack_num + cwnd;
						segmentSend.hdr.seq_num = seq_num;
						segmentSend.hdr.ack_num = ack_num;
						segmentSend.hdr.flag = 0x0010;	//ACK=1
						sendto(socket_fd, &segmentSend, sizeof(segmentSend), 0, (struct sockaddr*)&servaddr, size);
					}
				}
				else
				{
					recvfrom(socket_fd, &segmentRecv, MSS, 0, (struct sockaddr*)&servaddr, &size);
					if(segmentRecv.hdr.seq_num > ack_num)
					{
						printf("	Receive a packet (seq_num = %d , ack_num = %d)\n",segmentRecv.hdr.seq_num,segmentRecv.hdr.ack_num);
						seq_num++;
						segmentSend.hdr.seq_num = seq_num;
						segmentSend.hdr.ack_num = ack_num;
						segmentSend.hdr.flag = 0x0010;	//ACK=1
						sendto(socket_fd, &segmentSend, sizeof(segmentSend), 0, (struct sockaddr*)&servaddr, size);
						goto startToFastTransmit;
					}
					else
					{
						bytes_notyet_recv = bytes_notyet_recv - MSS;
						datatransmit_ack_count++;
						if(segmentRecv.hdr.flag == 0x0001)
						{
							data_transmit_fin = 1;
							break;
						}
						if(startRecv==0)
						{
							printf("Receive a file from %s : %d\n",inet_ntoa(servaddr.sin_addr), htons(servaddr.sin_port));
							startRecv = 1;
						}
						printf("	Receive a packet (seq_num = %d , ack_num = %d)\n",segmentRecv.hdr.seq_num,segmentRecv.hdr.ack_num);
						seq_num++;
						if(cwnd > MSS)
							ack_num = ack_num + MSS;
						else
							ack_num = ack_num + cwnd;
						segmentSend.hdr.seq_num = seq_num;
						segmentSend.hdr.ack_num = ack_num;
						segmentSend.hdr.flag = 0x0010;	//ACK=1
						sendto(socket_fd, &segmentSend, sizeof(segmentSend), 0, (struct sockaddr*)&servaddr, size);
					}
				}
				
			}
			if(data_transmit_fin == 1)
				break;
		}
		//============== end of data transmission ===============
		
		printf("=====Start four-way handshake=====\n");
		printf("Receive a packet (FIN) from %s : %d\n",inet_ntoa(servaddr.sin_addr), htons(servaddr.sin_port));
		printf("	Received a packet(seq_num = %d ,ack_num = %d)\n",segmentRecv.hdr.seq_num,segmentRecv.hdr.ack_num);
		seq_num++;
		ack_num = segmentRecv.hdr.seq_num + 1;
		segmentSend.hdr.seq_num = seq_num;
		segmentSend.hdr.ack_num = ack_num;
		segmentSend.hdr.flag = 0x0010;	//ACK=1
        sendto(socket_fd, &segmentSend, sizeof(segmentSend), 0, (struct sockaddr*)&servaddr, size);
		printf("Send a packet(ACK) to %s : %d\n",inet_ntoa(servaddr.sin_addr), htons(servaddr.sin_port));
		seq_num++;
		segmentSend.hdr.seq_num = seq_num;
		segmentSend.hdr.ack_num = ack_num;
		segmentSend.hdr.flag = 0x0001;	//FIN=1
		sendto(socket_fd, &segmentSend, sizeof(segmentSend), 0, (struct sockaddr*)&servaddr, size);
		printf("Send a packet(FIN) to %s : %d\n",inet_ntoa(servaddr.sin_addr), htons(servaddr.sin_port));
        recvfrom(socket_fd, &segmentRecv, MSS, 0, (struct sockaddr*)&servaddr, &size);
		if(segmentRecv.hdr.flag == 0x0010)
		{
			printf("Receive a packet (ACK) from %s : %d\n",inet_ntoa(servaddr.sin_addr), htons(servaddr.sin_port));
			printf("	Received a packet(seq_num = %d ,ack_num = %d)\n",segmentRecv.hdr.seq_num,segmentRecv.hdr.ack_num);
			printf("===== Complete the four-way handshake\n");
		}
		else
		{
			printf("Four-way handshake fail.Segment format error.\n");
		}
}

void *thread(void *op)
{
		int *a = (int *)op;
		if(*a==SERVER)
		{
			serverListen();
		}
		else if(*a==CLIENT)
		{
			clientSend();
		}
		exit(0);
}
int main(int argc, char **argv)
{	
		selfPort = atoi(argv[1]);
		int ret,ret2;
		int *op1 = (int*)malloc(sizeof(int));
		*op1 = 0;
		int *op2 = (int*)malloc(sizeof(int));
		*op2 = 1;

		//initialize the socket
		socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        bzero((char *)&myaddr, sizeof(myaddr));
        myaddr.sin_family = AF_INET;
        myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        myaddr.sin_port = htons(selfPort);
		bind(socket_fd, (struct sockaddr *)&myaddr,sizeof(myaddr));
		//=====================
		
		ret=pthread_create(&listen_t,NULL,thread,(void*) op1);
		ret2=pthread_create(&scan_t,NULL,thread,(void*) op2);
		pthread_join(listen_t,NULL);
		pthread_join(scan_t,NULL);
		return 0;
}
