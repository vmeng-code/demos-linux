//#define _GNU_SOURCE
#include<sys/uio.h> // for vmsplice(2), struct iovec
#include<sys/types.h> // for socket(2), bind(2), open(2), listen(2), accept(2), recv(2), setsockopt(2)
#include<sys/socket.h> // for socket(2), bind(2), listen(2), accept(2), recv(2), setsockopt(2), inet_addr(3)
#include<strings.h> // for bzero(3)
#include<stdio.h> // for perror(3), printf(3), atoi(3)
#include<errno.h> // for errno
#include<netdb.h> // for getservbyname(3)
#include<arpa/inet.h> // for ntohs(3)
#include<sys/stat.h> // for open(2)
#include<fcntl.h> // for open(2), vmsplice(2), splice(2)
#include<unistd.h> // for read(2), close(2), pipe(2)
#include<pthread.h> // for pthread_create(3)
#include<netinet/in.h> // for sockaddr_in, inet_addr(3)
#include<arpa/inet.h> // for inet_addr(3)
#include<sys/mman.h> // for mmap(2)
#include<stdlib.h> // for rand(3)
#include<assert.h> // for assert(3)

#include<us_helper.h> // our own helper

/*
 * This is an example of using vmsplice to send mucho data to clients.
 *
 * EXTRA_LIBS=-lpthread
 */

int get_backlog() {
	// read the data from the /proc/sys/net/core/somaxconn virtual file...
	const char* filename = "/proc/sys/net/core/somaxconn";
	const unsigned int size = 256;
	char buf[size];
	int fd;
	CHECK_NOT_M1(fd = open(filename, O_RDONLY));
	CHECK_NOT_M1(read(fd, buf, size));
	CHECK_NOT_M1(close(fd));
	return atoi(buf);
}

void *worker(void* arg) {
	int fd=*((int*)arg);
	TRACE("thread %d starting",gettid());
	TRACE("thread %d got fd %d",gettid(),fd);
	const unsigned int buflen=1024;
	char buff[buflen];
	char prbuff[buflen+1];
	ssize_t res;
	CHECK_NOT_M1(res=recv(fd,buff,buflen,0));
	snprintf(prbuff,res+1,"%s",buff);
	TRACE("thread %d received %s",gettid(),prbuff);
	// lets emulate some work
	// 1. allocate memory (via anonymous mmap(2)).
	unsigned int mysize=getpagesize()*100;
	void* mypointer;
	CHECK_NOT_VOIDP(mypointer=mmap(NULL,mysize,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0),MAP_FAILED);
	// 2. fill it with data.
	int* p=(int*)mypointer;
	for(unsigned int i=0;i<mysize/sizeof(int);i++) {
		p[i]=rand();
	}
	// 3. vmsplice it
	int mypipe[2];
	CHECK_NOT_M1(pipe(mypipe));
	/*
	struct iovec myiovec={
		.iov_base=mypointer,
		.iov_len=mysize,
	};
	*/
	struct iovec myiovec;
	myiovec.iov_base=mypointer;
	myiovec.iov_len=mysize;
	int bytes;
	char* pp=(char*)mypointer;
	while((bytes=vmsplice(mypipe[1],&myiovec,1,SPLICE_F_GIFT| SPLICE_F_MOVE))>0) {
		myiovec.iov_len-=bytes;
		pp+=bytes;
		myiovec.iov_base=(void*)pp;
		int todo=bytes;
		int ret;
		while((ret=splice(mypipe[0],NULL,fd,NULL,todo,SPLICE_F_MOVE)>0)) {
			todo-=ret;
		}
		assert(ret==0);
	}
	if(bytes<0) {
		perror("error has occured");
		exit(EXIT_FAILURE);
	}
	CHECK_NOT_M1(close(fd));
	TRACE("thread %d ending",gettid());
	return NULL;
}

int main(int argc,char** argv, char** envp) {
	//ssize_t datalen;
	//socklen_t fromaddrlen;
	//time_t t;
	//char ibuffer[1000], obuffer[1000];
	//
	if(argc!=3) {
		fprintf(stderr,"usage: %s [host] [port]\n",argv[0]);
		exit(EXIT_FAILURE);
	}
	const char* host=argv[1];
	const unsigned int port=atoi(argv[2]);
	printf("contact me at host %s port %d\n",host,port);

	// lets get the port number using getservbyname(3)
	//struct servent* p_servent;
	//CHECK_NOT_NULL(p_servent=getservbyname(serv_name,serv_proto));
	//print_servent(p_servent);

	// lets open the socket
	int sockfd;
	CHECK_NOT_M1(sockfd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP));
	printf("opened socket with sockfd %d\n",sockfd);

	// lets make the socket reusable
	int optval=1;
	CHECK_NOT_M1(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval)));
	printf("setsockopt was ok\n");

	// lets create the address
	struct sockaddr_in server;
	bzero(&server, sizeof(server));
	server.sin_family=AF_INET;
	//server.sin_addr.s_addr=inet_addr(host);
	server.sin_addr.s_addr=INADDR_ANY;
	//server.sin_addr.s_addr=0xff000000;
	//server.sin_port=p_servent->s_port;
	server.sin_port=htons(port);

	// lets bind to the socket to the address
	CHECK_NOT_M1(bind(sockfd,(struct sockaddr *)&server, sizeof(server)));
	printf("bind was successful\n");

	// lets listen in...
	int backlog=get_backlog();
	printf("backlog is %d\n",backlog);
	CHECK_NOT_M1(listen(sockfd,backlog));
	printf("listen was successful\n");

	while(true) {
		int fd;
		struct sockaddr_in client;
		socklen_t addrlen;
		CHECK_NOT_M1(fd=accept(sockfd,(struct sockaddr *)&client,&addrlen));
		printf("accepted fd %d\n",fd);
		// spawn a thread to handle the connection to that client...
		pthread_t thread;
		int* p=new int(fd);
		CHECK_ZERO(pthread_create(&thread, NULL, worker, p));
	}
	return EXIT_SUCCESS;
}
