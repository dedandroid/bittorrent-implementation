# Software organisation

## pkgmain
Part 1 of the program is implemented entirely inside of the pkgmain.c file, and the compiler binary pkgmake/pkgchecker. 

This part uses the source files primary source file pkgmain.c, along with all the other files: pkgchk.c, helpers.c, merkletree.c, sha256.c, peer.c and package.c.

Pkgchk.c and merkletree.c include the large majority of crucial functions that are to be executed by the binary. Pkgchk.c includes functions that load the bpkg, perform a variety of functions related to displaying hashes and finding correctly computed hashes. 

The implementation the Merkle tree that the program uses is done through a complete binary tree constructed inside of merkletree.c. This file performs basic searches on the tree through in-order traversal, construction of the tree using nodes with values that are pointers to the hashes of the chunks loaded into the bpkg file, and connections between all the nodes in a tree-like structure with essential binary tree properties. This file is also responsible for reading the contents of the specified .data file and computing its hash using the functions available in the sha256.c source file. In conjunction with Pkgchk.c, these files form the basis of part 1.

Lastly, additional "helper" functions, such as the stripping of whitespaces, and detecting whether a string is a number to allow the atoi() function to perform correctly (without erroneous output such as conversion of only the initial part of a string), are all included inside of the helper.c file. 

Since in part 2, threads and their functions were being used, the helper.c file was used to call some of these functions. To ensure compatibility, peer.c and package.c were added so that helpers.c operates knowing where the functions it calles are. Apart from this, these files are not playing any role in part 1.

## btide
Part 2 of this program is implemented inside of btide.c, with the compiled btide binary being used for execution.

This part uses all the source files from the previous parts, with particular emphasis on the use of peer.c, config.c and package.c.

Peer.c contains the basic networking data that will be used by the separate threads to create a listener and handle multiple clients at a time. It's responsible for containing functions that send important packets (RES, REQ), along with how these packets are handled.

Config.c is a small file that only contains the function responsible for loading in the config file, and also creating the directory specified, where all the .bpkg referenced files will be managed.

Package.c is largely used for managing the packages in the program and is most involved in accessing the managed_packages array. This program is essential to the fetching phase of the program as it makes sure the files being requested are available locally.

Btide.c uses threading to split an incoming connection onto a separate thread. The number of threads are the same as the max_users. There are 4 arrays used throughout the program. This includes the threads array that contains the threads, the peers array that contains the connected peers, the thread status array that indicates which thread, belonging to a certain thread ID, can be used, and the manages_package array that indicates how many packets are loaded in memory. As different threads may be reading and writing into the shared resources at any given time, 4 separate locks were also allocated in memory that the threads can use to avoid situations creating a race condition.

Additionally, helpers.c will be containing functions responsible for accessing the peer_arrays by peer.c and btide.c, along with creating the necessary threads.

Upon EOF or when an error such as listener failure occurs, the program calls a dedicated function that deallocates every allocated array and lock used throughout the program.


## tests
Test data is located inside the tests folder. The tests for each part have been divided up into their respective test directories. These test files CONTAIN BINARY FILES that are essential for testing the output.

The tests for part 1 can all be executed by executing the shell file "p1tests.sh". Part 1's individual tests can be executed by going into the corresponding tests directory and executing the corresponding shell file. The p1tests.sh file compiles and loads the program pkgchecker binary into the tests directory, from where all the other test files use a relative path to execute pkgchecker. 
