#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define BUFLEN 1500

void printMessage(char * message);
int checkMessageValidity(char * message);
int checkFileExist(char * fileName);
char * getFileName(char * message);
char * trimFileName(char * filePath);
char * getFileSize(char * fileName);
int sendFile(char * fileName, int socket);
char * buildMessage(char * code, char * fileSize);
int checkForCommand(char * message);

/* Function to handle an error by printing a message and exiting */
static void print_error_and_exit(char *errorMessage) {
	fprintf(stderr, "%s: %s\n", errorMessage, strerror(errno));
	exit(1);
}

int main(int argc, char *argv[]) {
	struct sockaddr_in srv_addr, client_addr; 
	socklen_t client_addr_len;

	int srv_fd, client_fd;

	int backlog = 10;
	char buf[BUFLEN];
	ssize_t rcount;

	/* Erzeuge das Socket. */
	/* Creating a socket */
	srv_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (srv_fd == -1) print_error_and_exit("Error at socket()");

	/* Define the socket address of the server */
	memset(&srv_addr, 0, sizeof(srv_addr));
	/* IPv4 Connection */
	srv_addr.sin_family = AF_INET;
    	/* Port */
    	srv_addr.sin_port = htons(PORT);
    	/* INADDR_ANY: accept any address */
    	srv_addr.sin_addr.s_addr = INADDR_ANY;

    	/* Binding socket to a port */
    	if (bind(srv_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1)
    		print_error_and_exit("Error at bind()");

    	/* Listen for incoming connections */
    	if (listen(srv_fd, backlog) == -1)
        	print_error_and_exit("Error at listen()");

    	printf("Ready to receive connections!\n");
    	/* While infinite loop to accept connections */
    	while (1) {
    		/* Waiting for incoming connection */
		client_addr_len = sizeof(client_addr);
		client_fd = accept(srv_fd, (struct sockaddr *)&client_addr, &client_addr_len);
		if (client_fd == -1) print_error_and_exit("Error at accept()");
        	printf("Processing client %s\n", inet_ntoa(client_addr.sin_addr));
		
		while(1) {
			int isAlive = 1;
			/* Loop to receive message with size bigger than buffer */
			rcount = BUFLEN;
			char * cmd = malloc(sizeof(char[BUFLEN]));
			while (rcount==BUFLEN) {
				printf("Waiting for input!\n");
				rcount = recv(client_fd, buf, BUFLEN, 0);
				printf("Input bytes: %ld\n", rcount);
				/* Close the socket when client closed the socket */
				if (rcount <= 0) {
					printf("Client closed connection!\n");
					isAlive = 0;
					break;
				}
				buf[rcount] = '\0';
				printf("%s\n", buf);
				if (checkForCommand(buf) > 0) strcpy(cmd, buf);
				/* Check message validity */ 
				if (checkMessageValidity(cmd) > 0) {
					char * fileName = getFileName(cmd);
					/* Check for file */
					if (checkFileExist(fileName) > 0) {
						/* Send requested file */
						char * fileSize = getFileSize(fileName);
						char * msg = buildMessage("200", fileSize);
						int msg_len = strlen(msg);
						if (send(client_fd, msg, msg_len, 0) == -1) print_error_and_exit("Error at write()");
						printf("%s\n", msg);
						free(msg);
						sendFile(fileName, client_fd);
					} else {
						/* Send error 404 */
						char * msg = buildMessage("404", "");
						int msg_len = strlen(msg);
						if (send(client_fd, msg, msg_len, 0) == -1) print_error_and_exit("Error at write()");
						printf("%s\n", msg);
						free(msg);
						sendFile("404.html", client_fd);
						/* Close the connection to the client*/
						isAlive = 0;
					}
					free(fileName);
				} else {
					/* Send Error 400 Bad Request file */
					char * msg = buildMessage("400", "");
					int msg_len = strlen(msg);
					if (send(client_fd, msg, msg_len, 0) == -1) print_error_and_exit("Error at write()");
					printf("%s\n", msg);
					free(msg);
					sendFile("400.html", client_fd);
					/* Close the connection to the client*/
					isAlive = 0;
					break;
				}
				if (!isAlive) break;
			}
			free(cmd);
			if (!isAlive) {
				close(client_fd);
				break;
			}
		}
        }

	/* Close the connection */
    	close(srv_fd);

	return EXIT_SUCCESS;
}

/* Function generates the response message with a given code */
char * buildMessage(char * code, char * fileSize) {
	char * txt1;
	if (strcmp(code, "200") == 0) {
		txt1 = "HTTP/1.1 200 OK\r\n";
		char * txt2 = "Content-Type: text/html\r\n";
		char * txt3 = "Connection: Open\r\n";
		char * txt4 = "\r\n";
		char * txt5 = "Content-Length: ";
		char * i = malloc(strlen(txt1) + strlen(txt2) + strlen(txt3) + strlen(txt4) + strlen(txt5) + strlen(fileSize));
		strcpy(i, txt1);
		strcat(i, txt5);
		strcat(i, fileSize);
		strcat(i, txt4);
		strcat(i, txt2);
		strcat(i, txt3);
		strcat(i, txt4);
		return i;
	} else {
		if (strcmp(code, "400") == 0) txt1 = "HTTP/1.1 400 Bad Request\r\n";
		if (strcmp(code, "404") == 0) txt1 = "HTTP/1.1 404 Not Found\r\n";
		if (strcmp(code, "500") == 0) txt1 = "HTTP/1.1 500 Internal Server Error\r\n";
		char * txt2 = "Content-Type: text/html\r\n";
		char * txt3 = "Connection: Close\r\n";
		char * txt4 = "\r\n";
		char * i = malloc(strlen(txt1) + strlen(txt2) + strlen(txt3) + strlen(txt4) + 1);
		strcpy(i, txt1);
		strcat(i, txt2);
		strcat(i, txt3);
		strcat(i, txt4);
		return i;
	}
	return "";
}

/* Function to look for a GET */
int checkForCommand(char * message) {
	int result = -1;
	char * temp = malloc(strlen(message)+1);
	strcpy(temp, message);
	char delimiter [] = " \r\n";
	char * flag;
	
	flag = strtok(temp, delimiter);
	if (strcmp(flag, "GET") == 0) result = 1;
	free(temp);
	return result;
}

/* Function to check whether the first line of an HTTP request is valid */
int checkMessageValidity(char * message) {
	int result = -1;
	char * temp = malloc(strlen(message)+1);
	strcpy(temp, message);
	char delimiter [] = " \r\n";
	char * msg, * flag, * end;
	
	flag = strtok(temp, delimiter);
	msg = strtok(NULL, delimiter); 
	end = strtok(NULL, delimiter);
	
	if (strcmp(flag, "GET") == 0) result = 1;
	if (strcmp(end, "HTTP/1.1") != 0) result = -1;
	free(temp);
	return result;
}

/* Function to get the requested file name of the given string*/
char * getFileName(char * message) {
	char * msg = malloc(strlen(message));
	strcpy(msg, message);
	char delimiter [] = " \r\n";
	char * ptr;
	
	ptr = strtok(msg, delimiter);
	ptr = strtok(NULL, delimiter);
	ptr = trimFileName(ptr);
	free(msg);
	return ptr;
}

/* Method deletes the first character of the given string */
char * trimFileName(char * filePath) {
	char * result = malloc(strlen(filePath));
	for (int i = 1; i < strlen(filePath); i++){
		result[i-1] = filePath[i];
	}
	return result;
}

/* Function to read the data size */
char * getFileSize(char * fileName) {
	struct stat fs;
	int fd = open(fileName, O_RDONLY);
	if(fstat(fd, &fs) == -1) printf("Can't read file size\n");
	char * fileSize = malloc(64);
	sprintf(fileSize, "%ld", fs.st_size);
	return fileSize;
}

/* Function checks whether the given file exists */
int checkFileExist(char * fileName) {
	int check = -1;
	FILE * fp = fopen(fileName, "r");
	if (fp != NULL) {
		check = 1;
		fclose(fp);
	}
	return check;
}

/* Function to send given file*/
int sendFile(char * fileName, int socket) {
	FILE * fp = fopen(fileName, "r");
	if (fp) {
		int bufferSize = 256;
		char line [bufferSize];
		size_t bytes = 0;
		
		while ((bytes = fread(line, sizeof(char), bufferSize, fp)) > 0) {
			if (bytes < bufferSize && bytes > 0) {
				for (int i = bytes; i < bufferSize; i++) line[i] = ' ';
			}
			if (send(socket, line, sizeof(line), 0) < 0) {
				print_error_and_exit(strcat("Error at sending file ", fileName));
			} 
		}
		
	}
	fclose(fp);
}





