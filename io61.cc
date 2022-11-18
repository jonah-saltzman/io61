#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <climits>
#include <cerrno>
#include <sys/mman.h>

static constexpr size_t BUFSIZE = 0x8000;

// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

enum io61_seek_mode {
    UNKNOWN,
    STRIDED,
    RANDOM
};

struct io61_file {
    int fd = -1;     // file descriptor
    unsigned char buf[BUFSIZE];
    static constexpr size_t bufsize = sizeof(buf);
    size_t tag = 0;
    size_t end_tag = 0;
    size_t pos_tag = 0;
    int mode;
    bool is_char_file = 0;
    unsigned char* mmap = (unsigned char*) MAP_FAILED;
    unsigned char* data;
    off_t size = 0;

    io61_file(int fdn, int m, void* map, off_t sz) {
        fd = fdn;
        mode = m;
        if (m) {
            end_tag = io61_file::bufsize;
        }
        mmap = (unsigned char*) map;
        if (map != MAP_FAILED) {
            end_tag = sz;
            data = this->mmap; //
        } else {               // rust wouldn't let me do this huh
            data = this->buf;  // 
        }
        size = sz;
    }
};

inline __attribute__((always_inline)) size_t buf_pos(io61_file* f) {
    return f->pos_tag - f->tag;
}

inline __attribute__((always_inline)) bool at_buff_end(io61_file* f) {
    return !(f->pos_tag ^ f->end_tag);
}

inline __attribute__((always_inline)) size_t max(size_t a, size_t b) {
    return a > b ? a : b;
}

inline __attribute__((always_inline)) size_t min(size_t a, size_t b) {
    return a < b ? a : b;
}

inline __attribute__((always_inline)) size_t round_down(off_t n, off_t mod) {
    return n - (n % mod);
}

// io61_fdopen(fd, mode)
//    Returns a new io61_file for file descriptor `fd`. `mode` is either
//    O_RDONLY for a read-only file or O_WRONLY for a write-only file.
//    You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    void* map = MAP_FAILED;
    struct stat stats;
    int r = fstat(fd, &stats);
    assert(r == 0);
    if (!mode && stats.st_size) {
        map = mmap(NULL, stats.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE | MAP_NORESERVE, fd, 0);
    }
    io61_file* f = new io61_file(fd, mode, map, stats.st_size);
    return f;
}


// io61_close(f)
//    Closes the io61_file `f` and releases all its resources.

int io61_close(io61_file* f) {
    if (f->mmap == MAP_FAILED) {
        io61_flush(f);
    }
    int r = close(f->fd);
    delete f;
    return r;
}


// io61_readc(f)
//    Reads a single (unsigned) byte from `f` and returns it. Returns EOF,
//    which equals -1, on end of file or error.

int io61_readc(io61_file* f) {    
    if (!at_buff_end(f)) {
        unsigned char c = f->data[buf_pos(f)];
        ++f->pos_tag;
        return c;
    } else {
        unsigned char c;
        int r = io61_read(f, &c, 1);
        if (r == 1) {
            return c;
        } else if (r == 0) {
            errno = 0;
            return -1;
        } else {
            assert(r == -1 && errno > 0);
            return -1;
        }
    }
}


// io61_read(f, buf, sz)
//    Reads up to `sz` bytes from `f` into `buf`. Returns the number of
//    bytes read on success. Returns 0 if end-of-file is encountered before
//    any bytes are read, and -1 if an error is encountered before any
//    bytes are read.
//
//    Note that the return value might be positive, but less than `sz`,
//    if end-of-file or error is encountered before all `sz` bytes are read.
//    This is called a “short read.”

ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) {
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);

    if (f->is_char_file) {
        memset(buf, 0, sz);
        return sz;
    }

    size_t pos = 0;
    while (pos != sz) {
        if (at_buff_end(f)) {
            ssize_t r = io61_fill(f);
            if (r == -1) {
                if (pos == 0)
                    return -1;
                else {
                    break;
                }
            } else if (r == 0) {
                break;
            }
        }

        size_t copy_sz = f->mmap == MAP_FAILED
            ? min(f->bufsize - (f->pos_tag - f->tag), sz - pos)
            : min(f->size - f->pos_tag, sz);
        memcpy(&buf[pos], &f->data[buf_pos(f)], copy_sz);
        pos += copy_sz;
        f->pos_tag += copy_sz;
    }
    return pos;
}

ssize_t io61_fill(io61_file* f, off_t start, off_t pos) {

    if (f->mmap != MAP_FAILED)
        return 0;

    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);

    if (f->is_char_file)
        return f->bufsize;

    if (start != -1) {
        f->tag = f->pos_tag = f->end_tag = start;
    } else {
        f->pos_tag = f->end_tag;
        f->tag = f->pos_tag;
    }
    ssize_t nread = 0;
    while (nread != f->bufsize) {
        ssize_t r = read(f->fd, f->buf, f->bufsize - nread);
        if (r < 0) {
            if (errno != EINTR) {
                return -1;
            } else
                continue;
        } else if (r == 0) {
            break;
        } else {
            nread += r;
            f->end_tag += r;
        }
    }

    if (pos != -1) {
        f->pos_tag = min(f->end_tag, pos);
    }

    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);

    return nread;
}


// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success and
//    -1 on error.

int io61_writec(io61_file* f, int ch) {
    if (f->is_char_file)
        return 0;
    if (buf_pos(f) == f->bufsize) {
        int r = io61_write(f, (const unsigned char*)&ch, 1);
        if (r == 1) {
            return 0;
        } else {
            return -1;
        }
    } else {
        f->data[buf_pos(f)] = (char) ch;
        ++f->pos_tag;
        f->end_tag = max(f->pos_tag, f->end_tag);
        return 0;
    }
}


// io61_write(f, buf, sz)
//    Writes `sz` characters from `buf` to `f`. Returns `sz` on success.
//    Can write fewer than `sz` characters when there is an error, such as
//    a drive running out of space. In this case io61_write returns the
//    number of characters written, or -1 if no characters were written
//    before the error occurred.

ssize_t io61_write(io61_file* f, const unsigned char* buf, size_t sz) {
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);

    if (f->is_char_file) {
        return sz;
    }

    size_t pos = 0;
    while (pos != sz) {

        if (at_buff_end(f)) {
            int r = io61_flush(f);
            if (r < 0) {
                if (pos == 0) {
                    return -1;
                } else {
                    break;
                }
            }
        }

        size_t write_sz = min(f->bufsize - (f->pos_tag - f->tag), sz - pos);
        memcpy(&f->data[buf_pos(f)], &buf[pos], write_sz);
        pos += write_sz;
        f->pos_tag += write_sz;
        f->end_tag = max(f->pos_tag, f->end_tag);
    }

    return pos;
}


// io61_flush(f)
//    Forces a write of any cached data written to `f`. Returns 0 on
//    success. Returns -1 if an error is encountered before all cached
//    data was written.
//
//    If `f` was opened read-only, `io61_flush(f)` returns 0. If may also
//    drop any data cached for reading.

int io61_flush(io61_file* f) {
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);
    assert(f->mmap == MAP_FAILED);
    
    ssize_t nwritten = 0;
    ssize_t to_write = buf_pos(f);

    if (f->is_char_file) {
        return to_write;
    }

    if (!f->mode) {
        f->tag = f->pos_tag = f->end_tag;
        return 0;
    }

    while (to_write) {
        ssize_t r = write(f->fd, &f->buf[nwritten], to_write);
        if (r < 0) {
            if (errno != EINTR)
                return -1;
            else
                continue;
        } else {
            nwritten += r;
            to_write -= r;
        }
    }

    f->pos_tag = f->end_tag;
    f->tag = f->pos_tag;
    f->end_tag += f->bufsize;
    return 0;
}


// io61_seek(f, pos)
//    Changes the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t pos) {
    assert(pos >= 0);
    unsigned long long u_pos = (unsigned long long) pos;
    if (f->mmap != MAP_FAILED) {
        f->pos_tag = pos;
        return 0;
    }

    if (f->mode) {
        if (u_pos >= f->tag && u_pos <= f->pos_tag) {
            f->pos_tag = u_pos;
            return 0;
        }
        off_t r = io61_flush(f);
        if (r == 0) {
            r = lseek(f->fd, pos, SEEK_SET);
            if (r == -1)
                return -1;
            if (r == 0 && pos != 0)
                f->is_char_file = 1;
            f->tag = f->pos_tag = pos;
            f->end_tag = pos + f->bufsize;
            return 0;
        } else {
            return -1;
        }
    } else {
        if (u_pos >= f->tag && u_pos <= f->end_tag) {
            f->pos_tag = u_pos;
            return 0;
        }
        off_t aligned_seek = round_down(pos, f->bufsize);
        off_t r = lseek(f->fd, aligned_seek, SEEK_SET);
        if (r == -1)
            return -1;
        if (r == 0 && aligned_seek != 0) {
            f->is_char_file = 1;
        }
        r = io61_fill(f, r, pos);
        if (r == -1)
            return -1;
        else
            return 0;
    }
}


// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Opens the file corresponding to `filename` and returns its io61_file.
//    If `!filename`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != nullptr` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename) {
        fd = open(filename, mode, 0666);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        fd = STDIN_FILENO;
    } else {
        fd = STDOUT_FILENO;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_fileno(f)
//    Returns the file descriptor associated with `f`.

int io61_fileno(io61_file* f) {
    return f->fd;
}


// io61_filesize(f)
//    Returns the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode)) {
        return s.st_size;
    } else {
        return -1;
    }
}