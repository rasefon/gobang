// gobang.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include "Board.h"

using namespace std;

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 16 
#define DEFAULT_PORT "27015"

#define SEND_DATA(len) iSendResult=::send(client_socket,recvbuf,len,0); \
   if(iSendResult==SOCKET_ERROR){\
      cout<<"send failed with error"<<WSAGetLastError()<<endl;\
      closesocket(client_socket);\
      WSACleanup();\
      return 1;\
   }\


static bool s_is_gaming = false;

int _tmain(int argc, _TCHAR* argv[])
{
   Board::compute_failure_function();

   WSADATA wsaData;
   int iResult;

   SOCKET listen_socket = INVALID_SOCKET;
   SOCKET client_socket = INVALID_SOCKET;

   struct addrinfo *result = NULL;
   struct addrinfo hints;

   int iSendResult;
   char recvbuf[DEFAULT_BUFLEN];
   int recvbuflen = DEFAULT_BUFLEN;

   // Initialize Winsock
   iResult = ::WSAStartup(MAKEWORD(2,2), &wsaData);
   if (iResult != 0) {
      cout << "WSAStartup failed with error: " << iResult << endl;
      return 1;
   }

   ZeroMemory(&hints, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_protocol = IPPROTO_TCP;
   hints.ai_flags = AI_PASSIVE;

   // Resolve the server address and port
   iResult = ::getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
   if ( iResult != 0 ) {
      cout << "getaddrinfo failed with error: " << iResult << endl;
      WSACleanup();
      return 1;
   }

   // Create a SOCKET for connecting to server
   listen_socket = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
   if (listen_socket == INVALID_SOCKET) {
      cout << "socket failed with error: " << WSAGetLastError() <<endl;
      ::freeaddrinfo(result);
      WSACleanup();
      return 1;
   }

   // Setup the TCP listening socket
   iResult = ::bind( listen_socket, result->ai_addr, (int)result->ai_addrlen);
   if (iResult == SOCKET_ERROR) {
      cout << "bind failed with error: " << WSAGetLastError() << endl;
      ::freeaddrinfo(result);
      ::closesocket(listen_socket);
      WSACleanup();
      return 1;
   }

   freeaddrinfo(result);

   iResult = ::listen(listen_socket, SOMAXCONN);
   if (iResult == SOCKET_ERROR) {
      cout << "listen failed with error: " << WSAGetLastError() <<endl;
      closesocket(listen_socket);
      WSACleanup();
      return 1;
   }

   // Accept a client socket
   client_socket = accept(listen_socket, NULL, NULL);
   if (client_socket == INVALID_SOCKET) {
      cout << "accept failed with error: " << WSAGetLastError() << endl;
      closesocket(listen_socket);
      WSACleanup();
      return 1;
   }

   // No longer need server socket
   closesocket(listen_socket);

   Board *bb = nullptr;
   // Receive until the peer shuts down the connection
   int x0, y0;
   int cp_x0, cp_x1, cp_y0, cp_y1;
   do {
      iResult = ::recv(client_socket, recvbuf, recvbuflen, 0);
      if (iResult > 0) {
         int i = 0;
         while (i < iResult && recvbuf[i] != '|') {
            i++;
         }

         if (i == iResult) {
            memset(recvbuf, 0, DEFAULT_BUFLEN);
            strcpy_s(recvbuf, 4, "400");
            SEND_DATA(4);
         }
         else if (0 == i) {
            i++;
            int x = atoi(&recvbuf[i]);

            while(recvbuf[i++]!=',');

            int y = atoi(&recvbuf[i]);
            while(recvbuf[i++]!='|');

            int first_step = atoi(&recvbuf[i]);
            if (first_step == 0) {
               bb->update_grid_status(x,y,'x');
               x0 = x;
               y0 = y;
            }
            else {
               if (bb->is_sibling(x0, y0, x, y)) {
                  memset(recvbuf, 0, DEFAULT_BUFLEN);
                  strcpy_s(recvbuf, 4, "403");
                  SEND_DATA(4);
               }
               else {
                  bb->update_grid_status(x,y,'x');
                  memset(recvbuf, 0, DEFAULT_BUFLEN);
                  strcpy_s(recvbuf, 4, "200");
                  SEND_DATA(4);
               }
            }
         }
         else {
            if (0 == strncmp(recvbuf, "new", i)) {
               if (bb) {
                  delete bb;
               }
               bb = new Board();
               memset(recvbuf, 0, DEFAULT_BUFLEN);
               strcpy_s(recvbuf, 4, "201");
               SEND_DATA(4);
            }
            else if (0 == strncmp(recvbuf, "cp", i)) {
               bb->parallel_alpha_beta_max(DEPTH, -_INFINITE_, _INFINITE_, false);
               TwoSteps& ts = bb->best_steps();
               bb->update_grid_status(ts.step1.i, ts.step1.j, 'o');
               bb->update_grid_status(ts.step2.i, ts.step2.j, 'o');
               cout << "white steps: (" << ts.step1.i <<","<<ts.step1.j<<") ("<<ts.step2.i<<","<<ts.step2.j<<")" <<endl;
               cp_x0 = ts.step1.i;
               cp_y0 = ts.step1.j;
               cp_x1 = ts.step2.i;
               cp_y1 = ts.step2.j;
               stringstream oStream;
               oStream << cp_x0 <<"," << cp_y0 << "," << cp_x1 << "," << cp_y1 << ends;
               string strBuff = oStream.str();
               memset(recvbuf, 0, DEFAULT_BUFLEN);
               strcpy_s(recvbuf, strBuff.size(), strBuff.c_str());
               SEND_DATA(strBuff.size());
            }
            else if (0 == strncmp(recvbuf, "go", i)) {
               if (bb->is_game_over()) {
                  memset(recvbuf, 0, DEFAULT_BUFLEN);
                  if (bb->m_black_win) {
                     strcpy_s(recvbuf, 2, "b");
                  } 
                  else {
                     strcpy_s(recvbuf, 2, "w");
                  }
                  SEND_DATA(2);
               }
               else {
                  memset(recvbuf, 0, DEFAULT_BUFLEN);
                  strcpy_s(recvbuf, 3, "no");
                  SEND_DATA(3);
               } 
            }
         }
      }
      else if (iResult == 0) {
         cout << "Connection closing..." << endl;
      }
      else  {
         cout << "recv failed with error: " << WSAGetLastError() << endl;
         closesocket(client_socket);
         WSACleanup();
         return 1;
      }

   } while (iResult > 0);

   // shutdown the connection since we're done
   iResult = shutdown(client_socket, SD_SEND);
   if (iResult == SOCKET_ERROR) {
      cout << "shutdown failed with error: " << WSAGetLastError();
      closesocket(client_socket);
      WSACleanup();
      return 1;
   }

   // cleanup
   closesocket(client_socket);
   WSACleanup();

   return 0;
}

