#ifndef __UV_EXTRA_H__
#define __UV_EXTRA_H__

#include <uv.h>

#ifdef __cplusplus
extern "C" {
#endif

struct uv_export_t;

int uv_export_start(uv_loop_t *loop, uv_stream_t *handle, const char* ipc_name, unsigned short count, struct uv_export_t **exp);
int uv_export_finish(struct uv_export_t *exp);

int uv_import(uv_loop_t *loop, uv_stream_t *handle, struct uv_export_t *exp);

#ifdef __cplusplus
}
#endif

#endif
