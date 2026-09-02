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

arena_declare_static(arena, 512 * 1024);

int main(){
	String source = read_file_whole("source.txt");
	printf("%.*s\n", strf(source));

	AST ast = {0};
	Error e = parse(source, &ast, &arena);
	if(e.type){
		e.file = strlit("source.txt");
		printf("%.*s:%d error[E%04d]: %.*s\n", strf(e.file), e.offset, e.type, strf(error_type_name(e.type)));
		printf("%.*s\n", strf(token_type_name(e.expected.token_type)));
		return 1;
	}

	node_format((IO_Writer){io_stdout()}, ast.root);
}

#include "base.c"
#include "scanner.c"
#include "parser.c"
#include "errors.c"
