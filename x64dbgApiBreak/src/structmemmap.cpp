#include <structmemmap.h>

#define MAX_TYPENAME_SIZE				128

#define ALLOCOBJECTLIST(type,count)		RESIZEOBJECTLIST(type,NULL,count)

#define SIZEOF_INT						sizeof(int)
#define SIZEOF_POINTER					sizeof(void *)
#define SIZEOF_CHAR						1
#define SIZEOF_WCHAR					2
#define SIZEOF_BYTE						1
#define SIZEOF_SHORT					2
#define SIZEOF_LONG						4
#define SIZEOF_LONGLONG					8

#define SIZEOF_WORD						SIZEOF_SHORT
#define SIZEOF_DWORD					SIZEOF_LONG
#define SIZEOF_QWORD					SIZEOF_LONGLONG


#define FAILBASE_SYSTEM					0
#define FAILBASE_TYPESTRUCT				4
#define FAILBASE_FIELD_DEF				FAILBASE_TYPESTRUCT + 5
#define FAILBASE_SYNTAX					FAILBASE_FIELD_DEF

#define FS_MEMERR						FAILBASE_SYSTEM + 1

#define FS_MAPTYPE						FAILBASE_TYPESTRUCT + 1
#define FS_TYPENAME						FAILBASE_TYPESTRUCT + 2
#define FS_BRACKOPEN					FAILBASE_TYPESTRUCT + 3
#define FS_EXPECTFIELD					FAILBASE_TYPESTRUCT + 4
#define FS_BRACKCLOSE					FAILBASE_TYPESTRUCT + 5

#define FS_NOFIELDTYPE					FAILBASE_FIELD_DEF + 1
#define FS_EXPECTFIELDNAME				FAILBASE_FIELD_DEF + 2
#define FS_EXPECTSQUBRCKOPEN			FAILBASE_FIELD_DEF + 3
#define FS_EXPECTSQUBRCKCLOSE			FAILBASE_FIELD_DEF + 4
#define FS_EXPECTARRSIZE				FAILBASE_FIELD_DEF + 5
#define FS_EXPECTSEMICOLON				FAILBASE_FIELD_DEF + 6

#define FS_EXPECTINCLFILEPATH			FAILBASE_SYNTAX + 1

//LINKED LIST

#define RECORD_OF(node, type)	( (type)((node)->value) )

typedef struct __SLISTNODE
{
	struct __SLISTNODE *next;
	void *				value;
}*PSLISTNODE, SLISTNODE;

typedef struct
{
	PSLISTNODE			head;
	PSLISTNODE			tail;
	LONG				count;
}*PSLIST, SLIST;

//TYPED MEMORY MAP

typedef void(*CONVERTER_METHOD)(char *, void *);

typedef struct
{
	WORD				cbSize;
	char				typeName[MAX_TYPENAME_SIZE];
}*PGENERIC_DATATYPE_INFO;

typedef struct
{
	WORD				cbSize;
	char				typeName[MAX_TYPENAME_SIZE];
	WORD				size;
	BOOL				isSigned;
	WORD				linkIndex;
	CONVERTER_METHOD	method;
}*PPRIMITIVETYPEINFO, PRIMITIVETYPEINFO;


#define ListOf(x) PSLIST

typedef struct
{
	WORD					cbSize;
	char					name[MAX_TYPENAME_SIZE];
	DWORD					structSize;
	ListOf(PTYPEINFOFIELD)	fields;
}*PSTRUCTINFO, STRUCTINFO;

typedef struct
{

	char				fieldName[128];
	WORD				arrSize;
	BOOL				isArray;
	BOOL				isPointer;
	BOOL				isStruct;

	union
	{
		PPRIMITIVETYPEINFO	primitiveType;
		PSTRUCTINFO			structType;
	}type;

}*PTYPEINFOFIELD, TYPEINFOFIELD;

typedef struct
{
	char					name[64];
	BOOL					isStructure;
	BOOL					isPointer;
	BOOL					isOutArg;
	union
	{
		PVOID				holder;
		PSTRUCTINFO			structInfo;
		PPRIMITIVETYPEINFO	primitiveInfo;
	}typeInfo;
}*PARGINFO, ARGINFO;

typedef struct __FNSIGN
{
	char					name[256];
	char					module[256];
	PARGINFO				args;
	SHORT					argCount;
	BOOL					hasOutArg;
}*PFNSIGN, FNSIGN;

typedef struct
{
	const char *			identName;
	PPRIMITIVETYPEINFO		pti;
}typeidents;



static typeidents TYPE_IDENTS[] =
{
	{ "int",NULL },
	{ "uint",NULL },
	{ "char",NULL },
	{ "wchar",NULL },
	{ "byte",NULL },
	{ "ubyte",NULL },
	{ "short",NULL },
	{ "ushort",NULL },
	{ "long", NULL },
	{ "ulong",NULL },
	{ "long64",NULL },
	{ "ulong64",NULL },
	{ "string",NULL },
	{ "wstring",NULL },
	{ "pointer",NULL }
};

static struct {
	const char *	alias;
	int				index;
}TYPE_ALIASES[] =
{
	{ "BYTE",4 },
	{ "WORD",7 },
	{ "DWORD",9 },
	{ "QWORD",11 }
};

#define MAX_TYPE_IDENTS				sizeof(TYPE_IDENTS) / sizeof(typeidents)
#define MAX_USABLE_TYPE_IDENTS		(MAX_TYPE_IDENTS - 3)

WCHAR SmmpScriptWorkDir[MAX_PATH] = { 0 };

const char *FAIL_MESSAGES[] =
{
	"Memory alloc error",
	"",
	"",
	"",
	/////////////
	"maptype keyword expected",
	"expected 'typename'",
	"expected '{'",
	"maptype body definition expected",
	"expected '}'",
	/////////////////////
	"field type is not valid",
	"expected 'field name'",
	"expected '['",
	"expected ']",
	"expected array size",
	"expected ';' to complete field definition",
	///////////////
	"filepath expected for @incl."
};

void SmmpRaiseParseError(LONG err)
{
	const LONG totalErrString = sizeof(FAIL_MESSAGES) / sizeof(char *);

	if (err >= 0 && err <= totalErrString)
	{
		DBGPRINT2(FAIL_MESSAGES[err - 1]);
	}
}

BOOL SmmpCreateSLIST(PSLIST *list)
{
	*list = ALLOCOBJECT(SLIST);

	if (*list != NULL)
	{
		RtlZeroMemory(*list, sizeof(SLIST));
		return TRUE;
	}

	return FALSE;
}

BOOL SmmpAddSList(PSLIST list, void *value)
{
	PSLISTNODE node;

	node = ALLOCOBJECT(SLISTNODE);

	if (!node)
		return FALSE;

	node->value = value;
	node->next = NULL;

	if (list->count == 0)
	{
		list->head = list->tail = node;
		list->count = 1;
		return TRUE;
	}

	list->tail->next = node;
	list->tail = node;
	list->count++;

	return TRUE;
}

void SmmpDestroySList(PSLIST list)
{
	PSLISTNODE tmp;

	while (list->head != NULL)
	{
		tmp = list->head->next;
		FREEOBJECT(list->head);
		list->head = tmp;
	}

	FREEOBJECT(list);
}

BOOL SmmCreateMapTypeInfo(const char *name, WORD size, BOOL isSigned, CONVERTER_METHOD method, WORD *index)
{
	PPRIMITIVETYPEINFO pmti = NULL;

	if (strcmp(TYPE_IDENTS[*index].identName, name))
		return FALSE;

	pmti = ALLOCOBJECT(PRIMITIVETYPEINFO);

	if (!pmti)
		return FALSE;

	pmti->cbSize = sizeof(PRIMITIVETYPEINFO);
	strcpy(pmti->typeName, name);
	pmti->size = size;
	pmti->isSigned = isSigned;

	TYPE_IDENTS[*index].pti = pmti;

	pmti->linkIndex = *index;
	pmti->method = method;

	(*index)++;

	SmmpAddSList(SmmpTypeList, pmti);

	return TRUE;
}

#include <stdio.h>

#define IMPLEMENT_CONVERTER(name, type,format) void Convert ## name  (char *buf, void *mem) { \
												sprintf(buf,format,*((type *)mem)); \
											} \



IMPLEMENT_CONVERTER(Int, int, "%d")
IMPLEMENT_CONVERTER(Uint, unsigned int, "%du")
IMPLEMENT_CONVERTER(Char, CHAR, "%c")
IMPLEMENT_CONVERTER(Wchar, WCHAR, "%c")
IMPLEMENT_CONVERTER(Byte, CHAR, "%d")
IMPLEMENT_CONVERTER(UByte, UCHAR, "%du")
IMPLEMENT_CONVERTER(Short, SHORT, "%d")
IMPLEMENT_CONVERTER(UShort, USHORT, "%du")
IMPLEMENT_CONVERTER(Long, LONG, "%ld")
IMPLEMENT_CONVERTER(ULong, ULONG, "%lu")
IMPLEMENT_CONVERTER(Long64, LONGLONG, "%lld")
IMPLEMENT_CONVERTER(ULong64, ULONGLONG, "%llu")
IMPLEMENT_CONVERTER(Pointer, LPVOID, "%p")


void ConvertString(char *buf, void *mem)
{
	sprintf(buf, "\"%s\"", ((LPSTR)mem));
}

void ConvertStringW(char *buf, void *mem)
{
	ULONG slen;
	wchar_t exBuf[0x1000];

	slen = wsprintf(exBuf, L"L\"%s\"", (LPWSTR)mem);

	if (!slen)
	{
		*buf = '\0';
		return;
	}

	if (!buf)
		return;

	WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, exBuf, slen, buf, slen, NULL, NULL);
}

PSLIST SmmpUserTypeList = NULL, SmmpFnSignList = NULL;
PSLIST SmmpTypeList = NULL;

BOOL SmmpGetTypeInfoByName(const char *name, PVOID *info, BOOL *isPrimitive)
{
	BOOL detectIsPrimitiveOnly = info == NULL;

	//First, lookup trough the primitives
	for (DWORD nx = 0; nx < MAX_TYPE_IDENTS; nx++)
	{
		if (!strcmp(TYPE_IDENTS[nx].identName, name))
		{
			if (!detectIsPrimitiveOnly)
				*info = TYPE_IDENTS[nx].pti;
			
			*isPrimitive = TRUE;

			return TRUE;
		}
	}

	if (detectIsPrimitiveOnly)
	{
		*isPrimitive = FALSE;
		return TRUE;
	}

	//If its not a primitive then scan the type list
	for (PSLISTNODE node = SmmpUserTypeList->head; node != NULL; node = node->next)
	{
		if (!strcmp(RECORD_OF(node, PSTRUCTINFO)->name, name))
		{
			*info = RECORD_OF(node, PSTRUCTINFO);
			*isPrimitive = FALSE;
			return TRUE;
		}
	}

	return FALSE;
}

BOOL SmmpInitBasePrimitiveTypes()
{
	LONG success = 0;
	WORD index = 0;

	if (TYPE_IDENTS[0].pti != NULL)
		return TRUE;

	success += (LONG)SmmCreateMapTypeInfo("int", SIZEOF_INT, FALSE, ConvertInt, &index);
	success += (LONG)SmmCreateMapTypeInfo("uint", SIZEOF_INT, TRUE, ConvertUint, &index);

	success += (LONG)SmmCreateMapTypeInfo("char", SIZEOF_CHAR, FALSE, ConvertChar, &index);

	success += (LONG)SmmCreateMapTypeInfo("wchar", SIZEOF_WCHAR, FALSE, ConvertWchar, &index);

	success += (LONG)SmmCreateMapTypeInfo("byte", SIZEOF_BYTE, FALSE, ConvertByte, &index);
	success += (LONG)SmmCreateMapTypeInfo("ubyte", SIZEOF_BYTE, TRUE, ConvertUByte, &index);

	success += (LONG)SmmCreateMapTypeInfo("short", SIZEOF_SHORT, FALSE, ConvertShort, &index);
	success += (LONG)SmmCreateMapTypeInfo("ushort", SIZEOF_SHORT, FALSE, ConvertUShort, &index);

	success += (LONG)SmmCreateMapTypeInfo("long", SIZEOF_LONG, FALSE, ConvertLong, &index);
	success += (LONG)SmmCreateMapTypeInfo("ulong", SIZEOF_LONG, FALSE, ConvertULong, &index);

	success += (LONG)SmmCreateMapTypeInfo("long64", SIZEOF_LONGLONG, FALSE, ConvertLong64, &index);
	success += (LONG)SmmCreateMapTypeInfo("ulong64", SIZEOF_LONGLONG, FALSE, ConvertULong64, &index);

	success += (LONG)SmmCreateMapTypeInfo("string", SIZEOF_INT, FALSE, ConvertString, &index);
	success += (LONG)SmmCreateMapTypeInfo("wstring", SIZEOF_INT, FALSE, ConvertStringW, &index);

	success += (LONG)SmmCreateMapTypeInfo("pointer", SIZEOF_INT, FALSE, ConvertPointer, &index);


	if (success != index)
	{
		return FALSE;
	}

	return TRUE;
}

//maptype NAME {
//typename fieldname1;
//typename fieldname2;
//typename[2] fieldname3;
//typename * fieldname4;
//}




__forceinline BOOL SmmpNextnode(PSLISTNODE *node)
{
	if ((*node)->next == NULL)
		return FALSE;

	(*node) = (*node)->next;
	return TRUE;
}

#define Stringof(node) ( RECORD_OF((node), char *) )

#define OneWayExit(fs, exitLabel)  { failStep = fs; goto exitLabel; }

#define IsCurlyBrckOpen(node) (*Stringof(node) == '{')
#define IsCurlyBrckClose(node) (*Stringof(node) == '}')

#define IsSquareBrckOpen(node) (*Stringof(node) == '[')
#define IsSquareBrckClose(node) (*Stringof(node) == ']')

#define IsPtrChar(node) (*Stringof(node) == '*')

#define IsSemiColon(node) (*Stringof(node) == ';')


BOOL SmmpIsSpecialToken(char chr)
{
	switch (chr)
	{
	case '(':
	case ')':
	case '[':
	case ']':
	case '{':
	case '}':
	case '*':
	case ';':
	case ',':
	case '!':
		return TRUE;
	}

	return FALSE;
}

BOOL SmmpIsNumeric(char c)
{
	return c >= '0' && c <= '9';
}

BOOL SmmpIsWhitespace(char c)
{
	switch (c)
	{
	case '\r':
	case '\n':
	case '\t':
	case ' ':
		return TRUE;
	}

	return FALSE;
}

BOOL SmmpIsChar(char c)
{
	return !SmmpIsWhitespace(c) && !SmmpIsNumeric(c) && !SmmpIsSpecialToken(c);
}

#define SmmpPushToken() { \
							tok = ALLOCSTRINGA(bufi); \
							strcpy(tok, buf); \
							memset(buf, 0, sizeof(buf)); \
							bufi = 0; \
							SmmpAddSList(tokList, tok); \
						}

BOOL SmmpTokenize(LPCSTR typeDefString, PSLIST *tokenList)
{
	char buf[512] = { 0 };
	const char *p = typeDefString;
	int bufi = 0;
	char *tok;
	BOOL numLoad = FALSE;

	PSLIST tokList;

	if (!SmmpCreateSLIST(&tokList))
		return FALSE;


	while (*p)
	{
		if (SmmpIsSpecialToken(*p))
		{
			if (bufi > 0)
				SmmpPushToken();

			tok = ALLOCSTRINGA(4);
			*tok = *p;
			*(tok + 1) = 0;

			SmmpAddSList(tokList, tok);
		}
		else if (SmmpIsWhitespace(*p))
		{
			if (bufi > 0)
				SmmpPushToken();
		}
		else
		{
			buf[bufi++] = *p;
		}

		p++;
	}

	if (bufi > 0)
		SmmpPushToken();

	*tokenList = tokList;

	return TRUE;
}

BOOL SmmpParseTypeField(PSLISTNODE *beginNode, PTYPEINFOFIELD *fieldPtr)
{
	PSLISTNODE node = NULL;
	PPRIMITIVETYPEINFO pmti = NULL;
	PSTRUCTINFO pstr = NULL;

	PTYPEINFOFIELD field = NULL;

	BOOL success = FALSE;
	LONG failStep = 0;

	node = *beginNode;

	PVOID info;
	BOOL isPrimitive;

	if (!SmmpGetTypeInfoByName(Stringof(node), &info, &isPrimitive))
		OneWayExit(FS_NOFIELDTYPE, exit);

	field = ALLOCOBJECT(TYPEINFOFIELD);
	memset(field, 0, sizeof(TYPEINFOFIELD));


	if (!field)
		OneWayExit(FS_MEMERR, exit);

	if (isPrimitive)
		field->type.primitiveType = (PPRIMITIVETYPEINFO)info;
	else
	{
		field->type.structType = (PSTRUCTINFO)info;
		field->isStruct = TRUE;
	}

	if (!SmmpNextnode(&node))
		OneWayExit(FS_EXPECTFIELDNAME, exit);

	if (IsSquareBrckOpen(node))
	{
		if (!SmmpNextnode(&node))
			OneWayExit(FS_EXPECTARRSIZE, exit);

		if (!IsSquareBrckClose(node))
		{
			field->arrSize = atoi(Stringof(node));

			if (field->arrSize == 0)
				OneWayExit(FS_EXPECTARRSIZE, exit);

			if (!SmmpNextnode(&node))
				OneWayExit(FS_EXPECTSQUBRCKCLOSE, exit);

			field->isArray = TRUE;

		}
		else
		{
			field->arrSize = 1;
			field->isArray = TRUE;
		}


		if (!SmmpNextnode(&node))
			OneWayExit(FS_EXPECTFIELDNAME, exit);

	}
	else if (IsPtrChar(node))
	{
		field->isPointer = TRUE;

		if (!SmmpNextnode(&node))
			OneWayExit(FS_EXPECTFIELDNAME, exit);
	}

	if (SmmpIsSpecialToken(*Stringof(node)))
		OneWayExit(FS_EXPECTFIELDNAME, exit);

	strcpy(field->fieldName, Stringof(node));


	if (!SmmpNextnode(&node))
		OneWayExit(FS_EXPECTSEMICOLON, exit);

	if (!IsSemiColon(node))
		OneWayExit(FS_EXPECTSEMICOLON, exit);

	if (!SmmpNextnode(&node))
		OneWayExit(-1, exit);

	success = TRUE;

exit:

	*beginNode = node;
	*fieldPtr = field;

	return success;
}

WORD SmmpParseTypeFields(PSLISTNODE *beginNode, PSTRUCTINFO typeInfo)
{
	PSLISTNODE node;
	PTYPEINFOFIELD field;

	BOOL success = FALSE;

	node = *beginNode;

	if (!SmmpCreateSLIST(&typeInfo->fields))
		return 0;

	while (node != NULL)
	{
		if (SmmpParseTypeField(&node, &field))
		{
			SmmpAddSList(typeInfo->fields, field);

			if (field->isArray)
			{
				if (field->isStruct)
					typeInfo->structSize += field->type.structType->structSize * field->arrSize;
				else
					typeInfo->structSize += field->type.primitiveType->size * field->arrSize;
			}
			else if (field->isPointer)
				typeInfo->structSize += SIZEOF_POINTER;
			else
			{
				if (field->isStruct)
					typeInfo->structSize += field->type.structType->structSize;
				else
					typeInfo->structSize += field->type.primitiveType->size;
			}
		}
		else
			break;
	}

	*beginNode = node;

	return (WORD)typeInfo->fields->count;
}

#define ARG_IN		1
#define ARG_OUT		2

BOOL SmmpParseFunctionSignature(PSLISTNODE *startNode, PFNSIGN *funcSign)
{
	char name[128] = { 0 }, module[128] = { 0 };
	char argType[64], argName[128];
	PARGINFO argList;
	SHORT argCount = 0;
	PFNSIGN fnSign;
	PVOID argTypeInfo;
	BOOL isPrimitive=FALSE,isPtr=FALSE;
	int argInOut = 0;

	PSLISTNODE node = (*startNode)->next;


	strcpy(module, Stringof(node));

	if (!SmmpNextnode(&node))
		return FALSE;

	if (*Stringof(node) != '!')
		return FALSE;

	if (!SmmpNextnode(&node))
		return FALSE;

	strcpy(name, Stringof(node));

	if (!SmmpNextnode(&node))
		return FALSE;

	if (*Stringof(node) != '(')
		return FALSE;

	argList = ALLOCOBJECTLIST(ARGINFO, 10);
	fnSign = ALLOCOBJECT(FNSIGN);


	do
	{
		if (!SmmpNextnode(&node))
			return FALSE;

		memset(argType, 0, sizeof(argType));
		memset(argName, 0, sizeof(argName));
		argInOut = 0;
		isPtr = FALSE;

		if (SmmpIsSpecialToken(*Stringof(node)))
			return FALSE;

		if (!_stricmp(Stringof(node), "out"))
			argInOut = ARG_OUT;
		else if (!_stricmp(Stringof(node), "in"))
			argInOut = ARG_IN;

		if (argInOut > 0)
		{
			if (!SmmpNextnode(&node))
				return FALSE;

			if (SmmpIsSpecialToken(*Stringof(node)))
				return FALSE;
		}

		strcpy(argType, Stringof(node));

		if (!SmmpNextnode(&node))
			return FALSE;

		if (IsPtrChar(node))
		{
			isPtr = TRUE;

			if (!SmmpNextnode(&node))
				return FALSE;

			if (SmmpIsSpecialToken(*Stringof(node)))
				return FALSE;
		}


		strcpy(argName, Stringof(node));

		if (!SmmpNextnode(&node))
			return FALSE;

		strcpy(argList[argCount].name, argName);

		if (!SmmpGetTypeInfoByName(argType, &argTypeInfo, &isPrimitive))
		{
			FREEOBJECT(fnSign);
			FREEOBJECT(argList);
			return FALSE;
		}

		argList[argCount].isStructure = !isPrimitive;
		argList[argCount].typeInfo.holder = argTypeInfo;
		argList[argCount].isOutArg = argInOut == ARG_OUT;
		argList[argCount].isPointer = isPtr;

		if (argInOut == ARG_OUT && !fnSign->hasOutArg)
			fnSign->hasOutArg = TRUE;

		argCount++;

	} while (*Stringof(node) == ',');

	if (*Stringof(node) != ')')
	{
		FREEOBJECT(fnSign);
		FREEOBJECT(argList);
		return FALSE;
	}

	if (!SmmpNextnode(&node))
		node = NULL;

	fnSign->argCount = argCount;
	fnSign->args = argList;
	strcpy(fnSign->name, name);
	strcpy(fnSign->module, module);

	*funcSign = fnSign;

	*startNode = node;

	return TRUE;
}

BOOL _SmmpParseType(PSLISTNODE *startNode, PSTRUCTINFO *typeInfo)
{
	PSLISTNODE node = NULL;
	PSTRUCTINFO pti = NULL;
	BOOL success = FALSE;
	LONG failStep = 0;

	node = *startNode;

	if (strcmp(Stringof(node), "maptype"))
	{
		OneWayExit(FS_MAPTYPE, exit);
	}

	if (!SmmpNextnode(&node))
		OneWayExit(FS_TYPENAME, exit); //typename expected

	pti = ALLOCOBJECT(STRUCTINFO);

	if (!pti)
		OneWayExit(FS_MEMERR, exit);

	memset(pti, 0, sizeof(STRUCTINFO));
	pti->cbSize = sizeof(STRUCTINFO);

	if (SmmpIsSpecialToken(*Stringof(node)))
		OneWayExit(FS_TYPENAME, exit);

	strcpy(pti->name, Stringof(node));

	if (!SmmpNextnode(&node))
		OneWayExit(FS_BRACKOPEN, exit); //{ Expected

	if (!IsCurlyBrckOpen(node))
		OneWayExit(FS_BRACKOPEN, exit); //{ expected

	if (!SmmpNextnode(&node))
		OneWayExit(FS_EXPECTFIELD, exit); //Field definition expected

										  //Compile fields

	if (!SmmpParseTypeFields(&node, pti))
		OneWayExit(FS_EXPECTFIELD, exit);

	if (!IsCurlyBrckClose(node))
		OneWayExit(FS_BRACKCLOSE, exit); //} expected

	if (!SmmpNextnode(&node))
		node = NULL;

	success = TRUE;
exit:

	*startNode = node;

	if (!success)
		SmmpRaiseParseError(failStep);

	*typeInfo = pti;

	return success;

}


void SmmpDumpMapTypeInfo(PPRIMITIVETYPEINFO mti)
{
	_DBGPRINT("Typename: %s\n", mti->typeName);
	_DBGPRINT("Size: %d\n", mti->size);
	_DBGPRINT("Signed: %d\n", mti->isSigned);
}

void SmmpDumpType(PSTRUCTINFO type)
{
	PTYPEINFOFIELD field;

	_DBGPRINT("Name: %s\n", type->name);
	_DBGPRINT("---Fields---\n\n");

	for (PSLISTNODE n = type->fields->head; n != NULL; n = n->next)
	{
		field = RECORD_OF(n, PTYPEINFOFIELD);

		if (field->isStruct)
		{
			SmmpDumpType(field->type.structType);
			return;
		}

		_DBGPRINT("Name: %s\n", field->fieldName);
		SmmpDumpMapTypeInfo(field->type.primitiveType);
		_DBGPRINT("Is Array: %d\n", field->isArray);

		if (field->isArray)
			_DBGPRINT("Array size: %d\n", field->arrSize);

		_DBGPRINT("Is pointer: %d\n\n", field->isPointer);
	}
}

BOOL SmmpMapMemory(void *memory, ULONG size, PSTRUCTINFO typeInfo, SHORT depth);

BOOL SmmpMapForPrimitiveType(PPRIMITIVETYPEINFO pti, char *buffer, BYTE *mem, SHORT lastDepth, OPTIONAL PTYPEINFOFIELD ptif)
{
	PPRIMITIVETYPEINFO pmti;
	BYTE *subMem;

	if (ptif != NULL && ptif->isPointer)
	{
		if (!strcmp(pti->typeName, "char"))
			pmti = TYPE_IDENTS[MAX_TYPE_IDENTS - 3].pti;
		else if (!strcmp(pti->typeName, "wchar"))
			pmti = TYPE_IDENTS[MAX_TYPE_IDENTS - 2].pti;
		else if (!strcmp(pti->typeName, "pointer"))
			pmti = TYPE_IDENTS[MAX_TYPE_IDENTS - 1].pti;
		else
		{
			switch (*((WORD *)&ptif->type))
			{
			case sizeof(PRIMITIVETYPEINFO) :
			{
				pmti = pti;

				subMem = (BYTE *)AbMemoryAlloc(pmti->size);

				AbMemReadGuaranteed((duint)mem, subMem, pmti->size);

				pmti->method(buffer, subMem);

				AbMemoryFree(subMem);

				return TRUE;
			}
			case sizeof(STRUCTINFO) :
			{
				DBGPRINT("Not implemeneted yet");
				return FALSE;
				break;
			}
			}
		}

		pmti->method(buffer, mem);

		return TRUE;
	}
	else if (ptif != NULL && ptif->isArray)
	{
		if (!strcmp(pti->typeName, "char"))
			pmti = TYPE_IDENTS[MAX_TYPE_IDENTS - 3].pti;
		else if (!strcmp(pti->typeName, "wchar"))
			pmti = TYPE_IDENTS[MAX_TYPE_IDENTS - 2].pti;
		else
		{
			for (int i = 0;i < ptif->arrSize;i++)
			{
				pti->method(buffer, mem);
				DBGPRINT("[%d] = %s ", i, buffer);
				mem += ptif->arrSize;
			}

		}

		if (pmti)
			pmti->method(buffer, mem);
	}
	else
		pti->method(buffer, mem);
}

BOOL SmmpMapForUserType(PSTRUCTINFO sti, char *buffer, BYTE *mem, SHORT lastDepth)
{
	return SmmpMapMemory(mem, sti->structSize, sti, lastDepth + 1);
}

BOOL SmmpMapField(PTYPEINFOFIELD ptif, char *buffer, BYTE *mem,SHORT lastDepth)
{
	PPRIMITIVETYPEINFO pmti = NULL;
	BYTE *memArr = NULL;

	if (ptif->isStruct)
		return SmmpMapForUserType(ptif->type.structType, buffer, mem, lastDepth);

	return SmmpMapForPrimitiveType(ptif->type.primitiveType, buffer, mem, lastDepth, ptif);
}

#define PRINT_DEPTH_TABS(depth) for (int i=0;i<(depth);i++) HlpDebugPrint("\t")

BOOL SmmpMapMemory(void *memory, ULONG size, PSTRUCTINFO typeInfo, SHORT depth)
{
	BYTE *mem;
	PTYPEINFOFIELD ptif = NULL;
	DWORD typeSize = 0;

	char buffer[512];

	if (size < typeInfo->structSize)
		return FALSE;

	PRINT_DEPTH_TABS(depth);

	DBGPRINT("%s = [\n", typeInfo->name);

	mem = (BYTE *)memory;

	for (PSLISTNODE node = typeInfo->fields->head; node != NULL; node = node->next)
	{
		ptif = RECORD_OF(node, PTYPEINFOFIELD);
		memset(buffer, 0, sizeof(buffer));

		if (!SmmpMapField(ptif, buffer, mem,depth))
			return FALSE;

		typeSize = ptif->isStruct ? ptif->type.structType->structSize : ptif->type.primitiveType->size;

		if (ptif->isArray)
			mem += typeSize * ptif->arrSize;
		else if (ptif->isPointer)
			mem += SIZEOF_POINTER;
		else
			mem += typeSize;

		PRINT_DEPTH_TABS(depth + 1);

		DBGPRINT("%s = %s\n", ptif->fieldName, buffer);
	}

	PRINT_DEPTH_TABS(depth);

	DBGPRINT("]\n");

	return TRUE;
}

BOOL SmmpTypeIsPrimitive(LPVOID typeData, BOOL *isPrimitive)
{
	PGENERIC_DATATYPE_INFO gdti = (PGENERIC_DATATYPE_INFO)typeData;

	if (gdti->cbSize == sizeof(PRIMITIVETYPEINFO))
	{
		*isPrimitive = TRUE;
		return TRUE;
	}
	else if (gdti->cbSize == sizeof(STRUCTINFO))
	{
		*isPrimitive = FALSE;
		return TRUE;
	}

	return FALSE;
}

BOOL SmmMapFunctionCall(PPASSED_PARAMETER_CONTEXT passedParams, PFNSIGN fnSign, ApiFunctionInfo *afi)
{
	PARGINFO argInfo;
	BOOL isPrimitive;
	PGENERIC_DATATYPE_INFO gdti;
	PDMA dmaContent;
	char mapContentBuffer[0x1000];

	if (!DmaCreateAdapter(sizeof(char), 0x1000, &dmaContent))
		return FALSE;

	
	if (_stricmp(fnSign->module, afi->ownerModule->name) || _stricmp(fnSign->name, afi->name))
		return FALSE;

	for (int i = 0;i < fnSign->argCount;i++)
	{
		argInfo = &fnSign->args[i];

		if (!SmmpTypeIsPrimitive(argInfo->typeInfo.holder, &isPrimitive))
			return FALSE;

		
		if (isPrimitive)
			SmmpMapForPrimitiveType((PPRIMITIVETYPEINFO)argInfo->typeInfo.holder, mapContentBuffer, (BYTE *)passedParams->paramList[i], 0, NULL);
		else
		{
			//I need a special formatting for function map of the user types!!
			SmmpMapForUserType((PSTRUCTINFO)argInfo->typeInfo.holder, mapContentBuffer, (BYTE *)passedParams->paramList[i], 0);
		}

		DmaWrite(dmaContent, 0, strlen(mapContentBuffer), mapContentBuffer);

	}
}

BOOL SmmMapRemoteMemory(duint memory, ULONG size, const char *typeName)
{
	BOOL result;

	void *mem = AbMemoryAlloc(size);

	if (!mem)
		return FALSE;

	//transfer remote process memory to the local memory
	if (!AbMemReadGuaranteed(memory, mem, size))
	{
		AbMemoryFree(mem);
		return FALSE;
	}

	result = SmmMapMemory(mem, size, typeName);

	AbMemoryFree(mem);

	return result;
}

BOOL SmmMapMemory(void *memory, ULONG size, const char *typeName)
{
	for (PSLISTNODE node = SmmpUserTypeList->head; node != NULL; node = node->next)
	{
		if (!strcmp(RECORD_OF(node, PSTRUCTINFO)->name, typeName))
			return SmmpMapMemory(memory, size, RECORD_OF(node, PSTRUCTINFO),0);
	}

	return FALSE;
}

BOOL SmmGetFunctionSignature(const char *module, const char *function, PFNSIGN *signInfo)
{
	PFNSIGN fnSign;

	if (!signInfo)
		return FALSE;

	for (PSLISTNODE node = SmmpFnSignList->head; node != NULL; node = node->next)
	{
		fnSign = RECORD_OF(node, PFNSIGN);

		if (!_stricmp(fnSign->module, module) && !_stricmp(fnSign->name, function))
		{
			*signInfo = fnSign;
			return TRUE;
		}
	}

	*signInfo = NULL;
	return FALSE;
}

BOOL SmmSigHasOutArgument(PFNSIGN signInfo)
{
	return signInfo->hasOutArg;
}

BOOL SmmpParseType(LPCSTR typeDefString, WORD *typeCount)
{
	PSLIST tokenList;
	PSLISTNODE node;
	PSTRUCTINFO typeInfo = NULL;
	PFNSIGN fnSign = NULL;
	WORD compiledTypeCount = 0;
	LONG err = -1;
	LPWSTR inclFileW = NULL;
	WCHAR fullInclFileW[MAX_PATH];


	if (!SmmpTokenize(typeDefString, &tokenList))
		return FALSE;

	node = tokenList->head;

	while (node != NULL)
	{
		if (!_stricmp(Stringof(node),"@incl"))
		{
			if (!SmmpNextnode(&node))
				err = FS_EXPECTINCLFILEPATH;

			if (!HlpTrimChar(Stringof(node), '\'', HLP_TRIM_BOTH))
				err = FS_EXPECTINCLFILEPATH;

			if (err >= 0)
			{
				SmmpRaiseParseError(err);
				return FALSE;
			}

			inclFileW = HlpAnsiToWideString(Stringof(node));

			if (!inclFileW)
				return FALSE;

			memset(fullInclFileW, 0, sizeof(fullInclFileW));

			wsprintfW(fullInclFileW, L"%s\\%s", SmmpScriptWorkDir, inclFileW);
			
			FREESTRING(inclFileW);

			SmmParseFromFileW(fullInclFileW, typeCount);

			if (!SmmpNextnode(&node))
				node = NULL;

			continue;
		}

		if (!_stricmp(Stringof(node), "fnsign"))
		{
			if (!SmmpParseFunctionSignature(&node, &fnSign))
			{
				return FALSE;
			}

			SmmpAddSList(SmmpFnSignList, fnSign);

		}
		else if (!_stricmp(Stringof(node), "maptype"))
		{
			if (!_SmmpParseType(&node, &typeInfo))
			{
				//release all
				return FALSE;
			}

			SmmpAddSList(SmmpUserTypeList, typeInfo);
			SmmpAddSList(SmmpTypeList, typeInfo);

			compiledTypeCount++;

			SmmpDumpType(typeInfo);
		}

	}

	if (!compiledTypeCount)
	{
		if (typeCount)
			*typeCount = 0;

		return FALSE;
	}

	if (typeCount)
		*typeCount += compiledTypeCount;

	return TRUE;
}

BOOL SmmParseType(LPCSTR typeDefString, WORD *typeCount)
{
	if (!typeCount)
		return FALSE;

	if (!SmmpFnSignList)
	{
		//TODO: cleanup on failure
		if (!SmmpCreateSLIST(&SmmpFnSignList))
			return FALSE;

		if (!SmmpCreateSLIST(&SmmpTypeList))
			return FALSE;

		if (!SmmpCreateSLIST(&SmmpUserTypeList))
			return FALSE;

		if (!SmmpInitBasePrimitiveTypes())
			return FALSE;

		*typeCount = 0;
	}

	return SmmpParseType(typeDefString,typeCount);
}



BOOL SmmParseFromFileW(LPCWSTR fileName, WORD *typeCount)
{
	HANDLE fileHandle = NULL;
	char *fileContent = NULL;
	DWORD fsLo, fsHi, read;
	BOOL result = FALSE;

	if (!SmmpFnSignList)
	{
		//change prototype to const wstr
		if (!HlpPathFromFilenameW((LPWSTR)fileName, SmmpScriptWorkDir, MAX_PATH))
			return FALSE;
	}
	
	fileHandle = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

	if (fileHandle == INVALID_HANDLE_VALUE)
		goto exit;

	fsLo = GetFileSize(fileHandle, &fsHi);

	if (fsHi > 0)
	{
		//Hollyshit. Are you really sure that is it contains plain text?
		goto exit;
	}

	//Is it also larger than 1Meg for a standart text file.
	if (fsLo >= 0x100000)
		goto exit;

	fileContent = (char *)AbMemoryAlloc(fsLo);

	if (!fileContent)
		goto exit;
	
	if (!ReadFile(fileHandle, fileContent, fsLo, &read, NULL))
		goto exit;

	result = SmmParseType(fileContent, typeCount);
exit:

	if (fileContent)
		AbMemoryFree(fileContent);

	CloseHandle(fileHandle);

	return result;
}

BOOL SmmParseFromFileA(LPCSTR fileName, WORD *typeCount)
{
	LPCWSTR fileNameW = NULL;
	BOOL result;

	fileNameW = HlpAnsiToWideString(fileName);

	if (!fileNameW)
		return FALSE;

	result = SmmParseFromFileW(fileNameW, typeCount);

	FREESTRING(fileNameW);

	return result;
}