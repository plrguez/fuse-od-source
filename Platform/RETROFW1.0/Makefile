DEVICE = retrofw
BUILD_TYPE = retrofw
PACKAGE_TYPE = ipk

all: buildfuse package

include ../Makefile.commonrules

$(TARGET): ipk_package

deploy: $(BUILDDIR)/$(TARGET)
	echo -e "user root\n"\
	"cd /media/mmcblk1p1/apps\n"\
	"lcd $(BUILDDIR)\n"\
	"put $(TARGET)\n"\
	"bye\n" > deploy_fuse.ftp
	ftp -inv 169.254.1.1 < deploy_fuse.ftp
	rm deploy_fuse.ftp