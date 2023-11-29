#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokenize.h"
#include "parse.h"
#include "varscope.h"
#include "type.h"

Node *parse_expr(Token **current) {
	Node *node = parse_or(current);
	normalize_type(node);
	return node;
}

static Node *parse_or(Token **current) {
	Node *node = parse_equality(current);
	for(;;) {
		Token *token = *current;
		if(consume_string(current, "or")) {
			node = new_node_binary(ND_OR, token, node, parse_equality(current));
			continue;
		}
		return node;
	}
}

static Node *parse_equality(Token **current) {
	Node *node = parse_relational(current);
	for(;;) {
		Token *token = *current;
		if(consume_string(current, "$")) {
			node = new_node_binary(ND_EQ, token, node, parse_relational(current));
			continue;
		}
		return node;
	}
}

static Node *parse_relational(Token **current) {
	Node *node = parse_add_sub(current);
	for(;;) {
		Token *token = *current;
		if(consume_string(current, ">")) {
			node = new_node_binary(ND_GT, token, node, parse_add_sub(current));
			continue;
		}
		if(consume_string(current, "<")) {
			node = new_node_binary(ND_LT, token, node, parse_add_sub(current));
			continue;
		}
		return node;
	}
}

static Node *parse_add_sub(Token **current) {
	Node *node = parse_mul_div(current);
	for(;;) {
		Token *token = *current;
		if(consume_string(current, "+")) {
			node = new_node_binary(ND_ADD, token, node, parse_mul_div(current));
			continue;
		}
		if(consume_string(current, "-")) {
			node = new_node_binary(ND_SUB, token, node, parse_mul_div(current));
			continue;
		}
		return node;
	}
}

static Node *parse_mul_div(Token **current) {
	Node *node = parse_postfix(current);
	for(;;) {
		Token *token = *current;
		if(consume_string(current, "*")) {
			node = new_node_binary(ND_MUL, token, node, parse_postfix(current));
			continue;
		}
		if(consume_string(current, "/")) {
			node = new_node_binary(ND_DIV, token, node, parse_postfix(current));
			continue;
		}
		if(consume_string(current, "%")) {
			node = new_node_binary(ND_MOD, token, node, parse_postfix(current));
			continue;
		}
		return node;
	}
}

static Node *parse_postfix(Token **current) {
	Node *node = parse_primary(current);
	for(;;) {
		Token *token = *current;

		if(consume_string(current, "[")) {
			Node *left = new_node(ND_ARRAYMEMBER, NULL);
			left->index = parse_expr(current);
			left->body = node;
			node = left;

			expect_string(current, "]");

			continue;
		}

		if(consume_string(current, ".")) {
			Node *left = new_node(ND_MEMBER, *current);

			left->body = node;
			node = left;
			expect_type(current, TK_IDENTIFIER);

			continue;
		}

		if(consume_string(current, "(")) {

			Node *left = new_node(ND_CALL, token);
			left->args = parse_args(current);

			left->body = node;
			node = left;

			expect_string(current, ")");


			continue;
		}
		return node;
	}
}

Node *parse_primary(Token **current) {
	Token *token = *current;
	if(consume_string(current, "new")) {
		Token *name = *current;

		expect_type(current, TK_IDENTIFIER);

		if(equals_string(current, "[")) {
			Node *node = new_node(ND_NEWARRAY, name);
			expect_string(current, "[");
			node->args = parse_expr(current);
			expect_string(current, "]");
			return node;
		} else {
			if(!type_get_class(name->data)) {
				printf("error, unknown type '%s'\n", name->data);
				exit(1);
			}

			Node *node = new_node(ND_NEW, name);
			expect_string(current, "(");
			node->args = parse_args(current);
			expect_string(current, ")");
			return node;
		}
	} else if(consume_string(current, "syscall")) {
		Node *node = new_node(ND_SYSCALL, NULL);
		expect_string(current, "(");
		node->args = parse_args(current);
		expect_string(current, ")");
		return node;
	} else if(consume_string(current, "(")) {
		Node *node = parse_expr(current);
		expect_string(current, ")");
		return node;
	} else if(consume_type(current, TK_IDENTIFIER)) {

		if(!varscope_get(token->data) && !type_get_class(token->data)) {
			printf("undefined variable %s\n", token->data);
			exit(1);
		}

		Node *node = new_node(ND_VARIABLE, token);

		return node;
	} else if(consume_type(current, TK_NUMBER)) {
		Node *node = new_node(ND_NUMBER, token);
		if(strstr(token->data, ".") != NULL) {
			node->data_type = TYPE_FLOAT;
		} else {
			node->data_type = TYPE_INT;
		}
		return node;
	} else if(consume_type(current, TK_STRING)) {
		return new_node(ND_STRING, token);
	} else {
		printf("error, unexpected token '%s'\n", (*current)->data);
		exit(1);
	}
}