#pragma once
#include "json_utils.h"

void syncManagerMenu();
void syncNow();
bool sync_add_operation(const std::string& op, const JsonValue& data);
