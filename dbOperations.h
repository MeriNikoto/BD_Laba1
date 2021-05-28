#pragma once

#include "commandParser.h"

void handle_get_m(command *cmd);
void handle_get_s(command *cmd);

void handle_update_m(command *cmd);
void handle_update_s(command *cmd);

void handle_insert_m(command *cmd);
void handle_insert_s(command *cmd);

void handle_del_m(command *cmd);
void handle_del_s(command *cmd);

void handle_count_m(command *cmd);
void handle_count_s(command *cmd);

void handle_ut_s(command* cmd);

