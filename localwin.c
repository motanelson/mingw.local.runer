// gcc shell_server.c -o shell_server -lws2_32
// ./shell_server.exe
// depois acesse http://127.0.0.1:5000/

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 5000
#define BUF_SIZE 8192

char secret_path[128];

void generate_secret_path() {
    srand((unsigned)time(NULL));
    strcpy(secret_path, "/");
    for (int i = 0; i < 10; i++) {
        char tmp[16];
        sprintf(tmp, "%x", rand() % 99999999);
        strcat(secret_path, tmp);
    }
    printf("Rota secreta: %s\n", secret_path);
}

void send_form(SOCKET client) {
    char response[BUF_SIZE];
    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n\r\n"
        "<!doctype html><html><head><title>Shell Local</title></head>"
        "<body style='background-color:yellow;color:black;font-family:monospace;'>"
        "<h2>Executar Comando</h2>"
        "<form method='POST' action='%s'>"
        "<input type='text' name='cmd' style='width:400px;'>"
        "<button type='submit'>OK</button></form>"
        "</body></html>",
        secret_path
    );
    send(client, response, (int)strlen(response), 0);
}

void send_result(SOCKET client, const char *output) {
    char response[BUF_SIZE];
    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n\r\n"
        "<!doctype html><html><head><title>Resultado</title></head>"
        "<body style='background-color:yellow;color:black;font-family:monospace;'>"
        "<h2>Resultado do Comando:</h2>"
        "<pre>%s</pre>"
        "<a href='/'>Voltar</a>"
        "</body></html>",
        output
    );
    send(client, response, (int)strlen(response), 0);
}

void handle_client(SOCKET client, struct sockaddr_in addr) {
    char buffer[BUF_SIZE];
    int len = recv(client, buffer, BUF_SIZE - 1, 0);
    if (len <= 0) return;
    buffer[len] = '\0';

    // só aceita localhost (127.0.0.1)
    if (addr.sin_addr.s_addr != inet_addr("127.0.0.1")) {
        char *resp = "HTTP/1.1 403 Forbidden\r\n\r\nAcesso negado.";
        send(client, resp, (int)strlen(resp), 0);
        return;
    }

    if (strncmp(buffer, "GET / ", 6) == 0) {
        send_form(client);
    }
    else if (strncmp(buffer, "POST ", 5) == 0 &&
             strstr(buffer, secret_path) != NULL) {
        // procurar "cmd=" no corpo
        char *body = strstr(buffer, "\r\n\r\n");
        if (!body) return;
        body += 4;
        
         
        char *cmd = strstr(body, "cmd=");
        if (!cmd) return;
        cmd += 4;

        // substituir + por espaço
        for (char *p = cmd; *p; p++) if (*p == '+') *p = ' ';
        char cmd1[4096]="\0";
        strcpy(cmd1,cmd);
        strcat(cmd1," > out.txt");
        // executar comando no Windows
        system(cmd1);
        char output[4096] = "";
        char line[256];
        FILE* fp;
        fp=fopen("out.txt","r");
        while (fgets(line, sizeof(line), fp)) {
            if (strlen(output) + strlen(line) < sizeof(output) - 1)
                strcat(output, line);
        }
        fclose(fp);

        send_result(client, output[0] ? output : "(sem saída)");
    }
    else {
        char *resp = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(client, resp, (int)strlen(resp), 0);
    }
}

int main() {
    WSADATA wsa;
    SOCKET server, client;
    struct sockaddr_in server_addr, client_addr;
    int client_len = sizeof(client_addr);

    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("WSAStartup falhou.\n");
        return 1;
    }

    generate_secret_path();

    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) { perror("socket"); exit(1); }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(server, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        perror("bind"); exit(1);
    }

    if (listen(server, 5) == SOCKET_ERROR) {
        perror("listen"); exit(1);
    }

    printf("Servidor rodando em http://127.0.0.1:%d/\n", PORT);

    while (1) {
        client = accept(server, (struct sockaddr*)&client_addr, &client_len);
        if (client == INVALID_SOCKET) continue;

        handle_client(client, client_addr);
        closesocket(client);
    }

    closesocket(server);
    WSACleanup();
    return 0;
}
