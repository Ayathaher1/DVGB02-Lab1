#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static void send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n <= 0) return;
        p += (size_t)n;
        len -= (size_t)n;
    }
}

static const char *guess_content_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    ext++;

    if (strcmp(ext, "html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, "png") == 0) return "image/png";
    return "application/octet-stream";
}

static void send_404(int client_fd) {
    const char *body =
        "<!doctype html><html><body>"
        "<h1>404 Not Found</h1>"
        "</body></html>\n";

    char header[256];
    int len = snprintf(header, sizeof(header),
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        strlen(body));

    send_all(client_fd, header, len);
    send_all(client_fd, body, strlen(body));
}

static void send_file(int client_fd, const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) < 0) {
        send_404(client_fd);
        return;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        send_404(client_fd);
        return;
    }

    const char *ctype = guess_content_type(filepath);

    char header[256];
    int len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n\r\n",
        ctype, (long long)st.st_size);

    send_all(client_fd, header, len);

    char buf[8192];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        send_all(client_fd, buf, n);
    }

    close(fd);
}

int main(int argc, char *argv[]) {

    const char *base_dir =
        "/Users/ayathaher/Desktop/DVGB02-Lab1/sample_website";

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) die("socket");

    int on = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        die("bind");

    if (listen(server_fd, 10) < 0)
        die("listen");

    printf("Webbserver lyssnar på: http://localhost:8080\n");
    printf("Serverar filer från: %s\n", base_dir);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;

        char req[4096] = {0};
        read(client_fd, req, sizeof(req) - 1);

        char method[16], path[1024];
        sscanf(req, "%15s %1023s", method, path);

        printf("%s %s\n", method, path);

        char filepath[2048];

        if (strcmp(path, "/") == 0) {
            snprintf(filepath, sizeof(filepath),
                     "%s/index.html", base_dir);
        } else {
            snprintf(filepath, sizeof(filepath),
                     "%s%s", base_dir, path);
        }

        send_file(client_fd, filepath);
        close(client_fd);
    }

    return 0;
}
