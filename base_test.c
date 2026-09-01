#include "base.h"
#include "testing.h"

typedef struct {
	u8 data[2048];
	usize capacity;
	usize len;
	usize write_limit;
} Test_Memory_Writer;

static isize test_memory_write(void* impl, IO_Mode mode, u8* buf, usize buflen){
	Test_Memory_Writer* memory = impl;
	if(mode != IO_Write){
		return IO_Err_Unsupported;
	}

	usize available = memory->capacity - memory->len;
	if(available == 0){
		return IO_Err_TooBig;
	}

	usize n = min(buflen, available);
	if(memory->write_limit != 0){
		n = min(n, memory->write_limit);
	}
	mem_copy(&memory->data[memory->len], buf, n);
	memory->len += n;
	return (isize)n;
}

static IO_Writer test_memory_writer(Test_Memory_Writer* memory){
	return (IO_Writer){
		.stream = {
			.impl = memory,
			.fn = test_memory_write,
		},
	};
}

static isize test_fmt_writev(IO_Writer writer, char const* fmt, ...){
	va_list args;
	va_start(args, fmt);
	isize result = fmt_writev(writer, fmt, args);
	va_end(args);
	return result;
}

void base_tests(Test* t){
	t->name = "base";

	{
		Test_Memory_Writer memory = {
			.capacity = sizeof(memory.data),
			.write_limit = 17,
		};
		isize written = fmt_write(
			test_memory_writer(&memory),
			"prefix:%0600d:suffix",
			7
		);

		bool padding_is_zero = true;
		for(usize i = 7; i < 606; i += 1){
			padding_is_zero = padding_is_zero && memory.data[i] == '0';
		}

		t_pred(t, written == 614);
		t_pred(t, memory.len == 614);
		t_pred(t, str_equal((String){(char*)memory.data, 7}, strlit("prefix:")));
		t_pred(t, padding_is_zero);
		t_pred(t, memory.data[606] == '7');
		t_pred(t, str_equal((String){(char*)&memory.data[607], 7}, strlit(":suffix")));
	}

	{
		Test_Memory_Writer memory = {.capacity = sizeof(memory.data)};
		isize written = test_fmt_writev(
			test_memory_writer(&memory),
			"%s %d",
			"value",
			42
		);
		t_pred(t, written == 8);
		t_pred(t, str_equal((String){(char*)memory.data, memory.len}, strlit("value 42")));
	}

	{
		Test_Memory_Writer memory = {.capacity = 8};
		isize result = fmt_write(test_memory_writer(&memory), "0123456789");
		t_pred(t, result == IO_Err_TooBig);
		t_pred(t, memory.len == 8);
	}

	t_pred(t, fmt_write((IO_Writer){0}, "closed") == IO_Err_Closed);
}
