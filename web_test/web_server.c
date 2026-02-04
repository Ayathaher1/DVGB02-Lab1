#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

static void send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n <= 0) return;
        p += (size_t)n;
        len -= (size_t)n;
    }
}

static const char *ctype(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    ext++;
    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) return "text/html";
    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, "png") == 0) return "image/png";
    return "application/octet-stream";
}

static void send_404(int cfd) {
    const char *body = "<!doctype html><html><body><h1>404 Not Found</h1></body></html>\n";
    char hdr[256];
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        strlen(body));
    send_all(cfd, hdr, (size_t)hlen);
    send_all(cfd, body, strlen(body));
}

static void send_file(int cfd, const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) < 0 || !S_ISREG(st.st_mode)) {
        send_404(cfd);
        return;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        send_404(cfd);
        return;
    }

    char hdr[512];
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n\r\n",
        ctype(filepath), (long long)st.st_size);

    send_all(cfd, hdr, (size_t)hlen);

    char buf[8192];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        send_all(cfd, buf, (size_t)n);
    }
    close(fd);
}

int main(void) {
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); return 1; }

    int on = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(sfd, 10) < 0) { perror("listen"); return 1; }

    char root[PATH_MAX];
    if (!getcwd(root, sizeof(root))) { perror("getcwd"); return 1; }

    while (1) {
        int cfd = accept(sfd, NULL, NULL);
        if (cfd < 0) continue;

        char req[4096] = {0};
        ssize_t rn = read(cfd, req, sizeof(req) - 1);
        if (rn <= 0) { close(cfd); continue; }

        char method[16] = {0}, path[1024] = {0};
        if (sscanf(req, "%15s %1023s", method, path) != 2) {
            send_404(cfd);
            close(cfd);
            continue;
        }

        if (strcmp(method, "GET") != 0) {
            send_404(cfd);
            close(cfd);
            continue;
        }

        if (strstr(path, "..")) {
            send_404(cfd);
            close(cfd);
            continue;
        }

        char filepath[PATH_MAX];
        if (strcmp(path, "/") == 0) {
            snprintf(filepath, sizeof(filepath), "%s/index.html", root);
        } else {
            snprintf(filepath, sizeof(filepath), "%s%s", root, path);
        }

        send_file(cfd, filepath);
        close(cfd);
    }
}
