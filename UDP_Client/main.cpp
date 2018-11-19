#include <thread>
#include <mutex>
#include <stdio.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>

#pragma comment(lib,"ws2_32.lib") //Winsock Library

#define SERVER "127.0.0.1"	//ip address of udp server
#define BUFFLEN 512	//Max length of buffer
#define PORT 8888	//The port on which to listen for incoming data

// Create client protocol
struct sockaddr_in si_other;
int sock, slen = sizeof(si_other);
char buff[BUFFLEN];
char message[BUFFLEN];
WSADATA wsa;

std::mutex listen_lock;
std::mutex message_lock;
bool is_listening = false;
std::thread* listen_thread;

void ListenForResponse()
{

	//clear the buffer by filling null, it might have previously received data
	memset(buff, '\0', BUFFLEN);
	//try to receive some data, this is a blocking call
	if (recvfrom(sock, buff, BUFFLEN, 0, (struct sockaddr *) &si_other, &slen) == SOCKET_ERROR)
	{
		printf("recvfrom() failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	puts(buff);

	auto lock = std::unique_lock<std::mutex>(listen_lock);
	is_listening = false;
	lock.unlock();
}

int main(void)
{
	//Initialise winsock
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	printf("Initialised.\n");

	//create socket
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
	{
		printf("socket() failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	//setup address structure
	memset((char *)&si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(PORT);
	si_other.sin_addr.S_un.S_addr = inet_addr(SERVER);

	// Setup user input thread
	char io_buff[BUFFLEN];
	bool quit = false;

	auto io_thread = std::thread([&] {
		while (!quit)
		{
			//auto lock = std::unique_lock<std::mutex>(io_lock);
			printf("Enter message : ");
			gets_s(io_buff);
			if (strcmp(io_buff, "quit"))
			{
				quit == true;
			}
			//lock.unlock();

			auto lock = std::unique_lock<std::mutex>(message_lock);
			memcpy(message, io_buff, sizeof(char)*BUFFLEN);
			lock.unlock();
		}
	});

	//start communication
	while (1)
	{
		//send the message
		auto lock = std::unique_lock<std::mutex>(message_lock);
		if (sendto(sock, message, strlen(message), 0, (struct sockaddr *) &si_other, slen) == SOCKET_ERROR)
		{
			printf("sendto() failed with error code : %d", WSAGetLastError());
			exit(EXIT_FAILURE);
		}
		memset(message, '\0', BUFFLEN); // Reset io buffer so we don't send the same message more than once
		lock.unlock();

		if (!is_listening)
		{
			auto lock = std::unique_lock<std::mutex>(listen_lock);
			is_listening = true;
			lock.unlock();
			//receive a reply and print it
			listen_thread = new std::thread(ListenForResponse);
		}
	}

	io_thread.join();
	listen_thread->join();
	delete listen_thread;

	closesocket(sock);
	WSACleanup();

	return 0;
}