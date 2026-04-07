/**
 * @file chess_piece_http.c
 * @brief GET /static/piece/[name].png — PNG figurky (embed), stejné jako v iOS Assets.
 */

#include "chess_piece_http.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>

static const char TAG_PIECE[] = "piece_png";

/* ESP-IDF EMBED_FILES: piece_assets/<name>.png */
extern const char _binary_PieceWhiteKing_png_start[] asm(
    "_binary_PieceWhiteKing_png_start");
extern const char _binary_PieceWhiteKing_png_end[] asm(
    "_binary_PieceWhiteKing_png_end");
extern const char _binary_PieceWhiteQueen_png_start[] asm(
    "_binary_PieceWhiteQueen_png_start");
extern const char _binary_PieceWhiteQueen_png_end[] asm(
    "_binary_PieceWhiteQueen_png_end");
extern const char _binary_PieceWhiteRook_png_start[] asm(
    "_binary_PieceWhiteRook_png_start");
extern const char _binary_PieceWhiteRook_png_end[] asm(
    "_binary_PieceWhiteRook_png_end");
extern const char _binary_PieceWhiteBishop_png_start[] asm(
    "_binary_PieceWhiteBishop_png_start");
extern const char _binary_PieceWhiteBishop_png_end[] asm(
    "_binary_PieceWhiteBishop_png_end");
extern const char _binary_PieceWhiteKnight_png_start[] asm(
    "_binary_PieceWhiteKnight_png_start");
extern const char _binary_PieceWhiteKnight_png_end[] asm(
    "_binary_PieceWhiteKnight_png_end");
extern const char _binary_PieceWhitePawn_png_start[] asm(
    "_binary_PieceWhitePawn_png_start");
extern const char _binary_PieceWhitePawn_png_end[] asm(
    "_binary_PieceWhitePawn_png_end");
extern const char _binary_PieceBlackKing_png_start[] asm(
    "_binary_PieceBlackKing_png_start");
extern const char _binary_PieceBlackKing_png_end[] asm(
    "_binary_PieceBlackKing_png_end");
extern const char _binary_PieceBlackQueen_png_start[] asm(
    "_binary_PieceBlackQueen_png_start");
extern const char _binary_PieceBlackQueen_png_end[] asm(
    "_binary_PieceBlackQueen_png_end");
extern const char _binary_PieceBlackRook_png_start[] asm(
    "_binary_PieceBlackRook_png_start");
extern const char _binary_PieceBlackRook_png_end[] asm(
    "_binary_PieceBlackRook_png_end");
extern const char _binary_PieceBlackBishop_png_start[] asm(
    "_binary_PieceBlackBishop_png_start");
extern const char _binary_PieceBlackBishop_png_end[] asm(
    "_binary_PieceBlackBishop_png_end");
extern const char _binary_PieceBlackKnight_png_start[] asm(
    "_binary_PieceBlackKnight_png_start");
extern const char _binary_PieceBlackKnight_png_end[] asm(
    "_binary_PieceBlackKnight_png_end");
extern const char _binary_PieceBlackPawn_png_start[] asm(
    "_binary_PieceBlackPawn_png_start");
extern const char _binary_PieceBlackPawn_png_end[] asm(
    "_binary_PieceBlackPawn_png_end");

typedef struct {
  const char *start;
  const char *end;
} piece_blob_t;

static esp_err_t http_send_piece_blob(httpd_req_t *req) {
  const piece_blob_t *b = (const piece_blob_t *)req->user_ctx;
  if (b == NULL || b->start == NULL || b->end == NULL || b->end <= b->start) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    return httpd_resp_send(req, "Bad asset", HTTPD_RESP_USE_STRLEN);
  }
  size_t len = (size_t)(b->end - b->start);
  httpd_resp_set_type(req, "image/png");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=604800");
  return httpd_resp_send(req, b->start, len);
}

static const piece_blob_t blob_PieceWhiteKing = {
    _binary_PieceWhiteKing_png_start,
    _binary_PieceWhiteKing_png_end};
static const piece_blob_t blob_PieceWhiteQueen = {
    _binary_PieceWhiteQueen_png_start,
    _binary_PieceWhiteQueen_png_end};
static const piece_blob_t blob_PieceWhiteRook = {
    _binary_PieceWhiteRook_png_start,
    _binary_PieceWhiteRook_png_end};
static const piece_blob_t blob_PieceWhiteBishop = {
    _binary_PieceWhiteBishop_png_start,
    _binary_PieceWhiteBishop_png_end};
static const piece_blob_t blob_PieceWhiteKnight = {
    _binary_PieceWhiteKnight_png_start,
    _binary_PieceWhiteKnight_png_end};
static const piece_blob_t blob_PieceWhitePawn = {
    _binary_PieceWhitePawn_png_start,
    _binary_PieceWhitePawn_png_end};
static const piece_blob_t blob_PieceBlackKing = {
    _binary_PieceBlackKing_png_start,
    _binary_PieceBlackKing_png_end};
static const piece_blob_t blob_PieceBlackQueen = {
    _binary_PieceBlackQueen_png_start,
    _binary_PieceBlackQueen_png_end};
static const piece_blob_t blob_PieceBlackRook = {
    _binary_PieceBlackRook_png_start,
    _binary_PieceBlackRook_png_end};
static const piece_blob_t blob_PieceBlackBishop = {
    _binary_PieceBlackBishop_png_start,
    _binary_PieceBlackBishop_png_end};
static const piece_blob_t blob_PieceBlackKnight = {
    _binary_PieceBlackKnight_png_start,
    _binary_PieceBlackKnight_png_end};
static const piece_blob_t blob_PieceBlackPawn = {
    _binary_PieceBlackPawn_png_start,
    _binary_PieceBlackPawn_png_end};

static const struct {
  const char *uri;
  const piece_blob_t *blob;
} s_piece_routes[] = {
    {"/static/piece/PieceWhiteKing.png", &blob_PieceWhiteKing},
    {"/static/piece/PieceWhiteQueen.png", &blob_PieceWhiteQueen},
    {"/static/piece/PieceWhiteRook.png", &blob_PieceWhiteRook},
    {"/static/piece/PieceWhiteBishop.png", &blob_PieceWhiteBishop},
    {"/static/piece/PieceWhiteKnight.png", &blob_PieceWhiteKnight},
    {"/static/piece/PieceWhitePawn.png", &blob_PieceWhitePawn},
    {"/static/piece/PieceBlackKing.png", &blob_PieceBlackKing},
    {"/static/piece/PieceBlackQueen.png", &blob_PieceBlackQueen},
    {"/static/piece/PieceBlackRook.png", &blob_PieceBlackRook},
    {"/static/piece/PieceBlackBishop.png", &blob_PieceBlackBishop},
    {"/static/piece/PieceBlackKnight.png", &blob_PieceBlackKnight},
    {"/static/piece/PieceBlackPawn.png", &blob_PieceBlackPawn},
};

esp_err_t chess_piece_register_http_uris(httpd_handle_t hd) {
  if (hd == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  for (size_t i = 0; i < sizeof(s_piece_routes) / sizeof(s_piece_routes[0]); i++) {
    httpd_uri_t u = {.uri = s_piece_routes[i].uri,
                     .method = HTTP_GET,
                     .handler = http_send_piece_blob,
                     .user_ctx = (void *)s_piece_routes[i].blob};
    esp_err_t e = httpd_register_uri_handler(hd, &u);
    if (e != ESP_OK) {
      ESP_LOGE(TAG_PIECE, "register %s failed: %s", s_piece_routes[i].uri,
               esp_err_to_name(e));
      return e;
    }
  }
  ESP_LOGI(TAG_PIECE, "registered %zu piece PNG URIs",
           sizeof(s_piece_routes) / sizeof(s_piece_routes[0]));
  return ESP_OK;
}
