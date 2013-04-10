// DON'T EDIT MANUALLY !!!!
// auto-generated by gen_txt.py !!!!

#ifndef MuiScrollBarDef_h
#define MuiScrollBarDef_h

#include "SerializeTxt.h"
using namespace sertxt;

struct ScrollBarDef {
    const char *  name;
    const char *  style;
    const char *  cursor;
};

extern const StructMetadata gScrollBarDefMetadata;

inline ScrollBarDef *DeserializeScrollBarDef(char *data, size_t dataLen)
{
    return (ScrollBarDef*)Deserialize(data, dataLen, &gScrollBarDefMetadata);
}

inline ScrollBarDef *DeserializeScrollBarDef(TxtNode* root)
{
    return (ScrollBarDef*)Deserialize(root, &gScrollBarDefMetadata);
}

inline uint8_t *SerializeScrollBarDef(ScrollBarDef *val, size_t *dataLenOut)
{
    return Serialize((const uint8_t*)val, &gScrollBarDefMetadata, dataLenOut);
}

inline void FreeScrollBarDef(ScrollBarDef *val)
{
    FreeStruct((uint8_t*)val, &gScrollBarDefMetadata);
}

#endif
