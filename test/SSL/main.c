#include <stdio.h>
#include <openssl/ssl.h>

int good1() {
  SSL_CTX *ctx = NULL;
  SSL *ssl = SSL_new(ctx);
  X509 *cert = SSL_get_peer_certificate(ssl);
  if (cert != NULL) {
    if (SSL_get_verify_result(ssl) == X509_V_OK)
      return 1;
  }
  return 0;
}

int good2() {
  SSL_CTX *ctx = NULL;
  SSL *ssl = SSL_new(ctx);
  X509 *cert = SSL_get_peer_certificate(ssl);
  if (cert != NULL) {
    if (SSL_get_verify_result(ssl) == X509_V_OK)
      return 1;
  }
  return 0;
}

int good3() {
  SSL_CTX *ctx = NULL;
  SSL *ssl = SSL_new(ctx);
  X509 *cert = SSL_get_peer_certificate(ssl);
  if (cert != NULL) {
    if (SSL_get_verify_result(ssl) == X509_V_OK)
      return 1;
  }
  return 0;
}

int good4() {
  SSL_CTX *ctx = NULL;
  SSL *ssl = SSL_new(ctx);
  if (SSL_get_verify_result(ssl) == X509_V_OK) {
    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert != NULL)
      return 1;
  }
  return 0;
}

int good5() {
  SSL_CTX *ctx = NULL;
  SSL *ssl = SSL_new(ctx);
  if (SSL_get_verify_result(ssl) == X509_V_OK) {
    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert != NULL)
      return 1;
  }
  return 0;
}

int good6() {
  SSL_CTX *ctx = NULL;
  SSL *ssl = SSL_new(ctx);
  if (SSL_get_verify_result(ssl) == X509_V_OK) {
    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert != NULL)
      return 1;
  }
  return 0;
}

int bad() {
  SSL_CTX *ctx = NULL;
  SSL *ssl = SSL_new(ctx);
  X509 *cert = SSL_get_peer_certificate(ssl);
  if (cert == NULL) { // this line is buggy
    if (SSL_get_verify_result(ssl) == X509_V_OK)
      return 1;
  }
  return 0;
}
