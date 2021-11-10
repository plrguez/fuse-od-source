DEVICE = lepus
BUILD_TYPE = lepus
PACKAGE_TYPE = opk

all: buildfuse package

include ../Makefile.commonrules

$(TARGET): opk_package

deploy: $(BUILDDIR)/$(TARGET)
	scp $^ root@10.1.1.4:/media/sdcard/apps/
