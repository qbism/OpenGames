#include "quakedef.h"

#ifdef WEBCLIENT
#define DOWNLOADMENU
#endif

#ifdef DOWNLOADMENU

#define ROOTDOWNLOADABLESSOURCE "http://fteqw.com/downloadables.txt"
#define INSTALLEDFILES	"installed.lst"	//the file that resides in the quakedir (saying what's installed).

#define DPF_HAVEAVERSION	1	//any old version
#define DPF_WANTTOINSTALL	2	//user selected it
#define DPF_DISPLAYVERSION	4	//some sort of conflict, the package is listed twice, so show versions so the user knows what's old.
#define DPF_DELETEONUNINSTALL 8	//for previously installed packages, remove them from the list
#define DPF_DOWNLOADING 16
#define DPF_ENQUED	32

void CL_StartCinematicOrMenu(void);

int dlcount=1;

//note: these are allocated for the life of the exe
char *downloadablelist[256] = {
	ROOTDOWNLOADABLESSOURCE
};
char *downloadablelistnameprefix[256] = {
	""
};
char downloadablelistreceived[256];	//well
int numdownloadablelists = 1;

typedef struct package_s {
	char fullname[256];
	char *name;

	char src[256];
	char dest[64];
	char gamedir[16];
	unsigned int version;	//integral.

	int dlnum;

	int flags;
	struct package_s *next;
} package_t;

typedef struct {
	menucustom_t *list;
	char intermediatefilename[MAX_QPATH];
	char pathprefix[MAX_QPATH];
	int parsedsourcenum;
	qboolean populated;
} dlmenu_t;

package_t *availablepackages;
int numpackages;

static package_t *BuildPackageList(vfsfile_t *f, int flags, char *prefix)
{
	char line[1024];
	package_t *p;
	package_t *first = NULL;
	char *sl;

	int version;

	do
	{
		if (!VFS_GETS(f, line, sizeof(line)-1))
			break;
		while((sl=strchr(line, '\n')))
			*sl = '\0';
		while((sl=strchr(line, '\r')))
			*sl = '\0';
		Cmd_TokenizeString (line, false, false);
	} while (!Cmd_Argc());

	if (strcmp(Cmd_Argv(0), "version"))
		return NULL;	//it's not the right format.

	version = atoi(Cmd_Argv(1));
	if (version != 0 && version != 1)
	{
		Con_Printf("Packagelist is of a future or incompatible version\n");
		return NULL;	//it's not the right version.
	}

	while(1)
	{
		if (!VFS_GETS(f, line, sizeof(line)-1))
			break;
		while((sl=strchr(line, '\n')))
			*sl = '\0';
		while((sl=strchr(line, '\r')))
			*sl = '\0';
		Cmd_TokenizeString (line, false, false);
		if (Cmd_Argc())
		{
			if (!strcmp(Cmd_Argv(0), "sublist"))
			{
				int i;
				sl = Cmd_Argv(1);

				for (i = 0; i < numdownloadablelists; i++)
				{
					if (!strcmp(downloadablelist[i], sl))
						break;
				}
				if (i == numdownloadablelists && i != 256)
				{
					downloadablelist[i] = BZ_Malloc(strlen(sl)+1);
					strcpy(downloadablelist[i], sl);

					if (*prefix)
						sl = va("%s/%s", prefix, Cmd_Argv(2));
					else
						sl = Cmd_Argv(2);
					downloadablelistnameprefix[i] = BZ_Malloc(strlen(sl)+1);
					strcpy(downloadablelistnameprefix[i], sl);

					numdownloadablelists++;

					i++;
				}
				continue;
			}

			if (Cmd_Argc() > 5 || Cmd_Argc() < 3)
			{
				Con_Printf("Package list is bad - %s\n", line);
				continue;	//but try the next line away
			}

			p = BZ_Malloc(sizeof(*p));

			if (*prefix)
				Q_strncpyz(p->fullname, va("%s/%s", prefix, Cmd_Argv(0)), sizeof(p->fullname));
			else
				Q_strncpyz(p->fullname, Cmd_Argv(0), sizeof(p->fullname));
			p->name = p->fullname;
			while((sl = strchr(p->name, '/')))
				p->name = sl+1;

			Q_strncpyz(p->src, Cmd_Argv(1), sizeof(p->src));
			Q_strncpyz(p->dest, Cmd_Argv(2), sizeof(p->dest));
			p->version = atoi(Cmd_Argv(3));
			Q_strncpyz(p->gamedir, Cmd_Argv(4), sizeof(p->gamedir));
			if (!*p->gamedir)
				strcpy(p->gamedir, "id1");

			p->flags = flags;

			p->next = first;
			first = p;
		}
	}

	return first;
}

static void WriteInstalledPackages(void)
{
	char *s;
	package_t *p;
	vfsfile_t *f = FS_OpenVFS(INSTALLEDFILES, "wb", FS_ROOT);
	if (!f)
	{
		Con_Printf("menu_download: Can't update installed list\n");
		return;
	}

	s = "version 1\n";
	VFS_WRITE(f, s, strlen(s));
	for (p = availablepackages; p ; p=p->next)
	{
		if (p->flags & DPF_HAVEAVERSION)
		{
			s = va("\"%s\" \"%s\" \"%s\" %i \"%s\"\n", p->fullname, p->src, p->dest, p->version, p->gamedir);
			VFS_WRITE(f, s, strlen(s));
		}
	}

	VFS_CLOSE(f);
}

static qboolean ComparePackages(package_t **l, package_t *p)
{
	int v = strcmp((*l)->fullname, p->fullname);
	if (v < 0)
	{
		p->next = (*l);
		(*l) = p;
		return true;
	}
	else if (v == 0)
	{
		if (p->version == (*l)->version)
		if (!strcmp((*l)->dest, p->dest))
		{ /*package matches, free, don't add*/
			strcpy((*l)->src, p->src);	//use the source of the new package (existing packages are read FIRST)
			(*l)->flags |= p->flags;
			(*l)->flags &= ~DPF_DELETEONUNINSTALL;
			BZ_Free(p);
			return true;
		}

		p->flags |= DPF_DISPLAYVERSION;
		(*l)->flags |= DPF_DISPLAYVERSION;
	}
	return false;
}

static void InsertPackage(package_t **l, package_t *p)
{
	package_t *lp;
	if (!*l)	//there IS no list.
	{
		*l = p;
		p->next = NULL;
		return;
	}
	if (ComparePackages(l, p))
		return;
	for (lp = *l; lp->next; lp=lp->next)
	{
		if (ComparePackages(&lp->next, p))
			return;
	}
	lp->next = p;
	p->next = NULL;
}
static void ConcatPackageLists(package_t *l2)
{
	package_t *n;
	while(l2)
	{
		n = l2->next;
		InsertPackage(&availablepackages, l2);
		l2 = n;

		numpackages++;
	}
}

static void dlnotification(struct dl_download *dl)
{
	int i;
	vfsfile_t *f;
	f = dl->file;
	dl->file = NULL;

	i = dl->user_num;

	if (f)
	{
		downloadablelistreceived[i] = 1;
		ConcatPackageLists(BuildPackageList(f, 0, downloadablelistnameprefix[i]));
		VFS_CLOSE(f);
	}
	else
		downloadablelistreceived[i] = -1;
}

static void MD_Draw (int x, int y, struct menucustom_s *c, struct menu_s *m)
{
	package_t *p;
	int fl;
	p = c->dptr;
	if (p)
	{
		Draw_FunString (x+4, y, "^Ue080^Ue082");

		fl = p->flags & (DPF_HAVEAVERSION | DPF_WANTTOINSTALL);
		if ((p->flags & (DPF_DOWNLOADING|DPF_ENQUED)) && ((int)(realtime*4)&1))
			fl |= DPF_HAVEAVERSION;	//flicker have if we're downloading it.
		switch(fl)
		{
		case 0:
			Draw_FunString (x+8, y, "^Ue081");
			break;
		case DPF_HAVEAVERSION:
			Draw_FunString (x, y, "REM");
			break;
		case DPF_WANTTOINSTALL:
			Draw_FunString (x, y, "GET");
			break;
		case DPF_HAVEAVERSION | DPF_WANTTOINSTALL:
			Draw_FunString (x+8, y, "^Ue083");
			break;
		}

		if (&m->selecteditem->common == &c->common)
			Draw_AltFunString (x+48, y, p->name);
		else
			Draw_FunString(x+48, y, p->name);

		if (p->flags & DPF_DISPLAYVERSION)
		{
			Draw_FunString(x+48+strlen(p->name)*8, y, va(" (%i.%i)", p->version/1000, p->version%1000));
		}
	}
}
static qboolean MD_Key (struct menucustom_s *c, struct menu_s *m, int key)
{
	package_t *p, *p2;
	p = c->dptr;
	if (key == K_ENTER || key == K_MOUSE1)
	{
		p->flags ^= DPF_WANTTOINSTALL;

		if (p->flags&DPF_WANTTOINSTALL)
		{
			for (p2 = availablepackages; p2; p2 = p2->next)
			{
				if (p == p2)
					continue;
				if (!strcmp(p->dest, p2->dest))
					p2->flags &= ~DPF_WANTTOINSTALL;
			}
		}
		else
			p->flags &= ~DPF_ENQUED;
		return true;
	}

	return false;
}

qboolean MD_PopMenu (union menuoption_s *mo,struct menu_s *m,int key)
{
	if (key == K_ENTER || key == K_MOUSE1)
	{
		M_RemoveMenu(m);
		return true;
	}
	return false;
}

static void Menu_Download_Got(struct dl_download *dl);
qboolean MD_ApplyDownloads (union menuoption_s *mo,struct menu_s *m,int key)
{
	if (key == K_ENTER || key == K_MOUSE1)
	{
		char *temp;
		package_t *last = NULL, *p;

		for (p = availablepackages; p ; p=p->next)
		{
			if (!(p->flags&DPF_WANTTOINSTALL) && (p->flags&DPF_HAVEAVERSION))
			{	//if we don't want it but we have it anyway:
				if (*p->gamedir)
				{
					char *fname = va("%s/%s", p->gamedir, p->dest);
					FS_Remove(fname, FS_ROOT);
				}
				else
					FS_Remove(p->dest, FS_GAME);
				p->flags&=~DPF_HAVEAVERSION;	//FIXME: This is error prone.

				WriteInstalledPackages();

				if (p->flags & DPF_DELETEONUNINSTALL)
				{
					if (last)
						last->next = p->next;
					else
						availablepackages = p->next;

				//	BZ_Free(p);

					return true;
				}
			}
			last = p;
		}

		for (p = availablepackages; p ; p=p->next)
		{
			if ((p->flags&DPF_WANTTOINSTALL) && !(p->flags&(DPF_HAVEAVERSION|DPF_DOWNLOADING)))
			{	//if we want it and don't have it:
				p->dlnum = dlcount++;
				temp = va("dl_%i.tmp", p->dlnum);
				Con_Printf("Downloading %s (to %s)\n", p->fullname, temp);
				p->flags|=DPF_DOWNLOADING;
				if (!HTTP_CL_Get(p->src, temp, Menu_Download_Got))
					p->flags&=~DPF_DOWNLOADING;
			}
		}
		return true;
	}
	return false;
}

void M_AddItemsToDownloadMenu(menu_t *m)
{
	char path[MAX_QPATH];
	int y;
	package_t *p;
	menucustom_t *c;
	char *slash;
	menuoption_t *mo;
	dlmenu_t *info = m->data;
	int prefixlen;
	p = availablepackages;

//	MC_AddRedText(m, 0, 40, "WntHav", false);

	prefixlen = strlen(info->pathprefix);
	y = 48+4;
	for (p = availablepackages; p; p = p->next)
	{
		if (strncmp(p->fullname, info->pathprefix, prefixlen))
			continue;
		slash = strchr(p->fullname+prefixlen, '/');
		if (slash)
		{
			Q_strncpyz(path, p->fullname, MAX_QPATH);
			slash = strchr(path+prefixlen, '/');
			if (slash)
				*slash = '\0';

			for (mo = m->options; mo; mo = mo->common.next)
				if (mo->common.type == mt_button)
					if (!strcmp(mo->button.text, path + prefixlen))
						break;
			if (!mo)
			{
				MC_AddConsoleCommand(m, 6*8, y, path+prefixlen, va("menu_download \"%s/\"", path));
				y += 8;
			}
		}
		else
		{
			c = MC_AddCustom(m, 0, y, p, 0);
			c->draw = MD_Draw;
			c->key = MD_Key;
			c->common.width = 320;
			c->common.height = 8;
			y += 8;
		}
	}

	y+=4;
	MC_AddCommand(m, 0, y, "      Back", MD_PopMenu);
	y+=8;
	MC_AddCommand(m, 0, y, "      Apply", MD_ApplyDownloads);
/*
	for (pn = 1, p = availablepackages; p && pn < info->firstpackagenum ; p=p->next, pn++)

	m->

		if (lastpathlen != p->name - p->fullname || strncmp(p->fullname, lastpath, lastpathlen))
		{
			lastpathlen = p->name - p->fullname;
			lastpath = p->fullname;


			if (!lastpathlen)
				Draw_FunStringLen(x+40, y, "/", 1);
			else
				Draw_FunStringLen(x+40, y, p->fullname, lastpathlen);
			y+=8;
		}
		Draw_Character (x, y, 128);
		Draw_Character (x+8, y, 130);
		Draw_Character (x+16, y, 128);
		Draw_Character (x+24, y, 130);

		//if you want it
		if (p->flags&DPF_WANTTOINSTALL)
			Draw_Character (x+4, y, 131);
		else
			Draw_Character (x+4, y, 129);

		//if you have it already
		if (p->flags&(DPF_HAVEAVERSION | ((((int)(realtime*4))&1)?DPF_DOWNLOADING:0) ))
			Draw_Character (x+20, y, 131);
		else
			Draw_Character (x+20, y, 129);

		if (pn == info->highlightednum)
			Draw_Alt_String(x+48, y, p->name);
		else
			Draw_String(x+48, y, p->name);

		if (p->flags & DPF_DISPLAYVERSION)
		{
			Draw_String(x+48+strlen(p->name)*8, y, va(" (%i.%i)", p->version/1000, p->version%1000));
		}
*/
}

void M_Download_UpdateStatus(struct menu_s *m)
{
	struct dl_download *dl;
	dlmenu_t *info = m->data;
	int i;

	while (!cls.downloadmethod && (info->parsedsourcenum==-1 || info->parsedsourcenum < numdownloadablelists))
	{	//done downloading
		char basename[64];

		info->parsedsourcenum++;

		if (info->parsedsourcenum < numdownloadablelists)
		{
			if (!downloadablelistreceived[info->parsedsourcenum])
			{
				sprintf(basename, "dlinfo_%i.inf", info->parsedsourcenum);
				dl = HTTP_CL_Get(downloadablelist[info->parsedsourcenum], basename, dlnotification);
				if (dl)
					dl->user_num = info->parsedsourcenum;
				else
					Con_Printf("Could not contact server\n");
				return;
			}
		}
	}
	for (i = 0; i < numdownloadablelists; i++)
	{
		if (!downloadablelistreceived[i])
		{
//			Draw_String(x+8, y+8, "Waiting for package list");
			return;
		}
	}

	if (!availablepackages)
	{
//		Draw_String(x+8, y+8, "Could not obtain a package list");
		return;
	}

	if (!info->populated)
	{
		info->populated = true;
		M_AddItemsToDownloadMenu(m);
	}
}
/*
static void M_Download_Draw (int x, int y, struct menucustom_s *c, struct menu_s *m)
{
	int pn;

	int lastpathlen = 0;
	char *lastpath="";

	package_t *p;
	dlmenu_t *info = m->data;

	int i;


	y+=8;
	Draw_Alt_String(x+4, y, "I H");
	(info->highlightednum==0?Draw_Alt_String:Draw_String)(x+40, y, "Apply changes");
	y+=8;

	for (pn = 1, p = availablepackages; p && pn < info->firstpackagenum ; p=p->next, pn++)
		;
	for (; p; p = p->next, y+=8, pn++)
	{
		if (lastpathlen != p->name - p->fullname || strncmp(p->fullname, lastpath, lastpathlen))
		{
			lastpathlen = p->name - p->fullname;
			lastpath = p->fullname;


			if (!lastpathlen)
				Draw_FunStringLen(x+40, y, "/", 1);
			else
				Draw_FunStringLen(x+40, y, p->fullname, lastpathlen);
			y+=8;
		}
		Draw_Character (x, y, 128);
		Draw_Character (x+8, y, 130);
		Draw_Character (x+16, y, 128);
		Draw_Character (x+24, y, 130);

		//if you want it
		if (p->flags&DPF_WANTTOINSTALL)
			Draw_Character (x+4, y, 131);
		else
			Draw_Character (x+4, y, 129);

		//if you have it already
		if (p->flags&(DPF_HAVEAVERSION | ((((int)(realtime*4))&1)?DPF_DOWNLOADING:0) ))
			Draw_Character (x+20, y, 131);
		else
			Draw_Character (x+20, y, 129);

		if (pn == info->highlightednum)
			Draw_Alt_String(x+48, y, p->name);
		else
			Draw_String(x+48, y, p->name);

		if (p->flags & DPF_DISPLAYVERSION)
		{
			Draw_String(x+48+strlen(p->name)*8, y, va(" (%i.%i)", p->version/1000, p->version%1000));
		}
	}
}
*/
static void Menu_Download_Got(struct dl_download *dl)
{
	char *fname = dl->localname;
	qboolean successful = dl->status == DL_FINISHED;
	char *ext;
	package_t *p;
	int dlnum = atoi(fname+3);

	for (p = availablepackages; p ; p=p->next)
	{
		if (p->dlnum == dlnum)
		{
			char *destname;
			char *diskname = fname;

			if (!successful)
			{
				p->flags &= ~DPF_DOWNLOADING;
				Con_Printf("Couldn't download %s (from %s to %s)\n", p->name, p->src, p->dest);
				return;
			}

			if (!(p->flags & DPF_DOWNLOADING))
			{
				Con_Printf("menu_download: We're not downloading %s, apparently\n", p->dest);
				return;
			}

			p->flags &= ~DPF_DOWNLOADING;

			ext = COM_FileExtension(p->dest);
			if (!stricmp(ext, "pak") || !stricmp(ext, "pk3"))
				FS_UnloadPackFiles();	//we reload them after

			if (*p->gamedir)
				destname = va("%s/%s", p->gamedir, p->dest);
			else
				destname = va("%s", p->dest);

			if (FS_Remove(destname, *p->gamedir?FS_ROOT:FS_GAME))
				Con_Printf("Deleted old %s\n", destname);
			if (!FS_Rename2(diskname, destname, FS_GAME, *p->gamedir?FS_ROOT:FS_GAME))
			{
				Con_Printf("Couldn't rename %s to %s. Removed instead.\n", diskname, destname);
				FS_Remove (diskname, FS_GAME);
				return;
			}
			Con_Printf("Downloaded %s (to %s)\n", p->name, destname);
			p->flags |= DPF_HAVEAVERSION;

			WriteInstalledPackages();

			ext = COM_FileExtension(p->dest);
			if (!stricmp(ext, "pak") || !stricmp(ext, "pk3"))
				FS_ReloadPackFiles();
			return;
		}
	}

	Con_Printf("menu_download: Can't figure out where %s came from\n", fname);
}
/*
static qboolean M_Download_Key (struct menucustom_s *c, struct menu_s *m, int key)
{
	char *temp;
	int pn;
	package_t *p, *p2;
	dlmenu_t *info = m->data;

	switch (key)
	{
	case K_UPARROW:
		if (info->highlightednum>0)
			info->highlightednum--;
		return true;
	case K_DOWNARROW:	//cap range when drawing
		if (info->highlightednum < numpackages)
			info->highlightednum++;
		return true;
	case K_ENTER:
		if (!info->highlightednum)
		{	//do it
			//uninstall packages first
			package_t *last = NULL;

			for (p = availablepackages; p ; p=p->next)
			{
				if (!(p->flags&DPF_WANTTOINSTALL) && (p->flags&DPF_HAVEAVERSION))
				{	//if we don't want it but we have it anyway:
					if (*p->gamedir)
					{
						char *fname = va("%s", p->gamedir, p->dest);
						FS_Remove(fname, FS_BASE);
					}
					else
						FS_Remove(p->dest, FS_GAME);
					p->flags&=~DPF_HAVEAVERSION;	//FIXME: This is error prone.

					WriteInstalledPackages();

					if (p->flags & DPF_DELETEONUNINSTALL)
					{
						if (last)
							last->next = p->next;
						else
							availablepackages = p->next;

						BZ_Free(p);

						return M_Download_Key(c, m, key);	//I'm lazy.
					}
				}
				last = p;
			}

			for (p = availablepackages; p ; p=p->next)
			{
				if ((p->flags&DPF_WANTTOINSTALL) && !(p->flags&(DPF_HAVEAVERSION|DPF_DOWNLOADING)))
				{	//if we want it and don't have it:
					p->dlnum = dlcount++;
					temp = va("dl_%i.tmp", p->dlnum);
					Con_Printf("Downloading %s (to %s)\n", p->fullname, temp);
					p->flags|=DPF_DOWNLOADING;
					if (!HTTP_CL_Get(p->src, temp, Menu_Download_Got))
						p->flags&=~DPF_DOWNLOADING;
				}
			}
		}
		else
		{
			for (pn = 1, p = availablepackages; p && pn < info->highlightednum ; p=p->next, pn++)
				;
			if (p)
			{
				p->flags = (p->flags&~DPF_WANTTOINSTALL) | DPF_WANTTOINSTALL - (p->flags&DPF_WANTTOINSTALL);

				if (p->flags&DPF_WANTTOINSTALL)
				{
					for (p2 = availablepackages; p2; p2 = p2->next)
					{
						if (p == p2)
							continue;
						if (!strcmp(p->dest, p2->dest))
							p2->flags &= ~DPF_WANTTOINSTALL;
					}
				}
			}
		}
		return true;
	}
	return false;
}
*/
void Menu_DownloadStuff_f (void)
{
	int i;
	menu_t *menu;
	dlmenu_t *info;

	key_dest = key_menu;
	m_state = m_complex;

	menu = M_CreateMenu(sizeof(dlmenu_t));
	info = menu->data;

	menu->event = M_Download_UpdateStatus;
/*
	menu->selecteditem = (menuoption_t *)(info->list = MC_AddCustom(menu, 0, 32, NULL));
	info->list->draw = M_Download_Draw;
	info->list->key = M_Download_Key;
*/
	info->parsedsourcenum = -1;

	Q_strncpyz(info->pathprefix, Cmd_Argv(1), sizeof(info->pathprefix));
	if (!*info->pathprefix)
	{
		for (i = 0; i < numdownloadablelists; i++)
			downloadablelistreceived[i] = 0;
	}

	MC_AddWhiteText(menu, 24, 8, "Downloads", false);
	MC_AddWhiteText(menu, 16, 24, "\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37", false);

	{
		static qboolean loadedinstalled;
		vfsfile_t *f = loadedinstalled?NULL:FS_OpenVFS(INSTALLEDFILES, "rb", FS_ROOT);
		loadedinstalled = true;
		if (f)
		{
			ConcatPackageLists(BuildPackageList(f, DPF_DELETEONUNINSTALL|DPF_HAVEAVERSION|DPF_WANTTOINSTALL, ""));
			VFS_CLOSE(f);
		}
	}
}
#elif defined(WEBCLIENT)
void Menu_DownloadStuff_f (void)
{
	Con_Printf("Not yet reimplemented\n");
}
#endif


#if defined(WEBCLIENT)

static int numbootdownloads;
#include "fs.h"
#ifdef AVAIL_ZLIB
extern searchpathfuncs_t zipfilefuncs;
static int CL_BootDownload_Extract(const char *fname, int fsize, void *ptr, void *spath)
{
	char buffer[512*1024];
	int read;
	void *zip = ptr;
	flocation_t loc;
	int slashes;
	const char *s;
	vfsfile_t *compressedpak;
	vfsfile_t *decompressedpak;

	if (zipfilefuncs.FindFile(zip, &loc, fname, NULL))
	{
		compressedpak = zipfilefuncs.OpenVFS(zip, &loc, "rb");
		if (compressedpak)
		{
			//this extra logic is so we can handle things like nexuiz/data/blah.pk3
			//as well as just data/blah.pk3
			slashes = 0;
			for (s = strchr(fname, '/'); s; s = strchr(s+1, '/'))
				slashes++;
			for (; slashes > 1; slashes--)
				fname = strchr(fname, '/')+1;

			if (!slashes)
			{
				FS_CreatePath(fname, FS_GAMEONLY);
				decompressedpak = FS_OpenVFS(fname, "wb", FS_GAMEONLY);
			}
			else
			{
				FS_CreatePath(fname, FS_ROOT);
				decompressedpak = FS_OpenVFS(fname, "wb", FS_ROOT);
			}
			if (decompressedpak)
			{
				for(;;)
				{
					read = VFS_READ(compressedpak, buffer, sizeof(buffer));
					if (read <= 0)
						break;
					if (VFS_WRITE(decompressedpak, buffer, read) != read)
					{
						Con_Printf("write failed writing %s. disk full?\n", fname);
						break;
					}
				}
				VFS_CLOSE(decompressedpak);
			}
			VFS_CLOSE(compressedpak);
		}
	}
	return true;
}
#endif

qboolean FS_LoadPackageFromFile(vfsfile_t *vfs, char *pname, char *localname, int *crc, qboolean copyprotect, qboolean istemporary, qboolean isexplicit);

void FS_GenCachedPakName(char *pname, char *crc, char *local, int llen);
static void CL_BootDownload_Complete(struct dl_download *dl)
{
	void *zip;
	char *q = strchr(dl->url, '?');
	char *ext;
	if (dl->file && dl->status == DL_FINISHED)
	{
		if (q) *q = '0';
		ext = COM_FileExtension(dl->url);
		if (q) *q = '?';
		if (!stricmp(ext, "zip"))
		{
#ifdef AVAIL_ZLIB
			if (dl->status == DL_FINISHED)
				zip = zipfilefuncs.OpenNew(dl->file, dl->url);
			else
				zip = NULL;
			if (zip)
			{
				dl->file = NULL;	//file is now owned by the zip context.
				if (dl->user_ctx)
				{
					vfsfile_t *in, *out;
					flocation_t loc;
					qboolean found = false;
					int crc;

					found = zipfilefuncs.FindFile(zip, &loc, dl->user_ctx, NULL);
					if (!found)
					{
						char *s = COM_SkipPath(dl->user_ctx);
						if (s != dl->user_ctx)
							found = zipfilefuncs.FindFile(zip, &loc, s, NULL);
					}

					if (found)
					{
						in = zipfilefuncs.OpenVFS(zip, &loc, "rb");
						if (in)
						{
							char local[MAX_OSPATH];
							FS_GenCachedPakName(dl->user_ctx, va("%i", dl->user_num), local, sizeof(local));
							FS_CreatePath(local, FS_ROOT);
							out = FS_OpenVFS(local, "wb", FS_ROOT);
							if (out)
							{
								char buffer[8192];
								int read;
								for(;;)
								{
									read = VFS_READ(in, buffer, sizeof(buffer));
									if (read <= 0)
										break;
									if (VFS_WRITE(out, buffer, read) != read)
									{
										Con_Printf("write failed writing %s. disk full?\n", local);
										break;
									}
								}
								VFS_CLOSE(out);
								out = FS_OpenVFS(local, "rb", FS_ROOT);
								crc = dl->user_num;
								if (!FS_LoadPackageFromFile(out, dl->user_ctx, local, &crc, true, false, true))
								{
									if (crc == dl->user_num)
										Con_Printf(CON_WARNING "Manifest package \"%s\" is unusable.\n", (char*)dl->user_ctx);
									else
										Con_Printf(CON_WARNING "Manifest package \"%s\" is unusable or has invalid crc. Stated crc %#x is not calculated crc %#x\n", (char*)dl->user_ctx, dl->user_num, crc);
									FS_Remove(local, FS_ROOT);
								}
							}
						}
						VFS_CLOSE(in);
					}

					free(dl->user_ctx);
				}
				else
				{
					/*scan it to extract its contents*/
					zipfilefuncs.EnumerateFiles(zip, "*/*.pk3", CL_BootDownload_Extract, zip);
					zipfilefuncs.EnumerateFiles(zip, "*/*.pak", CL_BootDownload_Extract, zip);
					zipfilefuncs.EnumerateFiles(zip, "*/*/*.pk3", CL_BootDownload_Extract, zip);
					zipfilefuncs.EnumerateFiles(zip, "*/*/*.pak", CL_BootDownload_Extract, zip);
				}

				/*close it, delete the temp file from disk, etc*/
				zipfilefuncs.ClosePath(zip);

				/*restart the filesystem so those new files can be found*/
				Cmd_ExecuteString("fs_restart\n", RESTRICT_LOCAL);
			}
#endif
		}
		else
		{
			//okay, its named directly, write it out as it is
			vfsfile_t *out;
			int crc, *crcptr = &crc;
			char local[MAX_OSPATH];

			qboolean ispackage = (!stricmp(ext, "pak") || !stricmp(ext, "pk3"));
#ifndef NACL
			if (ispackage)
			{
				FS_GenCachedPakName(dl->user_ctx, va("%i", dl->user_num), local, sizeof(local));
#else
			//nacl permits any files to be listed in the manifest file, as there's no prior files to override/conflict, so don't mess about with crcs etc
			crcptr = NULL;
			Q_strncpyz(local, dl->user_ctx, sizeof(local));
			if (1)
			{
#endif
				FS_CreatePath(local, FS_ROOT);

				//write it out
				out = FS_OpenVFS(local, "wb", FS_ROOT);
				if (!out)
					Con_Printf("Unable to open %s for writing\n", local);
				else
				{
					char buffer[65535];
					int read;
					for(;;)
					{
						read = VFS_READ(dl->file, buffer, sizeof(buffer));
						if (read <= 0)
							break;
						if (VFS_WRITE(out, buffer, read) != read)
						{
							Con_Printf("write failed writing %s. disk full?\n", local);
							break;
						}
					}
					VFS_CLOSE(out);

					if (ispackage)
					{
						if (crcptr)
						{
							//load it as a package
							out = FS_OpenVFS(local, "rb", FS_ROOT);
							if (out)
							{
								crc = dl->user_num;
								if (!FS_LoadPackageFromFile(out, dl->user_ctx, local, crcptr, true, false, true))
								{
									if (crc == dl->user_num)
										Con_Printf(CON_WARNING "Manifest package \"%s\" is unusable.\n", (char*)dl->user_ctx);
									else
										Con_Printf(CON_WARNING "Manifest package \"%s\" is unusable or has invalid crc. Stated crc %#x is not calculated crc %#x\n", (char*)dl->user_ctx, dl->user_num, crc);
									FS_Remove(local, FS_ROOT);
								}
								Cmd_ExecuteString("fs_restart\n", RESTRICT_LOCAL);
							}
						}
						else
							Cmd_ExecuteString("fs_restart\n", RESTRICT_LOCAL);
					}
				}
			}
			else
				Con_Printf("File %s isn't a recognised package!\n", (char*)dl->user_ctx);
		}
	}

	if (!--numbootdownloads)
	{
		CL_ExecInitialConfigs();
		Cmd_StuffCmds();
		Cbuf_Execute ();
		Cmd_ExecuteString("vid_restart\n", RESTRICT_LOCAL);
		CL_StartCinematicOrMenu();
	}
}

static void CL_Manifest_Complete(struct dl_download *dl)
{
	if (!dl->file)
		Con_Printf("Unable to load manifest from %s\n", dl->url);
	else
	{
		vfsfile_t *f;
		char buffer[1024];
		char *fname;
		int crc;
		char local[MAX_OSPATH];
		while(VFS_GETS(dl->file, buffer, sizeof(buffer)))
		{
			Cmd_TokenizeString(buffer, false, false);
			if (!Cmd_Argc())
				continue;
			fname = Cmd_Argv(0);
			crc = strtoul(Cmd_Argv(1), NULL, 0);

			f = FS_OpenVFS(fname, "rb", FS_ROOT);
			if (f)
			{
				//should be loaded as needed
				VFS_CLOSE(f);
			}
			else
			{
				FS_GenCachedPakName(fname, va("%i", crc), local, sizeof(local));
				f = FS_OpenVFS(local, "rb", FS_ROOT);
				if (f)
				{
					int truecrc = crc;
					if (!FS_LoadPackageFromFile(f, fname, local, &truecrc, true, false, true))
					{
						if (crc == truecrc)
							Con_Printf(CON_WARNING "Manifest package \"%s\" is unusable.\n", fname);
						else
							Con_Printf(CON_WARNING "Manifest package \"%s\" is unusable or has invalid crc. Stated crc %#x is not calculated crc %#x\n", fname, crc, truecrc);
						FS_Remove(local, FS_ROOT);
					}
				}
				else
				{
					int args = Cmd_Argc() - 2;
					if (!args)
						Con_Printf("No mirrors for \"%s\"\n", fname);
					else
					{
						struct dl_download *ndl;
#ifdef NACL
						args = 0;	//nacl currently depends upon the browser's download cache for all file persistance, which means we want the same file every time.
									//there's only one way to do that, sadly.
#else
						args = rand() % args;
#endif
						Con_Printf("Downloading \"%s\" from \"%s\"\n", fname, Cmd_Argv(2 + args));
						ndl = HTTP_CL_Get(Cmd_Argv(2), "", CL_BootDownload_Complete);

						if (ndl)
						{
							ndl->user_ctx = strdup(fname);
							ndl->user_num = crc;
				#ifdef MULTITHREAD
							DL_CreateThread(ndl, FS_OpenTemp(), CL_BootDownload_Complete);
				#endif
							numbootdownloads++;
						}
					}
				}
			}
		}
	}
	if (!--numbootdownloads)
	{
		CL_ExecInitialConfigs();
		Cmd_StuffCmds();
		Cbuf_Execute ();
		Cmd_ExecuteString("vid_restart\n", RESTRICT_LOCAL);
		CL_StartCinematicOrMenu();
	}
}

qboolean CL_CheckBootDownloads(void)
{
	char *downloads = fs_gamedownload.string;
	char token[2048];
	char *c, *s;
	vfsfile_t *f;
	struct dl_download *dl;
	int mirrors;

	int man = COM_CheckParm("-manifest");
	if (man)
	{
		const char *fname = com_argv[man+1];
		if (*fname)
		{
			Con_Printf("Checking manifest from \"%s\"\n", fname);

			dl = HTTP_CL_Get(fname, "", CL_Manifest_Complete);
			if (dl)
			{
	#ifdef MULTITHREAD
				DL_CreateThread(dl, FS_OpenTemp(), CL_Manifest_Complete);
	#endif
				numbootdownloads++;
			}
		}
	}

	while ((downloads = COM_ParseOut(downloads, token, sizeof(token))))
	{
		//FIXME: do we want to add some sort of file size indicator?
		c = token;
		while(*c && *c != ':' && *c != '|')
			c++;
		if (!*c)	//erp?
			continue;
		*c++ = 0;
		f = FS_OpenVFS(token, "rb", FS_ROOT);
		if (f)
		{
			Con_DPrintf("Already have %s\n", token);
			VFS_CLOSE(f);
			continue;
		}
		mirrors = 1;
		for (s = c; *s; s++)
		{
			if (*s == '|')
				mirrors++;
		}
		mirrors = rand() % mirrors;

		while(mirrors)
		{
			mirrors--;
			while(*c != '|')
				c++;
			c++;
		}
		for (s = c; *s; s++)
		{
			if (*s == '|')
				*s = 0;
		}

		Con_Printf("Attempting to download %s\n", c);

		dl = HTTP_CL_Get(c, "", CL_BootDownload_Complete);
		if (dl)
		{
#ifdef MULTITHREAD
			DL_CreateThread(dl, FS_OpenTemp(), CL_BootDownload_Complete);
#endif
			numbootdownloads++;
		}
	}

	return !numbootdownloads;
}
#else
qboolean CL_CheckBootDownloads(void)
{
	if (COM_CheckParm("-manifest"))
		Con_Printf("download manifests not supported in this build\n");
	return true;
}
#endif
