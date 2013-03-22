#!/usr/bin/env python

import os, util, struct
from gen_settings_types import Struct, settings, version, Field, String
from util import gob_uvarint_encode

g_magic_id = 0x53657454  # 'SetT' as 'Settings''
g_magic_id_str = "SetT"

g_script_dir = os.path.realpath(os.path.dirname(__file__))

def src_dir():
    p = os.path.join(g_script_dir, "..", "src")
    return util.verify_path_exists(os.path.realpath(p))

def settings_src_dir():
    p = os.path.join(g_script_dir, "..", "tools", "sertxt_test")
    return util.verify_path_exists(os.path.realpath(p))

def to_win_newlines(s):
    s = s.replace("\r\n", "\n")
    s = s.replace("\n", "\r\n")
    return s

def write_to_file(file_path, s): file(file_path, "w").write(to_win_newlines(s))

h_bin_tmpl   ="""// DON'T EDIT MANUALLY !!!!
// auto-generated by scripts/gen_settings.py !!!!

#ifndef SettingsSumatra_h
#define SettingsSumatra_h

%(struct_defs)s
#endif
"""

cpp_bin_tmpl = """// DON'T EDIT MANUALLY !!!!
// auto-generated by scripts/gen_settings.py !!!!

#include "BaseUtil.h"
#include "SerializeBin.h"
#include "SettingsSumatra.h"

using namespace serbin;

%(structs_metadata)s
%(values_global_data)s
%(top_level_funcs)s
"""

def ver_from_string(ver_str):
    parts = ver_str.split(".")
    assert len(parts) <= 4
    parts = [int(p) for p in parts]
    for n in parts:
        assert n > 0 and n < 255
    n = 4 - len(parts)
    if n > 0:
        parts = parts + [0]*n
    return parts[0] << 24 | parts[1] << 16 | parts[2] << 8 | parts[3]

# val is a top-level StructVal with primitive types and
# references to other struct types (forming a tree of values).
# we flatten the values into a list, in the reverse order of
# tree traversal
def flatten_struct(stru):
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
    vals.reverse()
    return vals

# serialize values in vals to binary and calculate offset of each
# val in encoded data.
# values are serialized in reverse order because
# it would be very complicated to serialize forward
# offsets in variable-length encoding
def serialize_top_level_struct(top_level_struct):
    vals = flatten_struct(top_level_struct)

    # the first 12 bytes are:
    #  - 4 bytes magic constant (for robustness)
    #  - 4 bytes for version
    #  - 4 bytes offset pointing to a top-level structure
    #      within the data
    offset = 12
    for stru in vals:
        stru.offset = offset
        for field in stru.values:
            data = field.serialized()
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
    if isinstance(obj, Struct):
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

"""
  // $StructName
  0x00, 0x01, // $type $name = $val
  ...
"""
def get_cpp_data_for_struct(stru):
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

    for val in stru.values:
        data = val.serialized()
        size += len(data)
        data_hex = data_to_hex(data)
        var_type = val.c_type()
        var_name = val.name
        if val.is_struct():
            val_str = "NULL"
            if val.val.offset != 0:
                val_str = short_object_id(val.val)
        else:
            if val.val == None:
                try:
                    assert isinstance(val.typ, String)
                except:
                    print(val)
                    print(val.name)
                    print(val.typ)
                    print(val.val)
                    raise
                val_str = ""
            elif type(val.val) == type(""):
                val_str = val.val
            else:
                try:
                    val_str = hex(val.val)
                except:
                    print(val)
                    print(val.name)
                    print(val.typ)
                    print(val.val)
                    raise
        s = "    %(data_hex)s, // %(var_type)s %(var_name)s = %(val_str)s" % locals()
        lines += [s]
    return (lines, size)

"""
static uint8_t g$(StructName)Default[] = {
   ... data
};
"""
def gen_cpp_data_for_struct_values(vals, version_str):
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
        (struct_lines, size) = get_cpp_data_for_struct(stru)
        lines += struct_lines
        total_size += size
    lines += ["};"]
    lines[0] = "static const uint8_t g%sDefault[%d] = {" % (name, total_size)

    return "\n".join(lines)

# TODO: could replace by a filed on Struct
g_struct_defs_generated = []

"""
struct $name {
   $type $field_name;
   ...
};
...
"""
def gen_struct_def(stru):
    global g_struct_defs_generated

    assert isinstance(stru, Struct)
    if stru in g_struct_defs_generated:
        return []
    #assert stru in GetAllStructs()
    name = stru.name()
    lines = ["struct %s {" % name]
    max_len = stru.get_max_field_type_len()
    fmt = "    %%-%ds %%s;" % max_len
    for val in stru.values:
        lines += [fmt % (val.c_type(), val.name)]
    lines += ["};\n"]
    return "\n".join(lines)

prototypes_tmpl = """#define %(name)sVersion "%(version_str)s"

%(name)s *Deserialize%(name)s(const uint8_t *data, int dataLen, bool *usedDefaultOut);
uint8_t *Serialize%(name)s(%(name)s *, int *dataLenOut);
void Free%(name)s(%(name)s *);
"""
def gen_struct_defs(vals, version_str):
    top_level = vals[-1]
    assert isinstance(top_level, Struct)
    name = top_level.__class__.__name__
    lines = [gen_struct_def(stru) for stru in vals]
    lines += [prototypes_tmpl % locals()]
    return "\n".join(lines)


"""
FieldMetadata g${name}FieldMetadata[] = {
    { $type, $offset, &g${name}StructMetadata },
};
"""
def gen_struct_fields(stru):
    assert isinstance(stru, Struct)
    struct_name = stru.name()
    lines = ["FieldMetadata g%(struct_name)sFieldMetadata[] = {" % locals()]
    max_type_len = 0
    for field in stru.values:
        max_type_len = max(max_type_len, len(field.typ.get_typ_enum()))
    max_type_len += 1

    typ_fmt = "%%-%ds " % max_type_len
    for field in stru.values:
        assert isinstance(field, Field)
        typ_enum = field.typ.get_typ_enum() + ","
        typ_enum = typ_fmt % typ_enum
        field_name = field.name
        offset = "offsetof(%(struct_name)s, %(field_name)s)" % locals()
        if field.is_struct():
            field_type = field.typ.name()
            lines += ["    { %(typ_enum)s %(offset)s, &g%(field_type)sMetadata }," % locals()]
        else:
            lines += ["    { %(typ_enum)s %(offset)s, NULL }," % locals()]
    lines += ["};\n"]
    return lines

"""
StructMetadata g${name}StructMetadata = { $size, $nFields, $fields };
"""
def gen_structs_metadata(structs):
    lines = []
    for stru in structs:
        struct_name = stru.name()
        nFields = len(stru.values)
        fields = "&g%sFieldMetadata[0]" % struct_name
        lines += gen_struct_fields(stru)
        lines += ["StructMetadata g%(struct_name)sMetadata = { sizeof(%(struct_name)s), %(nFields)d, %(fields)s };\n" % locals()]
    return "\n".join(lines)

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

def gen_top_level_funcs_bin(vals):
    top_level = vals[-1]
    assert isinstance(top_level, Struct)
    name = top_level.name()
    return top_level_funcs_bin_tmpl % locals()

def gen_bin():
    dst_dir = src_dir()

    val = settings
    vals = serialize_top_level_struct(val)

    struct_defs = gen_struct_defs(vals, version)
    write_to_file(os.path.join(dst_dir, "SettingsSumatra.h"),  h_bin_tmpl % locals())

    structs_metadata = gen_structs_metadata(vals)

    values_global_data = gen_cpp_data_for_struct_values(vals, version)
    top_level_funcs = gen_top_level_funcs_bin(vals)
    write_to_file(os.path.join(dst_dir, "SettingsSumatra.cpp"), cpp_bin_tmpl % locals())

############################### TEXT SERIALIZER

h_txt_tmpl   ="""// DON'T EDIT MANUALLY !!!!
// auto-generated by scripts/gen_settings.py !!!!

#ifndef SettingsSumatra_h
#define SettingsSumatra_h

%(struct_defs)s
#endif
"""

cpp_txt_tmpl = """// DON'T EDIT MANUALLY !!!!
// auto-generated by scripts/gen_settings.py !!!!

#include "BaseUtil.h"
#include "SerializeTxt.h"
#include "SettingsSumatra.h"

using namespace sertxt;

%(structs_metadata)s
%(values_global_data)s
%(top_level_funcs)s
"""

"""
FieldMetadata g${name}FieldMetadata[] = {
    { $name, $type, $offset, &g${name}StructMetadata },
};
"""
def gen_struct_fields_txt(stru):
    assert isinstance(stru, Struct)
    struct_name = stru.name()
    lines = ["FieldMetadata g%(struct_name)sFieldMetadata[] = {" % locals()]
    max_type_len = 0
    max_name_len = 0
    for field in stru.values:
        field_name = name2name(field.name)
        max_type_len = max(max_type_len, len(field.typ.get_typ_enum()))
        max_name_len = max(max_name_len, len(field_name))
    max_type_len += 1
    max_name_len += 2

    typ_fmt = "%%-%ds" % max_type_len
    name_fmt = "%%-%ds" % max_name_len
    for field in stru.values:
        assert isinstance(field, Field)
        typ_enum = field.typ.get_typ_enum() + ","
        typ_enum = typ_fmt % typ_enum
        field_name = field.name
        name_in_quotes = '"' +  name2name(field_name) + '"'
        field_name_c = name_fmt % name_in_quotes
        offset = "offsetof(%(struct_name)s, %(field_name)s)" % locals()
        if field.is_struct():
            field_type = field.typ.name()
            lines += ["    { %(field_name_c)s, %(typ_enum)s %(offset)s, &g%(field_type)sMetadata }," % locals()]
        else:
            lines += ["    { %(field_name_c)s, %(typ_enum)s %(offset)s, NULL }," % locals()]
    lines += ["};\n"]
    return lines

"""
StructMetadata g${name}StructMetadata = { $size, $nFields, $fields };
"""
def gen_structs_metadata_txt(structs):
    lines = []
    for stru in structs:
        struct_name = stru.name()
        nFields = len(stru.values)
        fields = "&g%sFieldMetadata[0]" % struct_name
        lines += gen_struct_fields_txt(stru)
        lines += ["StructMetadata g%(struct_name)sMetadata = { sizeof(%(struct_name)s), %(nFields)d, %(fields)s };\n" % locals()]
    return "\n".join(lines)

top_level_funcs_txt_tmpl = """
%(name)s *Deserialize%(name)s(const uint8_t *data, int dataLen, bool *usedDefaultOut)
{
    void *res = NULL;
    res = Deserialize(data, dataLen, %(name)sVersion, &g%(name)sMetadata);
    if (res) {
        *usedDefaultOut = false;
        return (%(name)s*)res;
    }
    res = Deserialize(NULL, 0, %(name)sVersion, &g%(name)sMetadata);
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

def gen_top_level_funcs_txt(vals):
    top_level = vals[-1]
    assert isinstance(top_level, Struct)
    name = top_level.name()
    return top_level_funcs_txt_tmpl % locals()

# fooBar => foo_bar
def name2name(s):
    if s == None:
        return None
    res = ""
    n_upper = 0
    for c in s:
        if c.isupper():
            if n_upper == 0:
                res += "_"
            n_upper += 1
            res += c.lower()
        else:
            if n_upper > 1:
                res += "_"
            res += c
            n_upper = 0
    return res

# TODO: support compact serialization of some structs e.g.
"""
  window_pos [
    x: 0
    y: 0
    dx: 0
    dy: 0
  ]
=>
  window_pos: 0 0 0 0
"""
# would need additional info in the data model
def gen_data_txt_rec(root_val, name, lines, indent):
    assert isinstance(root_val, Struct)
    if indent >= 0:
        prefix = " " * (indent*2)
    if name != None:
        name = name2name(name)
        lines += ["%s%s [" % (prefix, name)]
    for val in root_val.values:
        var_name = val.name
        var_name = name2name(var_name)
        if not val.is_struct():
            s = val.serialized_as_text()
            # omit serializing empty strings
            if s != None:
                lines += ["%s%s: %s" % (prefix + "  ", var_name, s)]
        else:
            if val.val.offset == 0:
                if False:
                    lines += ["%s%s: null" % (prefix + "  ", var_name)] # TODO: just omit the values?
            else:
                gen_data_txt_rec(val.val, var_name, lines, indent + 1)
    if name != None:
        lines += ["%s]" % prefix]

# TODO: convert setting names from fooBar => foo_bar, for better editability
def gen_txt():
    dst_dir = settings_src_dir()
    val = settings
    vals = serialize_top_level_struct(val)
    struct_defs = gen_struct_defs(vals, version)
    write_to_file(os.path.join(dst_dir, "SettingsSumatra.h"),  h_txt_tmpl % locals())

    structs_metadata = gen_structs_metadata_txt(vals)

    values_global_data = ""
    top_level_funcs = gen_top_level_funcs_txt(vals)
    write_to_file(os.path.join(dst_dir, "SettingsSumatra.cpp"), cpp_txt_tmpl % locals())

    lines = []
    gen_data_txt_rec(vals[-1], None, lines, -1)
    s = "\n".join(lines)
    write_to_file(os.path.join(dst_dir, "data.txt"), s)

def main():
    gen_txt()

if __name__ == "__main__":
    main()
