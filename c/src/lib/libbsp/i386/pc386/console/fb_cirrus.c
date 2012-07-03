/*
 * Copyright (c) 2012 - Pavel Pisa ( pisa@cmp.felk.cvut.cz )
 *
 * MODULE DESCRIPTION:
 * This module implements FB driver for "Cirrus GD5446" found in Qemu
 * emulator.
 *
 * The code is based on fb_vga.c written by
 *    Copyright (c) 2000  Rosimildo da Silva ( rdasilva@connecttel.com )
 * and it is inspired by X11 and Linux kernel sources
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>

#include <bsp.h>
#include <bsp/irq.h>
#include <rtems/libio.h>
#include <rtems/pci.h>

#include <rtems/fb.h>

#include "vga_registers.h"

/* these routines are defined in vgainit.c.*/
extern void ega_hwinit( void );
extern void ega_hwterm( void );

/* mutex attribure */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


#define FB_CIRRUS_MAX_CARDS 1

#define CIRRUS_VENDOR_ID         0x1013
#define CIRRUS_GD5446_DEVICE_ID  0x00b8

/* video modes
     640 x 480
     800 x 600
     1024 x 768
     1280 x 1024
*/
struct fb_cirrus_modeline {
  int clock;
  int hdisplay;
  int hsync_start;
  int hsync_end;
  int htotal;
  int hskew;
  int vdisplay;
  int vsync_start;
  int vsync_end;
  int vtotal;
  int vscan;
  unsigned int flags;
};

struct fb_cirrus_state {
  int              found;
  int              pbus;
  int              pdev;
  int              pfun;
  uint32_t         pci_bar[4];
  struct fb_var_screeninfo fb_var;
  struct fb_fix_screeninfo fb_fix;
  void             *mmregs;
  struct fb_cirrus_modeline *active_mode;
};

/*                                1/4bpp   8bpp   15/16bpp  24bpp  32bpp */
static int fb_cirrus_max_clocks[] = { 135100, 135100,  85500,  85500,      0 };

static struct fb_cirrus_modeline fb_cirrus_std_modelines[] = {
  { .clock = 31500,
    .hdisplay = 640, .hsync_start = 664, .hsync_end = 704, .htotal = 832, .hskew = 0,
    .vdisplay = 480, .vsync_start = 489, .vsync_end = 492, .vtotal = 520, .vscan = 0,
    .flags = 0
  },
  { .clock = 40000,
    .hdisplay = 800, .hsync_start = 840, .hsync_end = 968, .htotal = 1056, .hskew = 0,
    .vdisplay = 600, .vsync_start = 601, .vsync_end = 605, .vtotal = 628, .vscan = 0,
    .flags = 0
  },
  { .clock = 0,
  },
};

#ifndef FB_CIRRUS_DEFAULT_MODE
/* TUNABLE - until better place is found, the mode is selected there */
/* index 0 provides 640x480@60 Hz, index 1 800x600@60 Hz */
#define FB_CIRRUS_DEFAULT_MODE 0
#endif

#ifndef FB_CIRRUS_DEFAULT_BPP
/* Bits per pixel, 8, 16, 24 and 32 are supported */
/* 8 bit mode requires palete support which is not finished yet */
#define FB_CIRRUS_DEFAULT_BPP  24
#endif

static struct fb_cirrus_state fb_cirrus[FB_CIRRUS_MAX_CARDS];

static int
fb_cirrus_read_config_dword(
  struct fb_cirrus_state *fbst,
  unsigned char where,
  uint32_t     *pval)
{
  return pci_read_config_dword(
    fbst->pbus, fbst->pdev, fbst->pfun,
    where, pval);
}

static int
fb_cirrus_write_config_dword(
  struct fb_cirrus_state *fbst,
  unsigned char where,
  uint32_t     val)
{
  return pci_write_config_dword(
    fbst->pbus, fbst->pdev, fbst->pfun,
    where, val);
}

static void
fb_cirrus_write_reg8 (
  const struct fb_cirrus_state *fbst,
  unsigned int reg,
  unsigned int val)
{
  *(volatile uint8_t*)((char *)fbst->mmregs + reg) = val;
}

static void
fb_cirrus_write_reg32 (
  const struct fb_cirrus_state *fbst,
  unsigned int reg,
  uint32_t val)
{
  *(volatile uint32_t*)((char *)fbst->mmregs + reg) = val;
}

static unsigned int
fb_cirrus_read_reg8 (
  const struct fb_cirrus_state *fbst,
  unsigned int reg)
{
  return *(volatile uint8_t*)((char *)fbst->mmregs + reg);
}

static uint32_t
fb_cirrus_read_reg32 (
  const struct fb_cirrus_state *fbst,
  unsigned int reg)
{
  return *(volatile uint32_t*)((char *)fbst->mmregs + reg);
}
#define SEQ_INDEX 4
#define SEQ_DATA 5

static void
fb_cirrus_write_seq_reg (
  const struct fb_cirrus_state *fbst,
  unsigned int reg,
  unsigned int val)
{
  fb_cirrus_write_reg8(fbst, SEQ_INDEX, reg);
  fb_cirrus_write_reg8(fbst, SEQ_DATA, val);
}

#define CRT_INDEX 0x14
#define CRT_DATA 0x15

static void
fb_cirrus_write_crt_reg (
  const struct fb_cirrus_state *fbst,
  unsigned int reg,
  unsigned int val)
{
  fb_cirrus_write_reg8(fbst, CRT_INDEX, reg);
  fb_cirrus_write_reg8(fbst, CRT_DATA, val);
}

#define GDC_INDEX 0xe
#define GDC_DATA 0xf

static void
fb_cirrus_write_gdc_reg (
  const struct fb_cirrus_state *fbst,
  unsigned int reg,
  unsigned int val)
{
  fb_cirrus_write_reg8(fbst, GDC_INDEX, reg);
  fb_cirrus_write_reg8(fbst, GDC_DATA, val);
}


#define VGA_DAC_MASK 0x6

static void
fb_cirrus_write_hdr_reg (
  const struct fb_cirrus_state *fbst,
  unsigned int val)
{
  volatile unsigned int dummy __attribute__((unused));
  dummy = fb_cirrus_read_reg8(fbst, VGA_DAC_MASK);
  dummy = fb_cirrus_read_reg8(fbst, VGA_DAC_MASK);
  dummy = fb_cirrus_read_reg8(fbst, VGA_DAC_MASK);
  dummy = fb_cirrus_read_reg8(fbst, VGA_DAC_MASK);
  fb_cirrus_write_reg8(fbst, VGA_DAC_MASK, val);
}

static void
fb_cirrus_set_start_address(
  struct fb_cirrus_state *fbst,
  unsigned offset)
{
  uint32_t addr;
  uint8_t tmp;

  addr = offset >> 2;
  fb_cirrus_write_crt_reg(fbst, 0x0c, (uint8_t)((addr >> 8) & 0xff));
  fb_cirrus_write_crt_reg(fbst, 0x0d, (uint8_t)(addr & 0xff));

  fb_cirrus_write_reg8(fbst, CRT_INDEX, 0x1b);
  tmp = fb_cirrus_read_reg8(fbst, CRT_DATA);
  tmp &= 0xf2;
  tmp |= (addr >> 16) & 0x01;
  tmp |= (addr >> 15) & 0x0c;
  fb_cirrus_write_crt_reg(fbst, 0x1b, tmp);
  fb_cirrus_write_reg8(fbst, CRT_INDEX, 0x1d);
  tmp = fb_cirrus_read_reg8(fbst, CRT_DATA);
  tmp &= 0x7f;
  tmp |= (addr >> 12) & 0x80;
  fb_cirrus_write_crt_reg(fbst, 0x1d, tmp);
}


static int
fb_cirrus_set_crt_mode(
  struct fb_cirrus_state *fbst,
  struct fb_cirrus_modeline *mode)
{
  int hsyncstart, hsyncend, htotal, hdispend;
  int vtotal, vdispend;
  int tmp;
  int sr07 = 0, hdr = 0;
  int x, y;

  htotal = mode->htotal / 8;
  hsyncend = mode->hsync_end / 8;
  hsyncstart = mode->hsync_start / 8;
  hdispend = mode->hdisplay / 8;

  vtotal = mode->vtotal;
  vdispend = mode->vdisplay;

  vdispend -= 1;
  vtotal -= 2;

  htotal -= 5;
  hdispend -= 1;
  hsyncstart += 1;
  hsyncend += 1;

  fb_cirrus_write_crt_reg(fbst, CRTC_EndVertRetrace, 0x20);
  fb_cirrus_write_crt_reg(fbst, CRTC_HorzTotal, htotal);
  fb_cirrus_write_crt_reg(fbst, CRTC_HorzDispEnd, hdispend);
  fb_cirrus_write_crt_reg(fbst, CRTC_StartHorzRetrace, hsyncstart);
  fb_cirrus_write_crt_reg(fbst, CRTC_EndHorzRetrace, hsyncend);
  fb_cirrus_write_crt_reg(fbst, CRTC_VertTotal, vtotal & 0xff);
  fb_cirrus_write_crt_reg(fbst, CRTC_VertDispEnd, vdispend & 0xff);

  tmp = 0x40;
  if ((vdispend + 1) & 512)
    tmp |= 0x20;
  fb_cirrus_write_crt_reg(fbst, CRTC_MaxScanLine, tmp);

  tmp = 16;
  if (vtotal & 256)
    tmp |= 1;
  if (vdispend & 256)
    tmp |= 2;
  if ((vdispend + 1) & 256)
    tmp |= 8;
  if (vtotal & 512)
    tmp |= 32;
  if (vdispend & 512)
    tmp |= 64;
  fb_cirrus_write_crt_reg(fbst, CRTC_Overflow, tmp);

  tmp = 0;

  if ((htotal + 5) & 64)
    tmp |= 16;
  if ((htotal + 5) & 128)
    tmp |= 32;
  if (vtotal & 256)
    tmp |= 64;
  if (vtotal & 512)
    tmp |= 128;

  fb_cirrus_write_crt_reg(fbst, 0x1A, tmp);

  /* Disable Hercules/CGA compatibility */
  fb_cirrus_write_crt_reg(fbst, CRTC_ModeCtrl, 0x03);

  fb_cirrus_write_reg8(fbst, SEQ_INDEX, 0x7);
  sr07 = fb_cirrus_read_reg8(fbst, SEQ_DATA);
  sr07 &= 0xe0;
  hdr = 0;
  switch (fbst->fb_var.bits_per_pixel) {
    case 8:
      sr07 |= 0x11;
      break;
    case 16:
      sr07 |= 0xc7; /* has been 0xc1 */
      hdr = 0xc0;
      break;
    case 24:
      sr07 |= 0x15;
      hdr = 0xc5;
      break;
    case 32:
      sr07 |= 0x19;
      hdr = 0xc5;
      break;
    default:
       printk("FBCIRRUS fb_cirrus_set_crt_mode unsuported BPP\n");
      return -1;
  }

  fb_cirrus_write_seq_reg(fbst, 0x7, sr07);

  /* Program the pitch */
  tmp = fbst->fb_fix.line_length / 8;
  fb_cirrus_write_crt_reg(fbst, CRTC_Offset, tmp);

  /* Enable extended blanking and pitch bits, and enable full memory */
  tmp = 0x22;
  tmp |= (fbst->fb_fix.line_length >> 7) & 0x10;
  tmp |= (fbst->fb_fix.line_length >> 6) & 0x40;
  fb_cirrus_write_crt_reg(fbst, 0x1b, tmp);

  /* Enable high-colour modes */
  fb_cirrus_write_gdc_reg(fbst, GDC_Mode, 0x40);

  /* And set graphics mode */
  fb_cirrus_write_gdc_reg(fbst, GDC_Misc, 0x01);

  fb_cirrus_write_hdr_reg(fbst, hdr);

  fb_cirrus_set_start_address(fbst, 0);


  if (1) {
    uint32_t pixmask;

    if(fbst->fb_var.bits_per_pixel == 32)
      pixmask = 0xffffff;
    else
      pixmask = (1 << fbst->fb_var.bits_per_pixel) - 1;

    printk("FBCIRRUS mode set, test patter output\n");

    /*while(1)*/
    for(y = 0; y < fbst->fb_var.yres; y++) {
      for(x = 0; x < fbst->fb_var.xres; x++) {
        uint32_t color;
        char *addr = (char *)fbst->fb_fix.smem_start;
        addr += y * fbst->fb_fix.line_length;
        addr += x * fbst->fb_var.bits_per_pixel / 8;
        color = x & 1 ? 0 : y & 1 ? pixmask & 0x000ff00f : pixmask;
        if(y == fbst->fb_var.yres - 1) {
          if((x > 0) && (x < fbst->fb_var.xres-1))
            color = pixmask & 0x00555555;
        }
        switch (fbst->fb_var.bits_per_pixel) {
          case 8:  *(volatile uint8_t*) addr = color;
                   break;
          case 16: *(volatile uint16_t*) addr = color;
                   break;
          case 24: *(volatile uint32_t*) addr =
                   (*(volatile uint32_t*) addr & 0xff000000) | color;
                   break;
         case 32: *(volatile uint32_t*) addr = color;
                   break;
        }
      }
    }
  }

  return 0;
}

static uint16_t red16[] = {
   0x0000, 0x0000, 0x0000, 0x0000, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa,
   0x5555, 0x5555, 0x5555, 0x5555, 0xffff, 0xffff, 0xffff, 0xffff
};
static uint16_t green16[] = {
   0x0000, 0x0000, 0xaaaa, 0xaaaa, 0x0000, 0x0000, 0x5555, 0xaaaa,
   0x5555, 0x5555, 0xffff, 0xffff, 0x5555, 0x5555, 0xffff, 0xffff
};
static uint16_t blue16[] = {
   0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa,
   0x5555, 0xffff, 0x5555, 0xffff, 0x5555, 0xffff, 0x5555, 0xffff
};

/* Functionality to support multiple VGA frame buffers can be added easily,
 * but is not supported at this moment because there is no need for two or
 * more "classic" VGA adapters.  Multiple frame buffer drivers may be
 * implemented and If we had implement it they would be named as "/dev/fb0",
 * "/dev/fb1", "/dev/fb2" and so on.
 */
/*
 * fbvga device driver INITIALIZE entry point.
 */
rtems_device_driver frame_buffer_initialize(
  rtems_device_major_number  major,
  rtems_device_minor_number  minor,
  void                      *arg
)
{
  rtems_status_code status;
  int res;
  struct fb_cirrus_state *fbst;
  char devname[12];
  char *p;

  if(minor >= FB_CIRRUS_MAX_CARDS) {
    printk( "FBCIRRUS initialize -- unsupported minor\n" );
    return RTEMS_UNSATISFIED;
  }

  fbst = &fb_cirrus[minor];

  res = pci_find_device(
    CIRRUS_VENDOR_ID,
    CIRRUS_GD5446_DEVICE_ID,
    minor,
    &fbst->pbus,
    &fbst->pdev,
    &fbst->pfun
  );

  if ( res != PCIB_ERR_SUCCESS ) {
    fbst->found = -1;
    printk( "FBCIRRUS initialize -- device not found\n" );
    return RTEMS_UNSATISFIED;
  }

  printk( "FBCIRRUS -- driver initializing..\n" );

  /*
   * Register the device
   */
  p = strcpy(devname, "/dev/fb0");
  *(--p) = '0' + minor;

  status = rtems_io_register_name (devname, major, 0);
  if (status != RTEMS_SUCCESSFUL) {
    fbst->found = -1;
    printk("Error registering /dev/fbX FBCIRRUS framebuffer device!\n");
    rtems_fatal_error_occurred( status );
  }

  fbst->found = 1;

  return RTEMS_SUCCESSFUL;
}

/*
 * fbvga device driver OPEN entry point
 */
rtems_device_driver frame_buffer_open(
  rtems_device_major_number  major,
  rtems_device_minor_number  minor,
  void                      *arg
)
{
  struct fb_cirrus_state *fbst;
  int line_bytes;

  if(minor >= FB_CIRRUS_MAX_CARDS) {
    printk( "FBCIRRUS initialize -- unsupported minor\n" );
    return RTEMS_UNSATISFIED;
  }

  fbst = &fb_cirrus[minor];

  if (pthread_mutex_trylock(&mutex)!= 0) {
    printk( "FBCIRRUS open cannot grab mutex.\n" );
    return RTEMS_UNSATISFIED;
  }

  printf("FBCIRRUS found %d pbus %d pdev %d pfun %d\n",
    fbst->found, fbst->pbus, fbst->pdev, fbst->pfun);

  fb_cirrus_read_config_dword(fbst, PCI_BASE_ADDRESS_0, &fbst->pci_bar[0]);
  fb_cirrus_read_config_dword(fbst, PCI_BASE_ADDRESS_1, &fbst->pci_bar[1]);
  fb_cirrus_read_config_dword(fbst, PCI_BASE_ADDRESS_2, &fbst->pci_bar[2]);
  fb_cirrus_read_config_dword(fbst, PCI_BASE_ADDRESS_3, &fbst->pci_bar[3]);

  fbst->pci_bar[0] &= PCI_BASE_ADDRESS_MEM_MASK;
  fbst->pci_bar[1] &= PCI_BASE_ADDRESS_MEM_MASK;

  printf("FBCIRRUS PCI BARs 0x%08lx 0x%08lx 0x%08lx 0x%08lx\n",
    fbst->pci_bar[0], fbst->pci_bar[1], fbst->pci_bar[2], fbst->pci_bar[3]);

  if (_CPU_is_paging_enabled()) {
    _CPU_map_phys_address((void **) &(fbst->fb_fix.smem_start),
                            (void *)(fbst->pci_bar[0]),
                            0x1000000 ,
                            PTE_CACHE_DISABLE | PTE_WRITABLE);
    _CPU_map_phys_address((void **) &(fbst->mmregs),
                            (void *)(fbst->pci_bar[1]),
                            0x1000,
                            PTE_CACHE_DISABLE | PTE_WRITABLE);
   } else {
      fbst->fb_fix.smem_start = (volatile char *)fbst->pci_bar[0];
      fbst->mmregs = (void *)fbst->pci_bar[1];
   }

  fbst->fb_fix.smem_len = 0x1000000;

  fbst->fb_fix.type = FB_TYPE_PACKED_PIXELS;
  fbst->fb_fix.visual = FB_VISUAL_TRUECOLOR;

  printf("FBCIRRUS remapped fb 0x%08lx mmregs 0x%08lx\n",
    (unsigned long)fbst->fb_fix.smem_start, (unsigned long)fbst->mmregs);

  fbst->active_mode = &fb_cirrus_std_modelines[FB_CIRRUS_DEFAULT_MODE];

  fbst->fb_var.bits_per_pixel = FB_CIRRUS_DEFAULT_BPP;

  fbst->fb_var.xres = fbst->active_mode->hdisplay;
  fbst->fb_var.yres = fbst->active_mode->vdisplay;

  line_bytes = fbst->fb_var.xres * fbst->fb_var.bits_per_pixel;

  line_bytes = (line_bytes + 7) / 8;

  fbst->fb_fix.line_length = line_bytes;

  fb_cirrus_set_crt_mode(fbst, fbst->active_mode);

  printk( "FBCIRRUS open called.\n" );
  return RTEMS_SUCCESSFUL;

}

/*
 * fbvga device driver CLOSE entry point
 */
rtems_device_driver frame_buffer_close(
  rtems_device_major_number  major,
  rtems_device_minor_number  minor,
  void                      *arg
)
{
  if (pthread_mutex_unlock(&mutex) == 0){
    /* restore previous state.  for VGA this means return to text mode.
     * leave out if graphics hardware has been initialized in
     * frame_buffer_initialize() */
    printk( "FBCIRRUS close called.\n" );
    return RTEMS_SUCCESSFUL;
  }

  return RTEMS_UNSATISFIED;
}

/*
 * fbvga device driver READ entry point.
 */
rtems_device_driver frame_buffer_read(
  rtems_device_major_number  major,
  rtems_device_minor_number  minor,
  void                      *arg
)
{
  struct fb_cirrus_state *fbst;

  if(minor >= FB_CIRRUS_MAX_CARDS) {
    return RTEMS_UNSATISFIED;
  }

  fbst = &fb_cirrus[minor];

  rtems_libio_rw_args_t *rw_args = (rtems_libio_rw_args_t *)arg;
  rw_args->bytes_moved = ((rw_args->offset + rw_args->count) > fbst->fb_fix.smem_len ) ? (fbst->fb_fix.smem_len - rw_args->offset) : rw_args->count;
  memcpy(rw_args->buffer, (const void *) (fbst->fb_fix.smem_start + rw_args->offset), rw_args->bytes_moved);
  return RTEMS_SUCCESSFUL;
}

/*
 * frame_buffer device driver WRITE entry point.
 */
rtems_device_driver frame_buffer_write(
  rtems_device_major_number  major,
  rtems_device_minor_number  minor,
  void                      *arg
)
{
  struct fb_cirrus_state *fbst;

  if(minor >= FB_CIRRUS_MAX_CARDS) {
    return RTEMS_UNSATISFIED;
  }

  fbst = &fb_cirrus[minor];

  rtems_libio_rw_args_t *rw_args = (rtems_libio_rw_args_t *)arg;
  rw_args->bytes_moved = ((rw_args->offset + rw_args->count) > fbst->fb_fix.smem_len ) ? (fbst->fb_fix.smem_len - rw_args->offset) : rw_args->count;
  memcpy( (void *) (fbst->fb_fix.smem_start + rw_args->offset), rw_args->buffer, rw_args->bytes_moved);
  return RTEMS_SUCCESSFUL;
}

static int get_fix_screen_info(struct fb_cirrus_state *fbst, struct fb_fix_screeninfo *info )
{
  *info = fbst->fb_fix;
  return 0;
}

static int get_var_screen_info(struct fb_cirrus_state *fbst, struct fb_var_screeninfo *info )
{
  *info =  fbst->fb_var;
  return 0;
}

static int get_palette(struct fb_cirrus_state *fbst, struct fb_cmap *cmap )
{
  uint32_t i;

  if ( cmap->start + cmap->len >= 16 )
    return 1;

  for( i = 0; i < cmap->len; i++ ) {
    cmap->red[ cmap->start + i ]   = red16[ cmap->start + i ];
    cmap->green[ cmap->start + i ] = green16[ cmap->start + i ];
    cmap->blue[ cmap->start + i ]  = blue16[ cmap->start + i ];
  }
  return 0;
}

static int set_palette(struct fb_cirrus_state *fbst, struct fb_cmap *cmap )
{
  uint32_t i;

  if ( cmap->start + cmap->len >= 16 )
    return 1;

  for( i = 0; i < cmap->len; i++ ) {
    red16[ cmap->start + i ] = cmap->red[ cmap->start + i ];
    green16[ cmap->start + i ] = cmap->green[ cmap->start + i ];
    blue16[ cmap->start + i ] = cmap->blue[ cmap->start + i ];
  }
  return 0;
}

/*
 * IOCTL entry point -- This method is called to carry
 * all services of this interface.
 */
rtems_device_driver frame_buffer_control(
  rtems_device_major_number  major,
  rtems_device_minor_number  minor,
  void                      *arg
)
{
  rtems_libio_ioctl_args_t *args = arg;
  struct fb_cirrus_state *fbst;

  if(minor >= FB_CIRRUS_MAX_CARDS) {
    return RTEMS_UNSATISFIED;
  }

  fbst = &fb_cirrus[minor];

  printk( "FBCIRRUS ioctl called, cmd=%x\n", args->command  );

  switch( args->command ) {
    case FBIOGET_FSCREENINFO:
      args->ioctl_return =  get_fix_screen_info( fbst, ( struct fb_fix_screeninfo * ) args->buffer );
      break;
    case FBIOGET_VSCREENINFO:
      args->ioctl_return =  get_var_screen_info( fbst, ( struct fb_var_screeninfo * ) args->buffer );
      break;
    case FBIOPUT_VSCREENINFO:
      /* not implemented yet*/
      args->ioctl_return = -1;
      return RTEMS_UNSATISFIED;
    case FBIOGETCMAP:
      args->ioctl_return =  get_palette( fbst, ( struct fb_cmap * ) args->buffer );
      break;
    case FBIOPUTCMAP:
      args->ioctl_return =  set_palette( fbst, ( struct fb_cmap * ) args->buffer );
      break;

    default:
     args->ioctl_return = 0;
     break;
  }
  return RTEMS_SUCCESSFUL;
}
