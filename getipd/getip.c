#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#ifdef WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define closesocket(s) close(s)
#endif


#define MYPORT 8198    // the port users will be connecting to

#define BACKLOG 5     // how many pending connections queue will hold

#define BUF_SIZE 512

int fd_A[BACKLOG];    // accepted connection fd
int conn_amount = 0;    // current connection amount

void showclient()
{
	int i;
	printf("client amount: %d\n", conn_amount);
	for (i = 0; i < BACKLOG; i++) {
		printf("[%d]:%d  ", i, fd_A[i]);
	}
	printf("\n\n");
}

const char * g_app_dir	= NULL;
const char * g_exe_name = "getipd";

volatile long g_b_exit_server = 0;
int		g_web_root_len = 4;
int		g_app_dir_len = 0;

#ifdef _WIN32
#define PATH_DEL '\\'
#define PTHREAD_INITIALIZED {0,0}
#else
#define PATH_DEL '/'
#define PTHREAD_INITIALIZED 0
#endif


#undef MAX_PATH 
#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

char g_cur_exe_path[MAX_PATH];

void getExePath()
{
	char * pch;
#ifdef _WIN32
	GetModuleFileNameA(NULL,g_cur_exe_path,ARRAYSIZE(g_cur_exe_path));
	pch = strrchr(g_cur_exe_path,'\\');
	pch ++ ;
	*pch = '\0';
#else
	int cnt = readlink("/proc/self/exe", g_cur_exe_path, MAX_PATH);  
	if (cnt < 0 || cnt >= MAX_PATH) {  
		strcpy(g_cur_exe_path,"/usr/local/");
	}
	pch = strrchr(g_cur_exe_path,'/');
	if (pch) {
		pch ++;
		*pch = 0;
	}
#endif
	g_app_dir = g_cur_exe_path;
	g_app_dir_len = strlen(g_app_dir);
}

#ifdef _WIN32
void init_daemon() {};
#else

#ifndef NOFILE 
#define NOFILE 3 
#endif

void init_daemon()
{
	int pid;
	int i;
	pid=fork();
	if(pid<0)    
		exit(1);
	else if(pid>0)
		exit(0);

	setsid();
	pid=fork();
	if(pid>0)
		exit(0);
	else if(pid<0)    
		exit(1);

	for(i=0;i<NOFILE;i++)
		close(i);
	chdir(g_cur_exe_path);
	umask(0);
	return;
}
#endif

#ifndef WIN32
#define DAEMON_HINT "\t-d Running as a daemon process.\n"
#else
#define DAEMON_HINT ""
#endif

void Usage() 
{
	const char *progname = "getipd";
	const char *debug = "";
#ifdef WIN32
	debug = "-debug ";
#endif
	fprintf(stderr,"Usage:\n%s %s-r <directory> -p <port> -l <logfile> -t <TemplateFileDir> -c <ConfigurationFile> \n"
		"Parameters:\n"
		"\t-p tcp port,default: [%s]\n" DAEMON_HINT,
		progname,debug,MYPORT
		);

	exit(1);
}

int main(int argc,char * argv[])
{
	int sock_fd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct sockaddr_in server_addr;    // server address information
	struct sockaddr_in client_addr; // connector's address information
	socklen_t sin_size;
	int yes = 1,b_daemon = 0;
	char buf[BUF_SIZE];
	int ret;
	int i;
	
	fd_set fdsr;
	int maxsock,remove_count;
	struct timeval tv;
	unsigned short port = MYPORT;

#ifdef WIN32
	WSADATA wsaData;
	WORD wVersionRequested;

	wVersionRequested =MAKEWORD( 1, 1 );
	ret = WSAStartup( wVersionRequested, &wsaData );
	if ( ret != 0 ) {
		/* Tell the user that we couldn't find a useable */
		/* winsock.dll. */
		exit(1);
	}
#endif

	getExePath();

	g_exe_name = strrchr(argv[0],PATH_DEL);

	if (g_exe_name) {
		g_exe_name ++;
	}
	else {
		g_exe_name = argv[0];
	}
	
	/* Parse command line arguments */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-p") == 0) {
			port = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i],"-d")) {
			b_daemon = 1;
			break;
		}
		else if (!strcmp(argv[i],"-?") || !strcmp(argv[i],"-h")) {
			Usage();
			break;
		}
	}
	
	if (b_daemon) {
		init_daemon();
	}
	
	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	server_addr.sin_family = AF_INET;         // host byte order
	server_addr.sin_port = htons(MYPORT);     // short, network byte order
	server_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
	memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));

	if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(sock_fd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	printf("listen port %d\n", MYPORT);
	
	conn_amount = 0;
	sin_size = sizeof(client_addr);
	maxsock = sock_fd;

	memset(fd_A,0,sizeof (fd_A));
	
	while (1) {
		// timeout setting
		tv.tv_sec = 30;
		tv.tv_usec = 0;

		// initialize file descriptor set
		FD_ZERO(&fdsr);
		FD_SET(sock_fd, &fdsr);

		// add active connection to fd set
		for (i = 0; i < BACKLOG; i++) {
			if (fd_A[i] != 0) {
				FD_SET(fd_A[i], &fdsr);
			}
		}

		ret = select(maxsock + 1, &fdsr, NULL, NULL, &tv);
		if (ret < 0) {
			perror("select");
			break;
		} else if (ret == 0) {
			printf("timeout\n");
			continue;
		}

		remove_count = 0;

		// check every fd in the set
		for (i = 0; i < conn_amount; i++) {
			if (FD_ISSET(fd_A[i], &fdsr)) {
				ret = recv(fd_A[i], buf, sizeof(buf), 0);
				if (ret <= 0) {        // client close
					printf("client[%d] close\n", i);
					closesocket(fd_A[i]);
					FD_CLR(fd_A[i], &fdsr);
					fd_A[i] = 0;
					remove_count ++;
				} else {        // receive data
					int ret2;
					char ipAddr[128];

					if (ret < BUF_SIZE)
						memset(&buf[ret], '\0', 1);
					printf("client[%d] send:%s\n", i, buf);

					sin_size = sizeof(client_addr);

					ret2 = getpeername(fd_A[i],(struct sockaddr *)&client_addr, &sin_size);

					if (ret2 == 0) {
						int len = sprintf(buf,"{\"ip\":\"%s\"}", inet_ntop(AF_INET, &client_addr.sin_addr, ipAddr, sizeof(ipAddr)));

						send(fd_A[i], buf, len, 0);

						remove_count ++;

						closesocket(fd_A[i]);

						FD_CLR(fd_A[i], &fdsr);
						fd_A[i] = 0;
					}
				}
			}
		}
		//resort the socket
		if (remove_count > 0) {
			int j=0;
			for (i = 0; i < conn_amount; i++) {
				if (fd_A[i]) {
					fd_A[j] = fd_A[i];
					j++;
				}
			}

			for (i = j; i < conn_amount; i++) {
				fd_A[i] = 0;
			}

			conn_amount -= remove_count;
		}


		// check whether a new connection comes
		if (FD_ISSET(sock_fd, &fdsr)) {
			new_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &sin_size);
			if (new_fd <= 0) {
				perror("accept");
				continue;
			}

			// add to fd queue
			if (conn_amount < BACKLOG) {
				char ipAddr[128];

				fd_A[conn_amount++] = new_fd;

				printf("new connection client[%d] %s:%d\n", conn_amount,
					inet_ntop(AF_INET, &client_addr.sin_addr, ipAddr, sizeof(ipAddr)), ntohs(client_addr.sin_port));
				if (new_fd > maxsock)
					maxsock = new_fd;
			}
			else {
				printf("max connections arrived, exit\n");
				send(new_fd, "bye", 3, 0);
				closesocket(new_fd);
				break;
			}
		}
				
		showclient();
	}

	// close other connections
	for (i = 0; i < conn_amount; i++) {
		if (fd_A[i] != 0) {
			closesocket(fd_A[i]);
		}
	}

#ifdef WIN32
	WSACleanup();
#endif
	exit(0);
}
