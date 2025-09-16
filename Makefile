CC=gcc
CFLAGS=-Wall -std=c2x -g -fsanitize=address
LDFLAGS=-lm -lpthread
INCLUDE=-Iinclude
TEST=valgrind --error-exitcode=42 --exit-on-first-error=yes --leak-check=full --errors-for-leak-kinds=all --track-fds=yes

.PHONY: clean

# Required for Part 1 - Make sure it outputs a .o file
# to either objs/ or ./
# In your directory
pkgchk.o: src/chk/pkgchk.c
	$(CC) -c $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS)

helpers.o: src/chk/helpers.c
	$(CC) -c $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS)

merkletree.o: src/tree/merkletree.c
	$(CC) -c $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS)

sha256.o: src/crypt/sha256.c
	$(CC) -c $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS)


pkgchecker: src/pkgmain.c src/chk/pkgchk.c src/chk/helpers.c src/tree/merkletree.c src/crypt/sha256.c src/peer.c src/package.c
	$(CC) $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS) -o $@

pkgmain: src/pkgmain.c src/chk/pkgchk.c src/chk/helpers.c src/tree/merkletree.c src/crypt/sha256.c src/peer.c src/package.c
	$(CC) $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS) -o $@

sha256chk: sha256chk.c src/crypt/sha256.c
	$(CC) $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS) -o $@

test_leak: pkgchecker
	$(TEST) ./pkgchecker completeTests/complete_1.bpkg -all_hashes

test_btide: btide testy.txt
	$(TEST) ./btide config.cfg < testy.txt

# Required for Part 2 - Make sure it outputs `btide` file
# in your directory ./
btide: src/btide.c src/peer.c src/config.c src/chk/helpers.c src/chk/pkgchk.c src/package.c src/tree/merkletree.c src/crypt/sha256.c
	$(CC) $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS) -o $@

# Alter your build for p1 tests to build unit-tests for your
# merkle tree, use pkgchk to help with what to test for
# as well as some basic functionality
p1tests:
	bash p1test.sh

# Alter your build for p2 tests to build IO tests
# for your btide client, construct .in/.out files
# and construct a script to help test your client
# You can opt to constructing a program to
# be the tests instead, however please document
# your testing methods
p2tests:
	bash p2test.sh

clean:
	rm -f objs/*
    

