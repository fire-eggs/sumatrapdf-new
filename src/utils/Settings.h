/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Settings_h
#define Settings_h

namespace settings {

struct FieldMetadata;

typedef struct {
    uint16_t        size;
    uint16_t        nFields;
    FieldMetadata * fields;
} StructMetadata;

typedef enum : uint16_t {
    TYPE_BOOL,
    TYPE_I16,
    TYPE_U16,
    TYPE_I32,
    TYPE_U32,
    TYPE_U64,
    TYPE_FLOAT,
    TYPE_STR,
    TYPE_WSTR,
    TYPE_STRUCT_PTR,
    TYP_ARRAY,
} Typ;

// information about a single field
struct FieldMetadata {
    Typ              type; // TYPE_*
    // from the beginning of the struct
    uint16_t         offset;
    // type is TYPE_STRUCT_PTR, otherwise NULL
    StructMetadata * def;
};

void        FreeStruct(uint8_t *data, StructMetadata *def);
uint8_t*    Deserialize(const uint8_t *data, int dataSize, const char *version, StructMetadata *def);
uint8_t *   Serialize(const uint8_t *data, const char *version, StructMetadata *def, int *sizeOut);

} // namespace Settings

int         GobVarintEncode(int64_t val, uint8_t *d, int dLen);
int         GobUVarintEncode(uint64_t val, uint8_t *d, int dLen);
int         GobVarintDecode(const uint8_t *d, int dLen, int64_t *resOut);
int         GobUVarintDecode(const uint8_t *d, int dLen, uint64_t *resOut);

#endif
