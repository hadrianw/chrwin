/*
Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xproto.h>

#define LENGTH(X) (sizeof X / sizeof X[0])
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define PAD(e)    ((4 - ((e) % 4)) % 4)
#define UNUSED(X) (void)(X)

#define SOCKPATH "/tmp/.X11-unix/X"

static char buf[BUFSIZ * 8];
static size_t children = 0;

static CARD32 parent_xid;
static CARD32 realroot = 0;
// TODO: generate display number
static char fakedisplay[] = ":123";
static int ctl = -1;

void sigchld(int sig) {
	UNUSED(sig);
	children--;
	if(children) {
		shutdown(ctl, SHUT_RDWR);
	}
}

int
chldwait() {
	int status;
	do {
		status = wait(NULL);
	} while(status > 0);
	if(errno != ECHILD) {
		perror("chrwin: wait failed");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

static char *reqname[] = {
	[1] = "X_CreateWindow",
	[2] = "X_ChangeWindowAttributes",
	[3] = "X_GetWindowAttributes",
	[4] = "X_DestroyWindow",
	[5] = "X_DestroySubwindows",
	[6] = "X_ChangeSaveSet",
	[7] = "X_ReparentWindow",
	[8] = "X_MapWindow",
	[9] = "X_MapSubwindows",
	[10] = "X_UnmapWindow",
	[11] = "X_UnmapSubwindows",
	[12] = "X_ConfigureWindow",
	[13] = "X_CirculateWindow",
	[14] = "X_GetGeometry",
	[15] = "X_QueryTree",
	[16] = "X_InternAtom",
	[17] = "X_GetAtomName",
	[18] = "X_ChangeProperty",
	[19] = "X_DeleteProperty",
	[20] = "X_GetProperty",
	[21] = "X_ListProperties",
	[22] = "X_SetSelectionOwner",
	[23] = "X_GetSelectionOwner",
	[24] = "X_ConvertSelection",
	[25] = "X_SendEvent",
	[26] = "X_GrabPointer",
	[27] = "X_UngrabPointer",
	[28] = "X_GrabButton",
	[29] = "X_UngrabButton",
	[30] = "X_ChangeActivePointerGrab",
	[31] = "X_GrabKeyboard",
	[32] = "X_UngrabKeyboard",
	[33] = "X_GrabKey",
	[34] = "X_UngrabKey",
	[35] = "X_AllowEvents",
	[36] = "X_GrabServer",
	[37] = "X_UngrabServer",
	[38] = "X_QueryPointer",
	[39] = "X_GetMotionEvents",
	[40] = "X_TranslateCoords",
	[41] = "X_WarpPointer",
	[42] = "X_SetInputFocus",
	[43] = "X_GetInputFocus",
	[44] = "X_QueryKeymap",
	[45] = "X_OpenFont",
	[46] = "X_CloseFont",
	[47] = "X_QueryFont",
	[48] = "X_QueryTextExtents",
	[49] = "X_ListFonts",
	[50] = "X_ListFontsWithInfo",
	[51] = "X_SetFontPath",
	[52] = "X_GetFontPath",
	[53] = "X_CreatePixmap",
	[54] = "X_FreePixmap",
	[55] = "X_CreateGC",
	[56] = "X_ChangeGC",
	[57] = "X_CopyGC",
	[58] = "X_SetDashes",
	[59] = "X_SetClipRectangles",
	[60] = "X_FreeGC",
	[61] = "X_ClearArea",
	[62] = "X_CopyArea",
	[63] = "X_CopyPlane",
	[64] = "X_PolyPoint",
	[65] = "X_PolyLine",
	[66] = "X_PolySegment",
	[67] = "X_PolyRectangle",
	[68] = "X_PolyArc",
	[69] = "X_FillPoly",
	[70] = "X_PolyFillRectangle",
	[71] = "X_PolyFillArc",
	[72] = "X_PutImage",
	[73] = "X_GetImage",
	[74] = "X_PolyText8",
	[75] = "X_PolyText16",
	[76] = "X_ImageText8",
	[77] = "X_ImageText16",
	[78] = "X_CreateColormap",
	[79] = "X_FreeColormap",
	[80] = "X_CopyColormapAndFree",
	[81] = "X_InstallColormap",
	[82] = "X_UninstallColormap",
	[83] = "X_ListInstalledColormaps",
	[84] = "X_AllocColor",
	[85] = "X_AllocNamedColor",
	[86] = "X_AllocColorCells",
	[87] = "X_AllocColorPlanes",
	[88] = "X_FreeColors",
	[89] = "X_StoreColors",
	[90] = "X_StoreNamedColor",
	[91] = "X_QueryColors",
	[92] = "X_LookupColor",
	[93] = "X_CreateCursor",
	[94] = "X_CreateGlyphCursor",
	[95] = "X_FreeCursor",
	[96] = "X_RecolorCursor",
	[97] = "X_QueryBestSize",
	[98] = "X_QueryExtension",
	[99] = "X_ListExtensions",
	[100] = "X_ChangeKeyboardMapping",
	[101] = "X_GetKeyboardMapping",
	[102] = "X_ChangeKeyboardControl",
	[103] = "X_GetKeyboardControl",
	[104] = "X_Bell",
	[105] = "X_ChangePointerControl",
	[106] = "X_GetPointerControl",
	[107] = "X_SetScreenSaver",
	[108] = "X_GetScreenSaver",
	[109] = "X_ChangeHosts",
	[110] = "X_ListHosts",
	[111] = "X_SetAccessControl",
	[112] = "X_SetCloseDownMode",
	[113] = "X_KillClient",
	[114] = "X_RotateProperties",
	[115] = "X_ForceScreenSaver",
	[116] = "X_SetPointerMapping",
	[117] = "X_GetPointerMapping",
	[118] = "X_SetModifierMapping",
	[119] = "X_GetModifierMapping",
	[127] = "X_NoOperation",
};

static char *rplname[] = {
	[0] = "X_Error",		/* Error */
	[1] = "X_Reply",		/* Normal reply */
	[2] = "KeyPress",
	[3] = "KeyRelease",
	[4] = "ButtonPress",
	[5] = "ButtonRelease",
	[6] = "MotionNotify",
	[7] = "EnterNotify",
	[8] = "LeaveNotify",
	[9] = "FocusIn",
	[10] = "FocusOut",
	[11] = "KeymapNotify",
	[12] = "Expose",
	[13] = "GraphicsExpose",
	[14] = "NoExpose",
	[15] = "VisibilityNotify",
	[16] = "CreateNotify",
	[17] = "DestroyNotify",
	[18] = "UnmapNotify",
	[19] = "MapNotify",
	[20] = "MapRequest",
	[21] = "ReparentNotify",
	[22] = "ConfigureNotify",
	[23] = "ConfigureRequest",
	[24] = "GravityNotify",
	[25] = "ResizeRequest",
	[26] = "CirculateNotify",
	[27] = "CirculateRequest",
	[28] = "PropertyNotify",
	[29] = "SelectionClear",
	[30] = "SelectionRequest",
	[31] = "SelectionNotify",
	[32] = "ColormapNotify",
	[33] = "ClientMessage",
	[34] = "MappingNotify",
	[35] = "GenericEvent",
};

void request(socklen_t len) {
	printf("%6d ", len);

	xReq *req = (xReq*)buf;
	if(req->reqType < LENGTH(reqname) && reqname[req->reqType])
		fputs(reqname[req->reqType], stdout);
	else
		printf("UnknownRequest %d", req->reqType);

	switch(req->reqType) {
	case X_CreateWindow:
	{
		xCreateWindowReq *cwin = (xCreateWindowReq*)buf;
		if(cwin->parent != realroot)
			break;
		cwin->parent = parent_xid;
		break;
	}
	case X_QueryExtension:
	{
		xQueryExtensionReq *qext = (xQueryExtensionReq*)buf;
		printf(" %*s", qext->nbytes, (char*)&qext[1]);
		break;
	}
	default:
		break;
	}
	puts("");
}

void conn(socklen_t len) {
	printf("%6d ", len);

	xConnClientPrefix *prefix = (xConnClientPrefix*)buf;
	char *proto = (char*)&prefix[1];
	char *str = (char*)&proto[prefix->nbytesAuthProto +
	                          PAD(prefix->nbytesAuthProto)];
	printf("ConnClient %*s - ", prefix->nbytesAuthProto, proto);
	for(size_t i = 0; i < prefix->nbytesAuthString; i++)
		printf("%hhx", str[i]);
	puts("");
}

void setup(socklen_t len) {
	printf("%6d ", len);

	xConnSetupPrefix *prefix = (xConnSetupPrefix*)buf;
	switch(prefix->success) {
	case 2:
		puts("authenticate");
		return;
	case 1:
		break;
	case 0:
		puts("fail");
		return;
	default:
		puts("unknown fail");
		return;
	}
	xConnSetup *setup = (xConnSetup*)&prefix[1];
	char *vendor = (char*)&setup[1];
	xWindowRoot *root = (xWindowRoot*)&vendor[setup->nbytesVendor +
	                                          PAD(setup->nbytesVendor) +
						  sz_xPixmapFormat * setup->numFormats];
	printf("ConnSetup %d %*s 0x%x\n",
	       (int)setup->release, setup->nbytesVendor, vendor, (unsigned int)root->windowId);
	realroot = root->windowId;
}

void reply(socklen_t len) {
	printf("%6d ", len);

	xReply *rpl = (xReply*)buf;
	if(rpl->generic.type < LENGTH(rplname) && rplname[rpl->generic.type])
		puts(rplname[rpl->generic.type]);
	else
		printf("UnknownReply %d\n", rpl->generic.type);
}

int xconn(int c, int s) {
	socklen_t len;
	len = read(c, &buf, sizeof(buf));
	if(len == 0)
		return -1;
	conn(len);
	len -= write(s, &buf, len);
	if(len != 0)
		return -1;
	len = read(s, &buf, sizeof(buf));
	if(len == 0)
		return -1;
	setup(len);
	len -= write(c, &buf, len);

	return 0;
}

socklen_t
set_dpy(struct sockaddr_un *addr, char *dpy) {
	size_t siz = strlen(addr->sun_path);
	size_t dpysiz;
	if(!dpy || dpy[0] != ':' ||
	   siz + (dpysiz = strlen(++dpy) + 1) > sizeof(addr->sun_path)) {
		return 0;
	}
	memcpy(addr->sun_path + siz, dpy, dpysiz);
	return sizeof(addr->sun_family) + siz + dpysiz;
}

int
runpipe(int c) {
	int retval = EXIT_FAILURE;
	int s;
	struct sockaddr_un addr = {AF_UNIX, SOCKPATH};
	socklen_t len = set_dpy(&addr, getenv("DISPLAY"));
	if(!len) {
		fputs("chrwin: DISPLAY is not set properly\n", stderr);
		return EXIT_FAILURE;
	}
	puts(addr.sun_path);

	s = socket(addr.sun_family, SOCK_STREAM, 0);
	if(s < 0) {
		perror("chrwin: socket failed");
		goto out_c;
	}
	if(connect(s, (struct sockaddr*)&addr, len) < 0) {
		perror("chrwin: connect failed");
		goto out_s;
	}
	if(xconn(c, s) < 0)
		goto out_s;

	socklen_t clen = 0;
	socklen_t slen = 0;
	fd_set rfd;
	int nfds = MAX(c, s) + 1;
	for(;;) {
		FD_ZERO(&rfd);
		FD_SET(c, &rfd);
		FD_SET(s, &rfd);

		if(pselect(nfds, &rfd, 0, 0, 0, 0) < 0) {
			if(errno == EINTR) {
				continue;
			}
			perror("chrwin: select failed");
			goto out_s;
		}
		if(FD_ISSET(c, &rfd)) {
			clen = read(c, &buf, sizeof(buf));
			if(clen == 0)
				break;
			request(clen);

			clen -= write(s, &buf, clen);
		}
		if(FD_ISSET(s, &rfd)) {
			slen = read(s, &buf, sizeof(buf));
			if(slen == 0)
				break;
			reply(slen);

			slen -= write(c, &buf, slen);
		}
	}
	
	retval = EXIT_SUCCESS;
out_s:
	close(s);
out_c:
	close(c);
	return retval;
}

int
ctlconn(int fd, struct sockaddr_un *addr) {
	//TODO: generate display number
	socklen_t len = set_dpy(addr, fakedisplay);

	puts(addr->sun_path);
	if(bind(fd, (struct sockaddr*)addr, len) < 0) {
		perror("chrwin: bind failed");
		return -1;
	}

	if(listen(fd, 4) < 0) {
		perror("chrwin: listen failed");
		return -1;
	}

	if(sigaction(SIGCHLD, &(struct sigaction){
	             .sa_handler = sigchld, .sa_flags = SA_NOCLDSTOP},
		     NULL) < 0) {
		perror("chrwin: sigaction failed");
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[]) {
	UNUSED(argc);
	UNUSED(argv);

	int retval = EXIT_FAILURE;

	if(argc < 2) {
		fputs("chrwin: usage: chrwin PARENT_XID [cmd [args]]\n", stderr);
		return EXIT_FAILURE;
	}
	parent_xid = strtol(argv[1], 0, 16);

	struct sockaddr_un ctladdr = {AF_UNIX, SOCKPATH};
	int ctl = socket(ctladdr.sun_family, SOCK_STREAM, 0);
	if(ctl < 0) {
		perror("chrwin: socket failed");
		goto out;
	}
	if(ctlconn(ctl, &ctladdr) < 0)
		goto out_ctl;

	if(argc > 2) {
		pid_t pid = fork();
		if(pid == 0) {
			argv += 2;
			setenv("DISPLAY", fakedisplay, 1);
			execvp(argv[0], argv);
			perror("chrwin: execvp failed");
		} else if(pid == -1) {
			perror("chrwin: fork failed");
		}
	}

	do {
		int cfd = accept(ctl, NULL, NULL);
		if(cfd == -1) {
			perror("accept failed");
			continue;
		}
		
		pid_t pid = fork();
		if(pid == 0) {
			sigaction(SIGCHLD, NULL, NULL);
			close(ctl);
			return runpipe(cfd);
		} else {
			close(cfd);
			if(pid == -1) {
				perror("fork failed");
				continue;
			} else
				children++;
		}
	} while(children);

	retval = chldwait();
out_ctl:
	close(ctl);
	unlink(ctladdr.sun_path);
out:
	return retval;
}

