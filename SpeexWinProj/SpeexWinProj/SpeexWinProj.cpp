// SpeexWinProj.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "speex/speex_jitter.h"
#include "speex/speex_echo.h"
#include "speex/speex_preprocess.h"
#include "speex/speex_resampler.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

union jbpdata {
	unsigned int idx;
	unsigned char data[4];
};

void synthIn(JitterBufferPacket *in, int idx, int span) {
	union jbpdata d;
	d.idx = idx;

	in->data = (char*)d.data;
	in->len = sizeof(d);
	in->timestamp = idx * 10;
	in->span = span * 10;
	in->sequence = idx;
	in->user_data = 0;
}

void jitterFill(JitterBuffer *jb) {
	char buffer[65536];
	JitterBufferPacket in, out;
	int i;

	out.data = buffer;

	jitter_buffer_reset(jb);

	for(i=0;i<100;++i) {
		synthIn(&in, i, 1);
		jitter_buffer_put(jb, &in);

		out.len = 65536;
		if (jitter_buffer_get(jb, &out, 10, NULL) != JITTER_BUFFER_OK) {
			printf("Fill test failed iteration %d\n", i);
		}
		if (out.timestamp != i * 10) {
			printf("Fill test expected %d got %d\n", i*10, out.timestamp);
		}
		jitter_buffer_tick(jb);
	}
}

void TestJitter()
{
	char buffer[65536];
	JitterBufferPacket in, out;
	int i;

	JitterBuffer *jb = jitter_buffer_init(10);

	out.data = buffer;

	/* Frozen sender case */
	jitterFill(jb);
	for(i=0;i<100;++i) {
		out.len = 65536;
		jitter_buffer_get(jb, &out, 10, NULL);
		jitter_buffer_tick(jb);
	}
	synthIn(&in, 100, 1);
	jitter_buffer_put(jb, &in);
	out.len = 65536;
	if (jitter_buffer_get(jb, &out, 10, NULL) != JITTER_BUFFER_OK) {
		printf("Failed frozen sender resynchronize\n");
	} else {
		printf("Frozen sender: Jitter %d\n", out.timestamp - 100*10);
	}
	return ;
}

void TestNoise(char *pSrcFile,char *pDenoiseFile)
{
	#define NN 160
	short in[NN];
	int i;
	SpeexPreprocessState *st;
	int count=0;
	float f;

	FILE *pSrcIN       = fopen(pSrcFile, "rb");
	FILE *pDenoiseOUT  = fopen(pDenoiseFile,  "wb");

	st = speex_preprocess_state_init(NN, 32000);
	i=1;
	speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DENOISE, &i);
	i=0;
	speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_AGC, &i);
	i=24000;
	speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_AGC_LEVEL, &i);
	i=0;
	speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DEREVERB, &i);
	f=.0;
	speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DEREVERB_DECAY, &f);
	f=.0;
	speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DEREVERB_LEVEL, &f);

	while (1)
	{
		int vad;
		fread(in, sizeof(short), NN, pSrcIN);
		if (feof(pSrcIN))
			break;
		vad = speex_preprocess_run(st, in);
		/*fprintf (stderr, "%d\n", vad);*/
		fwrite(in, sizeof(short), NN, pDenoiseOUT);
		count++;
	}
	speex_preprocess_state_destroy(st);
	fclose(pSrcIN);
	fclose(pDenoiseOUT);
}

void _TestEcho(char *pSrcFile,char *pEchoFile,char *pAudioFile)
{
#define NN_ECHO 128
#define TAIL 1024

	FILE *echo_fd, *ref_fd, *e_fd;
	short echo_buf[NN_ECHO], ref_buf[NN_ECHO], e_buf[NN_ECHO];
	SpeexEchoState *st;
	SpeexPreprocessState *den;
	int sampleRate = 8000;

	echo_fd = fopen(pSrcFile, "rb");
	ref_fd  = fopen(pEchoFile,  "rb");
	e_fd    = fopen(pAudioFile, "wb");

	st = speex_echo_state_init(NN_ECHO, TAIL);
	den = speex_preprocess_state_init(NN_ECHO, sampleRate);
	speex_echo_ctl(st, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);
	speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_ECHO_STATE, st);

	while (!feof(ref_fd) && !feof(echo_fd))
	{
		fread(ref_buf, sizeof(short), NN_ECHO, ref_fd);
		fread(echo_buf, sizeof(short), NN_ECHO, echo_fd);
		speex_echo_cancellation(st, ref_buf, echo_buf, e_buf);
		speex_preprocess_run(den, e_buf);
		fwrite(e_buf, sizeof(short), NN_ECHO, e_fd);
	}
	speex_echo_state_destroy(st);
	speex_preprocess_state_destroy(den);
	fclose(e_fd);
	fclose(echo_fd);
	fclose(ref_fd);
}

int TestResampler()
{

#define NNTR 256
   spx_uint32_t i;
   short *in;
   short *out;
   float *fin, *fout;
   int count = 0;
   SpeexResamplerState *st = speex_resampler_init(1, 8000, 12000, 10, NULL);
   speex_resampler_set_rate(st, 96000, 44100);
   speex_resampler_skip_zeros(st);
   
   in = (short*)malloc(NNTR*sizeof(short));
   out = (short*)malloc(2*NNTR*sizeof(short));
   fin = (float*)malloc(NNTR*sizeof(float));
   fout = (float*)malloc(2*NNTR*sizeof(float));
   while (1)
   {
      spx_uint32_t in_len;
      spx_uint32_t out_len;
      fread(in, sizeof(short), NNTR, stdin);
      if (feof(stdin))
         break;
      for (i=0;i<NNTR;i++)
         fin[i]=in[i];
      in_len = NNTR;
      out_len = 2*NNTR;
      /*if (count==2)
         speex_resampler_set_quality(st, 10);*/
      speex_resampler_process_float(st, 0, fin, &in_len, fout, &out_len);
      for (i=0;i<out_len;i++)
         out[i]=floor(.5+fout[i]);
      /*speex_warning_int("writing", out_len);*/
      fwrite(out, sizeof(short), out_len, stdout);
      count++;
   }
   speex_resampler_destroy(st);
   free(in);
   free(out);
   free(fin);
   free(fout);
   return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	TestNoise("by_1C_16bit_32K.pcm","by_1C_16bit_32K_ns.pcm");
	return 0;
}

