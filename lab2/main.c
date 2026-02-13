#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

#include "circularBuffer.h"
#include "splitCommand.h"

#define CB_SIZE 4096

static void trim_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '\n') s[n - 1] = '\0';
}

static void reap_zombies(void) {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {
        // reaping background children
    }
}

/*
 * Llegeix una línia (fins '\n') utilitzant read() + CircularBuffer.
 * Retorna un char* (malloc) amb la línia (incloent '\n' si hi era).
 * Retorna NULL si EOF i no queda res pendent.
 */
static char *read_line_cb(CircularBuffer *cb, int *reachedEOF) {
    while (1) {
        int sz = buffer_size_next_element(cb, (unsigned char)'\n', *reachedEOF);
        if (sz > 0) {
            char *line = (char*)malloc((size_t)sz + 1);
            if (!line) return NULL;

            for (int i = 0; i < sz; i++) {
                line[i] = (char)buffer_pop(cb);
            }
            line[sz] = '\0';
            return line;
        }

        // No tenim línia completa
        if (*reachedEOF) {
            // EOF i no hi ha res més
            return NULL;
        }

        // Llegim més dades de stdin cap al circular buffer
        int freeb = buffer_free_bytes(cb);
        if (freeb <= 0) {
            // buffer ple però no hem trobat '\n' -> línia massa llarga per CB_SIZE
            // (en l'enunciat normalment no passa; però ho gestionem)
            // Fem "drain" fins trobar '\n' o EOF, acumulant en un buffer dinàmic.
            size_t cap = 8192, len = 0;
            char *big = (char*)malloc(cap);
            if (!big) return NULL;

            while (1) {
                // Si hi ha dades al circular buffer, les traiem
                while (buffer_used_bytes(cb) > 0) {
                    unsigned char c = buffer_pop(cb);
                    if (len + 1 >= cap) {
                        cap *= 2;
                        char *tmp = realloc(big, cap);
                        if (!tmp) { free(big); return NULL; }
                        big = tmp;
                    }
                    big[len++] = (char)c;
                    if (c == '\n') {
                        big[len] = '\0';
                        return big;
                    }
                }

                // Omplim una mica el CB i continuem
                unsigned char buf[CB_SIZE];
                ssize_t r = read(STDIN_FILENO, buf, sizeof(buf));
                if (r < 0) {
                    perror("read");
                    free(big);
                    return NULL;
                }
                if (r == 0) {
                    *reachedEOF = 1;
                    if (len > 0) {
                        big[len] = '\0';
                        return big;
                    } else {
                        free(big);
                        return NULL;
                    }
                }
                for (ssize_t i = 0; i < r; i++) {
                    // si el CB està ple, aboquem directament a big
                    if (buffer_free_bytes(cb) <= 0) {
                        if (len + 1 >= cap) {
                            cap *= 2;
                            char *tmp = realloc(big, cap);
                            if (!tmp) { free(big); return NULL; }
                            big = tmp;
                        }
                        big[len++] = (char)buf[i];
                        if (buf[i] == '\n') {
                            big[len] = '\0';
                            return big;
                        }
                    } else {
                        buffer_push(cb, buf[i]);
                    }
                }
            }
        }

        unsigned char tmp[CB_SIZE];
        ssize_t to_read = freeb;
        if (to_read > (ssize_t)sizeof(tmp)) to_read = (ssize_t)sizeof(tmp);

        ssize_t r = read(STDIN_FILENO, tmp, to_read);
        if (r < 0) {
            perror("read");
            return NULL;
        }
        if (r == 0) {
            *reachedEOF = 1;
            // tornem al loop; buffer_size_next_element retornarà el que quedi
            continue;
        }
        for (ssize_t i = 0; i < r; i++) {
            buffer_push(cb, tmp[i]);
        }
    }
}

static int run_single(char **argv, int wait_fg) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }

    if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp");
        _exit(127);
    }

    if (wait_fg) {
        int status;
        if (waitpid(pid, &status, 0) < 0) perror("waitpid");
        return status;
    }
    return 0;
}

static int run_piped(char **argv1, char **argv2) {
    int fds[2];
    if (pipe(fds) < 0) { perror("pipe"); return -1; }

    pid_t p1 = fork();
    if (p1 < 0) {
        perror("fork");
        close(fds[0]); close(fds[1]);
        return -1;
    }
    if (p1 == 0) {
        // stdout -> pipe write
        if (dup2(fds[1], STDOUT_FILENO) < 0) { perror("dup2"); _exit(127); }
        close(fds[0]);
        close(fds[1]);
        execvp(argv1[0], argv1);
        perror("execvp");
        _exit(127);
    }

    pid_t p2 = fork();
    if (p2 < 0) {
        perror("fork");
        close(fds[0]); close(fds[1]);
        waitpid(p1, NULL, 0);
        return -1;
    }
    if (p2 == 0) {
        // stdin <- pipe read
        if (dup2(fds[0], STDIN_FILENO) < 0) { perror("dup2"); _exit(127); }
        close(fds[1]);
        close(fds[0]);
        execvp(argv2[0], argv2);
        perror("execvp");
        _exit(127);
    }

    // parent
    close(fds[0]);
    close(fds[1]);

    int st1 = 0, st2 = 0;
    if (waitpid(p1, &st1, 0) < 0) perror("waitpid p1");
    if (waitpid(p2, &st2, 0) < 0) perror("waitpid p2");
    return st2;
}

int main(void) {
    CircularBuffer cb;
    if (buffer_init(&cb, CB_SIZE) != 0) {
        fprintf(stderr, "Error: cannot init circular buffer\n");
        return 1;
    }

    int reachedEOF = 0;

    while (1) {
        reap_zombies();

        char *mode = read_line_cb(&cb, &reachedEOF);
        if (!mode) break; // EOF net

        trim_newline(mode);

        if (strcmp(mode, "EXIT") == 0) {
            free(mode);
            break;
        }

        int is_single = (strcmp(mode, "SINGLE") == 0);
        int is_piped  = (strcmp(mode, "PIPED") == 0) || (strcmp(mode, "PIPE") == 0);
        int is_conc   = (strcmp(mode, "CONCURRENT") == 0);

        if (!is_single && !is_piped && !is_conc) {
            // mode desconegut: ignorem
            free(mode);
            continue;
        }

        char *line1 = read_line_cb(&cb, &reachedEOF);
        if (!line1) { free(mode); break; }

        // split_command modifica line1 i retorna argv (malloc) amb punters a dins de line1
        char **argv1 = split_command(line1);
        if (!argv1 || !argv1[0]) {
            free(argv1);
            free(line1);
            free(mode);
            continue;
        }

        if (is_piped) {
            char *line2 = read_line_cb(&cb, &reachedEOF);
            if (!line2) {
                free(argv1); free(line1); free(mode);
                break;
            }

            char **argv2 = split_command(line2);
            if (!argv2 || !argv2[0]) {
                free(argv2); free(line2);
                free(argv1); free(line1);
                free(mode);
                continue;
            }

            run_piped(argv1, argv2);

            free(argv2);
            free(line2);
        } else {
            run_single(argv1, is_conc ? 0 : 1);
        }

        free(argv1);
        free(line1);
        free(mode);
    }

    // Reap final de possibles fills en background
    reap_zombies();

    buffer_deallocate(&cb);
    return 0;
}
