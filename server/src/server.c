#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>

#define PORTA 5000                      // Número da porta para o servidor
#define MAX_CLIENTES 10                  // Número máximo de clientes concorrentes
#define TAMANHO_BUFFER 4096                // Tamanho do buffer de recebimento
#define PASTA_USUARIOS "./db/users/"    // Caminho pros arquivos dos usuários

volatile int rodando = 1;  // Controla o loop do servidor

int verdadeiro = 1;

// Manipulador de sinal para SIGINT utilizado na main()
void manipulador_sigint(int signum) {
  printf("Recebido SIGINT, encerrando o servidor...\n");
  rodando = 0;
}

const char *listar_todos_usuarios(const char *caminho) {
  DIR *dir;
  struct dirent *ent;
  json_object *users_array = json_object_new_array(); // Cria lista de objetos JSON

  if ((dir = opendir(caminho)) == NULL) {
    perror("Não foi possível abrir o diretório");
    return NULL;
  }

  while ((ent = readdir(dir)) != NULL) {
    // Ignora . e ..
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
      continue;
    }

    char caminho_completo[100];
    sprintf(caminho_completo, "%s/%s", caminho, ent->d_name);

    FILE *fp = fopen(caminho_completo, "r");
    if (fp == NULL) {
      printf("Não foi possível abrir o arquivo %s\n", caminho_completo);
      continue;
    }

    // Lê as informações do usuário do arquivo e cria um objeto JSON.
    json_object *user_obj = json_object_new_object();
    json_object_object_add(user_obj, "email", json_object_new_string(ent->d_name));

    char buffer[10000];
    fgets(buffer, sizeof(buffer), fp);
    json_object *json_obj = json_tokener_parse(buffer);
    json_object_array_add(users_array, json_obj); // Adiciona o objeto do usuário na lista de JSON 
    fclose(fp);
  }

  closedir(dir);

  // Converte lista JSON em string e retorna
  const char *json_str = json_object_to_json_string(users_array);

  size_t len = strlen(json_str);
  char *resp = (char *) malloc(len + 1);
  strncpy(resp, json_str, len);
  resp[len] = '\0';

  json_object_put(users_array); // Libera a lista da memória

  return resp;
}

// Deleta usuário com base no email fornecido (deleta o arquivo do usuário)
void deletar_usuario(const char *email, const char *caminho) {
  char caminho_completo[100];
  sprintf(caminho_completo, "%s/%s.json", caminho, email);

  if (remove(caminho_completo) != 0) {
    printf("Não foi possível deletar o arquivo %s\n", caminho_completo);
  } else {
    printf("Arquivo %s deletado com sucesso.\n", caminho_completo);
  }
}

// Função auxiliar utilizada na busca por parâmetro
int busca_substring(const char *texto, const char *subtexto) {
  unsigned int tamanho_texto = strlen(texto);
  unsigned int tamanho_subtexto = strlen(subtexto);
  if (tamanho_texto < tamanho_subtexto) {
    return 0; // Texto é menor que o subtexto
  }
  for (size_t i = 0; i <= tamanho_texto - tamanho_subtexto; i++) {
    if (strncasecmp(texto + i, subtexto, tamanho_subtexto) == 0) {
      return 1; // Substring encontrada
    }
  }
  return 0; // Substring não encontrada
}

// Busca por parâmetro
const char *query_usuarios(const json_object *payload, const char *caminho) {
  json_object *user_array = json_object_new_array(); // Cria a lista de JSON

  DIR *dir;
  struct dirent *ent;

  if ((dir = opendir(caminho)) == NULL) {
    perror("Não foi possível abrir o diretório");
    return NULL;
  }

  while ((ent = readdir(dir)) != NULL) {
    // Ignora . e ..
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
      continue;
    }

    char caminho_completo[100];
    sprintf(caminho_completo, "%s/%s", caminho, ent->d_name);

    FILE *fp = fopen(caminho_completo, "r");
    if (fp == NULL) {
      printf("Não foi possível abrir o arquivo %s\n", caminho_completo);
      continue;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fp)) {
      json_object *user = json_tokener_parse(buffer); // Parse JSON
      if (user != NULL) {
        int match = 1; // Flag que indica se usuario da match nos parâmetros do payload
        json_object_object_foreach(payload, key, val) {
          json_object *user_value = json_object_object_get(user, key);
          if (user_value != NULL) {
            const char *user_value_str = json_object_get_string(user_value);
            const char *payload_value_str = json_object_get_string(val);
            if (strcasecmp(key, "habilidades") == 0) {
              if (busca_substring(user_value_str, payload_value_str) == 0) {
                match = 0; // Para o campo habilidades se o parametro passado no payload não for SUBSTRING dele, flag = False e Break
                break;
              }
            } else if (strcmp(user_value_str, payload_value_str) != 0) {
              match = 0; // Para demais campos, se o do payload não for igual ao do usuário, flag = False e Break
              break;
            }
          } else {
            match = 0; // Caso em que o campo passado como filtro não existe não existe no usuário
            break;
          }
        }
        if (match) {
          // Se deu match, adiciona usuário (nome e email) na lista de resposta
          json_object *nome = NULL; //pega o nome
          json_object_object_get_ex(user, "nome", &nome);

          json_object *email = NULL; //pega o email
          json_object_object_get_ex(user, "email", &email);

          // Extract string values from retrieved properties
          const char *nome_str = json_object_get_string(nome);
          const char *email_str = json_object_get_string(email);

          // Cria novo objeto com email e nome e adicionar na lista de resposta
          json_object *new_user = json_object_new_object();
          json_object_object_add(new_user, "nome", nome);
          json_object_object_add(new_user, "email", email);

          json_object_array_add(user_array, new_user);
        } else {
          json_object_put(user); // da Free no JSON se não for adicionado na lista
        }
      }
    }

    fclose(fp);
  }

  closedir(dir);

  const char *json_str = json_object_to_json_string(user_array); // Converte JSON array em string

  size_t len = strlen(json_str);
  char *resp = (char *) malloc(len + 1);
  strncpy(resp, json_str, len);
  resp[len] = '\0';

  json_object_put(user_array); // Libera a lista da memória

  return resp;
}

// Retorna usuário a partir do email
const char *listar_usuario(const char *email, const char *caminho) {
  char caminho_completo[100];
  // Como o nome do arquivo é criado a partir do email do usuário, não é necessário procurar pelo usuário certo, só recriar o nome do arquivo através do email passado e abri-lo
  sprintf(caminho_completo, "%s/%s.json", caminho, email);

  FILE *fp = fopen(caminho_completo, "r");
  if (fp == NULL) {
    printf("Não foi possível abrir o arquivo %s\n", caminho_completo);
    return NULL;
  }

  char buffer[4096];
  json_object *user = NULL;

  while (fgets(buffer, sizeof(buffer), fp)) {
    user = json_tokener_parse(buffer); // Parse JSON do arquivo e coloca na variavel user
    if (user != NULL) {
      break;
    }
  }

  fclose(fp);

  if (user == NULL) {
    printf("Usuário com email %s não encontrado\n", email);
    return NULL;
  }

  const char *json_str = json_object_to_json_string(user); // Converte objeto JSON em string

  size_t len = strlen(json_str);
  char *resp = (char *) malloc(len + 1);
  strncpy(resp, json_str, len);
  resp[len] = '\0';

  json_object_put(user); // Libera JSON da memória

  return resp;
}

// Novo usuário: Cria um arquivo e armazena o json de entrada nele
void armazenar_info_usuario(json_object *json_obj, const char *caminho) {
  // Pega o email do JSON
  json_object *email_obj = NULL;
  if (json_object_object_get_ex(json_obj, "email", &email_obj) == 0) {
    printf("Campo 'email' não encontrado no objeto JSON\n");
    return;
  }

  const char *email = json_object_get_string(email_obj);

  // Cria arquivo com o email como seu nome
  char nome_arquivo[100];
  sprintf(nome_arquivo, "%s/%s.json", caminho, email);

  FILE *fp = fopen(nome_arquivo, "w");
  if (fp == NULL) {
    printf("Erro ao criar arquivo %s\n", nome_arquivo);
    return;
  }

  // Escreve objeto JSON no arquivo
  const char *json_str = json_object_to_json_string(json_obj);
  fprintf(fp, "%s", json_str);

  // Fecha arquivo
  fclose(fp);
}


void erro(const char *msg) {
  perror(msg);
  exit(1);
}

unsigned int clilen;
struct sockaddr_in serv_addr, cli_addr;
int sockfd;

#define TAMANHO_MAX_BUFFER_IMG 1500 // Maximum buffer size for a packet
#define MAX_PACOTES 10000

typedef struct {
    size_t tamanho_img; // tamanho da imagem que estamos enviando
    int seq_n; // qual a ordem desse pacote específico
    char data[TAMANHO_MAX_BUFFER_IMG];
} Packet;


size_t calcular_tamanho_imagem(const char* nome_arquivo) {
    FILE* arquivo = fopen(nome_arquivo, "rb");
    if (arquivo == NULL) {
        printf("Falha ao abrir o arquivo de imagem\n");
        return 0;
    }

    // Posiciona o ponteiro no final do arquivo
    fseek(arquivo, 0, SEEK_END);

    // Obtém o tamanho do arquivo
    long tamanho_arquivo = ftell(arquivo);

    // Fecha o arquivo
    fclose(arquivo);

    if (tamanho_arquivo < 0) {
        printf("Falha ao obter o tamanho do arquivo de imagem\n");
        return 0;
    }

    return (size_t)tamanho_arquivo;
}

size_t calcular_num_pacotes(size_t tamanho_imagem) {
    size_t num_pacotes = tamanho_imagem / TAMANHO_MAX_BUFFER_IMG;
    if (tamanho_imagem % TAMANHO_MAX_BUFFER_IMG != 0) {
        num_pacotes++;
    }
    return num_pacotes;
}

int enviar_imagem(const char* dados_imagem, size_t tamanho_imagem) {
    size_t num_pacotes = calcular_num_pacotes(tamanho_imagem);
    size_t bytes_enviados = 0;

    for (size_t i = 0; i < num_pacotes; i++) {
        size_t offset = i * TAMANHO_MAX_BUFFER_IMG;
        size_t tamanho_pacote = (i == num_pacotes - 1) ? tamanho_imagem - offset : TAMANHO_MAX_BUFFER_IMG;

        Packet pacote;
        pacote.tamanho_img = tamanho_imagem;
        pacote.seq_n = i;
        memcpy(pacote.data, dados_imagem + offset, tamanho_pacote);

        int bytes_enviados_pacote = sendto(sockfd, &pacote, sizeof(Packet), 0,
               (struct sockaddr *) &cli_addr, clilen);

        if (bytes_enviados_pacote < 0) {
            perror("Erro ao enviar pacote");
            return -1;
        }

        bytes_enviados += bytes_enviados_pacote;
    }

    return 0;
}

void lidar_com_comando(const char *comando) {
  json_object *json_obj = json_tokener_parse(comando);
  if (json_obj == NULL) {
    printf("Erro ao analisar o JSON\n");
    return;
  }

  json_object *command_obj, *payload_obj;
  const char* resposta;

  if (json_object_object_get_ex(json_obj, "command", &command_obj) &&
      json_object_object_get_ex(json_obj, "payload", &payload_obj)) {
    const char *command = json_object_get_string(command_obj);
    json_object *payload = payload_obj;

    if (strcmp(command, "new_user") == 0) {
      // Cria novo usuário
      printf("Criando novo usuário...\n");
      armazenar_info_usuario(payload, PASTA_USUARIOS);      
      resposta = "Novo usuário criado com sucesso!\n";
      printf(resposta);
    } else if (strcmp(command, "list_all_users") == 0) {
      // Lista todos os usuários
      printf("Listando todos os usuários...\n");
      resposta =listar_todos_usuarios(PASTA_USUARIOS);
      printf("Listagem de usuários concluída.\n");
    } else if (strcmp(command, "delete_user") == 0) {
      // Deleta usuário
      printf("Deletando usuário...\n");
      const char *email = json_object_get_string(payload);
      deletar_usuario(email, PASTA_USUARIOS);
      resposta = "Usuário deletado com sucesso!\n";
      printf("Usuário deletado com sucesso!\n");
    } else if (strcmp(command, "query_users") == 0) {
      // Filtra usuários por um de seus atributos
      printf("Filtrando usuários...\n");
      resposta = query_usuarios(payload, PASTA_USUARIOS);
      if (resposta == NULL) resposta = "Não achamos ninguém com esse atributo\n";
      printf("Filtro de usuários concluído.\n");

    } else if (strcmp(command, "list_user") == 0) {
      // Devolve usuário com o devido email
      printf("Listando usuário...\n");
      const char *email = json_object_get_string(payload);
      resposta = listar_usuario(email, PASTA_USUARIOS);
      if (resposta == NULL) resposta = "Não achamos ninguém com esse email\n";
      printf("Listagem concluída.\n");

    } else if (strcmp(command, "download_user_img") == 0) {
        // Devolve usuário com o devido email
        printf("Preparando para enviar a imagem...\n");
        const char *email = json_object_get_string(payload);

        char caminho_completo[100];
        sprintf(caminho_completo, "%s/%s.jpg", PASTA_USUARIOS, email);
        size_t tamanho_img = calcular_tamanho_imagem(caminho_completo);

        resposta = NULL;
        if (tamanho_img == 0) resposta = "Não temos a foto desse usuário";
        else {
            printf("Iniciando envio da imagem.\n");
            // Abrir o arquivo para leitura binária
            FILE* arquivo = fopen(caminho_completo, "rb");
            if (arquivo == NULL) {
                perror("Erro ao abrir arquivo");
                exit(EXIT_FAILURE);
            }

            // Alocar memória para armazenar os dados da imagem
            char* dados_imagem = (char*)malloc(tamanho_img);
            if (dados_imagem == NULL) {
                perror("Erro de alocação de memória");
                exit(EXIT_FAILURE);
            }

            // Ler os dados da imagem do arquivo
            size_t bytes_lidos = fread(dados_imagem, sizeof(char), tamanho_img, arquivo);
            if (bytes_lidos != tamanho_img) {
                perror("Erro ao ler dados da imagem");
                exit(EXIT_FAILURE);
            }

            enviar_imagem(dados_imagem, tamanho_img);

            printf("Finalizando envio de imagem\n");
            
        }
    }
    else {
      // Comando inválido
      resposta = "Comando inválido\n";
      printf("Comando inválido\n");
    }

  } else {
    // Entrada inválida/formato do JSON de entrada errado
    printf("Entrada inválida/Formato de objeto JSON inválido\n");
  }

  if (resposta != NULL)
      sendto(sockfd, resposta, strlen(resposta), 0,
         (struct sockaddr *) &cli_addr, clilen);

  json_object_put(json_obj);

}

//Função teste que imprime todos os usuários cadastrados
void imprimir_arquivos_na_pasta(const char *caminho) {
  DIR *dir;
  struct dirent *ent;

  if ((dir = opendir(caminho)) == NULL) {
    perror("Não foi possível abrir o diretório");
    return;
  }

  while ((ent = readdir(dir)) != NULL) {
    // Ignorar . e ..
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
      continue;
    }

    char caminho_completo[100];
    sprintf(caminho_completo, "%s/%s", caminho, ent->d_name);

    FILE *fp = fopen(caminho_completo, "r");
    if (fp == NULL) {
      printf("Não foi possível abrir o arquivo %s\n", caminho_completo);
      continue;
    }

    printf("Arquivo: %s\n", caminho_completo);

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fp)) {
      printf("%s", buffer);
    }

    printf("\n");
    fclose(fp);
  }

  closedir(dir);
}

int main() {
  signal(SIGINT, manipulador_sigint); // Registra manipulador de sinal SIGINT
  printf("Iniciando o servidor\n");

  // imprimir_arquivos_na_pasta(PASTA_USUARIOS); //Chamada teste

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    erro("ERRO abrindo o socket");

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(PORTA);

  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    erro("ERRO conectando");

  printf("Servidor iniciado na porta %d\n", PORTA);

  char buffer[TAMANHO_BUFFER];

  while (rodando) {
    memset(buffer, 0, sizeof(buffer));

    clilen = sizeof(cli_addr);
    int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                     (struct sockaddr *) &cli_addr, &clilen);
    if (n < 0)
      erro("ERRO lendo do socket");

    lidar_com_comando(buffer);
  }

  close(sockfd);

  return 0;
}

