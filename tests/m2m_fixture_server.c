#include "lonejson.h"

#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct m2m_fixture_server {
  lonejson *runtime;
  lonejson_owned_buffer store_json;
  int port;
  int max_requests;
} m2m_fixture_server;

static int write_all(int fd, const char *data, size_t len) {
  while (len != 0u) {
    ssize_t wrote = write(fd, data, len);
    if (wrote < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    data += (size_t)wrote;
    len -= (size_t)wrote;
  }
  return 0;
}

static void respond_json(int fd, int status, const char *body) {
  const char *reason = status == 200 ? "OK" : "Unauthorized";
  char header[256];
  int header_len = snprintf(header, sizeof(header),
                            "HTTP/1.1 %d %s\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: %lu\r\n"
                            "Connection: close\r\n\r\n",
                            status, reason, (unsigned long)strlen(body));
  if (header_len > 0) {
    (void)write_all(fd, header, (size_t)header_len);
    (void)write_all(fd, body, strlen(body));
  }
}

static const char *find_authorization_header(char *request) {
  char *line = request;

  while (line != NULL && *line != '\0') {
    char *next = strstr(line, "\r\n");
    if (next != NULL) {
      *next = '\0';
      next += 2;
    }
    if (strncmp(line, "Authorization:", strlen("Authorization:")) == 0) {
      char *value = line + strlen("Authorization:");
      while (*value == ' ' || *value == '\t') {
        ++value;
      }
      return value;
    }
    line = next;
  }
  return NULL;
}

static void handle_client(m2m_fixture_server *server, int client_fd) {
  char request_bytes[8192];
  ssize_t got;
  char *path;
  const char *authorization;
  lonejson_m2m_store store;
  lonejson_m2m_verify_request verify;
  lonejson_m2m_authentication auth;
  lonejson_error error;
  lonejson_status status;

  got = read(client_fd, request_bytes, sizeof(request_bytes) - 1u);
  if (got <= 0) {
    return;
  }
  request_bytes[got] = '\0';
  if (strncmp(request_bytes, "GET ", 4u) != 0) {
    respond_json(client_fd, 401, "{\"ok\":false,\"error\":\"unsupported\"}");
    return;
  }
  path = request_bytes + 4u;
  if (strncmp(path, "/health ", 8u) == 0) {
    respond_json(client_fd, 200, "{\"ok\":true}");
    return;
  }
  if (strncmp(path, "/protected ", 11u) != 0) {
    respond_json(client_fd, 401, "{\"ok\":false,\"error\":\"not_found\"}");
    return;
  }

  memset(&store, 0, sizeof(store));
  memset(&verify, 0, sizeof(verify));
  lonejson_error_init(&error);
  lonejson_m2m_authentication_init(&auth);
  authorization = find_authorization_header(request_bytes);
  store.json = server->store_json.data;
  store.len = server->store_json.len;
  verify.store = &store;
  verify.authorization_header = authorization;
  status = lonejson_m2m_verify_authorization(server->runtime, &verify, &auth,
                                             &error);
  if (status == LONEJSON_STATUS_OK &&
      auth.failure == LONEJSON_AUTH_FAILURE_NONE) {
    respond_json(client_fd, 200, "{\"ok\":true,\"authorized\":true}");
  } else {
    respond_json(client_fd, 401, "{\"ok\":false,\"authorized\":false}");
  }
  lonejson_m2m_authentication_cleanup(&auth);
}

static int run_server(m2m_fixture_server *server) {
  int listen_fd;
  int yes = 1;
  struct sockaddr_in addr;
  int handled = 0;

  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    return 1;
  }
  (void)setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons((uint16_t)server->port);
  if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
      listen(listen_fd, 8) != 0) {
    close(listen_fd);
    return 1;
  }
  while (handled < server->max_requests) {
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      close(listen_fd);
      return 1;
    }
    handle_client(server, client_fd);
    close(client_fd);
    ++handled;
  }
  close(listen_fd);
  return 0;
}

int main(int argc, char **argv) {
  lonejson_error error;
  lonejson_auth_provider provider;
  lonejson_config config;
  lonejson_m2m_credential_request request;
  lonejson_m2m_credential credential;
  m2m_fixture_server server;
  FILE *secrets;
  lonejson_status status;

  if (argc != 4) {
    fprintf(stderr, "usage: %s PORT SECRETS_PATH MAX_REQUESTS\n", argv[0]);
    return 2;
  }
  memset(&server, 0, sizeof(server));
  memset(&request, 0, sizeof(request));
  lonejson_m2m_credential_init(&credential);
  lonejson_error_init(&error);
  lonejson_owned_buffer_init(&server.store_json);
  server.port = atoi(argv[1]);
  server.max_requests = atoi(argv[3]);
  if (server.port <= 0 || server.max_requests <= 0) {
    return 2;
  }

  signal(SIGPIPE, SIG_IGN);
  config = lonejson_default_config();
  status = lonejson_auth_provider_init_openssl(&provider, NULL, &error);
  if (status == LONEJSON_STATUS_OK) {
    config.auth_provider = &provider;
    server.runtime = lonejson_new(&config, &error);
  }
  if (status != LONEJSON_STATUS_OK || server.runtime == NULL) {
    fprintf(stderr, "runtime setup failed: %s\n", error.message);
    return 1;
  }

  request.claim_json = "{\"scope\":[\"read\"],\"fixture\":\"m2m\"}";
  status = lonejson_m2m_credential_generate(server.runtime, &request,
                                            &credential, &error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_owned_buffer_sink(&server.store_json,
                                        "{\"credentials\":[", 16u, &error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_owned_buffer_sink(&server.store_json,
                                        credential.record_json.data,
                                        credential.record_json.len, &error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_owned_buffer_sink(&server.store_json, "]}", 2u, &error);
  }
  if (status != LONEJSON_STATUS_OK) {
    fprintf(stderr, "credential setup failed: %s\n", error.message);
    return 1;
  }

  secrets = fopen(argv[2], "wb");
  if (secrets == NULL) {
    perror("open secrets");
    return 1;
  }
  fprintf(
      secrets,
      "{\"client_id\":\"%s\",\"client_secret\":\"%s\",\"api_key\":\"%s\"}\n",
      credential.client_id, credential.client_secret, credential.api_key);
  fclose(secrets);

  status = (lonejson_status)run_server(&server);
  lonejson_m2m_credential_cleanup(&credential);
  lonejson_owned_buffer_free(&server.store_json);
  lonejson_free(server.runtime);
  return status == LONEJSON_STATUS_OK ? 0 : 1;
}
