BASEDIR = ../..
BUILDDIR = build
TARGET = fuse-gcw0.opk
BUILD = fuse-gcw0
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

all: buildfuse opk

$(TARGET): $(BUILD) $(ICON) $(FONT) $(ROMS) $(LIBFILES) $(README)
	@mksquashfs \
	default.gcw0.desktop \
	system.gcw0.desktop \
	$(README) \
	$(BUILDDIR)/$(BUILD) \
	$(ROMSDIR) \
	$(BUILDDIR)/$(ICON) \
	$(UIDIR) \
	$(LIBDIR) \
        $(BUILDDIR)/$(TARGET) \
	-all-root -noappend -no-exports -no-xattrs

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

opk: $(TARGET)

buildfuse:
	cd $(BASEDIR) && $(MAKE)
	mkdir -p $(BUILDDIR)


.PHONY: clean deploy debug all libfiles
clean:
	cd $(BASEDIR) && $(MAKE) clean
	rm -rf $(BUILDDIR)

deploy: $(BUILDDIR)/$(TARGET)
	scp $^ root@10.1.1.2:/media/sdcard/apps/
