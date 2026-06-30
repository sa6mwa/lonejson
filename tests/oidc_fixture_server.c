#include "lonejson.h"

#include <curl/curl.h>

#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

typedef struct fixture_http_sink {
  lonejson_http_response *response;
  size_t max_response_bytes;
  lonejson_error *error;
} fixture_http_sink;

typedef struct fixture_server {
  lonejson *runtime;
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy cache_policy;
  lonejson_jwt_claim_policy claim_policy;
  const char *issuer;
  const char *audience;
  int port;
  int max_requests;
} fixture_server;

static size_t fixture_curl_write(char *ptr, size_t size, size_t nmemb,
                                 void *userdata) {
  fixture_http_sink *sink = (fixture_http_sink *)userdata;
  size_t bytes;

  if (sink == NULL || sink->response == NULL ||
      (ptr == NULL && size != 0u && nmemb != 0u)) {
    return 0u;
  }
  if (size != 0u && nmemb > SIZE_MAX / size) {
    return 0u;
  }
  bytes = size * nmemb;
  if (bytes == 0u) {
    return 0u;
  }
  if (sink->max_response_bytes != 0u &&
      (sink->response->body.len > sink->max_response_bytes ||
       bytes > sink->max_response_bytes - sink->response->body.len)) {
    return 0u;
  }
  return lonejson_owned_buffer_sink(&sink->response->body, ptr, bytes,
                                    sink->error) == LONEJSON_STATUS_OK
             ? bytes
             : 0u;
}

static lonejson_status
fixture_http_request(void *user_data, const lonejson_http_request *request,
                     lonejson_http_response *response, lonejson_error *error) {
  CURL *curl;
  CURLcode code;
  struct curl_slist *headers = NULL;
  char content_type_header[160];
  char authorization_header[1152];
  fixture_http_sink sink;
  curl_off_t body_len;
  const char *cainfo;

  (void)user_data;
  if (request == NULL || response == NULL || request->method == NULL ||
      request->url == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (strcmp(request->method, "GET") != 0 &&
      strcmp(request->method, "POST") != 0) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  body_len = (curl_off_t)request->body_len;
  if ((size_t)body_len != request->body_len) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  lonejson_http_response_cleanup(response);
  lonejson_http_response_init(response);
  curl = curl_easy_init();
  if (curl == NULL) {
    return LONEJSON_STATUS_IO_ERROR;
  }
  sink.response = response;
  sink.max_response_bytes = request->max_response_bytes;
  sink.error = error;
  code = curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  if (code == CURLE_OK) {
    code = curl_easy_setopt(curl, CURLOPT_URL, request->url);
  }
  if (code == CURLE_OK && request->user_agent != NULL) {
    code = curl_easy_setopt(curl, CURLOPT_USERAGENT, request->user_agent);
  }
  cainfo = getenv("LONEJSON_OIDC_E2E_CAINFO");
  if (code == CURLE_OK && cainfo != NULL && cainfo[0] != '\0') {
    code = curl_easy_setopt(curl, CURLOPT_CAINFO, cainfo);
  }
  if (code == CURLE_OK) {
    code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fixture_curl_write);
  }
  if (code == CURLE_OK) {
    code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
  }
  if (code == CURLE_OK && request->authorization != NULL) {
    snprintf(authorization_header, sizeof(authorization_header),
             "Authorization: %.1128s", request->authorization);
    headers = curl_slist_append(headers, authorization_header);
    if (headers == NULL) {
      curl_easy_cleanup(curl);
      return LONEJSON_STATUS_ALLOCATION_FAILED;
    }
  }
  if (code == CURLE_OK && strcmp(request->method, "POST") == 0) {
    code = curl_easy_setopt(curl, CURLOPT_POST, 1L);
    if (code == CURLE_OK) {
      code = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body);
    }
    if (code == CURLE_OK) {
      code = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, body_len);
    }
    if (code == CURLE_OK && request->content_type != NULL) {
      struct curl_slist *next;
      snprintf(content_type_header, sizeof(content_type_header),
               "Content-Type: %.120s", request->content_type);
      next = curl_slist_append(headers, content_type_header);
      if (next == NULL) {
        if (headers != NULL) {
          curl_slist_free_all(headers);
        }
        curl_easy_cleanup(curl);
        return LONEJSON_STATUS_ALLOCATION_FAILED;
      }
      headers = next;
    }
  }
  if (code == CURLE_OK && headers != NULL) {
    code = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }
  if (code == CURLE_OK) {
    code = curl_easy_perform(curl);
  }
  if (code == CURLE_OK) {
    code =
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status_code);
  }
  if (headers != NULL) {
    curl_slist_free_all(headers);
  }
  curl_easy_cleanup(curl);
  if (code != CURLE_OK) {
    lonejson_http_response_cleanup(response);
    return LONEJSON_STATUS_IO_ERROR;
  }
  return LONEJSON_STATUS_OK;
}

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

static void handle_client(fixture_server *server, int client_fd) {
  char request[16384];
  ssize_t got;
  char *path;
  const char *authorization;
  lonejson_oidc_bearer_validation_request validation_request;
  lonejson_oidc_bearer_validation validation;
  lonejson_error error;
  lonejson_status status;

  got = read(client_fd, request, sizeof(request) - 1u);
  if (got <= 0) {
    return;
  }
  request[got] = '\0';
  if (strncmp(request, "GET ", 4u) != 0) {
    respond_json(client_fd, 401, "{\"ok\":false,\"error\":\"unsupported\"}");
    return;
  }
  path = request + 4u;
  if (strncmp(path, "/health ", 8u) == 0) {
    respond_json(client_fd, 200, "{\"ok\":true}");
    return;
  }
  if (strncmp(path, "/protected ", 11u) != 0) {
    respond_json(client_fd, 401, "{\"ok\":false,\"error\":\"not_found\"}");
    return;
  }
  authorization = find_authorization_header(request);
  lonejson_error_init(&error);
  lonejson_oidc_bearer_validation_init(&validation);
  memset(&validation_request, 0, sizeof(validation_request));
  validation_request.authorization_header = authorization;
  validation_request.jwks_cache = &server->cache;
  validation_request.jwks_policy = &server->cache_policy;
  validation_request.claim_policy = &server->claim_policy;
  status = lonejson_oidc_validate_bearer_token(
      server->runtime, &validation_request, &validation, &error);
  if (status == LONEJSON_STATUS_OK &&
      validation.failure == LONEJSON_AUTH_FAILURE_NONE) {
    respond_json(client_fd, 200, "{\"ok\":true,\"authorized\":true}");
  } else {
    respond_json(client_fd, 401, "{\"ok\":false,\"authorized\":false}");
  }
  lonejson_oidc_bearer_validation_cleanup(&validation);
}

static int run_server(fixture_server *server) {
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
  printf("listening on 127.0.0.1:%d\n", server->port);
  fflush(stdout);
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
  lonejson_auth_provider auth_provider;
  lonejson_http_provider http_provider;
  lonejson_oidc_discovery discovery;
  fixture_server server;
  const char *algs[] = {"RS256"};
  const char *issuers[1];
  const char *audiences[1];
  const char *required_scopes[] = {"openid", "profile"};
  const char *admin_access_token;
  const char *refresh_token;
  lonejson_oauth2_token_introspection introspection_request;
  lonejson_oauth2_introspection_response introspection;
  lonejson_oidc_userinfo_request userinfo_request;
  lonejson_oidc_userinfo_response userinfo;
  lonejson_oauth2_token_revocation revocation_request;
  lonejson_oauth2_token_flow flow;
  lonejson_oauth2_token_flow_policy flow_policy;
  lonejson_oauth2_token_flow_result flow_result;
  lonejson_oauth2_token_response flow_response;
  lonejson_status status;

  if (argc != 5 && argc != 7) {
    fprintf(stderr,
            "usage: %s ISSUER AUDIENCE PORT MAX_REQUESTS [ACCESS_TOKEN "
            "REFRESH_TOKEN]\n",
            argv[0]);
    return 2;
  }
  memset(&server, 0, sizeof(server));
  server.issuer = argv[1];
  server.audience = argv[2];
  server.port = atoi(argv[3]);
  server.max_requests = atoi(argv[4]);
  admin_access_token = argc == 7 ? argv[5] : NULL;
  refresh_token = argc == 7 ? argv[6] : NULL;
  if (server.port <= 0 || server.max_requests <= 0) {
    return 2;
  }

  signal(SIGPIPE, SIG_IGN);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  lonejson_error_init(&error);
  lonejson_oidc_discovery_init(&discovery);
  lonejson_oidc_jwks_cache_init(&server.cache);
  server.runtime = lonejson_new(NULL, &error);
  if (server.runtime == NULL) {
    fprintf(stderr, "lonejson_new failed: %s\n", error.message);
    return 1;
  }
  status = lonejson_auth_provider_init_openssl(&auth_provider, NULL, &error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_set_auth_provider(server.runtime, &auth_provider, &error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_http_provider_init_simple(&http_provider, NULL,
                                                "lonejson-oidc-e2e/1",
                                                fixture_http_request, &error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_set_http_provider(server.runtime, &http_provider, &error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oidc_fetch_discovery(server.runtime, server.issuer,
                                           256u * 1024u, &discovery, &error);
  }
  if (status != LONEJSON_STATUS_OK) {
    fprintf(stderr, "OIDC setup failed: %s\n", error.message);
    return 1;
  }

  memset(&server.cache_policy, 0, sizeof(server.cache_policy));
  server.cache_policy.issuer = server.issuer;
  server.cache_policy.jwks_uri = discovery.jwks_uri;
  server.cache_policy.max_jwks_bytes = 1024u * 1024u;
  server.cache_policy.now = (lonejson_int64)time(NULL);
  server.cache_policy.ttl_seconds = 300;
  status = lonejson_oidc_jwks_cache_refresh(server.runtime, &server.cache,
                                            &server.cache_policy, &error);
  if (status != LONEJSON_STATUS_OK) {
    fprintf(stderr, "JWKS refresh failed: %s\n", error.message);
    return 1;
  }

  if (admin_access_token != NULL && refresh_token != NULL) {
    if (discovery.introspection_endpoint == NULL ||
        discovery.revocation_endpoint == NULL ||
        discovery.userinfo_endpoint == NULL) {
      fprintf(stderr, "OIDC discovery did not advertise admin endpoints\n");
      return 1;
    }

    memset(&introspection_request, 0, sizeof(introspection_request));
    introspection_request.token = admin_access_token;
    introspection_request.token_type_hint = "access_token";
    introspection_request.client_id = "lonejson-m2m";
    introspection_request.client_secret = "lonejson-secret";
    introspection_request.use_basic_auth = 1;
    lonejson_oauth2_introspection_response_init(&introspection);
    status = lonejson_oauth2_introspect_token_request(
        server.runtime, discovery.introspection_endpoint,
        &introspection_request, 256u * 1024u, &introspection, &error);
    if (status != LONEJSON_STATUS_OK || !introspection.has_active ||
        !introspection.active) {
      fprintf(stderr, "token introspection failed: %s\n", error.message);
      lonejson_oauth2_introspection_response_cleanup(&introspection);
      return 1;
    }
    lonejson_oauth2_introspection_response_cleanup(&introspection);

    memset(&userinfo_request, 0, sizeof(userinfo_request));
    userinfo_request.access_token = admin_access_token;
    lonejson_oidc_userinfo_response_init(&userinfo);
    status = lonejson_oidc_fetch_userinfo(server.runtime,
                                          discovery.userinfo_endpoint,
                                          &userinfo_request, &userinfo, &error);
    if (status != LONEJSON_STATUS_OK || userinfo.json == NULL ||
        userinfo.len == 0u) {
      fprintf(stderr, "userinfo request failed: %s\n", error.message);
      lonejson_oidc_userinfo_response_cleanup(&userinfo);
      return 1;
    }
    lonejson_oidc_userinfo_response_cleanup(&userinfo);

    lonejson_oauth2_token_flow_init(&flow);
    memset(&flow_response, 0, sizeof(flow_response));
    flow_response.access_token = (char *)admin_access_token;
    flow_response.token_type = (char *)"Bearer";
    flow_response.refresh_token = (char *)refresh_token;
    flow_response.expires_in = 0;
    flow_response.has_expires_in = 1;
    status = lonejson_oauth2_token_flow_update_response(
        &flow, &flow_response, (lonejson_int64)time(NULL) - 1, &error);
    if (status != LONEJSON_STATUS_OK) {
      fprintf(stderr, "token flow setup failed: %s\n", error.message);
      lonejson_oauth2_token_flow_cleanup(&flow);
      return 1;
    }
    memset(&flow_policy, 0, sizeof(flow_policy));
    flow_policy.token_endpoint = discovery.token_endpoint;
    flow_policy.client_id = "lonejson-m2m";
    flow_policy.client_secret = "lonejson-secret";
    flow_policy.now = (lonejson_int64)time(NULL);
    flow_policy.max_response_bytes = 256u * 1024u;
    status = lonejson_oauth2_token_flow_ensure(
        server.runtime, &flow, &flow_policy, &flow_result, &error);
    if (status != LONEJSON_STATUS_OK ||
        flow_result.state != LONEJSON_OAUTH2_TOKEN_FLOW_REFRESHED ||
        flow.access_token == NULL) {
      fprintf(stderr, "token flow refresh failed: %s\n", error.message);
      lonejson_oauth2_token_flow_cleanup(&flow);
      return 1;
    }

    memset(&revocation_request, 0, sizeof(revocation_request));
    revocation_request.token = flow.refresh_token;
    revocation_request.token_type_hint = "refresh_token";
    revocation_request.client_id = "lonejson-m2m";
    revocation_request.client_secret = "lonejson-secret";
    revocation_request.use_basic_auth = 1;
    status = lonejson_oauth2_revoke_token_request(server.runtime,
                                                  discovery.revocation_endpoint,
                                                  &revocation_request, &error);
    if (status != LONEJSON_STATUS_OK) {
      fprintf(stderr, "token revocation failed: %s\n", error.message);
      lonejson_oauth2_token_flow_cleanup(&flow);
      return 1;
    }
    lonejson_oauth2_token_flow_cleanup(&flow);
  }

  issuers[0] = server.issuer;
  audiences[0] = server.audience;
  memset(&server.claim_policy, 0, sizeof(server.claim_policy));
  server.claim_policy.accepted_algs = algs;
  server.claim_policy.accepted_alg_count = 1u;
  server.claim_policy.accepted_issuers = issuers;
  server.claim_policy.accepted_issuer_count = 1u;
  server.claim_policy.accepted_audiences = audiences;
  server.claim_policy.accepted_audience_count = 1u;
  server.claim_policy.expected_azp = "lonejson-m2m";
  server.claim_policy.required_scopes = required_scopes;
  server.claim_policy.required_scope_count =
      sizeof(required_scopes) / sizeof(required_scopes[0]);
  server.claim_policy.now = (lonejson_int64)time(NULL);
  server.claim_policy.allowed_clock_skew = 60;

  status = (lonejson_status)run_server(&server);
  lonejson_oidc_jwks_cache_cleanup(&server.cache);
  lonejson_oidc_discovery_cleanup(&discovery);
  lonejson_free(server.runtime);
  curl_global_cleanup();
  return status == LONEJSON_STATUS_OK ? 0 : 1;
}
