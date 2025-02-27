#pragma once

#include "c__base.h"

#define MOD_NAME "Misc / Buffer Save"
#define C_THISCLASS C_StackClass


class C_THISCLASS : public C_RBASE {
  protected:
  public:
    C_StackClass();
    virtual ~C_StackClass();
    virtual int render(char visdata[2][2][576], int isBeat, int *framebuffer, int *fbout, int w, int h);
    virtual char *get_desc();
    virtual void load_config(unsigned char *data, int len);
    virtual int  save_config(unsigned char *data);

    int clear;
    volatile int dir_ch;

    int blend;
    int dir;
    int which;
    int adjblend_val;
};
