CC = gcc

file_system: super_blocks.o inode.o cursor.o input_check.o io.o file_system.o sort.o fs_operations.o
	$(CC) super_blocks/super_blocks.o inode/inode.o cursor/cursor.o input_check/input_check.o io/io.o file_system.o sort.o fs_operations/fs_operations.o -o $@

super_blocks.o: super_blocks/super_blocks.c super_blocks/super_blocks.h io/io.h inode/inode.h
	$(CC) -c super_blocks/super_blocks.c -o super_blocks/$@

inode.o: inode/inode.c inode/inode.h fs_operations/fs_operations.h io/io.h
	$(CC) -c inode/inode.c -o inode/$@

cursor.o: cursor/cursor.c cursor/cursor.h io/io.h fs_operations/fs_operations.h
	$(CC) -c cursor/cursor.c -o cursor/$@

input_check.o: input_check/input_check.c input_check/input_check.h
	$(CC) -c input_check/input_check.c -o input_check/$@

io.o: io/io.c io/io.h inode/inode.h fs_operations/fs_operations.h
	$(CC) -c io/io.c -o io/$@

file_system.o: file_system.c super_blocks/super_blocks.h io/io.h inode/inode.h cursor/cursor.h fs_operations/fs_operations.h input_check/input_check.h
	$(CC) -c file_system.c -o $@

sort.o: sort.c sort.h
	$(CC) -c sort.c -o $@

fs_operations.o: fs_operations/fs_operations.c fs_operations/fs_operations.h super_blocks/super_blocks.h io/io.h inode/inode.h sort.h
	$(CC) -c fs_operations/fs_operations.c -lm -o fs_operations/$@

author:
	@echo "Thanasis Lolos 1115201700072 sdi1700072"

clean:
	rm -rf *.o io/*.o inode/*.o cursor/*.o fs_operations/*.o input_check/*.o super_blocks/*.o cursor/*.o
