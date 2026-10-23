// Name Lastname: SUNIL KUMAR THAKURI StudentID: 251ADB032
// Repository:https://github.com/sshah3096-gif/Arithmetic-Parser-in-C
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: ChatGPT (OpenAI)
// AI assistance: ChatGPT helped explain the framework, where to start and how to start, helped me work with parser and how to approach it,
//it helped me on using parser and gave some code as well to work with.
// link to my chats:https://chatgpt.com/share/6adbeb3f-c024-83ed-abff-a9124ab6d874

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>


#define USER_NAME "sunilkumar"
#define USER_LASTNAME "thakuri"
#define STUDENT_ID "251ADB032"

typedef struct {
    long long integer;
    double real;
    int is_float;
} Value;

typedef struct {
    const char *text;
    size_t length;
    size_t pos;
    size_t error_pos;
    int error;
} Parser;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} Buffer;

static int process_file(const char *input_path, const char *requested_output_dir);
static const char *base_name(const char *path);
static int make_directory(const char *path);
static int ends_with_txt(const char *name);

int main(int argc, char *argv[])
{
    const char *input_path = NULL;
    const char *directory_path = NULL;
    const char *output_dir = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0) {
            if (i + 1 >= argc || directory_path != NULL || input_path != NULL) {
                fprintf(stderr, "Invalid or missing directory argument\n");
                return 1;
            }
            directory_path = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output-dir") == 0) {
            if (i + 1 >= argc || output_dir != NULL) {
                fprintf(stderr, "Missing or repeated output directory argument\n");
                return 1;
            }
            output_dir = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        } else if (input_path == NULL && directory_path == NULL) {
            input_path = argv[i];
        } else {
            fprintf(stderr, "Too many input arguments\n");
            return 1;
        }
    }

    if ((input_path == NULL) == (directory_path == NULL)) {
        fprintf(stderr, "Usage: calc [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] [input.txt]\n");
        return 1;
    }

    if (input_path) return process_file(input_path, output_dir) ? 0 : 1;

    DIR *dir = opendir(directory_path);
    if (!dir) {
        fprintf(stderr, "Cannot open input directory: %s\n", directory_path);
        return 1;
    }
    char default_output[1024];
    const char *dir_base = base_name(directory_path);
    snprintf(default_output, sizeof(default_output), "%s_%s_%s", dir_base, USER_NAME, STUDENT_ID);
    const char *chosen_output = output_dir ? output_dir : default_output;
    if (!make_directory(chosen_output)) {
        fprintf(stderr, "Cannot create output directory: %s\n", chosen_output);
        closedir(dir);
        return 1;
    }

    int ok = 1;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!ends_with_txt(entry->d_name)) continue;
        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", directory_path, entry->d_name);
        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (!process_file(path, chosen_output)) ok = 0;
    }
    closedir(dir);
    return ok ? 0 : 1;
}

static Value make_int(long long n)
{
    Value v;
    v.integer = n;
    v.real = (double)n;
    v.is_float = 0;
    return v;
}

static Value make_float(double n)
{
    Value v;
    v.integer = 0;
    v.real = n;
    v.is_float = 1;
    return v;
}

static double as_double(Value v)
{
    return v.is_float ? v.real : (double)v.integer;
}

static void set_error(Parser *p, size_t at)
{
    if (!p->error) {
        p->error = 1;
        p->error_pos = at + 1;
    }
}

static void skip_spaces(Parser *p)
{
    while (p->pos < p->length &&
           (p->text[p->pos] == ' ' || p->text[p->pos] == '\t' || p->text[p->pos] == '\r'))
        p->pos++;
}

static int append_text(Buffer *b, const char *s, size_t n)
{
    if (n > (size_t)-1 - b->length - 1) return 0;
    size_t needed = b->length + n + 1;
    if (needed > b->capacity) {
        size_t cap = b->capacity ? b->capacity : 256;
        while (cap < needed) {
            if (cap > (size_t)-1 / 2) { cap = needed; break; }
            cap *= 2;
        }
        char *next = realloc(b->data, cap);
        if (!next) return 0;
        b->data = next;
        b->capacity = cap;
    }
    memcpy(b->data + b->length, s, n);
    b->length += n;
    b->data[b->length] = '\0';
    return 1;
}

static int read_all(FILE *file, char **data, size_t *length)
{
    size_t cap = 4096, len = 0;
    char *buffer = malloc(cap);
    if (!buffer) return 0;
    for (;;) {
        if (len == cap) {
            if (cap > (size_t)-1 / 2) { free(buffer); return 0; }
            cap *= 2;
            char *next = realloc(buffer, cap);
            if (!next) { free(buffer); return 0; }
            buffer = next;
        }
        size_t got = fread(buffer + len, 1, cap - len, file);
        len += got;
        if (got == 0) {
            if (ferror(file)) { free(buffer); return 0; }
            break;
        }
    }
    *data = buffer;
    *length = len;
    return 1;
}

static Value parse_expression(Parser *p);
static Value parse_power(Parser *p);
static Value parse_primary(Parser *p)
{
    skip_spaces(p);
    if (p->pos >= p->length || p->text[p->pos] == '\n') {
        set_error(p, p->pos);
        return make_int(0);
    }

    if (p->text[p->pos] == '(') {
        p->pos++;
        Value result = parse_expression(p);
        skip_spaces(p);
        if (p->error) return result;
        if (p->pos < p->length && p->text[p->pos] == ')') {
            p->pos++;
        } else {
            set_error(p, p->pos); 
        }
        return result;
    }

    if (!isdigit((unsigned char)p->text[p->pos])) {
        set_error(p, p->pos);
        return make_int(0);
    }

    size_t start = p->pos;
    while (p->pos < p->length && isdigit((unsigned char)p->text[p->pos])) p->pos++;
    int is_float = 0;
    if (p->pos < p->length && p->text[p->pos] == '.') {
        size_t dot = p->pos++;
        if (p->pos >= p->length || !isdigit((unsigned char)p->text[p->pos])) {
            set_error(p, dot);
            return make_int(0);
        }
        is_float = 1;
        while (p->pos < p->length && isdigit((unsigned char)p->text[p->pos])) p->pos++;
    }
    if (p->pos < p->length && (p->text[p->pos] == 'e' || p->text[p->pos] == 'E')) {
        set_error(p, p->pos);
        return make_int(0);
    }

    size_t count = p->pos - start;
    char *number = malloc(count + 1);
    if (!number) { set_error(p, start); return make_int(0); }
    memcpy(number, p->text + start, count);
    number[count] = '\0';

    Value result;
    if (is_float) {
        errno = 0;
        double n = strtod(number, NULL);
        result = make_float(n);
        if (errno == ERANGE || !isfinite(n)) set_error(p, start);
    } else {
        errno = 0;
        long long n = strtoll(number, NULL, 10);
        result = make_int(n);
        if (errno == ERANGE) set_error(p, start);
    }
    free(number);
    return result;
}

static Value apply_power(Value base, Value exponent, Parser *p, size_t op, size_t exponent_start)
{
    if (!base.is_float && !exponent.is_float) {
        if (exponent.integer < 0) {
            set_error(p, exponent_start);
            return make_int(0);
        }
        long long answer = 1;
        for (long long i = 0; i < exponent.integer; i++) answer *= base.integer;
        return make_int(answer);
    }

    double answer = pow(as_double(base), as_double(exponent));
    if (!isfinite(answer)) {
        set_error(p, op);
        return make_float(0.0);
    }
    return make_float(answer);
}

static Value parse_power(Parser *p)
{
    Value base = parse_primary(p);
    if (p->error) return base;
    skip_spaces(p);
    if (p->pos + 1 < p->length && p->text[p->pos] == '*' && p->text[p->pos + 1] == '*') {
        size_t op = p->pos;
        p->pos += 2;
        skip_spaces(p);
        size_t exponent_start = p->pos;
        Value exponent = parse_power(p);
        if (p->error) return base;
        return apply_power(base, exponent, p, op, exponent_start);
    }
    return base;
}

static Value parse_term(Parser *p)
{
    Value result = parse_power(p);
    while (!p->error) {
        skip_spaces(p);
        if (p->pos >= p->length || p->text[p->pos] == '\n') break;
        char opchar = p->text[p->pos];
        if (opchar == '*' && p->pos + 1 < p->length && p->text[p->pos + 1] == '*') break;
        if (opchar != '*' && opchar != '/') break;
        size_t op = p->pos++;
        skip_spaces(p);
        size_t right_start = p->pos;
        Value right = parse_power(p);
        if (p->error) break;
        if (opchar == '/') {
            if ((right.is_float && right.real == 0.0) || (!right.is_float && right.integer == 0)) {
                set_error(p, right_start);
                break;
            }
            if (result.is_float || right.is_float) {
                result = make_float(as_double(result) / as_double(right));
                if (!isfinite(result.real)) set_error(p, op);
            } else {
                result = make_int(result.integer / right.integer);
            }
        } else {
            if (result.is_float || right.is_float) {
                result = make_float(as_double(result) * as_double(right));
                if (!isfinite(result.real)) set_error(p, op);
            } else {
                result = make_int(result.integer * right.integer);
            }
        }
    }
    return result;
}

static Value parse_expression(Parser *p)
{
    Value result = parse_term(p);
    while (!p->error) {
        skip_spaces(p);
        if (p->pos >= p->length || p->text[p->pos] == '\n') break;
        char opchar = p->text[p->pos];
        if (opchar != '+' && opchar != '-') break;
        size_t op = p->pos++;
        skip_spaces(p);
        Value right = parse_term(p);
        if (p->error) break;
        if (result.is_float || right.is_float) {
            double n = opchar == '+' ? as_double(result) + as_double(right)
                                     : as_double(result) - as_double(right);
            result = make_float(n);
            if (!isfinite(n)) set_error(p, op);
        } else {
            result = make_int(opchar == '+' ? result.integer + right.integer
                                             : result.integer - right.integer);
        }
    }
    return result;
}

static int make_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode);
    if (mkdir(path, 0777) == 0) return 1;
    if (errno == EEXIST && stat(path, &st) == 0) return S_ISDIR(st.st_mode);
    return 0;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void input_base_without_txt(const char *path, char *out, size_t out_size)
{
    snprintf(out, out_size, "%s", base_name(path));
    size_t len = strlen(out);
    if (len >= 4 && strcmp(out + len - 4, ".txt") == 0) out[len - 4] = '\0';
}

static int process_file(const char *input_path, const char *requested_output_dir)
{
    FILE *in = fopen(input_path, "rb");
    if (!in) {
        fprintf(stderr, "Cannot open input file: %s\n", input_path);
        return 0;
    }

    char *contents = NULL;
    size_t length = 0;
    if (!read_all(in, &contents, &length)) {
        fprintf(stderr, "Cannot read input file: %s\n", input_path);
        fclose(in);
        return 0;
    }
    fclose(in);

    char base[512], out_dir[1024], output_path[2048];
    input_base_without_txt(input_path, base, sizeof(base));
    if (requested_output_dir) {
        snprintf(out_dir, sizeof(out_dir), "%s", requested_output_dir);
    } else {
        snprintf(out_dir, sizeof(out_dir), "%s_%s_%s", base, USER_NAME, STUDENT_ID);
    }
    if (!make_directory(out_dir)) {
        fprintf(stderr, "Cannot create output directory: %s\n", out_dir);
        free(contents);
        return 0;
    }
    snprintf(output_path, sizeof(output_path), "%s/%s_%s_%s_%s.txt",
             out_dir, base, USER_NAME, USER_LASTNAME, STUDENT_ID);

    Buffer results = {NULL, 0, 0};
    Parser p;
    p.text = contents;
    p.length = length;
    p.pos = 0;
    p.error_pos = 0;
    p.error = 0;

    while (p.pos < p.length && !p.error) {
        size_t line_start = p.pos;
        size_t q = p.pos;
        while (q < p.length && (p.text[q] == ' ' || p.text[q] == '\t' || p.text[q] == '\r')) q++;

        if (q < p.length && p.text[q] == '#') {
            while (p.pos < p.length && p.text[p.pos] != '\n') p.pos++;
            if (p.pos < p.length) p.pos++;
            continue;
        }
        if (q >= p.length || p.text[q] == '\n') {
            p.pos = q;
            if (p.pos < p.length && p.text[p.pos] == '\n') p.pos++;
            continue;
        }

        p.pos = line_start;
        Value answer = parse_expression(&p);
        if (!p.error) {
            skip_spaces(&p);
            if (p.pos < p.length && p.text[p.pos] != '\n') set_error(&p, p.pos);
        }
        if (p.error) break;

        char line_out[128];
        int n;
        if (answer.is_float) n = snprintf(line_out, sizeof(line_out), "%.15g\n", answer.real);
        else n = snprintf(line_out, sizeof(line_out), "%lld\n", answer.integer);
        if (n < 0 || !append_text(&results, line_out, (size_t)n)) {
            fprintf(stderr, "Out of memory while saving results\n");
            free(contents);
            free(results.data);
            return 0;
        }
        if (p.pos < p.length && p.text[p.pos] == '\n') p.pos++;
    }

    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "Cannot write output file: %s\n", output_path);
        free(contents);
        free(results.data);
        return 0;
    }
    if (p.error) fprintf(out, "ERROR:%zu\n", p.error_pos);
    else if (results.length > 0) fwrite(results.data, 1, results.length, out);
    fclose(out);

    free(contents);
    free(results.data);
    return 1;
}

static int ends_with_txt(const char *name)
{
    size_t len = strlen(name);
    return len >= 4 && strcmp(name + len - 4, ".txt") == 0;
}

