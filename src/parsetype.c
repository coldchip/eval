#include <stdlib.h>
#include <stdio.h>
#include "parse.h"
#include "varscope.h"
#include "parsetype.h"
#include "type.h"

/*
	process class & method types without statements 
*/

void parsetype_peek_class(Node *node) {
	for(ListNode *c = list_begin(&node->bodylist); c != list_end(&node->bodylist); c = list_next(c)) {
		Node *class = (Node*)c;
		if(class->type != ND_CLASS) continue;

		type_insert(class->token->data);
	}


	for(ListNode *c = list_begin(&node->bodylist); c != list_end(&node->bodylist); c = list_next(c)) {
		Node *class = (Node*)c;
		if(class->type != ND_CLASS) continue;

		Ty *class_ty = type_get(class->token->data);

		for(ListNode *m = list_begin(&class->bodylist); m != list_end(&class->bodylist); m = list_next(m)) {
			Node *method = (Node*)m;

			switch(method->type) {
				case ND_CLASS_DECL: {
					Node *type = method->data_type;
					Ty *ty = type_get(type->token->data);
					if(!ty) {
						printf("unknown declaration type: %s\n", type->token->data);
						exit(1);
					}

					insert_method(class_ty, method->token->data, NULL, 0, ty);
				}
				break;
				case ND_METHOD: {
					Node *params = method->args;
					for(ListNode *p = list_begin(&params->bodylist); p != list_end(&params->bodylist); p = list_next(p)) {
						Node *param = (Node*)p;

					}

					Node *type = method->data_type;
					Ty *ty = type_get(type->token->data);
					if(!ty) {
						printf("unknown return type: %s\n", type->token->data);
						exit(1);
					}

					method->method = insert_method(class_ty, method->token->data, NULL, 0, ty);
				}
				break;
			}
		}
	}
	printf("\n\n");
}

void parsetype_program(Node *node) {
	for(ListNode *n = list_begin(&node->bodylist); n != list_end(&node->bodylist); n = list_next(n)) {
		Node *entry = (Node*)n;
		parsetype_class(entry);
	}
}

void parsetype_class(Node *node) {
	printf(" - class %s\n", node->token->data);

	Ty *ty = type_get(node->token->data);

	for(ListNode *n = list_begin(&node->bodylist); n != list_end(&node->bodylist); n = list_next(n)) {
		Node *entry = (Node*)n;
		switch(entry->type) {
			case ND_CLASS_DECL: {
				// parsetype_class_decl(entry);
			}
			break;
			case ND_METHOD: {
				parsetype_method(ty, entry);
			}
			break;
		}
	}
}

void parsetype_class_decl(Node *node) {
	Node *type = node->data_type;
	Ty *ty = type_get(type->token->data);
	if(!ty) {
		printf("unknown type %s in declaration\n", type->token->data);
		exit(1);
	}

	printf("\t - %s %s;\n", type->token->data, node->token->data);
}

void parsetype_method(Ty *class_ty, Node *node) {
	printf("\t - method %s %p\n", node->token->data, node->method);

	varscope_push();
	if(node->args) {
		Node *params = node->args;
		for(ListNode *p = list_begin(&params->bodylist); p != list_end(&params->bodylist); p = list_next(p)) {
			Node *param = (Node*)p;

			Node *type = param->data_type;
			Ty *ty = type_get(type->token->data);
			if(!ty) {
				printf("unknown type %s in declaration\n", type->token->data);
				exit(1);
			}

			varscope_add(param->token->data, ty);
		}
	}

	varscope_add("this", class_ty);

	for(ListNode *s = list_begin(&node->bodylist); s != list_end(&node->bodylist); s = list_next(s)) {
		Node *stmt = (Node*)s;
		parsetype_stmt(stmt);

	}
	varscope_pop();
}

void parsetype_stmt(Node *node) {
	switch(node->type) {
		case ND_IF: {
			parsetype_if(node);
		}
		break;
		case ND_WHILE: {
			parsetype_while(node);
		}
		break;
		case ND_BLOCK: {
			for(ListNode *s = list_begin(&node->bodylist); s != list_end(&node->bodylist); s = list_next(s)) {
				Node *stmt = (Node*)s;
				parsetype_stmt(stmt);
			}
		}
		break;
		case ND_RETURN: {
			parsetype_return(node);
		}
		break;
		case ND_DECL: {
			parsetype_decl(node);
		}
		break;
		case ND_ASSIGN: {
			parsetype_assign(node);
		}
		break;
		case ND_EXPR: {
			parsetype_expr(node);
		}
		break;
	}
}

void parsetype_if(Node *node) {
	parsetype_unfold_expr(node->condition);
	parsetype_stmt(node->body);
	if(node->alternate) {
		parsetype_stmt(node->alternate);
	}
}

void parsetype_while(Node *node) {
	parsetype_unfold_expr(node->condition);
	parsetype_stmt(node->body);
}

void parsetype_return(Node *node) {
	parsetype_unfold_expr(node->body);
}

void parsetype_decl(Node *node) {
	Node *type = node->data_type;
	Ty *ty = type_get(type->token->data);
	if(!ty) {
		printf("unknown type %s in declaration\n", type->token->data);
		exit(1);
	}

	varscope_add(node->token->data, ty);

	parsetype_unfold_expr(node->body);
}

void parsetype_assign(Node *node) {
	parsetype_unfold_expr(node->left);
	parsetype_unfold_expr(node->right);
}

void parsetype_expr(Node *node) {
	parsetype_unfold_expr(node->body);
}

Ty *parsetype_unfold_expr(Node *node) {
	switch(node->type) {
		case ND_EQ:
		case ND_GT:
		case ND_LT:
		case ND_ADD:
		case ND_SUB:
		case ND_MUL:
		case ND_DIV:
		case ND_MOD:
		case ND_OR: {
			Ty *left = parsetype_unfold_expr(node->left);
			Ty *right = parsetype_unfold_expr(node->right);
			return left;
		}
		break;
		case ND_MEMBER: {
			Ty *parent = parsetype_unfold_expr(node->body);

			TyMethod *method = type_get_method(parent, node->token->data);
			if(!method) {
				printf("unknown member %s\n", node->token->data);
				exit(1);
			}

			return method->type;
		}
		break;
		case ND_CALL: {
			Ty *parent = parsetype_unfold_expr(node->body);

			TyMethod *method = type_get_method(parent, node->token->data);
			if(!method) {
				printf("call to unknown member %s\n", node->token->data);
				exit(1);
			}

			for(ListNode *a = list_begin(&node->args->bodylist); a != list_end(&node->args->bodylist); a = list_next(a)) {
				Node *arg = (Node*)a;
				parsetype_unfold_expr(arg);
			}

			node->method = method;

			return method->type;
		}
		break;
		case ND_VARIABLE: {
			VarScope *var = varscope_get(node->token->data);
			Ty       *ty  = type_get(node->token->data);

			if(!var && !ty) {
				printf("undefined variable %s\n", node->token->data);
				exit(1);
			}

			return var ? var->type : ty;
		}
		break;
		case ND_NUMBER: {
			return type_get("int");
		}
		break;
		case ND_FLOAT: {
			return type_get("float");
		}
		break;
		case ND_STRING: {
			return type_get("char");
		}
		break;
		case ND_SYSCALL:
		case ND_NEW:
		case ND_NEWARRAY: {
			Node *type = node->data_type;
			Ty *ty = type_get(type->token->data);
			if(!ty) {
				printf("unknown type %s\n", type->token->data);
				exit(1);
			}

			return ty;
		}
		break;
		case ND_ARRAYMEMBER: {
			parsetype_unfold_expr(node->index);
			return parsetype_unfold_expr(node->body);
		}
		break;
		default: {
			printf("unknown node %i\n", node->type);
			exit(1);
		}
		break;
	}
}

void parsetype(Node *node) {
	type_clear();
	varscope_clear();

	Ty *int_type = type_insert("int");
	insert_method(int_type, "count", NULL, 0, int_type);
	Ty *char_type = type_insert("char");
	insert_method(char_type, "count", NULL, 0, int_type);
	type_insert("float");
	type_insert("void");

	parsetype_peek_class(node);
	parsetype_program(node);
}