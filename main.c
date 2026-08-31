#include "base.h"
#include <stdio.h>
#include "lang.h"

extern void* calloc(size_t, size_t);

String read_file_whole(char const* path){
	FILE* f = fopen(path, "rb");
	fseek(f, 0, SEEK_END);
	i64 end = ftell(f);
	rewind(f);
	i64 start = ftell(f);
	i64 size = end - start;
	u8* data = calloc(size + 1, 1);
	fread(data, 1, size, f);

	return (String){ .v = (char const*)data, .len = size };
}

int main(){
	String source = read_file_whole("main.c");
	printf("%.*s\n", strf(source));
}

#include "base.c"
#include "scanner.c"