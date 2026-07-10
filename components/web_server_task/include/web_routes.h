/**
 * @file web_routes.h
 * @brief HTTP URI route registration for web_server_task.
 */

#ifndef WEB_ROUTES_H
#define WEB_ROUTES_H

#include "esp_err.h"
#include "esp_http_server.h"

#define WEB_HTTP_SERVER_PORT 80

esp_err_t web_routes_register(httpd_handle_t handle);

#endif /* WEB_ROUTES_H */
