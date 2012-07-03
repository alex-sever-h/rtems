
/*
 * Copyright (c) 1992, 1993, 1996
 *     Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *     This product includes software developed by Berkeley Software
 *     Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *     BSDI video.h,v 2.2 1996/04/08 19:33:12 bostic Exp
 *
 * Subset of the full header file adapted for RTEMS operating system from
 *
 *      FreeBSD: src/usr.bin/doscmd/video.h,v 1.2.2.1 2002/04/25 11:04:51 tg Exp $
 */

/* CRTC registers

   We use the VGA register functions and don't care about the MDA. We also
   leave out the undocumented registers at 0x22, 0x24, 0x3?. */
#define        CRTC_HorzTotal          0x00
#define        CRTC_HorzDispEnd        0x01
#define CRTC_StartHorzBlank    0x02
#define        CRTC_EndHorzBlank       0x03
#define        CRTC_StartHorzRetrace   0x04
#define        CRTC_EndHorzRetrace     0x05
#define        CRTC_VertTotal          0x06
#define        CRTC_Overflow           0x07
#define        CRTC_ResetRowScan       0x08
#define        CRTC_MaxScanLine        0x09
#define        CRTC_CursStart          0x0a
#define        CRTC_CursEnd            0x0b
#define        CRTC_StartAddrHi        0x0c
#define        CRTC_StartAddrLo        0x0d
#define        CRTC_CurLocHi           0x0e
#define        CRTC_CurLocLo           0x0f
#define CRTC_StartVertRetrace  0x10
#define CRTC_EndVertRetrace    0x11
#define CRTC_VertDispEnd       0x12
#define CRTC_Offset            0x13
#define CRTC_UnderlineLoc      0x14
#define CRTC_StartVertBlank    0x15
#define CRTC_EndVertBlank      0x16
#define CRTC_ModeCtrl          0x17
#define CRTC_LineCompare       0x18

#define CRTC_Size              0x19

/* Port addresses for the CRTC

   The registers are read by
       OUT index_port, reg_nr
       IN  data_port, res

   They are written by
       OUT index_port, reg_nr
       OUT data_port, value
*/

#define        CRTC_IndexPortColor     0x03d4  /* CRTC Address Register (Color) */
#define        CRTC_DataPortColor      0x03d5  /* CRTC Data Register (Color) */
#define        CRTC_IndexPortMono      0x03b4  /* CRTC Address Register (Mono) */
#define        CRTC_DataPortMono       0x03b5  /* CRTC Data Register (Mono) */

/* GDC registers */
#define GDC_SetReset           0x00
#define GDC_EnableSetReset     0x01
#define GDC_ColorCompare       0x02
#define GDC_DataRotate         0x03
#define GDC_ReadMapSelect      0x04
#define GDC_Mode               0x05
#define GDC_Misc               0x06
#define GDC_ColorDontCare      0x07
#define GDC_BitMask            0x08

#define GDC_Size               0x09

/* Port addresses for the GDC */
#define GDC_IndexPort          0x03ce
#define GDC_DataPort           0x03cf
