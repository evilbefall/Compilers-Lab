#include "code_gen.h"

#include "lex.h"
#include "main.h"
#include "name.h"

#include <stdio.h>
#include <string.h>

int label_count = 0;

void check_IF(void);
void check_WHILE(void);
void check_BEGIN(void);
void check_ID(void);
void start(void);
void statement(void);
char *expression();
char *non_relational();
char *monomial();
char *factor();
void debug(char *str __attribute((unused))) {
#if _DEBUG_ == 1
	fprintf(stderr, "%s\n", str);
#endif
}

void start(void) {
	// start		->	statement+
	if(input_filename != NULL && match(EOI)) {
		err("FILE <%s> IS EMPTY\n", input_filename);
	}
	if((input_filename != NULL)) {
		fprintf(output_file, ".global _start\n");
		fprintf(output_file, ".section\n.text\n\n");
		fprintf(output_file, "_start:\n");
	}
	while(!match(EOI)) {
		statement();
	}
	if(input_filename != NULL) {
		fprintf(output_file, "\tHLT\n");
		fprintf(output_file, "\tmov eax, 1\n");
		fprintf(output_file, "\tmov ebx, 0\n");
		fprintf(output_file, "\tint 0x80\n");
	}
}

void statement(void) {
	debug("ENTER statement");
	// statement ->		expression() SEMI
	//				|	ID COL EQ expression() SEMI
	//				|	IF expression() THEN statement()
	//				|	WHILE expression() DO statement()
	//				|	BEGIN statement* END
	char *tempvar;

	if(match(IF)) {
		advance();
		check_IF();
	} else if(match(WHILE)) {
		advance();
		check_WHILE();
	} else if(match(BEGIN)) {
		advance();
		check_BEGIN();
	} else if(match(ID)) {
		check_ID();
	} else {
		tempvar = expression();
		if(match(SEMI))
			advance();
		else {
			err("Line %d: Missing semicolon\n", yylineno);
		}
		freename(tempvar);
	}
	debug("EXIT statement");
}

char *expression(void) {
	debug("ENTER expression");
	// expression ->	non_relational
	//				|	non_relational EQ non_relational
	//				|	non_relational GT non_relational
	//				|	non_relational LT non_relational
	char *tempvar, *tempvar2;

	tempvar = non_relational(0);
	if(match(EQ)) {
		advance();
		tempvar2 = non_relational(0);
		fprintf(output_file, "\tCMP %s, %s\n", tempvar, tempvar2);
		fprintf(output_file, "\tSETZ %s\n", tempvar);
		freename(tempvar2);
	} else if(match(LT)) {
		advance();
		tempvar2 = non_relational(0);
		fprintf(output_file, "\tCMP %s, %s\n", tempvar, tempvar2);
		fprintf(output_file, "\tSETL %s\n", tempvar);
		freename(tempvar2);
	} else if(match(GT)) {
		advance();
		tempvar2 = non_relational(0);
		fprintf(output_file, "\tCMP %s, %s\n", tempvar, tempvar2);
		fprintf(output_file, "\tSETG %s\n", tempvar);
		freename(tempvar2);
	}
	debug("EXIT expression");
	return tempvar;
}

char *non_relational(int inverted) {
	debug("ENTER non_relational");
	// non_relational	->	monomial + non_relational
	//					|	monomial - non_relational
	//					|	monomial
	char *tempvar, *tempvar2;
	int sign = 0;
	tempvar = monomial(0);
	if(match(PLUS) || match(MINUS)) {
		if(match(MINUS)) {
			sign = 1 ^ inverted;
			inverted = 1;
		} else {
			sign = 0 ^ inverted;
			inverted = 0;
		}
		advance();
		tempvar2 = non_relational(inverted);
		if(sign == 1)
			fprintf(output_file, "\tSUB %s, %s\n", tempvar, tempvar2);
		else if(sign == 0)
			fprintf(output_file, "\tADD %s, %s\n", tempvar, tempvar2);
		else
			err("Check sign innon_relational");
		freename(tempvar2);
		// } else if(match(MINUS)) {
		// 	advance();
		// 	tempvar2 = non_relational(0);
		// 	freename(tempvar2);
	}
	debug("EXIT non_relational");
	return tempvar;
}

char *monomial(int inverted) {
	debug("ENTER monomial");
	// monomial ->		factor MUL monomial
	//				|	factor DIV monomial
	//				|	factor
	char *tempvar, *tempvar2;
	int sign = 0;
	tempvar = factor();
	if(match(MUL) || match(DIV)) {
		if(match(DIV)) {
			sign = 1 ^ inverted;
			inverted = 1;
		} else {
			sign = 0 ^ inverted;
			inverted = 0;
		}
		advance();
		tempvar2 = monomial(inverted);
		if(sign == 1) {
			fprintf(output_file, "\tDIV %s, %s\n", tempvar, tempvar2);
		} else {
			fprintf(output_file, "\tMUL %s, %s\n", tempvar, tempvar2);
		}
		freename(tempvar2);
	}
	debug("EXIT monomial");
	return tempvar;
}

char *factor(void) {
	debug("ENTER factor");
	char *tempvar;

	if(match(NUMBER) || match(ID)) {
		fprintf(output_file, "\tMOV %s, %.*s\n", tempvar = newname(), yyleng, yytext);
		advance();
	} else if(match(LP)) {
		advance();
		tempvar = non_relational(0);
		if(match(RP)) {
			advance();
		} else {
			err("Line %d: Mismatched parenthesis\n", yylineno);
		}
	} else {
		err("Line %d: Number or identifier expected\n", yylineno);
	}
	debug("EXIT factor");
	return tempvar;
}

void check_IF(void) {
	char *tempvar;
	tempvar = expression();
	fprintf(output_file, "\tCMP %s, 0\n", tempvar);
	label_count++;
	int temp_label = label_count;
	fprintf(output_file, "\tJE L%d\n", temp_label);
	if(match(THEN)) {
		advance();
		statement();
		fprintf(output_file, "L%d:\n", temp_label);
	} else {
		err("THEN clause not found%s", "\n");
	}
	freename(tempvar);
}

void check_WHILE(void) {
	char *tempvar;
	label_count++;
	int temp_label = label_count;
	label_count++;
	int temp_label2 = label_count;
	fprintf(output_file, "L%d:\n", temp_label);
	tempvar = expression();
	fprintf(output_file, "\tCMP %s, 0\n", tempvar);
	fprintf(output_file, "\tJE L%d\n", temp_label2);
	if(match(DO)) {
		advance();
		statement();
		fprintf(output_file, "\tJMP L%d\n", temp_label);
		fprintf(output_file, "L%d:\n", temp_label2);
	} else {
		err("Line %d:DO clause not found\n", yylineno);
	}
	freename(tempvar);
}

void check_BEGIN(void) {
	while(!match(END) && !match(EOI)) {
		statement();
	}
	if(match(END)) {
		advance();
	} else {
		err("Line %d:No END clause found\n", yylineno);
	}
}

void check_ID(void) {
	char *tempvar2;
	char tempvar_arr[1024];
	strncpy(tempvar_arr, yytext, yyleng);
	tempvar_arr[yyleng] = 0;
	advance();
	if(match(COL)) {
		advance();
		if(match(EQ)) {
			advance();
			tempvar2 = expression();
			fprintf(output_file, "\tMOV %s, %s\n", tempvar_arr, tempvar2);
			if(match(SEMI))
				advance();
			else {
				err("Line %d: Missing semicolon\n", yylineno);
			}
		} else {
			err("Line %d: Assignment operator expected", yylineno);
		}
	} else {
		err("Line %d: Assignment operator expected", yylineno);
	}
	freename(tempvar2);
}