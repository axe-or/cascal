#include "base.h"
#include "lang.h"

static String error_type_names[] = {
	[Err_None] = strlit("None"),
	[Err_UnexpectedChar] = strlit("unexpected character"),
	[Err_UnknownChar] = strlit("unknown character"),
	[Err_InvalidBase] = strlit("invalid base"),
	[Err_InvalidNumber] = strlit("invalid number"),
	[Err_InvalidEscapeSequence] = strlit("invalid escape sequence"),
	[Err_InvalidStringChar] = strlit("invalid string character"),
	[Err_UnclosedString] = strlit("unclosed string"),
	[Err_UnclosedComment] = strlit("unclosed comment"),
	[Err_UnexpectedToken] = strlit("unexpected token"),
	[Err_MismatchedListCardinality] = strlit("mismatched cardinality"),
};
	
String error_type_name(enum Error_Type t){
	if(t < 0 || t >= Error_Type__COUNT){
		return strlit("<BAD ERROR TYPE>");
	}
	return error_type_names[(int) t];
}
