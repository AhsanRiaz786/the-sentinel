#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_OUTPUT 32768
#define MAX_COMPILE_LOG 8192
#define MAX_PATH_LEN 512
#define QUEUE_CAPACITY 64
#define DEFAULT_WORKERS 3
#define TIMEOUT_SECONDS 2
#define MEMORY_LIMIT_MB 256

typedef struct {
    int id;
    char path[MAX_PATH_LEN];
} Job;

typedef struct {
    Job items[QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} JobQueue;

typedef struct {
    char status[32];
    char output[MAX_OUTPUT];
    char compile_log[MAX_COMPILE_LOG];
    long time_ms;
    long max_rss_kb;
    int exit_code;
    int term_signal;
    bool timed_out;
    bool banned;
} ExecResult;

static JobQueue queue;
static int worker_count = DEFAULT_WORKERS;
static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
static void *worker_loop(void *arg);
static int enqueue_job(const Job *job);
static int dequeue_job(Job *job);
static bool compile_user_code(const char *src_path, char *bin_path, size_t bin_path_sz, ExecResult *res);
static void execute_binary(const char *bin_path, ExecResult *res);
static void to_json(const Job *job, const ExecResult *res, char *buf, size_t buf_sz);
static void json_escape(const char *src, char *dst, size_t dst_sz);
static bool contains_banned_tokens(const char *path, ExecResult *res);

// ---------------- Queue ----------------
static void queue_init(JobQueue *q) {
    memset(q, 0, sizeof(JobQueue));
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

static int enqueue_job(const Job *job) {
    JobQueue *q = &queue;
    pthread_mutex_lock(&q->mutex);
    while (q->count == QUEUE_CAPACITY) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    q->items[q->tail] = *job;
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

static int dequeue_job(Job *job) {
    JobQueue *q = &queue;
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }
    *job = q->items[q->head];
    q->head = (q->head + 1) % QUEUE_CAPACITY;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

// ---------------- Utility ----------------
static long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000L) + (tv.tv_usec / 1000);
}

static void json_escape(const char *src, char *dst, size_t dst_sz) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 2 < dst_sz; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '\\' || c == '\"') {
            if (j + 2 >= dst_sz) break;
            dst[j++] = '\\';
            dst[j++] = c;
        } else if (c == '\n') {
            if (j + 2 >= dst_sz) break;
            dst[j++] = '\\';
            dst[j++] = 'n';
        } else if (c == '\r') {
            if (j + 2 >= dst_sz) break;
            dst[j++] = '\\';
            dst[j++] = 'r';
        } else if (c == '\t') {
            if (j + 2 >= dst_sz) break;
            dst[j++] = '\\';
            dst[j++] = 't';
        } else {
            dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
}

static bool contains_banned_tokens(const char *path, ExecResult *res) {
    const char *banned[] = {
        "system(",
        "fork(",
        "exec",
        "popen(",
        "remove(",
        "rename(",
        "kill(",
        "chmod(",
        "chown(",
        "ptrace",
        NULL
    };
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(res->compile_log, MAX_COMPILE_LOG, "Could not open source file.");
        return true;
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        for (const char **p = banned; *p != NULL; p++) {
            if (strstr(line, *p)) {
                snprintf(res->compile_log, MAX_COMPILE_LOG, "Banned token detected: %s", *p);
                fclose(f);
                res->banned = true;
                return true;
            }
        }
    }
    fclose(f);
    return false;
}

// ---------------- Compile & Run ----------------
static bool compile_user_code(const char *src_path, char *bin_path, size_t bin_path_sz, ExecResult *res) {
    res->compile_log[0] = '\0';
    res->banned = false;

    if (contains_banned_tokens(src_path, res)) {
        strncpy(res->status, "Banned", sizeof(res->status));
        return false;
    }

    char template[] = "/tmp/sentinel-bin-XXXXXX";
    int fd = mkstemp(template);
    if (fd == -1) {
        snprintf(res->compile_log, MAX_COMPILE_LOG, "mkstemp failed: %s", strerror(errno));
        strncpy(res->status, "CompileError", sizeof(res->status));
        return false;
    }
    close(fd);
    // Remove the temporary file so gcc can write to that path
    unlink(template);
    strncpy(bin_path, template, bin_path_sz - 1);
    bin_path[bin_path_sz - 1] = '\0';

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "gcc -std=c11 -O2 -pipe \"%s\" -o \"%s\" -lm 2> /tmp/sentinel-compile.log",
             src_path, bin_path);
    int rc = system(cmd);
    if (rc != 0) {
        FILE *f = fopen("/tmp/sentinel-compile.log", "r");
        if (f) {
            fread(res->compile_log, 1, MAX_COMPILE_LOG - 1, f);
            res->compile_log[MAX_COMPILE_LOG - 1] = '\0';
            fclose(f);
        } else {
            snprintf(res->compile_log, MAX_COMPILE_LOG, "Compilation failed (no log).");
        }
        strncpy(res->status, "CompileError", sizeof(res->status));
        return false;
    }
    strncpy(res->status, "Compiled", sizeof(res->status));
    return true;
}

static void apply_limits(void) {
    struct rlimit cpu_limit = {TIMEOUT_SECONDS, TIMEOUT_SECONDS + 1};
    setrlimit(RLIMIT_CPU, &cpu_limit);

    struct rlimit as_limit = {MEMORY_LIMIT_MB * 1024 * 1024UL, MEMORY_LIMIT_MB * 1024 * 1024UL};
    setrlimit(RLIMIT_AS, &as_limit);

    struct rlimit fsize_limit = {10 * 1024 * 1024, 10 * 1024 * 1024};
    setrlimit(RLIMIT_FSIZE, &fsize_limit);
}

static void execute_binary(const char *bin_path, ExecResult *res) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        snprintf(res->status, sizeof(res->status), "RuntimeError");
        snprintf(res->output, MAX_OUTPUT, "pipe failed: %s", strerror(errno));
        return;
    }

    long start_ms = now_ms();
    pid_t pid = fork();
    if (pid == -1) {
        snprintf(res->status, sizeof(res->status), "RuntimeError");
        snprintf(res->output, MAX_OUTPUT, "fork failed: %s", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid == 0) {
        // Child
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        apply_limits();
        execl(bin_path, bin_path, (char *)NULL);
        perror("execl");
        _exit(127);
    }

    // Parent
    close(pipefd[1]);
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);

    char buf[1024];
    size_t out_len = 0;
    bool done = false;
    int status = 0;
    struct rusage usage;
    memset(&usage, 0, sizeof(usage));

    while (!done) {
        int w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            done = true;
        }

        ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            if (out_len + (size_t)n < MAX_OUTPUT - 1) {
                memcpy(res->output + out_len, buf, (size_t)n);
                out_len += (size_t)n;
                res->output[out_len] = '\0';
            }
        }

        long elapsed = now_ms() - start_ms;
        if (!done && elapsed > TIMEOUT_SECONDS * 1000) {
            kill(pid, SIGKILL);
            res->timed_out = true;
            done = true;
            waitpid(pid, &status, 0);
        }

        if (!done) {
            // Sleep briefly to avoid busy wait
            struct timespec ts = {.tv_sec = 0, .tv_nsec = 50 * 1000 * 1000};
            nanosleep(&ts, NULL);
        }
    }

    // Drain remaining output
    while (true) {
        ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
        if (n <= 0) break;
        if (out_len + (size_t)n < MAX_OUTPUT - 1) {
            memcpy(res->output + out_len, buf, (size_t)n);
            out_len += (size_t)n;
            res->output[out_len] = '\0';
        }
    }
    close(pipefd[0]);

    // Obtain resource usage
    if (wait4(pid, NULL, WNOHANG, &usage) == -1) {
        // ignore
    }

    long end_ms = now_ms();
    res->time_ms = end_ms - start_ms;
    res->max_rss_kb = usage.ru_maxrss;
    res->term_signal = 0;
    res->exit_code = 0;

    if (res->timed_out) {
        strncpy(res->status, "TimeLimitExceeded", sizeof(res->status));
        return;
    }

    if (WIFSIGNALED(status)) {
        res->term_signal = WTERMSIG(status);
        strncpy(res->status, "RuntimeError", sizeof(res->status));
    } else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        strncpy(res->status, "Success", sizeof(res->status));
    } else {
        res->exit_code = WEXITSTATUS(status);
        strncpy(res->status, "RuntimeError", sizeof(res->status));
    }
}

// ---------------- JSON output ----------------
static void to_json(const Job *job, const ExecResult *res, char *buf, size_t buf_sz) {
    char escaped_out[MAX_OUTPUT * 2];
    char escaped_log[MAX_COMPILE_LOG * 2];
    json_escape(res->output, escaped_out, sizeof(escaped_out));
    json_escape(res->compile_log, escaped_log, sizeof(escaped_log));

    snprintf(
        buf,
        buf_sz,
        "{"
        "\"job_id\":%d,"
        "\"status\":\"%s\","
        "\"output\":\"%s\","
        "\"compile_log\":\"%s\","
        "\"time_ms\":%ld,"
        "\"max_rss_kb\":%ld,"
        "\"exit_code\":%d,"
        "\"signal\":%d,"
        "\"timed_out\":%s,"
        "\"banned\":%s"
        "}",
        job->id,
        res->status,
        escaped_out,
        escaped_log,
        res->time_ms,
        res->max_rss_kb,
        res->exit_code,
        res->term_signal,
        res->timed_out ? "true" : "false",
        res->banned ? "true" : "false");
}

// ---------------- Worker ----------------
static void *worker_loop(void *arg) {
    (void)arg;
    while (true) {
        Job job;
        dequeue_job(&job);
        if (job.id == -1) break; // poison pill

        ExecResult res;
        memset(&res, 0, sizeof(res));

        char bin_path[MAX_PATH_LEN];
        if (compile_user_code(job.path, bin_path, sizeof(bin_path), &res)) {
            execute_binary(bin_path, &res);
            unlink(bin_path);
        }

        char json[4096];
        to_json(&job, &res, json, sizeof(json));

        pthread_mutex_lock(&print_mutex);
        printf("%s\n", json);
        fflush(stdout);
        pthread_mutex_unlock(&print_mutex);
    }
    return NULL;
}

// ---------------- Main ----------------
static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <file1.c> [file2.c ...]\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    queue_init(&queue);

    pthread_t threads[DEFAULT_WORKERS];
    for (int i = 0; i < worker_count; i++) {
        pthread_create(&threads[i], NULL, worker_loop, NULL);
    }

    int job_id = 1;
    for (int i = 1; i < argc; i++) {
        Job job;
        job.id = job_id++;
        strncpy(job.path, argv[i], sizeof(job.path) - 1);
        job.path[sizeof(job.path) - 1] = '\0';
        enqueue_job(&job);
    }

    // Send poison pills
    for (int i = 0; i < worker_count; i++) {
        Job poison = {.id = -1};
        enqueue_job(&poison);
    }

    for (int i = 0; i < worker_count; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}


