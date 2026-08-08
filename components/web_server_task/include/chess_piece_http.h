#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Registers GET /static/piece/Piece*.png (12 files from embed). */
esp_err_t chess_piece_register_http_uris(httpd_handle_t hd);

#ifdef __cplusplus
}
#endif
