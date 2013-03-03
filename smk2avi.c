#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "smacker.h"

#define w(p,n) fwrite(p,1,n,fp)

#define LIST w("LIST",4);

#define lu(p) \
{ \
	b[0] = (p & 0x000000FF); \
	b[1] = ((p & 0x0000FF00) >> 8); \
	b[2] = ((p & 0x00FF0000) >> 16); \
	b[3] = ((p & 0xFF000000) >> 24); \
	w(b,4); \
}

void process(const char *fn)
{
	FILE *fp;
	smk s;
	char outfile[256];
	unsigned char b[4];

	int		i,j;
	unsigned long temp_u;

	/* all and video info */
	unsigned long	w, h, f;
	double		fps;

	/* audio info */
	unsigned char	a_t, a_c[7], a_d[7];
	unsigned long	a_r[7];

	unsigned char **audio_data[7];
	unsigned long *audio_size[7];

	unsigned long cur_frame;

	printf("--------\nsmk2avi processing %s...\n",fn);

	/* open the smk file */
	s = smk_open_file(fn,SMK_MODE_MEMORY);
	if (s == NULL) return;

	/* get some info about the file */
	smk_info_all(s, NULL, &f, &fps);
	smk_info_video(s, &w, &h, NULL);
	smk_info_audio(s, &a_t, a_c, a_d, a_r);

	/* make 2 passes through the file.
		first one is to pull all the audio tracks only. */
	smk_enable_all(s,a_t);
	for (i = 0; i < 7; i ++)
	{
		audio_size[i] = malloc(f * sizeof(unsigned long));
		audio_data[i] = malloc(f * sizeof(unsigned char*));
	}

	printf("\tAudio processing frame: ");
	smk_first(s);
	for (cur_frame = 0; cur_frame < f; cur_frame ++)
	{
		printf("%u... ",cur_frame);
		for (i = 0; i < 7; i ++)
		{
			audio_size[i][f] = smk_get_audio_size(s,i);
			audio_data[i][f] = malloc(audio_size[i][f]);
			memcpy(audio_data[i][f],smk_get_audio(s,i),audio_size[i][f]);
		}
		smk_next(s);
	}
	printf("done!\n");

	smk_enable_all(s,SMK_VIDEO_TRACK);
	smk_first(s);

	sprintf(outfile,"%s.avi",fn);

	fp = fopen(outfile,"wb");

	// riff header
	w("RIFF",4);
	lu(0);
	w("AVI ",4);

	{
		// avi header list
		LIST;
		lu(0);
		w("hdrl",4);

		{
			// avi header
			w("avih",4);
			lu(44);
			{
				temp_u = (1000000 / fps ) + .5;
				lu( temp_u ); // framerate
				lu( 0 );
				lu( 1 );
				lu( 0 );
				lu( f );
				lu( 0 );
				lu( 8 );
				lu( 0 );
				lu( w );
				lu( h );
				lu( 0 );
			}

			// stream list: video stream
			LIST
			lu(0);
			w("strl",4);
	
			{
				w("strh",4);
				lu(0);
				{
					w("vidsDIB ",8);
					lu(0);
					lu(0);
					lu(0);
				temp_u = (1000000 / fps ) + .5;
				lu( temp_u ); // framerate
					lu( 1000000);
					lu(0);
					lu(0);
					lu(0);
					lu(0);
					lu(0);
					lu(0);
				}
				
	
				w("strf",4);
				lu(0);
				w("data",4);
			}
	
			// stream list: audio stream(s)
			for (i = 0; i < 7; i++)
			{
				LIST
				lu(0);
				w("strl",4);
	
				w("strh",4);
				lu(0);
				w("data",4);
	
				w("strf",4);
				lu(0);
				w("data",4);
			}
		}

		// movie data
		LIST
		lu(0);
		w("movi",4);
	
		for (i = 0; i < f; i ++)
		{
			LIST
			lu(0);
			w("rec ",4);
	
			w("00db",4);
			lu(0);
			w("data",4);
	
			for (j = 0; j < 7; j++)
			{
				sprintf(b,"%02uwb",(j + 1));
				w(b,4);
				w("data",4);
			}
		}
	}
	
	fclose(fp);	

	smk_close(s);

	printf("done.\n--------\n");
}

int main (int argc, char *argv[])
{
	int i;
	for (i = 1; i < argc; i ++)
	{
		process(argv[i]);
	}
	return 0;
}
