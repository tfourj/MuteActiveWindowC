QT += widgets network

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/core/main.cpp \
    src/core/mainwindow.cpp \
    src/config/config.cpp \
    src/utils/logger.cpp \
    src/audio/audio_muter.cpp \
    src/config/settings_manager.cpp \
    src/utils/process_selection_dialog.cpp \
    src/utils/theme_manager.cpp \
    src/utils/update_manager.cpp \
    src/utils/keyboard_hook.cpp

HEADERS += \
    src/core/mainwindow.h \
    src/config/config.h \
    src/utils/logger.h \
    src/audio/audio_muter.h \
    src/config/settings_manager.h \
    src/utils/process_selection_dialog.h \
    src/utils/theme_manager.h \
    src/utils/update_manager.h \
    src/utils/keyboard_hook.h

FORMS += \
    src/ui/mainwindow.ui

RESOURCES += \
    src/assets/maw.png

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Windows icon
win32 {
    RC_ICONS = src/assets/maw.ico
}

# Application name and version
TARGET = MuteActiveWindowC
VERSION = 1.3.2
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

# Auto-deploy after build (Release only)
win32:CONFIG(release, release|debug) {
    QMAKE_POST_LINK = $$(QTDIR)/bin/windeployqt.exe $$shell_quote($$OUT_PWD/release/$$shell_quote($$TARGET).exe) --no-compiler-runtime --no-translations --no-system-d3d-compiler --no-opengl-sw && \
    cmd /c "$$PWD\\clean_release.bat" "$$OUT_PWD\\release" && \
    cmd /c "$$PWD\\installer\\create_installer.bat" "$$OUT_PWD\\release" "$$shell_quote($$VERSION)"
}

# Windows executable properties
win32 {
    CONFIG += windows
}

INCLUDEPATH += src/core \
               src/audio \
               src/config \
               src/utils \
               src/ui
