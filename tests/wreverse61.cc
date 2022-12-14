#include "io61.hh"

// Usage: ./wreverse61 [-s SIZE] [-o OUTFILE] [FILE]
//    Copies the input FILE to OUTFILE one character at a time,
//    reversing the order of characters while writing the output.

int main(int argc, char* argv[]) {
    // Parse arguments
    io61_args args = io61_args("s:o:i:Fy").parse(argc, argv);

    // Open files, measure file sizes
    io61_file* inf = io61_open_check(args.input_file, O_RDONLY);
    io61_file* outf = io61_open_check(args.output_file,
                                      O_WRONLY | O_CREAT | O_TRUNC);

    if ((ssize_t) args.file_size < 0) {
        args.file_size = io61_filesize(inf);
    }
    if ((ssize_t) args.file_size < 0) {
        fprintf(stderr, "reverse61: can't get size of input file\n");
        exit(1);
    }
    if (io61_seek(outf, 0) < 0) {
        fprintf(stderr, "reverse61: output file is not seekable\n");
        exit(1);
    }

    while (args.file_size != 0) {
        --args.file_size;
        int ch = io61_readc(inf);
        assert(ch >= 0);

        int r = io61_seek(outf, args.file_size);
        assert(r == 0);

        r = io61_writec(outf, ch);
        assert(r == 0);

        args.after_write(outf);
    }

    io61_close(inf);
    io61_close(outf);
}
