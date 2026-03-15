TARGET = BetterCppShot

CXX = g++
WINDRES = windres

# Directories
DIST = dist
LIBDIR = lib
DLLDIR = dlls

# Detect architecture automatically
ARCH = x86
CXX_ARCH_FLAGS = -m32
DLLTOOL_TARGET = i386

ifeq ($(findstring x86_64,$(CXX)),x86_64)
    ARCH = x64
    CXX_ARCH_FLAGS = -m64
    DLLTOOL_TARGET = x86_64
endif
ifeq ($(findstring 64,$(CXX)),64)
    ARCH = x64
    CXX_ARCH_FLAGS = -m64
    DLLTOOL_TARGET = x86_64
endif

OUTPUT = $(DIST)/$(TARGET)-$(ARCH).exe
UNICOWS_DLL = $(DLLDIR)\unicows-$(ARCH).dll
GDIPLUS_DLL = $(DLLDIR)\gdiplus-$(ARCH).dll
UNICOWS_LIB = $(LIBDIR)\libunicows.a

# Compiler and linker flags
CXXFLAGS = -std=gnu++14 -D_WIN32_WINNT=0x0400 -D_WIN32_IE=0x0300 -DWINVER=0x0400 \
           -D_UNICODE -DUNICODE -Isrc -O0 -fno-keep-inline-dllexport $(CXX_ARCH_FLAGS)
LDFLAGS = -mwindows $(CXX_ARCH_FLAGS)
LIBS = -L$(LIBDIR) -lunicows -lgdiplus -ladvapi32 -lgdi32 -luser32 -lkernel32 -lcomctl32 -lshell32 -lole32

# Source files
CPP_SOURCES = \
	src/main.cpp \
	src/Utils.cpp \
	src/images/CompositeScreenshot.cpp \
	src/images/Screenshot.cpp \
	src/managers/Application.cpp \
	src/ui/Button.cpp \
	src/ui/Node.cpp \
	src/windows/BackdropWindow.cpp \
	src/windows/MainWindow.cpp \
	src/windows/Window.cpp

OBJECTS = $(CPP_SOURCES:.cpp=.o)

# Build everything
all: folders $(UNICOWS_LIB) res/resources.res.o $(OUTPUT) copy-dlls

# Create folders
folders:
	if not exist $(DIST) mkdir $(DIST)
	if not exist $(LIBDIR) mkdir $(LIBDIR)

# Resource compilation
res/resources.rc: res/resources.rc.in
	python -c "import sys; t=open('res/resources.rc.in').read(); t=t.replace('@CMAKE_SOURCE_DIR@', 'C:/Users/Redlean/Desktop/CppShot-master').replace('@PROJECT_VERSION_MAJOR@','1').replace('@PROJECT_VERSION_MINOR@','0').replace('@PROJECT_VERSION_PATCH@','0'); open('res/resources.rc','w').write(t)"

# Compile resource object with correct architecture
res/resources.res.o: res/resources.rc
ifeq ($(ARCH),x86)
	$(WINDRES) --target=pe-i386 res/resources.rc -I res -I src -O coff -o res/resources.res.o
else
	$(WINDRES) --target=pe-x86-64 res/resources.rc -I res -I src -O coff -o res/resources.res.o
endif

# Build libunicows.a only if missing
$(UNICOWS_LIB):
	dlltool -D $(UNICOWS_DLL) -d unicows.def -l $(UNICOWS_LIB) --target=$(DLLTOOL_TARGET)

# Compile C++ files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link EXE
$(OUTPUT): $(OBJECTS) res/resources.res.o
	$(CXX) $^ -o $@ $(LDFLAGS) $(LIBS)

# Copy DLLs only for x86 builds
copy-dlls:
ifeq ($(ARCH),x86)
	@cmd /C "if not exist "$(DLLDIR)/unicows-x86.dll" echo Error: dlls/unicows-x86.dll not found & exit /b 1"
	@cmd /C "if not exist "$(DLLDIR)/gdiplus-x86.dll" echo Error: dlls/gdiplus-x86.dll not found & exit /b 1"
	@cmd /C copy /Y "$(DLLDIR)\unicows-x86.dll" "$(DIST)\unicows.dll"
	@cmd /C copy /Y "$(DLLDIR)\gdiplus-x86.dll" "$(DIST)\gdiplus.dll"
else
	@echo x64 build detected – skipping DLL copy
endif
	
# Cleanup
clean:
	del /Q src\*.o src\windows\*.o src\managers\*.o src\images\*.o src\ui\*.o res\*.res.o res\resources.rc $(DIST)\*.exe $(DIST)\unicows.dll $(DIST)\gdiplus.dll 2>nul

.PHONY: all clean folders copy-dlls