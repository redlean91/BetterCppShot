TARGET = BetterCppShot

CXX = g++
WINDRES = windres

# Fix Unicode + legacy compatibility flags
CXXFLAGS = -std=gnu++14 -D_WIN32_WINNT=0x0400 -D_WIN32_IE=0x0300 -DWINVER=0x0400 -D_UNICODE -DUNICODE -Isrc -O0 -fno-keep-inline-dllexport

LDFLAGS = -mwindows

LIBS = -lgdiplus -ladvapi32 -lgdi32 -luser32 -lkernel32 -lcomctl32 -lshell32 -lole32

# Detect architecture from compiler target (more reliable than OS variables)
# Detect architecture using compiler naming (Windows friendly)
ARCH = x86

ifeq ($(findstring x86_64,$(CXX)),x86_64)
    ARCH = x64
endif

ifeq ($(findstring 64,$(CXX)),64)
    ARCH = x64
endif

OUTPUT = $(TARGET)-$(ARCH).exe

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

all: $(OUTPUT)

# Resource compilation
res/resources.rc: res/resources.rc.in
	python -c "import sys; t=open('res/resources.rc.in').read(); t=t.replace('@CMAKE_SOURCE_DIR@', 'C:/Users/Redlean/Desktop/CppShot-master').replace('@PROJECT_VERSION_MAJOR@','1').replace('@PROJECT_VERSION_MINOR@','0').replace('@PROJECT_VERSION_PATCH@','0'); open('res/resources.rc','w').write(t)"

res/resources.res.o: res/resources.rc
	$(WINDRES) res/resources.rc -I res -I src -O coff -o res/resources.res.o

# Link step
$(OUTPUT): $(OBJECTS) res/resources.res.o
	$(CXX) $^ -o $@ $(LDFLAGS) $(LIBS)

# Compile step
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Cleanup
clean:
	del /Q src\*.o src\windows\*.o src\managers\*.o src\images\*.o src\ui\*.o res\*.res.o res\resources.rc $(TARGET)-*.exe 2>nul

.PHONY: all clean