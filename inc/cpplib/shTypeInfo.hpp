typedef struct {
	const char *type;
	const char *name;
	int offset;
	int size;
} classVariableInfo_t;

typedef struct {
	//
	const char *typeName;
	//! The name of the class.
	const char *superType;
	//! The size of the class in bytes.
	int size;
	//! Stores the offsets of all variables in the class, connecting them to a name and type identifier.
	const ClassEdictVariableInfo_t *variables;
} ClassEdictTypeInfo_t;


static classTypeInfo_t classTypeInfo[] = {
	{ NULL, NULL, 0, NULL }
};