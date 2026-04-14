#include <iostream>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <cstring>
#include <sys/socket.h> // socket(), bind(), listen(), accept(), send(), recv()
#include <arpa/inet.h>  // inet_ntop(), inet_pton(), htons(), sockaddr_in
#include <unistd.h>     // close(), gethostname()
#include <netdb.h>      // getaddrinfo(), freeaddrinfo()

using namespace std; 
#define BUFFER 2048
#define SSH_CMD_BUFFER 4096

// leak checking command line 
// Compilation : clang++ distpsnotify.cc -o distpsnotify 
// ./distpsnotify -e "ps -eo pid=,ppid=,user=,args=" -a


// Example Command Line 1 : (All in one line)
//./distpsnotify -e "ps -eo pid=,ppid=,user=,args=" -a
// "/usr/ubuntu/remoteagent" -s "/usr/bin/ssh" -r "129.128.29.32" -r "129.128.29.33" -q "bash" -n 2 -p 8293
// compile line: g++ distpsnotify.cc -o distpsnotify


// Example Command Line 2: (All in one line)
//./remoteagent -e "ps -eo pid=,ppid=,user=,args=" -q "bash" -i "129.128.29.31" -p 8293


// Side Notes 
// "-e", singleton, execution command
// "-a", singleton, remote agent 
// "-s", singleton, remote shell
// "-r", possibly multiple instances, remote systems (Create a counter so we know how many)
//"-q", singleton, query
//"-n", singleton, iteration number
//"-p", singleton, port number

//the distpsnotify program must first create a working listening socket 
//so that the remote agents can connect a TCP/IP socket back to the distpsnotify program.  
//This TCP/IP socket is used by the distpsnotify program to send commands to the remote agents, and to receive data back from the remote agents.




// Function Declarations 
void PrintRemoteIP(int RemoteCounter, const char* ExecCommand, 
    const char* RemoteAgent, const char* RemoteShell, const char** RemoteIPs,
    const char* Query, int IterationNum,int Port);

// DO NOT TOUCH
// struct sockaddr_in ServerAddress;
struct addrinfo hints, *result;


int main(int argc, char *argv[]){

    // Check how many Remotes are there 
    int RemotesCounter = 0;
    for(int i = 0; i < argc; i++){
        if(strcmp(argv[i], "-r") == 0){
            RemotesCounter += 1;
        }
    }

    // Initlize Pointers 
    const char* ExecCommand = nullptr;          // -e: the ps/cat command to run
    const char* RemoteAgent = nullptr;          // -a: path to remoteagent binary
    const char* RemoteShell = nullptr;          // -s: path to ssh binary
    // This is for our remote IP's 
    const char** RemoteIPs = new const char*[RemotesCounter];   // -r: IP addresses
    int RIndex = 0;                             // index into RemoteIPs array
    const char* Query = nullptr;                // -q: search string
    const char* Temp1 = nullptr;                
    int IterationNum = 0;                       // -n: how many EXECUTE rounds
    const char* Temp2 = nullptr;
    int Port = 0;                               // -p: listening port number
    
    // similar to remoteagent.cc command line checker 
    for(int i = 1; i < argc; i+=2){

        if(strcmp(argv[i], "-e") == 0){ 
            ExecCommand = argv[i + 1];
        }
        else if(strcmp(argv[i], "-a") == 0){
            RemoteAgent = argv[i + 1];
        }
        else if(strcmp(argv[i], "-s") == 0){
            RemoteShell = argv[i + 1];
        }
        else if(strcmp(argv[i], "-r") == 0){
            RemoteIPs[RIndex++] = argv[i + 1];
        }
        else if(strcmp(argv[i], "-q") == 0 ){
            Query = argv[i + 1];
        }
        else if(strcmp(argv[i], "-n") == 0){
            Temp1 = argv[i + 1];
            IterationNum = atoi(Temp1);
        }
        else if(strcmp(argv[i], "-p") == 0){
            Temp2 = argv[i + 1];
            Port = atoi(Temp2);
        }


    }

    // The sequence is: socket() -> setsockopt() -> bind() -> listen() -> accept()
    // Making the listening end of the socket 
    // This is creating the socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0){
        fprintf(stderr, "ERROR: socket() failed\n");
        delete[] RemoteIPs;
        exit(1);
    }
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


    struct sockaddr_in ServerAddress;
    // Binding to the port 
    memset(&ServerAddress, 0, sizeof(ServerAddress));   // zero out the struct
    ServerAddress.sin_family = AF_INET;                 // IPv4
    ServerAddress.sin_addr.s_addr = INADDR_ANY;         // accept from any IP
    ServerAddress.sin_port = htons(Port);               // port in network byte order
    // INADDR_ANY means "listen on all network interfaces" (0.0.0.0).
    // htons() converts the port from host byte order to network byte order.

    
    if (::bind(listenfd, (struct sockaddr*)&ServerAddress, sizeof(ServerAddress)) < 0) {
        fprintf(stderr, "ERROR: bind failed: %d\n");
        close(listenfd);
        delete[] RemoteIPs;
        exit(1);
    }

    // Now we listen to the remoteagents 
    listen(listenfd, RemotesCounter);


    // Get my machines IP address 
    char Hostname[256];
    gethostname(Hostname, sizeof(Hostname));
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // I want an IPv4 address
    getaddrinfo(Hostname, NULL, &hints, &result); // find it 
        
    // conver to readable string 
    char LocalIP[INET_ADDRSTRLEN];
    struct sockaddr_in* addr = (struct sockaddr_in*)result->ai_addr;
    inet_ntop(AF_INET, &addr->sin_addr, LocalIP, sizeof(LocalIP));
    freeaddrinfo(result);
    fprintf(stderr, "DEBUG: LocalIP = %s\n", LocalIP);

    // allocate an array to store Popen() handles 
    // now launch ssh using the localIP
    FILE** SSHPROCCESS = new FILE*[RemotesCounter];

    for(int i = 0; i < RemotesCounter; i++){
        char SSHCOMMAND[BUFFER];
        snprintf(SSHCOMMAND, sizeof(SSHCOMMAND),
        "%s %s %s -e \\\"%s\\\" -q \\\"%s\\\" -i \\\"%s\\\" -p %d",
        RemoteShell, // /usr/bin/ssh
        RemoteIPs[i], // 129.128.29.32
        RemoteAgent, // /path/to/remoteagent
        ExecCommand, // ps -eo pid=,ppid=,user=,args=
        Query, // sshd
        LocalIP, // holder for now 
        Port); // 8293

        SSHPROCCESS[i] = popen(SSHCOMMAND, "r");
    }

    // Right before the accept loop: DEBUG
    //fprintf(stderr, "DEBUG: about to accept\n");


    // DOUBLE CHECK 

    // accept connections from each remote agent 
    int* clientSockets = new int[RemotesCounter];

    for (int i = 0; i < RemotesCounter; i++) {
        //clientSockets[i] = accept(listenfd, NULL, NULL);
        clientSockets[i] = -1;
    }

    // Right after the accept loop: DEBUG
    //fprintf(stderr, "DEBUG: connected\n");

    // Accept all connections and map each to the correct remote IP
    for (int i = 0; i < RemotesCounter; i++) {
        struct sockaddr_in peerAddr;
        socklen_t peerLen = sizeof(peerAddr);

        // accept() returns a new socket for this particular connection.
        // peerAddr is filled with the connecting client's address info.
        int connfd = accept(listenfd, (struct sockaddr*)&peerAddr, &peerLen);

        // Extract the peer's IP address as a string
        char peerIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peerAddr.sin_addr, peerIP, sizeof(peerIP));

        // Find which -r index this IP belongs to
        for (int j = 0; j < RemotesCounter; j++) {
            if (strcmp(peerIP, RemoteIPs[j]) == 0 && clientSockets[j] == -1) {
                clientSockets[j] = connfd;
                break;
            }
        }
    }


    for (int n = 1; n <= IterationNum; n++) {
        printf("N=%d\n", n);
        // Send EXECUTE to ALL agents
        for (int i = 0; i < RemotesCounter; i++) {
            // send() transmits data over a connected socket.
            // "EXECUTE\n" = 8 bytes (the command + newline delimiter)
            send(clientSockets[i], "EXECUTE\n", 8, 0);
        }

        // Read responses from each agent IN ORDER
        for (int i = 0; i < RemotesCounter; i++) {
            char line[BUFFER];

            // Read lines one at a time from this agent
            while (true) {
                int totalread = 0;
                while (totalread < BUFFER - 1) {
                    int bytes = recv(clientSockets[i], &line[totalread], 1, 0);
                    if (bytes <= 0) break;                  // connection error/closed
                    if (line[totalread] == '\n') break;     // end of this line
                    totalread++;
                }
                line[totalread] = '\0';
                
                 // Check for the START/STOP delimiters
                if (strcmp(line, "START") == 0) {
                    continue;  // skip START, just move on
                }
                else if (strcmp(line, "STOP") == 0) {
                    break;  // done with this agent
                }
                else {
                    // Print with IP prefix
                    // Data line — print with IP prefix:
                    //   e.g. "129.128.29.32::sshd:(839) -- sshd:(841)"
                    printf("%s::%s\n", RemoteIPs[i], line);
                }
            }
        }
        
    }

    // After all iterations, send QUIT to all agents
    for (int i = 0; i < RemotesCounter; i++) {
        send(clientSockets[i], "QUIT\n", 5, 0);
    }
    

    // -----------------------------------------------------------------------------
    for (int i = 0; i < RemotesCounter; i++) {
        close(clientSockets[i]); // close the TCP connection to this agent
        pclose(SSHPROCCESS[i]); // wait for the SSH child process to exit
    }
    // close the sockets and free the heap variables
    close(listenfd);
    delete[] clientSockets;
    delete[] SSHPROCCESS;
    delete[] RemoteIPs;
    return 0;
}
