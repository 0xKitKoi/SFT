#include <iostream>
#include <thread>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

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

int64_t GetFileSize(const std::string &fileName) {
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return -1;
    }
    return static_cast<int64_t>(file.tellg());
}

int RecvBuffer(int s, char *buffer, int bufferSize, int chunkSize = 4 * 1024) {
    int counter = 0;
    int i = 0;
    while (i < bufferSize) {
        const int l = recv(s, &buffer[i], std::min(chunkSize, bufferSize - i), 0);
        if (l <= 0) {
            return l;
        }
        i += l;
        counter++;
        if (i == 0 && counter >= 20) {
            return -1;
        }
    }
    return i;
}

int SendBuffer(int s, const char *buffer, int bufferSize, int chunkSize = 4 * 1024) {
    int i = 0;
    while (i < bufferSize) {
        const int l = send(s, &buffer[i], std::min(chunkSize, bufferSize - i), 0);
        if (l <= 0) {
            return l;
        }
        i += l;
    }
    return i;
}

int64_t SendFile(int s, const std::string &fileName, int chunkSize = 64 * 1024) {
    const int64_t fileSize = GetFileSize(fileName);
    if (fileSize < 0) {
        std::cout << "Failed to get file size\n";
        return -1;
    }
    std::ifstream file(fileName, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "Failed to open file\n";
        return -1;
    }

    if (SendBuffer(s, reinterpret_cast<const char *>(&fileSize), sizeof(fileSize)) != sizeof(fileSize)) {
        return -2;
    }

    char *buffer = new char[chunkSize];
    bool errored = false;
    int64_t i = fileSize;
    while (i != 0) {
        const int64_t ssize = std::min(i, static_cast<int64_t>(chunkSize));
        if (!file.read(buffer, ssize)) {
            errored = true;
            break;
        }
        const int l = SendBuffer(s, buffer, static_cast<int>(ssize));
        if (l <= 0) {
            errored = true;
            break;
        }
        i -= l;
    }
    delete[] buffer;

    file.close();

    return errored ? -3 : fileSize;
}

int64_t RecvFile(int s, const std::string &fileName, int chunkSize = 64 * 1024) {
    std::ofstream file(fileName, std::ofstream::binary);
    if (!file.is_open()) {
        return -1;
    }

    int64_t fileSize;
    if (RecvBuffer(s, reinterpret_cast<char *>(&fileSize), sizeof(fileSize)) != sizeof(fileSize)) {
        return -2;
    }

    char *buffer = new char[chunkSize];
    bool errored = false;
    int64_t i = fileSize;
    while (i != 0) {
        const int r = RecvBuffer(s, buffer, static_cast<int>(std::min(i, static_cast<int64_t>(chunkSize))));
        if ((r <= 0) || !file.write(buffer, r)) {
            errored = true;
            break;
        }
        i -= r;
    }
    delete[] buffer;

    file.close();
    std::cout << "Written to file: " << fileName << std::endl;

    return errored ? -3 : fileSize;
}

void ServerThread(parameters params) {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == -1) {
        std::cout << "Couldn't create server socket" << std::endl;
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(std::stoi(params.port));

    if (bind(server, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) == -1) {
        std::cout << "Failed to bind server socket" << std::endl;
        close(server);
        return;
    }

    std::cout << "Socket made. Waiting for client with a file.\n";

    if (listen(server, SOMAXCONN) == -1) {
        std::cout << "Failed to listen on server socket" << std::endl;
        close(server);
        return;
    }

    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    int client = accept(server, reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);
    if (client == -1) {
        std::cout << "Failed to accept client connection" << std::endl;
        close(server);
        return;
    }

    std::cout << "Got a client. Trying to receive the file.\n";

    const int64_t rc = RecvFile(client, "TRANSMITTEDFILE.BIN");
    if (rc < 0) {
        std::cout << "Failed to receive file: " << rc << std::endl;
    } else {
        s_Finished = !s_Finished;
    }

    close(client);
    close(server);
}

void ComThread() {
    // This thread will be for sending buffer sizes and 'commands'.
}

int main() {
	main:
    std::cout << "Send or receive a file? Type 'send' or 'get'\n\n-> ";
    std::string uI;
    std::getline(std::cin, uI);
    if (uI == "send") {
        // We are the client, sending a file to the server.
        parameters BRUH;
        std::cout << "What file am I sending? You should be able to drag and drop it here.\n";
        std::getline(std::cin, BRUH.filePath);
        std::cout << "\n What IP? \n\t-> ";
        std::getline(std::cin, BRUH.IP);
        std::cout << "\n What PORT? \n\t-> ";
        std::getline(std::cin, BRUH.port);

        if (BRUH.filePath.at(0) == '\"') {
            // If they dragged and dropped the file into the terminal window, cut off the "".
            BRUH.filePath = BRUH.filePath.substr(1, BRUH.filePath.size() - 2);
        }

        // Client code here
    } else if (uI == "get") {
        parameters BRUH;
        std::cout << "\n What IP to listen on? \n\t-> ";
        std::getline(std::cin, BRUH.IP);
        std::cout << "\n What PORT to listen on? \n\t-> ";
        std::getline(std::cin, BRUH.port);
        std::cout << "Attempting to start the Server.\n\t Press CTRL + C to Quit!\n";
        std::thread worker(ServerThread, BRUH);
        worker.join();
    }
    else {
    	std::cout << "Invalid Input.\n";
	goto main;
    }

    return 0;
}

