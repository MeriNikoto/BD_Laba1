#pragma once
#include "dbOperations.h"

#define RECORD_SIZE (128)
#define MAX_STR_SIZE (RECORD_SIZE - sizeof(char) - 2 * sizeof(int))

typedef struct indexPair {
	int indexID;
	int fileRow;
} indexPair;

typedef struct Index {
	int length;
	int bufferLength;
	indexPair* index;
} Index;

static Index index = { -1, -1, NULL };

static void writeRecord(int id1, int id2, char* str, FILE* f) {
	char c = 0;
	fwrite(&c, sizeof(char), 1, f);

	fwrite(&id1, sizeof(int), 1, f);
	fwrite(&id2, sizeof(int), 1, f);

	char record[MAX_STR_SIZE];
	for (int i = 0; i < MAX_STR_SIZE; i++) record[i] = 0;

	int len = strlen(str);

	strncpy(record, str, (len < MAX_STR_SIZE) ? len : MAX_STR_SIZE);
	fwrite(record, sizeof(record), 1, f);
}

static void readRecord(char* flag, int* id1, int* id2, char* str, int strSize, FILE* f) {
	fread(flag, sizeof(char), 1, f);
	fread(id1, sizeof(int), 1, f);
	fread(id2, sizeof(int), 1, f);
	fread(str, strSize, 1, f);
}

static int countRecords(const char* fileName) {
	FILE* f = fopen(fileName, "rb");
	fseek(f, 0, SEEK_END);
	return ftell(f) / RECORD_SIZE;
}

static void pushAvailableInsertLine(const char* trashFile, int id) {
	int num;

	FILE* f = fopen(trashFile, "rb+");
	int res = fread(&num, sizeof(int), 1, f);

	while (res == 1 && num != -1) {
		res = fread(&num, sizeof(int), 1, f);
	}

	num = id;
	if (res != 1) {
		fclose(f);

		f = fopen(trashFile, "ab");
		fwrite(&num, sizeof(num), 1, f);
		fclose(f);

		return;
	}

	fseek(f, -(int)(sizeof(int)), SEEK_CUR);

	fwrite(&num, sizeof(num), 1, f);
	fclose(f);
}

static int popAvailableInsertLine(const char* trashFile) {
	int num;

	FILE* f = fopen(trashFile, "rb+");
	int res = fread(&num, sizeof(int), 1, f);

	while (res == 1 && num == -1) {
		res = fread(&num, sizeof(int), 1, f);
	}

	if (res != 1) {
		fclose(f);
		fclose(fopen(trashFile, "w"));
		return -1;
	}

	fseek(f, -(int)sizeof(int), SEEK_CUR);

	int minusOne = -1;
	fwrite(&minusOne, sizeof(int), 1, f);

	fclose(f);
	return num;
}

static int compareIndexPairs(const void* l, const void* r) {
	const indexPair* lPair = l;
	const indexPair* rPair = r;

	return lPair->indexID - rPair->indexID;
}

static void sortIndex(Index* ind) {
	qsort(ind->index, ind->length, sizeof(indexPair), compareIndexPairs);
}

static bool doubleIndexBuffer(Index* ind) {
	int nwBufferLength = 2 * ind->bufferLength + 2;

	indexPair* nwIndex = realloc(ind->index, sizeof(indexPair) * nwBufferLength);
	if (nwIndex == NULL) return false;

	ind->bufferLength = nwBufferLength;
	ind->index = nwIndex;

	return true;
}

static bool addToIndex(Index* ind, int recordId, int recordRow, const char* fileName) {
	if (ind->length == ind->bufferLength) {
		if (!doubleIndexBuffer(ind)) return false;
	}

	indexPair nwPair = { recordId, recordRow };

	ind->index[ind->length] = nwPair;
	ind->length += 1;

	sortIndex(ind);

	FILE* f = fopen(fileName, "rb+");
	fwrite(&ind->length, sizeof(int), 1, f);
	fclose(f);

	f = fopen(fileName, "ab");
	fwrite(&nwPair.indexID, sizeof(int), 1, f);
	fwrite(&nwPair.fileRow, sizeof(int), 1, f);
	fclose(f);

	return true;
}

static void writeIndexToFile(Index ind, const char* fileName) {
	FILE* f = fopen(fileName, "wb");
	fwrite(&ind.length, sizeof(int), 1, f);

	for (int i = 0; i < ind.length; i++) {
		fwrite(&ind.index[i].indexID, sizeof(int), 1, f);
		fwrite(&ind.index[i].fileRow, sizeof(int), 1, f);
	}

	fclose(f);
}

static bool loadIndexFromFile(Index* ind, const char* fileName) {
	FILE* f = fopen(fileName, "rb");

	if (ind->index != NULL) {
		free(ind->index);
	}

	int readed = fread(&ind->length, sizeof(int), 1, f);
	if (ind->length < 0 || readed == 0) {
		ind->length = -1;

		fclose(f);
		return false;
	}

	ind->bufferLength = 2 * ind->length + 2;
	ind->index = (indexPair*)malloc(sizeof(indexPair) * ind->bufferLength);


	if (!ind->index) {
		printf("Can't allocate memory for index array\n");

		fclose(f);
		return false;
	}

	for (int i = 0; i < ind->length; i++) {
		ind->index[i] = (indexPair){ -1, -1 };

		fread(&(ind->index[i].indexID), sizeof(int), 1, f);
		fread(&(ind->index[i].fileRow), sizeof(int), 1, f);
	}

	fclose(f);
	return true;
}

static void createEmptyIndex(Index* ind, const char* fileName) {
	if (ind->index != NULL) {
		free(ind->index);
	}

	ind->length = 0;
	ind->bufferLength = 2;
	ind->index = (indexPair*)malloc(sizeof(indexPair) * ind->bufferLength);

	FILE* f = fopen(fileName, "wb");
	fwrite(&ind->length, sizeof(int), 1, f);
	fclose(f);
}

static void initiateIndex(Index* ind, const char* fileName) {
	if (ind->index == NULL) {
		if (!loadIndexFromFile(ind, fileName)) {
			createEmptyIndex(ind, fileName);
		}
	}

	sortIndex(ind);
}

static indexPair* findIndexPair(Index* ind, int id) {
	indexPair key = { id, -1 };
	indexPair* tar = bsearch(&key, ind->index, ind->length, sizeof(indexPair), compareIndexPairs);

	if (tar == NULL) {
		printf("Can't find hostel with such id.\n");
		return NULL;
	}

	return tar;
}

static void deleteFromIndex(Index* ind, const char* fileName, indexPair* element) {
	int elementN = element - ind->index;
	for (int i = elementN; i < ind->length - 1; i++) {
		ind->index[i] = ind->index[i + 1];
	}

	ind->length -= 1;
	writeIndexToFile(*ind, fileName);
}

static int findFirstChildRowN(Index* ind, const char* fileName, int id) {
	indexPair* tar = findIndexPair(ind, id);
	if (tar == NULL) return -2;

	int firstChildRowN = -1;
	FILE* f = fopen(fileName, "rb");
	fseek(f, RECORD_SIZE * tar->fileRow + sizeof(char) + sizeof(int), SEEK_SET);
	fread(&firstChildRowN, sizeof(int), 1, f);
	fclose(f);

	return firstChildRowN;
}

#pragma region get_m

bool verifyGet_mArgs(command* cmd) {
	bool wrong = false;

	wrong = wrong || cmd->id1 < -1;
	wrong = wrong || cmd->id2 != -1;
	wrong = wrong || strcmp("", cmd->strParam) != 0;

	if (wrong) {
		printf("Argument error: get_m have 1 or 0 id`s argument.\n");
	}

	return !wrong;
}

void handle_get_m_all() {
	if (index.length == 0) {
		printf("Table is empty.\n");
		return;
	}

	FILE* f = fopen("hostels.data", "rb");

	char flag;
	int id, workerId;
	char hostelsStr[MAX_STR_SIZE];

	bool shown = false;

	int curPos = 0;
	for (int i = 0; i < index.length; i++) {
		int nextPos = index.index[i].fileRow;
		int deltaPos = nextPos - curPos;

		fseek(f, RECORD_SIZE * deltaPos, SEEK_CUR);
		readRecord(&flag, &id, &workerId, hostelsStr, sizeof(hostelsStr), f);

		if (flag == 0) {
			if (!shown) {
				printf("ID\tClient hostel\n");
				shown = true;
			}
			printf("%d\t%s\n", id, hostelsStr);
		}

		curPos = nextPos + 1;
	}

	if (!shown) {
		printf("Table is empty.\n");
	}

	fclose(f);
}

void handle_get_m_id(int target) {
	indexPair key = { target, -1 };

	indexPair* tarPair = bsearch(&key, index.index, index.length, sizeof(indexPair), compareIndexPairs);
	if (tarPair == NULL) {
		printf("Record with such id doen`t exist.\n");
		return;
	}

	FILE* f = fopen("hostels.data", "rb");

	char flag;
	int id, songId;
	char hostelsStr[MAX_STR_SIZE];

	fseek(f, RECORD_SIZE * tarPair->fileRow, SEEK_CUR);
	readRecord(&flag, &id, &songId, hostelsStr, sizeof(hostelsStr), f);

	fclose(f);

	if (flag != 0) {
		printf("Record with such id doen`t exist.\n");
		return;
	}

	printf("%d\t%s\n", id, hostelsStr);
}

void handle_get_m(command* cmd) {
	if (!verifyGet_mArgs(cmd)) return;

	initiateIndex(&index, "hostels.ind");

	if (cmd->id1 == -1) {
		handle_get_m_all();
	}
	else {
		handle_get_m_id(cmd->id1);
	}
}

#pragma endregion

#pragma region get_s

bool verifyGet_sArgs(command* cmd) {
	bool wrong = false;

	wrong = wrong || cmd->id1 < 0;
	wrong = wrong || strcmp("", cmd->strParam) != 0;

	if (wrong) {
		printf("Argument error: get_s has 1 or 2 id arguments.\n");
	}

	return !wrong;
}

int readFirstChildRowN(int fileRow) {
	FILE* f = fopen("hostels.data", "rb");

	int firstChildRowN = -1;
	fseek(f, RECORD_SIZE * fileRow + sizeof(char) + sizeof(int), SEEK_SET);
	fread(&firstChildRowN, sizeof(int), 1, f);

	fclose(f);
	return firstChildRowN;
}

void showAllChildren(int firstRow, int specifiedChild) {
	FILE* f = fopen("clients.data", "rb");
	bool printed = false;

	char flag;
	int childId, nextChildRowN;
	char clientstr[MAX_STR_SIZE];

	int curRow = firstRow;
	while (curRow >= 0) {
		fseek(f, RECORD_SIZE * curRow, SEEK_SET);

		readRecord(&flag, &childId, &nextChildRowN, clientstr, sizeof(clientstr), f);
		curRow = nextChildRowN;

		if (specifiedChild >= 0 && childId != specifiedChild) continue;

		if (!printed) {
			printf("ID\Client Name\n");
		}

		printed = true;
		printf("%d\t%s\n", childId, clientstr);
	}

	if (!printed && specifiedChild >= 0) {
		printf("Can't find child with such id.\n");
	}

	fclose(f);
	return;
}

void handle_get_s(command* cmd) {
	if (!verifyGet_sArgs(cmd)) return;
	initiateIndex(&index, "hostels.ind");

	indexPair* tar = findIndexPair(&index, cmd->id1);
	if (tar == NULL) return;

	int firstChildRowN = readFirstChildRowN(tar->fileRow);
	if (firstChildRowN == -1) {
		printf("Record hasn't any children.\n");
		return;
	}

	showAllChildren(firstChildRowN, cmd->id2);
}

#pragma endregion

#pragma region update_m

bool verifyUpdate_m(command* cmd) {
	bool wrong = false;

	wrong = wrong || cmd->id1 < 0;
	wrong = wrong || cmd->id2 != -1;
	wrong = wrong || strcmp("", cmd->strParam) == 0;

	if (wrong) {
		printf("Argument error: update_m must have id and new hostel name.\n");
	}

	return !wrong;
}

void handle_update_m(command* cmd) {
	if (!verifyUpdate_m(cmd)) return;

	initiateIndex(&index, "hostels.ind");

	indexPair key = { cmd->id1, -1 };
	indexPair* tar = bsearch(&key, index.index, index.length, sizeof(indexPair), compareIndexPairs);

	if (tar == NULL) {
		printf("Can't find record with such id.\n");
		return;
	}

	char nwMsg[MAX_STR_SIZE];
	for (int i = 0; i < MAX_STR_SIZE; i++) nwMsg[i] = 0;

	int len = strlen(cmd->strParam);
	strncpy(nwMsg, cmd->strParam, (len < MAX_STR_SIZE) ? len : MAX_STR_SIZE);

	FILE* f = fopen("hostels.data", "rb+");

	fseek(f, RECORD_SIZE * tar->fileRow + sizeof(char) + 2 * sizeof(int), SEEK_CUR);
	fwrite(nwMsg, sizeof(nwMsg), 1, f);

	fclose(f);
}

#pragma endregion

#pragma region update_s

bool validateUpdate_sArgs(command* cmd) {
	bool wrong = false;

	wrong = wrong || cmd->id1 < 0;
	wrong = wrong || cmd->id2 < 0;
	wrong = wrong || strcmp("", cmd->strParam) == 0;

	if (wrong) {
		printf("Arguments error: update_s must have 2 ids and new client name.\n");
	}

	return !wrong;
}

bool seekToChild(FILE* f, int id, int startRow) {
	int curRow = startRow;

	while (curRow >= 0) {
		int childId, nextRowN;
		fseek(f, RECORD_SIZE * curRow + sizeof(char), SEEK_SET);

		fread(&childId, sizeof(int), 1, f);
		fread(&nextRowN, sizeof(int), 1, f);

		if (childId == id) {
			fseek(f, -((int)(sizeof(char) + 2 * sizeof(int))), SEEK_CUR);
			return true;
		}

		curRow = nextRowN;
	}

	return false;
}

void handle_update_s(command* cmd) {
	if (!validateUpdate_sArgs(cmd)) return;
	initiateIndex(&index, "hostels.ind");

	int firstChildRowN = findFirstChildRowN(&index, "hostels.data", cmd->id1);
	if (firstChildRowN < -1) return;

	if (firstChildRowN == -1) {
		printf("Can't find child with such id.\n");
		return;
	}

	FILE* f = fopen("clients.data", "rb+");
	if (!seekToChild(f, cmd->id2, firstChildRowN)) {
		fclose(f);
		printf("Can't find child with such id.\n");
		return;
	}

	char nwStr[MAX_STR_SIZE];
	for (int i = 0; i < sizeof(nwStr); i++) nwStr[i] = 0;
	int len = strlen(cmd->strParam);
	strncpy(nwStr, cmd->strParam, (len < MAX_STR_SIZE) ? len : MAX_STR_SIZE);

	fseek(f, sizeof(char) + 2 * sizeof(int), SEEK_CUR);
	fwrite(nwStr, sizeof(nwStr), 1, f);
	fclose(f);
}

#pragma endregion

#pragma region insert_m

bool verifyInsert_mArgs(command* cmd) {
	bool wrong = false;

	wrong = wrong || cmd->id1 != -1;
	wrong = wrong || cmd->id2 != -1;
	wrong = wrong || strcmp("", cmd->strParam) == 0;

	if (wrong) {
		printf("Argument error: insert_m gets string as input\n");
	}

	return !wrong;
}

int findNextID() {
	if (index.length == 0) {
		return 0;
	}

	int maxId = index.index[0].indexID;
	for (int i = 0; i < index.length; i++) {
		maxId = (maxId < index.index[i].indexID) ? index.index[i].indexID : maxId;
	}

	return maxId + 1;
}

void handle_insert_m(command* cmd) {
	if (!verifyInsert_mArgs(cmd)) return;

	initiateIndex(&index, "hostels.ind");

	int availableAddress = popAvailableInsertLine("hostels.trash");
	availableAddress = (availableAddress == -1) ? index.length : availableAddress;

	FILE* f = fopen("hostels.data", "rb+");
	fseek(f, availableAddress * RECORD_SIZE, SEEK_CUR);

	int recordID = findNextID();
	writeRecord(recordID, -1, cmd->strParam, f);
	fclose(f);

	addToIndex(&index, recordID, availableAddress, "hostels.ind");
}

#pragma endregion

#pragma region insert_s

bool verifyInsert_sArgs(command* cmd) {
	bool wrong = false;

	wrong = wrong || cmd->id1 < 0;
	wrong = wrong || cmd->id2 != -1;
	wrong = wrong || strcmp("", cmd->strParam) == 0;

	if (wrong) {
		printf("argError: insert_s gets id and string\n");
	}

	return !wrong;
}

int changeMastersChildPointer(int mRow, int nwChildLine) {
	FILE* f = fopen("hostels.data", "rb+");
	fseek(f, RECORD_SIZE * mRow + sizeof(char) + sizeof(int), SEEK_CUR);

	int lastChildRowN = -1;
	fread(&lastChildRowN, sizeof(int), 1, f);
	fseek(f, -((int)(sizeof(int))), SEEK_CUR);

	fwrite(&nwChildLine, sizeof(int), 1, f);
	fclose(f);

	return lastChildRowN;
}

void addChildToChildList(int lastChildRowN, int availableLine, const char* str) {
	FILE* f = fopen("clients.data", "rb+");
	int prevChildID = -1;
	if (lastChildRowN != -1) {
		fseek(f, RECORD_SIZE * lastChildRowN + sizeof(char), SEEK_CUR);
		fread(&prevChildID, sizeof(int), 1, f);
	}

	fseek(f, RECORD_SIZE * availableLine, SEEK_SET);
	writeRecord(prevChildID + 1, lastChildRowN, str, f);
	fclose(f);
}

void handle_insert_s(command* cmd) {
	if (!verifyInsert_sArgs(cmd)) return;
	initiateIndex(&index, "hostels.ind");

	indexPair* tar = findIndexPair(&index, cmd->id1);
	if (tar == NULL) return;

	int availableLine = popAvailableInsertLine("clients.trash");
	availableLine = (availableLine == -1) ? countRecords("clients.data") : availableLine;

	int lastChildRowN = changeMastersChildPointer(tar->fileRow, availableLine);
	addChildToChildList(lastChildRowN, availableLine, cmd->strParam);
}

#pragma endregion

#pragma region del_m

bool verifyDel_mArgs(command* cmd) {
	bool wrong = false;

	wrong = wrong || cmd->id1 < 0;
	wrong = wrong || cmd->id2 != -1;
	wrong = wrong || strcmp("", cmd->strParam) != 0;

	if (wrong) {
		printf("argsError: del_m gets 1 numger.\n");
	}

	return !wrong;
}

void deleteAllChildren(int masterId, int firstChildRowN) {
	indexPair* tar = findIndexPair(&index, masterId);
	if (tar == NULL) return;

	int nwFirstChild = -1;
	FILE* f = fopen("hostels.data", "rb+");
	fseek(f, RECORD_SIZE * tar->fileRow + sizeof(char) + sizeof(int), SEEK_SET);
	fwrite(&nwFirstChild, sizeof(int), 1, f);
	fclose(f);

	char delFlag = 1;

	f = fopen("clients.data", "rb+");
	int curRow = firstChildRowN;
	while (curRow >= 0) {
		fseek(f, RECORD_SIZE * curRow, SEEK_SET);

		fwrite(&delFlag, sizeof(char), 1, f);
		pushAvailableInsertLine("clients.trash", curRow);

		fseek(f, sizeof(int), SEEK_CUR);
		fread(&curRow, sizeof(int), 1, f);
	}
}

void handle_del_m(command* cmd) {
	if (!verifyDel_mArgs(cmd)) return;
	initiateIndex(&index, "hostels.ind");

	int childRow = findFirstChildRowN(&index, "hostels.data", cmd->id1);
	if (childRow < -1) return;

	if (childRow >= 0) {
		deleteAllChildren(cmd->id1, childRow);
	}

	char delFlag = 1;
	indexPair* tar = findIndexPair(&index, cmd->id1);

	FILE* f = fopen("hostels.data", "rb+");
	fseek(f, RECORD_SIZE * tar->fileRow, SEEK_SET);
	fwrite(&delFlag, sizeof(char), 1, f);
	fclose(f);

	pushAvailableInsertLine("hostels.trash", tar->fileRow);
	deleteFromIndex(&index, "hostels.ind", tar);
}

#pragma endregion

#pragma region del_s

bool verifyDel_sArgs(command* cmd) {
	bool wrong = false;

	wrong = wrong || cmd->id1 < 0;
	wrong = wrong || cmd->id2 < -1;
	wrong = wrong || strcmp("", cmd->strParam) != 0;

	if (wrong) {
		printf("argsError: get_s gets 1 or 2 indexes.\n");
	}

	return !wrong;
}

void deleteChildAt(FILE* f, int masterId, int prevRow, int tarRow, int nextRow) {
	pushAvailableInsertLine("clients.trash", tarRow);
	fseek(f, RECORD_SIZE * tarRow, SEEK_SET);

	char delFlag = 1;
	fwrite(&delFlag, sizeof(char), 1, f);

	if (prevRow >= 0) {
		fseek(f, RECORD_SIZE * prevRow + sizeof(char) + sizeof(int), SEEK_SET);
		fwrite(&nextRow, sizeof(int), 1, f);
		return;
	}

	indexPair* tar = findIndexPair(&index, masterId);
	FILE* m = fopen("hostels.data", "rb+");

	fseek(m, RECORD_SIZE * tar->fileRow + sizeof(char) + sizeof(int), SEEK_SET);
	fwrite(&nextRow, sizeof(int), 1, m);

	fclose(m);
}

bool deleteChild(int masterId, int firstChildRowN, int specifiedId) {
	FILE* f = fopen("clients.data", "rb+");

	int childId, nextChildRow;

	int prevRow = -1;
	int curRow = firstChildRowN;
	while (curRow != -1) {
		fseek(f, RECORD_SIZE * curRow + sizeof(char), SEEK_SET);
		fread(&childId, sizeof(int), 1, f);
		fread(&nextChildRow, sizeof(int), 1, f);

		if (childId == specifiedId) {
			deleteChildAt(f, masterId, prevRow, curRow, nextChildRow);

			fclose(f);
			return true;
		}

		prevRow = curRow;
		curRow = nextChildRow;
	}

	fclose(f);
	return false;
}

void handle_del_s(command* cmd) {
	if (!verifyDel_sArgs(cmd)) return;
	initiateIndex(&index, "hostels.ind");

	int firstRow = findFirstChildRowN(&index, "hostels.data", cmd->id1);
	if (firstRow < -1) return;

	if (firstRow == -1) {
		if (cmd->id2 >= 0) {
			printf("Can't find child with such index.\n");
		}

		return;
	}

	if (cmd->id2 >= 0) {
		if (!deleteChild(cmd->id1, firstRow, cmd->id2)) {
			printf("Can't find child with such index.\n");
		}
	}
	else {
		deleteAllChildren(cmd->id1, firstRow);
	}
}

#pragma endregion

#pragma region count_m

bool verityCount_mArgs(command* cmd) {
	bool wrong = false;

	wrong = wrong || cmd->id1 != -1;
	wrong = wrong || cmd->id2 != -1;
	wrong = wrong || strcmp("", cmd->strParam) != 0;

	if (wrong) {
		printf("argError: count_m doesn't get any argument.\n");
	}

	return !wrong;
}

void handle_count_m(command* cmd) {
	if (!verityCount_mArgs(cmd)) return;

	initiateIndex(&index, "hostels.ind");
	printf("%d\n", index.length);
}

#pragma endregion

#pragma region count_s

bool validateCount_sArgs(command* cmd) {
	bool wrong = false;

	wrong = wrong || cmd->id1 < 0;
	wrong = wrong || cmd->id2 != -1;
	wrong = wrong || strcmp("", cmd->strParam) != 0;

	if (wrong) {
		printf("argError: count_s gets 1 numeric parameter.\n");
	}

	return !wrong;
}

int countChildren(int firstChildRowN) {
	FILE* f = fopen("clients.data", "rb");

	int nxtRow;
	int counter = 0;
	int curRow = firstChildRowN;

	while (curRow >= 0) {
		fseek(f, RECORD_SIZE * curRow + sizeof(char) + sizeof(int), SEEK_SET);
		fread(&nxtRow, sizeof(int), 1, f);

		++counter;
		curRow = nxtRow;
	}

	return counter;
}

void handle_count_s(command* cmd) {
	if (!validateCount_sArgs(cmd)) return;
	initiateIndex(&index, "hostels.ind");

	int firstChildRowN = findFirstChildRowN(&index, "hostels.data", cmd->id1);
	if (firstChildRowN < -1) return;

	int numOfChildren = (firstChildRowN == -1) ? 0 : countChildren(firstChildRowN);
	printf("%d\n", numOfChildren);
}

#pragma endregion
