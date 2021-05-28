#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INFO_LEN (120)
#define isDigit(x) ('0' <= (x) && (x) <= '9')

typedef enum {
	undefined,
	quit,
	clearDB,
	get_m, get_s,
	update_m, update_s,
	insert_m, insert_s,
	del_m, del_s,
	count_m, count_s,
	ut_m, ut_s
} comnd;

typedef struct {
	comnd name;
	int id1;
	int id2;
	char strParam[INFO_LEN];
} command;

char* skipSpaces(char* str);

static comnd recognizeName(char* comStr, int n);

static int extractIntFromParamStr(char* paramStr, int* offset);

static bool extractParameters(command* cmd, char* paramStr);

bool getCommand(command* cmd);