CS 61 Problem Set 4
===================

**Fill out both this file and `AUTHORS.md` before submitting.** We grade
anonymously, so put all personally identifying information, including
collaborators, in `AUTHORS.md`.

Grading notes (if any)
----------------------
Even before implementing memory-mapped IO, my performance on many of the
tests was significantly below that of other students doing the same thing
(regular single-slot 4096B cache). After I implemented MMIO, some tests
improved slightly, but it seems like something else is my bottleneck.
I showed my code to both Dhilan and Austin, and they agreed that something
seemed wrong based on the test performance, but they couldn't figure out
what it was. Hoping my EC attempt (MMIO) can partly make up for these
performance issues.


Extra credit attempted (if any)
-------------------------------
- Memory-mapped IO