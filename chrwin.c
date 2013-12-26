#!/usr/bin/env ccrun
# -xc -std=c99 -D_BSD_SOURCE -D_GNU_SOURCE -Wall -Wextra -pedantic -lX11

/*
Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
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

typedef struct {
	int fd;
	struct sockaddr_un addr;
} Conn;

static Conn ctrl = {
	.addr = {
		AF_UNIX,
		"/tmp/.X11-unix/X123"
	}
};
static Conn client;
static Conn server = {
	.addr = {
		AF_UNIX,
		"/tmp/.X11-unix/X0"
	}
};

static char buf[BUFSIZ * 8];

static size_t children = 0;

static CARD32 realroot = 0;

void sigchld(int sig) {
	UNUSED(sig);
	children--;
	if(children) {
		shutdown(ctrl.fd, SHUT_RDWR);
	}
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
		//cwin->parent = 0x2c00004;
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
	       setup->release, setup->nbytesVendor, vendor, root->windowId);
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

int runpipe() {
	int retval = EXIT_FAILURE;
	socklen_t len = sizeof(server.addr.sun_family) + strlen(server.addr.sun_path);
	server.fd = socket(server.addr.sun_family, SOCK_STREAM, 0);
	if(connect(server.fd, (struct sockaddr*)&server.addr, len) < 0)
		goto out_server;

	fd_set rfd, wfd;
	socklen_t clen = 0;
	socklen_t slen = 0;

	clen = read(client.fd, &buf, sizeof(buf));
	if(clen == 0)
		goto out_server;
	conn(clen);
	clen -= write(server.fd, &buf, clen);
	if(clen != 0)
		goto out_server;
	slen = read(server.fd, &buf, sizeof(buf));
	if(slen == 0)
		goto out_server;
	setup(slen);

	int nfds = MAX(client.fd, server.fd) + 1;
	for(;;) {
		FD_ZERO(&rfd);
		FD_ZERO(&wfd);

		FD_SET(client.fd, &rfd);
		FD_SET(server.fd, &rfd);
	
		FD_SET(client.fd, &wfd);
		FD_SET(server.fd, &wfd);

		if(select(nfds, &rfd, &wfd, NULL, NULL) < 0)
			goto out_client;
		if(FD_ISSET(client.fd, &rfd) && clen == 0) {
			clen = read(client.fd, &buf, sizeof(buf));
			if(clen == 0)
				break;
			request(clen);
		}
		if(FD_ISSET(server.fd, &wfd) && clen > 0) {
			clen -= write(server.fd, &buf, clen);
		}
		if(FD_ISSET(server.fd, &rfd) && slen == 0) {
			slen = read(server.fd, &buf, sizeof(buf));
			if(slen == 0)
				break;
			reply(slen);
		}
		if(FD_ISSET(client.fd, &wfd) && slen > 0) {
			slen -= write(client.fd, &buf, slen);
		}
	}
	
	retval = EXIT_SUCCESS;
out_server:
	close(server.fd);
out_client:
	close(client.fd);
	return retval;
}

int main(int argc, char *argv[]) {
	UNUSED(argc);
	UNUSED(argv);

	int retval = EXIT_FAILURE;
	socklen_t len;

	len = sizeof(ctrl.addr.sun_family) + strlen(ctrl.addr.sun_path);
	ctrl.fd = socket(ctrl.addr.sun_family, SOCK_STREAM, 0);
	unlink(ctrl.addr.sun_path);
	if(bind(ctrl.fd, (struct sockaddr*)&ctrl.addr, len) < 0) {
		puts("bind failed");
		goto out_ctrl;
	}

	listen(ctrl.fd, 4);
	puts("listen");

	struct sigaction act = {
		.sa_handler = sigchld,
		.sa_flags = SA_NOCLDSTOP
	};
	sigaction(SIGCHLD, &act, NULL);

	do {
		len = sizeof(client.addr);
		client.fd = accept(ctrl.fd, &client.addr, &len);
		if(client.fd == -1)
			continue;
		
		pid_t pid = fork();
		if(pid == -1) {
			if(client.fd)
				close(client.fd);
		} else if(pid == 0) {
			sigaction(SIGCHLD, NULL, NULL);
			close(ctrl.fd);
			puts("client connected");
			retval = runpipe();
			goto out;
		} else {
			children++;
		}
	} while(children);

	int status;
	do {
		status = wait(NULL);
	} while(status > 0);
	if(errno != ECHILD) {
		puts("error during wait()");
		goto out_ctrl;
	}

	retval = EXIT_SUCCESS;
out_ctrl:
	close(ctrl.fd);
out:
	puts("done");
	return retval;
}

