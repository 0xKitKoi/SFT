#include <iostream>
#include <thread>
#include <fstream>

// needed for send() recv()
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <Ws2tcpip.h>
// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

struct parameters {
    std::string IP;
    std::string filePath;
    std::string port;
};

static bool s_Finished = false;

void DoWork() {
	while (!s_Finished) {
		std::cout << "Working..\n";
	}
}

int64_t GetFileSize(const std::string& fileName) {
    // no idea how to get filesizes > 2.1 GB in a C++ kind-of way.
    // I will cheat and use Microsoft's C-style file API
    FILE* f;
    if (fopen_s(&f, fileName.c_str(), "rb") != 0) {
        return -1;
    }
    _fseeki64(f, 0, SEEK_END);
    const int64_t len = _ftelli64(f);
    fclose(f);
    return len;
}
///
/// Recieves data in to buffer until bufferSize value is met
///
int RecvBuffer(SOCKET s, char* buffer, int bufferSize, int chunkSize = 4 * 1024) {
    int counter = 0;
    int i = 0;
    while (i < bufferSize) {
        //std::cout << "[" << i << "] " << " - Attempting to recvieve a buffersize.\n";
        const int l = recv(s, &buffer[i], __min(chunkSize, bufferSize - i), 0);
        if (l < 0) { return l; } // this is an error
        i += l;
        counter++;
        if (i == 0 && counter >= 20) {
            // probably died.
            return -1;
        }
    }
    return i;
}

///
/// Sends data in buffer until bufferSize value is met
///
int SendBuffer(SOCKET s, const char* buffer, int bufferSize, int chunkSize = 4 * 1024) {

    int i = 0;
    while (i < bufferSize) {
        const int l = send(s, &buffer[i], __min(chunkSize, bufferSize - i), 0);
        if (l < 0) { return l; } // this is an error
        i += l;
    }
    return i;
}

//
// Sends a file
// returns size of file if success
// returns -1 if file couldn't be opened for input
// returns -2 if couldn't send file length properly
// returns -3 if file couldn't be sent properly
//
int64_t SendFile(SOCKET s, const std::string& fileName, int chunkSize = 64 * 1024) {

    const int64_t fileSize = GetFileSize(fileName);
    if (fileSize < 0) { 
        std::cout << "died on getting the filesize\n";
        return -1; 
    }
    std::ifstream file(fileName, std::ios::binary);
    if (file.fail()) {
        std::cout << "Died on opening the file?\n";
        return -1;
    }

    if (SendBuffer(s, reinterpret_cast<const char*>(&fileSize),
        sizeof(fileSize)) != sizeof(fileSize)) {
        return -2;
    }

    char* buffer = new char[chunkSize];
    bool errored = false;
    int64_t i = fileSize;
    while (i != 0) {
        const int64_t ssize = __min(i, (int64_t)chunkSize);
        if (!file.read(buffer, ssize)) { errored = true; break; }
        const int l = SendBuffer(s, buffer, (int)ssize);
        if (l < 0) { errored = true; break; }
        i -= l;
    }
    delete[] buffer;

    file.close();

    return errored ? -3 : fileSize;
}

//
// Receives a file
// returns size of file if success
// returns -1 if file couldn't be opened for output
// returns -2 if couldn't receive file length properly
// returns -3 if couldn't receive file properly
//
int64_t RecvFile(SOCKET s, const std::string& fileName, int chunkSize = 64 * 1024) {
    std::ofstream file(fileName, std::ofstream::binary);
    if (file.fail()) { return -1; }

    int64_t fileSize;
    if (RecvBuffer(s, reinterpret_cast<char*>(&fileSize),
        sizeof(fileSize)) != sizeof(fileSize)) {
        return -2;
    }

    char* buffer = new char[chunkSize];
    bool errored = false;
    int64_t i = fileSize;
    while (i != 0) {
        const int r = RecvBuffer(s, buffer, (int)__min(i, (int64_t)chunkSize));
        if ((r < 0) || !file.write(buffer, r)) { errored = true; break; }
        i -= r;
    }
    delete[] buffer;

    file.close();
    std::cout << "Written to file: " << fileName << std::endl;

    return errored ? -3 : fileSize;
}
//LPVOID param
DWORD __stdcall ClientProc(LPVOID param) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);


    parameters* params = (parameters*)param;
    //std::cout << "Parameter dump: " << params->filePath << " || " << params->IP << " || " << params->port << " \n";


    struct addrinfo hints = { 0 }, * result, * ptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    //int ec = getaddrinfo("127.0.0.1", "9001", &hints, &result);
    std::cout << params->IP.c_str() << " || " << params->port.c_str() << std::endl;
    int ec = getaddrinfo(params->IP.c_str(), params->port.c_str(), &hints, &result);
    if (ec != 0) {
        std::cout << ec << " AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";
        std::cout << "[-] Either the server is not Listening, The port is NOT open, or you typed something very WRONG!\n";
        return ~0;
    }

    SOCKET client = INVALID_SOCKET;
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        client = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (client == SOCKET_ERROR) {
            // TODO: failed (don't just return, cleanup)
            std::cout << "Died on socket creation.\n";
        }
        if (connect(client, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
            closesocket(client);
            client = INVALID_SOCKET;
            continue;
        }
        break;
    }
    freeaddrinfo(result);

    if (client == SOCKET_ERROR) {
        std::cout << "Couldn't create client socket" << std::endl;
        return ~1;
    }
    //std::string* filePath = (std::string*)param;
    //std::cout << "Attempting to send " << *filePath << std::endl;
    std::cout << "Attempting to send " << params->filePath << std::endl;

    int64_t rc = SendFile(client, params->filePath);
    if (rc < 0) {
        std::cout << "Failed to send file: " << rc << std::endl;
    }

    closesocket(client);

    return 0;
}


void ServerThread(parameters params) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    {
        struct addrinfo hints = { 0 };
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;

        struct addrinfo* result = NULL;

        //if (0 != getaddrinfo("127.0.0.1", "9001", &hints, &result)) {
        if (0 != getaddrinfo(params.IP.c_str(), params.port.c_str(), &hints, &result)) {
            // TODO: failed (don't just return, clean up)
        }

        SOCKET server = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (server == INVALID_SOCKET) {
            // TODO: failed (don't just return, clean up)
        }

        if (bind(server, result->ai_addr, (int)result->ai_addrlen) == INVALID_SOCKET) {
            // TODO: failed (don't just return, clean up)
        }
        freeaddrinfo(result);

        std::cout << "Socket made. Waiting for client with a file.\n";

        if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
            // TODO: failed (don't just return, clean up)
        }

        // start a client on another thread
        //HANDLE hClientThread = CreateThread(NULL, 0, ClientProc, NULL, 0, 0);
        server:
            SOCKET client = accept(server, NULL, NULL);

            std::cout << "Got a client. Trying to recieve the file.\n";

            const int64_t rc = RecvFile(client, "TRANSMITTEDFILE.BIN");
            if (rc < 0) {
                std::cout << "Failed to recv file: " << rc << std::endl;
            }
            else {
                s_Finished = !s_Finished;
            }

            if (!s_Finished) {
                // try again?
                goto server;
            }
        

        closesocket(client);
        closesocket(server);

        //WaitForSingleObject(hClientThread, INFINITE);
        //CloseHandle(hClientThread);
    }
    WSACleanup();
}

void ComThread() {
    // this thread will be for sending buffer sizes, and 'commands'.
}



int main() {
    /*
	std::thread worker(DoWork);
	std::cin.get();
	s_Finished = true;
	worker.join();
	std::cin.get();
    */



    std::cout << "send shit or get shit?\nTYPE send OR get\n\n-> ";
    std::string uI;
    std::getline(std::cin, uI);
    if (uI == "send") {
        // we are the client, sending a file to the server.
        parameters BRUH;
        std::cout << "What file?\n";
        std::getline(std::cin, BRUH.filePath);
        std::cout << "\n What IP? \n\t-> ";
        std::getline(std::cin, BRUH.IP);
        std::cout << "\n What PORT? \n\t-> ";
        std::getline(std::cin, BRUH.port);

        if (BRUH.filePath.at(0) == '\"') {
            // if they dragged and dropped the file into the terminal window, cut off the "".
            BRUH.filePath = BRUH.filePath.substr(1, BRUH.filePath.size() - 2);
        }

        
        HANDLE hClientThread = CreateThread(NULL, 0, ClientProc, &BRUH, 0, 0);


        WaitForSingleObject(hClientThread, INFINITE);
        CloseHandle(hClientThread);
    }
    if (uI == "get") {
        parameters BRUH;
        std::cout << "\n What IP? \n\t-> ";
        std::getline(std::cin, BRUH.IP);
        std::cout << "\n What PORT? \n\t-> ";
        std::getline(std::cin, BRUH.port);
        std::cout << "Starting Server.\n\t Press CTRL + C to Quit!\n";
        std::thread worker(ServerThread, BRUH);
        worker.join();
        
    }



    system("pause");
    return 0;
}