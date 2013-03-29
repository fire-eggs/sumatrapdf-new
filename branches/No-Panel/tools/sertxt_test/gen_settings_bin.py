#!/usr/bin/env python

import sys
sys.path.append("scripts") # assumes is being run as ./tools/sertxt_test/gen_settings_txt.py

import os, util, struct, types, gen_settings_types
from gen_settings_types import Struct, Field, String, Array, WString
from gen_settings_txt import structs_from_top_level_value, field_val_as_str
from util import gob_uvarint_encode, gob_varint_encode

g_magic_id = 0x53657454  # 'SetT' as 'Settings''
g_magic_id_str = "SetT"

g_script_dir = os.path.realpath(os.path.dirname(__file__))

def settings_src_dir():
    return util.verify_path_exists(g_script_dir)

def to_win_newlines(s):
    s = s.replace("\r\n", "\n")
    s = s.replace("\n", "\r\n")
    return s

def write_to_file(file_path, s): file(file_path, "w").write(to_win_newlines(s))

def serbin_str(val):
    # empty strings are encoded as 0 (0 length)
    # non-empty strings are encoded as uvariant(len(s)+1)
    # (+1 for terminating 0), followed by string data (including terminating 0)
    if val == None:
        return gob_uvarint_encode(0)
    if type(val) == type(u""):
        val = val.encode("utf-8")
    data = gob_uvarint_encode(len(val)+1)
    data = data + val + chr(0)
    return data

def serbin_arr(field):
    assert field.is_array()
    n = len(field.val.values)
    d = gob_uvarint_encode(n)
    for val in field.val.values:
        assert isinstance(val, Struct)
        off = val.offset
        assert type(off) in (types.IntType, types.LongType)
        d += gob_uvarint_encode(off)
    return d

def _serbin(field):
    assert isinstance(field, Field)
    if field.is_no_store():
        return None
    if field.is_signed():
        return gob_varint_encode(long(field.val))
    if field.is_unsigned():
        return gob_uvarint_encode(long(field.val))
    if field.is_string():
        return serbin_str(field.val)
    # floats are serialied as strings
    if field.is_float():
        return serbin_str(str(field.val))
    if field.is_struct():
        off = field.val.offset
        assert type(off) in (types.IntType, types.LongType)
        return gob_uvarint_encode(off)
    if field.is_array():
        return serbin_arr(field)
    assert False, "don't know how to serialize %s" % str(field.typ)

def serbin(v, serialized_vals):
    assert isinstance(v, Field)
    if not v in serialized_vals:
        serialized_vals[v] = _serbin(v)
    return serialized_vals[v]

h_bin_tmpl   ="""// DON'T EDIT MANUALLY !!!!
// auto-generated by gen_settings_bin.py !!!!

#ifndef %(file_name)s_h
#define %(file_name)s_h

namespace serbin {

%(struct_defs)s
%(prototypes)s
} // namespace serbin
#endif
"""

cpp_bin_tmpl = """// DON'T EDIT MANUALLY !!!!
// auto-generated by gen_settings_bin.py !!!!

#include "BaseUtil.h"
#include "SerializeBin.h"
#include "%(file_name)s.h"

namespace serbin {

#define of offsetof
%(structs_metadata)s
#undef of

%(values_global_data)s
%(top_level_funcs)s
}

"""

def ver_from_string(ver_str):
    parts = ver_str.split(".")
    assert len(parts) <= 4
    parts = [int(p) for p in parts]
    for n in parts:
        assert n >= 0 and n < 255
    n = 4 - len(parts)
    if n > 0:
        parts = parts + [0]*n
    return parts[0] << 24 | parts[1] << 16 | parts[2] << 8 | parts[3]

# stru is a top-level Struct with primitive types and
# references to other struct types (forming a tree of values).
# we flatten the values into a list, in the reverse order of
# tree traversal
def flatten_values(stru):
    assert isinstance(stru, Struct)
    vals = []
    left = [stru]
    while len(left) > 0:
        stru = left.pop(0)
        assert isinstance(stru, Struct)
        vals += [stru]
        for field in stru.values:
            if field.is_struct():
                if field.val != None:
                    assert isinstance(field.val, Struct)
                    left += [field.val]
            elif field.is_array():
                for v in field.val.values:
                    assert isinstance(v, Struct)
                    left += [v]
    vals.reverse()
    return vals

# serialize values in vals to binary and calculate offset of each
# val in encoded data.
# values are serialized in reverse order because
# it would be very complicated to serialize forward
# offsets in variable-length encoding
def serialize_top_level_value(top_level_val, serialized_vals):
    vals = flatten_values(top_level_val)
    # the first 12 bytes are:
    #  - 4 bytes magic constant (for robustness)
    #  - 4 bytes for version
    #  - 4 bytes offset pointing to a top-level structure
    #      within the data
    offset = 12
    for stru in vals:
        stru.offset = offset
        for field in stru.values:
            data = serbin(field, serialized_vals)
            if None != data:
                offset += len(data)
        fields_count = len(stru.values)
        offset = offset + 4 + len(gob_uvarint_encode(fields_count))
    return vals

def data_to_hex(data):
    els = ["0x%02x" % ord(c) for c in data]
    return ", ".join(els)

def dump_val(val):
    print("%s name: %s val: %s offset: %s\n" % (str(val), "", str(val.val), str(val.offset)))

def data_with_comment_as_c(data, comment):
    data_hex = data_to_hex(data)
    return "    %(data_hex)s, // %(comment)s" % locals()

g_addr_to_int_map = {}
# change:
#   <$module.Struct object at 0x7fddfc4c>
# =>
#   Struct_$n where $n maps 0x7fddfc4c => unique integer
def short_object_id(obj):
    global g_addr_to_int_map
    if isinstance(obj, Struct) or isinstance(obj, Array):
        s = str(obj)[1:-1]
        s = ".".join(s.split(".")[1:])
        s = s.replace(" object at ", "@")
        (name, addr) = s.split("@")
        if addr not in g_addr_to_int_map:
            g_addr_to_int_map[addr] = str(len(g_addr_to_int_map))
        return name + "_" + g_addr_to_int_map[addr]
    #if isinstance(obj, Field):
    #    #ssert is_str(obj.val)
    #    return '"' + obj.val + '"'
    assert False, "%s is object of unkown type" % obj

def str_c_safe(s):
    if type(s) == type(""):
        s = s.decode("utf-8")
    try:
        assert type(s) == type(u"")
    except:
        print(s)
        raise
    s = s.encode('ascii','ignore')
    s = s.replace("\n", "\\n")
    s = s.replace("\r", "\\r")
    return s

"""
  // $StructName
  0x00, 0x01, // $type $name = $val
  ...
"""
def get_cpp_data_for_struct(stru, serialized_vals):
    assert isinstance(stru, Struct)
    name = stru.name()
    offset = stru.offset
    lines = ["", "    // offset: %s %s" % (hex(offset), short_object_id(stru))]
    assert stru.values not in (None, [])

    # magic id
    data = struct.pack("<I", g_magic_id)
    comment = "magic id '%s'" % g_magic_id_str
    lines += [data_with_comment_as_c(data, comment)]

    # number of fields as uvarint
    fields_count = len(stru.values)
    data = gob_uvarint_encode(fields_count)
    lines += [data_with_comment_as_c(data, "%d fields" % fields_count)]
    size = 4 + len(data)

    for field in stru.values:
        if field.is_no_store():
            continue
        data = serbin(field, serialized_vals)
        size += len(data)
        data_hex = data_to_hex(data)
        var_type = field.c_type()
        var_name = field.name
        if field.is_struct():
            val_str = "NULL"
            if field.val.offset != 0:
                val_str = short_object_id(field.val)
        elif field.is_array():
                val_str = short_object_id(field.val)
        else:
            val_str = field_val_as_str(field)
            if val_str == None:
                assert isinstance(field.typ, String) or isinstance(field.typ, WString)
                val_str = "NULL"
            val_str = str_c_safe(val_str)
        s = "    %(data_hex)s, // %(var_type)s %(var_name)s = %(val_str)s" % locals()
        lines += [s]
    return (lines, size)

"""
static uint8_t g$(StructName)Default[] = {
   ... data
};
"""
def gen_cpp_data_for_struct_values(vals, version_str, serialized_vals):
    top_level = vals[-1]
    assert isinstance(top_level, Struct)
    name = top_level.name()
    lines = [""] # will be replaced by: "static const uint8_t g%sDefault[%(total_size)d] = {" at the end

    data = struct.pack("<I", g_magic_id)
    comment = "magic id '%s'" % g_magic_id_str
    lines += [data_with_comment_as_c(data, comment)]
    # version
    data = struct.pack("<I", ver_from_string(version_str))
    comment = "version %s" % version_str
    lines += [data_with_comment_as_c(data, comment)]
    # offset of top-level struct
    data = struct.pack("<I", top_level.offset)
    comment = "top-level struct offset %s" % hex(top_level.offset)
    lines += [data_with_comment_as_c(data, comment)]

    total_size = 12
    for stru in vals:
        (struct_lines, size) = get_cpp_data_for_struct(stru, serialized_vals)
        lines += struct_lines
        total_size += size
    lines += ["};"]
    lines[0] = "static const uint8_t g%sDefault[%d] = {" % (name, total_size)

    return "\n".join(lines)

"""
struct $name {
   $type $field_name;
   ...
};
...
"""
def gen_struct_def(stru_cls):
    name = stru_cls.__name__
    lines = ["struct %s {" % name]
    rows = [[field.c_type(), field.name] for field in stru_cls.fields]
    lines += ["    %s  %s;" % (e1, e2) for (e1, e2) in util.fmt_rows(rows, [util.FMT_RIGHT])]
    lines += ["};\n"]
    return "\n".join(lines)

def gen_struct_defs(structs):
    return "\n".join([gen_struct_def(stru) for stru in structs])

"""
FieldMetadata g${name}FieldMetadata[] = {
    { $offset, $type, &g${name}StructMetadata },
};
"""
def gen_struct_fields_bin(stru_cls):
    struct_name = stru_cls.__name__
    lines = ["FieldMetadata g%sFieldMetadata[] = {" % struct_name]
    rows = []
    for field in stru_cls.fields:
        assert isinstance(field, Field)
        typ_enum = field.get_typ_enum(for_bin=True)
        offset = "of(%s, %s)" % (struct_name, field.name)
        val = "NULL"
        if field.is_struct() or field.is_array():
            val = "&g%sMetadata" % field.typ.name()
        col = [offset + ",", typ_enum + ",", val]
        rows.append(col)
    rows = util.fmt_rows(rows, [util.FMT_RIGHT, util.FMT_RIGHT, util.FMT_RIGHT])
    lines += ["    { %s %s %s }," % (e1, e2, e3) for (e1, e2, e3) in rows]
    lines += ["};\n"]
    return lines

"""
StructMetadata g${name}StructMetadata = { $size, $nFields, $fields };
"""
def gen_structs_metadata_bin(structs):
    lines = []
    for stru_cls in structs:
        struct_name = stru_cls.__name__
        nFields = len(stru_cls.fields)
        fields = "&g%sFieldMetadata[0]" % struct_name
        lines += gen_struct_fields_bin(stru_cls)
        lines += ["StructMetadata g%(struct_name)sMetadata = { sizeof(%(struct_name)s), %(nFields)d, %(fields)s };\n" % locals()]
    return "\n".join(lines)

prototypes_tmpl = """#define %(name)sVersion "%(version_str)s"

%(name)s *Deserialize%(name)s(const uint8_t *data, int dataLen, bool *usedDefaultOut);
uint8_t *Serialize%(name)s(%(name)s *, int *dataLenOut);
void Free%(name)s(%(name)s *);
"""

def gen_prototypes(stru_cls, version_str):
    name = stru_cls.__name__
    return prototypes_tmpl % locals()

top_level_funcs_bin_tmpl = """
%(name)s *Deserialize%(name)s(const uint8_t *data, int dataLen, bool *usedDefaultOut)
{
    void *res = NULL;
    res = Deserialize(data, dataLen, %(name)sVersion, &g%(name)sMetadata);
    if (res) {
        *usedDefaultOut = false;
        return (%(name)s*)res;
    }
    res = Deserialize(&g%(name)sDefault[0], sizeof(g%(name)sDefault), %(name)sVersion, &g%(name)sMetadata);
    CrashAlwaysIf(!res);
    *usedDefaultOut = true;
    return (%(name)s*)res;
}

uint8_t *Serialize%(name)s(%(name)s *val, int *dataLenOut)
{
    return Serialize((const uint8_t*)val, %(name)sVersion, &g%(name)sMetadata, dataLenOut);
}

void Free%(name)s(%(name)s *val)
{
    FreeStruct((uint8_t*)val, &g%(name)sMetadata);
}
"""

def gen_top_level_funcs_bin(top_level_val):
    assert isinstance(top_level_val, Struct)
    name = top_level_val.name()
    return top_level_funcs_bin_tmpl % locals()

def gen_bin_for_top_level_val(top_level_val, version, file_path_base):
    file_name = os.path.basename(file_path_base)
    prototypes = gen_prototypes(top_level_val.__class__, version)
    structs = structs_from_top_level_value(top_level_val)
    struct_defs = gen_struct_defs(structs)
    write_to_file(file_path_base + ".h",  h_bin_tmpl % locals())
    structs_metadata = gen_structs_metadata_bin(structs)
    top_level_funcs = gen_top_level_funcs_bin(top_level_val)
    serialized_vals = {}
    vals = serialize_top_level_value(top_level_val, serialized_vals)
    values_global_data = gen_cpp_data_for_struct_values(vals, version, serialized_vals)
    write_to_file(os.path.join(file_path_base + ".cpp"), cpp_bin_tmpl % locals())

def gen_bin_sumatra():
    dst_dir = settings_src_dir()
    file_path_base = os.path.join(dst_dir, "SettingsBinSumatra")
    top_level_val = gen_settings_types.Settings()
    gen_bin_for_top_level_val(top_level_val, "2.3", file_path_base)

def gen_bin_simple():
    dst_dir = settings_src_dir()
    file_path_base = os.path.join(dst_dir, "SettingsBinSimple")
    top_level_val = gen_settings_types.Simple()
    gen_bin_for_top_level_val(top_level_val, "1.0", file_path_base)

def main():
    gen_bin_sumatra()
    gen_bin_simple()

if __name__ == "__main__":
    main()
