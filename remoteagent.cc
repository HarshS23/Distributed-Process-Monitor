#include <iostream>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
using namespace std;

// -----------------------------------------------------------------------------------------------------------------------
//                                         INSTRUCTIONS TO RUN THIS PROGRAM 
// -----------------------------------------------------------------------------------------------------------------------
/*
        1. Make two terminals 
        2. Terminal 1 : LHS , Terminal 2 : RHS
        3. Terminal 1 Run Command: nc -l 1234 
        4. Terminal 2 Run Command: clang++ -g remoteagent.cc -o remoteagent 
        5. Terminal 2 Run Command: leaks -atExit -- ./remoteagent -e "cat smalltest.txt" -q "sshd" -i "127.0.0.1" -p 1234
        6. Terminal 1 Run Command: EXECUTE 
        7. Terminal 1 Run Command: QUIT
*/



// ------------------------------- LEAK CHECKING -------------------------------------------------------------------------
// Must use clang++ to check for memory leaks : clang++ -g remoteagent.cc -o remoteagent                                  
// To check for memory leaks command: leaks -atExit -- ./remoteagent -e "cat smalltest.txt" -q "sshd" -i "127.0.0.1" -p 8293
// ------------------------------- LEAK CHECKING -------------------------------------------------------------------------

// ------------------------------- COMPRESSION COMMANDS --------------------------------------------------------------------
// when compressing as zip on mac we must use this command to remove the excess files that apple adds 
// COMMAND: zip -r -X assn02.zip assn02
// ------------------------------- COMPRESSION COMMANDS --------------------------------------------------------------------


/*
Example Command line (run): ./remoteagent -e "ps -eo pid=,ppid=,user=,args=" -q "zsh" -i "129.128.29.31" -p 8293
argv[0] = ./remoteagent
argv[1] = -e
argv[2] = ps -eo pid=,ppid=,user=,args= 
argv[3] = -q
argv[4] = bash
argv[5] = -i
argv[6] = 129.128.29.31
argv[7] = -p
argv[8] = 8293
*/


// ---- Constants ------------------------------------------------------------
#define BUFFER 2048

// Maximum number of processes we can store from one popen() call.
// If a system has more than this many processes, increase this value.
#define MAX_PROCESSES 4096
// ----------------------------------------------------------------------------
// ---- Struct declaration ----------------------------------------------------
typedef struct{
    unsigned int PPID;          // Parent PID
    unsigned int PID;           // PID 
    char USER[(BUFFER / 4)];    // the User
    char COMMAND[(BUFFER / 4)]; // up to the first white space

}Process;

// struct sockaddr_in serverAddr;
// ------------------------------------------------------------------------------

// -- Function Declarations -----------------------------------------------------
Process* ParsePipeData(const char* CommandString, 
    unsigned int &counter, 
    const char* Query, 
    bool* matchFlags);
void PrintProcesses(Process* processes, 
    unsigned int count, 
    unsigned int targetPID, 
    int socketfd);
// ------------------------------------------------------------------------------

// ------------------------------------------------------------------------------
//                              MAIN 
// ------------------------------------------------------------------------------
int main(int argc, char *argv[]){

    // --- 1) Parse command-line arguments ------------------------------------
    // Expected: -e <cmd> -q <query> -i <server_ip> -p <port>  (4 pairs = 8 args + argv[0] = 9)

    if(argc != 9){
        std::cerr << "Error: NOT ENOUGH ARGUMENTS PROVIDED IN COMMAND LINE\n" << std::endl;
        std::cerr << "EXPECTED INPUT FORMAT : -e <cmd> -q <query> -i <server_ip> -p <port>\n" << std::endl;
        exit(1);
    }

    // Pointer declarations for strings
    const char* CommandString = nullptr;    // the -e execution command
    const char* Query = nullptr;            // the -q query string
    const char* IPAddress = nullptr;        // the -i server IP (distpsnotify)
    const char* temp = nullptr;             // the -p port number
    int Port;

    // Command Checking 
    for(int i = 1; i < argc ; i+=2){
        if(strcmp(argv[i], "-e") == 0){ 
            CommandString = argv[i + 1];
        }
        else if(strcmp(argv[i], "-q") == 0){ 
            Query = argv[i + 1];
        }
        else if(strcmp(argv[i], "-i") == 0){ 
            IPAddress = argv[i + 1];
        }
        else if(strcmp(argv[i], "-p") == 0){ 
            temp = argv[i + 1];
            Port = atoi(temp);
        }

    }

    // ------------------------- SOCKET CONNECTION -------------------------
    // ------------------------- Create a TCP socket -----------------------
    // socket(domain, type, protocol)
    //   AF_INET     = IPv4
    //   SOCK_STREAM = TCP (reliable, ordered byte-stream)
    //   0           = let the OS pick the protocol (TCP for SOCK_STREAM)
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketfd < 0){
        std::cerr << "Error: SOCKET ERROR, SOCKET FAILED \n" << endl;
        exit(1);
    }
    //----------------------------------------------------------------------
    // This tells connect() WHERE to connect: which IP and which port.
    // specifing where to send data 
    struct sockaddr_in ServerAddress;
    memset(&ServerAddress, 0, sizeof(ServerAddress));       // set the memory and clear it 
    ServerAddress.sin_family = AF_INET;                     // using IPv4 
    ServerAddress.sin_port = htons(Port);                   // Using the port number 
    //----------------------------------------------------------------------
    // inet_pton() converts the IP address string (e.g. "129.128.29.31")
    // into the binary form and stores it in serverAddr.sin_addr.
    //   "pton" stands for "presentation to network".
    inet_pton(AF_INET, IPAddress, &ServerAddress.sin_addr); // the IP Address 
    //   htons() = "host to network short"  converts the port number from
    //   the host machine's byte order to the big endian order that TCP/IP uses.

    //----------------------------------------------------------------------
    // connect() initiates a TCP 3-way handshake with the server.
    // It blocks until the connection is established or an error occurs.
    // Returns 0 on success, and -1 on failure.
    if(connect(socketfd, (struct sockaddr*) &ServerAddress , sizeof(ServerAddress))){
        fprintf(stderr, "ERROR: connect failed\n");
        exit(1);
    }
    //fprintf(stderr, "DEBUG: remoteagent connected\n");
    //----------------------------------------------------------------------

    //----------------------------------------------------------------------
    // The socket stays open across all iterations. distpsnotify sends
    // "EXECUTE\n" or "QUIT\n" one at a time; we respond accordingly.

    while(true){
        // read the command from the socket 
        char command[BUFFER];
        int totalread = 0;

        while(totalread < BUFFER - 1){
            int n = recv(socketfd, &command[totalread], 1, 0); // READ 1 BYTE 
            // recv() reads bytes from the connected socket.
            //   &command[totalread] = where to store the byte
            //   1                   = read exactly 1 byte
            //   0                   = no special flags
            // Returns: number of bytes read (1), 0 if connection closed, and -1 on error.
            if(n <= 0) break;                       // connection closed or theres some sort of error 
            if(command[totalread] == '\n') break;   // found the end of line or end of this command 
            totalread++;
        }
        command[totalread] = '\0'; // Null Terminated the string 
        
         // --- Handle EXECUTE command ----------------------------------------
        if(strcmp(command, "EXECUTE") == 0){

            unsigned int counter = 0;
            // Now dynamically allocated to support up to MAX_PROCESSES.
            bool* matchFlags = new bool[MAX_PROCESSES]();   // Track which processes match


            // Run the -e command via popen(), parse all processes,
            // and record which ones match the query.
            Process* ProcessArray = ParsePipeData(CommandString, counter, Query, matchFlags);
            if(ProcessArray == nullptr){
                return EXIT_FAILURE;
            }

            // sending the first message which is START
            // Send START delimiter so distpsnotify knows data is coming
            send(socketfd, "START\n", 6, 0);

            // Only print processes that matched the query
            // For each process that matched the query, print the
            // "parent(PPID) -- child(PID)" line and send it over the socket.
            for(uint i = 0; i < counter; i++){
                if(matchFlags[i]){
                    PrintProcesses(ProcessArray, counter, ProcessArray[i].PID, socketfd);
                }
            }

            // sending the last message to stop after all matches 
            send(socketfd, "STOP\n", 5, 0); 

            delete[] ProcessArray;
            delete[] matchFlags;
        }

    
        if(strcmp(command, "QUIT") == 0){
            break;
        }

        else if(totalread == 0){
            break;
        }
    }

    close(socketfd);        // close the TCP connection
    return 0;
}



// ----------------------------------------------------------------------------
// ParsePipeData
// ----------------------------------------------------------------------------
// Runs the execution command (-e) via popen(), reads each line of output,
// parses the 4-tuple (PID, PPID, USER, COMMAND), and records which lines
// match the query string.
//
// Returns: a heap-allocated array of Process structs (caller must delete[]).
//          counter is set to the number of valid entries.
//          matchFlags[i] is true if process i matched the query.
// ----------------------------------------------------------------------------
Process* ParsePipeData(const char* CommandString, unsigned int &counter, const char* Query, bool* matchFlags){


    // Start child process (Creates a pipe from the child proccess to the parent process)
    char buffer[BUFFER];

    // popen() creates a child process that runs CommandString using /bin/sh.
    // It returns a FILE* that we can read with fgets() this is the pipe
    // from the child's stdout to us (the parent).
    FILE *fp = popen(CommandString, "r");
    if(!fp){
        std::cerr << "ERROR: Failed to open pipe (popen() Failed)\n" << std::endl;
        counter = 0;
        return nullptr;
    }

    // Allocate process array on the heap (requires dynamic allocation)
    // do not touch 
    Process* ProcessArray = new Process[BUFFER]; // allocate on the heap 
    // Read lines one at a time from the child process
    while(fgets(buffer, sizeof(buffer), fp) != nullptr && counter < BUFFER){

        // --- EXITNOW handling ----------------------------------------------
        // Spec: "If the child process ever returns the string 'EXITNOW',
        //        the parent process should also exit."
        // Spec also says: "There may be whitespace before and after"
        // So we strip leading/trailing whitespace before comparing.
        char trimmed[BUFFER];
        strncpy(trimmed, buffer, sizeof(trimmed));
        trimmed[sizeof(trimmed) - 1] = '\0';

        // Strip leading whitespace
        char* start = trimmed;
        while (*start == ' ' || *start == '\t') start++;

        // Strip trailing whitespace/newline
        char* end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
            *end = '\0';
            end--;
        }

        if (strcmp(start, "EXITNOW") == 0) {
            pclose(fp);
            return ProcessArray;  // return what we have so far
        }
        

        // ==========================================================================================
        //                                      OLD CODE
        // ==========================================================================================
        // if((strstr(buffer, "EXITNOW\n") != nullptr) ||  (strstr(buffer, "EXITNOW") != nullptr)){
        //     pclose(fp);
        //     return ProcessArray;
        // }
        // ==========================================================================================



        int parsed = sscanf(buffer, "%u %u %s %s", 
            &ProcessArray[counter].PID, &ProcessArray[counter].PPID, 
            ProcessArray[counter].USER, ProcessArray[counter].COMMAND);

        if(parsed == 4){
            // Check if this FULL LINE (not just one field) contains the query.
            // strstr returns non NULL if Query appears anywhere in buffer.
            matchFlags[counter] = (strstr(buffer,Query) != nullptr);
            counter++;
        }

    }

    // pclose() waits for the child process to finish and closes the pipe.
    // Must always be called to avoid zombie processes.
    pclose(fp);
    return ProcessArray;
}



// ============================================================================
// PrintProcesses
// ----------------------------------------------------------------------------
// Given a target PID, finds it and its parent in the process array, then
// sends a formatted line "parentCMD(parentPID) -- childCMD(childPID)\n"
// over the socket to distpsnotify.
// ============================================================================
void PrintProcesses(Process* processes, unsigned int count, unsigned int targetPID, int socketfd){


    // Find the target process
    // finding the target process PID 
    Process* targetProcess = nullptr;
    for(uint i = 0; i < count; i++){
        if(processes[i].PID == targetPID){
            targetProcess = &processes[i];
            break;
        }
    }
    
    if(targetProcess == nullptr) return;
    
    // Find the parent process
    // store all processes before doing lookups 
    Process* parentProcess = nullptr;
    for(uint i = 0; i < count; i++){
        if(processes[i].PID == targetProcess->PPID){
            parentProcess = &processes[i];
            break;
        }
    }
    
    if(parentProcess == nullptr) return;
    
    // Print: parent -- child
    // Format: "parentCommand(parentPID) -- targetCommand(targetPID)\n"
    char OutputBuffer[BUFFER];
    snprintf(OutputBuffer, sizeof(OutputBuffer), "%s(%u) -- %s(%u)\n",
    parentProcess->COMMAND, parentProcess->PID, 
    targetProcess->COMMAND, targetProcess->PID);

    // send() transmits the formatted string over the connected TCP socket.
    //   socketfd                  = the socket file descriptor
    //   OutputBuffer              = pointer to the data to send
    //   strlen(OutputBuffer)      = number of bytes to send
    //   0                         = no special flags
    send(socketfd, OutputBuffer, strlen(OutputBuffer), 0);
    
}
