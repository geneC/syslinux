/*
 * url.h
 */

#ifndef CORE_PXE_URL_H
#define CORE_PXE_URL_H

enum url_type {
    URL_NORMAL,
    URL_OLD_TFTP,
    URL_PREFIX
};

struct url_info {
    char *scheme;
    char *user;
    char *passwd;
    char *host;
    unsigned int port;
    char *path;			/* Includes query */
    enum url_type type;
};

void parse_url(struct url_info *ui, char *url);
char *url_escape_unsafe(const char *input);
char *url_unescape(char *buffer, char terminator);

#endif /* CORE_PXE_URL_H */
