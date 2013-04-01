// DON'T EDIT MANUALLY !!!!
// auto-generated by gen_settings_txt.py !!!!

#ifndef SettingsTxtSumatra_h
#define SettingsTxtSumatra_h

namespace sertxt {

struct RectInt {
    const StructMetadata *  def;
    int32_t                 x;
    int32_t                 y;
    int32_t                 dx;
    int32_t                 dy;
};

struct BasicSettings {
    const StructMetadata *  def;
    bool                    globalPrefsOnly;
    const char *            currLanguage;
    bool                    toolbarVisible;
    bool                    pdfAssociateDontAsk;
    bool                    pdfAssociateDoIt;
    bool                    checkForUpdates;
    bool                    rememberMruFiles;
    bool                    useSystemColorScheme;
    const WCHAR *           inverseSearchCmdLine;
    const char *            versionToSkip;
    const char *            lastUpdateTime;
    uint16_t                defaultDisplayMode;
    float                   defaultZoom;
    int32_t                 windowState;
    RectInt *               windowPos;
    bool                    tocVisible;
    bool                    favVisible;
    int32_t                 sidebarDx;
    int32_t                 tocDy;
    bool                    showStartPage;
    int32_t                 openCountWeek;
    uint64_t                lastPrefUpdate;
};

struct PaddingSettings {
    const StructMetadata *  def;
    uint16_t                top;
    uint16_t                bottom;
    uint16_t                left;
    uint16_t                right;
    uint16_t                spaceX;
    uint16_t                spaceY;
};

struct ForwardSearch {
    const StructMetadata *  def;
    int32_t                 highlightOffset;
    int32_t                 highlightWidth;
    int32_t                 highlightPermanent;
    uint32_t                highlightColor;
    bool                    enableTexEnhancements;
};

struct AdvancedSettings {
    const StructMetadata *  def;
    bool                    traditionalEbookUI;
    bool                    escToExit;
    uint32_t                textColor;
    uint32_t                pageColor;
    uint32_t                mainWindowBackground;
    PaddingSettings *       pagePadding;
    ForwardSearch *         forwardSearch;
};

struct Fav {
    const StructMetadata *  def;
    const char *            name;
    int32_t                 pageNo;
    const char *            pageLabel;
    int32_t                 menuId;
};

struct AppState {
    const StructMetadata *  def;
    ListNode<Fav> *         favorites;
};

struct Settings {
    const StructMetadata *  def;
    BasicSettings *         basic;
    AdvancedSettings *      advanced;
    AppState *              appState;
    const char *            str_escape_test;
    const WCHAR *           wstr_1;
};

Settings *DeserializeSettings(const char *data, size_t dataLen);
Settings *DeserializeSettingsWithDefault(const char *data, size_t dataLen, const char *defaultData, size_t defaultDataLen);
uint8_t *SerializeSettings(Settings *, size_t *dataLenOut);
void FreeSettings(Settings *);

} // namespace sertxt
#endif
