#include <stdio.h>
#include <thread>
#include <mutex>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>

#pragma comment(lib,"ws2_32.lib") //Winsock Library

#define BUFFLEN 512	// Max length of buffer
#define PORT 8888	// The port on which to listen for incoming data
#define MAX_CLIENTS 16 // Max # of clients that can be connected at once

SOCKET sock;
struct sockaddr_in server, si_other;
int slen, recv_len;
char buff[BUFFLEN];
char message[BUFFLEN];
WSADATA wsa;

int num_clients;
std::mutex m;
bool is_client_connected[MAX_CLIENTS];
std::thread client_threads[MAX_CLIENTS];
struct sockaddr_in client_addresses[MAX_CLIENTS];

int FindFreeClientIndex()
{
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (!is_client_connected[i])
		{
			return i;
		}
	}
	return -1;
}

int FindExistingClientIndex(const struct sockaddr_in& address)
{
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (is_client_connected[i] &&
			strcmp(inet_ntoa(client_addresses[i].sin_addr), inet_ntoa(address.sin_addr)))
		{
			return i;
		}
	}
	return -1;
}

bool IsClientConnected(int clientIndex)
{
	return is_client_connected[clientIndex];
}

const sockaddr_in& GetClientAddress(int clientIndex)
{
	return client_addresses[clientIndex];
}

void HandleClientSockets(const struct sockaddr_in& address, char* buff)
{
	auto lock = std::unique_lock<std::mutex>(m);
	//print details of the client/peer and the data received
	printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
	printf("Data: %s\n", buff);

	//now reply the client with the same data
	if (sendto(sock, buff, recv_len, 0, (struct sockaddr*) &si_other, slen) == SOCKET_ERROR)
	{
		printf("sendto() failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	lock.unlock();
}

int main()
{
	slen = sizeof(si_other);

	//Initialise winsock
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	printf("Initialised.\n");

	//Create a socket
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		printf("Could not create socket : %d", WSAGetLastError());
	}
	printf("Socket created.\n");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PORT);

	//Bind
	if (bind(sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR)
	{
		printf("Bind failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	puts("Bind done");

	//keep listening for data
	while (1)
	{
		printf("Waiting for data...");
		fflush(stdout);

		//clear the buffer by filling null, it might have previously received data
		memset(buff, '\0', BUFFLEN);

		//try to receive some data, this is a blocking call
		if ((recv_len = recvfrom(sock, buff, BUFFLEN, 0, (struct sockaddr *) &si_other, &slen)) == SOCKET_ERROR)
		{
			printf("recvfrom() failed with error code : %d", WSAGetLastError());
			exit(EXIT_FAILURE);
		}

		// Check if client is a new connection
		int clientIndex = FindExistingClientIndex(si_other);
		if (clientIndex == -1)
		{
			// Check if a free connection slot is available
			clientIndex = FindFreeClientIndex();
			if (clientIndex > -1)
			{
				client_addresses[clientIndex] = si_other;
				client_threads[clientIndex] = std::thread(HandleClientSockets, client_addresses[clientIndex], buff);
			}
		}
		else
		{
			client_threads[clientIndex].join();
			client_threads[clientIndex] = std::thread(HandleClientSockets, client_addresses[clientIndex], buff);
		}
	}

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		client_threads[i].join();
	}

	closesocket(sock);
	WSACleanup();

	return 0;
}