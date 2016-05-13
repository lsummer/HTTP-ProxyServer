#include <stdio.h> 
#include <Windows.h> 
#include <process.h> 
#include <string.h> 
#include <tchar.h>

#pragma comment(lib,"Ws2_32.lib") 

#define MAXSIZE 65507 //发送数据报文的最大长度 
#define HTTP_PORT 80 //http 服务器端口 

#define CACHE_MAXSIZE 100
#define DATELENGTH 40
//Http 重要头部数据 
struct HttpHeader{
	char method[4]; // POST 或者 GET，注意有些为 CONNECT，本实验暂不考虑
	char url[1024];  //  请求的 url 
	char host[1024]; //  目标主机 
	char cookie[1024 * 10]; //cookie 
	HttpHeader(){
		ZeroMemory(this, sizeof(HttpHeader));
	}
};
//这个是高仿的htpp的头部数据，用于在cache中找到对象，但是节约了cookie的空间
struct cache_HttpHeader{
	char method[4]; // POST 或者 GET，注意有些为 CONNECT，本实验暂不考虑
	char url[1024];  //  请求的 url 
	char host[1024]; //  目标主机 
	cache_HttpHeader(){
		ZeroMemory(this, sizeof(cache_HttpHeader));
	}
};
//实现代理服务器的缓存技术
struct __CACHE{
	cache_HttpHeader htphed;
	char buffer[MAXSIZE];
	char date[DATELENGTH];//存储的更新时间
	__CACHE(){
		ZeroMemory(this->buffer, MAXSIZE);
		ZeroMemory(this->buffer, sizeof(date));
	}

};
int __CACHE_number = 0;//标记下一个应该放缓存的位置
__CACHE cache[CACHE_MAXSIZE];//真`缓存

BOOL InitSocket();
BOOL ParseHttpHead(char *buffer, HttpHeader * httpHeader);
BOOL ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
int Cache_find(__CACHE *cache, HttpHeader htp);

//代理相关参数 
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;
//禁止访问的主机，禁用网站
char* host[10] = { "today.hit.edu.cn", "jwts.hit.edu.cn", "resource.hit.edu.cn", "www.hit.edu.cn" };
const int host_number =  4;

//网站诱导
char* host_to_another = "djangobook.py3k.cn" ;//诱导到pt.hit.edu.cn,url=http://pt.hit.edu.cn/  ,url源=http://djangobook.py3k.cn/2.0/
char* another[2] = { "pt.hit.edu.cn","http://pt.hit.edu.cn/" };//跳转到的地址
//char* host_to_another = "www.pku.edu.cn";
//char* another[2] = { "today.hit.edu.cn", "http://today.hit.edu.cn/" };
//char Cache[][];
//由于新的连接都使用新线程进行处理，对线程的频繁的创建和销毁特别浪费资源
//可以使用线程池技术提高服务器效率 
//const int ProxyThreadMaxNum = 20; 
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0}; 
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0}; 
struct ProxyParam{
	SOCKET clientSocket;
	SOCKET serverSocket;
};

int _tmain(int argc, _TCHAR* argv[])
{
	
	printf("代理服务器正在启动\n");
	printf("初始化...\n");
	if (!InitSocket()){

		printf("socket 初始化失败\n");
		return -1;
	}
	printf("代理服务器正在运行，监听端口  %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;
	ProxyParam *lpProxyParam;
	HANDLE hThread;
	DWORD dwThreadID;
	//代理服务器不断监听 
	sockaddr_in verAddr;
	int hahaha = sizeof(SOCKADDR);
	while (true){
		acceptSocket = accept(ProxyServer, (SOCKADDR*)&verAddr, &(hahaha));
		lpProxyParam = new ProxyParam;
		if (lpProxyParam == NULL){
			continue;
		}
		if (!strcmp("127.0.0.1", inet_ntoa(verAddr.sin_addr))){
			printf("被限制的用户访问！\n");
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;
		hThread = (HANDLE)_beginthreadex(NULL, 0,
			&ProxyThread, (LPVOID)lpProxyParam, 0, 0);
		CloseHandle(hThread);
		Sleep(200);
	}
	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}


//************************************ 
// Method:        InitSocket 
// FullName:    InitSocket 
// Access:        public   
// Returns:      BOOL 
// Qualifier:  初始化套接字 
//************************************ 
BOOL InitSocket(){

	//加载套接字库（必须） 
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示 
	int err;
	//版本 2.2 
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库   
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0){
		//找不到 winsock.dll 
		printf("加载 winsock 失败，错误代码为: %d\n", WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("不能找到正确的 winsock 版本\n");
		WSACleanup();
		return FALSE;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == ProxyServer){
		printf("创建套接字失败，错误代码为：%d\n", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR){
		printf("绑定套接字失败\n");
		return FALSE;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR){
		printf("监听端口%d 失败", ProxyPort);
		return FALSE;
	}
	return TRUE;
}

//************************************ 
// Method:        ProxyThread 
// FullName:    ProxyThread 
// Access:        public   
// Returns:      unsigned int __stdcall 
// Qualifier:  线程执行函数 
// Parameter: LPVOID lpParameter 
//************************************ 
unsigned int __stdcall ProxyThread(LPVOID lpParameter){
	char Buffer[MAXSIZE];
	char *CacheBuffer;
	ZeroMemory(Buffer, MAXSIZE);
	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;
	recvSize = recv(((ProxyParam
		*)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0){
		goto error;
	}

	HttpHeader* httpHeader = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	bool change=ParseHttpHead(CacheBuffer, httpHeader);
	delete CacheBuffer;
	
	int j = 0;
	//对对应的网站不能访问--禁用网站！！！！ok
	for (j = 0; j < host_number; j++)//4表示禁用网站的个数，虽然那里是10，但是实际上只有4个，所以是4
	{
		int i = 0;
		bool find = true;
		for (i = 0; i < strlen(host[j]);i++){
			if (host[j][i] != httpHeader->host[i]) {
				find = false;
				break;
			}
		}
		if (find)
			goto error;
	}
	
	
	if (!ConnectToServer(&((ProxyParam
		*)lpParameter)->serverSocket, httpHeader->host)) {
		goto error;
	}
	printf("代理连接主机  %s  成功\n", httpHeader->host);
	//跳转到另外一个网站，如果change==tre
	if (change){
		char *CacheBuffer;
		CacheBuffer = new char[MAXSIZE];
		ZeroMemory(CacheBuffer, MAXSIZE);
		//memcpy(CacheBuffer, Buffer, recvSize);
		int ii = 0,lengthth=0;
		for (ii = 0; ii < strlen(httpHeader->method); ii++){
			CacheBuffer[lengthth++] = httpHeader->method[ii];
		}
		CacheBuffer[lengthth++] = ' ';
		for (ii = 0; ii < strlen(another[1]); ii++){
			CacheBuffer[lengthth++] = another[1][ii];
		}
		CacheBuffer[lengthth++] = ' ';
		char *hh = "HTTP/1.1";
		for (ii = 0; ii < strlen(hh); ii++){
			CacheBuffer[lengthth++] = hh[ii];
		}
		CacheBuffer[lengthth++] = '\r';
		CacheBuffer[lengthth++] = '\n';
		char *hhh = "HOST: ";
		for (ii = 0; ii < strlen(hhh); ii++){
			CacheBuffer[lengthth++] = hhh[ii];
		}
		for (ii = 0; ii < strlen(httpHeader->host); ii++){
			CacheBuffer[lengthth++] = httpHeader->host[ii];
		}
		CacheBuffer[lengthth++] = '\r';
		CacheBuffer[lengthth++] = '\n';
		char *ptr;
		const char *delim = "\r\n";
		char *p = strtok_s(Buffer, delim, &ptr);
		int length1 = strlen(p);
		length1 += 2;
		p = strtok_s(NULL, delim, &ptr);
		length1 += strlen(p);
		length1 += 2;
		for (ii =1; ii < recvSize-length1+1; ii++){
			CacheBuffer[lengthth++] = ptr[ii];
		}
		memcpy(Buffer, CacheBuffer, max(strlen(CacheBuffer),recvSize));
		delete CacheBuffer;
	}
	//printf("Buffer为----------------------------------\n%s", Buffer);
	//在缓冲区找到了该对象
	int find = Cache_find(cache, *httpHeader);
	if (find>=0){
		char *CacheBuffer;
		char Buffer2[MAXSIZE];
		int i = 0, length = 0,length2=0;
		ZeroMemory(Buffer2, MAXSIZE);
		CacheBuffer = new char[recvSize + 1];
		ZeroMemory(CacheBuffer, recvSize + 1);
		memcpy(CacheBuffer, Buffer, recvSize);

		const char *delim = "\r\n";
		char *ptr;
		char *p = strtok_s(CacheBuffer, delim, &ptr);
		length += strlen(p);
		length += 2;
		p = strtok_s(NULL, delim, &ptr);
		length += strlen(p);
		length += 2;
		length2 = length;
		
		char *ife="If-Modified-Since: ";
		for (i = 0; i < length; i++){
			Buffer2[i] = Buffer[i];
		}
		for (i = 0; i < strlen(ife); i++)
		{
			Buffer2[length + i] = ife[i];
		}
		length =length+ strlen(ife);
		for (i = 0; i < strlen(cache[find].date); i++)
		{
			Buffer2[length + i] = cache[find].date[i];
		}
		length += strlen(cache[find].date);
		Buffer2[length++] = '\r';
		Buffer2[length++] = '\n';
		for (i = length2; i < recvSize;i++){
			Buffer2[length++] = Buffer[i];
		}
		delete CacheBuffer;

		//printf("BUFFER2222222222222222222222222222222222222222222:\n%s\n", Buffer2);
		//将客户端发送的 HTTP 数据报文处理后转发给目标服务器 
		ret = send(((ProxyParam *)lpParameter)->serverSocket, Buffer2, strlen(Buffer2)
			+ 1, 0);
		//等待目标服务器返回数据 
		recvSize = recv(((ProxyParam
			*)lpParameter)->serverSocket, Buffer2, MAXSIZE, 0);
		
		if (recvSize <= 0){
			goto error;
		}
		//printf("\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\|||||%s\n",Buffer2);
		const char *blank = " ";
		const char *Modd = "304";
		if (!memcmp(&Buffer2[9], Modd, strlen(Modd)))
		{
			ret = send(((ProxyParam
				*)lpParameter)->clientSocket, cache[find].buffer, strlen(cache[find].buffer)+1, 0);
			goto error;
		}
	}
	//将客户端发送的 HTTP 数据报文直接转发给目标服务器 
	ret = send(((ProxyParam *)lpParameter)->serverSocket, Buffer, strlen(Buffer)
		+ 1, 0);
	//等待目标服务器返回数据 
	recvSize = recv(((ProxyParam
		*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0){
		goto error;
	}

	//得到时间
	char *chacheBuff = new char[MAXSIZE];
	ZeroMemory(chacheBuff, MAXSIZE);
	memcpy(chacheBuff, Buffer, MAXSIZE);
	const char *delim = "\r\n";
	char *ptr;
	char dada[DATELENGTH];
	ZeroMemory(dada, sizeof(dada));
	char *p = strtok_s(chacheBuff, delim, &ptr);
	bool cun = false;
	while (p){
		if (p[0] == 'L'){
			if (strlen(p) > 15){
				char header[15];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 14);
				if (!(strcmp(header, "Last-Modified:")))
				{
					memcpy(dada, &p[15], strlen(p) - 15);
					cun = true;
					break;
				}
			}
		}
		p = strtok_s(NULL, delim, &ptr);
	}
	if (cun){
		//把新的东东放入到缓存中去
		if (find >= 0)
		{
			memcpy(&(cache[find].buffer), Buffer, strlen(Buffer));
			memcpy(&(cache[find].date), dada, strlen(dada));
			
			//printf("----------------%s\n",dada);
		}
		else
		{
			memcpy(&(cache[__CACHE_number%CACHE_MAXSIZE].htphed.host), httpHeader->host, strlen(httpHeader->host));
			memcpy(&(cache[__CACHE_number%CACHE_MAXSIZE].htphed.method), httpHeader->method, strlen(httpHeader->method));
			memcpy(&(cache[__CACHE_number%CACHE_MAXSIZE].htphed.url), httpHeader->url, strlen(httpHeader->url));
			memcpy(&(cache[__CACHE_number%CACHE_MAXSIZE].buffer), Buffer, strlen(Buffer));
			memcpy(&(cache[__CACHE_number%CACHE_MAXSIZE].date), dada, strlen(dada));
			__CACHE_number++;
		}
	}
	//printf("收到的响应报文: %s \n", Buffer);

	//将目标服务器返回的数据直接转发给客户端 
	ret = send(((ProxyParam
		*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
	//错误处理 
error:
	printf("关闭套接字\n");
	Sleep(200);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete    lpParameter;
	_endthreadex(0);
	return 0;
}

//************************************ 
// Method:        ParseHttpHead 
// FullName:    ParseHttpHead 
// Access:        public   
// Returns:      void 
// Qualifier:  解析 TCP 报文中的 HTTP 头部 
// Parameter: char * buffer 
// Parameter: HttpHeader * httpHeader 
//************************************ 
BOOL ParseHttpHead(char *buffer, HttpHeader * httpHeader){
	char *p;
	char *ptr;
	bool change = false;
	const char * delim = "\r\n";
	/*
	strtok()用来将字符串分割成一个个片段。参数s指向欲分割的字符串，参数delim则为分割字符串中包含的所有字符。当strtok()在参数s的字符串中发现参数delim中包涵的分割字符时,则会将该字符改为\0 字符。在第一次调用时，strtok()必需给予参数s字符串，往后的调用则将参数s设置成NULL。每次调用成功则返回指向被分割出片段的指针。
	strtok函数会破坏被分解字符串的完整，调用前和调用后的s已经不一样了。
	*/
	p = strtok_s(buffer, delim, &ptr);//提取第一行 
	printf("%s\n", p);
	if (p[0] == 'G'){//GET 方式 
		memcpy(httpHeader->method, "GET", 3);
		
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
	}
	else if (p[0] == 'P'){//POST 方式 
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
	}
	printf("%s\n", httpHeader->url);
	p = strtok_s(NULL, delim, &ptr);//提取第二行

	while (p){
		//printf("-------%s\n", p);
		switch (p[0]){
		case 'H'://Host 
			if (!memcmp(&p[6], host_to_another, strlen(p) - 6))
			{
				memcpy(httpHeader->host, another[0], strlen(another[0]));
				//strcpy(httpHeader->url, "");
				memcpy(httpHeader->url, another[1],strlen(another[1]));
				change = true;
			}
			else{
				memcpy(httpHeader->host, &p[6], strlen(p) - 6);
			}
			break;
		case 'C'://Cookie 
			if (strlen(p) > 8){
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie")){
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
			}
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
	return change;
}

//************************************ 
// Method:        ConnectToServer 
// FullName:    ConnectToServer 
// Access:        public   
// Returns:      BOOL 
// Qualifier:  根据主机创建目标服务器套接字，并连接 
// Parameter: SOCKET * serverSocket 
// Parameter: char * host 
//************************************ 
BOOL ConnectToServer(SOCKET *serverSocket, char *host){
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	HOSTENT *hostent = gethostbyname(host);//这个函数的传入值是域名或者主机名，例如"www.google.cn"等等。传出值，是一个hostent的结构。如果函数调用失败，将返回NULL。
	if (!hostent){
		return FALSE;
	}
	/*
	返回hostent结构体类型指针
	struct hostent
	{
	char    *h_name;
	char    **h_aliases;
	int     h_addrtype;
	int     h_length;
	char    **h_addr_list;
	#define h_addr h_addr_list[0]
	};

	hostent->h_name
	表示的是主机的规范名。例如www.google.com的规范名其实是www.l.google.com。

	hostent->h_aliases
	表示的是主机的别名.www.google.com就是google他自己的别名。有的时候，有的主机可能有好几个别名，这些，其实都是为了易于用户记忆而为自己的网站多取的名字。

	hostent->h_addrtype
	表示的是主机ip地址的类型，到底是ipv4(AF_INET)，还是pv6(AF_INET6)

	hostent->h_length
	表示的是主机ip地址的长度

	hostent->h_addr_lisst
	表示的是主机的ip地址，注意，这个是以网络字节序存储的。千万不要直接用printf带%s参数来打这个东西，会有问题的哇。所以到真正需要打印出这个IP的话，需要调用inet_ntop()。

	const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt) ：
	这个函数，是将类型为af的网络地址结构src，转换成主机序的字符串形式，存放在长度为cnt的字符串中。返回指向dst的一个指针。如果函数调用错误，返回值是NULL。
	*/
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);//主机的ip地址
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET){
		return FALSE;
	}
	if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr))
		== SOCKET_ERROR){
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}
//判断两个报文是否相同--不包括cookie
BOOL Isequal(cache_HttpHeader htp1, HttpHeader htp2)
{
	if (strcmp(htp1.method,htp2.method)) return false;
	if (strcmp(htp1.url, htp2.url)) return false;
	if (strcmp(htp1.host, htp2.host)) return false;
	return true;
}

//在缓存中找到对应的对象
int Cache_find(__CACHE *cache, HttpHeader htp)
{
	int i = 0;
	for (i = 0; i < CACHE_MAXSIZE; i++)
	{
		if (Isequal(cache[i].htphed, htp)) return i;
	}
	return -1;
}