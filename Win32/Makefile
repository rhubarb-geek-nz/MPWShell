CL=cl
OBJDIR=obj\$(VSCMD_ARG_TGT_ARCH)
BINDIR=bin\$(VSCMD_ARG_TGT_ARCH)
BUNDLE=bundle
RESFILE=$(OBJDIR)\mpwshell.res
APP=$(BINDIR)\mpwshell.exe
MSI=MPWShell-$(MPWSHELL_STR3)-$(VSCMD_ARG_TGT_ARCH).msi
MSIX=$(BUNDLE)\MPWShell-$(MPWSHELL_STR3)-$(VSCMD_ARG_TGT_ARCH).msix
MSIXBUNDLE=MPWShell-$(MPWSHELL_STR3).msixbundle
CHM=htmlhelp\mpwshell.chm
SRC=mpwshell.c

all: $(APP) $(MSI) $(MSIX)
	
clean: 
	if exist $(APP) del $(APP)
	if exist $(MSI) del $(MSI)
	if exist $(MSIX) del $(MSIX)
	if exist $(MSIXBUNDLE) del $(MSIXBUNDLE)
	if exist $(OBJDIR)\*.obj del $(OBJDIR)\*.obj
	if exist $(OBJDIR)\*.res del $(OBJDIR)\*.res
	if exist $(OBJDIR) rmdir $(OBJDIR)
	if exist $(BINDIR) rmdir $(BINDIR)

$(APP): $(SRC) $(OBJDIR) $(BINDIR) $(RESFILE)
	$(CL) 							\
		/nologo						\
		/Fe$@ 						\
		/Fo$(OBJDIR)\				\
		/W3 						\
		/WX 						\
		/MT 						\
		/DNDEBUG 					\
		/DUNICODE					\
		$(SRC) 						\
		/link						\
		/INCREMENTAL:NO				\
		/PDB:NONE					\
		/SUBSYSTEM:WINDOWS			\
		user32.lib					\
		gdi32.lib					\
		shell32.lib					\
		advapi32.lib				\
		htmlhelp.lib				\
		comdlg32.lib				\
		version.lib					\
		/VERSION:$(MPWSHELL_INT2)	\
		$(RESFILE)
	if exist "$(BINDIR)\mpwshell.exp" del "$(BINDIR)\mpwshell.exp"
	if exist "$(BINDIR)\mpwshell.lib" del "$(BINDIR)\mpwshell.lib"
	signtool sign /sha1 "$(CertificateThumbprint)" /fd SHA256 /t http://timestamp.digicert.com $@

$(RESFILE): mpwshell.unicode.rc
	rc /r $(RCFLAGS) "/DMPWSHELL_INT4=$(MPWSHELL_INT4)" "/DMPWSHELL_STR4=\"$(MPWSHELL_STR4)\"" /fo$@ mpwshell.unicode.rc

$(OBJDIR) $(BINDIR) $(BUNDLE):
	mkdir $@

$(MSI): $(APP) $(CHM) "mpwshell.wxs"
	"$(WIX)bin\candle.exe" -nologo \
			"mpwshell.wxs" \
			"-dMPWSHELL_STR4=$(MPWSHELL_STR4)" \
			"-dMPWSHELL_STR3=$(MPWSHELL_STR3)" \
			"-dMSIARCH=$(VSCMD_ARG_TGT_ARCH)" \
			"-dMSIUPGRADECODE=$(MSIUPGRADECODE)" \
			"-dMSIIS64BIT=$(MSIIS64BIT)" \
			"-dMSIINSTALLVERS=$(MSIINSTALLVERS)" \
			"-dMSIPROGFILES=$(MSIPROGFILES)" \
			"-dMSISHORTCUTID=$(MSISHORTCUTID)"
	"$(WIX)bin\light.exe" -nologo -cultures:null -out $@ "mpwshell.wixobj"
	del "mpwshell.wixobj"
	del "mpwshell-$(MPWSHELL_STR3)-$(VSCMD_ARG_TGT_ARCH).wixpdb"
	signtool sign /sha1 "$(CertificateThumbprint)" /fd SHA256 /t http://timestamp.digicert.com $@

$(MSIXBUNDLE):
	makeappx bundle /d $(BUNDLE) /p $@
	signtool sign /sha1 "$(BundleThumbprint)" /fd SHA256 /t http://timestamp.digicert.com $@

$(MSIX): $(BUNDLE) $(APP) $(CHM)
	COPY /Y $(APP)
	COPY /Y $(CHM)
	DIR mpwshell.exe
	DIR mpwshell.chm
	makeappx pack /m "AppxManifest-$(MPWSHELL_STR4)-$(VSCMD_ARG_TGT_ARCH).xml" /f mapping.ini /p $@
	DEL mpwshell.exe
	DEL mpwshell.chm
	signtool sign /sha1 "$(BundleThumbprint)" /fd SHA256 /t http://timestamp.digicert.com $@
