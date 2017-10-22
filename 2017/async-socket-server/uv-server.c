#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "uv.h"

#include "utils.h"

#define N_BACKLOG 64


// Wraps malloc with error checking: dies if malloc fails.
void* xmalloc(size_t size) {
  void* ptr = malloc(size);
  if (!ptr) {
    die("malloc failed");
  }
  return ptr;
}


void on_alloc_buffer(uv_handle_t* handle, size_t suggested_size,
                     uv_buf_t* buf) {
  buf->base = (char*)xmalloc(suggested_size);
  buf->len = suggested_size;
}


void on_handle_closed(uv_handle_t* handle) {
  free(handle);
}


void on_peer_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
  if (nread < 0) {
    if (nread != UV_EOF) {
      fprintf(stderr, "Read error: %s\n", uv_strerror(nread));
    }
    // TODO: close connection here?
    //uv_close((uv_handle_t*)client, NULL);
    return;
  } else if (nread > 0) {
    printf("received %ld bytes\n", nread);
    /*write_req_t* req = (write_req_t*)malloc(sizeof(write_req_t));*/
    /*req->buf = uv_buf_init(buf->base, nread);*/
    /*uv_write((uv_write_t*)req, client, &req->buf, 1, echo_write);*/
    return;
  }

  free(buf->base);
}


void on_peer_connected(uv_stream_t* server, int status) {
  if (status < 0) {
    fprintf(stderr, "Peer connection error: %s\n", uv_strerror(status));
    return;
  }

  uv_tcp_t* client = (uv_tcp_t*)xmalloc(sizeof(*client));
  int rc;
  if ((rc = uv_tcp_init(uv_default_loop(), client)) < 0) {
    die("uv_tcp_init failed: %s", uv_strerror(rc));
  }

  if (uv_accept(server, (uv_stream_t*)client) == 0) {
    struct sockaddr peername;
    int namelen;

    if ((rc = uv_tcp_getpeername(client, &peername, &namelen)) < 0) {
      die("uv_tcp_getpeername failed: %s", uv_strerror(rc));
    }
    report_peer_connected((const struct sockaddr_in*)&peername, namelen);
    uv_read_start((uv_stream_t*)client, on_alloc_buffer, on_peer_read);
  } else {
    uv_close((uv_handle_t*)client, NULL);
  }
}


int main(int argc, const char** argv) {
  setvbuf(stdout, NULL, _IONBF, 0);

  int portnum = 9090;
  if (argc >= 2) {
    portnum = atoi(argv[1]);
  }
  printf("Serving on port %d\n", portnum);

  int rc;
  uv_tcp_t server;
  if ((rc = uv_tcp_init(uv_default_loop(), &server)) < 0) {
    die("uv_tcp_init failed: %s", uv_strerror(rc));
  }

  struct sockaddr_in addr;
  if ((rc = uv_ip4_addr("0.0.0.0", portnum, &addr)) < 0) {
    die("uv_ip4_addr failed: %s", uv_strerror(rc));
  }

  if ((rc = uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0)) < 0) {
    die("uv_tcp_bind failed: %s", uv_strerror(rc));
  }

  if ((rc = uv_listen((uv_stream_t*)&server, N_BACKLOG,
                      on_peer_connected)) < 0) {
    die("uv_listen failed: %s", uv_strerror(rc));
  }

  return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
