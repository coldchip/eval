/*
	The Chip Language Intepreter
	@Copyright Ryan Loh 2023
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include "chip.h"
#include "list.h"
#include "intepreter.h"

List globals; // generate instances of all classes to allow static invoking
static char *constants[8192] = {};
static List program;
Object *cache[8192] = {};

void load_file(const char *name) {
	FILE *fp = fopen(name, "rb");
	if(!fp) {
		printf("unable to load file %s\n", name);
		exit(1);
	}

	char magic[8];
	fread(magic, sizeof(char), 8, fp);

	int pgm_size = 0;
	fread(&pgm_size, sizeof(pgm_size), 1, fp);
	fseek(fp, pgm_size + 4, SEEK_CUR);

	int constants_count = 0;
	fread(&constants_count, sizeof(constants_count), 1, fp);

	for(int z = 0; z < constants_count; z++) {
		int constant_length = 0;
		fread(&constant_length, sizeof(constant_length), 1, fp);

		char constant_data[constant_length + 1];
		memset(constant_data, 0, sizeof(constant_data));

		fread(&constant_data, sizeof(char), constant_length, fp);

		SET_CONST(z, strdup(constant_data));
	}

	fseek(fp, sizeof(magic) + 4, SEEK_SET);

	int class_count = 0;
	fread(&class_count, sizeof(class_count), 1, fp);

	for(int i = 0; i < class_count; i++) {
		short method_count = 0;
		short class_name = 0;
		fread(&method_count, sizeof(method_count), 1, fp);
		fread(&class_name, sizeof(class_name), 1, fp);

		Class *class = malloc(sizeof(Class));
		class->name = GET_CONST(class_name);
		list_clear(&class->method);

		for(int x = 0; x < method_count; x++) {
			short op_count = 0;
			short method_id = 0;
			fread(&op_count, sizeof(op_count), 1, fp);
			fread(&method_id, sizeof(method_id), 1, fp);

			Method *method = malloc(sizeof(Method));
			method->name = GET_CONST(method_id);
			method->code_count = op_count;
			
			method->codes = malloc(sizeof(Op *) * op_count);

			for(int y = 0; y < op_count; y++) {
				char   op = 0;
				double op_left = 0;
				fread(&op, sizeof(op), 1, fp);
				fread(&op_left, sizeof(op_left), 1, fp);

				Op *ins = malloc(sizeof(Op));
				ins->op = op;
				ins->left = op_left;

				method->codes[y] = ins;
			}

			list_insert(list_end(&class->method), method);
		}

		list_insert(list_end(&program), class);
	}

	fclose(fp);
}

void emit_print() {
	for(ListNode *cn = list_begin(&program); cn != list_end(&program); cn = list_next(cn)) {
		Class *c = (Class*)cn;

		printf("@class %s\n", c->name);

		for(ListNode *mn = list_begin(&c->method); mn != list_end(&c->method); mn = list_next(mn)) {
			Method *m = (Method*)mn;

			int line = 1;
			printf("\t@method %s\n", m->name);
			printf("\tLINE\tOP\tVALUE\n\t--------------------------\n");

			for(int pc = 0; pc < m->code_count; pc++) {
				Op *ins = m->codes[pc];
				switch(ins->op) {
					case OP_LOAD_VAR: {
						printf("\t%i\tLOAD_VAR\t%s\n", line, GET_CONST(ins->left));
					}
					break;
					case OP_STORE_VAR: {
						printf("\t%i\tSTORE_VAR\t%s\n", line, GET_CONST(ins->left));
					}
					break;
					case OP_POP: {
						printf("\t%i\tPOP\t\n", line);
					}
					break;
					case OP_CMPEQ: {
						printf("\t%i\tCMPEQ\n", line);
					}
					break;
					case OP_CMPGT: {
						printf("\t%i\tCMPGT\n", line);
					}
					break;
					case OP_CMPLT: {
						printf("\t%i\tCMPLT\n", line);
					}
					break;
					case OP_ADD: {
						printf("\t%i\tADD\n", line);
					}
					break;
					case OP_SUB: {
						printf("\t%i\tSUB\n", line);
					}
					break;
					case OP_MUL: {
						printf("\t%i\tMUL\n", line);
					}
					break;
					case OP_DIV: {
						printf("\t%i\tDIV\n", line);
					}
					break;
					case OP_MOD: {
						printf("\t%i\tMOD\n", line);
					}
					break;
					case OP_OR: {
						printf("\t%i\tOR\n", line);
					}
					break;
					case OP_LOAD_NUMBER: {
						printf("\t%i\tLOAD_NUMBER\t%f\n", line, ins->left);
					}
					break;
					case OP_LOAD_CONST: {
						printf("\t%i\tLOAD_CONST\t%i\t//%s\n", line, (int)ins->left, GET_CONST(ins->left));
					}
					break;
					case OP_LOAD_MEMBER: {
						printf("\t%i\tLOAD_MEMBER\t%s\n", line, GET_CONST(ins->left));
					}
					break;
					case OP_STORE_MEMBER: {
						printf("\t%i\tSTORE_MEMBER\t%s\n", line, GET_CONST(ins->left));
					}
					break;
					case OP_CALL: {
						printf("\t%i\tCALL\t\tARGLEN: %i\n", line, (int)ins->left);
					}
					break;
					case OP_SYSCALL: {
						printf("\t%i\tSYSCALL\t\tARGLEN: %i\n", line, (int)ins->left);
					}
					break;
					case OP_NEW: {
						printf("\t%i\tNEW\t\tARGLEN: %i\n", line, (int)ins->left);
					}
					break;
					case OP_NEWARRAY: {
						printf("\t%i\tNEWARRAY\t%s\n", line, GET_CONST(ins->left));
					}
					break;
					case OP_LOAD_ARRAY: {
						printf("\t%i\tLOAD_ARRAY\n", line);
					}
					break;
					case OP_STORE_ARRAY: {
						printf("\t%i\tSTORE_ARRAY\n", line);
					}
					break;
					case OP_JMPIFT: {
						printf("\t%i\tJMPIFT\t%i\n", line, (int)ins->left);
					}
					break;
					case OP_JMP: {
						printf("\t%i\tJMP\t%i\n", line, (int)ins->left);
					}
					break;
					case OP_RET: {
						printf("\t%i\tRET\t\n", line);
					}
					break;
				}

				line++;
			}
		}
	}
}

Op *op_at(List *program, int line) {
	int size = 1;
	ListNode *position;

	for(position = list_begin(program); position != list_end(program); position = list_next(position)) {
		if(size == line) {
			return (Op*)position;
		}
		size++;
	}

	return NULL;
}

void store_var(List *vars, char *name, Object *object) {
	Var *previous = load_var(vars, name);
	if(previous) {
		// DECREF(previous->object);
		previous->object = object;
		// INCREF(object);
		return;
	}

	Var *var = malloc(sizeof(Var));
	strcpy(var->name, name);
	var->object = object;
	// INCREF(object);
	list_insert(list_end(vars), var);
}

void store_var_double(List *vars, char *name, double data) {
	Var *previous = load_var(vars, name);
	if(previous) {
		// DECREF(previous->object);
		*(double*)&previous->object = data;
		// INCREF(object);
		return;
	}

	Var *var = malloc(sizeof(Var));
	strcpy(var->name, name);
	*(double*)&var->object = data;
	// INCREF(object);
	list_insert(list_end(vars), var);
}

Var *load_var(List *vars, char *name) {
	for(ListNode *i = list_begin(&globals); i != list_end(&globals); i = list_next(i)) {
		Var *var = (Var*)i;
		if(strcmp(name, var->name) == 0) {
			return var;
		}
	}

	for(ListNode *i = list_begin(vars); i != list_end(vars); i = list_next(i)) {
		Var *var = (Var*)i;
		if(strcmp(name, var->name) == 0) {
			return var;
		}
	}
	return NULL;
}

void free_var(Var *var) {
	DECREF(var->object);
	list_remove(&var->node);
	free(var);
}

Class *get_class(char *name) {
	for(ListNode *i = list_begin(&program); i != list_end(&program); i = list_next(i)) {
		Class *class = (Class*)i;
		if(strcmp(name, class->name) == 0) {
			return class;
		}
	}
	return NULL;
}

Method *get_method(char *name1, char *name2) {
	Class *class = get_class(name1);
	if(class) {
		for(ListNode *i = list_begin(&class->method); i != list_end(&class->method); i = list_next(i)) {
			Method *method = (Method*)i;
			if(strcmp(name2, method->name) == 0) {
				return method;
			}
		}
	}
	return NULL;
}

int allocs = 0;

Object *new_object(Type type, char *name) {
	Object *o = malloc(sizeof(Object));
	o->data_string = NULL;
	o->data_number = 0;
	o->array = NULL;
	o->bound = NULL;
	o->type = type;
	o->name = strdup(name);
	o->refs = 0;
	list_clear(&o->varlist);

	allocs++;

	if(type == TY_VARIABLE) {
		Class *skeleton = get_class(name);
		if(!skeleton) {
			printf("Error, class %s not defined\n", name);
			exit(1);
		}

		for(ListNode *i = list_begin(&skeleton->method); i != list_end(&skeleton->method); i = list_next(i)) {
			Method *method = (Method*)i;
			Object *var = new_object(TY_FUNCTION, method->name);
			var->bound = o;
			var->method = method;
			store_var(&o->varlist, method->name, var);
		}
	}

	return o;
}

void incref_object(Object *object) {
	object->refs++;
}

void decref_object(Object *object) {
	object->refs--;

	if(!object->refs > 0) {
		free_object(object);
	}
}

void free_object(Object *object) {
	while(!list_empty(&object->varlist)) {
		Var *var = (Var*)list_remove(list_begin(&object->varlist));
		free_var(var);
	}

	if(object->data_string) {
		free(object->data_string);
	}
	if(object->array) {
		free(object->array);
	}
	free(object->name);
	free(object);

	allocs--;

	// printf("OBJECTS STILL REFRENCED: %i\n\n", allocs);
}

Object *empty_return = NULL;

Object *eval(Object *instance, Method *method, Object **args, int args_length) {
	/*
		assume if instance == NULL then method is static
	*/
	List varlist;
	list_clear(&varlist);

	if(instance != NULL) {
		store_var(&varlist, "this", instance);
	}

	Object *stack[512];
	int sp = 0;

	double stack2[512];
	int sp2 = 0;

	Object *ret = empty_return;
	INCREF(ret);

	if(args) {
		for(int i = 0; i < args_length; i++) {
			Object *arg = args[i];
			PUSH_STACK_OBJECT_2(arg);
		}
	}

	int pc = 0;
	while(pc < method->code_count) {
		Op *current = method->codes[pc];
		switch(current->op) {
			case OP_LOAD_VAR: {
				Var *var = load_var(&varlist, GET_CONST(current->left));
				if(!var) {
					printf("unable to load variable %s as it is not found\n", GET_CONST(current->left));
					exit(1);
				}

				Object *v1 = var->object;
				
				PUSH_STACK_OBJECT_2(v1);
			}
			break;
			case OP_STORE_VAR: {
				Object *v1 = POP_STACK_OBJECT_2();
				store_var(&varlist, GET_CONST(current->left), v1);
			}
			break;
			case OP_LOAD_NUMBER: {
				PUSH_STACK_2((double)current->left);


				// char *a = (char*)&stack2[0];

				// for(int i = 0; i < 128; i++) {
				// 	if(i % 8 == 0) {
				// 		printf("\n");
				// 	}
				// 	printf("%02x", a[i] & 0xff);
				// }
				// printf("\n");

			}
			break;
			case OP_LOAD_CONST: {
				if(cache[(int)current->left]) {
					Object *o = cache[(int)current->left];
					
					PUSH_STACK_OBJECT_2(o);
				} else {
					Object *o = new_object(TY_ARRAY, "char");
					
					char *str = GET_CONST(current->left);
					int   size = strlen(str);

					o->array = malloc(sizeof(double) * size);

					for(int i = 0; i < size; i++) {
						*(double*)(&o->array[i]) = str[i];
					}

					store_var_double(&o->varlist, "count", size);

					cache[(int)current->left] = o;
					
					PUSH_STACK_OBJECT_2(o);
				}
			}
			break;
			case OP_LOAD_MEMBER: {
				Object *instance = POP_STACK_OBJECT_2();

				Var *var = load_var(&instance->varlist, GET_CONST(current->left));
				if(!var) {
					printf("Unknown variable member %s\n", GET_CONST(current->left));
					exit(1);
				}

				Object *v1 = var->object;
				
				PUSH_STACK_OBJECT_2(v1);
			}
			break;
			case OP_STORE_MEMBER: {
				Object *instance = POP_STACK_OBJECT_2();
				Object *v2 = POP_STACK_OBJECT_2();
				store_var(&instance->varlist, GET_CONST(current->left), v2);
			}
			break;
			case OP_POP: {
				POP_STACK_2();
			}
			break;
			case OP_CMPEQ: {
				double a = POP_STACK_2();
				double b = POP_STACK_2();
				double c = b == a;
				PUSH_STACK_2(c);
			}
			break;
			case OP_CMPGT: {
				double a = POP_STACK_2();
				double b = POP_STACK_2();
				double c = (int)b > (int)a;
				PUSH_STACK_2(c);
			}
			break;
			case OP_CMPLT: {
				double a = POP_STACK_2();
				double b = POP_STACK_2();
				double c = (int)b < (int)a;
				PUSH_STACK_2(c);
			}
			break;
			case OP_ADD: {
				double a = POP_STACK_2();
				double b = POP_STACK_2();
				double c = b + a;
				PUSH_STACK_2(c);
			}
			break;
			case OP_SUB: {
				double a = POP_STACK_2();
				double b = POP_STACK_2();
				double c = b - a;
				PUSH_STACK_2(c);
			}
			break;
			case OP_MUL: {
				double a = POP_STACK_2();
				double b = POP_STACK_2();
				double c = b * a;
				PUSH_STACK_2(c);
			}
			break;
			case OP_DIV: {
				double a = POP_STACK_2();
				double b = POP_STACK_2();
				double c = b / a;
				PUSH_STACK_2(c);
			}
			break;
			case OP_MOD: {
				double a = (int)POP_STACK_2();
				double b = POP_STACK_2();
				double c = (int)b % (int)a;
				PUSH_STACK_2(c);
			}
			break;
			case OP_OR: {
				double a = POP_STACK_2();
				double b = POP_STACK_2();
				double c = (int)b || (int)a;
				PUSH_STACK_2(c);
			}
			break;
			case OP_CALL: {
				Object *instance = POP_STACK_OBJECT_2();

				Object *args[(int)current->left];

				for(int i = 0; i < current->left; i++) {
					Object *arg = POP_STACK_OBJECT_2();
					args[i] = arg;
				}

				if(instance->type != TY_FUNCTION) {
					printf("unknown function call %s\n", instance->data_string);
					exit(1);
				}

				Object *r = eval(instance->bound, instance->method, args, (int)current->left);
				PUSH_STACK_OBJECT_2(r);
			}
			break;
			case OP_SYSCALL: {
				int name = (int)POP_STACK_2();

				if(name == 0) {
					Object *text = POP_STACK();

					char buffer[8192];
					printf("%s", text->data_string);
					scanf("%s", buffer);

					Object *r = new_object(TY_VARIABLE, "String");
					r->data_string = strdup(buffer);

					Object *count = new_object(TY_VARIABLE, "Number");
					count->data_number = strlen(r->data_string);
					store_var(&r->varlist, "count", count);

					PUSH_STACK(r);

					DECREF(text);
					INCREF(r);
				} else if(name == 1) {
					double arg = POP_STACK_2();
					printf("%f\n", arg);

					PUSH_STACK_2(0);
				} else if(name == 2) {
					double arg = POP_STACK_2();
					printf("%c", (char)arg);

					PUSH_STACK_2(0);
				} else if(name == 3) {
					int sockfd = socket(AF_INET, SOCK_STREAM, 0);

					setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

					Object *r = new_object(TY_VARIABLE, "Number");
					r->data_number = sockfd;

					PUSH_STACK(r);

					INCREF(r);
				} else if(name == 4) {
					Object *fd   = POP_STACK();
					Object *ip   = POP_STACK();
					Object *port = POP_STACK();

					struct sockaddr_in servaddr;
					servaddr.sin_family = AF_INET;
					servaddr.sin_addr.s_addr = inet_addr(ip->data_string);
					servaddr.sin_port = htons((int)port->data_number);

					int result = bind((int)fd->data_number, (struct sockaddr*)&servaddr, sizeof(servaddr));
					listen((int)fd->data_number, 5);

					Object *r1 = new_object(TY_VARIABLE, "Number");
					r1->data_number = result == 0;

					PUSH_STACK(r1);

					DECREF(fd);
					DECREF(ip);
					DECREF(port);
					INCREF(r1);
				} else if(name == 5) {
					Object *fd = POP_STACK();

					int newfd = accept((int)fd->data_number, NULL, 0);

					Object *r = new_object(TY_VARIABLE, "Number");
					r->data_number = newfd;

					PUSH_STACK(r);

					DECREF(fd);
					INCREF(r);
				} else if(name == 6) {
					Object *fd = POP_STACK();

					char data[8192];
					read((int)fd->data_number, data, sizeof(data));

					Object *r = new_object(TY_VARIABLE, "String");
					r->data_string = strdup(data);

					Object *count = new_object(TY_VARIABLE, "Number");
					count->data_number = strlen(r->data_string);
					store_var(&r->varlist, "count", count);

					PUSH_STACK(r);

					DECREF(fd);
					INCREF(r);
				} else if(name == 7) {
					Object *fd = POP_STACK();
					Object *data = POP_STACK();
					Object *length = POP_STACK();

					int w = write((int)fd->data_number, data->data_string, (int)length->data_number);
					Object *r1 = new_object(TY_VARIABLE, "Number");
					r1->data_number = w;

					PUSH_STACK(r1);

					DECREF(fd);
					DECREF(data);
					DECREF(length);
					INCREF(r1);
				} else if(name == 8) {
					Object *fd = POP_STACK();

					close((int)fd->data_number);

					PUSH_STACK(empty_return);

					DECREF(fd);
					INCREF(empty_return);
				} else if(name == 9) {
					Object *r1 = new_object(TY_VARIABLE, "Number");
					r1->data_number = rand();

					PUSH_STACK(r1);

					INCREF(r1);
				} else if(name == 10) {
					Object *sec = POP_STACK();

					sleep((int)sec->data_number);

					PUSH_STACK(empty_return);

					DECREF(sec);
					INCREF(empty_return);
				} else if(name == 11) {
					Object *i = POP_STACK();

					int length = strlen(i->data_string);

					Object *r = new_object(TY_VARIABLE, "Number");
					r->data_number = length;

					PUSH_STACK(r);

					DECREF(i);
					INCREF(r);
				} else if(name == 12) {
					Object *i = POP_STACK();
					Object *index = POP_STACK();

					Object *r = new_object(TY_VARIABLE, "Number");
					r->data_number = (int)i->data_string[(int)index->data_number];

					PUSH_STACK(r);

					DECREF(i);
					DECREF(index);
					INCREF(r);
				} else {
					printf("unknown syscall %i\n", name);
					exit(1);
				}
			}
			break;
			case OP_NEW: {
				char *name = GET_CONST((int)current->left);
				if(!name) {
					printf("constant %s not found\n", name);
					exit(1);
				}
				Object *o = new_object(TY_VARIABLE, name);
				PUSH_STACK_OBJECT_2(o);

				// Object *args[(int)current->left];

				// for(int i = 0; i < current->left; i++) {
				// 	Object *arg = POP_STACK();
				// 	args[i] = arg;
				// }

				// Object *name = POP_STACK();

				// Class *class = get_class(name->data_string);
				// if(!class) {
				// 	printf("cannot clone class %s\n", name->data_string);
				// 	exit(1);
				// }

				// Object *instance = new_object(TY_VARIABLE, name->data_string);

				// INCREF(instance);
				
				// Method *constructor = get_method(name->data_string, "constructor");
				// if(constructor) {
				// 	DECREF(eval(instance, constructor, args, (int)current->left));
				// }
				
				// PUSH_STACK(instance);

				// DECREF(name);
			}
			break;
			case OP_NEWARRAY: {
				double size = POP_STACK_2();

				Object *instance = new_object(TY_ARRAY, GET_CONST((int)current->left));
				instance->array = malloc(sizeof(double) * (int)size);

				for(int i = 0; i < (int)size; i++) {
					instance->array[i] = empty_return;
				}

				store_var_double(&instance->varlist, "count", size);

				PUSH_STACK_OBJECT_2(instance);
			}
			break;
			case OP_LOAD_ARRAY: {
				double index = POP_STACK_2();
				Object *instance = POP_STACK_OBJECT_2();

				Object *item = instance->array[(int)index];

				PUSH_STACK_OBJECT_2(item);
			}
			break;
			case OP_STORE_ARRAY: {
				double index = POP_STACK_2();
				Object *instance = POP_STACK_OBJECT_2();
				Object *value = POP_STACK_OBJECT_2();

				instance->array[(int)index] = value;
			}
			break;
			case OP_JMPIFT: {
				double a = POP_STACK_2();
				double b = POP_STACK_2();

				if(a == b) {
					pc = (int)current->left - 1;
					continue;
				}
			}
			break;
			case OP_JMP: {
				pc = (int)current->left - 1;

				continue;
			}
			break;
			case OP_RET: {
				ret = POP_STACK_OBJECT_2();

				goto cleanup;
			}
			break;
			default: {
				printf("illegal instruction %i\n", current->op);
				exit(1);
			}
			break;
		}

		pc++;
	}

	cleanup:;

	// remove vars
	// while(!list_empty(&varlist)) {
	// 	Var *var = (Var*)list_remove(list_begin(&varlist));
		
	// 	free_var(var);
	// }

	return ret;
}

void intepreter(const char *input) {
	/* code */
	signal(SIGPIPE, SIG_IGN);

	list_clear(&globals);

	list_clear(&program);

	load_file(input);

	emit_print();

	for(ListNode *cn = list_begin(&program); cn != list_end(&program); cn = list_next(cn)) {
		Class *c = (Class*)cn;

		Object *var = new_object(TY_VARIABLE, c->name);
		store_var(&globals, c->name, var);
	}

	empty_return = new_object(TY_VARIABLE, "Number");
	empty_return->data_number = 0;
	INCREF(empty_return);

	Method *method_main = get_method("Main", "main");
	if(!method_main) {
		printf("entry point method main not found\n");
		exit(1);
	}

	eval(NULL, method_main, NULL, 0);
}