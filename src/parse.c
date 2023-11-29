#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "tokenize.h"
#include "parse.h"
#include "varscope.h"

Node *new_node(NodeType type, Token *token) {
	Node *node  = malloc(sizeof(Node));
	node->type  = type;
	node->data_type = 0;
	node->token = token;
	node->left  = NULL;
	node->right = NULL;
	node->index = NULL;
	node->body  = NULL;
	node->alternate = NULL;
	node->condition = NULL;
	node->args = NULL;

	list_clear(&node->bodylist);

	return node;
}

Node *new_node_binary(NodeType type, Token *token, Node *left, Node *right) {
	Node *node = new_node(type, token);

	if(left) {
		node->left = left;
	}

	if(right) {
		node->right = right;
	}

	return node;
}

bool is_type(Token **current) {
	if(equals_string(current, "Number")) {
		return true;
	}
	return false;
}

bool is_class(Token **current) {
	if(equals_string(current, "class")) {
		return true;
	}
	return false;
}

bool is_method(Token **current) {
	if(equals_string(current, "function")) {
		return true;
	}
	return false;
}

/* 
	<type> a = (expr)
*/

bool is_declaration(Token **current) {
	Token *state = *current;

	if(consume_type(current, TK_IDENTIFIER)) {
		if(consume_type(current, TK_IDENTIFIER)) {
			*current = state;
			return true;
		}
		if(consume_string(current, "[")) {
			if(consume_string(current, "]")) {
				if(consume_type(current, TK_IDENTIFIER)) {
					*current = state;
					return true;
				}
			}
		}
	}
	*current = state;
	return false;
}

/* 
	a = (expr)
*/

bool is_assign(Token **current) {
	Token *state = *current;

	/* todo: improve */
	if(parse_expr(current)) {
		if(consume_string(current, "=")) {
			*current = state;			
			return true;
		}
	}
	*current = state;
	return false;
}

static Node *parse_program(Token **current) {
	Node *node = new_node(ND_PROGRAM, NULL);

	while(is_class(current)) {
		list_insert(list_end(&node->bodylist), parse_class(current));
	}

	return node;
}

static Ty *parse_type(Token **current) {
	Ty *type = type_get_class((*current)->data);
	if(type) {
		consume_type(current, TK_IDENTIFIER);
		if(consume_string(current, "[")) {
			expect_string(current, "]");
		}
		return type;
	} else {
		printf("error, unknown type '%s'\n", (*current)->data);
		exit(1);
	}

	return NULL;
}

static Node *parse_class(Token **current) {
	Node *node = new_node(ND_CLASS, NULL);

	expect_type(current, TK_IDENTIFIER);

	node->token = *current;

	/* inserts type */
	type_insert((*current)->data);

	expect_type(current, TK_IDENTIFIER);

	expect_string(current, "{");

	while(is_method(current)) {
		list_insert(list_end(&node->bodylist), parse_method(current));
	}

	expect_string(current, "}");

	return node;
}

/*
	function <identifier>(<args>) {
		
	}

	function operator<op>() {
		
	}
*/

static Node *parse_method(Token **current) {
	Node *node = new_node(ND_METHOD, NULL);

	expect_type(current, TK_IDENTIFIER);

	node->token = *current;

	if(consume_string(current, "operator")) {
		insert_method((*current)->data);
		expect_type(current, TK_PUNCTUATION);
	} else {
		insert_method((*current)->data);
		expect_type(current, TK_IDENTIFIER);
	}

	varscope_push();

	expect_string(current, "(");
	node->args = parse_params(current);
	expect_string(current, ")");

	if(consume_string(current, ":")) {
		expect_string(current, "[");
		if(consume_string(current, "static")) {
			node->modifier = MOD_STATIC;
		} else {
			printf("unknown type modifier\n");
		}
		expect_string(current, "]");
	}

	expect_string(current, "{");

	varscope_add("this", type_current_class());

	while(!equals_string(current, "}")) {
		list_insert(list_end(&node->bodylist), parse_stmt(current));
	}

	varscope_pop();

	expect_string(current, "}");

	return node;
}

Node *parse_param(Token **current) {
	parse_type(current);
	Node *node = new_node(ND_VARIABLE, *current);

	varscope_add((*current)->data, NULL);
	
	expect_type(current, TK_IDENTIFIER);

	return node;
}

Node *parse_params(Token **current) {
	Node *node = new_node(ND_PARAM, NULL);
	if(equals_string(current, ")")) {
		return node;
	}

	node->length = 1;

	list_insert(list_end(&node->bodylist), parse_param(current));

	while(!equals_string(current, ")")) {
		expect_string(current, ",");
		list_insert(list_end(&node->bodylist), parse_param(current));
		node->length++;
	}
	return node;
}

Node *parse_arg(Token **current) {
	Node *node = parse_expr(current);
	return node;
}

Node *parse_args(Token **current) {
	Node *node = new_node(ND_ARG, NULL);
	if(equals_string(current, ")")) {
		return node;
	}

	node->length = 1;

	list_insert(list_end(&node->bodylist), parse_arg(current));

	while(!equals_string(current, ")")) {
		expect_string(current, ",");
		list_insert(list_end(&node->bodylist), parse_arg(current));
		node->length++;
	}
	return node;
}

static Node *parse_stmt(Token **current) {
	if(consume_string(current, "if")) {
		Node *node = new_node(ND_IF, NULL);
		expect_string(current, "(");
		node->condition = parse_expr(current);
		expect_string(current, ")");
		node->body = parse_stmt(current);
		if(consume_string(current, "else")) {
			node->alternate = parse_stmt(current);
		}
		return node;
	} else if(consume_string(current, "while")) {
		Node *node = new_node(ND_WHILE, NULL);

		expect_string(current, "(");
		node->condition = parse_expr(current);
		expect_string(current, ")");
		node->body = parse_stmt(current);
		return node;
	} else if(consume_string(current, "{")) {
		varscope_push();

		Node *node = new_node(ND_BLOCK, NULL);

		while(!equals_string(current, "}")) {
			list_insert(list_end(&node->bodylist), parse_stmt(current));
		}

		expect_string(current, "}");

		varscope_pop();

		return node;
	} else if(consume_string(current, "return")) {
		Node *node = new_node(ND_RETURN, NULL);
		node->body = parse_expr(current);
		expect_string(current, ";");
		return node;
	} else {
		Node *node = parse_expr_stmt(current);
		expect_string(current, ";");
		return node;
	}
}

static Node *parse_expr_stmt(Token **current) {
	if(is_declaration(current)) {
		/* <type> name = a; */

		Ty *ty = parse_type(current);

		Node *node = new_node(ND_DECLARATION, *current);

		varscope_add((*current)->data, ty);

		expect_type(current, TK_IDENTIFIER);

		if(consume_string(current, "=")) {
			node->body = parse_expr(current);
		}

		return node;
	} else if(is_assign(current)) {
		/* x.y = z */
		Node *node = new_node(ND_ASSIGN, NULL);
		node->left = parse_expr(current);
		if(consume_string(current, "=")) {
			node->right = parse_expr(current);
		}

		return node;
	} else {
		/* x.y.z() */
		Node *node = new_node(ND_EXPR, NULL);
		node->body = parse_expr(current);

		return node;
	}
}

Node *parse(List *tokens) {
	type_clear();
	varscope_clear();

	Token *current = (Token*)list_begin(tokens);
	return parse_program(&current);
}

DataType get_common_type(Node *left, Node *right) {
	if(left->data_type == TYPE_FLOAT || right->data_type == TYPE_FLOAT) {
		return TYPE_FLOAT;
	}
	if(left->data_type == TYPE_INT || right->data_type == TYPE_INT) {
		return TYPE_INT;
	}

	return left->data_type;
}

Ty *unfold_member_type(Node *node) {
	if(!node) {
		return;
	}
	Ty *var = unfold_member_type(node->body);

	if(!node->body) {
		VarScope *var = varscope_get(node->token->data);
		return var->type;
	} else {
		printf("hehehe %p %s\n", type_get_method(var, node->token->data), node->token->data);
	}

	printf("Mem: %s\n", node->token->data);
}

void normalize_type(Node *node) {
	if(node->left) {
		normalize_type(node->left);
	}
	if(node->right) {
		normalize_type(node->right);
	}

	if(node->left && node->right) {
		node->data_type = get_common_type(node->left, node->right);
	}

	if(node->type == ND_MEMBER) {
		unfold_member_type(node);
	}
}