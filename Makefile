CXX ?= g++
CXXFLAGS = -std=c++11 -Wall -Wextra $(shell fltk-config --cxxflags)
LDFLAGS = $(shell fltk-config --use-images --ldflags)

TARGET = flitpad
SRCS = main.cxx TextEditorApp.cxx FindReplaceDialog.cxx
OBJS = $(SRCS:.cxx=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cxx
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

appimage: all
	rm -rf AppDir
	mkdir -p AppDir/usr/bin AppDir/usr/share/applications AppDir/usr/share/icons/hicolor/32x32/apps
	cp flitpad AppDir/usr/bin/
	echo "[Desktop Entry]" > AppDir/usr/share/applications/flitpad.desktop
	echo "Name=Flitpad" >> AppDir/usr/share/applications/flitpad.desktop
	echo "Exec=flitpad" >> AppDir/usr/share/applications/flitpad.desktop
	echo "Type=Application" >> AppDir/usr/share/applications/flitpad.desktop
	echo "Categories=Utility;TextEditor;" >> AppDir/usr/share/applications/flitpad.desktop
	ln -sf usr/share/applications/flitpad.desktop AppDir/flitpad.desktop
	wget -nc https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
	chmod +x appimagetool-x86_64.AppImage
	ARCH=x86_64 ./appimagetool-x86_64.AppImage AppDir FLitpad-x86_64.AppImage

.PHONY: all clean
