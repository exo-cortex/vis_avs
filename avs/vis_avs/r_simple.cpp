/*
  LICENSE
  -------
Copyright 2005 Nullsoft, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer. 

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution. 

  * Neither the name of Nullsoft nor the names of its contributors may be used to 
    endorse or promote products derived from this software without specific prior written permission. 
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR 
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
// alphachannel safe 11/21/99
#include "c_simple.h"
#include "r_defs.h"


#ifndef LASER

#define PUT_INT(y) data[pos]=(y)&255; data[pos+1]=(y>>8)&255; data[pos+2]=(y>>16)&255; data[pos+3]=(y>>24)&255
#define GET_INT() (data[pos]|(data[pos+1]<<8)|(data[pos+2]<<16)|(data[pos+3]<<24))

void C_THISCLASS::load_config(unsigned char *data, int len)
{
	int pos=0;
  int x=0;
	if (len-pos >= 4) { effect=GET_INT(); pos+=4; }
	if (len-pos >= 4) { num_colors=GET_INT(); pos+=4; }
	if (num_colors <= 16) while (len-pos >= 4 && x < num_colors) { colors[x++]=GET_INT(); pos+=4; }
  else num_colors=0;
}

int  C_THISCLASS::save_config(unsigned char *data)
{
	int pos=0,x=0;
	PUT_INT(effect); pos+=4;
	PUT_INT(num_colors); pos+=4;
  while (x < num_colors) { PUT_INT(colors[x]); x++;  pos+=4; }
	return pos;
}


C_THISCLASS::C_THISCLASS()
{
	effect=0|(2<<2)|(2<<4);
  num_colors=1;
  memset(colors,0,sizeof(colors));
  colors[0]=RGB(255,255,255);
  color_pos=0;
}

C_THISCLASS::~C_THISCLASS()
{
}
	
int C_THISCLASS::render(char visdata[2][2][576], int isBeat, int *framebuffer, int*, int w, int h)
{
  if (!num_colors) return 0;
  if (isBeat&0x80000000) return 0;
	int x;
	float yscale = (float) h / 2.0f / 256.0f; 
	float xscale = 288.0f/w;
  int current_color;
  unsigned char *fa_data;
  char center_channel[576];
  int which_ch=(effect>>2)&3;
  int y_pos=(effect>>4)&3;
  color_pos++;
  if (color_pos >= num_colors * 64) color_pos=0;

  {
    int p=color_pos/64;
    int r=color_pos&63;
    int c1,c2;
    int r1,r2,r3;
    c1=colors[p];
    if (p+1 < num_colors)
      c2=colors[p+1];
    else c2=colors[0];

    r1=(((c1&255)*(63-r))+((c2&255)*r))/64;
    r2=((((c1>>8)&255)*(63-r))+(((c2>>8)&255)*r))/64;
    r3=((((c1>>16)&255)*(63-r))+(((c2>>16)&255)*r))/64;
    
    current_color=r1|(r2<<8)|(r3<<16);
  }

  if (which_ch>=2)
  {
    int w=0;
    if ((effect&3)>1) 
      w=1;

    for (x = 0; x < 576; x ++) center_channel[x]=visdata[w][0][x]/2+visdata[w][1][x]/2;
  }
  if (which_ch < 2) fa_data=(unsigned char *)&visdata[(effect&3)>1?1:0][which_ch][0];
  else fa_data=(unsigned char *)center_channel;

  if (effect&(1<<6))
  {
    switch (effect&2)
    { 
      case 2: // dot scope
		  {
			  int yh = y_pos*h / 2;
        if (y_pos==2) yh=h/4;
			  for (x = 0; x < w; x ++)
			  {
          float r=x*xscale;
          float s1=r-(int)r;
          float yr=(fa_data[(int)r]^128)*(1.0f-s1)+(fa_data[(int)r+1]^128)*(s1);
          int y=yh + (int) (yr*yscale);
          if (y >= 0 && y < h) framebuffer[x+y*w]=current_color;
			  }
		  }
      break;
      case 0: // dot analyzer
      {
			  int h2=h/2;     
			  float ys=yscale;
			  float xs=200.0f/w;
        int adj=1;
			  if (y_pos!=1) { ys=-ys; adj=0; }
        if (y_pos==2)
        {
          h2 -= (int) (ys*256/2);
        }
			  for (x = 0; x < w; x ++)
			  {
          float r=x*xs;
          float s1=r-(int)r;
          float yr=fa_data[(int)r]*(1.0f-s1)+fa_data[(int)r+1]*(s1);
          int y=h2+adj+(int)(yr*ys-1.0f);
          if (y >= 0 && y < h) framebuffer[x+w*y]=current_color;
			  }
      }
      break;
    }
  }
  else
    switch (effect&3)
    {
      case 0: //solid analyzer
      {
			  int h2=h/2;     
			  float ys=yscale;
			  float xs=200.0f/w;
        int adj=1;
			  if (y_pos!=1) { ys=-ys; adj=0; }
        if (y_pos==2)
        {
          h2 -= (int) (ys*256/2);
        }
			  for (x = 0; x < w; x ++)
			  {
          float r=x*xs;
          float s1=r-(int)r;
          float yr=fa_data[(int)r]*(1.0f-s1)+fa_data[(int)r+1]*(s1);
				  line(framebuffer,x,h2-adj,x,h2 + adj + (int) (yr*ys - 1.0f),w,h,current_color,(g_line_blend_mode&0xff0000)>>16);
			  }
	    }
      break;
      case 1: // line analyzer
		  {
			  int h2=h/2;
			  int lx,ly,ox,oy;
			  float xs= 1.0f/xscale*(288.0f/200.f);
			  float ys=yscale;
			  if (y_pos!=1) { ys=-ys; }
        if (y_pos == 2)
          h2 -= (int) (ys*256/2);

			  ly=h2 + (int) ((fa_data[0])*ys);
			  lx=0;
			  for (x = 1; x < 200; x ++)
			  {
				  oy=h2 + (int) ((fa_data[x])*ys);
				  ox=(int) (x*xs);
				  line(framebuffer,lx,ly,ox,oy,w,h,current_color,(g_line_blend_mode&0xff0000)>>16);
				  ly=oy;
				  lx=ox;
			  }
		  }
      break;
      case 2: // line scope
		  {
  		  float xs = 1.0f/xscale;
			  int lx, ly,ox,oy;
			  int yh;
        if (y_pos == 2)
          yh = h/4;
        else yh = y_pos*h/ 2;
			  lx=0;
			  ly=yh + (int) ((int)(fa_data[0]^128)*yscale);;
			  for (x = 1; x < 288; x ++)
			  {
				  ox=(int)(x*xs);
				  oy = yh + (int) ((int)(fa_data[x]^128)*yscale);
				  line(framebuffer, lx,ly,ox,oy,w,h,current_color,(g_line_blend_mode&0xff0000)>>16);
				  lx=ox;
				  ly=oy;
			  }
		  }	
      break;
      case 3: // solid scope
		  {
			  int yh = y_pos*h / 2;
        if (y_pos==2) yh=h/4;
			  int ys = yh+(int)(yscale*128.0f);
			  for (x = 0; x < w; x ++)
			  {
          float r=x*xscale;
          float s1=r-(int)r;
          float yr=(fa_data[(int)r]^128)*(1.0f-s1)+(fa_data[(int)r+1]^128)*(s1);
				  line(framebuffer,x,ys-1,x,yh + (int) (yr*yscale),w,h,current_color,(g_line_blend_mode&0xff0000)>>16);
			  }
		  }
      break;
    }
  return 0;
}

C_RBASE *R_SimpleSpectrum(char *desc)
{
	if (desc) { strcpy(desc,MOD_NAME); return NULL; }
	return (C_RBASE *) new C_THISCLASS();
}


#else
C_RBASE *R_SimpleSpectrum(char *desc)
{ return NULL; }
#endif
