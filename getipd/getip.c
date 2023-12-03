#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#ifdef WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define closesocket(s) close(s)
#endif


#define MAX_WAIT_TIMEOUT 10  // seconds

#define MYPORT 8198    // the port users will be connecting to

#define BACKLOG 10     // how many pending connections queue will hold

#define BUF_SIZE 512

int fd_A[BACKLOG];    // accepted connection fd
time_t fd_A_time[BACKLOG]; // accepted connection fd time

int conn_amount = 0;    // current connection amount

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
char g_log_file[MAX_PATH];

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
	memcpy(g_log_file, g_app_dir, g_app_dir_len);
	strcpy(g_log_file + g_app_dir_len, "getipd.log");
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

int b_daemon = 0;
int b_listClients = 0;

void showClients()
{
	int i;
	
	if (b_daemon || !b_listClients)
		return;
		
	printf("client amount: %d\n", conn_amount);
	for (i = 0; i < BACKLOG; i++) {
		printf("[%d]:%d  ", i, fd_A[i]);
	}
	printf("\n\n");
}

void saveOverflowLog(const char * clientIp)
{
	FILE * fp;
	
	fp = fopen(g_log_file, "wb");
	if (fp) {
		time_t t = time(NULL);
		struct tm * local = localtime(&t);
		char buf[1024];
		int len;
		
		len = sprintf(buf, "[%d-%d-%d %d:%d:%d] client IP:%s\n", local->tm_year+1900, local->tm_mon+1, local->tm_mday, 
												 local->tm_hour, local->tm_min, local->tm_sec,
												 clientIp);
		
		fwrite(buf,1, len, fp);
		
		fclose(fp);
	}
}

int main(int argc,char * argv[])
{
	int sock_fd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct sockaddr_in server_addr;    // server address information
	struct sockaddr_in client_addr; // connector's address information
	socklen_t sin_size;
	int yes = 1;
	char buf[BUF_SIZE];
	char ipAddr[128];
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
		else if (!strcmp(argv[i],"-l")) {
			b_listClients = 1;
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

	if (!b_daemon) {
		printf("listen port %d\n", MYPORT);
	}
	
	conn_amount = 0;
	sin_size = sizeof(client_addr);
	maxsock = sock_fd;

	memset(fd_A,0,sizeof (fd_A));
	
	while (1) {
		// timeout setting
		tv.tv_sec = MAX_WAIT_TIMEOUT;
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
			if (!b_daemon) {
				printf("select %d sockets timeout\n", conn_amount);
			}
			if (conn_amount == 0)
				continue;
		}

		remove_count = 0;
		
		// check every fd in the set
		for (i = 0; i < conn_amount; i++) {
			if (FD_ISSET(fd_A[i], &fdsr)) {
				ret = recv(fd_A[i], buf, sizeof(buf), 0);
				if (ret <= 0) {        // client close
					if (!b_daemon) {
						printf("client[%d] close\n", i);
					}
					closesocket(fd_A[i]);
					FD_CLR(fd_A[i], &fdsr);
					fd_A[i] = 0;
					remove_count++;
				} else {        // receive data
					int ret2;
					
					if (ret < BUF_SIZE)
						memset(&buf[ret], '\0', 1);
					if (!b_daemon) {
						printf("client[%d] send:%s\n", i, buf);
					}

					sin_size = sizeof(client_addr);

					ret2 = getpeername(fd_A[i],(struct sockaddr *)&client_addr, &sin_size);

					if (ret2 == 0) {
						if (client_addr.sin_family == AF_INET) {
							int len = sprintf(buf,"{\"ip\":\"%s\"}", inet_ntop(AF_INET, &client_addr.sin_addr, ipAddr, sizeof(ipAddr)));

							send(fd_A[i], buf, len, 0);

							remove_count++;

							closesocket(fd_A[i]);

							FD_CLR(fd_A[i], &fdsr);
							fd_A[i] = 0;
						}
					}
					else {
						printf("getpeername failed: %d\n", ret2);
					}
				}
			}
		}
		
		//check client timeout
		if (conn_amount > 0) {
			time_t now = time(NULL);
			
			for (i = 0; i < conn_amount; i++) {
				if (fd_A[i]) {
					time_t time_out = now - fd_A_time[i];
					
					if (time_out > MAX_WAIT_TIMEOUT) {
						
						if (!b_daemon) {
							
							struct tm * local = localtime(&fd_A_time[i]);
							char time_buf[64];
							int len;
							
							len = sprintf(time_buf, "%d-%d-%d %d:%d:%d", local->tm_year+1900, local->tm_mon+1, local->tm_mday, 
																	 local->tm_hour, local->tm_min, local->tm_sec);
												 
							sin_size = sizeof(client_addr);

							getpeername(fd_A[i],(struct sockaddr *)&client_addr, &sin_size);
							inet_ntop(AF_INET, &client_addr.sin_addr, ipAddr, sizeof(ipAddr));
							
					
							printf("client[%d]:%d %s from: %s timeout:%d seconds\n", i, fd_A[i], ipAddr, time_buf, (int) time_out);
						}
						
						send(fd_A[i], "timeout", 7, 0);
						closesocket(fd_A[i]);
						
						fd_A[i] = 0;
						
						remove_count++;
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
					fd_A_time[j] = fd_A_time[i];
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
			char ipAddr[128];
			const char * sIpAddr;
			
			new_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &sin_size);
			if (new_fd <= 0) {
				perror("accept");
				continue;
			}

			sIpAddr = inet_ntop(AF_INET, &client_addr.sin_addr, ipAddr, sizeof(ipAddr));
			// add to fd queue
			if (conn_amount < BACKLOG) {

				fd_A_time[conn_amount] = time(NULL);
				fd_A[conn_amount++] = new_fd;

				if (!b_daemon) {
					printf("new connection client[%d] %s:%d\n", conn_amount, sIpAddr, ntohs(client_addr.sin_port));
				}
				if (new_fd > maxsock)
					maxsock = new_fd;
			}
			else {
				saveOverflowLog(sIpAddr);
				
				if (!b_daemon) {
					printf("max connections arrived, close the client\n");
				}
				send(new_fd, "bye", 3, 0);
				closesocket(new_fd);
			}
		}
				
		showClients();
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
