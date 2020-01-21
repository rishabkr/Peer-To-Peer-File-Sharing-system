#include<iostream>
#include<fstream>
#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<unistd.h>
#include<fcntl.h>
#include<arpa/inet.h>
#include<sys/syscall.h>
#include<netinet/in.h>
#include<vector>
#include<sys/types.h>
#include<netdb.h>
#include<openssl/sha.h>
#include<sys/stat.h>
#include<pthread.h>
#include<thread>
#include<stdlib.h>
#include<signal.h>
#define PORT 32112
#define BACKLOG 10
#define BUF_MAX 2048
using namespace std;
int client_fd,port_no,n;
struct sockaddr_in remote_server_addr;
struct hostent *server;
typedef struct cli_as_server_args
{
char *ip="127.0.0.1";
int cli_port;
}cli_as_server_args;
vector<string>input_cmds;
string get_hash(int fd)
{
	unsigned char buf[BUF_MAX];
	unsigned char hashbuf[20];
	int bytes;
	char temp[40];
	string hash="";
	string send_hash="";
	while((bytes=read(bytes,buf,sizeof(buf)))>0)
	{
		hash="";
		buf[bytes]='\0';
		SHA1(buf,bytes,hashbuf);
		for(long int i=0;i<20;i++)
		{
			sprintf(temp,"%02x",hashbuf[i]);
			hash+=temp;
		}
		send_hash+=hash.substr(0,20);
	}
	return send_hash;
}
long get_file_size(string fname)
{
FILE *fp=fopen(fname.c_str(),"rb");
fseek(fp,0,SEEK_END);
int sz=ftell(fp);
fclose(fp);
return sz;
}
vector<string> process_string(string cmd,char delim)
{
	vector<string> temp;
	string tok="";
	for(int i=0;i<cmd.length();i++)
	{
		char c=cmd[i];
		if(c==delim)
		{
			temp.push_back(tok);
			tok="";
		}
		else
		{
			tok+=c;
		}
	}
	temp.push_back(tok);
	return temp;
}	
void *c_as_server(void *cli_as_server_arg)
{
	
	int my_fd;
	int nsockfd;
	cli_as_server_args* c_server=(cli_as_server_args*)cli_as_server_arg;
	pthread_t ct_id;
	int my_port;
	int bind_cs;
	int newcli;
	int opt=1;
	my_fd=socket(AF_INET,SOCK_STREAM,0);
	if(setsockopt(my_fd,SOL_SOCKET,(SO_REUSEPORT|SO_REUSEADDR),&opt,sizeof(opt))!=0)
	{
		perror("failure in setting socket options");
		exit(-1);
	}
	socklen_t len;
	struct sockaddr_in new_addr;
	struct sockaddr_in my_addr;
	
	my_addr.sin_family=AF_INET;
	my_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
	my_addr.sin_port=htons(c_server->cli_port);
	cout<<(c_server->cli_port)<<endl;
	bind_cs=bind(my_fd,(struct sockaddr*)&my_addr,sizeof(my_addr));
	if(bind_cs<0)
	{
	perror("Binding Error\n");
	exit(-1);
	}
cout<<"entering listening state"<<endl;
newcli=listen(my_fd,BACKLOG);
if(newcli<0)
{
	perror("Couldn't establish connection to any client \n");
	exit(1);
}
len=sizeof(struct sockaddr_in);
while(1)
{
if((nsockfd=accept(my_fd,(struct sockaddr*)&new_addr,&len))>=0)
{
	cout<<"got a connection on socket number "<<nsockfd<<"\n";
	cout<<"accepted"<<"\n";
	char req_buf[BUF_MAX]={'\0'};
	int req_size=recv(nsockfd,req_buf,sizeof(req_buf),0);
	cout<<"the requested file is "<<req_buf<<"\n";
	FILE* send_file;
	char buffer[BUF_MAX];
	send_file=fopen(req_buf,"rb");
	if(!send_file)
	{
	perror("error opening file \n");
	exit(-1);
	}
	int data_len;
	int send_stat;
	size_t x=sizeof(char);
	while(!feof(send_file)&&(data_len=fread(buffer,x,sizeof(buffer),send_file))>0)
	{
		if(data_len)
		{
		send_stat=send(nsockfd,buffer,data_len,0);
		bzero(buffer,BUF_MAX);
		}
	}
	if(send_stat<0)
	{
	perror("error in sending file\n");
	exit(-1);
	}
	fclose(send_file);
	close(nsockfd);
	printf("File successfully sent\n");
	}
}		
}	
int connect_to_peer(int port_no)
{
	client_fd=socket(AF_INET,SOCK_STREAM,0);
	if(client_fd<0)
	{
		perror("Couldnt create client socket \n");
		exit(-1);
	}
	struct sockaddr_in peer_server_addr;
	peer_server_addr.sin_family=AF_INET;
	peer_server_addr.sin_port=htons(port_no);
	peer_server_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
	//bcopy((char *)server->h_addr, 
         //(char *)&remote_server_addr.sin_addr.s_addr,
         //server->h_length);         
	int server_fd;
	server_fd=connect(client_fd,(struct sockaddr*)&peer_server_addr,sizeof(peer_server_addr));
	if(server_fd<0)
	{
		cout<<"server_fd"<<endl;
		perror("Couldn't establish connection \n");
		exit(-1);
	}
	else
	{
	cout<<"Connected to peer successfully. \n";
	}
	return client_fd;	
}

int connect_to_tracker(struct hostent *server,int port_no)
{
	client_fd=socket(AF_INET,SOCK_STREAM,0);
	if(client_fd<0)
	{
		perror("Couldnt create client socket \n");
		exit(-1);
	}
	remote_server_addr.sin_family=AF_INET;
	remote_server_addr.sin_port=htons(port_no);
	remote_server_addr.sin_addr.s_addr=INADDR_ANY;
	if(server==NULL)
	{
		perror("No such server exists \n");
		exit(-1);
	}
	bcopy((char *)server->h_addr, 
         (char *)&remote_server_addr.sin_addr.s_addr,
         server->h_length);         
	int server_fd;
	server_fd=connect(client_fd,(struct sockaddr*)&remote_server_addr,sizeof(remote_server_addr));
	if(server_fd<0)
	{
		cout<<"server_fd"<<endl;
		perror("Couldn't establish connection \n");
		exit(-1);
	}
	else
	{
	cout<<"Connected to tracker successfully. \n";
	}
	return client_fd;	
}

void share_with_tracker(string filename,struct hostent *server,int port_no,int listen_port)
{							
				int cli_fd=connect_to_tracker(server,port_no);
				char *cmd="share";
				int cm=send(cli_fd,cmd,BUF_MAX,0);
				cout<<"command "<<cmd<<" sent! \n";
				int fd=open(filename.c_str(),O_RDONLY);
				if(fd<0)
				{
					cerr<<"error opening file \n";
				}
				string send_hash=get_hash(fd);
				close(fd);
				cout<<"The hash is "<<send_hash<<"\n";
				char details[BUF_MAX];
				sprintf(details,"%s %s %d",filename.c_str(),send_hash.c_str(),listen_port);
				send(cli_fd,details,BUF_MAX,0);
				if(send>0)
				{
					cout<<"sent successfully : closing connection \n";
					close(cli_fd);
				}
}

void request_from_tracker(string filename,struct hostent *server,int port_no)
{	
	int cli_fd=connect_to_tracker(server,port_no);
	char *cmd="download";
	int cm=send(cli_fd,cmd,BUF_MAX,0);
	cout<<"command "<<cmd<<" sent! \n";
	char down[BUF_MAX];
	sprintf(down,"%s",filename.c_str());
	send(cli_fd,down,BUF_MAX,0);
	char port_num[BUF_MAX];
	recv(cli_fd,port_num,sizeof(port_num),0);
	int p_num;
	sscanf(port_num,"%d",&p_num);
	cout<<"The recieved portnumber is "<<p_num<<"\n";
	if(p_num!=0)
	{ 
	int cfd=connect_to_peer(p_num);
	send(cfd,filename.c_str(),BUF_MAX,0);
	string save_file=filename;
	int save=creat(save_file.c_str(),0777);
	if(save<0)
	{	cout<<"save"<<endl;
	perror("Couldnt create file to save \n");
	exit(-1);
	}
	close(save);
	FILE* recvfile=fopen(save_file.c_str(),"wb");
	char buffer[BUF_MAX]={'\0'};	
	int get_file;
	size_t x=sizeof(char);
	while((get_file=recv(client_fd,buffer,sizeof(buffer),0))>0)
	{
	if(get_file<0)
	{	cout<<"getfile"<<endl;
	perror("Couldnt retrieve file from server \n");
	exit(-1);
	}
	fwrite(buffer,x,get_file,recvfile);
	bzero(buffer,BUF_MAX);
	}
	}
	printf("File recieved successfully \n");
	close(cli_fd);
}
void create_user(string username,string password,struct hostent *server,int port_no)
{
	int cli_fd=connect_to_tracker(server,port_no);
	char *cmd="create_user";
	int cm=send(cli_fd,cmd,BUF_MAX,0);
	cout<<"command "<<cmd<<" sent! \n";
	char identi[BUF_MAX];
	sprintf(identi,"%s %s",username.c_str(),password.c_str());
	send(cli_fd,identi,BUF_MAX,0);
	char recv_status[BUF_MAX];
	recv(cli_fd,recv_status,sizeof(recv_status),0);
	printf("%s",recv_status);
	close(cli_fd);
}
void login_user(string username,string password,struct hostent *server,int port_no)
{
	int cli_fd=connect_to_tracker(server,port_no);
	char *cmd="login";
	int cm=send(cli_fd,cmd,BUF_MAX,0);
	cout<<"command "<<cmd<<" sent! \n";
	char identi[BUF_MAX];
	sprintf(identi,"%s %s",username.c_str(),password.c_str());
	send(cli_fd,identi,BUF_MAX,0);
	char recv_status[BUF_MAX];
	recv(cli_fd,recv_status,sizeof(recv_status),0);
	printf("%s",recv_status);
	close(cli_fd);
}
int main(int argc,char *argv[])
{	
	if(argc<4)
	{
	perror("Please provide ip address followed by port_number then followed by listening port_number\n");
	exit(-1);
	}
	server=gethostbyname(argv[1]);
	port_no=atoi(argv[2]);
	pthread_t client_threadid;
	char buffer[4096];	
	cli_as_server_args *cli_as_server_arg=(cli_as_server_args*)malloc(sizeof(cli_as_server_args));
	cli_as_server_arg->cli_port=(atoi(argv[3]));
	int listen_port=(atoi(argv[3]));
	if(pthread_create(&client_threadid,NULL,c_as_server,(void *)cli_as_server_arg)!=0)
	{
	perror("couldn't create new thread \n");
	exit(-1);
	}	
		pthread_detach(client_threadid);
		string cmd;
		printf("Enter a command (share <filename>,download<filename>,exit\n");
		getline(cin>>ws,cmd);		
		cout<<"Entered command is "<<cmd<<"\n";
		vector<string> inp;
		inp=process_string(cmd,' ');
		if(inp[0]=="share")
		{		string command=inp[0];
				string filename=inp[1];
				cout<<"Sharing file "<<filename<<" with the tracker \n";
				share_with_tracker(filename,server,port_no,listen_port);
		}
		else if(inp[0]=="download")
		{		
				string command=inp[0];
				cout<<"getting peer details for file "<<inp[1]<<" from the tracker"<<"\n";
				string filename=inp[1];
				request_from_tracker(filename,server,port_no);	
		}
		else if(inp[0]=="remove")
		{
			
		}
		else if(inp[0]=="create_user")
		{
			string username=inp[1];
			string password=inp[2];
			printf("Registering with tracker...\n");
			create_user(username,password,server,port_no);
		}
		else if(inp[0]=="login")
		{
			string username=inp[1];
			string password=inp[2];
			printf("Trying to login...\n");
			login_user(username,password,server,port_no);
		}
	return 0;
}


