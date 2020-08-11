BASEDIR = ../..
BUILDDIR = build
TARGET = fuse.ipk
BUILD = fuse-retrofw
ICON = fuse.png
README = ../readme.txt
UIDIR = $(BUILDDIR)/ui
FONT = fuse.font
LIBDIR = $(BUILDDIR)/lib
LIBFILES = $(LIBDIR)/cassette.bmp $(LIBDIR)/microdrive.bmp $(LIBDIR)/plus3disk.bmp $(LIBDIR)/keyboard.png $(LIBDIR)/keyboard.scr
ROMSDIR = $(BUILDDIR)/roms
BASEROMSDIR = $(BASEDIR)/roms
EXTRAROMSDIR = ../roms
ROMS = $(ROMSDIR)/*.rom

all: buildfuse ipk

$(TARGET): $(BUILD) $(ICON) $(FONT) $(ROMS) $(LIBFILES) $(README)
	@rm -rf /tmp/.fuse-ipk/ && mkdir -p /tmp/.fuse-ipk/root/home/retrofw/emus/fuse /tmp/.fuse-ipk/root/home/retrofw/apps/gmenu2x/sections/emulators
	@cp -r $(BUILDDIR)/* $(README) /tmp/.fuse-ipk/root/home/retrofw/emus/fuse
	@cp fuse.lnk /tmp/.fuse-ipk/root/home/retrofw/apps/gmenu2x/sections/emulators
	@sed "s/^Version:.*/Version: $$(date +%Y%m%d)/" control > /tmp/.fuse-ipk/control
	@cp conffiles postinst /tmp/.fuse-ipk/
	@tar --owner=0 --group=0 -czvf /tmp/.fuse-ipk/control.tar.gz -C /tmp/.fuse-ipk/ control conffiles
	@tar --owner=0 --group=0 -czvf /tmp/.fuse-ipk/data.tar.gz -C /tmp/.fuse-ipk/root/ .
	@echo 2.0 > /tmp/.fuse-ipk/debian-binary
	@ar r $(BUILDDIR)/$(TARGET) /tmp/.fuse-ipk/control.tar.gz /tmp/.fuse-ipk/data.tar.gz /tmp/.fuse-ipk/debian-binary

$(BUILD): $(BASEDIR)/fuse
	cp $^ $(BUILDDIR)/$@

$(ICON): $(BASEDIR)/data/icons/32x32/fuse.png
	cp $^ $(BUILDDIR)/$@

$(ROMS): $(BASEDIR)/roms/*.rom
	mkdir -p $(ROMSDIR)
	cp $(BASEROMSDIR)/*.rom $(ROMSDIR)
	-cp $(EXTRAROMSDIR)/*.rom $(ROMSDIR)

$(FONT): $(BASEDIR)/ui/widget/fuse.font
	mkdir -p $(UIDIR)/widget
	cp $^ $(UIDIR)/widget/$@

$(LIBFILES): libfiles

libfiles: $(BASEDIR)/lib/cassette.bmp $(BASEDIR)/lib/microdrive.bmp $(BASEDIR)/lib/plus3disk.bmp $(BASEDIR)/lib/keyboard.png $(BASEDIR)/lib/keyboard.scr
	mkdir -p $(LIBDIR)
	cp $^ $(LIBDIR)

ipk: $(TARGET)

buildfuse:
	cd $(BASEDIR) && $(MAKE)
	mkdir -p $(BUILDDIR)


.PHONY: clean deploy debug all libfiles
clean:
	cd $(BASEDIR) && $(MAKE) clean
	rm -rf $(BUILDDIR)

deploy: $(BUILDDIR)/$(TARGET)
	echo -e "user root\n"\
	        "cd /media/mmcblk1p1/apps\n"\
	        "lcd $(BUILDDIR)\n"\
	        "put $(TARGET)\n"\
	        "bye\n" > deploy_fuse.ftp
	ftp -inv 169.254.1.1 < deploy_fuse.ftp
	rm deploy_fuse.ftp