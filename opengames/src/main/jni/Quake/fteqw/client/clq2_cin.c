#include "quakedef.h"
#ifndef NOMEDIA

typedef struct
{
	qbyte	*data;
	int		count;
} cblock_t;

typedef struct cinematics_s
{
	qboolean	restart_sound;
	int		s_rate;
	int		s_width;
	int		s_channels;

	int		width;
	int		height;
	qbyte	*pic;
	qbyte	*pic_pending;

	// order 1 huffman stuff
	int		*hnodes1;	// [256][256][2];
	int		numhnodes1[256];

	int		h_used[512];
	int		h_count[512];


	int cinematictime;
	qboolean cinematicpalette_active;
	qbyte cinematicpalette[768];

	vfsfile_t *cinematic_file;
	int cinematicframe;
} cinematics_t;


void CIN_StopCinematic (cinematics_t *cin)
{
	cin->cinematictime = 0;	// done
	if (cin->pic)
	{
		Z_Free (cin->pic);
		cin->pic = NULL;
	}
	if (cin->pic_pending)
	{
		Z_Free (cin->pic_pending);
		cin->pic_pending = NULL;
	}
	if (cin->cinematic_file)
	{
		VFS_CLOSE (cin->cinematic_file);
		cin->cinematic_file = NULL;
	}
	if (cin->hnodes1)
	{
		Z_Free (cin->hnodes1);
		cin->hnodes1 = NULL;
	}

	Z_Free(cin);
}

//==========================================================================

/*
==================
SmallestNode1
==================
*/
static int	SmallestNode1 (cinematics_t *cin, int numhnodes)
{
	int		i;
	int		best, bestnode;

	best = 99999999;
	bestnode = -1;
	for (i=0 ; i<numhnodes ; i++)
	{
		if (cin->h_used[i])
			continue;
		if (!cin->h_count[i])
			continue;
		if (cin->h_count[i] < best)
		{
			best = cin->h_count[i];
			bestnode = i;
		}
	}

	if (bestnode == -1)
		return -1;

	cin->h_used[bestnode] = true;
	return bestnode;
}


/*
==================
Huff1TableInit

Reads the 64k counts table and initializes the node trees
==================
*/
static void Huff1TableInit (cinematics_t *cin)
{
	int		prev;
	int		j;
	int		*node, *nodebase;
	qbyte	counts[256];
	int		numhnodes;

	cin->hnodes1 = Z_Malloc (256*256*2*4);
	memset (cin->hnodes1, 0, 256*256*2*4);

	for (prev=0 ; prev<256 ; prev++)
	{
		memset (cin->h_count,0,sizeof(cin->h_count));
		memset (cin->h_used,0,sizeof(cin->h_used));

		// read a row of counts
		VFS_READ (cin->cinematic_file, counts, sizeof(counts));
		for (j=0 ; j<256 ; j++)
			cin->h_count[j] = counts[j];

		// build the nodes
		numhnodes = 256;
		nodebase = cin->hnodes1 + prev*256*2;

		while (numhnodes != 511)
		{
			node = nodebase + (numhnodes-256)*2;

			// pick two lowest counts
			node[0] = SmallestNode1 (cin, numhnodes);
			if (node[0] == -1)
				break;	// no more

			node[1] = SmallestNode1 (cin, numhnodes);
			if (node[1] == -1)
				break;

			cin->h_count[numhnodes] = cin->h_count[node[0]] + cin->h_count[node[1]];
			numhnodes++;
		}

		cin->numhnodes1[prev] = numhnodes-1;
	}
}

/*
==================
Huff1Decompress
==================
*/
static cblock_t Huff1Decompress (cinematics_t *cin, cblock_t in)
{
	qbyte		*input;
	qbyte		*out_p;
	int			nodenum;
	int			count;
	cblock_t	out;
	int			inbyte;
	int			*hnodes, *hnodesbase;
//int		i;

	// get decompressed count
	count = in.data[0] + (in.data[1]<<8) + (in.data[2]<<16) + (in.data[3]<<24);
	input = in.data + 4;
	out_p = out.data = Z_Malloc (count);

	// read bits

	hnodesbase = cin->hnodes1 - 256*2;	// nodes 0-255 aren't stored

	hnodes = hnodesbase;
	nodenum = cin->numhnodes1[0];
	while (count)
	{
		inbyte = *input++;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin->numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin->numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin->numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin->numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin->numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin->numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin->numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin->numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
	}

	if (input - in.data != in.count && input - in.data != in.count+1)
	{
		Con_Printf ("Decompression overread by %i\n", (int)(input - in.data) - in.count);
	}
	out.count = out_p - out.data;

	return out;
}

/*
==================
SCR_ReadNextFrame
==================
*/
qbyte *CIN_ReadNextFrame (cinematics_t *cin)
{
	int		r;
	int		command;
	qbyte	samples[22050/14*4];
	qbyte	compressed[0x20000];
	int		size;
	qbyte	*pic;
	cblock_t	in, huf1;
	int		start, end, count;

	// read the next frame
	r = VFS_READ (cin->cinematic_file, &command, 4);
	if (r == 0)		// we'll give it one more chance
		r = VFS_READ (cin->cinematic_file, &command, 4);

	if (r != 4)
		return NULL;
	command = LittleLong(command);
	if (command == 2)
		return NULL;	// last frame marker

	if (command == 1)
	{	// read palette
		VFS_READ (cin->cinematic_file, cin->cinematicpalette, sizeof(cin->cinematicpalette));
	}

	// decompress the next frame
	VFS_READ (cin->cinematic_file, &size, 4);
	size = LittleLong(size);
	if (size > sizeof(compressed) || size < 1)
		Host_Error ("Bad compressed frame size");
	VFS_READ (cin->cinematic_file, compressed, size);

	// read sound
	start = cin->cinematicframe*cin->s_rate/14;
	end = (cin->cinematicframe+1)*cin->s_rate/14;
	count = end - start;

	VFS_READ (cin->cinematic_file, samples, count*cin->s_width*cin->s_channels);

	if (cin->s_width == 1)
		COM_CharBias(samples, count*cin->s_channels);
	else if (cin->s_width == 2)
		COM_SwapLittleShortBlock((short *)samples, count*cin->s_channels);

	S_RawAudio (0, samples, cin->s_rate, count, cin->s_channels, cin->s_width);

	in.data = compressed;
	in.count = size;

	huf1 = Huff1Decompress (cin, in);

	pic = huf1.data;

	cin->cinematicframe++;

	return pic;
}


/*
==================
SCR_RunCinematic

0 = error
1 = success
2 = success but nothing changed
==================
*/
int CIN_RunCinematic (cinematics_t *cin, qbyte **outdata, int *outwidth, int *outheight, qbyte **outpalette)
{
	int		frame;

	if (cin->cinematictime <= 0)
	{
		cin->cinematictime = realtime;
		cin->cinematicframe = -1;
	}

	if (cin->cinematictime-3 > realtime*1000)
		cin->cinematictime = realtime*1000;

	frame = (realtime*1000 - cin->cinematictime)*14/1000;
	if (frame <= cin->cinematicframe)
	{
		*outdata = cin->pic;
		*outwidth = cin->width;
		*outheight = cin->height;
		*outpalette = cin->cinematicpalette;
		return 2;
	}
	if (frame > cin->cinematicframe+1)
	{
		Con_Printf ("Dropped frame: %i > %i\n", frame, cin->cinematicframe+1);
		cin->cinematictime = realtime*1000 - cin->cinematicframe*1000/14;
	}
	if (cin->pic)
		Z_Free (cin->pic);
	cin->pic = CIN_ReadNextFrame (cin);
	if (!cin->pic)
	{
		return 0;
	}

	*outdata = cin->pic;
	*outwidth = cin->width;
	*outheight = cin->height;
	*outpalette = cin->cinematicpalette;
	return 1;
}

/*
==================
SCR_DrawCinematic

Returns true if a cinematic is active, meaning the view rendering
should be skipped
==================
*/
/*
qboolean CIN_DrawCinematic (void)
{
	if (cin->cinematictime <= 0)
	{
		return false;
	}

	if (key_dest == key_menu)
	{	// blank screen and pause if menu is up
//		re.CinematicSetPalette(NULL);
		cin.cinematicpalette_active = false;
//		return true;
	}

	if (!cin.cinematicpalette_active)
	{
//		re.CinematicSetPalette(cl.cinematicpalette);
		cin.cinematicpalette_active = true;
	}

	if (!cin.pic)
		return true;


	Media_ShowFrame8bit(cin.pic, cin.width, cin.height, cin.cinematicpalette);

//	re.DrawStretchRaw (0, 0, viddef.width, viddef.height,
//		cin.width, cin.height, cin.pic);

	return true;
}
*/

/*
==================
SCR_PlayCinematic

==================
*/
cinematics_t *CIN_PlayCinematic (char *arg)
{
	int		width, height;
	char	name[MAX_OSPATH];
//	int		old_khz;
	vfsfile_t *file;

	cinematics_t *cin = NULL;

	file = FS_OpenVFS(arg, "rb", FS_GAME);

	if (!file)
	{
		snprintf (name, sizeof(name), "video/%s", arg);
		file = FS_OpenVFS(name, "rb", FS_GAME);
	}

	if (file)
		cin = Z_Malloc(sizeof(*cin));
	if (cin)
	{
		cin->cinematicframe = 0;
		cin->cinematic_file = file;

		VFS_READ (cin->cinematic_file, &width, 4);
		VFS_READ (cin->cinematic_file, &height, 4);
		cin->width = LittleLong(width);
		cin->height = LittleLong(height);

		VFS_READ (cin->cinematic_file, &cin->s_rate, 4);
		cin->s_rate = LittleLong(cin->s_rate);
		VFS_READ (cin->cinematic_file, &cin->s_width, 4);
		cin->s_width = LittleLong(cin->s_width);
		VFS_READ (cin->cinematic_file, &cin->s_channels, 4);
		cin->s_channels = LittleLong(cin->s_channels);

		Huff1TableInit (cin);

		cin->cinematicframe = 0;
		cin->pic = CIN_ReadNextFrame (cin);
		cin->cinematictime = Sys_DoubleTime ()*1000+0.001;
	}
	else
	{
		Con_Printf(CON_WARNING "Cinematic %s not found.\n", name);
	}
	return cin;
}

#endif
