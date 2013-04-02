// DON'T EDIT MANUALLY !!!!
// auto-generated by gen_settings_txt.py !!!!

#ifndef SettingsTxtSimple_h
#define SettingsTxtSimple_h

namespace sertxt {

struct SimpleXY {
    int32_t  x;
    int32_t  y;
};

struct Simple {
    bool           bTrue;
    bool           bFalse;
    uint16_t       u16_1;
    int32_t        i32_1;
    uint32_t       u32_1;
    uint64_t       u64_1;
    uint32_t       col_1;
    float          float_1;
    SimpleXY *     xy1;
    const char *   str_1;
    const char *   str_escape;
    SimpleXY *     xy2;
    const WCHAR *  wstr_1;
};

Simple *DeserializeSimple(const char *data, size_t dataLen);
Simple *DeserializeSimpleWithDefault(const char *data, size_t dataLen, const char *defaultData, size_t defaultDataLen);
uint8_t *SerializeSimple(Simple *, size_t *dataLenOut);
void FreeSimple(Simple *);

} // namespace sertxt
#endif
