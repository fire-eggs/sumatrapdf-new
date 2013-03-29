"""
This script generates a struct and enough metadata for reading
a variety of preference values from a user provided settings file.
See further below for the definition of all currently supported options.
"""

import util2

class Type(object):
	def __init__(self, name, ctype):
		self.name = name; self.ctype = ctype

Bool = Type("Bool", "bool")
Color = Type("Color", "COLORREF")
Float = Type("Float", "float")
Int = Type("Int", "int")
Int64 = Type("Int64", "int64_t")
String = Type("String", "WCHAR *")
Utf8String = Type("Utf8String", "char *")

class Field(object):
	def __init__(self, name, type, default, comment, internal=False):
		self.name = name; self.type = type; self.default = default; self.comment = comment
		self.internal = internal; self.cname = name[0].lower() + name[1:] if name else None

	def cdefault(self):
		if self.type == Bool:
			return "true" if self.default else "false"
		if self.type == Color:
			return "0x%06x" % self.default
		if self.type == Float:
			return '(int64_t)"%g"' % self.default # converting float to int64_t rounds the value
		if self.type == Int or self.type == Int64:
			return "%d" % self.default
		if self.type == String:
			return '(int64_t)L"%s"' % self.default if self.default is not None else "NULL"
		if self.type == Utf8String:
			return '(int64_t)"%s"' % self.default if self.default is not None else "NULL"
		if self.type.name == "Custom":
			return self.default
		return None

	def inidefault(self):
		if self.type == Bool:
			return "%s = %s" % (self.name, "true" if self.default else "false")
		if self.type == Color:
			return "%s = #%02x%02x%02x" % (self.name, self.default & 0xFF, (self.default >> 8) & 0xFF, (self.default >> 16) & 0xFF)
		if self.type == Float:
			return "%s = %g" % (self.name, self.default)
		if self.type == Int or self.type == Int64:
			return "%s = %d" % (self.name, self.default)
		if self.type == String:
			if self.default is not None:
				return "%s = %s" % (self.name, self.default.encode("UTF-8"))
			return "; %s =" % self.name
		if self.type == Utf8String:
			if self.default is not None:
				return "%s = %s" % (self.name, self.default)
			return "; %s =" % self.name
		if self.type.name == "Custom":
			return self.default
		return "; %s = ???" % self.name

class Struct(Field):
	def __init__(self, name, fields, comment, structName=None, compact=False):
		self.structName = structName or name
		super(Struct, self).__init__(name, Type("Struct", "%s" % self.structName), fields, comment)
		if compact: self.type.name = "Compact"

class Array(Field):
	def __init__(self, name, fields, comment, structName=None):
		self.structName = structName or name
		super(Array, self).__init__(name, Type("Array", "%s *" % self.structName), fields, comment)

# ##### setting definitions for SumatraPDF #####

RectI = [
	Field("X", Int, 0, "x coordinate"),
	Field("Y", Int, 0, "y coordinate"),
	Field("Dx", Int, 0, "width"),
	Field("Dy", Int, 0, "height"),
]

PointI = [
	Field("X", Int, 0, "x coordinate"),
	Field("Y", Int, 0, "y coordinate"),
]

AdvancedPrefs = [
	Field("TraditionalEbookUI", Bool, False,
		"whether the UI used for PDF documents will be used for ebooks as well " +
		"(enables printing and searching, disables automatic reflow)"),
	Field("ReuseInstance", Bool, False,
		"whether opening a new document should happen in an already running SumatraPDF " +
		"instance so that there's only one process and documents aren't opend twice"),
	Field("MainWindowBackground", Color, 0xFFF200,
		"background color of the non-document windows, traditionally yellow"),
	Field("EscToExit", Bool, False,
		"whether the Esc key will exit SumatraPDF same as 'q'"),
	Field("TextColor", Color, 0x000000,
		"color value with which black (text) will be substituted"),
	Field("PageColor", Color, 0xFFFFFF,
		"color value with which white (background) will be substituted"),
]

PagePadding = [
	Field("OuterX", Int, 4, "size of the left/right margin between window and document"),
	Field("OuterY", Int, 2, "size of the top/bottom margin between window and document"),
	Field("InnerX", Int, 4, "size of the horizontal margin between two pages"),
	Field("InnerY", Int, 4, "size of the vertical margin between two pages"),
]

BackgroundGradient = [
	Field("Enabled", Bool, False, "whether to draw a gradient behind the pages"),
	Field("ColorTop", Color, 0xAA2828, "color at the top of the document (first page)"),
	Field("ColorMiddle", Color, 0x28AA28, "color at the center of the document (middlest page)"),
	Field("ColorBottom", Color, 0x2828AA, "color at the bottom of the document (last page)"),
]

PrinterDefaults = [
	Field("PrintScale", Utf8String, "shrink", "default value for scaling (shrink, fit, none)"),
	Field("PrintAsImage", Bool, False, "default value for the compatibility option"),
]

ForwardSearch = [
	Field("HighlightOffset", Int, 0,
		"when set to a positive value, the forward search highlight style will " +
		"be changed to a rectangle at the left of the page (with the indicated " +
		"amount of margin from the page margin)"),
	Field("HighlightWidth", Int, 15,
		"the width of the highlight rectangle for when HighlightOffset is set"),
	Field("HighlightColor", Color, 0x6581FF,
		"the color used for the forward search highlight"),
	Field("HighlightPermanent", Bool, False,
		"whether the forward search highlight will remain visible until the next " +
		"mouse click instead of fading away instantly"),
]

ExternalViewer = [
	Field("CommandLine", String, None,
		"command line with which to call the external viewer, may contain " +
		"%p for page numer and %1 for the file name"),
	Field("Name", String, None,
		"name of the external viewer to be shown in the menu (implied by CommandLine if missing)"),
	Field("Filter", String, None,
		"filter for which file types the menu item is to be shown (e.g. \"*.pdf;*.xps\"; \"*\" if missing)"),
]

UserPrefs = [
	Struct("AdvancedPrefs", AdvancedPrefs,
		"these values allow to tweak various bits and pieces of SumatraPDF"),
	Struct("PrinterDefaults", PrinterDefaults,
		"these values allow to override the default settings in the Print dialog"),
	Struct("PagePadding", PagePadding,
		"these values allow to change how far apart pages are layed out"), # TODO: compact?
	Struct("BackgroundGradient", BackgroundGradient,
		"these values allow to tweak the experimental feature for using a color " +
		"gradient to subconsciously determine reading progress"),
	Struct("ForwardSearch", ForwardSearch,
		"these values allow to customize how the forward search highlight appears"),
	Array("ExternalViewer", ExternalViewer,
		"this list contains a list of additional external viewers for various file types"),
]

FileSettings = [
	Field("FilePath", String, None,
		"absolute path to a document that's been loaded successfully"),
	Field("OpenCount", Int, 0,
		"in order to prevent documents that haven't been opened for a while " +
		"but used to be opened very frequently constantly remain in top positions, " +
		"the openCount will be cut in half after every week, so that the " +
		"Frequently Read list hopefully better reflects the currently relevant documents"),
	Field("IsPinned", Bool, False,
		"a user can \"pin\" a preferred document to the Frequently Read list " +
		"so that the document isn't replaced by more frequently used ones"),
	Field("IsMissing", Bool, False,
		"if a document can no longer be found but we still remember valuable state, " +
		"it's classified as missing so that it can be hidden instead of removed"),
	Field("UseGlobalValues", Bool, False,
		"whether global defaults should be used when reloading this file instead of " +
		"the values listed below"),
	Field("DisplayMode", String, "automatic", # TODO: Type_Custom, DM_AUTOMATIC
		"how pages should be layed out for this document"),
	Struct("ScrollPos", PointI,
		"how far this document has been scrolled", structName="PointI", compact=True),
	Field("PageNo", Int, 1,
		"the scrollPos values are relative to the top-left corner of this page"),
	Field("ReparseIdx", Int, 0,
		"for bookmarking ebook files: offset of the current page reparse point within html"),
	Field("ZoomVirtual", Float, 100.0,
		"the current zoom factor in % (negative values indicate virtual settings)"),
	Field("Rotation", Int, 0,
		"how far pages have been rotated as a multiple of 90 degrees"),
	Field("WindowState", Int, 0,
		"default state of new SumatraPDF windows (same as the last closed)"),
	Struct("WindowPos", RectI,
		"default position (can be on any monitor)", structName="RectI", compact=True),
	Field("DecryptionKey", Utf8String, None,
		"hex encoded MD5 fingerprint of file content (32 chars) followed by " +
		"crypt key (64 chars) - only applies for PDF documents"),
	Field("TocVisible", Bool, True,
		"whether the table of contents (Bookmarks) sidebar is shown for this document"),
	Field("SidebarDx", Int, 0,
		"the width of the left sidebar panel containing the table of contents"),
	Field("TocState", Utf8String, None, # TODO: inline int array?
		"tocState is an array of ids for ToC items that have been toggled by " +
		"the user (i.e. aren't in their default expansion state). - " +
		"Note: We intentionally track toggle state as opposed to expansion state " +
		"so that we only have to save a diff instead of all states for the whole " +
		"tree (which can be quite large) - and also due to backwards compatibility"),
	Array("Favorite", [
		Field("Name", String, None, "name of this favorite as shown in the menu"),
		Field("PageNo", Int, 0, "which page this favorite is about"),
		Field("PageLabel", String, None, "optional label for this page (if logical and physical numers disagree)"),
		Field("MenuId", Int, 0, "assigned in AppendFavMenuItems()", internal=True),
	], "Values which are persisted for bookmarks/favorites"),
	Field("Index", Type("Custom", "size_t"), "0",
		"temporary value needed for FileHistory::cmpOpenCount",
		internal=True),
	Field("Thumbnail", Type("Custom", "void *"), "NULL", # TODO: RenderedBitmap *
		"the thumbnail is persisted separately as a PNG in sumatrapdfcache",
		internal=True),
]

AppPrefs = [
	Field("GlobalPrefsOnly", Bool, False,
		"whether not to store display settings for individual documents"),
	Field("CurrLangCode", String, None, # TODO: Type_Custom
		"pointer to a static string that is part of LangDef, don't free"),
	Field("ToolbarVisible", Bool, True,
		"whether the toolbar should be visible by default in the main window"),
	Field("FavVisible", Bool, False,
		"whether the Favorites sidebar should be visible by default in the main window"),
	Field("PdfAssociateDontAskAgain", Bool, False,
		"if false, we won't ask the user if he wants Sumatra to handle PDF files"),
	Field("PdfAssociateShouldAssociate", Bool, False,
		"if pdfAssociateDontAskAgain is true, says whether we should silently associate or not"),
	Field("EnableAutoUpdate", Bool, True,
		"whether SumatraPDF should check once a day whether updates are available"),
	Field("RememberOpenedFiles", Bool, True,
		"if true, we remember which files we opened and their settings"),
	Field("UseSysColors", Bool, False,
		"whether to display documents black-on-white or in system colors"),
	Field("InverseSearchCmdLine", String, None,
		"pattern used to launch the editor when doing inverse search"),
	Field("EnableTeXEnhancements", Bool, False,
		"whether to expose the SyncTeX enhancements to the user"),
	Field("VersionToSkip", String, None,
		"When we show 'new version available', user has an option to check 'skip this version'. " +
		"This remembers which version is to be skipped. If NULL - don't skip"),
	Field("LastUpdateTime", Int64, 0,
		"the time SumatraPDF has last checked for updates (cf. EnableAutoUpdate)"),
	Field("DefaultDisplayMode", String, "automatic", # TODO: Type_Custom, DM_AUTOMATIC
		"how pages should be layed out by default"),
	Field("DefaultZoom", Float, -1,
		"the default zoom factor in % (negative values indicate virtual settings)"),
	Field("WindowState", Int, 1,
		"default state of new SumatraPDF windows (same as the last closed)"),
	Struct("WindowPos", RectI,
		"default position (can be on any monitor)", structName="RectI", compact=True),
	Field("TocVisible", Bool, True,
		"whether the table of contents (Bookmarks) sidebar should be shown by " +
		"default when its available for a document"),
	Field("SidebarDx", Int, 0,
		"if sidebar (favorites and/or bookmarks) is visible, this is "+
		"the width of the left sidebar panel containing them"),
	Field("TocDy", Int, 0,
		"if both favorites and bookmarks parts of sidebar are visible, this is " +
		"the height of bookmarks (table of contents) part"),
	Field("ShowStartPage", Bool, True,
		"whether to display Frequently Read documents or the About page in an empty window"),
	Field("OpenCountWeek", Int, 0,
		"week count since 2011-01-01 needed to \"age\" openCount values in file history"),
	Field("CbxR2L", Bool, False,
		"display CBX double pages from right to left"),
	# file history and favorites
	Array("File", FileSettings,
		"Most values in this structure are remembered individually for every file and " +
		"are by default also persisted so that reading can be resumed"),
	# non-serialized fields
	Field("LastPrefUpdate", Int64, 0,
		"modification time of the preferences file when it was last read",
		internal=True),
	Field("UnknownSettings", Utf8String, None,
		"a list of settings which this version of SumatraPDF didn't know how to handle ",
		internal=True),
]

GlobalPrefs = Struct("GlobalPrefs", AppPrefs,
	"Most values on this structure can be updated through the UI and are persisted " +
	"in SumatraPDF.ini (previously in sumatrapdfprefs.dat)")

UserPrefs = Struct("UserPrefs", UserPrefs,
	"All values in this structure are read from SumatraPDF-user.ini and can't be " +
	"changed from within the UI")

# ##### end of setting definitions for SumatraPDF #####

def FormatComment(comment, start):
	result, parts, line = [], comment.split(), start
	while parts:
		while parts and (line == start or len(line + parts[0]) < 72):
			line += " " + parts.pop(0)
		result.append(line)
		line = start
	return result

def BuildStruct(struct, built=[]):
	lines = ["struct %s {" % struct.name]
	if struct.comment:
		lines = FormatComment(struct.comment, "//") + lines
	for field in struct.default:
		lines += FormatComment(field.comment, "\t//")
		lines.append("\t%s %s;" % (field.type.ctype, field.cname))
		if type(field) == Array:
			lines.append("\tsize_t %sCount;" % field.cname)
		if type(field) in [Struct, Array] and field.name == field.structName and field.name not in built:
			lines = [BuildStruct(field), ""] + lines
			built.append(field.name)
	lines.append("};")
	return "\n".join(lines)

def BuildMetaData(struct, built=[]):
	fieldInfo, metadata = [], []
	for field in struct.default:
		if field.internal:
			continue
		if type(field) == Struct:
			fieldInfo.append("\t{ \"%s\", Type_%s, offsetof(%s, %s), g%sInfo, NULL }," % (field.name, field.type.name, struct.structName, field.cname, field.structName))
		elif type(field) == Array:
			fieldInfo.append("\t{ \"%s\", Type_%s, offsetof(%s, %s), g%sInfo, NULL }," % (field.name, field.type.name, struct.structName, field.cname, field.structName))
			fieldInfo.append("\t{ NULL, Type_Meta, offsetof(%s, %sCount), g%sInfo, NULL }," % (struct.structName, field.cname, field.name))
		else:
			fieldInfo.append("\t{ \"%s\", Type_%s, offsetof(%s, %s), NULL, %s }," % (field.name, field.type.name, struct.structName, field.cname, field.cdefault()))
		if type(field) in [Struct, Array] and field.structName not in built:
			metadata.append(BuildMetaData(field))
			built.append(field.structName)
	metadata.append("\n".join([
		"static SettingInfo g%sInfo[] = {" % struct.structName,
		# include size information in the first line instead of a second structure
		"\t{ NULL, Type_Meta, sizeof(%s), NULL, %d }," % (struct.structName, len(fieldInfo)),
	] + fieldInfo + [
		"};"
	]))
	return "\n\n".join(metadata)

def AssembleDefaults(struct):
	lines, more = [], []
	for field in struct.default:
		if field.internal:
			continue
		if type(field) in [Struct, Array]:
			more.append("\n".join(FormatComment(field.comment, ";") + ["[%s]" % field.name, AssembleDefaults(field)]))
		else:
			if field.comment:
				lines += FormatComment(field.comment, ";")
			lines.append(field.inidefault())
	return "\n".join(lines) + "\n".join(more) + "\n"

AppPrefs3_Header = """\
/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

// This file is auto-generated by gen_appprefs3.py

#ifndef AppPrefs3_h
#define AppPrefs3_h

%(appStructDef)s

%(userStructDef)s

enum SettingType {
	Type_Meta, Type_Struct, Type_Array, Type_Compact,
	Type_Bool, Type_Color, Type_Float, Type_Int, Type_Int64, Type_String, Type_Utf8String,
};

struct SettingInfo {
	const char *name;
	SettingType type;
	size_t offset;
	SettingInfo *substruct;
	int64_t def;
};
STATIC_ASSERT(sizeof(int64_t) >= sizeof(void *), ptr_is_max_64_bit);

static inline size_t GetFieldCount(SettingInfo *meta) { return (size_t)meta[0].def; }
static inline size_t GetStructSize(SettingInfo *meta) { return meta[0].offset; }

#ifdef INCLUDE_APPPREFS3_METADATA
%(appStructMetadata)s

%(userStructMetadata)s
#endif

#endif
"""

def main():
	util2.chdir_top()

	appStructDef = BuildStruct(GlobalPrefs)
	appStructMetadata = BuildMetaData(GlobalPrefs)
	userStructDef = BuildStruct(UserPrefs)
	userStructMetadata = BuildMetaData(UserPrefs)

	content = AppPrefs3_Header % locals()
	open("tools/serini_test/AppPrefs3.h", "wb").write(content.replace("\n", "\r\n").replace("\t", "    "))
	
	content = AssembleDefaults(UserPrefs)
	open("tools/serini_test/SumatraPDF-user.ini", "wb").write(content.replace("\n", "\r\n"))

if __name__ == "__main__":
	main()
