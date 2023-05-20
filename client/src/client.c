#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 200000 // Tamanho máxio do buffer das mensagens recebidas do servidor
#define SERVER_ADDRESS "127.0.0.1"

unsigned int PORT = 5000;

//Função auxiliar para ler a entrada
void read_input(char *var) {
  fflush(stdin); //Limpa buffer de entrada
  fgets(var, 10000, stdin); // Lê uma linha inteira (includindo espaços)
  var[strcspn(var, "\n")] = 0; // Remove newline character
}

int main(int argc, char *argv[]) {
  // Verifica se foi fornecido um argumento para botar na PORT
  if (argc >= 2) {
    // atoi faz o parse para unsigned int
    PORT = atoi(argv[1]);
  }

  int client_socket;
  struct sockaddr_in server_address;
  char buffer[MAX_BUFFER_SIZE] = {0};

  // Cria o socket
  if ((client_socket = socket(AF_INET, SOCK_DGRAM, 0)) == 0) {
    perror("Failed to create socket");
    exit(EXIT_FAILURE);
  }

  // Seta os parâmetros do endereço do servidor
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(PORT);

  // Converte endereços IPv4 e IPv6 de texto pra binário
  if (inet_pton(AF_INET, SERVER_ADDRESS, &server_address.sin_addr) <= 0) {
    perror("Invalid address/ Address not supported");
    exit(EXIT_FAILURE);
  }


  //variáveis a serem utilizadas na comunicação cliente servidor
  char action[20];
  char payload[500] = "{}";
  char email[50];
  char nome[50];
  char sobrenome[50];
  char cidade[50];
  char formacao[50];
  char ano[10];
  char habilidades[500];
  int bytes_received;

  printf("Bem vindo ao cadastro de Usuários mc833, selecione uma das operações abaixo para continuar:\n");

  char input[10];

  while (1)
    {
      //"Menu" com as opções disponíveis pro cliente
      printf("1. Cadastrar novo usuário\n");
      printf("2. Listar todos os usuários\n");
      printf("3. Buscar por um usuário\n");
      printf("4. Listar todas as pessoas formadas em certo curso\n");
      printf("5. Listar todas as pessoas formadas em certo ano\n");
      printf("6. Listar todas as pessoas que possuem certa habilidade\n");
      printf("7. Remover um usuário\n");
      printf("8. Sair\n");

      read_input(input);

      //Em cada caso abaixo primeiro é montado o payload a ser mandado ao servidor.

      // Operação de cadastro de novo usuário
      if (strcmp(input, "1") == 0) {
        strcpy(action, "new_user");
        printf("email: ");
        read_input(email);
        printf("nome: ");
        read_input(nome);
        printf("sobrenome: ");
        read_input(sobrenome);
        printf("cidade de residência: ");
        read_input(cidade);
        printf("formação acadêmica: ");
        read_input(formacao);
        printf("ano de formatura: ");
        read_input(ano);
        printf("habilidades: ");
        read_input(habilidades);

        sprintf(payload, "{ \"nome\": \"%s %s\", \"sobrenome\": \"%s\", \"cidade\": \"%s\", \"formacao\": \"%s\", \"ano\": \"%s\", \"email\": \"%s\", \"habilidades\": \"%s\" }", nome, sobrenome, sobrenome, cidade, formacao, ano, email, habilidades);


        // Operação de print de todos os usuários cadastrados
      } else if (strcmp(input, "2") == 0) {
        strcpy(action, "list_all_users");
        strcpy(payload, "{}");

        // Operação busca de um único usuário
      } else if (strcmp(input, "3") == 0) {
        strcpy(action, "list_user");
        printf("Email do usuário que procura: ");
        read_input(email);
        sprintf(payload, "\"%s\"", email);

        // Operação de busca de usuários com certa formação
      } else if (strcmp(input, "4") == 0) {
        strcpy(action, "query_users");
        printf("Nome do curso: ");
        read_input(formacao);
        sprintf(payload, "{ \"formacao\": \"%s\" }", formacao);

        // Operação de busca de usuários com certo ano formação
      } else if (strcmp(input, "5") == 0) {
        strcpy(action, "query_users");
        printf("Ano de formação: ");
        read_input(ano);
        sprintf(payload, "{ \"ano\": \"%s\" }", ano);

        // Operação de busca de usuários com certa habilidade
      } else if (strcmp(input, "6") == 0) {
        strcpy(action, "query_users");
        printf("Nome da habilidade: ");
        read_input(habilidades);
        sprintf(payload, "{ \"habilidades\": \"%s\" }", habilidades);

        // Operação de exclusão de usuário a partir do email
      } else if (strcmp(input, "7") == 0) {
        strcpy(action, "delete_user");
        printf("Email do usuário que deseja deletar: ");
        read_input(email);
        sprintf(payload, "\"%s\"", email);

        // Finaliza as operações do cliente e sai do loop
      } else if (strcmp(input, "8") == 0) { 
        break;

      } else {
        printf("Opção inválida");
      }

      // Caso o usuário tenha enviado um comando para o servidor, é printado a resposta
      if (strcmp(input, "1") == 0 || strcmp(input, "2") == 0 || strcmp(input, "3") == 0 || strcmp(input, "4") == 0 || strcmp(input, "5") == 0 || strcmp(input, "6") == 0 || strcmp(input, "7") == 0) {
        const char buff[10000];
        sprintf(buff, "{\"command\": \"%s\", \"payload\":%s}", action, payload);
        printf("%s\n", buff);
        sendto(client_socket, buff, strlen(buff), 0,
               (struct sockaddr*)&server_address, sizeof(server_address));
        bytes_received = recvfrom(client_socket, buffer, sizeof(buffer), 0,
                                  (struct sockaddr*)&server_address, sizeof(server_address));
        buffer[bytes_received] = '\0';
        printf("Server response: %s\n", buffer);

        for (int i = 0; i < MAX_BUFFER_SIZE; i++)
          buffer[i] = '\0';
      }
    }
    return 0;
}
