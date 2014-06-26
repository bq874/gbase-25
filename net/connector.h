#ifndef CONNECTOR_H_
#define CONNECTOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "core/os_def.h"
#include "base/connbuffer.h"
#include "net/sock.h"
#include "net/reactor.h"

// return buffer size processed, return -1 means process fail, reactor will close it
typedef int32_t (*connector_read_func)(sock_t sock,
                                       const char* buffer,
                                       int32_t buflen);
typedef void (*connector_close_func)(sock_t sock);

struct connector_t;
struct connector_t* connector_create(struct reactor_t* r,
                                     connector_read_func read_cb,
                                     connector_close_func close_cb,
                                     struct connbuffer_t* read_buf,
                                     struct connbuffer_t* write_buf);
int32_t connector_release(struct connector_t* con);
int32_t connector_fd(struct connector_t* con);
void connector_set_fd(struct connector_t* con, sock_t sock);
int32_t connector_start(struct connector_t* con);
int32_t connector_stop(struct connector_t* con);

//  return = 0 success
//  return < 0, fail, maybe full
int32_t connector_send(struct connector_t* con, const char* buffer, int buflen);

#ifdef __cplusplus
}
#endif

#endif // CONNECTOR_H_



