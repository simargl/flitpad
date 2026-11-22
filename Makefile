CXX      ?= g++
CXXFLAGS ?= -O2
PREFIX   ?= /usr/local

# -MMD/-MP: header changes trigger the right rebuilds
ALL_CXXFLAGS = -std=c++11 -Wall -Wextra -MMD -MP $(CXXFLAGS) $(shell fltk-config --cxxflags)
LDFLAGS_FLTK = $(shell fltk-config --use-images --ldflags)

TARGET = flitpad
SRCS   = main.cxx TextEditorApp.cxx FindReplaceDialog.cxx Highlighter.cxx EditHelpers.cxx PathUtil.cxx Settings.cxx IconFont.cxx
OBJS   = $(SRCS:.cxx=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS_FLTK)

%.o: %.cxx
	$(CXX) $(ALL_CXXFLAGS) -c $< -o $@

# Highlighter and EditHelpers don't depend on FLTK, so the tests build without it.
test: tests/test_highlight tests/test_paths tests/test_settings
	./tests/test_highlight
	./tests/test_paths
	./tests/test_settings

tests/test_highlight: tests/test_highlight.cxx Highlighter.cxx EditHelpers.cxx Highlighter.h EditHelpers.h
	$(CXX) -std=c++11 -Wall -Wextra $(CXXFLAGS) tests/test_highlight.cxx Highlighter.cxx EditHelpers.cxx -o $@

tests/test_paths: tests/test_paths.cxx PathUtil.cxx PathUtil.h
	$(CXX) -std=c++11 -Wall -Wextra $(CXXFLAGS) tests/test_paths.cxx PathUtil.cxx -o $@

tests/test_settings: tests/test_settings.cxx Settings.cxx IconFont.cxx Settings.h IconFont.h ColorMath.h
	$(CXX) -std=c++11 -Wall -Wextra $(CXXFLAGS) tests/test_settings.cxx Settings.cxx IconFont.cxx -o $@

# ./tests/show_styles <Language> <file> [--runs]  -- prints the highlighter's output
tests/show_styles: tests/show_styles.cxx Highlighter.cxx Highlighter.h
	$(CXX) -std=c++11 -Wall -Wextra $(CXXFLAGS) tests/show_styles.cxx Highlighter.cxx -o $@

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	install -Dm644 flitpad.png $(DESTDIR)$(PREFIX)/share/icons/hicolor/64x64/apps/flitpad.png

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(TARGET) tests/test_highlight tests/test_paths tests/test_settings tests/show_styles

appimage: all
	rm -rf AppDir
	mkdir -p AppDir/usr/bin AppDir/usr/share/applications AppDir/usr/share/icons/hicolor/64x64/apps
	cp flitpad AppDir/usr/bin/
	cp flitpad.png AppDir/usr/share/icons/hicolor/64x64/apps/flitpad.png
	cp flitpad.png AppDir/flitpad.png

	echo '#!/bin/sh' > AppDir/AppRun
	echo 'SELF="$$(readlink -f "$$0")"' >> AppDir/AppRun
	echo 'HERE="$$(dirname "$$SELF")"' >> AppDir/AppRun
	echo 'exec "$$HERE/usr/bin/flitpad" "$$@"' >> AppDir/AppRun
	chmod +x AppDir/AppRun

	echo "[Desktop Entry]" > AppDir/usr/share/applications/flitpad.desktop
	echo "Name=Flitpad" >> AppDir/usr/share/applications/flitpad.desktop
	echo "Exec=flitpad" >> AppDir/usr/share/applications/flitpad.desktop
	echo "Icon=flitpad" >> AppDir/usr/share/applications/flitpad.desktop
	echo "Type=Application" >> AppDir/usr/share/applications/flitpad.desktop
	echo "Categories=Utility;TextEditor;" >> AppDir/usr/share/applications/flitpad.desktop
	ln -sf usr/share/applications/flitpad.desktop AppDir/flitpad.desktop

	wget -nc https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
	chmod +x appimagetool-x86_64.AppImage

	# Extract appimagetool to bypass FUSE requirements inside Docker
	./appimagetool-x86_64.AppImage --appimage-extract

	# Run the extracted tool
	ARCH=x86_64 ./squashfs-root/AppRun AppDir FLitpad-x86_64.AppImage

-include $(OBJS:.o=.d)

.PHONY: all clean test install appimage
