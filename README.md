io61
===================

io61 is the implementation of a subset of stdio functions I wrote
for CS61. io61 implements `fseek`, `fread`, `fwrite`, `freadc`,
and `fwritec` using memory-mapped IO or a single-slot cache, 
depending on the file type. It is a correct implementation and
passes a number of difficult tests found in `/tests`. It is also
a fast implementation, outperforming `stdio` by an average factor
of 9.38x across 45 tests of various access patterns and payload
sizes.

![Course test results](https://i.imgur.com/rj0fPBc.png)

 I wrote all
the code in `io61.cc` and `io61.hh` except the small setup functions
at the end of `io61.cc`.

