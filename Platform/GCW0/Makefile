DEVICE = gcw0
BUILD_TYPE = od
PACKAGE_TYPE = opk

all: buildfuse package

include ../Makefile.commonrules

$(TARGET): opk_package

deploy: $(BUILDDIR)/$(TARGET)
	scp $^ root@10.1.1.2:/media/sdcard/apps/
