UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	binary_folder = linux_x64
	fuse_link_name = fuse3
endif
ifeq ($(UNAME_S),Darwin)
		binary_folder = macOS
		fuse_link_name = osxfuse
endif

secfs:
	gcc -O3 -Wall -Werror -Wextra -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code \
		-o secfs \
		src/main.c src/security/encryption.c src/utilities/utilities.c src/filesystem/filesystem.c src/db/indexdb.c src/db/blockdb.c src/filesystem/secfs.c src/security/passwordinput.c \
		-Lvendor/openssl/$(binary_folder) \
		-Lvendor/fuse/$(binary_folder) \
		-Lvendor/uuid/$(binary_folder) \
		-Ivendor/openssl/include \
		-Ivendor/fuse/include \
		-Ivendor/uuid/include \
	 	-lssl -lcrypto -l$(fuse_link_name) -luuid -lpthread

clean:
	rm secfs
