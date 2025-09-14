// gcc shell_server.c -o shell_server
// ./shell_server
// depois acesse http://127.0.0.1:5000/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 5000
#define BUF_SIZE 8192

char secret_path[128];

// gera string hexadecimal aleatória
void generate_secret_path() {
    srand(time(NULL));
    strcpy(secret_path, "/");
    for (int i = 0; i < 10; i++) {
        char tmp[16];
        sprintf(tmp, "%x", rand() % 99999999);
        strcat(secret_path, tmp);
    }
    printf("Rota secreta: %s\n", secret_path);
}

void send_form(int client) {
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
    send(client, response, strlen(response), 0);
}

void send_result(int client, const char *output) {
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
    send(client, response, strlen(response), 0);
}

void handle_client(int client, struct sockaddr_in addr) {
    char buffer[BUF_SIZE];
    int len = recv(client, buffer, BUF_SIZE - 1, 0);
    if (len <= 0) return;
    buffer[len] = '\0';

    // só aceita localhost
    if (ntohl(addr.sin_addr.s_addr) != INADDR_LOOPBACK) {
        char *resp = "HTTP/1.1 403 Forbidden\r\n\r\nAcesso negado.";
        send(client, resp, strlen(resp), 0);
        return;
    }

    if (strncmp(buffer, "GET / ", 6) == 0) {
        send_form(client);
    }
    else if (strncmp(buffer, "POST ", 5) == 0 &&
             strstr(buffer, secret_path) != NULL) {
        // procurar "cmd=" no corpo do POST
        char *body = strstr(buffer, "\r\n\r\n");
        if (!body) return;
        body += 4;
        char *cmd = strstr(body, "cmd=");
        if (!cmd) return;
        cmd += 4;

        // substituir + por espaço
        for (char *p = cmd; *p; p++) if (*p == '+') *p = ' ';

        // executar comando
        FILE *fp = popen(cmd, "r");
        if (!fp) {
            send_result(client, "Erro ao executar comando.");
            return;
        }

        char output[4096] = "";
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strlen(output) + strlen(line) < sizeof(output) - 1)
                strcat(output, line);
        }
        pclose(fp);

        send_result(client, output[0] ? output : "(sem saída)");
    }
    else {
        char *resp = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(client, resp, strlen(resp), 0);
    }
}

int main() {
    int server, client;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    generate_secret_path();

    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); exit(1);
    }

    if (listen(server, 5) < 0) {
        perror("listen"); exit(1);
    }

    printf("Servidor rodando em http://127.0.0.1:%d/\n", PORT);

    while (1) {
        client = accept(server, (struct sockaddr*)&client_addr, &client_len);
        if (client < 0) { perror("accept"); continue; }

        if (fork() == 0) { // processo filho
            close(server);
            handle_client(client, client_addr);
            close(client);
            exit(0);
        }
        close(client);
    }
    return 0;
}

